"""
A caixa do Tijolo para imprimir em 3D: o fundo, a frente e as 14 teclas.

    python3 caixa/caixa.py            (da pasta montagem/; precisa do CadQuery)

Sai em montagem/saida/caixa/: fundo.stl, frente.stl (a cara para baixo, do
jeito de imprimir), teclas.stl (as 12), tecla_verde.stl e tecla_vermelha.stl;
para ver: frente_no_lugar.stl, teclas_no_lugar.stl e visual/ (versões leves,
para a vista 3D do guia). No fim confere se alguma peça bate na caixa.
Para fazer só uma parte: caixa.py fundo | frente | teclas | visual | conferir.
As medidas vêm de projeto.py (veja projeto.MEDIDAS: o que conferir antes de
encomendar).

Como fecha: a frente desce reta sobre o fundo; em cima, duas linguetas com
dente entram nos rasgos da parede de cima (clic); embaixo, dois parafusos M2
entram por trás nos pilares da frente. A LILYGO fica apoiada em 4 pilares com
pinos nos furos dela; a frente segura a placa e a tela por cima.
"""
import os
import sys

import cadquery as cq

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(AQUI))
import projeto as P  # noqa: E402

FONTE = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

# ---------------------------------------------------------------------------
# medidas da caixa
# ---------------------------------------------------------------------------
X0, X1 = P.CAV_X0 - P.PAREDE, P.CAV_X1 + P.PAREDE
Y0, Y1 = P.CAV_Y0 - P.PAREDE, P.CAV_Y1 + P.PAREDE
R_FORA = 5.0                       # canto arredondado (visto de frente)
R_DENTRO = R_FORA - P.PAREDE
BORDA = 1.4                        # arredondado das quinas da frente e de trás
ZD = P.Z_DIVISAO                   # onde a frente encontra o fundo

# parafusos de baixo: M2 × 8, entram por trás (pelo fundo) nos pilares da frente
PARAF_X = (6.0, P.PLACA_LARGURA - 6.0)
PARAF_Y = 2.7
PILAR_D = 4.6                      # o pilar da frente (desce até perto do fundo)
PILAR_Z0 = P.FUNDO + 0.2

# linguetas de cima (frente): descem por dentro da parede de cima e o dente
# entra no rasgo dela; a lingueta verga 0,45 mm para passar
LING = dict(xs=(P.PLACA_LARGURA / 2 - 10.9, P.PLACA_LARGURA / 2 + 10.9), larg=10.0, esp=1.2, z0=21.0,
            folga=0.15, dente=0.6, dente_alt=1.2)
RASGO = dict(fundo=0.8, z0=20.7, z1=22.6)

# teclas
FOLGA_TECLA = 0.3                  # em volta da tecla, no furo da frente
ABA_TECLA = 0.5                    # a aba de baixo da tecla passa isto do furo, de cada lado
ABA_ALT = 2.4
POCO_PAREDE = 1.2
SAI_TECLA = 1.2                    # quanto a tecla sai da frente
RECESSO_PINO = (3.9, 0.55)         # embaixo da tecla: o botãozinho da chave entra aqui (diâmetro, fundo)
Z_TECLA0 = P.Z_PLACA_FRENTE + P.CHAVE_ALTURA - RECESSO_PINO[1]


def caixa_arredondada(x0, y0, x1, y1, z0, z1, r):
    return (cq.Workplane("XY").workplane(offset=z0)
            .center((x0 + x1) / 2, (y0 + y1) / 2)
            .rect(x1 - x0, y1 - y0).extrude(z1 - z0)
            .edges("|Z").fillet(r))


def bloco(x0, y0, z0, x1, y1, z1):
    return cq.Workplane("XY").box(x1 - x0, y1 - y0, z1 - z0, centered=False).translate((x0, y0, z0))


def cilindro_z(x, y, z0, z1, d):
    return cq.Workplane("XY").workplane(offset=z0).center(x, y).circle(d / 2).extrude(z1 - z0)


def cilindro_y(x, z, y0, y1, d):
    return (cq.Workplane("XZ").workplane(offset=-y0).center(x, z).circle(d / 2).extrude(-(y1 - y0)))


