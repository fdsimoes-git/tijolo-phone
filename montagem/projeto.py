"""
O Tijolo, segunda montagem: a LILYGO de costas e uma placa de circuito na frente.

As barras de pinos da LILYGO estão soldadas com os pinos para o lado da pilha
(e ficam assim). Então a LILYGO entra de costas na caixa: os componentes para
o fundo, a pilha para a frente. A placa de circuito da frente (encomendada)
encaixa nos pinos com duas barras fêmea de perna longa e passa por cima da
pilha. Na frente dela ficam a tela, os contatos do teclado 3×4 (as teclas e a
borracha do teclado que o Felipe já tem, desmontado) e os botões verde e
vermelho. Sem fio nenhum entre as placas, e a frente sai inteira para trocar
a pilha.

A tela vai de cabeça para baixo (os pinos embaixo, perto das teclas), para a
ponta de cima dela passar por cima do alto-falante; o firmware gira a imagem.

Coordenadas, em mm, olhando o Tijolo de frente (a tela e as teclas para
você), com a tela em cima:
  X: da esquerda para a direita, a partir da parede da esquerda (por dentro)
  Y: de baixo para cima, a partir da parede de baixo (por dentro)
  Z: de trás para a frente, a partir da face de fora do fundo da caixa

Medidas das peças: modelo 3D oficial da LILYGO (lilygo_pecas.json) e desenho
da tela MSP2401 (LCDWiki). O teclado ainda é provisório: as medidas dele
chegam com as fotos (ver TECLADO e MEDIDAS).
"""
import json
import os
from dataclasses import dataclass, field


def _mm(v: float, casas: int = 1) -> str:
    return f"{v:.{casas}f}".replace(".", ",")


# ---------------------------------------------------------------------------
# LILYGO T-A7670X-S3-Standard (coordenadas próprias: componentes para você,
# ESP32 em cima, origem no canto de baixo à esquerda)
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class Lilygo:
    largura: float = 33.07
    altura: float = 110.54
    espessura: float = 1.6
    raio: float = 2.03
    barra_x: tuple = (1.359, 31.808)        # as duas barras (J1 à esquerda, J2 à direita)
    pino1_y: float = 103.308                # pino 1 das duas barras (lado do ESP32)
    furos_fixacao: tuple = ((2.591, 2.316), (30.353, 2.316), (2.609, 107.878), (30.319, 107.868))
    # a chave liga/desliga, na borda esquerda: corpo y0..y1, botão y0..y1, quanto sai da borda, z do botão
    chave: tuple = (54.66, 63.66, 59.24, 60.54, 1.85, 1.15, 2.55)
    chave_corpo_x: tuple = (-1.9, 3.6)
    jst_solar: tuple = (0.42, 48.27, 5.22, 54.29, 6.25)     # x0, y0, x1, y1, altura: SAI
    sim: tuple = (0.75, 34.44, 13.06, 45.81)                # o chip entra pela borda esquerda
    usb_esp_y: float = 58.30
    usb_modem_y: float = 46.62
    usb_largura: float = 8.94
    usb_altura: float = 3.21
    usb_sai: float = 0.84                    # o USB passa da borda da placa
    qwiic: tuple = ((20.22, 26.52), (27.71, 34.01))   # na borda direita, do lado da pilha; saem 0,76
    qwiic_sai: float = 0.76
    ufl_lte: tuple = (16.40, 1.82)
    # lado da pilha
    suporte_18650: tuple = (6.49, 13.66, 27.15, 99.68)
    pilha_y: tuple = (24.2, 89.2)            # a pilha de 65 mm, centralizada no suporte
    pilha_x: float = 16.81                   # o eixo da pilha
    pilha_raio: float = 9.3
    bateria_fundo: float = 18.8             # da face da placa até o ponto mais alto da pilha (18,3 a 18,8)
    spek: tuple = (1.32, 10.49, 5.67, 14.24)
    mic: tuple = (1.41, 5.33, 5.76, 9.08)
    rtc: tuple = (22.09, 2.52, 25.84, 6.88)
    barra_plastico: float = 2.54             # o plástico da barra, do lado da pilha
    barra_pino: float = 6.0                  # CONFERIR: quanto o pino passa do plástico


L = Lilygo()

_AQUI = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(_AQUI, "lilygo_pecas.json"), encoding="utf-8") as _f:
    _PECAS = json.load(_f)
# o conector SOLAR sai (é a peça mais alta do lado dos componentes)
PECAS_COMP = [p for p in _PECAS["lado_de_cima"] if "HC-PH" not in p["nome"]]
PECAS_PILHA = _PECAS["lado_de_baixo"]

