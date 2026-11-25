"""
A PCB da frente do Tijolo (KiCad 7, gerada por código).

    python3 placa/placa.py          (da pasta montagem/)

Sai em saida/placa/: tijolo-pcb.kicad_pcb (abre no KiCad), o relatório do
teste de regras (DRC), os Gerbers e a furação num .zip para a JLCPCB, a lista
de peças e as imagens das duas faces.

O que vai na PCB (2 camadas, 0,8 mm):
  - atrás: as duas barras fêmea de perna longa que encaixam nos pinos da
    LILYGO (J1 e J2) e os capacitores da tela, que ficam pendurados no vão ao
    lado da LILYGO;
  - na frente: a tela (soldada pelos pinos dela, os pinos embaixo), os botões
    verde e vermelho (chaves tácteis 6×6) e, por baixo da aba do teclado, a
    fileira de 9 furos onde a aba se liga com 7 perninhas de fio.
Nenhuma solda fica em cima da pilha: o script confere. Os 4 pinos do toque da
tela, que cairiam ali, são cortados e têm furos sem cobre.
O roteamento é automático (roteador.py); o GND enche as duas faces.
"""
import json
import os
import re
import subprocess
import sys
import zipfile

import pcbnew
from shapely.geometry import Point, box
from shapely.ops import unary_union

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(AQUI))
import projeto as P  # noqa: E402
from roteador import FRENTE, TRAS, Grade, SemCaminho  # noqa: E402

SAIDA = os.path.join(os.path.dirname(AQUI), "saida", "placa")
NOME = "tijolo-pcb"
VERSAO = "v3 · 2026-11"
LIB = "/usr/share/kicad/footprints"
OX, OY = 100.0, 200.0                      # onde a placa fica na folha do KiCad
TRILHA, FOLGA, VIA, VIA_FURO = 0.3, 0.2, 0.6, 0.3
FOLGA_ZONA = 0.3

# onde a ESP32 tem a antena (embaixo da PCB, a uns 20 mm): ali o GND não enche
_ax0, _ay0, _ax1, _ay1 = P.caixa_lilygo(*P.L.antena_esp)
SEM_COBRE = box(_ax0 - 2.0, _ay0 - 2.0, _ax1 + 2.0, _ay1 + 2.0)


def mm(v):
    return pcbnew.FromMM(float(v))


def kp(x, y):
    """Tijolo (Y para cima) → KiCad (Y para baixo)"""
    return pcbnew.VECTOR2I(mm(x + OX), mm(OY - y))


def tj(v):
    """KiCad → Tijolo"""
    return pcbnew.ToMM(v.x) - OX, OY - pcbnew.ToMM(v.y)