def cilindro_x(y, z, x0, x1, d):
    return cq.Workplane("YZ").workplane(offset=x0).center(y, z).circle(d / 2).extrude(x1 - x0)


def teclas_posicoes():
    """(rótulo, x, y, cor) de cada tecla"""
    import fiacao as F
    v = []
    for ch in F.CHAVES:
        x, y = P.centro_chave(ch.c, ch.k)
        v.append((ch.rotulo, x, y, ch.cor))
    return v


# ---------------------------------------------------------------------------
# o fundo
# ---------------------------------------------------------------------------
def fundo():
    f = caixa_arredondada(X0, Y0, X1, Y1, 0, ZD, R_FORA)
    f = f.faces("<Z").edges().fillet(BORDA)
    f = f.cut(caixa_arredondada(P.CAV_X0, P.CAV_Y0, P.CAV_X1, P.CAV_Y1, P.FUNDO, ZD + 1, R_DENTRO))

    # pilares da LILYGO, com pino no furo dela e aletas embaixo
    for (fx, fy) in P.L.furos_fixacao:
        x, y = P.lilygo_para_tijolo(fx, fy)
        f = f.union(cilindro_z(x, y, P.FUNDO - 0.1, P.Z_LILYGO, 5.2))
        f = f.union(cilindro_z(x, y, P.Z_LILYGO - 0.1, P.Z_LILYGO + 1.3, 2.1).faces(">Z").edges().chamfer(0.3))
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            if dy > 0 and fy > P.L.altura / 2:
                continue                              # nos de cima, para o lado do alto-falante não
            if dx:
                aleta = bloco(min(x, x + dx * 5.6), y - 0.6, P.FUNDO - 0.1, max(x, x + dx * 5.6), y + 0.6, P.FUNDO + 6.0)
            else:
                aleta = bloco(x - 0.6, min(y, y + dy * 5.6), P.FUNDO - 0.1, x + 0.6, max(y, y + dy * 5.6), P.FUNDO + 6.0)
            f = f.union(aleta)

    # berço do alto-falante (em pé, virado para cima): uma calha em cada ponta
    fy0, fy1 = P.FALANTE_Y, P.FALANTE_Y + P.FALANTE[2]
    for xp, lado in ((P.FALANTE_X0, -1), (P.FALANTE_X0 + P.FALANTE[0], 1)):
        b = bloco(min(xp - 1.5 * lado, xp + 2.5 * lado), fy0 - 1.4, P.FUNDO - 0.1,
                  max(xp - 1.5 * lado, xp + 2.5 * lado), fy1 + 1.4, P.FUNDO + 12.0)
        b = b.cut(bloco(min(xp - 1.6 * lado, xp + 0.2 * lado), fy0 - 0.2, P.FUNDO,
                        max(xp - 1.6 * lado, xp + 0.2 * lado), fy1 + 0.2, P.FUNDO + 13.0))
        f = f.union(b)
    # grade do som na parede de cima
    for i in range(10):
        x = 11.0 + i * 2.6
        f = f.cut(caixa_arredondada(x, P.CAV_Y1 - 0.5, x + 1.3, Y1 + 0.5, 5.0, 18.5, 0.6))

    # berço do microfone (deitado, virado para baixo) e os furos do som
    mx, mz, md = P.MIC_X, P.MIC_Z, P.MIC_D
    b = bloco(mx - md / 2 - 1.6, P.CAV_Y0 - 0.1, P.FUNDO - 0.1, mx + md / 2 + 1.6, P.MIC_L - 0.6, mz + 1.0)
    b = b.cut(cilindro_y(mx, mz, P.CAV_Y0 - 0.2, P.MIC_L + 0.2, md + 0.3))
    f = f.union(b)
    for dx, dz in ((-1.7, -1.0), (1.7, -1.0), (0.0, 1.9)):
        f = f.cut(cilindro_y(mx + dx, mz + dz, Y0 - 0.5, P.CAV_Y0 + 0.5, 1.2))

    # USB-C da ESP32 (o de carregar e gravar): a abertura deixa passar a capa do cabo
    usb_z = P.Z_LILYGO_CIMA + 1.6
    usb_y = P.LILYGO_Y + P.L.usb_esp_y
    janela = (cq.Workplane("YZ").workplane(offset=P.CAV_X1 - 0.5).center(usb_y, usb_z)
              .rect(12.4, 6.4).extrude(P.PAREDE + 1.0).edges("|X").fillet(2.6))
    f = f.cut(janela)
    # os USB e os QWIIC passam 0,5 mm da placa: canais por dentro da parede,
    # abertos até em cima (a LILYGO desce reta para dentro do fundo)
    for yc in (P.L.usb_esp_y, P.L.usb_modem_y):
        y = P.LILYGO_Y + yc
        f = f.cut(bloco(P.CAV_X1 - 0.1, y - 5.0, P.Z_LILYGO_CIMA - 1.2, P.CAV_X1 + 0.7, y + 5.0, ZD + 1))
    q0 = P.LILYGO_Y + P.L.qwiic[0][0] - 0.3
    q1 = P.LILYGO_Y + P.L.qwiic[1][1] + 0.3
    f = f.cut(bloco(P.CAV_X1 - 0.1, q0, P.Z_LILYGO - 3.5, P.CAV_X1 + 0.7, q1, ZD + 1))

    # guia da antena (0,3 mm), na parede da esquerda por dentro
    f = f.cut(bloco(P.CAV_X0 - 0.3, 14.0, P.FUNDO + 0.4, P.CAV_X0 + 0.1, 125.5, P.FUNDO + 0.4 + 20.6))

    # rasgos das linguetas de cima
    for x in LING["xs"]:
        f = f.cut(bloco(x - LING["larg"] / 2 - 0.5, P.CAV_Y1 - 0.1, RASGO["z0"], x + LING["larg"] / 2 + 0.5,
                        P.CAV_Y1 + RASGO["fundo"], RASGO["z1"]))

    # parafusos de baixo: furo no fundo, com escareado por fora (cabeça chata)
    for x in PARAF_X:
        f = f.cut(cilindro_z(x, PARAF_Y, -0.5, P.FUNDO + 0.5, 2.4))
        cone = (cq.Workplane("XY").workplane(offset=-0.01).center(x, PARAF_Y)
                .circle(2.2).workplane(offset=1.2).circle(1.2).loft())
        f = f.cut(cone)

    # o nome atrás
    nome = (cq.Workplane("XY").workplane(offset=-0.01)
            .center((X0 + X1) / 2, (Y0 + Y1) / 2 - 30)
            .text("TIJOLO", 7.0, 0.45, fontPath=FONTE, halign="center", valign="center", combine=False)
            .mirror("YZ", basePointVector=((X0 + X1) / 2, 0, 0)))
    f = f.cut(nome)
    return f


