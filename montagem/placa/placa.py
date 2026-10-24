"""
A placa de circuito da frente do Tijolo (KiCad 7, gerada por código).

    python3 placa/placa.py          (da pasta montagem/)

Sai em saida/placa/: tijolo-frente.kicad_pcb (abre no KiCad), o relatório do
DRC, os Gerbers e a furação num .zip para a JLCPCB, a lista de peças e as
imagens das duas faces.

O que vai na placa (2 camadas):
  - atrás: as duas barras fêmea de perna longa que encaixam nos pinos da
    LILYGO (J1 e J2) e os capacitores da tela;
  - na frente: os 14 pinos da tela (de cabeça para baixo, os pinos embaixo),
    os 12 contatos do teclado (pentes intercalados, para as pastilhas da
    borracha do teclado), os botões verde e vermelho (chaves tácteis 6×6) e
    uma fileira de furos livres para o futuro.
O roteamento é automático (roteador.py); o GND enche as duas faces.
"""
import math
import os
import subprocess
import sys
import zipfile

import pcbnew
from shapely.geometry import Point, Polygon, box
from shapely.ops import unary_union

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(AQUI))
import projeto as P  # noqa: E402
from roteador import FRENTE, TRAS, Grade, SemCaminho  # noqa: E402

SAIDA = os.path.join(os.path.dirname(AQUI), "saida", "placa")
LIB = "/usr/share/kicad/footprints"
OX, OY = 100.0, 250.0                      # onde a placa fica na folha do KiCad
TRILHA, FOLGA, VIA, VIA_FURO = 0.3, 0.2, 0.6, 0.3
FOLGA_ZONA = 0.3


def mm(v):
    return pcbnew.FromMM(float(v))


def kp(x, y):
    """Tijolo (Y para cima) → KiCad (Y para baixo)"""
    return pcbnew.VECTOR2I(mm(x + OX), mm(OY - y))


def tj(v):
    """KiCad → Tijolo"""
    return pcbnew.ToMM(v.x) - OX, OY - pcbnew.ToMM(v.y)