# ---------------------------------------------------------------------------
# a placa
# ---------------------------------------------------------------------------
class Placa:
    def __init__(self):
        self.b = pcbnew.BOARD()
        self.nets = {}
        self.pads = {}          # net → [(geometria, camadas)]
        self.obstaculos = []    # (geometria, camadas) sem net
        self.pecas = []         # (referência, o que é, valor, lado, x, y) para a lista de peças
        self.furos = []         # (x, y, diâmetro, com cobre?) de todos os furos
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
        ds.m_MinResolvedSpokes = 1          # todo pad de GND também tem trilha própria
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
        corpo = box(P.PCB_X0, P.PCB_Y0, P.PCB_X1, P.PCB_Y1)
        lingua = box(P.LINGUA[0], P.LINGUA[1], P.LINGUA[2], P.LINGUA[3] + 0.5)
        forma = unary_union([corpo, lingua])
        forma = forma.buffer(-1.2).buffer(1.2, 16)          # as quinas de fora
        forma = forma.buffer(1.0, 16).buffer(-1.0)          # as de dentro (a fresa não faz canto vivo)
        s = pcbnew.PCB_SHAPE(self.b, pcbnew.SHAPE_T_POLY)
        s.SetPolyPoints([kp(x, y) for x, y in list(forma.exterior.coords)[:-1]])
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(mm(0.1))
        s.SetFilled(False)
        self.b.Add(s)
        return forma

    def texto(self, t, x, y, tam=1.0, camada=pcbnew.F_SilkS, rot=0, negrito=False, alinha=None):
        tx = pcbnew.PCB_TEXT(self.b)
        tx.SetText(t)
        tx.SetPosition(kp(x, y))
        tx.SetLayer(camada)
        tx.SetTextSize(pcbnew.VECTOR2I(mm(tam), mm(tam)))
        tx.SetTextThickness(mm(max(0.15, tam * (0.2 if negrito else 0.15))))
        if rot:
            tx.SetTextAngleDegrees(rot)
        atras = camada in (pcbnew.B_SilkS, pcbnew.B_Cu, pcbnew.B_Fab)
        if atras:
            tx.SetMirrored(True)
        if alinha == "esq":
            tx.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_RIGHT if atras else pcbnew.GR_TEXT_H_ALIGN_LEFT)
        elif alinha == "dir":
            tx.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT if atras else pcbnew.GR_TEXT_H_ALIGN_RIGHT)
        self.b.Add(tx)

    def linha(self, pts, camada=pcbnew.F_SilkS, largura=0.15):
        for a, c in zip(pts, pts[1:]):
            s = pcbnew.PCB_SHAPE(self.b, pcbnew.SHAPE_T_SEGMENT)
            s.SetStart(kp(*a))
            s.SetEnd(kp(*c))
            s.SetLayer(camada)
            s.SetWidth(mm(largura))
            self.b.Add(s)

    def retangulo(self, x0, y0, x1, y1, camada=pcbnew.F_SilkS, largura=0.15):
        self.linha([(x0, y0), (x1, y0), (x1, y1), (x0, y1), (x0, y0)], camada, largura)

    def circulo(self, x, y, d, camada=pcbnew.F_SilkS, largura=0.15):
        s = pcbnew.PCB_SHAPE(self.b, pcbnew.SHAPE_T_CIRCLE)
        s.SetCenter(kp(x, y))
        s.SetEnd(kp(x + d / 2, y))
        s.SetLayer(camada)
        s.SetWidth(mm(largura))
        self.b.Add(s)

    # ------------------------------------------------------- pads → roteador
    def _registra_pads(self, fp):
        """as geometrias (Tijolo) dos pads de um footprint, por net"""
        for pad in fp.Pads():
            x, y = tj(pad.GetPosition())
            sx, sy = pcbnew.ToMM(pad.GetSize().x), pcbnew.ToMM(pad.GetSize().y)
            rot = pad.GetOrientationDegrees()
            if round(rot) % 180 == 90:
                sx, sy = sy, sx
            forma = pad.GetShape()
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
            furado = pad.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH)
            if furado:
                self.furos.append((x, y, pcbnew.ToMM(pad.GetDrillSize().x),
                                   pad.GetAttribute() == pcbnew.PAD_ATTRIB_PTH))
            nome = pad.GetNetname()
            if nome:
                self.pads.setdefault(nome, []).append((g, camadas))
            else:
                self.obstaculos.append((g, camadas or [FRENTE, TRAS]))
            if pad.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
                self.obstaculos.append((Point(x, y).buffer(pcbnew.ToMM(pad.GetDrillSize().x) / 2), [FRENTE, TRAS]))

    def footprint_lib(self, lib, nome, ref, oque, valor, x, y, rot=0, atras=False, nets=None, lista=True):
        fp = pcbnew.FootprintLoad(os.path.join(LIB, lib + ".pretty"), nome)
        fp.SetReference(ref)
        fp.SetValue(valor)
        fp.SetPosition(kp(x, y))
        if rot:
            fp.SetOrientationDegrees(rot)
        fp.Reference().SetVisible(False)    # os nomes vão à mão, onde cabem
        fp.Value().SetVisible(False)
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
        if lista:
            self.pecas.append((ref, oque, valor, "trás" if atras else "frente", x, y))
        return fp

    def footprint_novo(self, ref, valor):
        fp = pcbnew.FOOTPRINT(self.b)
        fp.SetReference(ref)
        fp.SetValue(valor)
        fp.SetPosition(pcbnew.VECTOR2I(0, 0))
        fp.Reference().SetVisible(False)
        fp.Value().SetVisible(False)
        return fp

    def pad_tht(self, fp, nome, x, y, d=1.7, furo=1.0, quadrado=False):
        p = pcbnew.PAD(fp)
        p.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
        p.SetShape(pcbnew.PAD_SHAPE_RECT if quadrado else pcbnew.PAD_SHAPE_CIRCLE)
        p.SetSize(pcbnew.VECTOR2I(mm(d), mm(d)))
        p.SetDrillSize(pcbnew.VECTOR2I(mm(furo), mm(furo)))
        p.SetLayerSet(pcbnew.PAD.PTHMask())
        p.SetName(nome)
        v = pcbnew.VECTOR2I(mm(x), mm(-y))
        p.SetPos0(v)
        p.SetPosition(v)
        fp.Add(p)
        return p

    def furo_simples(self, fp, x, y, furo=1.1):
        """um furo sem cobre (não leva solda)"""
        p = pcbnew.PAD(fp)
        p.SetAttribute(pcbnew.PAD_ATTRIB_NPTH)
        p.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
        p.SetSize(pcbnew.VECTOR2I(mm(furo), mm(furo)))
        p.SetDrillSize(pcbnew.VECTOR2I(mm(furo), mm(furo)))
        p.SetLayerSet(pcbnew.PAD.UnplatedHoleMask())
        p.SetName("")
        v = pcbnew.VECTOR2I(mm(x), mm(-y))
        p.SetPos0(v)
        p.SetPosition(v)
        fp.Add(p)
        return p

    def coloca(self, fp, nets=None):
        """um footprint feito aqui, com os pads já nas coordenadas do Tijolo"""
        fp.SetPosition(kp(0, 0))
        self.b.Add(fp)
        for pad in fp.Pads():
            n = (nets or {}).get(pad.GetName())
            if n:
                pad.SetNet(self.net(n))
                if n == "GND":
                    pad.SetThermalSpokeAngleDegrees(45)
        self._registra_pads(fp)

    def pad_em(self, fp, nome):
        for pad in fp.Pads():
            if pad.GetName() == nome:
                return tj(pad.GetPosition())
        raise KeyError(nome)


