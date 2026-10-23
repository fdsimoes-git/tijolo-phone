"""
O Tijolo montado em sanduíche: as posições de tudo, num lugar só.

A placa da frente é uma placa ilhada (perfurada) de 18 × 51 furos. Ela leva
as 14 teclas e a tela, e encaixa direto nas duas barras de pinos da LILYGO,
que fica atrás (com a pilha virada para o fundo da caixa). Na frente da
placa não corre nenhum fio: só pontes de solda. Os fios ficam atrás, em dois
lugares:
  - a faixa da esquerda (colunas A a D), que fica fora da LILYGO e continua
    ao alcance do ferro depois de juntar as placas: a tela e o lado esquerdo
    do teclado;
  - entre as placas, por cima da LILYGO: a matriz do teclado e os fios que
    sobem até a barra da direita. Esses vão ANTES de juntar.

Coordenadas, em mm, olhando o Tijolo de frente (a tela e as teclas para
você), com a tela em cima:
  X: da esquerda para a direita, a partir da borda esquerda da placa da frente
  Y: de baixo para cima, a partir da parede de baixo da caixa (por dentro)
  Z: de trás para a frente, a partir da face de fora do fundo da caixa
Furos da placa: coluna A..R (da esquerda para a direita) e linha 1..51
(de cima para baixo), como "F12".

Medidas das peças: pesquisa de 23/10/2026 (modelo 3D oficial da LILYGO, em
lilygo_pecas.json; desenho da tela MSP2401 da LCDWiki). As marcadas com
CONFERIR o usuário mede antes de encomendar a caixa (ver MEDIDAS).
"""
import json
import os
from dataclasses import dataclass

PASSO = 2.54
COLUNAS = 18                 # A..R
LINHAS = 51                  # k = 0 (embaixo) .. 50 (em cima); linha impressa = 51 - k

PLACA_Y0 = 5.40              # borda de baixo da placa da frente (abaixo: as abas dos parafusos)
PLACA_LARGURA = COLUNAS * PASSO            # 45.72
PLACA_ALTURA = LINHAS * PASSO              # 129.54
PLACA_ESPESSURA = 1.6


def col_x(c: float) -> float:
    return PASSO / 2 + PASSO * c


def k_y(k: float) -> float:
    return PLACA_Y0 + PASSO / 2 + PASSO * k


def letra(c: int) -> str:
    return chr(65 + c)


def furo(c: int, k: int) -> str:
    """(5, 40) → "F11" (a linha impressa conta de cima para baixo)"""
    return f"{letra(c)}{LINHAS - k}"


def ck(nome: str):
    """"F11" → (5, 40)"""
    c = ord(nome[0].upper()) - 65
    return c, LINHAS - int(nome[1:])


def xy(nome: str):
    c, k = ck(nome)
    return col_x(c), k_y(k)


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
    jst_solar: tuple = (0.42, 48.27, 5.22, 54.29, 6.25)     # x0, y0, x1, y1, altura: SAI (ver o guia)
    sim: tuple = (0.75, 34.44, 13.06, 45.81)
    usb_esp_y: float = 58.30
    usb_modem_y: float = 46.62
    usb_largura: float = 8.94
    usb_altura: float = 3.21
    usb_sai: float = 0.84                    # o USB passa da borda da placa
    qwiic: tuple = ((20.22, 26.52), (27.71, 34.01))   # na borda direita, embaixo; saem 0,76
    qwiic_sai: float = 0.76
    ufl_lte: tuple = (16.40, 1.82)
    # lado da pilha (embaixo)
    suporte_18650: tuple = (6.49, 13.66, 27.15, 99.68)
    bateria_fundo: float = 18.8             # da face de baixo da placa até a pilha (18,3 a 18,8)
    spek: tuple = (1.32, 10.49, 5.67, 14.24)
    mic: tuple = (1.41, 5.33, 5.76, 9.08)
    rtc: tuple = (22.09, 2.52, 25.84, 6.88)


L = Lilygo()

