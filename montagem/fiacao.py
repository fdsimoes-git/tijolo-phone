"""
O que vai em cada furo da placa da frente e cada ligação ("F12": coluna F,
linha 12, contando de cima).

Na FRENTE da placa não corre fio: só as chaves, a tela e pontes de solda
entre furos vizinhos (um fio que vem de trás atravessa o furo e a ponte o
liga ao pino da LILYGO ao lado).

ATRÁS:
  - antes de juntar com a LILYGO: a matriz do teclado (pontes e fios nus
    curtos) e os 7 fios que sobem pela coluna O até a barra da direita (eles
    atravessam os furos Q16..Q22 e ficam com uma ponta para fora, na frente);
  - depois de juntar: os fios da faixa da esquerda (colunas A a D), que fica
    fora da LILYGO: a tela, o botão verde, o GND dos botões e a coluna 1 4 7 *.
"""
import os
from dataclasses import dataclass, field
from typing import List, Tuple

import projeto as P

# A tela vista de frente, com os pinos em cima: o VCC fica à esquerda (o
# normal) ou à direita (TELA_ESPELHADA=1). Os fios são os mesmos; só mudam as
# colunas de onde saem.
TELA_ESPELHADA = os.environ.get("TELA_ESPELHADA") == "1"

# Os pinos da ESP32 nesta montagem (o config.h tem os mesmos, com MONTAGEM_PLACA = 1)
GPIO_TECLADO = {           # linha da matriz → pino
    "fileira 1 2 3": "IO15",
    "fileira 4 5 6": "IO38",
    "fileira 7 8 9": "IO39",
    "fileira * 0 #": "IO40",
    "coluna 1 4 7 *": "IO17",
    "coluna 2 5 8 0": "IO37",
    "coluna 3 6 9 #": "IO16",
}
GPIO_VERDE = "IO35"
GPIO_VERMELHO = "IO21"


@dataclass
class Chave:
    rotulo: str
    c: int            # coluna da perna esquerda
    k: int            # fileira do centro
    cor: str = "tecla"

    def pernas(self):
        """(de cima: esquerda, direita), (de baixo: esquerda, direita). As de cima estão sempre
        ligadas entre si, idem as de baixo; apertar liga as de cima às de baixo."""
        cima = (P.furo(self.c, self.k + 1), P.furo(self.c + 3, self.k + 1))
        baixo = (P.furo(self.c, self.k - 1), P.furo(self.c + 3, self.k - 1))
        return cima, baixo


CHAVES: List[Chave] = []
for i, k in enumerate(P.K_TECLAS):
    for j, c in enumerate(P.COLS_TECLAS):
        CHAVES.append(Chave(P.TECLAS[i][j], c, k))
CHAVES.append(Chave("verde", P.COLS_TECLAS[0], P.K_BOTOES, "verde"))
CHAVES.append(Chave("vermelho", P.COLS_TECLAS[2], P.K_BOTOES, "vermelho"))


def chave(rotulo: str) -> Chave:
    return next(c for c in CHAVES if c.rotulo == rotulo)


# ---------------------------------------------------------------------------
# As ligações
# ---------------------------------------------------------------------------
@dataclass
class Ligacao:
    sinal: str
    caminho: List[str]        # furos por onde passa (o primeiro e o último são as pontas)
    lado: str                 # "tras" ou "frente"
    tipo: str                 # "ponte" (solda entre vizinhos), "nu" (fio nu curto), "fio" (encapado)
    etapa: str                # "matriz", "direita", "esquerda", "pontes", "tela"
    nota: str = ""
    cor: str = ""             # a cor sugerida do fio encapado

    @property
    def de(self):
        return self.caminho[0]

    @property
    def ate(self):
        return self.caminho[-1]


LIG: List[Ligacao] = []


def lig(sinal, caminho, lado, tipo, etapa, nota="", cor=""):
    LIG.append(Ligacao(sinal, list(caminho), lado, tipo, etapa, nota, cor))


R = P.LINHAS                               # para escrever as linhas de cima para baixo


def fila_cima(k):                           # a fileira das pernas de cima de uma tecla
    return R - (k + 1)


def fila_baixo(k):
    return R - (k - 1)


