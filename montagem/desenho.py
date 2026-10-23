"""
Os desenhos da montagem (SVG, em mm), para o guia:
  frente.svg        a placa da frente vista de frente (chaves, pinos, pontes)
  tras_antes.svg    vista por trás (espelhada, como fica na mão para soldar),
                    antes de juntar com a LILYGO: a matriz e os 7 fios da direita
  tras_depois.svg   vista por trás depois de juntar: a faixa da esquerda (a tela e os botões)
  corte.svg         o corte de lado da montagem inteira, dentro da caixa

As cores vêm das classes (o guia define as cores dos dois temas); a cor de
cada fio vai inline (é a cor do fio de verdade).
"""
import math
import os

import projeto as P
import fiacao as F

LARG = P.PLACA_LARGURA
Y0, Y1 = P.PLACA_Y0, P.PLACA_Y0 + P.PLACA_ALTURA

COR_FIO = {
    "vermelho": "#d93a32", "preto": "#2b2b2b", "laranja": "#f08a24", "amarelo": "#e8c22a", "verde": "#3aa356",
    "azul": "#2f7fd6", "roxo": "#8b5cc9", "cinza": "#8e959c", "branco": "#f4f1ea", "marrom": "#8a5a36",
}


class Folha:
    """Um desenho da placa. espelhar=True: visto por trás (a coluna A fica à direita)."""

    def __init__(self, espelhar: bool, rotulo: str, ml=16.0, mr=16.0, mt=9.0, mb=6.0, y_de=Y0, y_ate=Y1 + 0.0):
        self.esp = espelhar
        self.p = []
        self.ml, self.mr, self.mt, self.mb = ml, mr, mt, mb
        self.y_de, self.y_ate = y_de, y_ate
        self.w = LARG + ml + mr
        self.h = (y_ate - y_de) + mt + mb
        self.rotulo = rotulo

    def x(self, x):
        return self.ml + ((LARG - x) if self.esp else x)

    def y(self, y):
        return self.mt + (self.y_ate - y)

    def xy(self, nome):
        c, k = P.ck(nome)
        return self.x(P.col_x(c)), self.y(P.k_y(k))

    def add(self, s):
        self.p.append(s)

    def svg(self, cls=""):
        cab = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.w:.1f} {self.h:.1f}" '
               f'class="desenho {cls}" role="img" aria-label="{self.rotulo}">')
        return cab + "".join(self.p) + "</svg>"

    def ret(self, x0, y0, x1, y1, cls, rx=0.0, extra=""):
        a, b = sorted((self.x(x0), self.x(x1)))
        c, d = sorted((self.y(y0), self.y(y1)))
        self.add(f'<rect x="{a:.2f}" y="{c:.2f}" width="{b - a:.2f}" height="{d - c:.2f}" rx="{rx:.2f}" class="{cls}" {extra}/>')

    def texto(self, x, y, t, cls="rot", anc="middle", cru=False, extra=""):
        xx, yy = (x, y) if cru else (self.x(x), self.y(y))
        if self.esp and not cru and anc != "middle":
            anc = "end" if anc == "start" else "start"
        self.add(f'<text x="{xx:.2f}" y="{yy:.2f}" text-anchor="{anc}" class="{cls}" {extra}>{t}</text>')

    def linha(self, x0, y0, x1, y1, cls, extra=""):
        self.add(f'<line x1="{self.x(x0):.2f}" y1="{self.y(y0):.2f}" x2="{self.x(x1):.2f}" y2="{self.y(y1):.2f}" '
                 f'class="{cls}" {extra}/>')


# ---------------------------------------------------------------------------
# peças comuns
# ---------------------------------------------------------------------------
def placa(f: Folha):
    f.ret(0, Y0, LARG, Y1, "pcb", 0.6)


def grade(f: Folha, apagados=()):
    for c in range(P.COLUNAS):
        for k in range(P.LINHAS):
            x, y = f.x(P.col_x(c)), f.y(P.k_y(k))
            cls = "ilha ilhaApagada" if P.furo(c, k) in apagados else "ilha"
            f.add(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="0.8" class="{cls}"/>'
                  f'<circle cx="{x:.2f}" cy="{y:.2f}" r="0.42" class="buraco"/>')


