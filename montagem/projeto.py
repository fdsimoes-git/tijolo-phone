"""
O Tijolo, montagem com a PCB da frente: todas as medidas num lugar só.

Como fica (de trás para a frente):
  - a LILYGO de costas: os componentes para o fundo da caixa, a pilha e os
    pinos (que já estão soldados do lado da pilha) para a frente. A ponta da
    ESP32 fica para baixo e os USB-C saem pela parede da direita;
  - a PCB da frente, encomendada: encaixa nos pinos da LILYGO com duas barras
    fêmea de perna longa e passa por cima da pilha. Ela leva a tela (soldada
    pelos próprios pinos), os botões verde e vermelho e os 7 furos onde se
    liga a aba do teclado;
  - o teclado 3×4 do Felipe, INTEIRO (não precisa abrir), colado na PCB com
    fita dupla face de espuma;
  - a tela com os pinos para baixo, que é a posição normal dela.

Coordenadas, em mm, olhando o Tijolo de frente (a tela em cima):
  X: da esquerda para a direita, a partir da parede da esquerda (por dentro)
  Y: de baixo para cima, a partir da parede de baixo (por dentro)
  Z: de trás para a frente, a partir da face de fora do fundo da caixa

De onde vêm as medidas:
  - LILYGO: modelo 3D oficial (lilygo_pecas.json) e o desenho com cotas (DXF)
    do repositório da LILYGO: placa de 33,1 × 110,6 mm, barras a 30,45 mm uma
    da outra, 4 furos de Ø2,55;
  - tela: desenho do fabricante, MSP2401_Size.pdf da LCDWiki (V1.0, 2024-04-15);
  - teclado: foto com cotas do anúncio (51,4 × 69,3 mm) e ficha da Adafruit
    3845, que é o mesmo teclado (70,0 × 51,0 × 9,7 mm);
  - alto-falante e microfone: os nomes das peças (3525 e 9767).
As que só dá para confirmar com a peça na mão estão em MEDIDAS, no fim. O
Felipe mediu as peças dele em 24/11/2026: o teclado (51,2 × 63,7 × 10,2), a
pilha na LILYGO (20,0), a chave táctil (4,8), a perna da barra fêmea (10,9) e
a largura do alto-falante (35,1). Esses valores já estão aqui.
"""
import json
import math
import os
from dataclasses import dataclass


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
    barra_x: tuple = (1.359, 31.808)        # J1 (lado da chave) e J2 (lado dos USB)
    pino1_y: float = 103.308                # pino 1 das duas barras (ponta da ESP32)
    furos_fixacao: tuple = ((2.591, 2.316), (30.353, 2.316), (2.609, 107.878), (30.319, 107.868))
    furo_fixacao_d: float = 2.55            # o desenho oficial (DXF) dá Ø2,55, com um anel livre de Ø5,5
    # a chave liga/desliga, na borda do lado da J1: corpo y0..y1, botão y0..y1, quanto sai da borda, z do botão
    chave: tuple = (54.66, 63.66, 59.24, 60.54, 1.85, 1.15, 2.55)
    chave_corpo_x: tuple = (-1.9, 3.6)
    chave_altura: float = 3.65
    sim: tuple = (0.75, 34.44, 13.06, 45.81)                # o chip entra pela borda do lado da J1
    usb_esp_y: float = 58.30
    usb_modem_y: float = 46.62
    usb_largura: float = 8.94
    usb_altura: float = 3.21
    usb_sai: float = 0.84                    # o USB passa da borda da placa
    qwiic: tuple = ((20.22, 26.52), (27.71, 34.01))   # na borda dos USB, do lado da pilha; saem 0,76
    qwiic_sai: float = 0.76
    ufl_lte: tuple = (16.40, 1.82)
    antena_esp: tuple = (7.49, 104.0, 25.51, 110.61)  # a antena da ESP32 (ponta do módulo)
    # lado da pilha
    suporte_18650: tuple = (6.49, 13.66, 27.15, 99.68)
    suporte_altura: float = 15.01
    pilha_y: tuple = (24.2, 89.2)            # a pilha de 65 mm, centralizada no suporte
    pilha_x: float = 16.81                   # o eixo da pilha
    pilha_raio: float = 9.3
    # Da face da placa até o ponto mais alto da pilha: 20,0, MEDIDO na placa do
    # Felipe. Pelo modelo 3D eu esperava de 18,1 a 18,9 e tinha reservado 19,6:
    # a medida de verdade deu mais, e é ela que vale.
    pilha_altura: float = 20.0
    spek: tuple = (1.32, 10.49, 5.67, 14.24)     # conector SPK (SH 1.0, de pé, 4,47 de altura)
    mic: tuple = (1.41, 5.33, 5.76, 9.08)        # conector MIC
    barra_plastico: float = 2.54             # o plástico da barra, do lado da pilha
    barra_pino: float = 5.84                 # o pino, depois do plástico (barra comum de 2,54 mm)


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
# Tela 2,4" SPI ILI9341 sem toque (LCDWiki MSP2401). Coordenadas próprias:
# de frente, com os pinos embaixo (a posição normal: a imagem fica em pé com
# a rotação 0); u da borda esquerda, v da borda de baixo (a dos pinos).
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class Tela:
    largura: float = 42.72
    altura: float = 77.18
    pcb: float = 1.6
    lcd: float = 2.2 + 0.5                   # o vidro com a luz (2,2) e a fita de trás (0,5)
    pino1_u: float = 4.85                    # pino 1 (VCC), à esquerda
    pinos_v: float = 2.00
    furos: tuple = ((3.00, 6.92), (39.72, 6.92), (3.00, 74.18), (39.72, 74.18))   # Ø3,2
    vidro_v: tuple = (10.41, 70.67)          # o vidro tem a largura toda da plaquinha
    imagem_u: tuple = (3.00, 39.72)
    imagem_v: tuple = (18.96, 67.92)
    j4_v: float = 75.01                      # os 4 furos do cartão SD (sem pinos), em cima
    # atrás: o soquete do cartão SD (o grande), encostado na borda esquerda
    sd_u: tuple = (0.8, 20.4)
    sd_v: tuple = (33.0, 62.6)
    sd_altura: float = 3.0
    # atrás: regulador, transistor, resistores (o mais alto tem 1,1)
    pecas_u: tuple = (18.0, 39.0)
    pecas_v: tuple = (13.0, 50.5)
    pecas_altura: float = 1.3
    barra_plastico: float = 2.54             # o plástico amarelo da barra, atrás
    barra_pino: float = 5.84                 # o pino, depois do plástico
    solda_frente: float = 1.4                # as soldas da barra, na frente da plaquinha