FILEIRAS = ["1 2 3", "4 5 6", "7 8 9", "* 0 #"]

# ---------- 1. a matriz, atrás, antes de juntar ----------
for i, k in enumerate(P.K_TECLAS):
    r = fila_cima(k)
    s = f"fileira {FILEIRAS[i]}"
    lig(s, [f"E{r}", f"F{r}", f"G{r}", f"H{r}"], "tras", "nu", "matriz", "liga a tecla da esquerda à do meio")
    lig(s, [f"K{r}", f"L{r}", f"M{r}", f"N{r}"], "tras", "nu", "matriz", "liga a tecla do meio à da direita")

COLUNAS_M = [("coluna 1 4 7 *", "B", "C"), ("coluna 2 5 8 0", "H", "I"), ("coluna 3 6 9 #", "N", "O")]
BAIXO = [fila_baixo(k) for k in P.K_TECLAS]            # 38, 42, 46, 50
for s, perna, bus in COLUNAS_M:
    lig(s, [f"{bus}{r}" for r in range(BAIXO[0], BAIXO[-1] + 1)], "tras", "nu", "matriz",
        "fio nu descendo pela coluna; solde nos furos das 4 pontes")
    for r in BAIXO:
        lig(s, [f"{perna}{r}", f"{bus}{r}"], "tras", "ponte", "matriz")

# o GND dos botões: as pernas de cima do verde e do vermelho
rb_cima = fila_cima(P.K_BOTOES)            # 32
rb_baixo = fila_baixo(P.K_BOTOES)          # 34
lig("GND dos botões", [f"E{rb_cima}"] + [f"{P.letra(c)}{rb_cima}" for c in range(5, 13)] + [f"N{rb_cima}"],
    "tras", "nu", "matriz", "liga as pernas de cima do verde e do vermelho")

# ---------- 2. os 7 fios da direita, atrás, antes de juntar ----------
# Sobem juntos pela coluna O (o "vale" da LILYGO: ali as peças embaixo são
# baixas) e viram para a direita na linha do pino: atravessam Q16..Q22 e a
# ponta fica para fora, na frente, para a ponte com o pino da barra R.
DESTINO = {}               # sinal → linha na barra da direita
for s, g in GPIO_TECLADO.items():
    if g != "IO17":
        DESTINO[s] = P.LINHAS - (P.K_PINO1 - (P.j2_de(g) - 1))
DESTINO["botão vermelho"] = P.LINHAS - (P.K_PINO1 - (P.j2_de(GPIO_VERMELHO) - 1))

CORES = {"fileira 1 2 3": "laranja", "fileira 4 5 6": "amarelo", "fileira 7 8 9": "verde",
         "fileira * 0 #": "azul", "coluna 2 5 8 0": "roxo", "coluna 3 6 9 #": "cinza",
         "botão vermelho": "vermelho"}


def subida(saida: List[str], s: str, nota=""):
    n = DESTINO[s]
    i0 = int(saida[-1][1:])
    caminho = list(saida) + [f"O{r}" for r in range(i0 - 1, n - 1, -1)] + [f"P{n}", f"Q{n}"]
    lig(s, caminho, "tras", "fio", "direita", nota or f"atravessa Q{n} e fica com 1 cm para fora, na frente",
        CORES[s])


# a coluna 3 6 9 # já está na coluna O: sobe direto do alto do fio nu
subida(["O38"], "coluna 3 6 9 #")
# a coluna 2 5 8 0 sai do alto do fio nu da coluna I e vai pela linha 37 até a O
subida(["I38", "I37"] + [f"{P.letra(c)}37" for c in range(9, 15)], "coluna 2 5 8 0")
# as fileiras saem da perna de cima à direita da tecla da direita, vão para a
# coluna R, sobem até a linha 35 e entram na O
for i, k in enumerate(P.K_TECLAS):
    r = fila_cima(k)
    s = f"fileira {FILEIRAS[i]}"
    subida([f"Q{r}", f"R{r}"] + [f"R{x}" for x in range(r - 1, 34, -1)] + ["Q35", "P35", "O35"], s)
# o botão vermelho: da perna de baixo à direita, pela linha 34 até a O
subida([f"Q{rb_baixo}", f"P{rb_baixo}", f"O{rb_baixo}"], "botão vermelho")

