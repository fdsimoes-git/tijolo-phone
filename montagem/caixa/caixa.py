"""
A caixa do Tijolo para imprimir em 3D: o fundo, a frente e os botões verde e
vermelho. Por dentro vão a LILYGO de costas, a PCB da frente com a tela e o
teclado 3×4 inteiro (medidas em projeto.py).

    python3 caixa/caixa.py            (da pasta montagem/; precisa do CadQuery)

Sai em montagem/saida/caixa/: fundo.stl, frente.stl (a cara para baixo, do
jeito de imprimir), botao_verde.stl e botao_vermelho.stl, e os mesmos em
.step; para ver: frente_no_lugar.stl e visual/ (versões leves, para a vista 3D
do guia). No fim confere se alguma peça de dentro bate na caixa.
Para fazer só uma parte: caixa.py fundo | frente | botoes | visual | conferir.

Como fecha: a frente desce reta sobre o fundo; em cima, duas linguetas com
dente entram nas janelas da parede de cima (clic); embaixo, dois parafusos M2
entram por trás nos pilares da frente. A LILYGO fica de costas em 4 pilares
com pino nos furos dela; a PCB encaixa nos pinos da LILYGO e apoia em pilares
do fundo; a ponta de cima da tela apoia nas duas torres ao lado do
alto-falante. Pela abertura de baixo da frente passam só as teclas: a frente
cobre a aba de fora do teclado. O som sai por fendas na frente, acima da tela.
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
BORDA = 1.0                        # chanfro das quinas da frente e de trás (as duas imprimem viradas para a mesa)
ZD = P.Z_PCB_FRENTE                # onde a frente encontra o fundo: a face da frente da PCB

# Parafusos de baixo: de cabeça chata, entram por trás (pelo fundo) nos pilares da frente. O furo de 1,75 mm
# serve dois parafusos: é 0,8 × o diâmetro do autoatarraxante de 2,2 × 9,5 (DIN 7982, o que se acha no Brasil)
# e fica perto do que o guia de furos do MakerWorld indica para o M2 de rosca de máquina abrir a rosca no
# plástico (1,70 a 1,74 em PETG; com 1,6 ele entrava mais apertado). O pilar não engrossa: fica a 0,4 mm da
# parede de baixo do fundo e a 0,8 mm do canto da PCB.
PARAF_X = (4.5, P.CAV_X1 - 4.5)
PARAF_Y = 2.7
PILAR_D = 4.6
PILAR_FURO = 1.75
PILAR_FUNDURA = 9.0                # o furo vai até 10,7 mm da face de trás: cabem o 2,2 × 9,5 e o M2 × 10
PILAR_BOCA = (2.4, 0.45)           # a entrada do furo, em cone: o parafuso entra reto e a rebarba não afasta as metades
PILAR_Z0 = P.FUNDO + 0.2
assert PARAF_Y - PILAR_D / 2 - P.CAV_Y0 >= 0.39 and P.PCB_Y0 - (PARAF_Y + PILAR_D / 2) >= 0.45, "pilar do parafuso"

# linguetas de cima (frente): descem por dentro da parede de cima, entre ela e a
# ponta da tela, e o dente entra num rasgo da parede. Ficam na frente das pontas
# do alto-falante. O dente tem rampa embaixo (entra apertando), um trecho reto e
# a face de cima a 60° (segura firme, mas sai puxando).
LING = dict(xs=(6.5, P.CAV_X1 - 6.5), larg=8.0, esp=1.2, folga=0.15, dente=0.5, comp=11.0)
LING["z0"] = ZD - LING["comp"]
DENTE = dict(rampa=0.6, reto=0.15, volta=0.29)
# O teto do rasgo fica um pouco abaixo do fim do dente: fechada, a lingueta
# continua um tiquinho dobrada e puxa a frente contra o fundo (sem jogo).
RASGO = dict(fundo=0.8, z0=LING["z0"] - 0.3, z1=LING["z0"] + DENTE["rampa"] + DENTE["reto"] + 0.15)

# Os 4 pilares da LILYGO, na ordem de P.L.furos_fixacao. Dois, em diagonal, têm
# pino (acertam a posição); os outros dois têm furo para parafuso M2 × 5.
PILARES_COM_PINO = (0, 3)
PINO_D = 2.3                       # o furo da LILYGO tem 2,55

# os apoios da PCB no fundo: três pilares soltos no vão da esquerda (a antena
# passa entre eles e a parede), duas nervuras na parede de baixo e uma na da direita
POSTES = [(3.5, 15.0), (3.5, 47.0), (3.5, P.TELA_PINOS_Y + 0.4)]
POSTE_D = 3.4
NERVURA_POSTE = (4.5, 4.0)         # a nervura de cada pilar: comprimento e quanto fica abaixo da PCB
NERVURAS_BAIXO = (12.0, P.CAV_X1 - 12.0)
NERVURA_DIREITA = (46.5, 13.3, 15.5)        # x0, y0, y1 (vai até a parede)
FOLGA_VIDRO_LADO = 0.6             # em volta do vidro da tela, no rebaixo da frente
CANAL = 0.9                        # fundura dos canais da parede da direita, por onde descem os USB-C e os QWIIC

# o alto-falante: a calha e as duas torres que seguram a ponta de cima da tela
CALHA_Y = (P.FALANTE_Y, P.FALANTE_Y + P.FALANTE[2] + 2 * P.FALANTE_FOLGA)
CALHA_X = (P.FALANTE_X0 - P.FALANTE_FOLGA, P.FALANTE_X0 + P.FALANTE[0] + P.FALANTE_FOLGA)
TORRE_Y = (CALHA_Y[0] - 0.05, CALHA_Y[1] - 0.2)
# O som sai pela frente do Tijolo, por fendas logo acima da ponta da tela (onde vai o ouvido). Uma parede
# atrás do alto-falante fecha o canto de cima da caixa, para o som não se perder lá dentro.
PAREDE_SOM = 1.0
PASSA_CABO = (11.0, 14.5, 2.5)     # o rasgo do cabo do alto-falante nessa parede: x0, x1 e a fundura
SOM_X = (LING["xs"][0] + LING["larg"] / 2 + 0.7, LING["xs"][1] - LING["larg"] / 2 - 0.7)   # entre as duas linguetas
SOM_FENDAS, SOM_PONTE = 3, 1.2
# O USB-C de carregar e gravar: por fora, um rebaixo onde entra a capa do cabo; no fundo dele, uma parede
# fina com um furo só para a ponta de metal. A boca do conector fica a 0,2 mm dessa parede.
USB = dict(canal=0.7, mascara=0.4, capa=P.USB_CAPA, ponta=P.USB_FURO)

# a antena do 4G (uma fita de 110,5 × 20 mm) cola na parede da esquerda, por dentro
ANTENA_Y = (34.0, 145.5)

# os botões verde e vermelho: chave táctil 6×6 e um botão impresso
CHAVE_ALTURA = P.CHAVE_TACTIL_ALTURA    # da face da PCB até o topo do botãozinho
CHAVE_CORPO = 3.5
BOTAO = (10.0, 5.0)                # o botão impresso, de frente
FOLGA_BOTAO = 0.3
ABA_BOTAO = 0.5
SAI_BOTAO = 1.0
RECESSO_PINO = (4.6, P.BOTAO_RECESSO)   # folgado: o botãozinho (Ø3,5) pode ficar até 0,5 mm fora do centro
Z_BOTAO0 = P.Z_PCB_FRENTE + CHAVE_ALTURA - RECESSO_PINO[1]
ABA_ALT = P.BOTAO_ABA              # a aba do botão acompanha a altura da chave: solto, ele sobe 0,3 mm até a frente
Z_POCO0 = P.Z_TELA_FRENTE + 0.3    # o poço do botão começa acima da plaquinha da tela
assert 1.2 <= ABA_ALT <= 2.2 and abs(P.Z_FRENTE_DENTRO - (Z_BOTAO0 + ABA_ALT) - P.BOTAO_JOGO) < 0.006, ABA_ALT

# a costela da frente que segura a PCB no lugar (entre os dois botões)
COSTELA = (28.0, P.BOTOES_Y - 2.2, 38.0, P.BOTOES_Y + 1.8)
# o degrau da borda: a frente tem uma aba fina que entra num rebaixo da borda do fundo, na volta toda
DEGRAU = dict(larg=0.9, alt=1.0, folga=0.15)


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


def botoes_posicoes():
    return [("verde", P.BOTOES_X[0], P.BOTOES_Y), ("vermelho", P.BOTOES_X[1], P.BOTOES_Y)]


def texto_atras(t, x, y, tam, fundo=0.45):
    """um texto gravado na face de trás (espelhado, para ser lido por trás)"""
    return (cq.Workplane("XY").workplane(offset=-0.01).center(x, y)
            .text(t, tam, fundo, fontPath=FONTE, halign="center", valign="center", combine=False)
            .mirror("YZ", basePointVector=(x, 0, 0)))


# ---------------------------------------------------------------------------
# o fundo
# ---------------------------------------------------------------------------
def fundo():
    f = caixa_arredondada(X0, Y0, X1, Y1, 0, ZD, R_FORA)
    f = f.faces("<Z").edges().chamfer(BORDA)
    f = f.cut(caixa_arredondada(P.CAV_X0, P.CAV_Y0, P.CAV_X1, P.CAV_Y1, P.FUNDO, ZD + 1, R_DENTRO))

    # pilares da LILYGO (de costas: os componentes para o fundo): dois com pino no furo dela,
    # dois com furo para parafuso M2 × 5 (o furo para 0,8 mm antes da face de fora)
    for i, (fx, fy) in enumerate(P.L.furos_fixacao):
        x, y = P.lilygo_para_tijolo(fx, fy)
        f = f.union(cilindro_z(x, y, P.FUNDO - 0.1, P.Z_LILYGO_COMP, 5.2))
        if i in PILARES_COM_PINO:
            f = f.union(cilindro_z(x, y, P.Z_LILYGO_COMP - 0.1, P.Z_LILYGO_COMP + 1.3, PINO_D)
                        .faces(">Z").edges().chamfer(0.3))
        else:
            f = f.cut(cilindro_z(x, y, 0.8, P.Z_LILYGO_COMP + 0.1, 1.7))

    # a chave liga/desliga da LILYGO: um bolso para o corpo e uma fenda para o botãozinho
    cy0, cy1, by0, by1, sai, bz0, bz1 = P.L.chave
    ax0, ay0, ax1, ay1 = P.caixa_lilygo(P.L.chave_corpo_x[0], cy0, P.L.chave_corpo_x[1], cy1)
    prof = P.Z_LILYGO_COMP - P.L.chave_altura - 0.3
    f = f.cut(bloco(ax0 - 0.3, ay0 - 0.3, prof, ax1 + 0.3, ay1 + 0.3, P.FUNDO + 0.5))
    # O botãozinho corre uns 2 mm: ligado fica para o lado da ESP32 (para baixo, no Tijolo), que é
    # onde ele está no modelo 3D; desligado, para o lado do modem (para cima). A fenda cobre os dois.
    kx0, ky0, kx1, ky1 = P.caixa_lilygo(-sai, by0, 0.0, by1)
    fenda_y0, fenda_y1 = ky0 - 1.6, ky1 + 3.0
    f = f.cut(bloco(kx0 - 0.2, fenda_y0, -0.5, kx1 + 0.2, fenda_y1, P.FUNDO + 0.5))

    # USB-C: os dois saem pela borda da direita (de frente), perto do fundo. O da ESP32 (carregar e
    # gravar) tem abertura para a capa do cabo, aberta para trás; o do modem só tem o canal por dentro.
    usb_z = P.Z_LILYGO_COMP - P.L.usb_altura / 2
    for yc, abre in ((P.L.usb_esp_y, True), (P.L.usb_modem_y, False)):
        _, y = P.lilygo_para_tijolo(0, yc)
        if not abre:
            f = f.cut(bloco(P.CAV_X1 - 0.1, y - 5.0, usb_z - 2.0, P.CAV_X1 + CANAL, y + 5.0, ZD + 1))
            continue
        # o canal por onde o conector desce: mais raso na altura dele (a parede fina fica logo à frente da boca),
        # com uma rampa na entrada
        z_raso = usb_z + USB["capa"][1] / 2 + 0.5
        f = f.cut(bloco(P.CAV_X1 - 0.1, y - 5.0, usb_z - 2.0, P.CAV_X1 + USB["canal"], y + 5.0, z_raso))
        f = f.cut(bloco(P.CAV_X1 - 0.1, y - 5.0, z_raso - 0.01, P.CAV_X1 + CANAL, y + 5.0, ZD + 1))
        dx = CANAL - USB["canal"]
        rampa = (cq.Workplane("XZ").workplane(offset=-(y - 5.0))
                 .polyline([(P.CAV_X1 + USB["canal"] - 0.01, z_raso - dx), (P.CAV_X1 + CANAL, z_raso + 0.01),
                            (P.CAV_X1 + USB["canal"] - 0.01, z_raso + 0.01)]).close().extrude(-10.0))
        f = f.cut(rampa)
        # por fora, o rebaixo da capa do cabo (aberto para trás: a capa chega quase até a face de trás)
        x_masc = P.CAV_X1 + USB["canal"] + USB["mascara"]
        rebaixo = (cq.Workplane("YZ").workplane(offset=x_masc).center(y, usb_z)
                   .rect(*USB["capa"]).extrude(X1 - x_masc + 1.0).edges("|X").fillet(2.6))
        f = f.cut(rebaixo)
        # e o furo da ponta de metal do cabo
        furo = (cq.Workplane("YZ").workplane(offset=P.CAV_X1 - 0.5).center(y, usb_z)
                .rect(*USB["ponta"]).extrude(P.PAREDE + 1.0).edges("|X").fillet(USB["ponta"][1] / 2 - 0.01))
        f = f.cut(furo)
    # os QWIIC (do lado da pilha) também passam da borda: canal
    _, q0, _, q1 = P.caixa_lilygo(0, P.L.qwiic[0][0], 1, P.L.qwiic[1][1])
    f = f.cut(bloco(P.CAV_X1 - 0.1, q0 - 0.3, P.Z_LILYGO_PILHA - 0.3, P.CAV_X1 + CANAL, q1 + 0.3, ZD + 1))
    # um rebaixo raso na parede da direita, de fora a fora: dá folga para a borda da LILYGO, para a
    # barra J2 e para a borda da PCB, que ficam todas encostadas nessa parede
    _, _, _, ly1 = P.caixa_lilygo(0, 0, P.L.largura, P.L.altura)
    f = f.cut(bloco(P.CAV_X1 - 0.1, 4.0, P.FUNDO + 2.0, P.CAV_X1 + P.REBAIXO_DIREITA, ly1 + 1.5, ZD + 1))

    # o rebaixo da antena, na parede da esquerda por dentro
    f = f.cut(bloco(P.CAV_X0 - 0.3, ANTENA_Y[0], P.FUNDO + 0.4, P.CAV_X0 + 0.1, ANTENA_Y[1], P.FUNDO + 0.4 + 20.6))

    # o alto-falante em pé, virado para a parede de cima: uma calha em cada ponta. Do lado de fora de
    # cada calha sobe uma torre, encostada na parede, que apoia a ponta de cima da plaquinha da tela.
    cy_0, cy_1 = CALHA_Y
    for lado, xe in ((-1, CALHA_X[0]), (1, CALHA_X[1])):
        xa, xb = (xe - 4.3, xe + 1.7) if lado < 0 else (xe - 1.7, xe + 4.3)
        b = bloco(xa, cy_0 - 1.0, P.FUNDO - 0.1, xb, cy_1 + 0.8, P.FUNDO + 12.0)
        sa, sb = (xe, xe + 1.8) if lado < 0 else (xe - 1.8, xe)
        b = b.cut(bloco(sa, cy_0, P.FUNDO, sb, cy_1, P.FUNDO + 13.0))
        f = f.union(b)
        if lado < 0:
            f = f.union(bloco(P.CAV_X0 - 0.1, TORRE_Y[0], P.FUNDO - 0.1, xe - 0.2, TORRE_Y[1], ZD))
            f = f.union(bloco(P.CAV_X0 + 0.4, TORRE_Y[0], ZD - 0.1, xe - 0.2, TORRE_Y[1], P.Z_TELA - 0.05))
        else:
            f = f.union(bloco(xe + 0.2, TORRE_Y[0], P.FUNDO - 0.1, P.CAV_X1 + 0.1, TORRE_Y[1], ZD))
            f = f.union(bloco(xe + 0.2, TORRE_Y[0], ZD - 0.1, P.CAV_X1 - 0.4, TORRE_Y[1], P.Z_TELA - 0.05))
    # A parede de cima não tem mais grade: o som sai pelas fendas da frente (ver frente()). A parede atrás
    # do alto-falante vai de lado a lado e do fundo até a plaquinha da tela, que apoia nela; dos lados da
    # tela ela sobe até perto da frente. Assim o som só tem as fendas por onde sair.
    ya, yb = CALHA_Y[0] - PAREDE_SOM, CALHA_Y[0]
    f = f.union(bloco(P.CAV_X0 - 0.1, ya, P.FUNDO - 0.1, P.CAV_X1 + 0.1, yb, ZD))
    f = f.union(bloco(P.CAV_X0 + 0.4, ya, ZD - 0.1, P.CAV_X1 - 0.4, yb, P.Z_TELA - 0.05))
    for xa, xb in ((P.CAV_X0 + 0.4, P.TELA_X - 0.5), (P.TELA_X + P.T.largura + 0.5, P.CAV_X1 - 0.4)):
        f = f.union(bloco(xa, ya, P.Z_TELA - 0.1, xb, yb, P.Z_FRENTE_DENTRO - 0.3))
    # o rasgo por onde passa o cabo do alto-falante (a plaquinha da tela fecha por cima)
    f = f.cut(bloco(PASSA_CABO[0], ya - 0.1, P.Z_TELA - 0.05 - PASSA_CABO[2], PASSA_CABO[1], yb + 0.1, P.Z_TELA + 0.1))

    # berço do microfone (deitado, virado para a parede de baixo) e os furos do som
    mx, mz, md = P.MIC_X, P.MIC_Z, P.MIC_D
    b = bloco(mx - md / 2 - 1.6, P.CAV_Y0 - 0.1, P.FUNDO - 0.1, mx + md / 2 + 1.6, P.MIC_L - 0.6, mz + 1.0)
    b = b.cut(cilindro_y(mx, mz, P.CAV_Y0 - 0.2, P.MIC_L + 0.2, md + 0.3))
    f = f.union(b)
    for dx, dz in ((-1.7, -1.0), (1.7, -1.0), (0.0, 1.9)):
        f = f.cut(cilindro_y(mx + dx, mz + dz, Y0 - 0.5, P.CAV_Y0 + 0.5, 1.2))

    # os apoios da PCB
    z_apoio = P.Z_PCB - 0.05
    for x, y in POSTES:
        f = f.union(cilindro_z(x, y, P.FUNDO - 0.1, z_apoio, POSTE_D))
        f = f.union(bloco(x, y - 0.7, P.FUNDO - 0.1, x + NERVURA_POSTE[0], y + 0.7,
                          P.Z_PCB - NERVURA_POSTE[1]))                                       # nervura, para não quebrar
    for xc in NERVURAS_BAIXO:
        f = f.union(bloco(xc - 1.0, P.CAV_Y0 - 0.1, P.FUNDO - 0.1, xc + 1.0, 3.8, z_apoio))
    nx0, ny0, ny1 = NERVURA_DIREITA
    f = f.union(bloco(nx0, ny0, P.FUNDO - 0.1, P.CAV_X1 + P.REBAIXO_DIREITA + 0.1, ny1, z_apoio))

    # janelas das linguetas na parede de cima
    for x in LING["xs"]:
        f = f.cut(bloco(x - LING["larg"] / 2 - 0.5, P.CAV_Y1 - 0.1, RASGO["z0"], x + LING["larg"] / 2 + 0.5,
                        P.CAV_Y1 + RASGO["fundo"], RASGO["z1"]))

    # parafusos de baixo: furo no fundo, com escareado por fora (cabeça chata)
    for x in PARAF_X:
        f = f.cut(cilindro_z(x, PARAF_Y, -0.5, P.FUNDO + 0.5, 2.4))
        cone = (cq.Workplane("XY").workplane(offset=-0.01).center(x, PARAF_Y)
                .circle(2.2).workplane(offset=1.2).circle(1.2).loft())
        f = f.cut(cone)

    # o rebaixo da borda, onde entra a aba da frente
    dg = DEGRAU["larg"]
    anel = caixa_arredondada(P.CAV_X0 - dg, P.CAV_Y0 - dg, P.CAV_X1 + dg, P.CAV_Y1 + dg, ZD - DEGRAU["alt"], ZD + 1,
                             R_DENTRO + dg)
    anel = anel.cut(caixa_arredondada(P.CAV_X0, P.CAV_Y0, P.CAV_X1, P.CAV_Y1, ZD - 2, ZD + 2, R_DENTRO))
    f = f.cut(anel)                                    # só a parede: os pilares e as torres ficam inteiros

    # o nome atrás e as posições da chave, ao lado da fenda (espelhados: lidos por trás)
    meio = (X0 + X1) / 2
    f = f.cut(texto_atras("TIJOLO", meio, 44.0, 7.0))
    f = f.cut(texto_atras("ON", kx0 - 6.0, fenda_y0 + 0.9, 3.0, 0.35))
    f = f.cut(texto_atras("OFF", kx0 - 6.6, fenda_y1 - 0.9, 3.0, 0.35))
    return f


# ---------------------------------------------------------------------------
# a frente
# ---------------------------------------------------------------------------
def frente():
    f = caixa_arredondada(X0, Y0, X1, Y1, ZD, P.Z_TOPO, R_FORA)
    f = f.faces(">Z").edges().chamfer(BORDA)
    f = f.cut(caixa_arredondada(P.CAV_X0, P.CAV_Y0, P.CAV_X1, P.CAV_Y1, ZD - 1, P.Z_FRENTE_DENTRO, R_DENTRO))

    # a abertura do teclado: só a parte alta, com as teclas, passa por ela (a frente cobre a aba de fora
    # do teclado); a boca tem um chanfro por fora
    jx0, jy0, jx1, jy1 = P.JANELA_TECLADO
    f = f.cut(caixa_arredondada(jx0, jy0, jx1, jy1, P.Z_FRENTE_DENTRO - 1, P.Z_TOPO + 1, P.JANELA_RAIO))
    ch = 0.4
    boca = (caixa_arredondada(jx0 - ch, jy0 - ch, jx1 + ch, jy1 + ch, P.Z_TOPO - ch, P.Z_TOPO + 1, P.JANELA_RAIO + ch)
            .faces("<Z").edges().chamfer(ch - 0.001))
    f = f.cut(boca)
    # por dentro, um chanfro maior: a parte alta do teclado alarga para baixo, e assim a frente não encosta nela
    al = P.JANELA_ALIVIO
    alivio = (caixa_arredondada(jx0 - al, jy0 - al, jx1 + al, jy1 + al, P.Z_FRENTE_DENTRO - 1, P.Z_FRENTE_DENTRO + al,
                                P.JANELA_RAIO + al).faces(">Z").edges().chamfer(al - 0.001))
    f = f.cut(alivio)

    # a saída do som: fendas na frente, logo acima da ponta da tela, por cima do vão entre a tela e a parede
    # de cima (é por esse vão que o som sobe)
    sy0, sy1 = P.TELA_Y1 + 0.25, P.CAV_Y1 - 0.25
    comp = (SOM_X[1] - SOM_X[0] - (SOM_FENDAS - 1) * SOM_PONTE) / SOM_FENDAS
    for i in range(SOM_FENDAS):
        sx0 = SOM_X[0] + i * (comp + SOM_PONTE)
        f = f.cut(caixa_arredondada(sx0, sy0, sx0 + comp, sy1, P.Z_FRENTE_DENTRO - 1, P.Z_TOPO + 1, (sy1 - sy0) / 2 - 0.01))

    # a tela: o vidro entra num rebaixo por dentro e a aba da frente cobre a borda dele;
    # a janela mostra a imagem e um pouco de margem, com chanfro por fora
    m = 0.8
    wx0, wx1 = P.IMAGEM_X[0] - m, P.IMAGEM_X[1] + m
    wy0, wy1 = P.IMAGEM_Y[0] - m, P.IMAGEM_Y[1] + m
    fv = FOLGA_VIDRO_LADO
    f = f.cut(bloco(P.TELA_X - fv, P.VIDRO_Y[0] - fv, P.Z_FRENTE_DENTRO - 0.5,
                    P.TELA_X + P.T.largura + fv, P.VIDRO_Y[1] + fv, P.Z_VIDRO + P.FOLGA_VIDRO))
    f = f.cut(caixa_arredondada(wx0, wy0, wx1, wy1, P.Z_FRENTE_DENTRO - 0.5, P.Z_TOPO + 1, 1.0))
    chanfro = (cq.Workplane("XY").workplane(offset=P.Z_TOPO - 0.4)
               .center((wx0 + wx1) / 2, (wy0 + wy1) / 2).rect(wx1 - wx0, wy1 - wy0)
               .workplane(offset=0.41).rect(wx1 - wx0 + 0.8, wy1 - wy0 + 0.8).loft())
    f = f.cut(chanfro)
    # quatro apoios que seguram a plaquinha da tela pelos cantos (em cima dos furos dela)
    for u, v in P.T.furos:
        x, y = P.tela_para_tijolo(u, v)
        f = f.union(cilindro_z(x, y, P.Z_TELA_FRENTE + 0.15, P.Z_FRENTE_DENTRO + 0.1, 4.6))

    # o nome, gravado na faixa entre os botões e a tela
    nome = (cq.Workplane("XY").workplane(offset=P.Z_TOPO - 0.4).center(P.MEIO_X, (P.BOTOES_Y + 4.6 + P.VIDRO_Y[0]) / 2)
            .text("TIJOLO", 5.0, 0.5, fontPath=FONTE, halign="center", valign="center", combine=False))
    f = f.cut(nome)

    # os botões verde e vermelho: poço que guia o botão e furo na frente
    lp = BOTAO[0] + 2 * ABA_BOTAO + 2 * 0.2
    ap = BOTAO[1] + 2 * ABA_BOTAO + 2 * 0.2
    for _, x, y in botoes_posicoes():
        bx0 = P.CAV_X0 - 0.1 if x < P.MEIO_X else x - lp / 2 - 1.0
        bx1 = x + lp / 2 + 1.0 if x < P.MEIO_X else P.CAV_X1 + 0.1
        f = f.union(bloco(bx0, y - ap / 2 - 1.0, Z_POCO0, bx1, y + ap / 2 + 0.7, P.Z_FRENTE_DENTRO + 0.1))
    for _, x, y in botoes_posicoes():
        f = f.cut(caixa_arredondada(x - lp / 2, y - ap / 2, x + lp / 2, y + ap / 2, Z_POCO0 - 0.1, P.Z_FRENTE_DENTRO, 1.0))
        f = f.cut(caixa_arredondada(x - BOTAO[0] / 2 - FOLGA_BOTAO, y - BOTAO[1] / 2 - FOLGA_BOTAO,
                                    x + BOTAO[0] / 2 + FOLGA_BOTAO, y + BOTAO[1] / 2 + FOLGA_BOTAO,
                                    P.Z_FRENTE_DENTRO - 0.2, P.Z_TOPO + 1, 1.0))

    # a costela que segura a PCB, entre os botões
    cx0, cy0, cx1, cy1 = COSTELA
    f = f.union(bloco(cx0, cy0, P.Z_PCB_FRENTE + 0.15, cx1, cy1, P.Z_FRENTE_DENTRO + 0.1))

    # pilares dos parafusos de baixo (descem até perto do fundo; o parafuso entra por trás)
    for x in PARAF_X:
        pilar = cilindro_z(x, PARAF_Y, PILAR_Z0, P.Z_FRENTE_DENTRO + 0.1, PILAR_D)
        # a raiz do pilar, colada na parede de baixo da frente (da aba da borda para cima)
        pilar = pilar.union(bloco(x - PILAR_D / 2, P.CAV_Y0 - 0.1, ZD - DEGRAU["alt"] + 0.15, x + PILAR_D / 2, PARAF_Y,
                                  P.Z_FRENTE_DENTRO + 0.1))
        boca = (cq.Workplane("XY").workplane(offset=PILAR_Z0 - 0.01).center(x, PARAF_Y)
                .circle(PILAR_BOCA[0] / 2).workplane(offset=PILAR_BOCA[1]).circle(PILAR_FURO / 2).loft())
        f = f.union(pilar.cut(cilindro_z(x, PARAF_Y, PILAR_Z0 - 0.1, PILAR_Z0 + PILAR_FUNDURA, PILAR_FURO)).cut(boca))

    # linguetas de cima. A raiz é uma nervura colada na parede de cima da frente (da face de dentro
    # até a aba da borda); dali para baixo a lingueta fica solta, a 0,15 mm da parede do fundo, e dobra.
    ly1 = P.CAV_Y1 - LING["folga"]
    z_raiz = ZD - DEGRAU["alt"] + 0.15
    for x in LING["xs"]:
        x0, x1 = x - LING["larg"] / 2, x + LING["larg"] / 2
        f = f.union(bloco(x0, ly1 - LING["esp"], z_raiz, x1, P.CAV_Y1 + 0.1, P.Z_FRENTE_DENTRO + 0.1))
        f = f.union(bloco(x0, ly1 - LING["esp"], LING["z0"], x1, ly1, z_raiz + 0.1))
        z0, d = LING["z0"], LING["dente"]
        za = z0 + DENTE["rampa"]
        zb = za + DENTE["reto"]
        perfil = (cq.Workplane("YZ").workplane(offset=x0)
                  .polyline([(ly1 - 0.05, z0), (ly1 + d, za), (ly1 + d, zb), (ly1 - 0.05, zb + DENTE["volta"])]).close()
                  .extrude(x1 - x0))
        f = f.union(perfil)

    # a aba da borda: entra no rebaixo do fundo e alinha as duas metades na volta toda
    da = DEGRAU["larg"] - DEGRAU["folga"]
    aba = caixa_arredondada(P.CAV_X0 - da, P.CAV_Y0 - da, P.CAV_X1 + da, P.CAV_Y1 + da, ZD - DEGRAU["alt"] + 0.15,
                            ZD + 0.1, R_DENTRO + da)
    aba = aba.cut(caixa_arredondada(P.CAV_X0, P.CAV_Y0, P.CAV_X1, P.CAV_Y1, ZD - 2, ZD + 1, R_DENTRO))
    f = f.union(aba)
    return f


# ---------------------------------------------------------------------------
# os botões verde e vermelho
# ---------------------------------------------------------------------------
def botao(cor: str):
    """O botão em pé, com a base em z = 0 (o recesso do botãozinho embaixo, o símbolo gravado em cima)"""
    lg, al = BOTAO
    altura = P.Z_TOPO + SAI_BOTAO - Z_BOTAO0
    aba = (cq.Workplane("XY").rect(lg + 2 * ABA_BOTAO, al + 2 * ABA_BOTAO).extrude(ABA_ALT)
           .edges("|Z").fillet(1.2))
    corpo = (cq.Workplane("XY").workplane(offset=ABA_ALT - 0.01).rect(lg, al).extrude(altura - ABA_ALT + 0.01)
             .edges("|Z").fillet(1.0).faces(">Z").edges().fillet(0.5))
    t = aba.union(corpo)
    t = t.cut(cq.Workplane("XY").workplane(offset=-0.01).circle(RECESSO_PINO[0] / 2).extrude(RECESSO_PINO[1] + 0.01))
    fundo_ = 0.5
    simbolo = "✓" if cor == "verde" else "✕"
    txt = (cq.Workplane("XY").workplane(offset=altura - fundo_)
           .text(simbolo, 4.0, fundo_ + 0.2, fontPath=FONTE, halign="center", valign="center", combine=False))
    return t.cut(txt)


def botoes_no_lugar():
    pecas = []
    for cor, x, y in botoes_posicoes():
        pecas.append((cor, botao(cor).translate((x, y, Z_BOTAO0))))
    return pecas


def salvar(peca, nome, pasta):
    caminho = os.path.join(pasta, nome)
    cq.exporters.export(peca, caminho, tolerance=0.02, angularTolerance=0.15)
    return caminho


# ---------------------------------------------------------------------------
# o miolo (para conferir e para ver): as placas e as peças, em blocos
# ---------------------------------------------------------------------------
def pcb_solido():
    corpo = caixa_arredondada(P.PCB_X0, P.PCB_Y0, P.PCB_X1, P.PCB_Y1, P.Z_PCB, P.Z_PCB_FRENTE, 1.2)
    lx0, ly0, lx1, ly1 = P.LINGUA
    return corpo.union(caixa_arredondada(lx0, ly0, lx1, ly1 + 1.5, P.Z_PCB, P.Z_PCB_FRENTE, 1.0))


def miolo():
    """Cada peça de dentro como um sólido simples: [(nome, sólido)]"""
    v = []
    zc, zp = P.Z_LILYGO_COMP, P.Z_LILYGO_PILHA
    lx0, ly0, lx1, ly1 = P.caixa_lilygo(0, 0, P.L.largura, P.L.altura)
    pcb = caixa_arredondada(lx0, ly0, lx1, ly1, zc, zp, P.L.raio)
    for (fx, fy) in P.L.furos_fixacao:
        x, y = P.lilygo_para_tijolo(fx, fy)
        pcb = pcb.cut(cilindro_z(x, y, zc - 1, zp + 1, P.L.furo_fixacao_d))
    v.append(("placa da LILYGO", pcb))
    # os componentes, virados para o fundo
    for p in P.PECAS_COMP:
        if p["altura"] < 1.0:
            continue
        x0, y0, x1, y1 = P.caixa_lilygo(p["x0"], p["y0"], p["x1"], p["y1"])
        v.append((p["nome"] + " (componentes)", bloco(x0, y0, zc - p["altura"], x1, y1, zc)))
    # o lado da pilha, virado para a frente
    for p in P.PECAS_PILHA:
        if p["fundura"] < 1.0 or "BATTERY" in p["nome"]:
            continue
        x0, y0, x1, y1 = P.caixa_lilygo(p["x0"], p["y0"], p["x1"], p["y1"])
        v.append((p["nome"] + " (lado da pilha)", bloco(x0, y0, zp, x1, y1, zp + p["fundura"])))
    hx0, hy0, hx1, hy1 = P.caixa_lilygo(*P.L.suporte_18650)
    v.append(("ponta do suporte da pilha", bloco(hx0, hy0, zp, hx1, hy0 + 2.0, zp + P.L.suporte_altura)))
    v.append(("ponta do suporte da pilha 2", bloco(hx0, hy1 - 2.0, zp, hx1, hy1, zp + P.L.suporte_altura)))
    v.append(("parede do suporte da pilha", bloco(hx0, hy0, zp, hx0 + 1.5, hy1, zp + P.L.suporte_altura)))
    v.append(("parede do suporte da pilha 2", bloco(hx1 - 1.5, hy0, zp, hx1, hy1, zp + P.L.suporte_altura)))
    v.append(("pilha 18650", cilindro_y(P.PILHA_X, P.Z_PILHA_EIXO, P.PILHA_Y[0], P.PILHA_Y[1], 2 * P.L.pilha_raio)))
    # as barras: o plástico da LILYGO, a fêmea encaixada e as pernas até a PCB
    for barra in ("J1", "J2"):
        x, ya = P.pino_barra(barra, 1)
        _, yb = P.pino_barra(barra, 16)
        v.append((f"barra {barra}", bloco(x - 1.27, ya - 1.27, zp, x + 1.27, yb + 1.27, P.Z_FEMEA1)))
        v.append((f"pernas {barra}", bloco(x - 0.4, ya - 0.4, P.Z_FEMEA1, x + 0.4, yb + 0.4, P.Z_FEMEA1 + P.FEMEA_PERNA)))
        v.append((f"barra macho {barra}", bloco(x - 0.4, ya - 0.4, zp, x + 0.4, yb + 0.4, zp + P.L.barra_plastico + P.L.barra_pino)))
    # a PCB
    v.append(("PCB", pcb_solido()))
    # atrás da PCB: o eletrolítico (em pé, no vão da esquerda) e os cerâmicos
    v.append(("capacitor eletrolítico", cilindro_z(14.6, P.BOTOES_Y - 2.1, P.Z_PCB - 11.5, P.Z_PCB, 6.5)))
    yc = P.TELA_PINOS_Y - 2.2
    for nome, cx, cy in (("C1", 14.8, yc), ("C4", 22.4, yc), ("C5", 22.0, P.BOTOES_Y - 0.3)):
        v.append((f"capacitor {nome}", bloco(cx - 2.9, cy - 1.3, P.Z_PCB - 6.5, cx + 2.9, cy + 1.3, P.Z_PCB)))
    # os botões (chaves 6×6) e as pernas deles atrás da PCB
    for cor, x, y in botoes_posicoes():
        v.append((f"chave {cor}", bloco(x - 3, y - 3, P.Z_PCB_FRENTE, x + 3, y + 3, P.Z_PCB_FRENTE + CHAVE_CORPO)))
        v.append((f"botãozinho {cor}", cilindro_z(x, y, P.Z_PCB_FRENTE + CHAVE_CORPO - 0.01,
                                                  P.Z_PCB_FRENTE + CHAVE_ALTURA, 3.5)))
        for dx in (-3.25, 3.25):
            for dy in (-2.25, 2.25):
                v.append((f"perna da chave {cor}", cilindro_z(x + dx, y + dy, P.Z_PCB - P.SOLDA_ATRAS, P.Z_PCB, 1.6)))
    # a tela: a plaquinha, o vidro, o que tem atrás dela e os pinos
    tx0, tx1 = P.TELA_X, P.TELA_X + P.T.largura
    v.append(("plaquinha da tela", bloco(tx0, P.TELA_Y0, P.Z_TELA, tx1, P.TELA_Y1, P.Z_TELA_FRENTE)))
    v.append(("vidro da tela", bloco(tx0, P.VIDRO_Y[0], P.Z_TELA_FRENTE, tx1, P.VIDRO_Y[1], P.Z_VIDRO)))
    sx0, sy0 = P.tela_para_tijolo(P.T.sd_u[0], P.T.sd_v[0])
    sx1, sy1 = P.tela_para_tijolo(P.T.sd_u[1], P.T.sd_v[1])
    v.append(("soquete do cartão SD da tela", bloco(sx0, sy0, P.Z_TELA - P.T.sd_altura, sx1, sy1, P.Z_TELA)))
    px0, py0 = P.tela_para_tijolo(P.T.pecas_u[0], P.T.pecas_v[0])
    px1, py1 = P.tela_para_tijolo(P.T.pecas_u[1], P.T.pecas_v[1])
    v.append(("peças atrás da tela", bloco(px0, py0, P.Z_TELA - P.T.pecas_altura, px1, py1, P.Z_TELA)))
    xa, yp = P.pino_tela(1)
    xb, _ = P.pino_tela(14)
    v.append(("plástico dos pinos da tela", bloco(xa - 1.27, yp - 1.27, P.Z_PCB_FRENTE, xb + 1.27, yp + 1.27, P.Z_TELA)))
    v.append(("soldas dos pinos na frente da tela", bloco(xa - 1.0, yp - 1.0, P.Z_TELA_FRENTE, xb + 1.0, yp + 1.0,
                                                           P.Z_TELA_FRENTE + P.T.solda_frente)))
    for i in range(1, 15):
        if i in P.TELA_PINOS_CORTADOS:
            continue
        x, y = P.pino_tela(i)
        v.append((f"pino {i} da tela", cilindro_z(x, y, P.Z_PCB - P.SOLDA_ATRAS, P.Z_PCB, 1.6)))
    # o teclado: a base, a parte crespa com as teclas e a aba com as soldas
    kx0, kx1 = P.TECLADO_X0, P.TECLADO_X0 + P.K.largura
    zt = P.Z_TECLADO
    v.append(("teclado (base)", bloco(kx0, P.TECLADO_Y0, zt, kx1, P.TECLADO_Y1, zt + P.K.base)))
    # a parte alta tem as paredes inclinadas: em degraus, do tamanho de baixo (junto da aba) até o de cima
    degraus = 6
    for i in range(degraus):
        e = P.K.plato_chanfro * (1 - i / degraus)
        za = zt + P.K.base - 0.01 + i * (P.K.plato - P.K.base) / degraus
        zb = zt + P.K.base + (i + 1) * (P.K.plato - P.K.base) / degraus
        v.append(("teclado (parte crespa)", caixa_arredondada(P.PLATO_X[0] - e, P.PLATO_Y[0] - e, P.PLATO_X[1] + e,
                                                              P.PLATO_Y[1] + e, za, zb, 3.0 + e)))
    yc = (P.PLATO_Y[0] + P.PLATO_Y[1]) / 2
    for r in range(4):
        for c in range(3):
            x = P.MEIO_X + (c - 1) * P.K.passo[0]
            y = yc + (1.5 - r) * P.K.passo[1]
            tecla = (cq.Workplane("XY").workplane(offset=zt + P.K.plato - 0.01).center(x, y).rect(9.4, 9.4)
                     .workplane(offset=P.K.espessura - P.K.plato).rect(P.K.tecla, P.K.tecla).loft())
            v.append((f"teclado (tecla {P.TECLAS[r][c]})", tecla))
    v.append(("aba do teclado", bloco(P.MEIO_X - P.K.aba[0] / 2, P.TECLADO_Y0 - P.K.aba[1], zt,
                                       P.MEIO_X + P.K.aba[0] / 2, P.TECLADO_Y0 + 0.01, zt + 1.6 + 1.5)))
    for fx0, fx1 in ((3.0, 17.5), (23.5, 47.5)):
        v.append(("fita de espuma", bloco(fx0, 27.0, P.Z_PCB_FRENTE, fx1, 66.0, zt)))
    # o som
    v.append(("alto-falante", bloco(P.FALANTE_X0, P.FALANTE_Y + P.FALANTE_FOLGA, P.FUNDO + 0.05, P.FALANTE_X0 + P.FALANTE[0],
                                    P.FALANTE_Y + P.FALANTE_FOLGA + P.FALANTE[2], P.FUNDO + 0.05 + P.FALANTE[1])))
    v.append(("microfone", cilindro_y(P.MIC_X, P.MIC_Z, P.CAV_Y0 + 0.05, P.MIC_L, P.MIC_D)))
    return v


GRUPOS = ("lilygo", "encaixe", "pcb", "tela", "vidro", "pilha", "som", "teclado", "teclas")


def grupo_de(nome):
    if nome == "pilha 18650":
        return "pilha"
    if nome.startswith(("barra ", "pernas ")):
        return "encaixe"                              # as barras da LILYGO e as fêmeas da PCB
    if nome == "vidro da tela":
        return "vidro"
    if "tela" in nome:
        return "tela"
    if nome.startswith("teclado (tecla "):
        return "teclas"
    if "teclado" in nome or nome == "fita de espuma":
        return "teclado"
    if nome in ("alto-falante", "microfone"):
        return "som"
    if nome.startswith(("PCB", "chave", "botãozinho", "perna da chave", "capacitor")):
        return "pcb"
    return "lilygo"


def volume_comum(a, b):
    try:
        return a.intersect(b).val().Volume()
    except Exception:
        return 0.0


def conferir():
    """O que bate em quê (volume da interseção, mm³)"""
    pecas = miolo()
    caixa = [("fundo", fundo()), ("frente", frente())]
    erros = []
    # cada peça de imprimir tem de ser um sólido só (nada solto, nada preso por um fio)
    for nome, peca in caixa + [(f"botão {cor}", botao(cor)) for cor in ("verde", "vermelho")]:
        n = peca.solids().size()
        if n != 1:
            vols = sorted((round(s.Volume(), 1) for s in peca.solids().vals()), reverse=True)
            erros.append(f"{nome}: {n} pedaços soltos (volumes em mm³: {vols})")
    for nc, c in caixa:
        for np_, p in pecas:
            vol = volume_comum(c, p)
            if vol > 0.05:
                erros.append(f"{nc} × {np_}: {vol:.2f} mm³")
    for cor, b in botoes_no_lugar():
        vol = volume_comum(caixa[1][1], b)
        if vol > 0.05:
            erros.append(f"frente × botão {cor}: {vol:.2f} mm³")
        for np_, p in pecas:
            vol = volume_comum(b, p)
            if vol > 0.05 and not np_.startswith("botãozinho"):     # o botãozinho entra no recesso do botão
                erros.append(f"botão {cor} × {np_}: {vol:.2f} mm³")
    vol = volume_comum(caixa[0][1], caixa[1][1])
    if vol > 0.05:
        erros.append(f"fundo × frente: {vol:.2f} mm³")
    # as peças de dentro entre si, por grupos (dentro do mesmo grupo elas se encostam de propósito;
    # as barras encaixam na LILYGO e são soldadas na PCB)
    por_grupo = {}
    for n, p in pecas:
        g = grupo_de(n)
        por_grupo[g] = p if g not in por_grupo else por_grupo[g].union(p)
    nomes = list(por_grupo)
    for i, a in enumerate(nomes):
        for b in nomes[i + 1:]:
            if "encaixe" in (a, b) and {a, b} & {"lilygo", "pcb"}:
                continue
            if {a, b} == {"lilygo", "pilha"}:
                continue                              # a pilha fica presa no suporte da LILYGO
            if {a, b} in ({"tela", "vidro"}, {"teclado", "teclas"}):
                continue                              # são a mesma peça
            vol = volume_comum(por_grupo[a], por_grupo[b])
            if vol > 0.02:
                erros.append(f"{a} × {b}: {vol:.2f} mm³")
    return erros


def folgas(limite=0.45):
    """As menores distâncias entre a caixa e as peças de dentro (só as menores que o limite)"""
    from OCP.BRepExtrema import BRepExtrema_DistShapeShape
    pecas = miolo()
    caixa = [("fundo", fundo()), ("frente", frente())]
    linhas = []
    for nc, c in caixa:
        cs = c.val().wrapped
        for np_, p in pecas:
            d = BRepExtrema_DistShapeShape(cs, p.val().wrapped)
            d.Perform()
            if d.IsDone() and d.Value() < limite:
                linhas.append((d.Value(), f"{nc} × {np_}"))
    return sorted(linhas)


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
    for cor, b in botoes_no_lugar():
        leve(b, f"botao_{cor}.stl")
    grupos = {g: [] for g in GRUPOS}
    for n, p in miolo():
        grupos[grupo_de(n)].append(p)
    for g, lista in grupos.items():
        tudo = None
        for p in lista:
            tudo = p if tudo is None else tudo.union(p)
        leve(tudo, f"miolo_{g}.stl")
    return feitos


if __name__ == "__main__":
    pasta = os.path.join(os.path.dirname(AQUI), "saida", "caixa")
    os.makedirs(pasta, exist_ok=True)
    so = sys.argv[1:] or ["fundo", "frente", "botoes", "visual", "conferir"]
    if len(sys.argv) == 1:
        for velho in os.listdir(pasta):
            if velho.endswith((".stl", ".step")):
                os.remove(os.path.join(pasta, velho))
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
    if "botoes" in so:
        for cor in ("verde", "vermelho"):
            b = botao(cor)
            print(salvar(b, f"botao_{cor}.stl", pasta))
            cq.exporters.export(b, os.path.join(pasta, f"botao_{cor}.step"))
    if "visual" in so:
        for arq in salvar_visual(os.path.join(pasta, "visual")):
            print(arq)
    if "folgas" in so:
        for d, nome in folgas():
            print(f"  {d:5.2f} mm  {nome}")
    if "conferir" in so:
        erros = conferir()
        print("conferência da caixa:", "ok" if not erros else "")
        for e in erros:
            print("  ", e)
        if erros:
            sys.exit(1)
