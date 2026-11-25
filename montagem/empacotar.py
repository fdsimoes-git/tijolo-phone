"""
Junta o que vai para a fábrica de placas e para a gráfica 3D:

    python3 empacotar.py     (da pasta montagem/; depois de placa/placa.py, caixa/caixa.py e guia.py)

Sai em fabricar/ (esta fica no git: é o que alguém precisa para montar um Tijolo
sem instalar o KiCad nem o CadQuery) e, em saida/, os dois arquivos de mandar,
com a versão no nome (a mesma que vem escrita na PCB), para não confundir com
os de uma versão anterior: tijolo-caixa-3d-2026-11-25.zip (os STL, os .step e
um LEIA-ME para a gráfica 3D; a data é a P.CAIXA_VERSAO) e
tijolo-pcb-v3-gerbers-jlcpcb.zip (para a fábrica de placas; é uma cópia do
saida/placa/tijolo-pcb-gerbers-jlcpcb.zip, com a versão que vem na placa).
"""
import os
import re
import shutil
import struct
import zipfile

import projeto as P

AQUI = os.path.dirname(os.path.abspath(__file__))
SAIDA = os.path.join(AQUI, "saida")
FABRICAR = os.path.join(AQUI, "fabricar")
GUIA = "https://claude.ai/artifact/CuJ8r7VZzr5Dn8WH7geubx"

with open(os.path.join(AQUI, "placa", "placa.py"), encoding="utf-8") as _f:
    VERSAO_PCB = re.search(r'^VERSAO = "([^"]+)"', _f.read(), re.M).group(1)
ETIQUETA = VERSAO_PCB.split()[0]             # "v3": vai no nome do zip da PCB
ZIP_CAIXA = f"tijolo-caixa-3d-{P.CAIXA_VERSAO}.zip"
ZIP_PCB = f"tijolo-pcb-{ETIQUETA}-gerbers-jlcpcb.zip"

LARGURA = P.CAV_X1 - P.CAV_X0 + 2 * P.PAREDE
COMPRIMENTO = P.CAV_Y1 - P.CAV_Y0 + 2 * P.PAREDE
PCB_L, PCB_A = P.PCB_X1 - P.PCB_X0, P.PCB_Y1 - P.LINGUA[1]

STL = ["fundo.stl", "frente.stl", "botao_verde.stl", "botao_vermelho.stl"]
STEP = ["fundo.step", "frente.step", "botao_verde.step", "botao_vermelho.step"]
PCB = ["tijolo-pcb-gerbers-jlcpcb.zip", "tijolo-pcb.kicad_pcb", "tijolo-pcb.kicad_pro", "pecas.csv"]


def mm(v, casas=1):
    return f"{v:.{casas}f}".replace(".", ",")


def medidas_stl(caminho):
    """(largura, comprimento, altura) de um STL binário, em mm"""
    with open(caminho, "rb") as f:
        dados = f.read()
    n = struct.unpack_from("<I", dados, 80)[0]
    assert 84 + 50 * n == len(dados), caminho
    mn, mx = [1e9] * 3, [-1e9] * 3
    for i in range(n):
        v = struct.unpack_from("<9f", dados, 84 + 50 * i + 12)
        for k in range(9):
            mn[k % 3] = min(mn[k % 3], v[k])
            mx[k % 3] = max(mx[k % 3], v[k])
    return tuple(b - a for a, b in zip(mn, mx))


def leia_me():
    medidas = "\n".join(f"| {oque} | {valor} | {faixa} | {fonte} |" for _, oque, valor, faixa, fonte in P.MEDIDAS)
    return f"""# Tijolo: arquivos para fabricar

O passo a passo da montagem está no [guia]({GUIA}). Aqui estão os arquivos que vão para a fábrica de placas e para a
gráfica 3D. Eles saem dos scripts da pasta `montagem/`; não edite à mão.

![O Tijolo montado](tijolo.png)

## Antes de imprimir e de encomendar

As medidas vêm dos desenhos dos fabricantes. Estas só dá para confirmar com a peça na mão, e os arquivos daqui foram feitos
com as peças do Felipe:

| O que medir | No projeto | Serve | De onde veio |
|---|---|---|---|
{medidas}

Se alguma peça sua sair da faixa, mude a medida em `montagem/projeto.py` e gere tudo de novo.

## A PCB (`pcb/`)

| Arquivo | Para quê |
|---|---|
| `tijolo-pcb-gerbers-jlcpcb.zip` | o que se envia para a fábrica (Gerbers e furação) |
| `tijolo-pcb.kicad_pcb` e `.kicad_pro` | a placa, para abrir no KiCad 7 |
| `pecas.csv` | as peças e onde vão |
| `pcb-frente.svg` e `pcb-tras.svg` | as duas faces, para conferir na bancada |

Para encomendar (JLCPCB ou parecida): 2 camadas, FR-4, **espessura de 0,8 mm**, 5 unidades, acabamento HASL sem chumbo. A
placa mede {mm(PCB_L)} × {mm(PCB_A)} mm e leva escrito **{VERSAO_PCB}**. Com a espessura padrão, de 1,6 mm, a caixa não fecha.

| Na placa | Peça | Lado |
|---|---|---|
| J1, J2 | barra fêmea 1 × 16 de perna longa (empilhável), 2,54 mm, perna de {P.PERNA_MIN_TXT} mm ou mais | atrás; a solda é na frente |
| TELA | a tela 2,4" ILI9341, pelos pinos dela (os 4 marcados com X são cortados antes) | frente |
| VERDE, VERMELHO | chave táctil 6 × 6 mm, com {mm(P.CHAVE_TACTIL_ALTURA)} mm de altura, 4 pernas | frente |
| C2 | capacitor eletrolítico de 100 µF, 10 V ou mais | atrás |
| C1, C5 | capacitor cerâmico de 100 nF | atrás |
| C4 | capacitor cerâmico de 100 pF (opcional) | atrás |
| ABA DO TECLADO | 7 perninhas de fio até a aba do teclado 3×4 | frente |

## A caixa (`caixa/`)

| Arquivo | O que é | Como imprimir |
|---|---|---|
| `fundo.stl` | a metade de trás | a face de trás na mesa |
| `frente.stl` | a metade da frente | a face da frente na mesa (já está virada) |
| `botao_verde.stl` | o botão verde (✓) | em pé, como está |
| `botao_vermelho.stl` | o botão vermelho (✕) | em pé, como está |

PETG ou PLA, camada de 0,2 mm, 3 perímetros, 20% de preenchimento, **sem suportes**. A caixa fechada mede
{mm(LARGURA)} × {mm(COMPRIMENTO)} × {mm(P.Z_TOPO)} mm.

Parafusos: 2 M2 × 8 de cabeça chata fecham a caixa, por trás. Outros 2 M2 × 5 prendem a LILYGO no fundo.
"""