# ---------------------------------------------------------------------------
# os pentes do teclado
# ---------------------------------------------------------------------------
def pente(diametro=6.0, espinha=0.5, dedo=0.35, vao=0.3):
    """Dois pentes intercalados num círculo: (A, B) em coordenadas locais (mm, Y para cima).
    A tem a espinha à esquerda; B, à direita. A pastilha de carbono da borracha liga os dois."""
    r = diametro / 2
    disco = Point(0, 0).buffer(r, 64)
    dentro = Point(0, 0).buffer(r - espinha, 64)
    anel = disco.difference(dentro)
    anel_a = anel.intersection(box(-r, -r, -vao / 2, r))
    anel_b = anel.intersection(box(vao / 2, -r, r, r))
    faixas_a, faixas_b = [], []
    passo = dedo + vao
    n = int((2 * (r - espinha) - vao) // passo)
    y0 = -(n - 1) * passo / 2
    for k in range(n):
        y = y0 + k * passo
        faixa = box(-r, y - dedo / 2, r, y + dedo / 2).intersection(dentro)
        (faixas_a if k % 2 == 0 else faixas_b).append(faixa)
    # B primeiro (a espinha da direita e os dedos dela, longe da espinha A);
    # depois A, longe de tudo de B
    b = unary_union([anel_b] + [f.difference(anel_a.buffer(vao, 32)) for f in faixas_b])
    a = unary_union([anel_a] + [f.difference(b.buffer(vao, 32)) for f in faixas_a])
    a = a.buffer(0.01).buffer(-0.01)
    b = b.buffer(0.01).buffer(-0.01)
    if a.geom_type != "Polygon" or b.geom_type != "Polygon":
        raise ValueError("pente partido")
    assert a.distance(b) >= vao - 0.03, a.distance(b)
    return a, b


# ---------------------------------------------------------------------------
# a placa
# ---------------------------------------------------------------------------
class Placa:
    def __init__(self):
        self.b = pcbnew.BOARD()
        self.nets = {}
        self.pads = {}          # net → [(geometria, camadas)]
        self.obstaculos = []    # (geometria, camadas) sem net
        self.pecas = []         # (referência, valor, footprint, lado, x, y, rot) para a lista de peças
        self.furos = []         # (x, y, diâmetro) de todos os furos
        self._regras()
        self.contorno = self._contorno()

    # ------------------------------------------------------------- básicos
    def _regras(self):
        ds = self.b.GetDesignSettings()
        ds.m_TrackMinWidth = mm(0.2)
        ds.m_MinClearance = mm(FOLGA)
        ds.m_ViasMinSize = mm(0.5)
        ds.m_MinThroughDrill = mm(0.3)
        ds.m_CopperEdgeClearance = mm(0.3)
        ds.m_HoleClearance = mm(0.25)
        nc = ds.m_NetSettings.m_DefaultNetClass
        nc.SetClearance(mm(FOLGA))
        nc.SetTrackWidth(mm(TRILHA))
        nc.SetViaDiameter(mm(VIA))
        nc.SetViaDrill(mm(VIA_FURO))
        ds.SetBoardThickness(mm(P.PCB_ESP))

    def net(self, nome):
        if nome is None:
            return None
        if nome not in self.nets:
            ni = pcbnew.NETINFO_ITEM(self.b, nome)
            self.b.Add(ni)
            self.nets[nome] = ni
        return self.nets[nome]

    def _contorno(self):
        x0, y0, x1, y1 = P.PCB_X0, P.PCB_Y0, P.PCB_X1, P.PCB_Y1
        forma = box(x0, y0, x1, y1).buffer(-1.5).buffer(1.5, 16)
        s = pcbnew.PCB_SHAPE(self.b, pcbnew.SHAPE_T_POLY)
        s.SetPolyPoints([kp(x, y) for x, y in list(forma.exterior.coords)[:-1]])
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(mm(0.1))
        s.SetFilled(False)
        self.b.Add(s)
        return forma

    def texto(self, t, x, y, tam=1.0, camada=pcbnew.F_SilkS, rot=0, negrito=False):
        tx = pcbnew.PCB_TEXT(self.b)
        tx.SetText(t)
        tx.SetPosition(kp(x, y))
        tx.SetLayer(camada)
        tx.SetTextSize(pcbnew.VECTOR2I(mm(tam), mm(tam)))
        tx.SetTextThickness(mm(max(0.16, tam * (0.2 if negrito else 0.15))))
        if rot:
            tx.SetTextAngleDegrees(rot)
        if camada in (pcbnew.B_SilkS, pcbnew.B_Cu):
            tx.SetMirrored(True)
        self.b.Add(tx)

    def linha(self, pts, camada=pcbnew.F_SilkS, largura=0.15):
        for a, c in zip(pts, pts[1:]):
            s = pcbnew.PCB_SHAPE(self.b, pcbnew.SHAPE_T_SEGMENT)
            s.SetStart(kp(*a))
            s.SetEnd(kp(*c))
            s.SetLayer(camada)
            s.SetWidth(mm(largura))
            self.b.Add(s)

    # ------------------------------------------------------- pads → router
    def _registra_pads(self, fp):
        """as geometrias (Tijolo) dos pads de um footprint, por net"""
        for pad in fp.Pads():
            x, y = tj(pad.GetPosition())
            sx, sy = pcbnew.ToMM(pad.GetSize().x), pcbnew.ToMM(pad.GetSize().y)
            rot = pad.GetOrientationDegrees()
            if round(rot) % 180 == 90:
                sx, sy = sy, sx
            forma = pad.GetShape()
            if forma == pcbnew.PAD_SHAPE_CUSTOM:
                continue                      # os pentes entram à parte
            if forma == pcbnew.PAD_SHAPE_CIRCLE:
                g = Point(x, y).buffer(sx / 2, 24)
            elif forma == pcbnew.PAD_SHAPE_OVAL:
                r = min(sx, sy) / 2
                g = box(x - sx / 2 + r, y - sy / 2 + r, x + sx / 2 - r, y + sy / 2 - r).buffer(r, 16)
            else:
                g = box(x - sx / 2, y - sy / 2, x + sx / 2, y + sy / 2)
            camadas = []
            if pad.IsOnLayer(pcbnew.F_Cu):
                camadas.append(FRENTE)
            if pad.IsOnLayer(pcbnew.B_Cu):
                camadas.append(TRAS)
            if pad.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH):
                self.furos.append((x, y, pcbnew.ToMM(pad.GetDrillSize().x)))
            nome = pad.GetNetname()
            if nome:
                self.pads.setdefault(nome, []).append((g, camadas))
            else:
                self.obstaculos.append((g, camadas))
            if pad.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
                self.obstaculos.append((Point(x, y).buffer(pcbnew.ToMM(pad.GetDrillSize().x) / 2), [FRENTE, TRAS]))

    def footprint_lib(self, lib, nome, ref, valor, x, y, rot=0, atras=False, nets=None):
        fp = pcbnew.FootprintLoad(os.path.join(LIB, lib + ".pretty"), nome)
        fp.SetReference(ref)
        fp.SetValue(valor)
        fp.SetPosition(kp(x, y))
        if rot:
            fp.SetOrientationDegrees(rot)
        self.b.Add(fp)                      # antes de virar: o Flip precisa da placa
        if atras:
            fp.Flip(fp.GetPosition(), True)
        for pad in fp.Pads():
            n = (nets or {}).get(pad.GetName())
            if n:
                pad.SetNet(self.net(n))
                if n == "GND":
                    pad.SetThermalSpokeAngleDegrees(45)      # entre vizinhos a 2,54: só cabe na diagonal
        self._registra_pads(fp)
        self.pecas.append((ref, valor, f"{lib}:{nome}", "trás" if atras else "frente", x, y, rot))
        return fp

    def footprint_novo(self, ref, valor, x, y, atras=False):
        fp = pcbnew.FOOTPRINT(self.b)
        fp.SetReference(ref)
        fp.SetValue(valor)
        fp.SetPosition(kp(0, 0) if False else pcbnew.VECTOR2I(0, 0))
        fp.Reference().SetVisible(False)
        fp.Value().SetVisible(False)
        return fp

    def pad_tht(self, fp, nome, dx, dy, d=1.7, furo=1.0, quadrado=False):
        p = pcbnew.PAD(fp)
        p.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
        p.SetShape(pcbnew.PAD_SHAPE_RECT if quadrado else pcbnew.PAD_SHAPE_CIRCLE)
        p.SetSize(pcbnew.VECTOR2I(mm(d), mm(d)))
        p.SetDrillSize(pcbnew.VECTOR2I(mm(furo), mm(furo)))
        p.SetLayerSet(pcbnew.PAD.PTHMask())
        p.SetName(nome)
        v = pcbnew.VECTOR2I(mm(dx), mm(-dy))
        p.SetPos0(v)
        p.SetPosition(v)
        fp.Add(p)
        return p

    def pad_custom(self, fp, nome, poligono, ancora):
        """pad SMD de cobre com a forma do polígono (coordenadas locais, Y para cima), sem pasta"""
        p = pcbnew.PAD(fp)
        p.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
        p.SetShape(pcbnew.PAD_SHAPE_CUSTOM)
        p.SetAnchorPadShape(pcbnew.PAD_SHAPE_CIRCLE)
        p.SetSize(pcbnew.VECTOR2I(mm(0.3), mm(0.3)))
        ls = pcbnew.LSET()
        ls.AddLayer(pcbnew.F_Cu)
        ls.AddLayer(pcbnew.F_Mask)
        p.SetLayerSet(ls)
        ax, ay = ancora
        pts = [pcbnew.VECTOR2I(mm(x - ax), mm(-(y - ay))) for x, y in list(poligono.exterior.coords)[:-1]]
        p.AddPrimitivePoly(pcbnew.VECTOR_VECTOR2I(pts), 0, True)
        p.SetName(nome)
        v = pcbnew.VECTOR2I(mm(ax), mm(-ay))
        p.SetPos0(v)
        p.SetPosition(v)
        fp.Add(p)
        return p

    def coloca(self, fp, x, y, nets=None):
        fp.SetPosition(kp(x, y))
        self.b.Add(fp)
        for pad in fp.Pads():
            n = (nets or {}).get(pad.GetName())
            if n:
                pad.SetNet(self.net(n))
        self._registra_pads(fp)