J1 = ["3V3", "GND", "IO1", "IO47", "IO14", "IO9", "IO36", "IO35", "IO0", "IO17", "IO48", "IO45",
      "IO11", "IO12", "IO13", "VBAT"]
J2 = ["TX", "RX", "3V3", "GND", "IO2", "IO3", "IO41", "IO40", "IO39", "IO38", "IO15", "IO16",
      "IO37", "IO21", "GND", "VBUS"]

# ---------------------------------------------------------------------------
# Espessuras (Z), de trás para a frente
# ---------------------------------------------------------------------------
FUNDO = 1.5                  # a parede de trás
FRENTE = 1.5                 # a parede da frente
PAREDE = 1.8                 # as paredes dos lados
VAO_COMP = 3.3               # do fundo até os componentes da LILYGO (o mais alto, o USB-C, tem 3,21)
FOLGA_PILHA = 0.5            # entre a pilha e a placa da frente
PCB_ESP = 1.2                # a placa de circuito
TELA_APOIO = 2.0             # entre a placa e a plaquinha da tela (sem o plástico da barra da tela)

Z_LILYGO_COMP = FUNDO + VAO_COMP                       # face dos componentes da LILYGO (virada para trás)
Z_LILYGO_PILHA = Z_LILYGO_COMP + L.espessura           # face do lado da pilha (virada para a frente)
Z_PILHA = Z_LILYGO_PILHA + L.bateria_fundo             # o ponto mais alto da pilha
Z_PCB = Z_PILHA + FOLGA_PILHA                          # face de trás da placa de circuito
Z_PCB_FRENTE = Z_PCB + PCB_ESP


@dataclass(frozen=True)
class Tela:
    """Tela 2,4" SPI ILI9341 (LCDWiki MSP2401, sem toque, com cartão micro SD atrás)"""
    largura: float = 42.72
    altura: float = 77.18
    pcb: float = 1.6
    vidro_alt: float = 60.26
    vidro_esp: float = 2.2 + 0.5             # vidro + fita
    area_larg: float = 36.72
    area_alt: float = 48.96
    pinos_da_borda: float = 2.0              # CONFERIR: da fileira de pinos até a borda da plaquinha
    vidro_da_borda_pinos: float = (77.18 - 60.26) / 2
    imagem_do_vidro: float = (60.26 - 48.96) / 2
    furos: tuple = (3.0, 4.96)               # furos Ø3,2: da borda comprida e da curta
    sd_altura: float = 1.8                   # CONFERIR: a peça mais alta atrás


T = Tela()
Z_TELA = Z_PCB_FRENTE + TELA_APOIO                     # face de trás da plaquinha da tela
Z_VIDRO = Z_TELA + T.pcb + T.vidro_esp                 # frente do vidro
Z_FRENTE_DENTRO = Z_VIDRO + 0.25
Z_TOPO = Z_FRENTE_DENTRO + FRENTE

# A barra fêmea de perna longa: encaixa nos pinos da LILYGO (assenta no plástico
# dela) e as pernas sobem até a placa de circuito
FEMEA_ALTURA = 8.5
Z_FEMEA0 = Z_LILYGO_PILHA + L.barra_plastico
Z_FEMEA1 = Z_FEMEA0 + FEMEA_ALTURA
FEMEA_PERNA_MIN = Z_PCB_FRENTE - Z_FEMEA1 + 0.6        # a perna precisa passar a placa e sobrar um pouco

# ---------------------------------------------------------------------------
# Em X (de frente)
# ---------------------------------------------------------------------------
CAV_X0 = 0.0
CAV_X1 = 46.3                # a largura de dentro
FOLGA = 0.3
PCB_X0, PCB_X1 = CAV_X0 + FOLGA, CAV_X1 - FOLGA
# A LILYGO de costas fica encostada na parede da esquerda: os USB-C saem por ela.
LILYGO_X = CAV_X0 + 0.34                               # a borda (de frente) da esquerda da LILYGO
TELA_X = (CAV_X0 + CAV_X1 - T.largura) / 2
TELA_CX = TELA_X + T.largura / 2

# ---------------------------------------------------------------------------
# O teclado (PROVISÓRIO até chegarem as medidas): as teclas e a borracha do
# teclado 3×4 do Felipe, sobre contatos desenhados na placa
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class Teclado:
    passo_x: float = 14.0                    # entre os centros das teclas, de lado
    passo_y: float = 11.0                    # entre os centros das fileiras
    tecla: tuple = (12.0, 8.0)               # o tamanho da tecla (de frente)
    contato: float = 6.0                     # o diâmetro do contato na placa (embaixo da pastilha da borracha)
    altura: float = 8.0                      # da placa até o topo da tecla (borracha + tecla)
    provisorio: bool = True