def montar():
    pl = Placa()
    F, B = pcbnew.F_SilkS, pcbnew.B_SilkS

    # --- as barras fêmea (atrás), nos pinos da LILYGO. O pino 1 fica embaixo.
    usados = set(P.TELA_GPIO.values()) | {n for n in P.TECLADO_ABA if n} | {P.GPIO_VERDE, P.GPIO_VERMELHO}
    for barra, lista in (("J1", P.J1), ("J2", P.J2)):
        x, y = P.pino_barra(barra, 1)
        nets = {str(i + 1): nome for i, nome in enumerate(lista) if nome in usados}
        fp = pl.footprint_lib("Connector_PinSocket_2.54mm", "PinSocket_1x16_P2.54mm_Vertical", barra,
                              "barra fêmea 1×16 de perna longa (empilhável), 2,54 mm",
                              f"perna de {P.FEMEA_PERNA_MIN:.1f} mm ou mais", x, y, rot=180, atras=True, nets=nets)
        for item in list(fp.GraphicalItems()):
            if item.GetLayer() in (pcbnew.F_SilkS, pcbnew.B_SilkS):
                fp.Remove(item)
        for n in (1, 16):                                   # confere: os furos caem nos pinos da LILYGO
            px, py = pl.pad_em(fp, str(n))
            ex, ey = P.pino_barra(barra, n)
            assert abs(px - ex) < 0.01 and abs(py - ey) < 0.01, (barra, n, px, py, ex, ey)
        lado = -1 if barra == "J2" else 1                   # os nomes dos pinos, para dentro da placa
        for i, nome in enumerate(lista):
            px, py = P.pino_barra(barra, i + 1)
            pl.texto(nome, px + lado * 1.6, py, tam=0.8, camada=B, alinha="esq" if lado > 0 else "dir")
        _, ytopo = P.pino_barra(barra, 16)
        pl.texto(barra, x, ytopo + 2.4, tam=1.0, camada=B, negrito=True)
        pl.texto(barra, x + lado * 2.6, ytopo + 0.4, tam=1.0, camada=F, negrito=True)
        y0 = P.pino_barra(barra, 1)[1]
        for camada in (F, B):
            pl.retangulo(x - 1.1, y0 - 1.25, x + 1.1, ytopo + 1.25, camada, 0.12)

    # --- a tela: os pinos dela entram por cima e são soldados atrás
    fp = pl.footprint_novo("TELA", "tela ILI9341 2,4\"")
    nets = {}
    for i in range(1, 15):
        x, y = P.pino_tela(i)
        if i in P.TELA_PINOS_CORTADOS:
            # Esses pinos ficam em cima da pilha: são cortados rente ao plástico da barra antes de
            # encaixar a tela. O furo é sem cobre (não leva solda) e só esconde o toquinho que sobra.
            pl.furo_simples(fp, x, y, furo=1.1)
            continue
        pl.pad_tht(fp, str(i), x, y, d=1.7, furo=1.0, quadrado=(i == 1))
        nome = P.TELA_PINOS[i - 1]
        if nome in P.TELA_GPIO:
            nets[str(i)] = P.TELA_GPIO[nome]
    pl.coloca(fp, nets)
    pl.pecas.append(("TELA", "tela 2,4\" ILI9341 (a sua), soldada pelos pinos", "-", "frente", *P.pino_tela(1)))
    curtos = {"SDI(MOSI)": "SDI", "SDO(MISO)": "SDO", "RESET": "RST", "T_IRQ": "IRQ"}
    x1, y1 = P.pino_tela(1)
    pl.texto("1", x1 - 1.7, y1, tam=0.9, negrito=True)
    for i in range(1, 15):
        x, y = P.pino_tela(i)
        nome = curtos.get(P.TELA_PINOS[i - 1], P.TELA_PINOS[i - 1])
        if i in P.TELA_PINOS_CORTADOS:
            for camada in (F, B):                           # um X em volta do furo, nas duas faces
                for sx, sy in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
                    pl.linha([(x + sx * 0.62, y + sy * 0.62), (x + sx * 0.9, y + sy * 0.9)], camada, 0.15)
        else:
            pl.texto(nome, x, y + 1.45, tam=0.8)
    xa, _ = P.pino_tela(P.TELA_PINOS_CORTADOS[0])
    xb, _ = P.pino_tela(P.TELA_PINOS_CORTADOS[-1])
    pl.texto("CORTE ESTES 4", (xa + xb) / 2, y1 + 1.45, tam=0.8)
    pl.texto("SEM SOLDA NESTES 4", (xa + xb) / 2, y1 + 1.75, tam=0.8, camada=B)
    # o contorno da tela (só desenho)
    pl.retangulo(P.TELA_X, P.TELA_Y0, P.TELA_X + P.T.largura, P.PCB_Y1 + 3.0, pcbnew.F_Fab, 0.1)

    # --- os botões verde e vermelho (chaves tácteis 6×6): GND em cima, o pino da ESP32 embaixo
    for ref, cor, x, gpio in (("SW1", "VERDE", P.BOTOES_X[0], P.GPIO_VERDE), ("SW2", "VERMELHO", P.BOTOES_X[1],
                                                                                 P.GPIO_VERMELHO)):
        fp = pl.footprint_lib("Button_Switch_THT", "SW_PUSH_6mm", ref, f"chave táctil 6×6 mm, 4 pernas ({cor.lower()})",
                              "5 mm de altura", x - 3.25, P.BOTOES_Y + 2.25, nets={"1": "GND", "2": gpio})
        for item in list(fp.GraphicalItems()):
            if item.GetLayer() == pcbnew.F_SilkS:
                fp.Remove(item)
        pl.retangulo(x - 2.1, P.BOTOES_Y - 3.4, x + 2.1, P.BOTOES_Y + 3.4, F, 0.12)
        pl.texto(cor, x, P.BOTOES_Y, tam=0.8, rot=90)

    # --- a aba do teclado: 9 furos; os 7 do meio vão para a ESP32
    fp = pl.footprint_novo("TECLADO", "teclado 3×4 (a aba)")
    nets = {}
    for k in range(1, P.K.furos_aba + 1):
        x, y = P.furo_aba(k)
        pl.pad_tht(fp, str(k), x, y, d=1.8, furo=1.1)
        if P.TECLADO_ABA[k - 1]:
            nets[str(k)] = P.TECLADO_ABA[k - 1]
    pl.coloca(fp, nets)
    pl.pecas.append(("TECLADO", "teclado 3×4 (o seu), colado com fita de espuma; a aba ligada por 7 perninhas de fio",
                     "-", "frente", *P.furo_aba(5)))
    xa, ya = P.furo_aba(1)
    xb, _ = P.furo_aba(P.K.furos_aba)
    pl.texto("ABA DO TECLADO", P.MEIO_X, ya + 1.75, tam=0.8)
    # o contorno do teclado (só desenho) e onde vai a fita de espuma (na serigrafia)
    pl.retangulo(P.TECLADO_X0, P.TECLADO_Y0, P.TECLADO_X0 + P.K.largura, P.TECLADO_Y1, pcbnew.F_Fab, 0.1)
    for fx0, fx1 in FITAS:
        pl.retangulo(fx0, FITA_Y[0], fx1, FITA_Y[1], F, 0.12)
        pl.texto("FITA DE ESPUMA", (fx0 + fx1) / 2, (FITA_Y[0] + FITA_Y[1]) / 2, tam=0.9, rot=90)
    # a borda de cima do teclado, na serigrafia: é por ela que se acerta o teclado na hora de colar
    # (dos lados ele fica rente às bordas da PCB)
    yb = P.TECLADO_Y1 + 0.25
    for bx0, bx1 in ((2.0, 10.6), (25.0, 46.0)):          # sem encostar nos nomes J1 e J2
        pl.linha([(bx0, yb), (bx1, yb)], F, 0.2)
    pl.texto("BORDA DE CIMA DO TECLADO", 36.0, yb + 1.0, tam=0.8)

    # --- os capacitores da tela: atrás, pendurados no vão ao lado da LILYGO.
    # Cada um tem dois furos a 2,5 mm (cabe um cerâmico de disco, ou um 0805 soldado entre as ilhas).
    for ref, oque, valor, (cx, cy), lib_nome, pinos in CAPACITORES:
        fp = pl.footprint_lib("Capacitor_THT", lib_nome, ref, oque, valor, cx, cy, atras=True, nets=pinos)
        x2, y2 = pl.pad_em(fp, "2")
        passo = 2.5 if "P2.50" in lib_nome else 5.0
        assert abs(x2 - (cx - passo)) < 0.01 and abs(y2 - cy) < 0.01, (ref, x2, y2)
        curto = valor.replace(" nF", "n").replace(" pF", "p").replace(" µF", "µ")
        if passo == 5.0:
            for item in list(fp.GraphicalItems()):
                if item.GetLayer() == B:
                    fp.Remove(item)
            for camada in (F, B):
                pl.texto(curto, cx - passo / 2, cy, tam=0.8, camada=camada)
        else:                                               # o eletrolítico: o valor embaixo do círculo
            for camada in (F, B):
                pl.texto(f"{ref} {curto}", cx - passo / 2, cy - 4.1, tam=0.8, camada=camada)
            pl.texto("+", cx, cy + 1.6, tam=1.0, camada=F, negrito=True)
            pl.circulo(cx - passo / 2, cy, 6.5, F, 0.12)

    # --- textos
    pl.texto("TIJOLO", 33.0, 15.0, tam=3.0, negrito=True)
    pl.texto(f"PCB da frente · {VERSAO}", 33.0, 11.4, tam=0.9)
    pl.texto("TELA: PINOS PARA BAIXO", 33.5, P.BOTOES_Y + 1.7, tam=0.8)
    pl.texto("corte as pernas rente", 33.5, P.BOTOES_Y + 0.4, tam=0.8)
    pl.texto("ESTA FACE PARA A PILHA", 34.0, 20.0, tam=1.0, camada=B, negrito=True)
    pl.texto(f"TIJOLO · PCB da frente · {VERSAO}", 34.0, 17.6, tam=0.9, camada=B)
    pl.texto("barras fêmea deste lado; solde na outra face", 34.0, 15.6, tam=0.8, camada=B)
    return pl