def leia_me_da_grafica():
    fundo = medidas_stl(os.path.join(SAIDA, "caixa", "fundo.stl"))
    botao = medidas_stl(os.path.join(SAIDA, "caixa", "botao_verde.stl"))
    return f"""TIJOLO - caixa para imprimir em 3D
Versão de {P.CAIXA_VERSAO}, feita com as medidas das peças: a caixa fechada tem {mm(P.Z_TOPO)} mm de espessura.

Quatro peças, já viradas do jeito de imprimir:

  fundo.stl            a metade de trás: a face de trás na mesa
  frente.stl           a metade da frente: a face da frente na mesa (já está virada)
  botao_verde.stl      em pé, como está; filamento verde, se tiver
  botao_vermelho.stl   em pé, como está; filamento vermelho, se tiver

Material: PETG (de preferência) ou PLA.
Camada de 0,2 mm, 3 perímetros, 20% de preenchimento.
SEM SUPORTES em nenhuma peça. Se os cantos do fundo levantarem da mesa, usar aba (brim).

Medidas, para conferir depois de imprimir:
  a caixa fechada tem {mm(LARGURA)} x {mm(COMPRIMENTO)} x {mm(P.Z_TOPO)} mm;
  o fundo tem {mm(fundo[0])} x {mm(fundo[1])} x {mm(fundo[2])} mm;
  cada botão tem {mm(botao[0])} x {mm(botao[1])} x {mm(botao[2])} mm;
  as paredes têm {mm(P.PAREDE)} mm.

A pasta step/ tem as mesmas peças em STEP, para quem quiser mexer no desenho
(nela a frente está na posição de montagem, com a face da frente para cima).
"""


def copia(origem, destino):
    os.makedirs(os.path.dirname(destino), exist_ok=True)
    shutil.copyfile(origem, destino)


def figura():
    """a capa do guia (fundo transparente) sobre um fundo claro, para o LEIA-ME. A capa é uma foto da
    vista 3D do guia, tirada com o navegador; sem ela, fica a figura que já estava em fabricar/"""
    origem = os.path.join(SAIDA, "guia", "capa.png")
    if not os.path.isfile(origem):
        return
    from PIL import Image
    capa = Image.open(origem).convert("RGBA")
    fundo = Image.new("RGBA", capa.size, (238, 240, 235, 255))
    fundo.alpha_composite(capa)
    fundo.convert("RGB").save(os.path.join(FABRICAR, "tijolo.png"), optimize=True)


def empacotar():
    os.makedirs(FABRICAR, exist_ok=True)
    for arq in STL:
        copia(os.path.join(SAIDA, "caixa", arq), os.path.join(FABRICAR, "caixa", arq))
    for arq in PCB:
        copia(os.path.join(SAIDA, "placa", arq), os.path.join(FABRICAR, "pcb", arq))
    for lado in ("frente", "tras"):
        copia(os.path.join(SAIDA, "guia", f"pcb-{lado}.svg"), os.path.join(FABRICAR, "pcb", f"pcb-{lado}.svg"))
    figura()
    with open(os.path.join(FABRICAR, "LEIA-ME.md"), "w", encoding="utf-8") as f:
        f.write(leia_me())

    for velho in os.listdir(SAIDA):                    # os zips de mandar de uma versão anterior
        if velho.endswith(".zip"):
            os.remove(os.path.join(SAIDA, velho))
    pasta = ZIP_CAIXA[:-len(".zip")]
    z2 = os.path.join(SAIDA, ZIP_CAIXA)
    with zipfile.ZipFile(z2, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr(f"{pasta}/LEIA-ME.txt", leia_me_da_grafica())
        z.write(os.path.join(FABRICAR, "tijolo.png"), f"{pasta}/tijolo.png")
        for arq in STL:
            z.write(os.path.join(SAIDA, "caixa", arq), f"{pasta}/{arq}")
        for arq in STEP:
            z.write(os.path.join(SAIDA, "caixa", arq), f"{pasta}/step/{arq}")
    z1 = os.path.join(SAIDA, ZIP_PCB)
    shutil.copyfile(os.path.join(SAIDA, "placa", "tijolo-pcb-gerbers-jlcpcb.zip"), z1)
    return [z2, z1]


if __name__ == "__main__":
    for arq in empacotar():
        print(arq, os.path.getsize(arq) // 1024, "KB")
    total = 0
    for pasta, _, arqs in os.walk(FABRICAR):
        for a in arqs:
            total += os.path.getsize(os.path.join(pasta, a))
    print("fabricar/:", total // 1024, "KB")