def montar():
    pl = Placa()

    # --- as barras fêmea (atrás), nos pinos da LILYGO
    for barra, lista in (("J1", P.J1), ("J2", P.J2)):
        x, y = P.pino_barra(barra, 1)
        nets = {}
        for i, nome in enumerate(lista):
            if nome in ("3V3", "GND") or nome in P.TELA_GPIO.values() or nome in (
                    P.GPIO_FILEIRA + P.GPIO_COLUNA + [P.GPIO_VERDE, P.GPIO_VERMELHO]):
                nets[str(i + 1)] = nome
            elif nome in ("IO2", "IO3", "IO48", "VBAT"):
                nets[str(i + 1)] = nome                      # vão para os furos livres
        pl.footprint_lib("Connector_PinSocket_2.54mm", "PinSocket_1x16_P2.54mm_Vertical", barra,
                         "barra fêmea 1×16, perna longa", x, y, atras=True, nets=nets)
        # os nomes dos pinos, atrás e na frente
        for i, nome in enumerate(lista):
            px, py = P.pino_barra(barra, i + 1)
            lado = -1 if barra == "J2" else 1
            pl.texto(nome, px + lado * 3.0, py, tam=0.8, camada=pcbnew.B_SilkS)

    # --- a tela: 14 pinos, a tela de cabeça para baixo
    fp = pl.footprint_novo("TELA", "tela ILI9341 2,4\" (pinos soldados por cima)", 0, 0)
    nets = {}
    for i in range(1, 15):
        x, y = P.pino_tela(i)
        pl.pad_tht(fp, str(i), x, y, d=1.7, furo=1.0, quadrado=(i == 1))
        nome = P.TELA_PINOS[i - 1]
        if nome in P.TELA_GPIO:
            nets[str(i)] = P.TELA_GPIO[nome]
    pl.coloca(fp, 0, 0, nets)
    for i in range(1, 15):
        x, y = P.pino_tela(i)
        nome = P.TELA_PINOS[i - 1].replace("SDI(MOSI)", "MOSI").replace("SDO(MISO)", "MISO")
        pl.texto(nome, x, y - 2.1, tam=0.8, rot=90)
    # o contorno da tela e do vidro (só desenho)
    tx0, tx1 = P.TELA_X, P.TELA_X + P.T.largura
    pl.linha([(tx0, P.TELA_BASE), (tx1, P.TELA_BASE)], pcbnew.F_Fab)
    pl.linha([(tx0, P.TELA_BASE), (tx0, P.PCB_Y1)], pcbnew.F_Fab)
    pl.linha([(tx1, P.TELA_BASE), (tx1, P.PCB_Y1)], pcbnew.F_Fab)
    pl.texto("TELA (de cabeça para baixo: os pinos embaixo)", P.TELA_CX, P.TELA_BASE + 12, tam=1.0)

    # --- os capacitores da tela, atrás, perto do VCC (fora da pilha)
    xv, yv = P.pino_tela(1)
    xg, _ = P.pino_tela(2)
    lado = 1 if xv > P.TELA_CX else -1
    cx = xv + lado * 1.0
    caps = [("C1", "100 nF", "C_0805_2012Metric", 0.0), ("C2", "22 µF", "C_1206_3216Metric", 3.2),
            ("C3", "22 µF", "C_1206_3216Metric", 6.6), ("C4", "100 pF", "C_0805_2012Metric", 10.0)]
    for ref, val, nome, dy in caps:
        pl.footprint_lib("Capacitor_SMD", nome, ref, val, cx, yv + 3.0 + dy, rot=0, atras=True,
                         nets={"1": "3V3", "2": "GND"} if lado < 0 else {"1": "GND", "2": "3V3"})
    # 100 nF no RESET da tela (pino 4)
    xr, _ = P.pino_tela(4)
    pl.footprint_lib("Capacitor_SMD", "C_0805_2012Metric", "C5", "100 nF", xr, yv + 3.0, rot=0, atras=True,
                     nets={"1": "IO9", "2": "GND"} if lado > 0 else {"1": "GND", "2": "IO9"})

    # --- os botões verde e vermelho
    for ref, x, gpio, rot in (("SW_VERDE", P.TECLAS_X[0], P.GPIO_VERDE, 0), ("SW_VERMELHO", P.TECLAS_X[2],
                                                                                 P.GPIO_VERMELHO, 0)):
        # o footprint tem o pino 1 em (0, 0) e o centro do corpo em (3,25; 2,25) (Y do KiCad para baixo)
        pl.footprint_lib("Button_Switch_THT", "SW_PUSH_6mm", ref, "chave táctil 6×6", x - 3.25, P.BOTOES_Y + 2.25,
                         nets={"1": "GND", "2": gpio})
    pl.texto("VERDE", P.TECLAS_X[0], P.BOTOES_Y + 4.3, tam=0.8)
    pl.texto("VERMELHO", P.TECLAS_X[2], P.BOTOES_Y + 4.3, tam=0.8)

    # --- o teclado: 12 pentes e a matriz já ligada
    # Cada tecla tem dois pentes intercalados: A (a fileira, espinha à esquerda, com um terminal
    # para fora à esquerda) e B (a coluna, espinha à direita, com uma via logo à direita).
    # As fileiras correm na frente, por cima dos contatos; as colunas, atrás, de via em via.
    a, bb = pente(P.K.contato)
    r_ = P.K.contato / 2
    pl.pentes = []
    pl.pre = {}              # net → [(camada, [(x, y), ...]) ...], vias em pl.pre_vias
    pl.pre_vias = {}
    for r, linha in enumerate(P.TECLAS):
        for c, rot in enumerate(linha):
            x, y = P.TECLAS_X[c], P.TECLAS_Y[r]
            fp = pl.footprint_novo(f"K{rot}", f"tecla {rot}", 0, 0)
            ga = Polygon([(px + x, py + y) for px, py in a.exterior.coords])
            ga = ga.union(box(x - r_ - 1.0, y - 0.45, x - r_ + 0.3, y + 0.45))     # o terminal da fileira
            gb = Polygon([(px + x, py + y) for px, py in bb.exterior.coords])
            gb = gb.union(box(x + r_ - 0.3, y - 0.45, x + r_ + 0.5, y + 0.45))     # o terminal da coluna
            pl.pad_custom(fp, "1", ga, (x - r_ - 0.6, y))
            pl.pad_custom(fp, "2", gb, (x + r_ + 0.2, y))
            fp.SetPosition(pcbnew.VECTOR2I(0, 0))
            pl.b.Add(fp)
            fila, col = P.GPIO_FILEIRA[r], P.GPIO_COLUNA[c]
            for pad in fp.Pads():
                pad.SetNet(pl.net(fila if pad.GetName() == "1" else col))
            fp.SetPosition(kp(0, 0))
            pl.pads.setdefault(fila, []).append((ga, [FRENTE]))
            pl.pads.setdefault(col, []).append((gb, [FRENTE]))
            pl.texto(rot, x, y + P.K.tecla[1] / 2 + 0.2, tam=0.9)
            pl.pentes.append((rot, x, y))
            tx, ty = P.K.tecla
            pl.linha([(x - tx / 2, y - ty / 2), (x + tx / 2, y - ty / 2), (x + tx / 2, y + ty / 2),
                      (x - tx / 2, y + ty / 2), (x - tx / 2, y - ty / 2)], pcbnew.F_Fab, 0.1)
            # a via da coluna, à direita do pente, e o toco até ela
            vx = x + r_ + 1.3
            pl.pre.setdefault(col, []).append((FRENTE, [(x + r_ + 0.2, y), (vx, y)]))
            pl.pre_vias.setdefault(col, []).append((vx, y))
    dy = r_ + 1.4
    for r in range(4):
        y = P.TECLAS_Y[r]
        fila = P.GPIO_FILEIRA[r]
        for c in range(2):
            xa = P.TECLAS_X[c] - r_ - 0.7
            xb = P.TECLAS_X[c + 1] - r_ - 0.7
            pl.pre.setdefault(fila, []).append((FRENTE, [(xa, y), (xa, y + dy), (xb, y + dy), (xb, y)]))
    for c in range(3):
        col = P.GPIO_COLUNA[c]
        vx = P.TECLAS_X[c] + r_ + 1.3
        pl.pre.setdefault(col, []).append((TRAS, [(vx, P.TECLAS_Y[3]), (vx, P.TECLAS_Y[0])]))
    # o que já está ligado vira um grupo só para o roteador
    pl.grupos = {}
    for net in list(P.GPIO_FILEIRA) + list(P.GPIO_COLUNA):
        juntos = [(gm, cm) for gm, cm in pl.pads[net] if gm.geom_type == "Polygon" and gm.area > 5]
        resto = [(gm, cm) for gm, cm in pl.pads[net] if not (gm.geom_type == "Polygon" and gm.area > 5)]
        pl.grupos[net] = [juntos] + [[pr] for pr in resto]

    # --- os furos livres (para o futuro): 3V3, GND, IO2, IO3, IO48, VBAT
    livres = ["3V3", "GND", "IO2", "IO3", "IO48", "VBAT"]
    fp = pl.footprint_novo("EXTRA", "furos livres", 0, 0)
    ex, ey = P.PCB_X1 - 2.2, P.LILYGO_Y + 30.0
    for i, n in enumerate(livres):
        pl.pad_tht(fp, str(i + 1), ex, ey + i * 2.54, d=1.6, furo=0.9, quadrado=(i == 0))
    pl.coloca(fp, 0, 0, {str(i + 1): n for i, n in enumerate(livres)})
    for i, n in enumerate(livres):
        pl.texto(n, ex - 2.8, ey + i * 2.54, tam=0.8)

    # --- textos
    pl.texto("TIJOLO", P.TELA_CX, P.TELA_BASE + 30, tam=3.0, negrito=True)
    pl.texto("placa da frente · v1 · 2026-10", P.TELA_CX, P.TELA_BASE + 25.5, tam=1.0)
    pl.texto("LILYGO DE COSTAS: J1/J2 nos pinos do lado da pilha", P.TELA_CX, P.TELA_BASE + 50,
             tam=1.0, camada=pcbnew.B_SilkS)
    pl.texto("J2", P.pino_barra("J2", 1)[0], P.pino_barra("J2", 1)[1] + 2.3, tam=1.0, camada=pcbnew.B_SilkS)
    pl.texto("J1", P.pino_barra("J1", 1)[0], P.pino_barra("J1", 1)[1] + 2.3, tam=1.0, camada=pcbnew.B_SilkS)
    return pl