# ---------- 3. as pontes da frente, depois de juntar ----------
for s, n in DESTINO.items():
    lig(s, [f"Q{n}", f"R{n}"], "frente", "ponte", "pontes", "inclui a ponta do fio que atravessa o Q")

# ---------- 4. a faixa da esquerda, atrás, depois de juntar ----------
# Os fios entram na frente pelo furo E (ou D) e uma ponte de solda os liga ao
# pino da barra F ao lado. A outra ponta: na tela (pino da fileira 1) ou nas
# pernas dos botões e no fio nu da coluna 1 4 7 *.
TELA_LIGA = [   # pino da tela (1..14), sinal, pino da LILYGO (barra esquerda), cor
    (1, "VCC", "3V3", "vermelho"), (2, "GND", "GND", "preto"), (3, "CS", "IO47", "laranja"),
    (4, "RESET", "IO9", "amarelo"), (5, "DC", "IO14", "verde"), (6, "SDI (MOSI)", "IO11", "azul"),
    (7, "SCK", "IO12", "roxo"), (8, "LED", "IO36", "branco"), (9, "SDO (MISO)", "IO13", "cinza"),
]


def col_tela(pino: int) -> str:
    """pino 1..14 → letra da coluna (de frente; o VCC à esquerda, ou à direita se espelhada)"""
    if TELA_ESPELHADA:
        return P.letra(P.TELA_COL0 + 13 - (pino - 1))
    return P.letra(P.TELA_COL0 + pino - 1)


def linha_j1(nome: str) -> int:
    return P.LINHAS - (P.K_PINO1 - (P.j1_de(nome) - 1))


# As "pistas" (a distância da borda esquerda) de cada fio na faixa da
# esquerda: o maço desce pelas colunas A a C, e cada fio sai na sua linha.
for pino, sinal, gpio, cor in TELA_LIGA:
    n = linha_j1(gpio)
    c = col_tela(pino)
    lig(f"tela {sinal}", [f"{c}1", f"{c}2", f"B2", f"B{n}", f"C{n}", f"D{n}", f"E{n}"], "tras", "fio", "esquerda",
        f"de baixo do pino {sinal} da tela até o furo E{n}; na frente, ponte E{n}–F{n} ({gpio})", cor)

n_verde = linha_j1(GPIO_VERDE)
n_col1 = linha_j1("IO17")
n_gnd = linha_j1("GND")
lig("botão verde", [f"B{rb_baixo}", f"A{rb_baixo}", f"A{n_verde}", f"B{n_verde}", f"C{n_verde}", f"D{n_verde}",
                    f"E{n_verde}"], "tras", "fio", "esquerda", f"da perna de baixo do verde até E{n_verde} ({GPIO_VERDE})",
    "verde")
lig("GND dos botões", [f"B{rb_cima}", f"A{rb_cima}", f"A{n_gnd}", f"B{n_gnd}", f"C{n_gnd}", f"D{n_gnd}"],
    "tras", "fio", "esquerda", f"da perna de cima do verde até D{n_gnd} (GND)", "preto")
lig("coluna 1 4 7 *", [f"C{BAIXO[0]}", f"C{BAIXO[0] - 1}", f"A{BAIXO[0] - 1}", f"A{n_col1}", f"B{n_col1}",
                       f"C{n_col1}", f"D{n_col1}", f"E{n_col1}"], "tras", "fio", "esquerda",
    f"do alto do fio nu da coluna C até E{n_col1} (IO17)", "marrom")

# pontes da frente da esquerda
for pino, sinal, gpio, cor in TELA_LIGA:
    n = linha_j1(gpio)
    lig(f"tela {sinal}", [f"E{n}", f"F{n}"], "frente", "ponte", "pontes", "inclui a ponta do fio que atravessa o E")
lig("botão verde", [f"E{n_verde}", f"F{n_verde}"], "frente", "ponte", "pontes")
lig("coluna 1 4 7 *", [f"E{n_col1}", f"F{n_col1}"], "frente", "ponte", "pontes")
lig("GND dos botões", [f"D{n_gnd}", f"E{n_gnd}"], "frente", "ponte", "pontes", "junta com a ponte E–F da tela")

