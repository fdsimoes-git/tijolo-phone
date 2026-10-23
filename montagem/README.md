# Montagem do Tijolo: placa ilhada e caixa 3D

O Tijolo montado em sanduíche, quase sem fios: uma placa ilhada na frente, com as 14 teclas e a tela, encaixada direto nas
barras de pinos da LILYGO. Na frente da placa não corre nenhum fio, só pontes de solda; os 19 fios ficam atrás, curtos.
Uma caixa impressa em 3D fecha tudo.

O passo a passo para quem vai soldar está no [guia](https://claude.ai/artifact/CuJ8r7VZzr5Dn8WH7geubx) (uma página com os desenhos furo a furo, as tabelas, os testes com o
multímetro, a vista 3D da caixa e o formulário das medidas). Esta pasta tem os scripts que geram o guia, os desenhos e a caixa:
mudando uma medida em `projeto.py`, tudo se refaz igual.

## Os arquivos

| Arquivo | O que faz |
|---|---|
| `projeto.py` | Todas as medidas num lugar só: a grade da placa (18 × 51 furos), onde fica a LILYGO, as alturas (Z), a tela, as teclas, a caixa, o alto-falante e o microfone. `MEDIDAS` lista o que medir antes de encomendar a caixa. `python3 projeto.py` mostra um resumo. |
| `lilygo_pecas.json` | As caixas das peças da LILYGO, tiradas do modelo 3D oficial (T-A7670X-S3-Standard.step, de 26/08/2026): onde está cada peça e a altura dela, dos dois lados da placa. |
| `fiacao.py` | O que vai em cada furo e cada ligação: a matriz do teclado, os 7 fios da direita, os 12 da faixa da esquerda e as pontes da frente. `conferir()` procura furos repetidos, fio nu passando por solda de outro sinal e fio sem folga em cima das peças da LILYGO. `python3 fiacao.py` lista tudo e confere. |
| `desenho.py` | Os desenhos (SVG): a frente, atrás antes e depois de juntar com a LILYGO e o corte de lado. Faz também a frente e o "atrás depois" para a tela com o VCC à direita. |
| `caixa/caixa.py` | A caixa (CadQuery): o fundo, a frente e as 14 teclas, já virados do jeito de imprimir, e versões leves para a vista 3D do guia. No fim, confere se alguma peça de dentro bate na caixa. |
| `guia.py` | O guia: `saida/guia/index.html` (com os desenhos dentro) e `saida/guia/3d/pecas.json` (as peças da vista 3D). |

Tudo sai em `saida/` (fora do git).

## Como gerar

Precisa do Python 3.10 ou mais novo com o `numpy` e, para a caixa, do CadQuery 2.x (`pip install cadquery`). Da pasta
`montagem/`:

```
python3 fiacao.py            # confere a fiação
python3 caixa/caixa.py       # a caixa (uns 2 minutos) e a conferência das colisões
python3 guia.py              # o guia, com os desenhos
```

`caixa/caixa.py fundo`, `frente`, `teclas`, `visual` ou `conferir` faz só aquela parte. `TELA_ESPELHADA=1 python3 fiacao.py`
mostra a fiação da tela com o VCC à direita. A imagem de capa do guia (`capa.png`, o que aparece antes da vista 3D carregar) é
uma foto da própria vista 3D, tirada com o navegador.

## Como fica

De trás para a frente (Z, em mm, a partir da face de fora do fundo):

| Z | O quê |
|---|---|
| 0 a 1,8 | o fundo da caixa |
| 2,2 | a pilha 18650, no suporte da LILYGO |
| 21,0 a 22,6 | a LILYGO, com os componentes para a frente |
| 22,6 a 27,7 | dois plásticos de barra de pinos empilhados (5,08 mm): ali ficam a matriz do teclado e os 7 fios da direita |
| 27,7 a 29,3 | a placa ilhada |
| 31,8 a 36,1 | a tela, sobre um plástico de barra, e as teclas |
| 38,2 | a face da frente da caixa (as teclas saem 1,2 mm) |

A caixa tem 50,3 × 141,2 × 38,2 mm. A frente desce reta sobre o fundo: em cima, duas linguetas com dente fazem clic nos rasgos
da parede; embaixo, dois parafusos M2 × 8 entram por trás nos pilares da frente. A LILYGO fica apoiada em 4 pilares com pinos nos
furos dela.

## Os pinos (`MONTAGEM_PLACA 1`)

Na placa ilhada, os botões verde e vermelho são de verdade e a coluna 1 4 7 * do teclado troca de pino com o vermelho, que
precisa de um pino que acorde a ESP32 (IO21). Com `#define MONTAGEM_PLACA 1` no `include/config.h`:

| O quê | Pino |
|---|---|
| Fileiras 1 2 3 / 4 5 6 / 7 8 9 / * 0 # | IO15 / IO38 / IO39 / IO40 |
| Colunas 1 4 7 * / 2 5 8 0 / 3 6 9 # | IO17 / IO37 / IO16 |
| Botão verde / vermelho | IO35 / IO21 |
| Tela | a mesma da Fase 2 (CS IO47, RESET IO9, DC IO14, MOSI IO11, SCK IO12, MISO IO13, luz IO36) |

Depois de gravar, ensine o teclado de novo: segure o 1 enquanto aparece "Ligando o modem…" (ou `teclado aprender` no Monitor
Serial).

## Antes de encomendar a caixa

A caixa usa as medidas do desenho do fabricante da tela (MSP2401, LCDWiki), do modelo 3D da LILYGO e dos nomes das peças
(alto-falante 3525, microfone 9767). As que podem variar estão em `projeto.MEDIDAS`, e o guia tem um formulário com elas:
meça as suas peças e confira antes de mandar imprimir.