# onde vai a fita dupla face de espuma, entre as fileiras de solda das barras
FITAS = [(3.0, 17.5), (23.5, 47.5)]
FITA_Y = (27.0, 66.0)
# (referência, o que é, valor, (x, y) do furo 1, footprint, {pino: net}). As peças ficam atrás, então o
# furo 2 cai à ESQUERDA do 1 (de frente): 2,5 mm no eletrolítico (o furo 1 é o +), 5 mm nos cerâmicos.
_yc = P.TELA_PINOS_Y - 2.2
CAPACITORES = [
    ("C2", "capacitor eletrolítico 100 µF, 10 V ou mais (Ø até 6,3 mm)", "100 µF", (15.85, P.BOTOES_Y - 2.1),
     "CP_Radial_D6.3mm_P2.50mm", {"1": "3V3", "2": "GND"}),
    ("C1", "capacitor cerâmico 100 nF", "100 nF", (17.3, _yc), "C_Disc_D5.0mm_W2.5mm_P5.00mm",
     {"1": "3V3", "2": "GND"}),
    ("C4", "capacitor cerâmico 100 pF", "100 pF", (24.9, _yc), "C_Disc_D5.0mm_W2.5mm_P5.00mm",
     {"1": "3V3", "2": "GND"}),
    ("C5", "capacitor cerâmico 100 nF (no RESET da tela)", "100 nF", (24.5, P.BOTOES_Y - 0.3),
     "C_Disc_D5.0mm_W2.5mm_P5.00mm", {"1": "IO9", "2": "GND"}),
]