# ---------------------------------------------------------------------------
# roteamento
# ---------------------------------------------------------------------------
ORDEM = ["IO16", "IO37", "IO17", "IO15", "IO38", "IO39", "IO40", "IO21", "IO35",
         "IO47", "IO9", "IO14", "IO36", "IO11", "IO12", "IO13", "IO2", "IO3", "IO48", "VBAT", "3V3", "GND"]


def rotear_com_tentativas(pl, tentativas=12):
    """roteia; se alguma net falha, ela passa para a frente da fila e tenta de novo"""
    ordem = list(ORDEM)
    for n in range(tentativas):
        rotas, falhas = rotear(pl, ordem)
        print(f"  tentativa {n + 1}: {('falharam ' + ', '.join(sorted(set(falhas)))) if falhas else 'tudo ligado'}")
        if not falhas:
            return rotas, falhas
        primeiro = []
        for f in falhas:
            if f not in primeiro:
                primeiro.append(f)
        ordem = primeiro + [x for x in ordem if x not in primeiro]
    return rotas, falhas


def rotear(pl, ordem=ORDEM):
    g = Grade(pl.contorno, passo=0.2, trilha=TRILHA, folga=FOLGA, via=VIA, borda=0.4, custo_via=14.0)
    ids = {net: k + 1 for k, net in enumerate(sorted(pl.pads))}
    for net, lista in pl.pads.items():
        for geom, camadas in lista:
            g.cobre(geom, ids[net], camadas)
    for geom, camadas in pl.obstaculos:
        g.proibe(geom, camadas)
    for x, y, d in pl.furos:
        g.sem_via(Point(x, y).buffer(d / 2 + 0.25 + VIA_FURO / 2 + 0.15, 16))
    # ao lado das barras, andar custa mais: as trilhas saem delas logo, sem emparedar os vizinhos
    for barra in ("J1", "J2"):
        bx, by1 = P.pino_barra(barra, 1)
        _, by0 = P.pino_barra(barra, 16)
        g.evita(box(bx - 2.6, by0 - 1.5, bx + 2.6, by1 + 1.5), (FRENTE, TRAS), 3.0)
    rotas = {}
    # o que já vem roteado (a matriz do teclado)
    for net, trechos in pl.pre.items():
        vias = pl.pre_vias.get(net, [])
        g.marca_rota(ids[net], trechos, vias)
        rotas.setdefault(net, []).append((trechos, vias))
    falhas = []
    for net in list(ordem) + [n for n in pl.pads if n not in ordem]:
        if net not in pl.pads:
            continue
        nid = ids[net]
        grupos = pl.grupos.get(net) or [[pr] for pr in pl.pads[net]]
        if len(grupos) < 2:
            continue

        def celulas(grupo):
            out = set()
            for gm, cm in grupo:
                out.update(g.celulas_de(gm, cm))
            return out

        arvore = celulas(grupos[0])
        if net in pl.pre:                    # as trilhas prontas também valem de origem
            for c, pts in pl.pre[net]:
                for (xa, ya), (xb, yb) in zip(pts, pts[1:]):
                    n = max(1, int(math.hypot(xb - xa, yb - ya) / 0.2))
                    for t in range(n + 1):
                        j, i = g.celula(xa + (xb - xa) * t / n, ya + (yb - ya) * t / n)
                        arvore.add((c, j, i))
        faltam = list(range(1, len(grupos)))
        c0 = unary_union([gm for gm, _ in grupos[0]]).centroid
        while faltam:
            faltam.sort(key=lambda k: unary_union([gm for gm, _ in grupos[k]]).centroid.distance(c0))
            k = faltam.pop(0)
            alvo = celulas(grupos[k])
            try:
                caminho = g.busca(nid, list(arvore), list(alvo), preferencia={FRENTE: "v", TRAS: "h"})
            except SemCaminho:
                falhas.append(net)
                continue
            trilhas, vias = g.pecas(caminho, nid)
            g.marca_rota(nid, trilhas, vias)
            rotas.setdefault(net, []).append((trilhas, vias))
            arvore.update(caminho)
            arvore.update(alvo)
    # a costura do GND: vias onde cabem, numa grade de 5 mm (as zonas ligam tudo depois)
    gid = ids["GND"]
    costura = []
    x0, y0, x1, y1 = pl.contorno.bounds
    y = y0 + 2.5
    while y < y1 - 2:
        x = x0 + 2.5
        while x < x1 - 2:
            j, i = g.celula(x, y)
            folga_ok = all(g.dono_via[cm][jj, ii] in (0, gid) for cm in (0, 1)
                           for jj in range(j - 3, j + 4) for ii in range(i - 3, i + 4)
                           if 0 <= jj < g.ny and 0 <= ii < g.nx)
            if folga_ok:
                costura.append(g.ponto(j, i))
                g.marca_rota(gid, [], [g.ponto(j, i)])
            x += 5.0
        y += 5.0
    rotas.setdefault("GND", []).append(([], costura))
    return rotas, falhas