# ---------------------------------------------------------------------------
# a frente
# ---------------------------------------------------------------------------
def frente():
    f = caixa_arredondada(X0, Y0, X1, Y1, ZD, P.Z_TOPO, R_FORA)
    f = f.faces(">Z").edges().fillet(BORDA)
    f = f.cut(caixa_arredondada(P.CAV_X0, P.CAV_Y0, P.CAV_X1, P.CAV_Y1, ZD - 1, P.Z_FRENTE_DENTRO, R_DENTRO))

    # janela da tela (a imagem e um pouco de margem), com chanfro por fora
    m = 0.6
    jx0, jx1 = P.TELA_X + 3.0 - m, P.TELA_X + P.T.largura - 3.0 + m
    jy0, jy1 = P.IMAGEM_BASE - m, P.IMAGEM_TOPO + m
    f = f.cut(caixa_arredondada(jx0, jy0, jx1, jy1, P.Z_FRENTE_DENTRO - 0.5, P.Z_TOPO + 1, 1.0))
    chanfro = (cq.Workplane("XY").workplane(offset=P.Z_TOPO - 0.8)
               .center((jx0 + jx1) / 2, (jy0 + jy1) / 2).rect(jx1 - jx0, jy1 - jy0)
               .workplane(offset=0.81).rect(jx1 - jx0 + 1.6, jy1 - jy0 + 1.6).loft())
    f = f.cut(chanfro)

    # poços das teclas: guiam a tecla por baixo da frente; furos na frente
    teclas = teclas_posicoes()
    lp = P.TECLA_LARG + 2 * ABA_TECLA + 2 * 0.2
    ap = P.TECLA_ALT + 2 * ABA_TECLA + 2 * 0.2
    z_poco0 = P.Z_PLACA_FRENTE + P.CHAVE_CORPO + 0.25
    for _, x, y, _ in teclas:
        f = f.union(caixa_arredondada(x - lp / 2 - POCO_PAREDE, y - ap / 2 - POCO_PAREDE, x + lp / 2 + POCO_PAREDE,
                                      y + ap / 2 + POCO_PAREDE, z_poco0, P.Z_FRENTE_DENTRO + 0.1, 1.6))
    for _, x, y, _ in teclas:
        f = f.cut(caixa_arredondada(x - lp / 2, y - ap / 2, x + lp / 2, y + ap / 2, z_poco0 - 0.1, P.Z_FRENTE_DENTRO, 1.4))
        f = f.cut(caixa_arredondada(x - P.TECLA_LARG / 2 - FOLGA_TECLA, y - P.TECLA_ALT / 2 - FOLGA_TECLA,
                                    x + P.TECLA_LARG / 2 + FOLGA_TECLA, y + P.TECLA_ALT / 2 + FOLGA_TECLA,
                                    P.Z_FRENTE_DENTRO - 0.2, P.Z_TOPO + 1, 1.4))
    # pinos que encostam na placa entre as teclas (ela não verga quando você aperta)
    for x in (P.col_x(5.5), P.col_x(11.5)):
        for y in ((P.centro_chave(1, P.K_TECLAS[1])[1] + P.centro_chave(1, P.K_TECLAS[2])[1]) / 2,
                  (P.centro_chave(1, P.K_TECLAS[0])[1] + P.centro_chave(1, P.K_BOTOES)[1]) / 2):
            f = f.union(cilindro_z(x, y, P.Z_PLACA_FRENTE + 0.12, z_poco0 + 0.1, 3.0))

    # pilares dos parafusos de baixo (descem até perto do fundo; o parafuso entra por trás)
    for x in PARAF_X:
        pilar = cilindro_z(x, PARAF_Y, PILAR_Z0, P.Z_FRENTE_DENTRO + 0.1, PILAR_D)
        pilar = pilar.union(bloco(x - PILAR_D / 2, P.CAV_Y0 + 0.15, ZD - 2.0, x + PILAR_D / 2, PARAF_Y, P.Z_FRENTE_DENTRO + 0.1))
        f = f.union(pilar.cut(cilindro_z(x, PARAF_Y, PILAR_Z0 - 0.1, PILAR_Z0 + 9.0, 1.6)))

    # linguetas de cima: descem por dentro da parede de cima; o dente tem rampa
    # embaixo (entra apertando) e 30° em cima (sai puxando firme)
    ly1 = P.CAV_Y1 - LING["folga"]
    for x in LING["xs"]:
        x0, x1 = x - LING["larg"] / 2, x + LING["larg"] / 2
        f = f.union(bloco(x0, ly1 - LING["esp"], LING["z0"], x1, ly1, ZD + 0.5))
        z0, z1, d = LING["z0"], LING["z0"] + LING["dente_alt"], LING["dente"]
        perfil = (cq.Workplane("YZ").workplane(offset=x0)
                  .polyline([(ly1 - 0.05, z0), (ly1 + d, z0 + 0.6), (ly1 + d, z0 + 0.75), (ly1 - 0.05, z0 + 1.2)]).close()
                  .extrude(x1 - x0))
        f = f.union(perfil)
    return f