def conferir_pilha(pl):
    """nenhum furo com solda pode ficar em cima da pilha sem sobrar espaço"""
    ruins = []
    for x, y, d, cobre in pl.furos:
        if not cobre:
            continue
        f = P.folga_sobre_a_pilha(x, y)
        if f is not None and f < P.SOLDA_ATRAS + P.FOLGA_SOLDA_PILHA:
            ruins.append(f"furo em ({x:.2f}, {y:.2f}): só {f:.2f} mm até a pilha")
    return ruins


# ---------------------------------------------------------------------------
# roteamento
# ---------------------------------------------------------------------------
ORDEM = ["IO37", "IO15", "IO17", "IO40", "IO16", "IO39", "IO38",            # o teclado (os mais compridos)
         "IO47", "IO9", "IO14", "IO36", "IO11", "IO12", "IO13",             # a tela
         "IO35", "IO21", "3V3", "GND"]


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
    for x, y, d, _ in pl.furos:
        g.sem_via(Point(x, y).buffer(d / 2 + 0.25 + VIA_FURO / 2 + 0.15, 16))
    # ao lado das barras, andar custa mais: as trilhas saem delas logo, sem emparedar os vizinhos
    for barra in ("J1", "J2"):
        bx, by0 = P.pino_barra(barra, 1)
        _, by1 = P.pino_barra(barra, 16)
        g.evita(box(bx - 2.6, by0 - 1.5, bx + 2.6, by1 + 1.5), (FRENTE, TRAS), 3.0)
    # em cima da antena da ESP32, também: as trilhas dão a volta se der
    g.evita(SEM_COBRE, (FRENTE, TRAS), 1.5)
    rotas = {}
    falhas = []
    for net in list(ordem) + [n for n in pl.pads if n not in ordem]:
        if net not in pl.pads:
            continue
        nid = ids[net]
        grupos = [[pr] for pr in pl.pads[net]]
        if len(grupos) < 2:
            continue

        def celulas(grupo):
            out = set()
            for gm, cm in grupo:
                out.update(g.celulas_de(gm, cm))
            return out

        arvore = celulas(grupos[0])
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
            dentro = pl.contorno.buffer(-1.0).contains(Point(x, y)) and not SEM_COBRE.buffer(0.8).contains(Point(x, y))
            folga_ok = dentro and all(g.dono_via[cm][jj, ii] in (0, gid) for cm in (0, 1)
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


def zonas_gnd(b, contorno):
    """o GND enchendo as duas faces (a placa precisa ter sido carregada do disco: o ZONE_FILLER
    de uma placa feita só na memória derruba o KiCad 7), menos em cima da antena da ESP32"""
    gnd = b.FindNet("GND")
    x0, y0, x1, y1 = contorno.bounds
    for camada in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(b)
        z.SetLayer(camada)
        z.SetNet(gnd)
        ol = z.Outline()
        ol.NewOutline()
        for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
            ol.Append(mm(x + OX), mm(OY - y))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        z.SetMinThickness(mm(0.25))
        z.SetLocalClearance(mm(FOLGA_ZONA))
        z.SetThermalReliefGap(mm(0.4))
        z.SetThermalReliefSpokeWidth(mm(0.4))
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        b.Add(z)
    # a área sem cobre (só o enchimento: trilhas podem passar)
    k = pcbnew.ZONE(b)
    k.SetIsRuleArea(True)
    k.SetDoNotAllowCopperPour(True)
    k.SetDoNotAllowTracks(False)
    k.SetDoNotAllowVias(False)
    k.SetDoNotAllowPads(False)
    k.SetDoNotAllowFootprints(False)
    ls = pcbnew.LSET()
    ls.AddLayer(pcbnew.F_Cu)
    ls.AddLayer(pcbnew.B_Cu)
    k.SetLayerSet(ls)
    ol = k.Outline()
    ol.NewOutline()
    ax0, ay0, ax1, ay1 = SEM_COBRE.bounds
    for x, y in ((ax0, ay0), (ax1, ay0), (ax1, ay1), (ax0, ay1)):
        ol.Append(mm(x + OX), mm(OY - y))
    b.Add(k)
    b.BuildConnectivity()
    pcbnew.ZONE_FILLER(b).Fill(b.Zones())


# ---------------------------------------------------------------------------
# saída
# ---------------------------------------------------------------------------
def salvar(b, arq):
    b.SetFileName(arq)
    pcbnew.SaveBoard(arq, b)
    # as regras que o KiCad guarda no projeto: aceita térmico de 1 raio (os pinos das barras são
    # vizinhos demais para 2) e não reclama das bibliotecas (os footprints vêm do código)
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


def exportar(arq, nome=NOME):
    drc = os.path.join(SAIDA, "drc.txt")
    codigo = ("import pcbnew, sys; b = pcbnew.LoadBoard(sys.argv[1]); "
              "pcbnew.WriteDRCReport(b, sys.argv[2], pcbnew.EDA_UNITS_MILLIMETRES, True)")
    subprocess.run([sys.executable, "-c", codigo, arq, drc], check=True, capture_output=True)
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
    for lado, camadas_svg in (("frente-letras", "F.SilkS,Edge.Cuts,F.Mask"), ("tras-letras", "B.SilkS,Edge.Cuts,B.Mask")):
        args = ["kicad-cli", "pcb", "export", "svg", "-o", os.path.join(SAIDA, f"{nome}-{lado}.svg"),
                "--layers", camadas_svg, "--exclude-drawing-sheet", "--page-size-mode", "2"]
        if lado.startswith("tras"):
            args.append("--mirror")
        subprocess.run(args + [arq], check=True, capture_output=True)
    return drc, zipf


def lista_de_pecas(pl):
    linhas = ["referência;o que é;valor;lado;x;y"]
    for ref, oque, val, lado, x, y in pl.pecas:
        linhas.append(f"{ref};{oque};{val};{lado};{x:.2f};{y:.2f}")
    with open(os.path.join(SAIDA, "pecas.csv"), "w", encoding="utf-8") as f:
        f.write("\n".join(linhas) + "\n")


def resumo_drc(drc):
    """(erros, avisos, sem ligação, texto dos erros): só ignora os avisos de biblioteca (os footprints vêm do código)"""
    with open(drc, encoding="utf-8") as f:
        texto = f.read()
    erros, avisos = [], []
    for bloco in re.split(r"\n(?=\[)", texto):
        m = re.match(r"\[(\w+)\]", bloco)
        if not m:
            continue
        tipo = m.group(1)
        if tipo in ("lib_footprint_issues", "lib_footprint_mismatch"):
            continue
        (erros if "Severity: error" in bloco else avisos).append(bloco.strip())
    u = re.search(r"\*\* Found (\d+) unconnected pads \*\*", texto)
    return erros, avisos, (int(u.group(1)) if u else -1), texto


if __name__ == "__main__":
    os.makedirs(SAIDA, exist_ok=True)
    arq = os.path.join(SAIDA, NOME + ".kicad_pcb")
    pl = montar()
    ruins = conferir_pilha(pl)
    print("soldas em cima da pilha:", ruins or "nenhuma")
    if ruins:
        sys.exit(1)
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
    x0, y0, x1, y1 = pl.contorno.bounds
    print(arq)
    print(f"tamanho: {x1 - x0:.1f} × {y1 - y0:.1f} mm, {P.PCB_ESP} mm de espessura")
    print("falhas no roteamento:", falhas or "nenhuma")
    print("vias de costura:", n_vias)
    print("DRC:", len(erros), "erros,", len(avisos), "avisos,", nu, "pads sem ligação")
    for e in (erros + avisos)[:30]:
        print("  ", e.replace("\n", " | ")[:300])
    if erros or falhas or nu:
        sys.exit(1)