def aplicar_rotas(pl, rotas):
    for net, lista in rotas.items():
        ni = pl.net(net)
        for trilhas, vias in lista:
            for c, pts in trilhas:
                for a, b in zip(pts, pts[1:]):
                    t = pcbnew.PCB_TRACK(pl.b)
                    t.SetStart(kp(*a))
                    t.SetEnd(kp(*b))
                    t.SetWidth(mm(TRILHA))
                    t.SetLayer(pcbnew.F_Cu if c == FRENTE else pcbnew.B_Cu)
                    t.SetNet(ni)
                    pl.b.Add(t)
            for x, y in vias:
                v = pcbnew.PCB_VIA(pl.b)
                v.SetPosition(kp(x, y))
                v.SetWidth(mm(VIA))
                v.SetDrill(mm(VIA_FURO))
                v.SetViaType(pcbnew.VIATYPE_THROUGH)
                v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
                v.SetNet(ni)
                pl.b.Add(v)


def _via(b, pos, net):
    v = pcbnew.PCB_VIA(b)
    v.SetPosition(pos)
    v.SetWidth(mm(VIA))
    v.SetDrill(mm(VIA_FURO))
    v.SetViaType(pcbnew.VIATYPE_THROUGH)
    v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    v.SetNet(net)
    b.Add(v)