def eixos(f: Folha, linhas_extra=()):
    for c in range(P.COLUNAS):
        f.texto(P.col_x(c), Y1 + 1.5, P.letra(c), "eixo")
        f.texto(P.col_x(c), Y0 - 2.5, P.letra(c), "eixo")
    marcar = {1, 51} | {r for r in range(5, 51, 5)} | set(linhas_extra)
    for r in sorted(marcar):
        k = P.LINHAS - r
        for lado, anc in ((-0.9, "end"), (LARG + 0.9, "start")):
            f.texto(lado, P.k_y(k) - 0.5, str(r), "eixo eixoLinha", anc)


def pino_quadrado(f: Folha, nome, cls):
    x, y = f.xy(nome)
    f.add(f'<rect x="{x - 0.95:.2f}" y="{y - 0.95:.2f}" width="1.9" height="1.9" rx="0.2" class="{cls}"/>')


def solda(f: Folha, nome, cls="solda", r=0.9):
    x, y = f.xy(nome)
    f.add(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{r}" class="{cls}"/>')


def barras(f: Folha, rotulos=True, frente=True):
    """Os pinos das barras da LILYGO, com o nome do lado de fora da placa"""
    for n in range(1, 17):
        for col, nomes, esquerda in ((P.COL_J1, P.J1, True), (P.COL_J2, P.J2, False)):
            nome = P.furo(col, P.K_PINO1 - (n - 1))
            pino_quadrado(f, nome, "pinoLilygo")
            if not rotulos:
                continue
            s = nomes[n - 1].split()[0]
            x, y = f.xy(nome)
            fora_esq = esquerda != f.esp      # o rótulo vai para o lado de fora (da borda mais perto)
            xb = f.x(0) if (esquerda and not f.esp) or (not esquerda and f.esp) else f.x(LARG)
            if fora_esq:
                borda = min(f.x(0), f.x(LARG))
                xt, x_guia = borda - 5.0, borda - 4.4
                anc = "end"
            else:
                borda = max(f.x(0), f.x(LARG))
                xt, x_guia = borda + 5.0, borda + 4.4
                anc = "start"
            usado = s in USADOS
            f.add(f'<line x1="{borda + (-3.6 if anc == "end" else 3.6):.2f}" y1="{y:.2f}" x2="{x_guia:.2f}" y2="{y:.2f}" '
                  f'class="guia{" guiaUsada" if usado else ""}"/>')
            f.add(f'<text x="{xt:.2f}" y="{y + 0.5:.2f}" text-anchor="{anc}" '
                  f'class="rotPino{" rotPinoUsado" if usado else ""}">{s}</text>')


USADOS = {"3V3", "GND", "IO47", "IO14", "IO9", "IO36", "IO35", "IO17", "IO11", "IO12", "IO13",
          "IO40", "IO39", "IO38", "IO15", "IO16", "IO37", "IO21"}


def chaves_frente(f: Folha):
    for ch in F.CHAVES:
        cx, cy = P.centro_chave(ch.c, ch.k)
        f.ret(cx - 3, cy - 3, cx + 3, cy + 3, f"chave chave_{ch.cor}", 0.5)
        f.add(f'<circle cx="{f.x(cx):.2f}" cy="{f.y(cy):.2f}" r="1.75" class="botaozinho botao_{ch.cor}"/>')
        if ch.cor == "tecla":
            f.texto(cx, cy - 0.72, ch.rotulo, "rotTecla")
        (a, b), (c, d) = ch.pernas()
        for n in (a, b, c, d):
            solda(f, n, "pernaFrente", 0.55)


def chaves_tras(f: Folha):
    for ch in F.CHAVES:
        cx, cy = P.centro_chave(ch.c, ch.k)
        f.ret(cx - 3, cy - 3, cx + 3, cy + 3, "chaveSombra" + (f" sombra_{ch.cor}" if ch.cor != "tecla" else ""), 0.5)
        if ch.cor == "tecla":
            f.texto(cx, cy - 0.6, ch.rotulo, "rotTeclaTras")
        (a, b), (c, d) = ch.pernas()
        for n in (a, b, c, d):
            solda(f, n, "soldaPerna", 0.85)


def pinos_tela(f: Folha, rotulos=True):
    for p in range(1, 15):
        nome = f"{F.col_tela(p)}1"
        pino_quadrado(f, nome, "pinoTela" if p <= 9 else "pinoTela pinoTelaLivre")
        if rotulos and p <= 9:
            x, y = f.xy(nome)
            s = P.TELA_PINOS[p - 1].split("(")[0]
            f.add(f'<text x="{x - 0.3:.2f}" y="{y - 4.6:.2f}" text-anchor="start" transform="rotate(-55 {x - 0.3:.2f} {y - 4.6:.2f})" '
                  f'class="rotPinoTela">{s}</text>')


def caminho_d(f: Folha, nomes, deslocar=(0.0, 0.0), raio=1.2):
    """Um fio pelos furos (cantos arredondados); deslocar = (dx, dy) em mm, para separar fios do mesmo maço"""
    pts = []
    for n in nomes:
        x, y = P.xy(n)
        pts.append((f.x(x + deslocar[0]), f.y(y + deslocar[1])))
    # tira pontos repetidos e colineares
    lim = [pts[0]]
    for q in pts[1:]:
        if abs(q[0] - lim[-1][0]) < 1e-6 and abs(q[1] - lim[-1][1]) < 1e-6:
            continue
        lim.append(q)
    pts = lim
    if len(pts) == 1:
        return f"M{pts[0][0]:.2f},{pts[0][1]:.2f}"
    d = f"M{pts[0][0]:.2f},{pts[0][1]:.2f}"
    for i in range(1, len(pts) - 1):
        (ax, ay), (bx, by), (cx, cy) = pts[i - 1], pts[i], pts[i + 1]
        v1 = (bx - ax, by - ay)
        v2 = (cx - bx, cy - by)
        l1, l2 = math.hypot(*v1), math.hypot(*v2)
        if l1 < 1e-6 or l2 < 1e-6 or abs(v1[0] * v2[1] - v1[1] * v2[0]) < 1e-6:
            d += f" L{bx:.2f},{by:.2f}"
            continue
        r = min(raio, l1 / 2, l2 / 2)
        p1 = (bx - v1[0] / l1 * r, by - v1[1] / l1 * r)
        p2 = (bx + v2[0] / l2 * r, by + v2[1] / l2 * r)
        d += f" L{p1[0]:.2f},{p1[1]:.2f} Q{bx:.2f},{by:.2f} {p2[0]:.2f},{p2[1]:.2f}"
    d += f" L{pts[-1][0]:.2f},{pts[-1][1]:.2f}"
    return d


def fio(f: Folha, l, deslocar=(0.0, 0.0), ponta_final="passa"):
    cor = COR_FIO.get(l.cor, "#888")
    d = caminho_d(f, l.caminho, deslocar)
    f.add(f'<path d="{d}" class="fioBorda"/>')
    f.add(f'<path d="{d}" class="fio" style="stroke:{cor}"/>')
    x0, y0 = f.xy(l.de)
    f.add(f'<circle cx="{x0:.2f}" cy="{y0:.2f}" r="0.95" class="soldaFio" style="stroke:{cor}"/>')
    x1, y1 = f.xy(l.ate)
    if ponta_final == "passa":
        f.add(f'<circle cx="{x1:.2f}" cy="{y1:.2f}" r="1.05" class="atravessa" style="stroke:{cor}"/>')
    else:
        f.add(f'<circle cx="{x1:.2f}" cy="{y1:.2f}" r="0.95" class="soldaFio" style="stroke:{cor}"/>')


def nu(f: Folha, l):
    if l.tipo == "ponte":
        x0, y0 = f.xy(l.de)
        x1, y1 = f.xy(l.ate)
        f.add(f'<line x1="{x0:.2f}" y1="{y0:.2f}" x2="{x1:.2f}" y2="{y1:.2f}" class="ponteSolda"/>')
    else:
        d = caminho_d(f, l.caminho, raio=0.01)
        f.add(f'<path d="{d}" class="fioNu"/>')
        for n in (l.caminho[0], l.caminho[-1]) if "C38" not in l.caminho and "I38" not in l.caminho and "O38" not in l.caminho else l.caminho[::4]:
            solda(f, n, "soldaNu", 0.7)


def lilygo_contorno(f: Folha, cls="lilygoContorno"):
    lx, ly = P.LILYGO_X, P.LILYGO_Y
    f.ret(lx, ly, lx + P.L.largura, ly + P.L.altura, cls, P.L.raio)


ROTULOS_DEPOIS = []


def pecas_lilygo(f: Folha):
    """As peças altas da LILYGO embaixo da placa (vista por trás): o que deixa pouco espaço"""
    rotulos = {"ESP32": "ESP32 3,2", "CASE-D": "", "473090285": "cartão TF 2,9", "LGA-124": "modem 2,5",
               "USB-C": "USB-C 3,2", "FPC": "conector 2,1", "SIM-SMD": "chip 1,8", "SW-TH": "chave 3,7"}
    for p in P.PECAS_CIMA:
        if p["altura"] < 1.7:
            continue
        x0, y0 = P.lilygo_para_tijolo(p["x0"], p["y0"])
        x1, y1 = P.lilygo_para_tijolo(p["x1"], p["y1"])
        x0, x1 = max(x0, -1.5), min(x1, LARG + 1.5)
        cls = "pecaAlta" if p["altura"] >= 2.8 else "pecaMedia"
        f.ret(x0, y0, x1, y1, cls, 0.5)
        r = next((v for k, v in rotulos.items() if k in p["nome"]), None)
        if r:
            ROTULOS_DEPOIS.append(((x0 + x1) / 2, (y0 + y1) / 2 - 0.5, r))
    # a chave liga/desliga (sai pela borda esquerda da LILYGO)


def legenda_lado(f: Folha, texto):
    f.texto(f.w / 2, 4.2, texto, "tituloFolha", cru=True)


# ---------------------------------------------------------------------------
# os desenhos
# ---------------------------------------------------------------------------
def frente() -> str:
    f = Folha(False, "Placa da frente, vista de frente: chaves, pinos da LILYGO e pontes de solda", ml=19, mr=19, mt=17)
    placa(f)
    grade(f)
    eixos(f, linhas_extra=(9, 16, 22, 24, 32, 34))
    barras(f)
    chaves_frente(f)
    # a tela: só o contorno (as pontes ficam embaixo dela)
    f.ret(P.TELA_X, P.TELA_BASE, P.TELA_X + P.T.largura, P.TELA_TOPO, "telaContorno", 1.0)
    f.ret(P.TELA_X + 0.4, P.VIDRO_BASE, P.TELA_X + P.T.largura - 0.4, P.VIDRO_TOPO, "telaVidro", 0.6)
    yl = P.k_y(P.LINHAS - 27)
    f.ret(P.TELA_X + 8, yl - 3.6, P.TELA_X + P.T.largura - 8, yl + 2.6, "fundoRotulo", 0.6)
    f.texto(P.TELA_X + P.T.largura / 2, yl, "contorno da tela (vai por último)", "rotTela")
    f.texto(P.TELA_X + P.T.largura / 2, yl - 2.5, "as pontes ficam embaixo dela", "rotTelaP")
    pinos_tela(f)
    # pontes da frente
    for l in F.LIG:
        if l.lado == "frente":
            x0, y0 = f.xy(l.de)
            x1, y1 = f.xy(l.ate)
            cor = COR_FIO.get(next((m.cor for m in F.LIG if m.sinal == l.sinal and m.cor), ""), "#999")
            f.add(f'<line x1="{x0:.2f}" y1="{y0:.2f}" x2="{x1:.2f}" y2="{y1:.2f}" class="ponteFrente"/>')
            f.add(f'<circle cx="{x0:.2f}" cy="{y0:.2f}" r="0.62" class="pontaFio" style="fill:{cor}"/>')
    legenda_lado(f, "FRENTE")
    return f.svg("frente")


def tras_antes() -> str:
    f = Folha(True, "Placa da frente vista por trás, antes de juntar com a LILYGO: a matriz do teclado e os 7 fios da direita",
              ml=19, mr=19, mt=13)
    placa(f)
    pecas_lilygo(f)
    lilygo_contorno(f)
    grade(f)
    eixos(f, linhas_extra=(16, 22, 32, 34, 35, 37))
    # o plástico das barras (vem com a LILYGO)
    for col in (P.COL_J1, P.COL_J2):
        x = P.col_x(col)
        f.ret(x - 1.27, P.k_y(P.K_PINO1 - 15) - 1.27, x + 1.27, P.k_y(P.K_PINO1) + 1.27, "plasticoBarra", 0.3)
    for col, nome in ((P.COL_J1, "J1"), (P.COL_J2, "J2")):
        f.texto(P.col_x(col), P.k_y(P.K_PINO1) + 2.2, nome, "rotBarra")
    chaves_tras(f)
    for x, y, t in ROTULOS_DEPOIS:
        f.ret(x - len(t) * 0.42, y - 0.55, x + len(t) * 0.42, y + 1.45, "fundoRotulo", 0.4)
        f.texto(x, y, t, "rotPeca")
    ROTULOS_DEPOIS.clear()
    for l in F.LIG:
        if l.lado == "tras" and l.etapa == "matriz":
            nu(f, l)
    # o maço da direita: um deslocamento pequeno por fio, para ver os 7
    dire = [l for l in F.LIG if l.etapa == "direita"]
    ordem = sorted(dire, key=lambda l: int(l.ate[1:]))
    for i, l in enumerate(ordem):
        dx = (i - 3) * 0.22
        fio(f, l, (dx if not f.esp else -dx, 0.0))
    f.texto(P.col_x(14), P.k_y(P.LINHAS - 28) + 0.2, "maço", "rotMaco")
    legenda_lado(f, "ATRÁS · ANTES DE JUNTAR")
    return f.svg("tras")


def tras_depois() -> str:
    f = Folha(True, "Placa da frente vista por trás, depois de juntar: os fios da faixa da esquerda", ml=19, mr=19, mt=19)
    placa(f)
    grade(f)
    # a LILYGO tapa tudo, menos a faixa da esquerda e a ponta de cima
    lilygo_contorno(f, "lilygoTapa")
    f.texto(P.LILYGO_X + P.L.largura / 2, P.LILYGO_Y + P.L.altura / 2 + 6, "LILYGO", "rotLilygoGrande")
    f.texto(P.LILYGO_X + P.L.largura / 2, P.LILYGO_Y + P.L.altura / 2 + 1, "(já soldada, atrás", "rotLilygoP")
    f.texto(P.LILYGO_X + P.L.largura / 2, P.LILYGO_Y + P.L.altura / 2 - 3, "desta placa)", "rotLilygoP")
    # o alto-falante, embaixo das linhas de cima
    fx0, fx1 = P.FALANTE_X0, P.FALANTE_X0 + P.FALANTE[0]
    f.ret(fx0, P.FALANTE_Y, fx1, P.FALANTE_Y + P.FALANTE[2], "falantePlanta", 0.6)
    f.texto((fx0 + fx1) / 2, P.FALANTE_Y + 1.4, "alto-falante: sem fios aqui", "rotFalante")
    eixos(f, linhas_extra=(9, 10, 12, 13, 14, 15, 16, 18, 21, 22, 23, 32, 34, 37, 38))
    pinos_tela(f, rotulos=False)
    for p, s, _, _ in F.TELA_LIGA:
        x, y = f.xy(f"{F.col_tela(p)}1")
        nome = s.split()[0]
        f.add(f'<text x="{x - 0.3:.2f}" y="{y - 4.6:.2f}" text-anchor="start" transform="rotate(-55 {x - 0.3:.2f} {y - 4.6:.2f})" '
              f'class="rotPinoTela">{nome}</text>')
    # as pernas dos botões e da coluna 1 (a matriz já está feita)
    for ch in F.CHAVES:
        if ch.c == P.COLS_TECLAS[0]:
            (a, b), (c, d) = ch.pernas()
            for n in (a, b, c, d):
                solda(f, n, "soldaPerna soldaPernaApagada", 0.85)
    for l in F.LIG:
        if l.lado == "tras" and l.etapa == "matriz" and l.sinal == "coluna 1 4 7 *":
            nu(f, l)
    # os fios: os da tela descem pela coluna B; os outros sobem pela A
    esq = [l for l in F.LIG if l.etapa == "esquerda"]
    tela = [l for l in esq if l.sinal.startswith("tela")]
    tela.sort(key=lambda l: int(l.ate[1:]))
    for i, l in enumerate(tela):
        dx = -(i - 4) * 0.28
        fio(f, l, (dx, 0.0))
    for l in esq:
        if not l.sinal.startswith("tela"):
            fio(f, l)
    # capacitores entre o VCC e o GND da tela
    x0, y0 = f.xy(f"{F.col_tela(1)}1")
    x1, _ = f.xy(f"{F.col_tela(2)}1")
    xa, xb = sorted((x0, x1))
    f.add(f'<rect x="{xa + 0.6:.2f}" y="{y0 - 0.55:.2f}" width="{xb - xa - 1.2:.2f}" height="1.1" rx="0.4" class="capCeram"/>')
    if (xa + xb) / 2 > f.w / 2:            # o rótulo do lado mais perto
        xr, xp, anc, dx = max(f.x(0), f.x(LARG)), xb - 0.6, "start", 1.4
    else:
        xr, xp, anc, dx = min(f.x(0), f.x(LARG)), xa + 0.6, "end", -1.4
    f.add(f'<line x1="{xp:.2f}" y1="{y0:.2f}" x2="{xr + dx * 0.7:.2f}" y2="{y0 - 3.2:.2f}" class="guia guiaUsada"/>')
    f.add(f'<text x="{xr + dx:.2f}" y="{y0 - 3.6:.2f}" text-anchor="{anc}" class="rotCap">capacitores</text>')
    f.add(f'<text x="{xr + dx:.2f}" y="{y0 - 1.9:.2f}" text-anchor="{anc}" class="rotCap">VCC–GND</text>')
    legenda_lado(f, "ATRÁS · DEPOIS DE JUNTAR")
    return f.svg("tras")


# ---------------------------------------------------------------------------
# O corte de lado (Y para a direita, Z para cima): a montagem inteira
# ---------------------------------------------------------------------------
def corte() -> str:
    yl0, yl1 = P.CAV_Y0 - P.PAREDE - 1, P.CAV_Y1 + P.PAREDE + 1
    w = yl1 - yl0 + 48
    h = P.Z_TOPO + 22
    p = []

    def X(y):
        return 3 + (y - yl0)

    def Z(z):
        return h - 9 - z

    def ret(y0, z0, y1, z1, cls, rx=0.0):
        a, b = sorted((X(y0), X(y1)))
        c, d = sorted((Z(z0), Z(z1)))
        p.append(f'<rect x="{a:.2f}" y="{c:.2f}" width="{b - a:.2f}" height="{d - c:.2f}" rx="{rx}" class="{cls}"/>')

    def tx(y, z, t, cls="rotCorte", anc="start"):
        p.append(f'<text x="{X(y):.2f}" y="{Z(z):.2f}" text-anchor="{anc}" class="{cls}">{t}</text>')

    # caixa: fundo e frente
    cy0, cy1 = P.CAV_Y0 - P.PAREDE, P.CAV_Y1 + P.PAREDE
    ret(cy0, 0, cy1, P.Z_DIVISAO, "caixaFundo", 2.5)
    ret(P.CAV_Y0, P.FUNDO, P.CAV_Y1, P.Z_DIVISAO + 0.01, "cavidade")
    ret(cy0, P.Z_DIVISAO, cy1, P.Z_TOPO, "caixaFrente", 2.5)
    ret(P.CAV_Y0, P.Z_DIVISAO - 0.01, P.CAV_Y1, P.Z_FRENTE_DENTRO, "cavidade")
    # janela da tela
    ret(P.IMAGEM_BASE - 0.6, P.Z_FRENTE_DENTRO - 0.01, P.IMAGEM_TOPO + 0.6, P.Z_TOPO + 0.01, "cavidade")
    # pilha 18650 (embaixo da LILYGO)
    s0, s1 = P.LILYGO_Y + P.L.suporte_18650[1], P.LILYGO_Y + P.L.suporte_18650[3]
    ret(s0 + 1.0, P.Z_LILYGO - 18.6, s1 - 2.0, P.Z_LILYGO - 0.2, "bateria", 3)
    ret(s0, P.Z_LILYGO - 15.0, s0 + 1.6, P.Z_LILYGO, "suporte")
    ret(s1 - 1.6, P.Z_LILYGO - 15.0, s1, P.Z_LILYGO, "suporte")
    # LILYGO e peças
    ret(P.LILYGO_Y, P.Z_LILYGO, P.LILYGO_Y + P.L.altura, P.Z_LILYGO_CIMA, "lilygoCorte")
    ret(P.LILYGO_Y + 6.4, P.Z_LILYGO_CIMA, P.LILYGO_Y + 30.4, P.Z_LILYGO_CIMA + 2.46, "pecaCorte")
    ret(P.LILYGO_Y + 85.1, P.Z_LILYGO_CIMA, P.LILYGO_Y + 110.5, P.Z_LILYGO_CIMA + 3.16, "pecaCorte")
    # barras de pinos (dois plásticos)
    yb0, yb1 = P.k_y(P.K_PINO1 - 15) - 1.27, P.k_y(P.K_PINO1) + 1.27
    for n in range(16):
        yy = P.k_y(P.K_PINO1 - n)
        p.append(f'<line x1="{X(yy):.2f}" y1="{Z(P.Z_LILYGO - 1.4):.2f}" x2="{X(yy):.2f}" y2="{Z(P.Z_PLACA_FRENTE + 1.0):.2f}" class="pinoCorte"/>')
    ret(yb0, P.Z_LILYGO_CIMA, yb1, P.Z_LILYGO_CIMA + 2.54, "plastico")
    ret(yb0, P.Z_LILYGO_CIMA + 2.54, yb1, P.Z_PLACA, "plastico2")
    # placa da frente
    ret(P.PLACA_Y0, P.Z_PLACA, P.PLACA_Y0 + P.PLACA_ALTURA, P.Z_PLACA_FRENTE, "placaCorte")
    # tela
    ret(P.TELA_BASE, P.Z_TELA, P.TELA_TOPO, P.Z_TELA + P.T.pcb, "telaCortePcb")
    ret(P.VIDRO_BASE, P.Z_TELA + P.T.pcb, P.VIDRO_TOPO, P.Z_VIDRO, "telaCorteVidro")
    ret(P.k_y(P.K_TELA) - 1.27, P.Z_PLACA_FRENTE, P.k_y(P.K_TELA) + 1.27, P.Z_TELA, "plastico")
    # teclas
    for k in list(P.K_TECLAS) + [P.K_BOTOES]:
        yy = P.k_y(k)
        ret(yy - 3, P.Z_PLACA_FRENTE, yy + 3, P.Z_PLACA_FRENTE + P.CHAVE_CORPO, "chaveCorte", 0.4)
        ret(yy - 0.9, P.Z_PLACA_FRENTE + P.CHAVE_CORPO, yy + 0.9, P.Z_PLACA_FRENTE + P.CHAVE_ALTURA, "chaveCorte")
        zt = P.Z_PLACA_FRENTE + P.CHAVE_ALTURA
        ret(yy - P.TECLA_ALT / 2 - 0.5, zt, yy + P.TECLA_ALT / 2 + 0.5, zt + 1.0, "teclaCorte", 0.3)
        ret(yy - P.TECLA_ALT / 2, zt + 1.0, yy + P.TECLA_ALT / 2, P.Z_TOPO + 1.2, "teclaCorte", 0.6)
    # alto-falante em pé, virado para cima
    fy = P.FALANTE_Y
    ret(fy, P.FUNDO, fy + P.FALANTE[2], P.FUNDO + P.FALANTE[1], "falante", 0.8)
    # microfone
    ret(0.2, P.MIC_Z - P.MIC_D / 2, 0.2 + P.MIC_L, P.MIC_Z + P.MIC_D / 2, "mic", 1.5)

    # cotas de altura, à direita
    xc = X(cy1) + 4
    cotas = [(0, "fundo da caixa"), (P.Z_LILYGO, "LILYGO"), (P.Z_PLACA, "placa da frente"), (P.Z_VIDRO, "vidro da tela"),
             (P.Z_TOPO, "frente da caixa")]
    for z, nome in cotas:
        p.append(f'<line x1="{X(cy1):.2f}" y1="{Z(z):.2f}" x2="{xc:.2f}" y2="{Z(z):.2f}" class="cota"/>')
        p.append(f'<text x="{xc + 1:.2f}" y="{Z(z) + 1.0:.2f}" class="rotCota">{z:.1f} mm · {nome}</text>')
    # rótulos
    tx(s0 + 20, P.Z_LILYGO - 10.5, "pilha 18650", "rotCorteClaro")
    tx(P.LILYGO_Y + 36, P.Z_LILYGO - 2.6, "suporte da pilha (é da LILYGO)", "rotCortePeq")
    tx(P.LILYGO_Y + 34, P.Z_LILYGO_CIMA + 0.9, "LILYGO", "rotCortePeq rotCorteClaro")
    tx(yb0 - 1.0, P.Z_PLACA - 1.6, "2 plásticos de barra de pinos (5,08 mm)", "rotCortePeq", "end")
    tx(P.TELA_BASE + 6, P.Z_VIDRO + 2.2, "tela (1 plástico embaixo)", "rotCortePeq")
    tx(P.k_y(P.K_TECLAS[-1]) - 3.5, P.Z_TOPO + 3.4, "teclas", "rotCortePeq")
    tx(fy + P.FALANTE[2] + 1.0, P.FUNDO + 13, "alto-falante", "rotCortePeq")
    tx(0.6, P.MIC_Z + P.MIC_D / 2 + 1.6, "mic", "rotCortePeq")
    tx(cy0, h - 2, "↓ embaixo do Tijolo", "rotCorteEixo")
    tx(cy1, h - 2, "em cima ↑ (a saída do som)", "rotCorteEixo", "end")
    cab = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w:.1f} {h:.1f}" class="desenho corte" '
           f'role="img" aria-label="Corte de lado da montagem dentro da caixa">')
    return cab + "".join(p) + "</svg>"


DESENHOS = {"frente": frente, "tras_antes": tras_antes, "tras_depois": tras_depois, "corte": corte}


def gerar(pasta="saida"):
    """Os desenhos normais e, com a tela espelhada (o VCC à direita), os dois que mudam"""
    import importlib
    os.makedirs(pasta, exist_ok=True)
    feitos = []
    for espelhada in (False, True):
        os.environ["TELA_ESPELHADA"] = "1" if espelhada else "0"
        importlib.reload(F)
        for nome, fn in DESENHOS.items():
            if espelhada and nome not in ("frente", "tras_depois"):
                continue
            arq = f"{pasta}/{nome}{'_espelhada' if espelhada else ''}.svg"
            with open(arq, "w", encoding="utf-8") as fh:
                fh.write(fn())
            feitos.append(arq)
    os.environ["TELA_ESPELHADA"] = "0"
    importlib.reload(F)
    return feitos


if __name__ == "__main__":
    for arq in gerar():
        print(arq)
