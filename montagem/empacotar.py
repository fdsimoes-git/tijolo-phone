"""
Junta o que vai para a fábrica de placas e para a gráfica 3D:

    python3 empacotar.py     (da pasta montagem/; depois de placa/placa.py, caixa/caixa.py e guia.py)

Sai em fabricar/ (esta fica no git: é o que alguém precisa para montar um Tijolo
sem instalar o KiCad nem o CadQuery) e, em saida/, tijolo-caixa-3d.zip: os STL,
os .step e um LEIA-ME para a gráfica 3D. Para a fábrica de placas vai direto o
saida/placa/tijolo-pcb-gerbers-jlcpcb.zip.
"""
import os
import shutil
import zipfile

import projeto as P

AQUI = os.path.dirname(os.path.abspath(__file__))
SAIDA = os.path.join(AQUI, "saida")
FABRICAR = os.path.join(AQUI, "fabricar")
GUIA = "https://claude.ai/artifact/CuJ8r7VZzr5Dn8WH7geubx"

LARGURA = P.CAV_X1 - P.CAV_X0 + 2 * P.PAREDE
COMPRIMENTO = P.CAV_Y1 - P.CAV_Y0 + 2 * P.PAREDE
PCB_L, PCB_A = P.PCB_X1 - P.PCB_X0, P.PCB_Y1 - P.LINGUA[1]

STL = ["fundo.stl", "frente.stl", "botao_verde.stl", "botao_vermelho.stl"]
STEP = ["fundo.step", "frente.step", "botao_verde.step", "botao_vermelho.step"]
PCB = ["tijolo-pcb-gerbers-jlcpcb.zip", "tijolo-pcb.kicad_pcb", "tijolo-pcb.kicad_pro", "pecas.csv"]


def mm(v, casas=1):
    return f"{v:.{casas}f}".replace(".", ",")


def leia_me():
    medidas = "\n".join(f"| {oque} | {valor} | {faixa} |" for _, oque, valor, faixa, _ in P.MEDIDAS)
    return f"""# Tijolo: arquivos para fabricar

O passo a passo da montagem está no [guia]({GUIA}). Aqui estão os arquivos que vão para a fábrica de placas e para a
gráfica 3D. Eles saem dos scripts da pasta `montagem/`; não edite à mão.

![O Tijolo montado](tijolo.png)

## Antes de imprimir e de encomendar

As medidas vêm dos desenhos dos fabricantes. Estas só dá para confirmar com a peça na mão:

| O que medir | No projeto | Serve |
|---|---|---|
{medidas}

Se alguma sair da faixa, mude a medida em `montagem/projeto.py` e gere tudo de novo.

## A PCB (`pcb/`)

| Arquivo | Para quê |
|---|---|
| `tijolo-pcb-gerbers-jlcpcb.zip` | o que se envia para a fábrica (Gerbers e furação) |
| `tijolo-pcb.kicad_pcb` e `.kicad_pro` | a placa, para abrir no KiCad 7 |
| `pecas.csv` | as peças e onde vão |
| `pcb-frente.svg` e `pcb-tras.svg` | as duas faces, para conferir na bancada |

Para encomendar (JLCPCB ou parecida): 2 camadas, FR-4, **espessura de 0,8 mm**, 5 unidades, acabamento HASL sem chumbo. A
placa mede {mm(PCB_L)} × {mm(PCB_A)} mm. Com a espessura padrão, de 1,6 mm, a caixa não fecha.

| Na placa | Peça | Lado |
|---|---|---|
| J1, J2 | barra fêmea 1 × 16 de perna longa (empilhável), 2,54 mm, perna de {mm(P.FEMEA_PERNA_MIN)} mm ou mais | atrás; a solda é na frente |
| TELA | a tela 2,4" ILI9341, pelos pinos dela (os 4 marcados com X são cortados antes) | frente |
| VERDE, VERMELHO | chave táctil 6 × 6 × 5 mm, 4 pernas | frente |
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
    return f"""TIJOLO - caixa para imprimir em 3D

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
  o fundo tem {mm(LARGURA)} x {mm(COMPRIMENTO)} x 30,1 mm;
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

    z2 = os.path.join(SAIDA, "tijolo-caixa-3d.zip")
    with zipfile.ZipFile(z2, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("tijolo-caixa-3d/LEIA-ME.txt", leia_me_da_grafica())
        z.write(os.path.join(FABRICAR, "tijolo.png"), "tijolo-caixa-3d/tijolo.png")
        for arq in STL:
            z.write(os.path.join(SAIDA, "caixa", arq), f"tijolo-caixa-3d/{arq}")
        for arq in STEP:
            z.write(os.path.join(SAIDA, "caixa", arq), f"tijolo-caixa-3d/step/{arq}")
    return [z2, os.path.join(SAIDA, "placa", "tijolo-pcb-gerbers-jlcpcb.zip")]


if __name__ == "__main__":
    for arq in empacotar():
        print(arq, os.path.getsize(arq) // 1024, "KB")
    total = 0
    for pasta, _, arqs in os.walk(FABRICAR):
        for a in arqs:
            total += os.path.getsize(os.path.join(pasta, a))
    print("fabricar/:", total // 1024, "KB")