def zonas_gnd(b, contorno):
    """o GND enchendo as duas faces (a placa precisa ter sido carregada do disco: o ZONE_FILLER
    de uma placa feita só na memória derruba o KiCad 7)"""
    gnd = b.FindNet("GND")
    for camada in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(b)
        z.SetLayer(camada)
        z.SetNet(gnd)
        ol = z.Outline()
        ol.NewOutline()
        x0, y0, x1, y1 = contorno.bounds
        for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
            ol.Append(mm(x + OX), mm(OY - y))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        z.SetMinThickness(mm(0.25))
        z.SetLocalClearance(mm(FOLGA_ZONA))
        z.SetThermalReliefGap(mm(0.4))
        z.SetThermalReliefSpokeWidth(mm(0.4))
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        b.Add(z)
    b.BuildConnectivity()
    pcbnew.ZONE_FILLER(b).Fill(b.Zones())


def costura(b, contorno, passo=5.0):
    """vias de GND ligando as duas faces, onde as duas zonas têm cobre"""
    gnd = b.FindNet("GND")
    zonas = {z.GetLayer(): z for z in b.Zones()}
    n = 0
    x0, y0, x1, y1 = contorno.bounds
    y = y0 + 2.5
    while y < y1 - 2:
        x = x0 + 2.5
        while x < x1 - 2:
            pos = kp(x, y)
            if all(zonas[c].HitTestFilledArea(c, pos, -mm(VIA / 2 + 0.3)) for c in (pcbnew.F_Cu, pcbnew.B_Cu)):
                _via(b, pos, gnd)
                n += 1
            x += passo
        y += passo
    b.BuildConnectivity()
    pcbnew.ZONE_FILLER(b).Fill(b.Zones())
    return n