_AQUI = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(_AQUI, "lilygo_pecas.json"), encoding="utf-8") as _f:
    _PECAS = json.load(_f)
# as peças do lado de cima, menos o conector SOLAR (sai: é mais alto que o vão entre as placas)
PECAS_CIMA = [p for p in _PECAS["lado_de_cima"] if "HC-PH" not in p["nome"]]
PECAS_BAIXO = _PECAS["lado_de_baixo"]

# As barras da LILYGO caem nas colunas F e R da placa da frente; o pino 1 na
# linha 9. A LILYGO fica encostada na borda direita (os USB-C na parede da
# direita) e deixa livre a faixa da esquerda (colunas A a D).
COL_J1 = 5                   # F
COL_J2 = 17                  # R
K_PINO1 = 42                 # linha 9
LILYGO_X = col_x(COL_J1) - L.barra_x[0]          # borda esquerda da LILYGO: 12.61
LILYGO_Y = k_y(K_PINO1) - L.pino1_y              # borda de baixo da LILYGO: 10.04

J1 = ["3V3", "GND", "IO1", "IO47", "IO14", "IO9", "IO36", "IO35", "IO0", "IO17", "IO48", "IO45",
      "IO11 MOSI", "IO12 SCK", "IO13 MISO", "VBAT"]
J2 = ["TX", "RX", "3V3", "GND", "IO2 SCL", "IO3 SDA", "IO41", "IO40", "IO39", "IO38", "IO15", "IO16",
      "IO37", "IO21", "GND", "VBUS"]


def pino_j1(n: int) -> str:
    return furo(COL_J1, K_PINO1 - (n - 1))


def pino_j2(n: int) -> str:
    return furo(COL_J2, K_PINO1 - (n - 1))


def j1_de(nome: str) -> int:
    """"IO36" → 7 (o pino da barra da esquerda)"""
    return next(i + 1 for i, s in enumerate(J1) if s.split()[0] == nome)


def j2_de(nome: str) -> int:
    return next(i + 1 for i, s in enumerate(J2) if s.split()[0] == nome)


def lilygo_para_tijolo(x: float, y: float):
    """coordenadas da LILYGO → X, Y do Tijolo"""
    return LILYGO_X + x, LILYGO_Y + y


def dentro_da_lilygo(x: float, y: float, margem: float = 0.0) -> bool:
    return (LILYGO_X - margem <= x <= LILYGO_X + L.largura + margem and
            LILYGO_Y - margem <= y <= LILYGO_Y + L.altura + margem)


def altura_embaixo(x: float, y: float, raio: float = 1.0) -> float:
    """A peça mais alta da LILYGO embaixo do ponto (x, y) da placa da frente (num quadrado de ±raio)"""
    h = 0.0
    for p in PECAS_CIMA:
        x0, y0 = lilygo_para_tijolo(p["x0"], p["y0"])
        x1, y1 = lilygo_para_tijolo(p["x1"], p["y1"])
        if x0 < x + raio and x1 > x - raio and y0 < y + raio and y1 > y - raio:
            h = max(h, p["altura"])
    return h


# ---------------------------------------------------------------------------
# Altura (Z) de cada camada
# ---------------------------------------------------------------------------
PAREDE = 2.0                 # paredes da caixa
FUNDO = 1.8                  # fundo (atrás) e frente
FOLGA_BATERIA = 0.4
Z_LILYGO = FUNDO + FOLGA_BATERIA + L.bateria_fundo          # face de baixo da LILYGO
Z_LILYGO_CIMA = Z_LILYGO + L.espessura
VAO = 2 * 2.54               # entre a LILYGO e a placa da frente: dois plásticos de barra de pinos
Z_PLACA = Z_LILYGO_CIMA + VAO                               # face de trás da placa da frente
Z_PLACA_FRENTE = Z_PLACA + PLACA_ESPESSURA