T = Tela()
TELA_PINOS = ["VCC", "GND", "CS", "RESET", "DC", "SDI(MOSI)", "SCK", "LED", "SDO(MISO)",
              "T_CLK", "T_CS", "T_DIN", "T_DO", "T_IRQ"]
TELA_GPIO = {"VCC": "3V3", "GND": "GND", "CS": "IO47", "RESET": "IO9", "DC": "IO14", "SDI(MOSI)": "IO11",
             "SCK": "IO12", "LED": "IO36", "SDO(MISO)": "IO13"}
# Os pinos do toque não são usados (a tela não tem toque). Os quatro que caem
# em cima da pilha são cortados rente ao plástico; o T_IRQ fica, só para firmar.
TELA_PINOS_CORTADOS = (10, 11, 12, 13)


# ---------------------------------------------------------------------------
# Teclado 3×4 de teclas de plástico (o do anúncio; o mesmo da Adafruit 3845).
# Coordenadas próprias: de frente, a aba dos contatos embaixo.
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class Teclado:
    largura: float = 51.2                    # o corpo preto: MEDIDO (o anúncio diz 51,4; a Adafruit, 51,0)
    altura: float = 63.7                     # o corpo preto, sem a aba: MEDIDO (a foto do anúncio dava 64,2)
    aba: tuple = (36.2, 5.3)                 # a aba verde dos contatos: largura e quanto sai do corpo
    furos_aba: int = 9                       # 9 furos a 2,54 mm; os 7 do meio são os do teclado
    furos_aba_do_corpo: float = 3.2          # da borda do corpo até a fileira de furos
    espessura: float = 10.2                  # de trás até o topo das teclas: MEDIDO (a Adafruit diz 9,7)
    base: float = 3.5                        # de trás até o topo da aba de fora (onde ficam os 4 furos)
    plato: float = 5.5                       # de trás até o topo da parte crespa
    plato_margem: tuple = (2.0, 3.25)        # da borda do corpo até a parte crespa: dos lados, em cima e embaixo
    passo: tuple = (14.25, 13.38)            # entre as teclas: de lado e de cima para baixo
    tecla: float = 7.8                       # o topo da tecla
    furos: tuple = (46.4, 59.7)              # os 4 furos dos cantos (não são usados)