K = Teclado()
TECLAS = [["1", "2", "3"], ["4", "5", "6"], ["7", "8", "9"], ["*", "0", "#"]]
# a matriz: cada fileira e cada coluna num pino (os mesmos da montagem na placa ilhada)
GPIO_FILEIRA = ["IO15", "IO38", "IO39", "IO40"]        # 1 2 3 / 4 5 6 / 7 8 9 / * 0 #
GPIO_COLUNA = ["IO17", "IO37", "IO16"]                 # 1 4 7 * / 2 5 8 0 / 3 6 9 #
GPIO_VERDE, GPIO_VERMELHO = "IO35", "IO21"

# ---------------------------------------------------------------------------
# Em Y (de baixo para cima): as teclas, os botões, a tela; a LILYGO fica com a
# ponta de cima logo abaixo do alto-falante
# ---------------------------------------------------------------------------
PCB_Y0 = 5.4                 # a borda de baixo da placa (embaixo: os pilares dos parafusos)
TECLA_Y0 = PCB_Y0 + 1.5 + K.tecla[1] / 2               # centro da fileira de baixo (* 0 #)
TECLAS_Y = [TECLA_Y0 + K.passo_y * (3 - i) for i in range(4)]   # fileiras 1 2 3 .. * 0 #
TECLAS_X = [TELA_CX + K.passo_x * (j - 1) for j in range(3)]
BOTOES_Y = TECLAS_Y[0] + K.passo_y                     # verde e vermelho, em cima do 1 e do 3
BOTAO = 6.0                  # chave táctil 6×6
TELA_BASE = BOTOES_Y + BOTAO / 2 + 0.5
TELA_TOPO = TELA_BASE + T.altura
TELA_PINOS_Y = TELA_BASE + T.pinos_da_borda            # a tela de cabeça para baixo: os pinos embaixo
VIDRO_BASE = TELA_BASE + T.vidro_da_borda_pinos
VIDRO_TOPO = VIDRO_BASE + T.vidro_alt
IMAGEM_BASE = VIDRO_BASE + T.imagem_do_vidro
IMAGEM_TOPO = IMAGEM_BASE + T.area_alt
CAV_Y0 = 0.0
CAV_Y1 = TELA_TOPO + 0.4

# Alto-falante 3525 em pé no topo, virado para a grade da parede de cima, embaixo da ponta da tela
FALANTE = (35.0, 25.0, 4.6)                  # largura (X), altura (Z), espessura (Y)
FALANTE_X0 = (CAV_X0 + CAV_X1 - FALANTE[0]) / 2
FALANTE_AR = 2.5                             # na frente do cone, até a parede (passa a lingueta da frente)
FALANTE_Y = CAV_Y1 - FALANTE_AR - FALANTE[2]  # face de trás
LILYGO_Y = FALANTE_Y - 1.1 - L.altura        # a ponta de baixo (do modem) da LILYGO (o berço do falante no meio)
PCB_Y1 = FALANTE_Y - 0.5                     # a placa acaba antes do alto-falante
# Microfone 9767 (Ø9,7 × 6,7) embaixo, deitado, virado para a parede de baixo
MIC_D, MIC_L = 9.7, 6.7
MIC_X, MIC_Z = (CAV_X0 + CAV_X1) / 2, 13.0


def lilygo_para_tijolo(x: float, y: float):
    """coordenadas da LILYGO (componentes para você) → X, Y do Tijolo (ela de costas: espelha em X)"""
    return LILYGO_X + (L.largura - x), LILYGO_Y + y


def caixa_lilygo(x0, y0, x1, y1):
    """um retângulo da LILYGO → (X0, Y0, X1, Y1) do Tijolo"""
    a = lilygo_para_tijolo(x0, y0)
    b = lilygo_para_tijolo(x1, y1)
    return min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]), max(a[1], b[1])


def pino_barra(barra: str, n: int):
    """(X, Y) do pino n (1..16) da barra "J1" ou "J2" """
    bx = L.barra_x[0] if barra == "J1" else L.barra_x[1]
    return lilygo_para_tijolo(bx, L.pino1_y - 2.54 * (n - 1))


def barra_de(gpio: str):
    """"IO36" → ("J1", 7)"""
    for nome, lista in (("J1", J1), ("J2", J2)):
        if gpio in lista:
            return nome, lista.index(gpio) + 1
    raise KeyError(gpio)