# ---------------------------------------------------------------------------
# Tela 2,4" SPI ILI9341 (LCDWiki MSP2401, sem toque, com cartão micro SD atrás)
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class Tela:
    largura: float = 42.72
    altura: float = 77.18
    pcb: float = 1.6
    vidro_alt: float = 60.26
    vidro_esp: float = 2.2 + 0.5             # vidro + fita
    area_larg: float = 36.72
    area_alt: float = 48.96
    pinos_da_borda: float = 2.0              # CONFERIR: da fileira de pinos até a borda da plaquinha
    plasticos: int = 1                       # plásticos da barra de pinos entre a tela e a placa (2 se o cartão SD for alto)
    # CONFERIR: da borda da plaquinha do lado dos pinos até o vidro, e do vidro até a imagem.
    # Sem a medida, ficam centralizados.
    vidro_da_borda_pinos: float = (77.18 - 60.26) / 2
    imagem_do_vidro: float = (60.26 - 48.96) / 2
    furos: tuple = (3.0, 4.96)               # furos Ø3,2: da borda comprida e da curta

    @property
    def espaco(self) -> float:
        return 2.54 * self.plasticos


T = Tela()
K_TELA = 50                                  # fileira dos pinos da tela: linha 1 (em cima)
TELA_X = (PLACA_LARGURA - T.largura) / 2     # a tela centralizada na placa
TELA_TOPO = k_y(K_TELA) + T.pinos_da_borda   # os pinos ficam em cima
TELA_BASE = TELA_TOPO - T.altura
Z_TELA = Z_PLACA_FRENTE + T.espaco           # face de trás da plaquinha da tela
Z_VIDRO = Z_TELA + T.pcb + T.vidro_esp       # frente do vidro
VIDRO_TOPO = TELA_TOPO - T.vidro_da_borda_pinos
VIDRO_BASE = VIDRO_TOPO - T.vidro_alt
IMAGEM_TOPO = VIDRO_TOPO - T.imagem_do_vidro
IMAGEM_BASE = IMAGEM_TOPO - T.area_alt
# A ordem dos pinos da tela, de frente, com os pinos em cima: VCC à esquerda
# (CONFERIR no verso da tela; ver o guia)
TELA_PINOS = ["VCC", "GND", "CS", "RESET", "DC", "SDI(MOSI)", "SCK", "LED", "SDO(MISO)",
              "T_CLK", "T_CS", "T_DIN", "T_DO", "T_IRQ"]
TELA_COL0 = 2                                # pino 1 na coluna C

# ---------------------------------------------------------------------------
# Teclas: chave táctil 6×6×5 mm, 4 pernas. As pernas a 6,5 mm uma da outra
# (de lado a lado do corpo) estão sempre ligadas; apertar liga as duas
# fileiras (a 4,5 mm). Na placa: 3 furos × 2 furos (7,62 × 5,08).
# ---------------------------------------------------------------------------
CHAVE_ALTURA = 5.0           # do topo da placa até o topo do botãozinho
CHAVE_CORPO = 3.5            # altura do corpo (sem o botãozinho)
TECLA_LARG, TECLA_ALT = 12.4, 7.6
COLS_TECLAS = (1, 7, 13)     # coluna da perna esquerda de cada coluna de teclas (B, H, N)
K_TECLAS = (14, 10, 6, 2)    # centro de cada fileira: 1 2 3 / 4 5 6 / 7 8 9 / * 0 #
K_BOTOES = 18                # verde e vermelho, em cima do 1 e do 3
TECLAS = [["1", "2", "3"], ["4", "5", "6"], ["7", "8", "9"], ["*", "0", "#"]]
LETRAS = {"2": "abc", "3": "def", "4": "ghi", "5": "jkl", "6": "mno", "7": "pqrs", "8": "tuv", "9": "wxyz",
          "0": "␣", "1": ".,?", "*": "Aa", "#": "⌫"}


def centro_chave(c_esq: int, k: int):
    return col_x(c_esq + 1.5), k_y(k)