K = Teclado()
TECLAS = [["1", "2", "3"], ["4", "5", "6"], ["7", "8", "9"], ["*", "0", "#"]]
# Os 9 furos da aba, da esquerda para a direita (de frente). O firmware aprende
# qual fio é qual ("teclado aprender"), então a ordem não importa. Os três pinos
# que acordam a ESP32 (IO15, IO16, IO17) ficam nos furos da tecla 3.
TECLADO_ABA = [None, "IO37", "IO15", "IO17", "IO40", "IO16", "IO39", "IO38", None]
GPIO_VERDE, GPIO_VERMELHO = "IO35", "IO21"

# ---------------------------------------------------------------------------
# Espessuras (Z), de trás para a frente
# ---------------------------------------------------------------------------
FUNDO = 1.5                  # a parede de trás
FRENTE = 1.5                 # a parede da frente
PAREDE = 1.8                 # as paredes dos lados
VAO_COMP = 3.5               # do fundo até os componentes da LILYGO (o mais alto, o USB-C, tem 3,21)
FOLGA_PILHA = 0.6            # entre a pilha e a PCB
PCB_ESP = 0.8                # a PCB da frente
ARO = 0.8                    # a aba da frente que cobre a borda do vidro
FOLGA_VIDRO = 0.2

Z_LILYGO_COMP = FUNDO + VAO_COMP                       # face dos componentes da LILYGO (virada para trás)
Z_LILYGO_PILHA = Z_LILYGO_COMP + L.espessura           # face do lado da pilha (virada para a frente)
Z_PILHA = Z_LILYGO_PILHA + L.pilha_altura              # o ponto mais alto da pilha
Z_PCB = Z_PILHA + FOLGA_PILHA                          # face de trás da PCB
Z_PCB_FRENTE = Z_PCB + PCB_ESP
Z_TELA = Z_PCB_FRENTE + T.barra_plastico               # face de trás da plaquinha da tela (apoiada no plástico da barra)
Z_TELA_FRENTE = Z_TELA + T.pcb
Z_VIDRO = Z_TELA_FRENTE + T.lcd                        # a frente do vidro
Z_TOPO = Z_VIDRO + FOLGA_VIDRO + ARO                   # a face da frente da caixa
Z_FRENTE_DENTRO = Z_TOPO - FRENTE

# A barra fêmea de perna longa ("empilhável"): encaixa nos pinos da LILYGO,
# assenta no plástico deles, e as pernas sobem até a PCB
FEMEA_ALTURA = 8.5
Z_FEMEA0 = Z_LILYGO_PILHA + L.barra_plastico
Z_FEMEA1 = Z_FEMEA0 + FEMEA_ALTURA
FEMEA_PERNA_MIN = Z_PCB_FRENTE - Z_FEMEA1 + 0.5        # a perna precisa passar a PCB e sobrar para a solda
FEMEA_PERNA = 10.9                                     # as barras do Felipe: MEDIDO
FEMEA_SOBRA = FEMEA_PERNA - (Z_PCB_FRENTE - Z_FEMEA1)  # quanto a perna passa da face da frente da PCB
assert FEMEA_PERNA >= FEMEA_PERNA_MIN - 0.005, (FEMEA_PERNA, FEMEA_PERNA_MIN)

# O teclado fica colado na PCB com fita dupla face de espuma (1 a 1,5 mm), que
# também o separa das soldas das barras, que ficam embaixo dele
FITA_ESPUMA = 1.5
Z_TECLADO = Z_PCB_FRENTE + FITA_ESPUMA                 # as costas do teclado
Z_TECLAS = Z_TECLADO + K.espessura

# ---------------------------------------------------------------------------
# Em X e Y (de frente)
# ---------------------------------------------------------------------------
CAV_X0, CAV_Y0 = 0.0, 0.0
FOLGA_TECLADO = 0.6          # de cada lado do teclado, na abertura da frente
CAV_X1 = round(K.largura + 2 * FOLGA_TECLADO, 3)       # a largura de dentro: a do teclado (52,4)
MEIO_X = (CAV_X0 + CAV_X1) / 2
FOLGA = 0.3