# ---------------------------------------------------------------------------
# saída
# ---------------------------------------------------------------------------
def salvar(b, arq):
    b.SetFileName(arq)
    pcbnew.SaveBoard(arq, b)
    # as regras que o KiCad guarda no projeto: aceita térmico de 1 raio (os pinos das barras são
    # vizinhos demais para 2) e não reclama das bibliotecas (os footprints vêm do código)
    import json
    pro = arq[:-len(".kicad_pcb")] + ".kicad_pro"
    with open(pro, encoding="utf-8") as f:
        d = json.load(f)
    ds = d["board"]["design_settings"]
    ds["rules"]["min_resolved_spokes"] = 1
    for k in ("lib_footprint_issues", "lib_footprint_mismatch"):
        ds["rule_severities"][k] = "ignore"
    # os pads de GND também têm trilha própria (o roteador liga o GND inteiro): um raio de térmico basta
    ds["rule_severities"]["starved_thermal"] = "warning"
    with open(pro, "w", encoding="utf-8") as f:
        json.dump(d, f, indent=2)


def exportar(arq, nome="tijolo-frente"):
    b = pcbnew.LoadBoard(arq)
    drc = os.path.join(SAIDA, "drc.txt")
    pcbnew.WriteDRCReport(b, drc, pcbnew.EDA_UNITS_MILLIMETRES, True)
    gerb = os.path.join(SAIDA, "gerbers")
    os.makedirs(gerb, exist_ok=True)
    for f in os.listdir(gerb):
        os.remove(os.path.join(gerb, f))
    camadas = "F.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts"
    subprocess.run(["kicad-cli", "pcb", "export", "gerbers", "-o", gerb + "/", "--layers", camadas,
                    "--subtract-soldermask", arq], check=True, capture_output=True)
    subprocess.run(["kicad-cli", "pcb", "export", "drill", "-o", gerb + "/", "--format", "excellon",
                    "--excellon-separate-th", "--generate-map", "--map-format", "gerberx2", arq],
                   check=True, capture_output=True)
    zipf = os.path.join(SAIDA, nome + "-gerbers-jlcpcb.zip")
    with zipfile.ZipFile(zipf, "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(os.listdir(gerb)):
            z.write(os.path.join(gerb, f), f)
    for lado, camadas_svg in (("frente", "F.Cu,F.SilkS,F.Mask,Edge.Cuts,F.Fab"),
                              ("tras", "B.Cu,B.SilkS,B.Mask,Edge.Cuts")):
        args = ["kicad-cli", "pcb", "export", "svg", "-o", os.path.join(SAIDA, f"{nome}-{lado}.svg"),
                "--layers", camadas_svg, "--exclude-drawing-sheet", "--page-size-mode", "2"]
        if lado == "tras":
            args.append("--mirror")
        subprocess.run(args + [arq], check=True, capture_output=True)
    return drc, zipf


def lista_de_pecas(pl):
    linhas = ["referência;valor;footprint;lado;x;y"]
    for ref, val, fp, lado, x, y, rot in pl.pecas:
        linhas.append(f"{ref};{val};{fp};{lado};{x:.2f};{y:.2f}")
    with open(os.path.join(SAIDA, "pecas.csv"), "w", encoding="utf-8") as f:
        f.write("\n".join(linhas) + "\n")


def resumo_drc(drc):
    """(erros, avisos, sem ligação, texto dos erros): ignora os avisos de biblioteca e de serigrafia"""
    import re
    with open(drc, encoding="utf-8") as f:
        texto = f.read()
    erros, avisos = [], []
    for bloco in re.split(r"\n(?=\[)", texto):
        m = re.match(r"\[(\w+)\]", bloco)
        if not m:
            continue
        tipo = m.group(1)
        if tipo in ("lib_footprint_issues", "lib_footprint_mismatch", "silk_over_copper", "silk_overlap",
                    "text_height", "text_thickness"):
            continue
        (erros if "Severity: error" in bloco else avisos).append(bloco.strip())
    u = re.search(r"\*\* Found (\d+) unconnected pads \*\*", texto)
    return erros, avisos, (int(u.group(1)) if u else -1), texto


if __name__ == "__main__":
    os.makedirs(SAIDA, exist_ok=True)
    arq = os.path.join(SAIDA, "tijolo-frente.kicad_pcb")
    pl = montar()
    rotas, falhas = rotear_com_tentativas(pl)
    aplicar_rotas(pl, rotas)
    salvar(pl.b, arq)
    b = pcbnew.LoadBoard(arq)
    zonas_gnd(b, pl.contorno)
    salvar(b, arq)
    n_vias = sum(len(v) for t, v in rotas.get("GND", []))
    drc, zipf = exportar(arq)
    lista_de_pecas(pl)
    erros, avisos, nu, texto = resumo_drc(drc)
    print(arq)
    print("falhas no roteamento:", falhas or "nenhuma")
    print("vias de costura:", n_vias)
    print("DRC:", len(erros), "erros,", len(avisos), "avisos,", nu, "pads sem ligação")
    for e in (erros + avisos)[:30]:
        print("  ", e.replace("\n", " | ")[:300])