# ---------------------------------------------------------------------------
# as teclas
# ---------------------------------------------------------------------------
def tecla(rotulo: str, cor: str):
    """Uma tecla em pé, com a base em z = 0 (o recesso do botãozinho embaixo, a legenda gravada em cima)"""
    lg, al = P.TECLA_LARG, P.TECLA_ALT
    altura = P.Z_TOPO + SAI_TECLA - Z_TECLA0
    aba = (cq.Workplane("XY").rect(lg + 2 * ABA_TECLA, al + 2 * ABA_TECLA).extrude(ABA_ALT)
           .edges("|Z").fillet(1.6))
    corpo = (cq.Workplane("XY").workplane(offset=ABA_ALT - 0.01).rect(lg, al).extrude(altura - ABA_ALT + 0.01)
             .edges("|Z").fillet(1.4).faces(">Z").edges().fillet(0.6))
    t = aba.union(corpo)
    t = t.cut(cq.Workplane("XY").workplane(offset=-0.01).circle(RECESSO_PINO[0] / 2).extrude(RECESSO_PINO[1] + 0.01))
    fundo = 0.5
    topo = altura - fundo
    if cor != "tecla":
        txt = (cq.Workplane("XY").workplane(offset=topo)
               .text("☎", 5.2, fundo + 0.2, fontPath=FONTE, halign="center", valign="center", combine=False))
        return t.cut(txt)
    letras = P.LETRAS.get(rotulo, "")
    n = (cq.Workplane("XY").workplane(offset=topo).center(-2.4 if letras else 0, 0)
         .text(rotulo, 4.6, fundo + 0.2, fontPath=FONTE, halign="center", valign="center", combine=False))
    t = t.cut(n)
    if letras:
        tam = 2.2 if len(letras) <= 3 else 1.9
        l = (cq.Workplane("XY").workplane(offset=topo).center(2.9, -0.1)
             .text(letras, tam, fundo + 0.2, fontPath=FONTE, halign="center", valign="center", combine=False))
        t = t.cut(l)
    return t