# O teclado, embaixo. A aba dele fica escondida pelo queixo da frente. Quem fica
# fixa é a borda de CIMA do corpo: é por ela que se acerta o teclado na hora de
# colar (tem uma linha na PCB), os botões e a tela contam a partir dela e as
# soldas das barras da LILYGO (a última em Y 69,3) têm de ficar embaixo dele.
TECLADO_X0 = MEIO_X - K.largura / 2
TECLADO_Y1 = 70.2                                      # a borda de cima do corpo
TECLADO_Y0 = round(TECLADO_Y1 - K.altura, 3)           # a borda de baixo do corpo
ABA_Y = TECLADO_Y0 - K.furos_aba_do_corpo              # a fileira de furos da aba
assert TECLADO_Y0 - K.aba[1] >= 0.7                    # a aba não encosta na parede de baixo
JANELA_TECLADO = (CAV_X0, TECLADO_Y0 - 0.8, CAV_X1, TECLADO_Y1 + 1.0)    # a abertura da frente

# Os botões verde e vermelho (chaves tácteis 6×6), entre o teclado e a tela.
# Ficam perto das bordas: no meio as pernas deles cairiam em cima da pilha.
BOTAO = 6.0
CHAVE_TACTIL_ALTURA = 4.8                              # da PCB até o topo do botãozinho: MEDIDO (a chave "de 5 mm")
# O botão impresso que vai em cima de cada chave: a base dele tem um recesso onde entra o botãozinho,
# e uma aba em volta, que fica presa atrás da frente da caixa
BOTAO_RECESSO = 0.55
BOTAO_JOGO = 0.3                                       # solto, o botão sobe isto até a aba encostar na frente
BOTAO_ABA = round(Z_FRENTE_DENTRO - (Z_PCB_FRENTE + CHAVE_TACTIL_ALTURA - BOTAO_RECESSO) - BOTAO_JOGO, 2)
BOTOES_Y = TECLADO_Y1 + 5.4
BOTOES_X = (7.0, CAV_X1 - 7.0)                         # verde, vermelho

# A tela, em cima, com os pinos para baixo
TELA_X = MEIO_X - T.largura / 2
TELA_Y0 = BOTOES_Y + BOTAO / 2 + 0.4                   # a borda de baixo da plaquinha
TELA_Y1 = TELA_Y0 + T.altura
TELA_PINOS_Y = TELA_Y0 + T.pinos_v
VIDRO_Y = (TELA_Y0 + T.vidro_v[0], TELA_Y0 + T.vidro_v[1])
IMAGEM_X = (TELA_X + T.imagem_u[0], TELA_X + T.imagem_u[1])
IMAGEM_Y = (TELA_Y0 + T.imagem_v[0], TELA_Y0 + T.imagem_v[1])
CAV_Y1 = TELA_Y1 + 2.0                                 # em cima da tela descem as duas linguetas da frente

# A LILYGO de costas, encostada na parede da direita (por onde saem os USB-C),
# com a ponta da ESP32 para baixo. As barras dela ficam embaixo do teclado e
# acabam logo abaixo dos botões.
LILYGO_X = CAV_X1 - 0.34 - L.largura                   # a borda da esquerda (de frente)
LILYGO_Y = 24.0                                        # a ponta de baixo (a da ESP32)

# A PCB da frente: do queixo até logo acima dos pinos da tela, com uma língua
# embaixo, que passa por baixo da aba do teclado. Quem põe a PCB no lugar são as
# barras da LILYGO, então ela fica a 0,6 mm das paredes (a da direita tem um
# rebaixo de 0,3 mm: a fileira J2 fica perto demais da borda para estreitar a PCB).
FOLGA_PCB = 0.6
REBAIXO_DIREITA = 0.3
PCB_X0, PCB_X1 = CAV_X0 + FOLGA_PCB, CAV_X1 + REBAIXO_DIREITA - FOLGA_PCB
PCB_Y0 = 5.8                                           # embaixo, nos cantos, ficam os pilares dos parafusos
PCB_Y1 = TELA_PINOS_Y + 2.4
LINGUA = (9.5, 0.7, CAV_X1 - 9.5, PCB_Y0)              # x0, y0, x1, y1