# A tela de cabeça para baixo: de frente, o pino 1 (VCC) fica à direita (com o
# VCC à esquerda quando os pinos estão em cima, que é o normal). Com
# TELA_ESPELHADA=1 (o VCC à direita com os pinos em cima), fica à esquerda.
TELA_ESPELHADA = os.environ.get("TELA_ESPELHADA") == "1"
TELA_PINOS = ["VCC", "GND", "CS", "RESET", "DC", "SDI(MOSI)", "SCK", "LED", "SDO(MISO)",
              "T_CLK", "T_CS", "T_DIN", "T_DO", "T_IRQ"]
TELA_GPIO = {"VCC": "3V3", "GND": "GND", "CS": "IO47", "RESET": "IO9", "DC": "IO14", "SDI(MOSI)": "IO11",
             "SCK": "IO12", "LED": "IO36", "SDO(MISO)": "IO13"}


def pino_tela(i: int):
    """(X, Y) do pino i (1..14) da tela"""
    sentido = 1 if TELA_ESPELHADA else -1
    return TELA_CX + sentido * (i - 7.5) * 2.54, TELA_PINOS_Y


# ---------------------------------------------------------------------------
# As medidas que o Felipe confere (o guia mostra um formulário com elas)
# ---------------------------------------------------------------------------
MEDIDAS = [
    ("pino_lilygo", "LILYGO: quanto o pino da barra passa do plástico", "do lado da pilha", _mm(L.barra_pino)),
    ("pilha", "LILYGO com a pilha: da face da placa até o ponto mais alto da pilha", "", _mm(L.bateria_fundo)),
    ("tela_pinos", "Tela: da fileira de pinos até a borda da plaquinha", "do lado dos pinos", _mm(T.pinos_da_borda)),
    ("tela_vidro", "Tela: da borda da plaquinha (lado dos pinos) até o começo do vidro", "", _mm(T.vidro_da_borda_pinos, 2)),
    ("tela_imagem", "Tela: do começo do vidro até o começo da imagem", "com a tela acesa, mostrando branco",
     _mm(T.imagem_do_vidro, 2)),
    ("tela_sd", "Tela: a peça mais alta atrás", "normalmente o cartão micro SD; e diga de que lado fica", _mm(T.sd_altura)),
    ("falante", "Alto-falante: largura × altura × espessura", "",
     f"{_mm(FALANTE[0], 0)} × {_mm(FALANTE[1], 0)} × {_mm(FALANTE[2])}"),
    ("mic", "Microfone: diâmetro × altura", "", f"{_mm(MIC_D)} × {_mm(MIC_L)}"),
]


def resumo() -> str:
    j1a, j1b = pino_barra("J1", 1), pino_barra("J1", 16)
    j2a = pino_barra("J2", 1)
    linhas = [
        f"LILYGO (de costas): X {LILYGO_X:.2f}..{LILYGO_X + L.largura:.2f}, Y {LILYGO_Y:.2f}..{LILYGO_Y + L.altura:.2f}",
        f"barras: J1 em X {j1a[0]:.2f}, J2 em X {j2a[0]:.2f}, Y {j1b[1]:.2f}..{j1a[1]:.2f}",
        f"placa de circuito: X {PCB_X0:.2f}..{PCB_X1:.2f}, Y {PCB_Y0:.2f}..{PCB_Y1:.2f} "
        f"({PCB_X1 - PCB_X0:.1f} × {PCB_Y1 - PCB_Y0:.1f} mm, {PCB_ESP} mm)",
        f"tela: X {TELA_X:.2f}..{TELA_X + T.largura:.2f}, Y {TELA_BASE:.2f}..{TELA_TOPO:.2f}, pinos em Y {TELA_PINOS_Y:.2f}",
        f"teclas: fileiras em Y {', '.join(f'{y:.1f}' for y in TECLAS_Y)}; botões em Y {BOTOES_Y:.1f}",
        f"Z: LILYGO {Z_LILYGO_COMP:.2f}..{Z_LILYGO_PILHA:.2f}, pilha até {Z_PILHA:.2f}, placa {Z_PCB:.2f}..{Z_PCB_FRENTE:.2f}, "
        f"tela {Z_TELA:.2f}, vidro {Z_VIDRO:.2f}, topo {Z_TOPO:.2f}",
        f"barra fêmea: {Z_FEMEA0:.2f}..{Z_FEMEA1:.2f}, perna de pelo menos {FEMEA_PERNA_MIN:.1f} mm",
        f"caixa por fora: {CAV_X1 - CAV_X0 + 2 * PAREDE:.1f} × {CAV_Y1 - CAV_Y0 + 2 * PAREDE:.1f} × {Z_TOPO:.1f} mm",
    ]
    return "\n".join(linhas)


if __name__ == "__main__":
    print(resumo())