# ---------------------------------------------------------------------------
# A caixa
# ---------------------------------------------------------------------------
FOLGA = 0.3
CAV_X0, CAV_X1 = -FOLGA, PLACA_LARGURA + FOLGA
CAV_Y0 = 0.0
CAV_Y1 = TELA_TOPO + 1.5                     # espaço para a lingueta de cima
Z_FRENTE_DENTRO = Z_VIDRO + 0.25             # espuma fina entre o vidro e a frente
Z_TOPO = Z_FRENTE_DENTRO + FUNDO
Z_DIVISAO = Z_PLACA_FRENTE                   # a frente encontra o fundo na face da placa

# Alto-falante 3525 em pé, no espaço livre acima da LILYGO, virado para cima (CONFERIR as medidas)
FALANTE = (35.0, 25.0, 4.6)                  # largura (X), altura (Z), espessura (Y)
FALANTE_X0 = 8.5                             # deixa as colunas A a C livres para os fios
FALANTE_Y = LILYGO_Y + L.altura + 0.9        # face de trás do falante
# Microfone 9767 (Ø9,7 × 6,7) embaixo, virado para baixo
MIC_D, MIC_L = 9.7, 6.7
MIC_X, MIC_Z = PLACA_LARGURA / 2, 13.5

# As medidas que o usuário confere antes de encomendar a caixa (o guia mostra
# um formulário com elas): (chave, o que medir, dica, o valor que a caixa usa)
def _mm(v: float, casas: int = 1) -> str:
    return f"{v:.{casas}f}".replace(".", ",")


MEDIDAS = [
    ("tela_pinos", "Tela: da fileira de pinos até a borda da plaquinha", "do lado dos pinos", _mm(T.pinos_da_borda)),
    ("tela_vidro", "Tela: da borda da plaquinha (lado dos pinos) até o começo do vidro", "", _mm(T.vidro_da_borda_pinos, 2)),
    ("tela_imagem", "Tela: do começo do vidro até o começo da imagem", "com a tela acesa, mostrando branco",
     _mm(T.imagem_do_vidro, 2)),
    ("tela_sd", "Tela: a peça mais alta atrás", "normalmente o cartão micro SD; e diga de que lado fica", "1,8"),
    ("falante", "Alto-falante: largura × altura × espessura", "",
     f"{_mm(FALANTE[0], 0)} × {_mm(FALANTE[1], 0)} × {_mm(FALANTE[2])}"),
    ("mic", "Microfone: diâmetro × altura", "", f"{_mm(MIC_D)} × {_mm(MIC_L)}"),
    ("pilha", "LILYGO com a pilha: da face de baixo da placa até o ponto mais baixo da pilha", "", _mm(L.bateria_fundo)),
    ("chave", "Chave táctil: do fundo do corpo ao topo do botãozinho", "", _mm(CHAVE_ALTURA)),
]


def resumo() -> str:
    linhas = [
        f"placa da frente: {PLACA_LARGURA:.2f} × {PLACA_ALTURA:.2f} mm ({COLUNAS} × {LINHAS} furos), Y {PLACA_Y0:.2f}..{PLACA_Y0 + PLACA_ALTURA:.2f}",
        f"LILYGO: X {LILYGO_X:.2f}..{LILYGO_X + L.largura:.2f}, Y {LILYGO_Y:.2f}..{LILYGO_Y + L.altura:.2f}",
        f"barras: J1 em {pino_j1(1)}..{pino_j1(16)}, J2 em {pino_j2(1)}..{pino_j2(16)}",
        f"tela: X {TELA_X:.2f}..{TELA_X + T.largura:.2f}, Y {TELA_BASE:.2f}..{TELA_TOPO:.2f}, pinos na linha {LINHAS - K_TELA}",
        f"Z: LILYGO {Z_LILYGO:.2f}, placa {Z_PLACA:.2f}..{Z_PLACA_FRENTE:.2f}, tela {Z_TELA:.2f}, vidro {Z_VIDRO:.2f}, topo {Z_TOPO:.2f}",
        f"caixa por fora: {CAV_X1 - CAV_X0 + 2 * PAREDE:.1f} × {CAV_Y1 - CAV_Y0 + 2 * PAREDE:.1f} × {Z_TOPO:.1f} mm",
    ]
    return "\n".join(linhas)


if __name__ == "__main__":
    print(resumo())