# Alto-falante 3525 em pé no topo, virado para a grade da parede de cima,
# embaixo da ponta da tela. A calha aceita até 6 mm de espessura. A largura
# (35,1) foi MEDIDA; a altura e a espessura ainda vêm do nome da peça.
FALANTE = (35.1, 25.0, 6.0)                  # largura (X), altura (Z), espessura máxima (Y)
FALANTE_X0 = MEIO_X - FALANTE[0] / 2
FALANTE_AR = 2.0                             # na frente do cone, até a parede (ali descem as linguetas da frente)
FALANTE_Y = CAV_Y1 - FALANTE_AR - FALANTE[2] - 0.2     # o fundo da calha
# Microfone 9767 (Ø9,7 × 6,7) embaixo, deitado, virado para a parede de baixo
MIC_D, MIC_L = 9.7, 6.7
MIC_X, MIC_Z = MEIO_X, 13.0


def lilygo_para_tijolo(x: float, y: float):
    """coordenadas da LILYGO (componentes para você, ESP32 em cima) → X, Y do Tijolo
    (ela de costas e com a ESP32 para baixo: vira em torno do eixo X)"""
    return LILYGO_X + x, LILYGO_Y + (L.altura - y)


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


def pino_tela(i: int):
    """(X, Y) do pino i (1..14) da tela: o 1 (VCC) à esquerda"""
    return TELA_X + T.pino1_u + (i - 1) * 2.54, TELA_PINOS_Y


def furo_aba(k: int):
    """(X, Y) do furo k (1..9) da aba do teclado, da esquerda para a direita"""
    return MEIO_X + (k - (K.furos_aba + 1) / 2) * 2.54, ABA_Y


def tela_para_tijolo(u: float, v: float):
    return TELA_X + u, TELA_Y0 + v


# A pilha, de frente
PILHA_X = lilygo_para_tijolo(L.pilha_x, 0)[0]
PILHA_Y = (LILYGO_Y + L.altura - L.pilha_y[1], LILYGO_Y + L.altura - L.pilha_y[0])
Z_PILHA_EIXO = Z_PILHA - L.pilha_raio


def folga_sobre_a_pilha(x: float, y: float):
    """Do ponto (x, y) na face de trás da PCB até a pilha embaixo dele, em mm
    (None se ali não tem pilha). Serve para não deixar solda em cima da pilha."""
    if not (PILHA_Y[0] - 1.0 <= y <= PILHA_Y[1] + 1.0):
        return None
    d = abs(x - PILHA_X)
    if d >= L.pilha_raio:
        return None
    return Z_PCB - (Z_PILHA_EIXO + math.sqrt(L.pilha_raio ** 2 - d ** 2))


SOLDA_ATRAS = 1.6            # quanto uma perna soldada e cortada pode passar atrás da PCB
FOLGA_SOLDA_PILHA = 1.0      # e quanto tem de sobrar até a pilha

# ---------------------------------------------------------------------------
# As medidas que só dá para confirmar com a peça na mão
# (chave, o que medir, o valor do projeto, o que serve, de onde veio o valor).
# As faixas saem das folgas de cima: quem montar com peças de outra medida
# troca o valor lá em cima e gera tudo de novo.
# ---------------------------------------------------------------------------
MEDIDO_EM = "24/11/2026"
FALANTE_ALTURA_MAX = math.floor(Z_TELA - FUNDO - 0.5)  # em pé na calha, até quase encostar atrás da tela
PERNA_MIN_TXT = _mm(math.ceil(FEMEA_PERNA_MIN * 10 - 0.001) / 10)
MEDIDAS = [
    ("teclado_largura", "Teclado: largura do corpo preto", f"{_mm(K.largura)} mm",
     f"de {_mm(CAV_X1 - 1.6)} a {_mm(CAV_X1 - 0.4)}", f"medido em {MEDIDO_EM}"),
    ("teclado_altura", "Teclado: comprimento do corpo preto, sem a aba verde", f"{_mm(K.altura)} mm",
     f"de {_mm(K.altura - 0.5)} a {_mm(K.altura + 0.5)}", f"medido em {MEDIDO_EM}"),
    ("teclado_espessura", "Teclado: espessura, de trás até o topo das teclas", f"{_mm(K.espessura)} mm", "até 11",
     f"medido em {MEDIDO_EM}"),
    ("pilha", "LILYGO com a pilha: da face da placa (lado da pilha) até o ponto mais alto da pilha",
     f"{_mm(L.pilha_altura)} mm", f"até {_mm(L.pilha_altura + FOLGA_PILHA / 2)}", f"medido em {MEDIDO_EM}"),
    ("tela_vcc", "Tela: de frente, com os pinos para baixo, o VCC é o pino da esquerda", "sim", "tem de ser sim",
     f"desenho da LCDWiki; conferido em {MEDIDO_EM}"),
    ("falante", "Alto-falante: largura × altura × espessura",
     f"{_mm(FALANTE[0])} × {_mm(FALANTE[1], 0)} × até {_mm(FALANTE[2], 0)} mm",
     f"largura até {_mm(FALANTE[0] + 0.2)}; altura até {FALANTE_ALTURA_MAX}; espessura até {_mm(FALANTE[2], 0)}",
     f"largura medida em {MEDIDO_EM}; altura e espessura pelo nome da peça (3525): FALTA MEDIR"),
    ("chave", "Chave táctil (os botões verde e vermelho): altura, da base até o topo do botãozinho",
     f"{_mm(CHAVE_TACTIL_ALTURA)} mm", f"de {_mm(CHAVE_TACTIL_ALTURA - 0.3)} a {_mm(CHAVE_TACTIL_ALTURA + BOTAO_JOGO - 0.1)}",
     f"medido em {MEDIDO_EM}"),
    ("perna", "Barra fêmea de perna longa: comprimento da perna", f"{_mm(FEMEA_PERNA)} mm",
     f"{PERNA_MIN_TXT} ou mais", f"medido em {MEDIDO_EM}"),
]
FALTA_MEDIR = ("falante",)                   # das de cima, as que ainda não foram conferidas por inteiro


