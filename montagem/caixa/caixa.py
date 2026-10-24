"""
A caixa do Tijolo (segunda montagem: LILYGO de costas, placa de circuito na
frente) para imprimir em 3D: o fundo, a frente e os botões verde e vermelho.
As 12 teclas são as do teclado 3×4 do Felipe (desmontado): a frente tem as
aberturas delas.

    python3 caixa/caixa.py            (da pasta montagem/; precisa do CadQuery)

Sai em montagem/saida/caixa/: fundo.stl, frente.stl (a cara para baixo, do
jeito de imprimir), botao_verde.stl e botao_vermelho.stl; para ver:
frente_no_lugar.stl e visual/ (versões leves, para a vista 3D do guia). No fim
confere se alguma peça de dentro bate na caixa.
Para fazer só uma parte: caixa.py fundo | frente | botoes | visual | conferir.

Como fecha: a frente desce reta sobre o fundo; em cima, duas linguetas com
dente entram nas janelas da parede de cima (clic); embaixo, dois parafusos M2
entram por trás nos pilares da frente. A LILYGO fica de costas em 4 pilares
com pino nos furos dela; a placa de circuito encaixa nos pinos da LILYGO e
apoia em pilares do fundo; a frente prende a tela e a borracha do teclado.
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
ZD = P.Z_PCB_FRENTE                # onde a frente encontra o fundo: a face da placa de circuito

# parafusos de baixo: M2 × 8, entram por trás (pelo fundo) nos pilares da frente
PARAF_X = (6.0, P.CAV_X1 - 6.0)
PARAF_Y = 2.7
PILAR_D = 4.6
PILAR_Z0 = P.FUNDO + 0.2

# linguetas de cima (frente): descem por dentro da parede de cima e o dente
# entra na janela dela. Ficam entre a parede e o alto-falante.
LING = dict(xs=(P.CAV_X1 / 2 - 10.9, P.CAV_X1 / 2 + 10.9), larg=10.0, esp=1.2, folga=0.15,
            dente=0.6, dente_alt=1.2, comp=6.5)
LING["z0"] = ZD - LING["comp"]
RASGO = dict(fundo=0.8, z0=LING["z0"] - 0.3, z1=LING["z0"] + LING["dente_alt"] + 0.4)

# pilares do fundo que seguram a placa de circuito por trás (x, y)
APOIOS = [(12.0, 9.0), (P.CAV_X1 - 12.0, 9.0), (40.0, P.LILYGO_Y + 12.0), (40.0, P.LILYGO_Y + 92.0)]
APOIO_D = 3.0

# os botões verde e vermelho: chave táctil 6×6×5 e um botão impresso
CHAVE_ALTURA = 5.0                 # da face da placa até o topo do botãozinho
CHAVE_CORPO = 3.5
BOTAO = (12.0, 7.0)                # o botão impresso, de frente
FOLGA_BOTAO = 0.3
ABA_BOTAO = 0.5
ABA_ALT = 1.8
SAI_BOTAO = 1.2
RECESSO_PINO = (3.9, 0.55)
Z_BOTAO0 = P.Z_PCB_FRENTE + CHAVE_ALTURA - RECESSO_PINO[1]


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


def botoes_posicoes():
    return [("verde", P.TECLAS_X[0], P.BOTOES_Y), ("vermelho", P.TECLAS_X[2], P.BOTOES_Y)]


# ---------------------------------------------------------------------------
# o fundo
# ---------------------------------------------------------------------------
def fundo():
    f = caixa_arredondada(X0, Y0, X1, Y1, 0, ZD, R_FORA)
    f = f.faces("<Z").edges().fillet(BORDA)
    f = f.cut(caixa_arredondada(P.CAV_X0, P.CAV_Y0, P.CAV_X1, P.CAV_Y1, P.FUNDO, ZD + 1, R_DENTRO))

    # pilares da LILYGO (de costas: os componentes para o fundo), com pino no furo dela
    for (fx, fy) in P.L.furos_fixacao:
        x, y = P.lilygo_para_tijolo(fx, fy)
        f = f.union(cilindro_z(x, y, P.FUNDO - 0.1, P.Z_LILYGO_COMP, 5.2))
        f = f.union(cilindro_z(x, y, P.Z_LILYGO_COMP - 0.1, P.Z_LILYGO_COMP + 1.3, 2.1).faces(">Z").edges().chamfer(0.3))

    # a chave liga/desliga da LILYGO: um bolso para o corpo e uma fenda para o botãozinho
    cy0, cy1, by0, by1, sai, bz0, bz1 = P.L.chave
    ax0, ay0, ax1, ay1 = P.caixa_lilygo(P.L.chave_corpo_x[0], cy0, P.L.chave_corpo_x[1], cy1)
    prof = P.Z_LILYGO_COMP - 3.65 - 0.3                 # o corpo tem 3,65 de altura; o vão, 3,3
    f = f.cut(bloco(ax0 - 0.3, ay0 - 0.3, prof, ax1 + 0.3, ay1 + 0.3, P.FUNDO + 0.5))
    kx0, ky0, kx1, ky1 = P.caixa_lilygo(-sai, by0, 0.0, by1)
    f = f.cut(bloco(kx0 - 0.2, ky0 - 1.8, -0.5, kx1 + 0.2, ky1 + 1.8, P.FUNDO + 0.5))

    # USB-C: os dois saem da borda da esquerda (de frente), perto do fundo. O da ESP32 (carregar e
    # gravar) tem abertura para a capa do cabo, aberta para trás; o do modem só tem o canal por dentro.
    usb_z = P.Z_LILYGO_COMP - 1.6
    for yc, abre in ((P.L.usb_esp_y, True), (P.L.usb_modem_y, False)):
        y = P.LILYGO_Y + yc
        f = f.cut(bloco(P.CAV_X0 - 0.7, y - 5.0, usb_z - 2.0, P.CAV_X0 + 0.1, y + 5.0, ZD + 1))
        if abre:
            janela = (cq.Workplane("YZ").workplane(offset=X0 - 0.5).center(y, usb_z)
                      .rect(12.4, 6.4).extrude(P.PAREDE + 1.0).edges("|X").fillet(2.6))
            f = f.cut(janela)
            f = f.cut(bloco(X0 - 0.5, y - 6.2 + 2.6, -0.5, P.CAV_X0 + 0.1, y + 6.2 - 2.6, usb_z))   # aberta para trás
    # os QWIIC (do lado da pilha) também passam da borda: canal
    q0 = P.LILYGO_Y + P.L.qwiic[0][0] - 0.3
    q1 = P.LILYGO_Y + P.L.qwiic[1][1] + 0.3
    f = f.cut(bloco(P.CAV_X0 - 0.7, q0, P.Z_LILYGO_PILHA - 0.3, P.CAV_X0 + 0.1, q1, ZD + 1))

    # o rebaixo da antena, na parede da direita por dentro
    f = f.cut(bloco(P.CAV_X1 - 0.1, 14.0, P.FUNDO + 0.4, P.CAV_X1 + 0.3, 14.0 + 111.5, P.FUNDO + 0.4 + 20.6))

    # berço do alto-falante (em pé, virado para a parede de cima): uma calha em cada ponta
    fy0, fy1 = P.FALANTE_Y, P.FALANTE_Y + P.FALANTE[2]
    for xp, lado in ((P.FALANTE_X0, -1), (P.FALANTE_X0 + P.FALANTE[0], 1)):
        b = bloco(min(xp - 1.5 * lado, xp + 2.5 * lado), fy0 - 0.8, P.FUNDO - 0.1,
                  max(xp - 1.5 * lado, xp + 2.5 * lado), fy1 + 1.2, P.FUNDO + 12.0)
        b = b.cut(bloco(min(xp - 1.6 * lado, xp + 0.2 * lado), fy0 - 0.2, P.FUNDO,
                        max(xp - 1.6 * lado, xp + 0.2 * lado), fy1 + 0.2, P.FUNDO + 13.0))
        f = f.union(b)
    # a grade do som na parede de cima
    n_fendas = 10
    x_ini = P.FALANTE_X0 + P.FALANTE[0] / 2 - (n_fendas - 1) * 2.6 / 2 - 0.65
    for i in range(n_fendas):
        x = x_ini + i * 2.6
        f = f.cut(caixa_arredondada(x, P.CAV_Y1 - 0.5, x + 1.3, Y1 + 0.5, 5.0, 18.5, 0.6))

    # berço do microfone (deitado, virado para a parede de baixo) e os furos do som
    mx, mz, md = P.MIC_X, P.MIC_Z, P.MIC_D
    b = bloco(mx - md / 2 - 1.6, P.CAV_Y0 - 0.1, P.FUNDO - 0.1, mx + md / 2 + 1.6, P.MIC_L - 0.6, mz + 1.0)
    b = b.cut(cilindro_y(mx, mz, P.CAV_Y0 - 0.2, P.MIC_L + 0.2, md + 0.3))
    f = f.union(b)
    for dx, dz in ((-1.7, -1.0), (1.7, -1.0), (0.0, 1.9)):
        f = f.cut(cilindro_y(mx + dx, mz + dz, Y0 - 0.5, P.CAV_Y0 + 0.5, 1.2))

    # os pilares que seguram a placa de circuito por trás
    for x, y in APOIOS:
        f = f.union(cilindro_z(x, y, P.FUNDO - 0.1, P.Z_PCB - 0.05, APOIO_D))

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

    # o nome atrás (espelhado: lido por trás) e a marca da chave
    meio = (X0 + X1) / 2
    nome = (cq.Workplane("XY").workplane(offset=-0.01)
            .center(meio, (Y0 + Y1) / 2 - 30)
            .text("TIJOLO", 7.0, 0.45, fontPath=FONTE, halign="center", valign="center", combine=False)
            .mirror("YZ", basePointVector=(meio, 0, 0)))
    f = f.cut(nome)
    ky = (ky0 + ky1) / 2
    marca = (cq.Workplane("XY").workplane(offset=-0.01)
             .center(kx0 - 4.5, ky)
             .text("LIGA ↑", 2.6, 0.35, fontPath=FONTE, halign="center", valign="center", combine=False)
             .mirror("YZ", basePointVector=(kx0 - 4.5, 0, 0)))
    f = f.cut(marca)
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
    jx0, jx1 = P.TELA_CX - P.T.area_larg / 2 - m, P.TELA_CX + P.T.area_larg / 2 + m
    jy0, jy1 = P.IMAGEM_BASE - m, P.IMAGEM_TOPO + m
    f = f.cut(caixa_arredondada(jx0, jy0, jx1, jy1, P.Z_FRENTE_DENTRO - 0.5, P.Z_TOPO + 1, 1.0))
    chanfro = (cq.Workplane("XY").workplane(offset=P.Z_TOPO - 0.8)
               .center((jx0 + jx1) / 2, (jy0 + jy1) / 2).rect(jx1 - jx0, jy1 - jy0)
               .workplane(offset=0.81).rect(jx1 - jx0 + 1.6, jy1 - jy0 + 1.6).loft())
    f = f.cut(chanfro)

    # as aberturas das 12 teclas do teclado (PROVISÓRIO: o tamanho das teclas de verdade)
    tx, ty = P.K.tecla
    for r in range(4):
        for c in range(3):
            x, y = P.TECLAS_X[c], P.TECLAS_Y[r]
            f = f.cut(caixa_arredondada(x - tx / 2 - 0.3, y - ty / 2 - 0.3, x + tx / 2 + 0.3, y + ty / 2 + 0.3,
                                        P.Z_FRENTE_DENTRO - 0.2, P.Z_TOPO + 1, 1.2))

    # os botões verde e vermelho: poço que guia o botão e furo na frente
    lp = BOTAO[0] + 2 * ABA_BOTAO + 2 * 0.2
    ap = BOTAO[1] + 2 * ABA_BOTAO + 2 * 0.2
    z_poco0 = P.Z_PCB_FRENTE + CHAVE_CORPO + 0.25
    for _, x, y in botoes_posicoes():
        f = f.union(caixa_arredondada(x - lp / 2 - 1.2, y - ap / 2 - 1.2, x + lp / 2 + 1.2, y + ap / 2 + 1.2,
                                      z_poco0, P.Z_FRENTE_DENTRO + 0.1, 1.6))
    for _, x, y in botoes_posicoes():
        f = f.cut(caixa_arredondada(x - lp / 2, y - ap / 2, x + lp / 2, y + ap / 2, z_poco0 - 0.1, P.Z_FRENTE_DENTRO, 1.4))
        f = f.cut(caixa_arredondada(x - BOTAO[0] / 2 - FOLGA_BOTAO, y - BOTAO[1] / 2 - FOLGA_BOTAO,
                                    x + BOTAO[0] / 2 + FOLGA_BOTAO, y + BOTAO[1] / 2 + FOLGA_BOTAO,
                                    P.Z_FRENTE_DENTRO - 0.2, P.Z_TOPO + 1, 1.4))

    # pilares dos parafusos de baixo (descem até perto do fundo; o parafuso entra por trás)
    for x in PARAF_X:
        pilar = cilindro_z(x, PARAF_Y, PILAR_Z0, P.Z_FRENTE_DENTRO + 0.1, PILAR_D)
        pilar = pilar.union(bloco(x - PILAR_D / 2, P.CAV_Y0 + 0.15, ZD - 2.0, x + PILAR_D / 2, PARAF_Y,
                                  P.Z_FRENTE_DENTRO + 0.1))
        f = f.union(pilar.cut(cilindro_z(x, PARAF_Y, PILAR_Z0 - 0.1, PILAR_Z0 + 9.0, 1.6)))

    # linguetas de cima: descem por dentro da parede de cima; o dente tem rampa
    # embaixo (entra apertando) e 30° em cima (sai puxando firme)
    ly1 = P.CAV_Y1 - LING["folga"]
    for x in LING["xs"]:
        x0, x1 = x - LING["larg"] / 2, x + LING["larg"] / 2
        f = f.union(bloco(x0, ly1 - LING["esp"], LING["z0"], x1, ly1, ZD + 0.5))
        z0, d = LING["z0"], LING["dente"]
        perfil = (cq.Workplane("YZ").workplane(offset=x0)
                  .polyline([(ly1 - 0.05, z0), (ly1 + d, z0 + 0.6), (ly1 + d, z0 + 0.75), (ly1 - 0.05, z0 + 1.2)]).close()
                  .extrude(x1 - x0))
        f = f.union(perfil)
    return f


# ---------------------------------------------------------------------------
# os botões verde e vermelho
# ---------------------------------------------------------------------------
def botao(cor: str):
    """O botão em pé, com a base em z = 0 (o recesso do botãozinho embaixo, o ☎ gravado em cima)"""
    lg, al = BOTAO
    altura = P.Z_TOPO + SAI_BOTAO - Z_BOTAO0
    aba = (cq.Workplane("XY").rect(lg + 2 * ABA_BOTAO, al + 2 * ABA_BOTAO).extrude(ABA_ALT)
           .edges("|Z").fillet(1.6))
    corpo = (cq.Workplane("XY").workplane(offset=ABA_ALT - 0.01).rect(lg, al).extrude(altura - ABA_ALT + 0.01)
             .edges("|Z").fillet(1.4).faces(">Z").edges().fillet(0.6))
    t = aba.union(corpo)
    t = t.cut(cq.Workplane("XY").workplane(offset=-0.01).circle(RECESSO_PINO[0] / 2).extrude(RECESSO_PINO[1] + 0.01))
    fundo = 0.5
    txt = (cq.Workplane("XY").workplane(offset=altura - fundo)
           .text("☎", 5.0, fundo + 0.2, fontPath=FONTE, halign="center", valign="center", combine=False))
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
def miolo():
    """Cada peça de dentro como um sólido simples: [(nome, sólido)]"""
    v = []
    zc, zp = P.Z_LILYGO_COMP, P.Z_LILYGO_PILHA
    lx0, ly0, lx1, ly1 = P.caixa_lilygo(0, 0, P.L.largura, P.L.altura)
    pcb = caixa_arredondada(lx0, ly0, lx1, ly1, zc, zp, P.L.raio)
    for (fx, fy) in P.L.furos_fixacao:
        x, y = P.lilygo_para_tijolo(fx, fy)
        pcb = pcb.cut(cilindro_z(x, y, zc - 1, zp + 1, 2.5))
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
    s = P.L.suporte_18650
    hx0, hy0, hx1, hy1 = P.caixa_lilygo(*s)
    v.append(("ponta do suporte da pilha", bloco(hx0, hy0, zp, hx1, hy0 + 2.0, zp + 15.0)))
    v.append(("ponta do suporte da pilha 2", bloco(hx0, hy1 - 2.0, zp, hx1, hy1, zp + 15.0)))
    ex, _ = P.lilygo_para_tijolo(P.L.pilha_x, 0)
    py0, py1 = P.LILYGO_Y + P.L.pilha_y[0], P.LILYGO_Y + P.L.pilha_y[1]
    v.append(("pilha 18650", cilindro_y(ex, zp + P.L.bateria_fundo - P.L.pilha_raio, py0, py1, 2 * P.L.pilha_raio)))
    # as barras: o plástico da LILYGO, a fêmea encaixada e as pernas até a placa
    for barra in ("J1", "J2"):
        x, ya = P.pino_barra(barra, 1)
        _, yb = P.pino_barra(barra, 16)
        v.append((f"barra {barra}", bloco(x - 1.27, yb - 1.27, zp, x + 1.27, ya + 1.27, P.Z_FEMEA1)))
        v.append((f"pernas {barra}", bloco(x - 0.4, yb - 0.4, P.Z_FEMEA1, x + 0.4, ya + 0.4, P.Z_PCB_FRENTE + 1.0)))
    # a placa de circuito
    v.append(("placa de circuito", caixa_arredondada(P.PCB_X0, P.PCB_Y0, P.PCB_X1, P.PCB_Y1, P.Z_PCB,
                                                     P.Z_PCB_FRENTE, 1.5)))
    # os capacitores atrás da placa (perto do VCC da tela) e as soldas dos pinos da tela
    xv, yv = P.pino_tela(1)
    lado = 1 if xv > P.TELA_CX else -1
    v.append(("capacitores", bloco(xv + lado * 1.0 - 2.0, yv + 2.0, P.Z_PCB - 1.6, xv + lado * 1.0 + 2.0,
                                   yv + 14.2, P.Z_PCB)))
    # os botões (chaves 6×6)
    for cor, x, y in botoes_posicoes():
        v.append((f"chave {cor}", bloco(x - 3, y - 3, P.Z_PCB_FRENTE, x + 3, y + 3, P.Z_PCB_FRENTE + CHAVE_CORPO)))
        v.append((f"botãozinho {cor}", cilindro_z(x, y, P.Z_PCB_FRENTE + CHAVE_CORPO - 0.01,
                                                  P.Z_PCB_FRENTE + CHAVE_ALTURA, 3.5)))
    # a tela, apoiada 2 mm acima da placa
    v.append(("plaquinha da tela", bloco(P.TELA_X, P.TELA_BASE, P.Z_TELA, P.TELA_X + P.T.largura, P.TELA_TOPO,
                                         P.Z_TELA + P.T.pcb)))
    v.append(("vidro da tela", bloco(P.TELA_X, P.VIDRO_BASE, P.Z_TELA + P.T.pcb, P.TELA_X + P.T.largura,
                                     P.VIDRO_TOPO, P.Z_VIDRO)))
    v.append(("peças atrás da tela", bloco(P.TELA_X + 3, P.TELA_BASE + 6, P.Z_TELA - P.T.sd_altura,
                                           P.TELA_X + P.T.largura - 3, P.TELA_TOPO - 3, P.Z_TELA)))
    # o teclado (PROVISÓRIO: a borracha e as teclas como um bloco por tecla)
    tx, ty = P.K.tecla
    for r in range(4):
        for c in range(3):
            x, y = P.TECLAS_X[c], P.TECLAS_Y[r]
            v.append((f"tecla {P.TECLAS[r][c]}", bloco(x - tx / 2, y - ty / 2, P.Z_PCB_FRENTE + 0.05,
                                                      x + tx / 2, y + ty / 2, P.Z_TOPO + 1.0)))
    v.append(("alto-falante", bloco(P.FALANTE_X0, P.FALANTE_Y, P.FUNDO + 0.05, P.FALANTE_X0 + P.FALANTE[0],
                                    P.FALANTE_Y + P.FALANTE[2], P.FUNDO + P.FALANTE[1])))
    v.append(("microfone", cilindro_y(P.MIC_X, P.MIC_Z, P.CAV_Y0 + 0.05, P.MIC_L, P.MIC_D)))
    return v


def conferir():
    """O que bate em quê (volume da interseção, mm³)"""
    pecas = miolo()
    caixa = [("fundo", fundo()), ("frente", frente())]
    erros = []
    for nc, c in caixa:
        for np_, p in pecas:
            if nc == "frente" and np_.startswith("tecla "):
                continue                                  # a tecla passa pela abertura de propósito
            try:
                vol = c.intersect(p).val().Volume()
            except Exception:
                vol = 0.0
            if vol > 0.05:
                erros.append(f"{nc} × {np_}: {vol:.2f} mm³")
    for cor, b in botoes_no_lugar():
        vol = caixa[1][1].intersect(b).val().Volume()
        if vol > 0.05:
            erros.append(f"frente × botão {cor}: {vol:.2f} mm³")
    vol = caixa[0][1].intersect(caixa[1][1]).val().Volume()
    if vol > 0.05:
        erros.append(f"fundo × frente: {vol:.2f} mm³")
    return erros


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
    grupos = {"lilygo": [], "placa": [], "tela": [], "pilha": [], "som": [], "teclas": []}
    for n, p in miolo():
        g = ("pilha" if "pilha" in n and "suporte" not in n else "tela" if "tela" in n else
             "teclas" if n.startswith("tecla ") else
             "placa" if n.startswith(("placa de circuito", "chave", "botãozinho", "barra", "pernas", "capacitores")) else
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
    for velho in os.listdir(pasta):
        if velho.endswith((".stl", ".step")):
            os.remove(os.path.join(pasta, velho))
    so = sys.argv[1:] or ["fundo", "frente", "botoes", "visual", "conferir"]
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
        print(salvar(botao("verde"), "botao_verde.stl", pasta))
        print(salvar(botao("vermelho"), "botao_vermelho.stl", pasta))
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