# Os capacitores da tela, atrás, soldados direto nos pinos VCC e GND dela
CAPACITORES = [
    ("100 nF (cerâmico, \"104\")", f"{col_tela(1)}1", f"{col_tela(2)}1", "qualquer lado"),
    ("100 µF (eletrolítico)", f"{col_tela(1)}1", f"{col_tela(2)}1", "a perna mais curta (−) no GND"),
]

# ---------------------------------------------------------------------------
# Conferências: furos repetidos, alturas embaixo dos fios de trás
# ---------------------------------------------------------------------------
PERNAS = {}
for ch in CHAVES:
    (a, b), (c, d) = ch.pernas()
    for nome in (a, b, c, d):
        PERNAS[nome] = ch.rotulo


def ocupados():
    """furo → o que está nele (pernas, barras, tela)"""
    o = dict((n, f"perna da tecla {r}") for n, r in PERNAS.items())
    for n in range(1, 17):
        o[P.pino_j1(n)] = f"J1-{n} {P.J1[n - 1]}"
        o[P.pino_j2(n)] = f"J2-{n} {P.J2[n - 1]}"
    for p in range(1, 15):
        o[f"{col_tela(p)}1"] = f"tela {P.TELA_PINOS[p - 1]}"
    return o


def conferir():
    erros = []
    o = ocupados()
    # nenhum fio de trás passa por cima de um furo com perna, pino ou solda que não seja dele
    pontas = {}
    for l in LIG:
        for n in (l.de, l.ate):
            pontas.setdefault(n, set()).add(l.sinal)
    for l in LIG:
        if l.lado != "tras":
            continue
        meio = l.caminho[1:-1]
        for n in meio:
            if n in o and l.tipo != "fio":
                erros.append(f"{l.sinal}: o fio nu passa por {n} ({o[n]})")
            if n in pontas and l.sinal not in pontas[n] and l.tipo != "fio":
                erros.append(f"{l.sinal}: o fio nu passa pela solda de {n} ({pontas[n]})")
    # os fios nus não se cruzam nem encostam em solda de outro sinal (mesmo furo)
    por_furo = {}
    for l in LIG:
        if l.lado == "tras" and l.tipo in ("nu", "ponte"):
            for n in l.caminho:
                por_furo.setdefault(n, set()).add(l.sinal)
    for n, s in por_furo.items():
        if len(s) > 1:
            erros.append(f"furo {n}: sinais diferentes no mesmo lugar: {sorted(s)}")
    # cada furo das pontas existe
    for l in LIG:
        for n in l.caminho:
            c, k = P.ck(n)
            if not (0 <= c < P.COLUNAS and 0 <= k < P.LINHAS):
                erros.append(f"{l.sinal}: furo {n} fora da placa")
    # as alturas por cima da LILYGO: fio de trás antes de juntar precisa de 0,9 mm (um fio) livre
    for l in LIG:
        if l.lado == "tras" and l.etapa in ("matriz", "direita"):
            for n in l.caminho:
                x, y = P.xy(n)
                if P.dentro_da_lilygo(x, y):
                    livre = P.VAO - P.altura_embaixo(x, y, 0.9)
                    if livre < 1.6:
                        erros.append(f"{l.sinal}: em {n} sobra só {livre:.2f} mm entre a placa e a LILYGO")
    # a faixa da esquerda (depois de juntar) não pode entrar na LILYGO
    for l in LIG:
        if l.etapa == "esquerda":
            for n in l.caminho[:-1]:
                x, y = P.xy(n)
                if P.dentro_da_lilygo(x, y) and not n.startswith(("E", "D")):
                    erros.append(f"{l.sinal}: {n} fica em cima da LILYGO (não dá para soldar depois de juntar)")
    return erros


def tabela():
    return LIG


if __name__ == "__main__":
    print(len(CHAVES), "chaves;", len(LIG), "ligações")
    for l in LIG:
        print(f"{l.etapa:9} {l.lado:6} {l.tipo:6} {l.de:>4} → {l.ate:<4} {l.sinal:18} {l.cor:9} {l.nota}")
    e = conferir()
    print("\nconferência:", "ok" if not e else "")
    for x in e:
        print("  ", x)