def resumo() -> str:
    j1a, j1b = pino_barra("J1", 1), pino_barra("J1", 16)
    j2a = pino_barra("J2", 1)
    lx0, ly0, lx1, ly1 = caixa_lilygo(0, 0, L.largura, L.altura)
    linhas = [
        f"LILYGO (de costas, ESP32 para baixo): X {lx0:.2f}..{lx1:.2f}, Y {ly0:.2f}..{ly1:.2f}",
        f"barras: J1 em X {j1a[0]:.2f}, J2 em X {j2a[0]:.2f}, Y {j1a[1]:.2f}..{j1b[1]:.2f}",
        f"pilha: eixo em X {PILHA_X:.2f}, Y {PILHA_Y[0]:.2f}..{PILHA_Y[1]:.2f}, topo em Z {Z_PILHA:.2f}",
        f"PCB: X {PCB_X0:.2f}..{PCB_X1:.2f}, Y {LINGUA[1]:.2f}..{PCB_Y1:.2f} "
        f"({PCB_X1 - PCB_X0:.1f} × {PCB_Y1 - LINGUA[1]:.1f} mm, {PCB_ESP} mm)",
        f"teclado: X {TECLADO_X0:.2f}..{TECLADO_X0 + K.largura:.2f}, Y {TECLADO_Y0:.2f}..{TECLADO_Y1:.2f}, "
        f"furos da aba em Y {ABA_Y:.2f}",
        f"botões: Y {BOTOES_Y:.2f}, X {BOTOES_X[0]:.2f} e {BOTOES_X[1]:.2f}",
        f"tela: X {TELA_X:.2f}..{TELA_X + T.largura:.2f}, Y {TELA_Y0:.2f}..{TELA_Y1:.2f}, pinos em Y {TELA_PINOS_Y:.2f}, "
        f"imagem Y {IMAGEM_Y[0]:.2f}..{IMAGEM_Y[1]:.2f}",
        f"Z: LILYGO {Z_LILYGO_COMP:.2f}..{Z_LILYGO_PILHA:.2f}, pilha até {Z_PILHA:.2f}, PCB {Z_PCB:.2f}..{Z_PCB_FRENTE:.2f}, "
        f"tela {Z_TELA:.2f}, vidro {Z_VIDRO:.2f}, frente {Z_TOPO:.2f}, teclas {Z_TECLAS:.2f}",
        f"barra fêmea: {Z_FEMEA0:.2f}..{Z_FEMEA1:.2f}, perna de pelo menos {FEMEA_PERNA_MIN:.1f} mm",
        f"caixa por fora: {CAV_X1 - CAV_X0 + 2 * PAREDE:.1f} × {CAV_Y1 - CAV_Y0 + 2 * PAREDE:.1f} × {Z_TOPO:.1f} mm",
    ]
    return "\n".join(linhas)


if __name__ == "__main__":
    print(resumo())