def todas_as_teclas(cor_filtro):
    """As teclas de uma cor, lado a lado para imprimir (a base no z = 0)"""
    v = [(r, c) for r, _, _, c in teclas_posicoes() if (c == "tecla") == (cor_filtro == "tecla") and
         (cor_filtro == "tecla" or c == cor_filtro)]
    pecas = None
    for i, (r, c) in enumerate(v):
        t = tecla(r, c).translate(((i % 3) * 17.0, -(i // 3) * 12.0, 0))
        pecas = t if pecas is None else pecas.union(t)
    return pecas


def teclas_no_lugar():
    pecas = None
    for r, x, y, c in teclas_posicoes():
        t = tecla(r, c).translate((x, y, Z_TECLA0))
        pecas = t if pecas is None else pecas.union(t)
    return pecas


def salvar(peca, nome, pasta):
    caminho = os.path.join(pasta, nome)
    cq.exporters.export(peca, caminho, tolerance=0.02, angularTolerance=0.15)
    return caminho


# ---------------------------------------------------------------------------
# o miolo (só para conferir e para ver): as placas e as peças, em blocos
# ---------------------------------------------------------------------------
def miolo():
    """Cada peça de dentro como um sólido simples: [(nome, sólido)]"""
    v = []
    lx, ly = P.LILYGO_X, P.LILYGO_Y
    zl, zc = P.Z_LILYGO, P.Z_LILYGO_CIMA
    pcb = caixa_arredondada(lx, ly, lx + P.L.largura, ly + P.L.altura, zl, zc, P.L.raio)
    for (fx, fy) in P.L.furos_fixacao:
        x, y = P.lilygo_para_tijolo(fx, fy)
        pcb = pcb.cut(cilindro_z(x, y, zl - 1, zc + 1, 2.5))
    v.append(("placa da LILYGO", pcb))
    for p in P.PECAS_CIMA:
        if p["altura"] < 1.0:
            continue
        x0, y0 = P.lilygo_para_tijolo(p["x0"], p["y0"])
        x1, y1 = P.lilygo_para_tijolo(p["x1"], p["y1"])
        v.append((p["nome"], bloco(x0, y0, zc, x1, y1, zc + p["altura"])))
    for p in P.PECAS_BAIXO:
        if p["fundura"] < 1.0 or "BATTERY" in p["nome"]:
            continue
        x0, y0 = P.lilygo_para_tijolo(p["x0"], p["y0"])
        x1, y1 = P.lilygo_para_tijolo(p["x1"], p["y1"])
        v.append((p["nome"] + " (embaixo)", bloco(x0, y0, zl - p["fundura"], x1, y1, zl)))
    s = P.L.suporte_18650
    hx0, hy0 = P.lilygo_para_tijolo(s[0], s[1])
    hx1, hy1 = P.lilygo_para_tijolo(s[2], s[3])
    v.append(("ponta do suporte da pilha", bloco(hx0, hy0, zl - 15.0, hx1, hy0 + 2.0, zl)))
    v.append(("ponta do suporte da pilha 2", bloco(hx0, hy1 - 2.0, zl - 15.0, hx1, hy1, zl)))
    v.append(("pilha 18650", cilindro_y((hx0 + hx1) / 2, zl - 9.3, hy0 + 2.5, hy1 - 2.5, 18.6)))
    # as barras: dois plásticos entre as placas, os pinos atravessando
    for col in (P.COL_J1, P.COL_J2):
        x = P.col_x(col)
        y0b, y1b = P.k_y(P.K_PINO1 - 15) - 1.27, P.k_y(P.K_PINO1) + 1.27
        v.append((f"barra {col}", bloco(x - 1.25, y0b, zc, x + 1.25, y1b, P.Z_PLACA)))
    v.append(("placa da frente", bloco(0, P.PLACA_Y0, P.Z_PLACA, P.PLACA_LARGURA, P.PLACA_Y0 + P.PLACA_ALTURA,
                                       P.Z_PLACA_FRENTE)))
    for r, x, y, c in teclas_posicoes():
        v.append((f"chave {r}", bloco(x - 3, y - 3, P.Z_PLACA_FRENTE, x + 3, y + 3, P.Z_PLACA_FRENTE + P.CHAVE_CORPO)))
        v.append((f"botãozinho {r}", cilindro_z(x, y, P.Z_PLACA_FRENTE + P.CHAVE_CORPO - 0.01,
                                                P.Z_PLACA_FRENTE + P.CHAVE_ALTURA, 3.5)))
    v.append(("plástico da tela", bloco(P.col_x(2) - 1.27, P.k_y(P.K_TELA) - 1.25, P.Z_PLACA_FRENTE,
                                        P.col_x(15) + 1.27, P.k_y(P.K_TELA) + 1.25, P.Z_TELA)))
    v.append(("plaquinha da tela", bloco(P.TELA_X, P.TELA_BASE, P.Z_TELA, P.TELA_X + P.T.largura, P.TELA_TOPO,
                                         P.Z_TELA + P.T.pcb)))
    v.append(("vidro da tela", bloco(P.TELA_X, P.VIDRO_BASE, P.Z_TELA + P.T.pcb, P.TELA_X + P.T.largura, P.VIDRO_TOPO,
                                     P.Z_VIDRO)))
    v.append(("alto-falante", bloco(P.FALANTE_X0, P.FALANTE_Y, P.FUNDO + 0.05, P.FALANTE_X0 + P.FALANTE[0],
                                    P.FALANTE_Y + P.FALANTE[2], P.FUNDO + P.FALANTE[1])))
    v.append(("microfone", cilindro_y(P.MIC_X, P.MIC_Z, P.CAV_Y0 + 0.05, P.MIC_L, P.MIC_D)))
    # os pinos da tela saem 4,4 mm atrás da placa, na linha 1
    v.append(("pinos da tela atrás", bloco(P.col_x(2) - 0.4, P.k_y(P.K_TELA) - 0.4, P.Z_PLACA - 4.4,
                                           P.col_x(15) + 0.4, P.k_y(P.K_TELA) + 0.4, P.Z_PLACA)))
    return v


def conferir():
    """O que bate em quê (volume da interseção, mm³)"""
    pecas = miolo()
    caixa = [("fundo", fundo()), ("frente", frente()), ("teclas", teclas_no_lugar())]
    erros = []
    for nc, c in caixa:
        for np_, p in pecas:
            if nc == "teclas" and np_.startswith("botãozinho"):
                continue                                  # a tecla encosta no botãozinho de propósito
            try:
                vol = c.intersect(p).val().Volume()
            except Exception:
                vol = 0.0
            if vol > 0.05:
                erros.append(f"{nc} × {np_}: {vol:.2f} mm³")
    # as teclas dentro da frente (o furo e o poço), e a frente no fundo
    for (na, a), (nb, b) in (((caixa[1]), (caixa[2])), ((caixa[0]), (caixa[1]))):
        vol = a.intersect(b).val().Volume()
        if vol > 0.05:
            erros.append(f"{na} × {nb}: {vol:.2f} mm³")
    return erros


def salvar_miolo(pasta):
    pecas = miolo()
    tudo = None
    for _, p in pecas:
        tudo = p if tudo is None else tudo.union(p)
    return salvar(tudo, "miolo.stl", pasta)


def tecla_lisa(rotulo, cor):
    """A tecla sem a legenda (para a vista 3D do guia: bem mais leve)"""
    lg, al = P.TECLA_LARG, P.TECLA_ALT
    altura = P.Z_TOPO + SAI_TECLA - Z_TECLA0
    aba = (cq.Workplane("XY").rect(lg + 2 * ABA_TECLA, al + 2 * ABA_TECLA).extrude(ABA_ALT).edges("|Z").fillet(1.6))
    corpo = (cq.Workplane("XY").workplane(offset=ABA_ALT - 0.01).rect(lg, al).extrude(altura - ABA_ALT + 0.01)
             .edges("|Z").fillet(1.4).faces(">Z").edges().fillet(0.6))
    return aba.union(corpo)


def salvar_visual(pasta):
    """STL leves para a vista 3D do guia"""
    os.makedirs(pasta, exist_ok=True)
    feitos = []

    def leve(peca, nome):
        caminho = os.path.join(pasta, nome)
        cq.exporters.export(peca, caminho, tolerance=0.08, angularTolerance=0.4)
        feitos.append(caminho)

    leve(fundo(), "fundo.stl")
    leve(frente(), "frente.stl")
    pretas = verde = vermelha = None
    for r, x, y, c in teclas_posicoes():
        t = tecla_lisa(r, c).translate((x, y, Z_TECLA0))
        if c == "verde":
            verde = t
        elif c == "vermelho":
            vermelha = t
        else:
            pretas = t if pretas is None else pretas.union(t)
    leve(pretas, "teclas.stl")
    leve(verde, "tecla_verde.stl")
    leve(vermelha, "tecla_vermelha.stl")
    pecas = miolo()
    grupos = {"lilygo": [], "placa": [], "tela": [], "pilha": [], "som": []}
    for n, p in pecas:
        g = ("pilha" if "pilha" in n else "tela" if "tela" in n else
             "placa" if (n.startswith(("placa da frente", "chave", "botãozinho", "barra"))) else
             "som" if n in ("alto-falante", "microfone") else "lilygo")
        grupos[g].append(p)
    for g, lista in grupos.items():
        tudo = None
        for p in lista:
            tudo = p if tudo is None else tudo.union(p)
        leve(tudo, f"miolo_{g}.stl")
    return feitos


if __name__ == "__main__":
    pasta = os.path.join(os.path.dirname(AQUI), "saida", "caixa")
    os.makedirs(pasta, exist_ok=True)
    so = sys.argv[1:] or ["fundo", "frente", "teclas", "visual", "conferir"]
    if "fundo" in so:
        f = fundo()
        print(salvar(f, "fundo.stl", pasta))
        cq.exporters.export(f, os.path.join(pasta, "fundo.step"))
    if "frente" in so:
        fr = frente()
        # para imprimir: a cara para baixo
        print(salvar(fr.rotate((0, 0, 0), (0, 1, 0), 180).translate((0, 0, P.Z_TOPO)), "frente.stl", pasta))
        cq.exporters.export(fr, os.path.join(pasta, "frente.step"))
        print(salvar(fr, "frente_no_lugar.stl", pasta))
    if "teclas" in so:
        print(salvar(todas_as_teclas("tecla"), "teclas.stl", pasta))
        print(salvar(todas_as_teclas("verde"), "tecla_verde.stl", pasta))
        print(salvar(todas_as_teclas("vermelho"), "tecla_vermelha.stl", pasta))
        print(salvar(teclas_no_lugar(), "teclas_no_lugar.stl", pasta))
    if "visual" in so:
        for arq in salvar_visual(os.path.join(pasta, "visual")):
            print(arq)
    if "conferir" in so:
        erros = conferir()
        print("conferência da caixa:", "ok" if not erros else "")
        for e in erros:
            print("  ", e)
        if erros:
            sys.exit(1)
