"""
O guia de montagem (uma página HTML, publicada como artifact):

    python3 guia.py          (da pasta montagem/; antes: placa/placa.py e caixa/caixa.py)

Sai em saida/guia/: index.html, pcb-frente.svg e pcb-tras.svg (os desenhos da PCB,
tirados do KiCad e recoloridos) e 3d/pecas.json (as peças da vista 3D). A imagem de
capa (capa.png, o que aparece antes da vista 3D carregar) é uma foto da própria
vista 3D, tirada com o navegador.
"""
import base64
import html
import json
import os
import re
import struct

import numpy as np

import projeto as P

AQUI = os.path.dirname(os.path.abspath(__file__))
SAIDA = os.path.join(AQUI, "saida", "guia")
CDN = "https://cdn.jsdelivr.net/npm/three@0.128.0"
ARTIFACT = "https://claude.ai/artifact/CuJ8r7VZzr5Dn8WH7geubx"

with open(os.path.join(AQUI, "placa", "placa.py"), encoding="utf-8") as _f:
    VERSAO_PCB = re.search(r'^VERSAO = "([^"]+)"', _f.read(), re.M).group(1)
ETIQUETA = VERSAO_PCB.split()[0]             # "v3": vai no nome dos arquivos mandados na conversa (empacotar.py)

LARGURA = P.CAV_X1 - P.CAV_X0 + 2 * P.PAREDE
COMPRIMENTO = P.CAV_Y1 - P.CAV_Y0 + 2 * P.PAREDE
PCB_L, PCB_A = P.PCB_X1 - P.PCB_X0, P.PCB_Y1 - P.LINGUA[1]
# o desenho anterior (placa ilhada com 14 chaves e fios), para comparar: espessura, largura, comprimento, fios
ANTES = dict(espessura=38.17, largura=50.3, comprimento=141.2, fios=19)
# a versão anterior desta montagem, feita só com as medidas dos fabricantes (antes de o Felipe medir as peças)
SEM_MEDIR = dict(espessura=35.44, pilha=19.6, teclas=3.36, aba_botao=1.6, calha=35.4, teclado_altura=64.2)


def esc(s):
    return html.escape(str(s), quote=True)


def mm(v, casas=1):
    return f"{v:.{casas}f}".replace(".", ",")


# ---------------------------------------------------------------------------
# a página
# ---------------------------------------------------------------------------
CSS = r"""
/* Manual de bancada: uma coluna de texto, figuras largas e a placa como protagonista. */
:root {
  --papel: #eef0eb;  --cartao: #f8f9f6;  --tinta: #1c2420;  --tinta-2: #4c5852;  --linha: #c9cfc7;
  --verde: #2f6247;  --cobre: #b06a1f;  --cobre-claro: #f3e2cd;  --alerta: #a3341f;  --alerta-fundo: #f6dfd8;
  --ok: #2f7a4a;  --sombra: rgba(28, 36, 32, .08);  --fundo-3d: #e2e6e0;  --prancha: #f4f2ea;
  --d-caixa: #8f989f;  --d-caixa-2: #b4bcc2;  --d-lilygo: #27485a;  --d-pilha: #d9a441;  --d-pcb: #2f7a4f;
  --d-tela: #a8483c;  --d-vidro: #20262e;  --d-vao: #dfe4dd;  --d-som: #8a6f4e;
  --display: "Barlow Condensed", "Arial Narrow", sans-serif;
  --texto: "IBM Plex Sans", "Segoe UI", system-ui, sans-serif;
  --mono: "IBM Plex Mono", ui-monospace, "SFMono-Regular", Menlo, monospace;
}
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) {
  --papel: #121815;  --cartao: #19211d;  --tinta: #e1e7e1;  --tinta-2: #a5b1aa;  --linha: #2f3a34;
  --verde: #5fae80;  --cobre: #e0a060;  --cobre-claro: #3a2a17;  --alerta: #f08a72;  --alerta-fundo: #3a1d16;
  --ok: #6fc98f;  --sombra: rgba(0, 0, 0, .35);  --fundo-3d: #1b2420;  --prancha: #e9e6dc;
  --d-caixa: #6f7880;  --d-caixa-2: #4d565d;  --d-lilygo: #3d6b83;  --d-pilha: #c9973a;  --d-pcb: #3f9463;
  --d-tela: #b85a4d;  --d-vidro: #3b454e;  --d-vao: #232d28;  --d-som: #a98a63;  color-scheme: dark; } }
:root[data-theme="dark"] {
  --papel: #121815;  --cartao: #19211d;  --tinta: #e1e7e1;  --tinta-2: #a5b1aa;  --linha: #2f3a34;
  --verde: #5fae80;  --cobre: #e0a060;  --cobre-claro: #3a2a17;  --alerta: #f08a72;  --alerta-fundo: #3a1d16;
  --ok: #6fc98f;  --sombra: rgba(0, 0, 0, .35);  --fundo-3d: #1b2420;  --prancha: #e9e6dc;
  --d-caixa: #6f7880;  --d-caixa-2: #4d565d;  --d-lilygo: #3d6b83;  --d-pilha: #c9973a;  --d-pcb: #3f9463;
  --d-tela: #b85a4d;  --d-vidro: #3b454e;  --d-vao: #232d28;  --d-som: #a98a63;  color-scheme: dark; }

* { box-sizing: border-box; }
body { margin: 0; background: var(--papel); color: var(--tinta); font: 400 16px/1.6 var(--texto);
  padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px)); }
.pagina { max-width: 1120px; margin: 0 auto; padding-block: 28px 80px; display: grid;
  grid-template-columns: 220px minmax(0, 1fr); gap: 40px; }
@media (max-width: 900px) { .pagina { grid-template-columns: minmax(0, 1fr); gap: 0; } nav.indice { display: none; } }
nav.indice { position: sticky; top: calc(env(safe-area-inset-top, 0px) + 20px); align-self: start;
  font-size: 14px; line-height: 1.45; padding-top: 6px; max-height: calc(100vh - 40px); overflow-y: auto; }
nav.indice a { display: block; color: var(--tinta-2); text-decoration: none; padding: 4px 0 4px 12px;
  border-left: 2px solid var(--linha); }
nav.indice a:hover, nav.indice a:focus-visible { color: var(--tinta); border-left-color: var(--cobre); outline: none; }
nav.indice .num { font-family: var(--mono); font-size: 12px; color: var(--cobre); margin-right: 6px; }
main { min-width: 0; }
h1, h2, h3 { font-family: var(--display); font-weight: 600; line-height: 1.1; text-wrap: balance; margin: 0; }
h1 { font-size: clamp(44px, 7vw, 72px); letter-spacing: .5px; }
h2 { font-size: clamp(30px, 4vw, 40px); margin-top: 64px; padding-top: 18px; border-top: 2px solid var(--tinta); }
h3 { font-size: 23px; margin-top: 30px; }
p, li { max-width: 68ch; }
p { margin: 12px 0; }
a { color: var(--verde); }
.eyebrow { font-family: var(--mono); font-size: 13px; letter-spacing: 1.5px; text-transform: uppercase; color: var(--cobre); }
.lead { font-size: 19px; color: var(--tinta-2); max-width: 60ch; }
.mono { font-family: var(--mono); }
.pequeno { font-size: 13px; color: var(--tinta-2); }
kbd, code { font-family: var(--mono); font-size: .9em; background: var(--cartao); border: 1px solid var(--linha);
  border-radius: 4px; padding: 0 5px; overflow-wrap: anywhere; }
.fichas { display: flex; flex-wrap: wrap; gap: 8px 10px; margin: 20px 0 6px; padding: 0; list-style: none; }
.fichas li { font-family: var(--mono); font-size: 13px; border: 1px solid var(--linha); border-radius: 999px;
  padding: 4px 12px; background: var(--cartao); }

/* a vista 3D */
.vista { position: relative; margin: 26px 0 8px; border-radius: 10px; overflow: hidden; background: var(--fundo-3d);
  aspect-ratio: 16 / 11; max-width: 100%; }
.vista img, .vista canvas { position: absolute; inset: 0; width: 100%; height: 100%; object-fit: contain; display: block; }
.vista canvas { touch-action: none; cursor: grab; }
@media (max-width: 560px) { .vista { aspect-ratio: 4 / 5; } }
.vista .controles { position: absolute; left: 12px; right: 12px; bottom: 12px; display: flex; flex-wrap: wrap; gap: 6px; z-index: 2; }
.vista .dica { position: absolute; right: 12px; top: 10px; font-size: 12px; color: var(--tinta-2); z-index: 2; }
button.chip { font: 500 13px/1 var(--texto); color: var(--tinta); background: var(--cartao); border: 1px solid var(--linha);
  border-radius: 999px; padding: 8px 12px; cursor: pointer; }
button.chip[aria-pressed="true"] { background: var(--tinta); color: var(--papel); border-color: var(--tinta); }
button.chip:focus-visible, input:focus-visible { outline: 2px solid var(--cobre); outline-offset: 2px; }

/* figuras */
figure { margin: 22px 0; }
figcaption { font-size: 14px; color: var(--tinta-2); margin-top: 8px; max-width: 75ch; }
.duas { display: grid; grid-template-columns: repeat(auto-fit, minmax(230px, 1fr)); gap: 18px; align-items: start; }
.prancha { background: var(--prancha); border: 1px solid var(--linha); border-radius: 8px; padding: 12px; min-width: 0; }
.prancha img { display: block; width: 100%; height: auto; }
.prancha .titulo { font-family: var(--mono); font-size: 12px; letter-spacing: .8px; text-transform: uppercase; color: #4c5852;
  margin-bottom: 8px; }
.lado-a-lado { display: grid; grid-template-columns: minmax(0, 420px) minmax(0, 1fr); gap: 28px; align-items: start; }
@media (max-width: 760px) { .lado-a-lado { grid-template-columns: minmax(0, 1fr); } }
.desenho-caixa { background: var(--cartao); border: 1px solid var(--linha); border-radius: 8px; padding: 10px; min-width: 0; }
svg.dz { display: block; width: 100%; height: auto; }
svg.dz text { font-family: var(--texto); fill: var(--tinta); }
svg.dz .fino { fill: var(--tinta-2); }
svg.dz .guia { stroke: var(--tinta-2); stroke-width: .8; fill: none; }
svg.dz .cota { stroke: var(--tinta); stroke-width: 1; fill: none; }
svg.dz .c-caixa { fill: var(--d-caixa); } svg.dz .c-caixa2 { fill: var(--d-caixa-2); } svg.dz .c-lilygo { fill: var(--d-lilygo); }
svg.dz .c-pilha { fill: var(--d-pilha); } svg.dz .c-pcb { fill: var(--d-pcb); } svg.dz .c-tela { fill: var(--d-tela); }
svg.dz .c-vidro { fill: var(--d-vidro); } svg.dz .c-vao { fill: var(--d-vao); } svg.dz .c-som { fill: var(--d-som); }
svg.dz .contorno { fill: none; stroke: var(--tinta-2); stroke-width: .35; }
svg.dz .tracejado { fill: none; stroke: var(--tinta-2); stroke-width: .35; stroke-dasharray: 1.4 1; }
svg.dz .cabo-som { fill: none; stroke: var(--cobre); stroke-width: 1.1; stroke-linecap: round; stroke-linejoin: round; }
svg.dz .cabo-mic { fill: none; stroke: var(--verde); stroke-width: 1.1; stroke-linecap: round; stroke-linejoin: round; }
svg.dz .cabo-antena { fill: none; stroke: var(--alerta); stroke-width: .9; stroke-linecap: round; stroke-dasharray: 2 1.4; }

/* tabelas */
.rolagem { overflow-x: auto; margin: 14px 0; border: 1px solid var(--linha); border-radius: 8px; background: var(--cartao); }
table { border-collapse: collapse; width: 100%; font-size: 14px; font-variant-numeric: tabular-nums; }
th, td { text-align: left; padding: 7px 10px; border-bottom: 1px solid var(--linha); vertical-align: top; }
th { font-family: var(--mono); font-weight: 500; font-size: 12px; letter-spacing: .6px; text-transform: uppercase; color: var(--tinta-2); }
tbody tr:last-child td { border-bottom: 0; }
td.num, th.num { text-align: right; white-space: nowrap; }
.tem { color: var(--ok); font-weight: 600; white-space: nowrap; }
.comprar { color: var(--cobre); font-weight: 600; white-space: nowrap; }
.total td { font-weight: 600; border-top: 2px solid var(--tinta); }
tr.fora td { background: var(--alerta-fundo); }
tr.fora td:first-child { box-shadow: inset 4px 0 0 var(--alerta); }

/* passos */
.passo { margin-top: 56px; }
.passo-cab { display: flex; gap: 16px; align-items: baseline; border-top: 2px solid var(--tinta); padding-top: 16px; }
.passo-num { font-family: var(--display); font-size: 54px; line-height: .8; color: var(--cobre); font-weight: 700; min-width: 1.2em; }
.passo-cab h2 { margin: 0; border: 0; padding: 0; }
.quando { font-family: var(--mono); font-size: 12px; letter-spacing: 1px; text-transform: uppercase; color: var(--tinta-2); }
ol.faz { padding-left: 22px; }
ol.faz li { margin: 8px 0; }
.aviso { border-left: 4px solid var(--alerta); background: var(--alerta-fundo); padding: 10px 14px; border-radius: 0 6px 6px 0; margin: 16px 0; max-width: 72ch; }
.nota { border-left: 4px solid var(--verde); background: var(--cartao); padding: 10px 14px; border-radius: 0 6px 6px 0; margin: 16px 0; max-width: 72ch; }
.confere { list-style: none; padding: 0; margin: 14px 0; display: grid; gap: 6px; }
.confere li { display: flex; gap: 10px; align-items: flex-start; }
.confere input { margin-top: 5px; width: 17px; height: 17px; accent-color: var(--verde); flex: none; }
.confere label { cursor: pointer; }
.confere input:checked + label { color: var(--tinta-2); text-decoration: line-through; text-decoration-color: var(--linha); }

@media (prefers-reduced-motion: reduce) { * { scroll-behavior: auto !important; } }
"""


def passo(num, ancora, titulo, quando, corpo, confere):
    itens = "".join(
        f"<li><input type='checkbox' id='ok-{ancora}-{i}' data-confere='{ancora}-{i}'><label for='ok-{ancora}-{i}'>{c}</label></li>"
        for i, c in enumerate(confere))
    return f"""
<section class="passo" id="{ancora}">
  <div class="passo-cab"><span class="passo-num">{num}</span><div><h2>{titulo}</h2><div class="quando">{quando}</div></div></div>
  {corpo}
  <h3>Conferir antes de seguir</h3>
  <ul class="confere">{itens}</ul>
</section>"""


# ---------------------------------------------------------------------------
# os desenhos
# ---------------------------------------------------------------------------
CAMADAS = [   # de trás para a frente: (espessura, nome, classe)
    (P.FUNDO, "fundo da caixa", "c-caixa"),
    (P.VAO_COMP, "componentes da LILYGO", "c-vao"),
    (P.L.espessura, "placa da LILYGO", "c-lilygo"),
    (P.L.pilha_altura, "pilha 18650 no suporte", "c-pilha"),
    (P.FOLGA_PILHA, "folga", "c-vao"),
    (P.PCB_ESP, "PCB da frente", "c-pcb"),
    (P.T.barra_plastico, "plástico dos pinos da tela", "c-vidro"),
    (P.T.pcb, "plaquinha da tela", "c-tela"),
    (P.T.lcd, "vidro da tela", "c-vidro"),
    (P.FOLGA_VIDRO + P.ARO, "folga e aro da frente", "c-caixa"),
]


def desenho_espessura():
    """A pilha de camadas, em escala, com o nome e a espessura de cada uma"""
    esc_ = 9.0                                   # px por mm
    x0, larg = 46, 96
    total = sum(c[0] for c in CAMADAS)
    assert abs(total - P.Z_TOPO) < 0.01, (total, P.Z_TOPO)
    topo = 26
    alt = total * esc_
    base = topo + alt
    pecas, rot = [], []
    z = 0.0
    for esp, nome, classe in CAMADAS:
        y1 = base - z * esc_
        y0 = base - (z + esp) * esc_
        pecas.append(f"<rect class='{classe}' x='{x0}' y='{y0:.1f}' width='{larg}' height='{y1 - y0:.1f}'/>")
        rot.append([(y0 + y1) / 2, esp, nome])
        z += esp
    # os rótulos, de cima para baixo, sem encavalar
    rot.sort()
    passo_min = 27.0
    ys = [r[0] for r in rot]
    for i in range(1, len(ys)):
        ys[i] = max(ys[i], ys[i - 1] + passo_min)
    sobra = ys[-1] - (base + 2)
    if sobra > 0:
        ys = [y - sobra for y in ys]
        for i in range(len(ys) - 2, -1, -1):
            ys[i] = min(ys[i], ys[i + 1] - passo_min)
    xr = x0 + larg + 34
    for (yc, esp, nome), yl in zip(rot, ys):
        pecas.append(f"<polyline class='guia' points='{x0 + larg + 2},{yc:.1f} {x0 + larg + 16},{yc:.1f} {xr - 6},{yl:.1f}'/>")
        pecas.append(f"<text x='{xr}' y='{yl + 4.5:.1f}' font-size='13'><tspan font-weight='600' "
                     f"font-family='IBM Plex Mono, monospace'>{mm(esp, 2 if esp == P.T.barra_plastico else 1)}</tspan>"
                     f"<tspan dx='8'>{esc(nome)}</tspan></text>")
    # a cota do total
    xc = x0 - 16
    pecas.append(f"<path class='cota' d='M{xc - 5},{topo} H{xc + 5} M{xc},{topo} V{base} M{xc - 5},{base} H{xc + 5}'/>")
    pecas.append(f"<text x='{xc - 7}' y='{(topo + base) / 2:.1f}' font-size='14' font-weight='600' text-anchor='middle' "
                 f"transform='rotate(-90 {xc - 7} {(topo + base) / 2:.1f})'>{mm(total)} mm</text>")
    pecas.append(f"<text class='fino' x='{x0 + larg / 2}' y='{topo - 9}' font-size='12' text-anchor='middle'>frente</text>")
    pecas.append(f"<text class='fino' x='{x0 + larg / 2}' y='{base + 17}' font-size='12' text-anchor='middle'>trás</text>")
    h = base + 26
    return (f"<svg class='dz' viewBox='0 0 400 {h:.0f}' role='img' aria-label='As camadas do Tijolo, de trás para a frente, "
            f"somando {mm(total)} milímetros'>{''.join(pecas)}</svg>")


def desenho_cabos():
    """O fundo visto de frente, sem a PCB: onde ficam o alto-falante, o microfone, a antena e por onde vão os cabos"""
    mx0, my0 = 8.0, 8.0                          # margens (mm)
    larg_txt = 52.0
    W = mx0 + LARGURA + larg_txt
    H = my0 * 2 + COMPRIMENTO

    def x(v):
        return mx0 + P.PAREDE + v

    def y(v):
        return my0 + P.PAREDE + (P.CAV_Y1 - v)

    def ret(x0, y0, x1, y1, classe, raio=0.0, extra=""):
        return (f"<rect class='{classe}' x='{x(x0):.2f}' y='{y(y1):.2f}' width='{x1 - x0:.2f}' height='{y1 - y0:.2f}' "
                f"rx='{raio}' {extra}/>")

    p = []
    # a caixa
    p.append(f"<rect class='c-caixa' x='{mx0}' y='{my0}' width='{LARGURA:.2f}' height='{COMPRIMENTO:.2f}' rx='5'/>")
    p.append(ret(P.CAV_X0, P.CAV_Y0, P.CAV_X1, P.CAV_Y1, "c-vao", 3.2))
    # a antena, colada na parede da esquerda
    p.append(ret(-0.2, 34.0, 0.9, 145.5, "c-som"))
    # a LILYGO e a pilha
    lx0, ly0, lx1, ly1 = P.caixa_lilygo(0, 0, P.L.largura, P.L.altura)
    p.append(ret(lx0, ly0, lx1, ly1, "c-lilygo", 2.0))
    hx0, hy0, hx1, hy1 = P.caixa_lilygo(*P.L.suporte_18650)
    p.append(ret(hx0, hy0, hx1, hy1, "c-caixa2", 1.0))
    p.append(ret(P.PILHA_X - P.L.pilha_raio, P.PILHA_Y[0], P.PILHA_X + P.L.pilha_raio, P.PILHA_Y[1], "c-pilha", 2.5))
    for barra in ("J1", "J2"):
        bx, by0 = P.pino_barra(barra, 1)
        _, by1 = P.pino_barra(barra, 16)
        p.append(ret(bx - 1.27, by0 - 1.27, bx + 1.27, by1 + 1.27, "c-vidro"))
    # os conectores do som (do lado da pilha) e o da antena (na face de baixo)
    sx0, sy0, sx1, sy1 = P.caixa_lilygo(*P.L.spek)
    qx0, qy0, qx1, qy1 = P.caixa_lilygo(*P.L.mic)
    p.append(ret(sx0, sy0, sx1, sy1, "c-pcb"))
    p.append(ret(qx0, qy0, qx1, qy1, "c-pcb"))
    ux, uy = P.lilygo_para_tijolo(*P.L.ufl_lte)
    p.append(f"<circle class='tracejado' cx='{x(ux):.2f}' cy='{y(uy):.2f}' r='1.6'/>")
    # o alto-falante e o microfone
    fy0 = P.FALANTE_Y + P.FALANTE_FOLGA
    p.append(ret(P.FALANTE_X0, fy0, P.FALANTE_X0 + P.FALANTE[0], fy0 + P.FALANTE[2], "c-som", 0.8))
    p.append(ret(P.MIC_X - P.MIC_D / 2, 0.1, P.MIC_X + P.MIC_D / 2, P.MIC_L, "c-som", 0.8))
    # a chave e o USB-C
    cy0, cy1 = P.L.chave[0], P.L.chave[1]
    kx0, ky0, kx1, ky1 = P.caixa_lilygo(P.L.chave_corpo_x[0], cy0, 0.0, cy1)
    p.append(ret(kx0, ky0, kx1, ky1, "c-vidro"))
    _, usb_y = P.lilygo_para_tijolo(0, P.L.usb_esp_y)
    p.append(ret(P.CAV_X1 - 1.6, usb_y - 4.5, P.CAV_X1 + P.PAREDE + 0.3, usb_y + 4.5, "c-vidro", 0.8))

    # os cabos
    def caminho(pts, classe):
        d = " ".join(f"{'M' if i == 0 else 'L'}{x(a):.2f},{y(b):.2f}" for i, (a, b) in enumerate(pts))
        return f"<path class='{classe}' d='{d}'/>"

    smx, smy = (sx0 + sx1) / 2, (sy0 + sy1) / 2
    mcx, mcy = (qx0 + qx1) / 2, (qy0 + qy1) / 2
    p.append(caminho([(P.FALANTE_X0 + 3.0, fy0), (12.5, fy0 - 5.0), (12.5, smy), (sx0, smy)], "cabo-som"))
    p.append(caminho([(P.MIC_X - 2.0, P.MIC_L), (9.0, 13.0), (9.0, mcy), (qx0, mcy)], "cabo-mic"))
    p.append(caminho([(0.9, 141.0), (6.0, 141.0), (6.0, ly1 + 3.0), (ux, ly1 + 3.0), (ux, uy)], "cabo-antena"))

    # os rótulos, à direita
    xt = mx0 + LARGURA + 5.0
    rotulos = [
        (fy0 + 3.0, P.FALANTE_X0 + P.FALANTE[0], "alto-falante", "em pé, virado para a grade"),
        (uy, ux + 1.6, "conector LTE", "na face de baixo da LILYGO"),
        (mcy, qx1, "conector MIC", "microfone"),
        (smy, sx1, "conector SPEK", "alto-falante"),
        (104.0, lx1, "LILYGO, de costas", "a pilha para a frente"),
        (usb_y, P.CAV_X1 + P.PAREDE + 0.3, "USB-C", "carregar e gravar"),
        ((ky0 + ky1) / 2, kx0 + 0.5, "chave liga/desliga", "alcançada por trás"),
        (40.0, P.CAV_X0 + 0.9, "antena 4G", "colada na parede da esquerda"),
        (3.0, P.MIC_X + P.MIC_D / 2, "microfone", "virado para os 3 furinhos"),
    ]
    rotulos.sort(key=lambda r: -r[0])
    ys = [y(r[0]) for r in rotulos]
    passo_min = 9.2
    for i in range(1, len(ys)):
        ys[i] = max(ys[i], ys[i - 1] + passo_min)
    sobra = ys[-1] - (H - my0 - 4.0)
    if sobra > 0:
        ys = [v - sobra for v in ys]
        for i in range(len(ys) - 2, -1, -1):
            ys[i] = min(ys[i], ys[i + 1] - passo_min)
    for (yv, xv, nome, sub), yl in zip(rotulos, ys):
        p.append(f"<polyline class='guia' style='stroke-width:.25' points='{x(xv):.2f},{y(yv):.2f} {xt - 3.5:.2f},{yl:.2f} "
                 f"{xt - 1:.2f},{yl:.2f}'/>")
        p.append(f"<text x='{xt:.2f}' y='{yl + 0.4:.2f}' font-size='3.5' font-weight='600'>{esc(nome)}</text>")
        p.append(f"<text class='fino' x='{xt:.2f}' y='{yl + 4.2:.2f}' font-size='3'>{esc(sub)}</text>")
    return (f"<svg class='dz' viewBox='0 0 {W:.1f} {H:.1f}' role='img' aria-label='O fundo da caixa visto de frente, com a "
            f"LILYGO, o alto-falante em cima, o microfone embaixo, a antena na parede da esquerda e o caminho dos cabos'>"
            f"{''.join(p)}</svg>")


def svg_da_pcb(lado):
    """O desenho de uma face da PCB (só as letras e as ilhas), do KiCad, com as cores trocadas"""
    with open(os.path.join(AQUI, "saida", "placa", f"tijolo-pcb-{lado}-letras.svg"), encoding="utf-8") as f:
        s = f.read()
    cores = {"#D864FF": "#B8791F", "#02FFEE": "#B8791F",        # as ilhas (onde vai solda)
             "#F2EDA1": "#1E2B24", "#E8B2A7": "#1E2B24",        # as letras
             "#D0D2CD": "#6B766F"}                               # a borda
    for de, para in cores.items():
        s = s.replace(de, para)
    # sem o cabeçalho de XML antigo (<!DOCTYPE ...>): o artifact não aceita, e os navegadores não precisam
    s = re.sub(r"<!DOCTYPE[^>]*>\s*", "", s, count=1)
    face = "a face da frente" if lado == "frente" else "a face de trás, vista por trás"
    s = re.sub(r"<title>.*?</title>", f"<title>Tijolo, PCB da frente: {face}</title>", s, count=1, flags=re.S)
    return s


# ---------------------------------------------------------------------------
# as tabelas
# ---------------------------------------------------------------------------
def tabela(cab, linhas, classes=None):
    classes = classes or [""] * len(cab)
    th = "".join(f"<th class='{c}'>{h}</th>" for h, c in zip(cab, classes))
    corpo = ""
    for ln in linhas:
        cls = ""
        if isinstance(ln, dict):
            cls, ln = f" class='{ln['classe']}'", ln["celulas"]
        corpo += f"<tr{cls}>" + "".join(f"<td class='{c}'>{v}</td>" for v, c in zip(ln, classes)) + "</tr>"
    return f"<div class='rolagem'><table><thead><tr>{th}</tr></thead><tbody>{corpo}</tbody></table></div>"


def tabela_ligacoes():
    def pino(gpio):
        b, n = P.barra_de(gpio)
        return f"{b}-{n}"
    linhas = []
    for nome, gpio in P.TELA_GPIO.items():
        if gpio in ("3V3", "GND"):
            onde = "J1-1 e J2-3" if gpio == "3V3" else "J1-2, J2-4 e J2-15"
            linhas.append((f"Tela: {esc(nome)}", onde, gpio))
        else:
            linhas.append((f"Tela: {esc(nome)}", pino(gpio), gpio))
    for k, gpio in enumerate(P.TECLADO_ABA, start=1):
        if gpio:
            linhas.append((f"Aba do teclado: furo {k}", pino(gpio), gpio))
    linhas.append(("Botão verde", pino(P.GPIO_VERDE), P.GPIO_VERDE))
    linhas.append(("Botão vermelho", pino(P.GPIO_VERMELHO), P.GPIO_VERMELHO))
    return tabela(["Na PCB", "Pino da LILYGO", "ESP32"], linhas, ["", "mono", "mono"])


def pagina():
    ganho = ANTES["espessura"] - P.Z_TOPO
    aba_botao = P.BOTAO_ABA
    # como o teclado fica em relação às bordas da PCB (é por elas que se acerta na hora de colar)
    sobra_esq = P.TECLADO_X0 - P.PCB_X0
    sobra_dir = P.PCB_X1 - (P.TECLADO_X0 + P.K.largura)

    def lado_teclado(sobra, lado):
        if abs(sobra) < 0.06:
            return f"o lado {lado} fica rente à borda da PCB"
        if sobra > 0:
            return f"do lado {lado} sobram {mm(sobra)} mm de PCB"
        return f"do lado {lado} o teclado passa {mm(-sobra)} mm da PCB"
    lados_teclado = f"{lado_teclado(sobra_esq, 'esquerdo')} e {lado_teclado(sobra_dir, 'direito')}"
    sem_suporte = P.Z_TOPO - (P.Z_PCB - (P.Z_FEMEA1 + 0.1))        # a PCB logo acima das barras fêmea comuns

    indice = [("conferir", "", "As suas medidas"), ("medidas", "", "As medidas revisadas"), ("espessura", "", "A espessura e a PCB"),
              ("comprar", "", "O que comprar"), ("pcb", "", "Encomendar a PCB"), ("imprimir", "", "Imprimir a caixa"),
              ("cabos", "", "Cabos e fios"),
              ("lilygo", "1", "Preparar a LILYGO"), ("pecas", "2", "As peças da PCB"), ("tela", "3", "A tela"),
              ("barras", "4", "As barras fêmea"), ("teclado", "5", "O teclado"), ("som", "6", "Som e antena"),
              ("juntar", "7", "Juntar e testar"), ("fechar", "8", "Fechar"),
              ("problemas", "", "Se algo não funcionar"), ("fontes", "", "De onde vêm as medidas")]
    nav = "".join(f"<a href='#{a}'>{f'<span class=num>{n}</span>' if n else ''}{t}</a>" for a, n, t in indice)

    # ------------------------------------------------------------------ os passos
    passos = []
    passos.append(passo(1, "lilygo", "Preparar a LILYGO", "15 minutos · sem pilha e sem USB", f"""
<p>A LILYGO fica no fundo da caixa, de costas: os componentes olham para trás, a pilha e os pinos para a frente. Os pinos
continuam como você soldou, com o plástico do lado da pilha.</p>
<ol class="faz">
  <li>Tire a pilha e o cabo USB.</li>
  <li><b>Tire o conector SOLAR.</b> É o conector branco de 2 pinos ao lado da chave liga/desliga, do lado dos componentes. Ele
  tem 6,25 mm de altura e embaixo da LILYGO sobram {mm(P.VAO_COMP)} mm. Corte o corpo de plástico com o alicate de corte e depois
  os dois pinos, rente à placa. Não precisa dessoldar. O painel solar não é usado no Tijolo.</li>
  <li>Ponha o chip (nano-SIM) no suporte, se ainda não estiver lá.</li>
  <li>Encaixe as duas <b>barras fêmea de perna longa</b> nos pinos da LILYGO, até encostarem no plástico preto. As pernas
  compridas ficam apontando para cima.</li>
</ol>""",
        ["O conector SOLAR saiu e não sobrou ponta alta no lugar dele.",
         "O chip está no suporte.",
         "As duas barras fêmea estão encaixadas até o fim, uma em cada fileira de 16 pinos."]))

    passos.append(passo(2, "pecas", "As peças pequenas da PCB", "30 minutos", f"""
<p>A face da frente da PCB tem escrito <b>TIJOLO</b>. A de trás, que fica virada para a pilha, tem escrito
<b>ESTA FACE PARA A PILHA</b>. Cada peça tem o lugar marcado na placa.</p>
<ol class="faz">
  <li><b>Os capacitores vão atrás</b> e a solda é feita pela frente. Os de cerâmica não têm lado: C1 (100 nF) e, se você tiver,
  C5 (100 nF) e C4 (100 pF). Encoste o corpo na placa.</li>
  <li><b>C2, o de 100 µF</b>, tem lado: a perna comprida (o +) entra no furo quadrado, marcado com +. A faixa clara do corpo
  fica do lado do outro furo. O corpo fica em pé, encostado na placa, atrás.</li>
  <li><b>As duas chaves tácteis vão na frente</b>: uma no retângulo VERDE, outra no VERMELHO. As 4 pernas só entram de um
  jeito. Solde atrás.</li>
  <li><b>Corte todas as pernas rente.</b> Atrás da placa, nenhuma pode passar de 1,5 mm: a chave vermelha fica logo acima da
  pilha.</li>
</ol>
<div class="nota">C1 e C2 seguram a alimentação da tela. C4 e C5 são um reforço contra a tela branca nas ligações. Se você não
tiver o de 100 pF, deixe o lugar do C4 vazio: funciona sem ele.</div>""",
        ["C2 está com o + no furo quadrado.",
         "Os capacitores estão na face de trás e as chaves na da frente.",
         "Atrás da placa, nenhuma perna passa de 1,5 mm."]))

    passos.append(passo(3, "tela", "A tela", "20 minutos", f"""
<p>A tela é soldada na PCB pelos pinos dela, com os pinos para baixo: a posição normal. O plástico amarelo dos pinos
encosta na PCB e serve de calço. Só os 9 primeiros pinos são usados; os outros 5 são do toque, que esta tela não tem.</p>
<ol class="faz">
  <li><b>Corte 4 pinos da tela</b>: contando a partir do VCC, o 10º, o 11º, o 12º e o 13º (T_CLK, T_CS, T_DIN e T_DO). Corte
  rente ao plástico amarelo. Eles cairiam bem em cima da pilha. O último pino (T_IRQ) fica.</li>
  <li>Encaixe a tela pela frente da PCB, na fileira de furos de cima. O VCC entra no furo quadrado, marcado com
  <b>1</b>. Os toquinhos dos 4 pinos cortados entram nos 4 furos marcados com X. O vidro fica virado para você.</li>
  <li>Segure a tela reta, com o plástico amarelo encostado na PCB, e solde atrás os 10 pinos: do 1 ao 9 e o último.
  <b>Nos 4 furos com X não vai solda.</b></li>
  <li>Corte as sobras atrás, deixando no máximo 1,5 mm.</li>
  <li>Cole um pedaço de fita isolante atrás da PCB, por cima dos 4 furos sem solda.</li>
</ol>
<div class="aviso">Daqui em diante, pegue o conjunto pela PCB, não pela tela: ela fica presa só pelos pinos até entrar na caixa,
onde a ponta de cima apoia em duas torres.</div>""",
        ["Os 4 pinos foram cortados e nada aparece nos furos com X, atrás da PCB.",
         "O VCC da tela está no furo quadrado (1).",
         "A tela está reta, com o plástico amarelo encostado na PCB.",
         "Atrás, os pinos soldados não passam de 1,5 mm."]))

    passos.append(passo(4, "barras", "As barras fêmea", "20 minutos · sem pilha", f"""
<p>As barras são soldadas com tudo no lugar. Assim a PCB fica na altura certa, apoiada nos pilares do fundo.</p>
<div class="nota">As pernas das suas barras têm {mm(P.FEMEA_PERNA)} mm. Com a PCB no lugar, a ponta de cada perna passa só uns
{mm(P.FEMEA_SOBRA)} mm da face da frente. É pouco, mas basta: os furos têm metal por dentro e a solda desce por eles.</div>
<ol class="faz">
  <li>Ponha a LILYGO no fundo impresso, com as barras fêmea encaixadas e <b>sem a pilha</b>. A ponta da ESP32 fica para baixo
  (o lado do microfone) e os USB-C, para a direita. Dois pilares do fundo têm pino, que entra nos furos dela. Os parafusos
  ficam para o passo 6.</li>
  <li>Desça a PCB: as 32 pernas entram nas duas fileiras de furos (J1 e J2). A PCB apoia nos três pilares da esquerda e nas
  nervuras de baixo e da direita. A ponta de cima da tela apoia nas duas torres, ao lado do lugar do alto-falante.</li>
  <li>Solde, pela frente, a <b>primeira e a última perna de cada barra</b>: 4 soldas. Encoste o ferro na ponta da perna e na
  ilha ao mesmo tempo e deixe a solda entrar no furo. Cuidado para não encostar o ferro na borda da caixa.</li>
  <li>Tire tudo do fundo e separe a PCB da LILYGO, puxando reto: as barras saem presas na PCB. Solde as outras 28 pernas na
  bancada.</li>
  <li>O teclado vai por cima dessas soldas. Com as suas barras não sobra ponta para cortar; se alguma passar de 1 mm, corte
  rente.</li>
  <li>Cole uma tira de fita isolante por cima de cada fileira de soldas.</li>
</ol>""",
        ["A PCB estava assentada nos pilares e a tela, nas torres, quando as 4 primeiras pernas foram soldadas.",
         "As 32 pernas estão soldadas, com a solda fechando a volta de cada ponta, e nenhuma passa de 1 mm.",
         "As duas fileiras de soldas estão cobertas com fita isolante."]))

    passos.append(passo(5, "teclado", "O teclado", "20 minutos", f"""
<p>O teclado entra inteiro, sem abrir. Ele fica colado na PCB com fita dupla face de espuma, e a aba verde dos contatos se liga
à PCB por 7 perninhas de fio.</p>
<ol class="faz">
  <li>Cole a fita de espuma nos dois retângulos marcados FITA DE ESPUMA e tire o papel de cima.</li>
  <li>Segure o teclado acima da PCB, sem encostar, com a aba verde para baixo, sobre a língua da PCB. Acerte a posição: a
  borda de cima do teclado fica na linha marcada <b>BORDA DE CIMA DO TECLADO</b>; {lados_teclado}. Os 9 furos da aba caem em
  cima dos 9 furos da PCB.</li>
  <li>Encoste e aperte para colar. A fita não deixa corrigir depois.</li>
  <li>Passe uma perninha de fio nu (uns 8 mm; perna de resistor serve) em cada um dos <b>7 furos do meio</b>, atravessando a
  aba e a PCB. Solde em cima, na aba, e embaixo, atrás da PCB. Corte as sobras.</li>
</ol>
<div class="nota">Os dois furos das pontas não ligam em nada: pode soldar também, só para firmar. A ordem dos 7 fios não
importa, porque o Tijolo aprende qual é qual (passo 7). Se os furos da aba ficarem até 1 mm acima ou abaixo dos da PCB, não tem
problema: as perninhas de fio acompanham.</div>""",
        [f"A borda de cima do teclado está na linha da PCB; {lados_teclado}.",
         "As 7 perninhas do meio estão soldadas na aba e atrás da PCB.",
         "Nenhuma perninha encosta na vizinha."]))

    passos.append(passo(6, "som", "Alto-falante, microfone e antena", "40 minutos", f"""
<p>Os três ficam no fundo da caixa. O desenho em <a href="#cabos">Cabos e fios</a> mostra o lugar de cada um e o caminho dos
cabos.</p>
<ol class="faz">
  <li><b>Antena.</b> Cole a antena na parede da esquerda, por dentro, no rebaixo raso, com o cabo saindo por cima. Encaixe a
  ponta do cabo no conector <b>LTE</b> da LILYGO: fica do lado dos componentes, na ponta do modem. Aperte reto até fazer
  clic.</li>
  <li><b>Alto-falante.</b> O conector que vem nele não serve na LILYGO. Corte o conector e emende os dois fios num cabo JST SH
  1.0, deixando uns 6 cm no total. Isole as emendas com fita isolante. Ponha o alto-falante em pé na calha de cima, apoiado
  no lado comprido, com a frente virada para a grade. Se ficar frouxo, cole uma tirinha de fita isolante atrás dele.</li>
  <li><b>Microfone.</b> Encaixe um cabo JST SH 1.0 no conector MIC e veja qual fio cai no pino marcado com + na placa. Esse
  fio vai no terminal positivo do microfone: o que <b>não</b> está ligado à carcaça. O outro vai no terminal da carcaça. O
  caminho pede uns 15 cm e o cabo tem 10: emende 6 cm de fio no meio. Deite o microfone no berço de baixo, com a face de feltro
  virada para os três furinhos.</li>
  <li>Ponha a LILYGO no fundo, descendo reto nos dois pinos, e aperte os 2 parafusos M2 × 5 nos outros dois pilares, só até
  encostar. O cabo da antena dá a volta pela ponta de cima da placa.</li>
  <li>Encaixe os cabos nos conectores <b>SPEK</b> e <b>MIC</b>, do lado da pilha. Os cabos correm pelo vão da esquerda.</li>
  <li>Ponha a chave liga/desliga em <b>ON</b>. Na LILYGO está escrito ON e OFF ao lado dela, do lado da pilha.</li>
</ol>""",
        ["A antena está colada na parede e encaixada no conector LTE (não no GNSS).",
         "O alto-falante está na calha, virado para a grade, e o cabo dele no SPEK.",
         "O fio do + do conector MIC está no terminal do microfone que não toca a carcaça.",
         "Nenhum cabo passa por cima dos pilares nem das torres."]))

    passos.append(passo(7, "juntar", "Juntar e testar", "30 minutos · com a pilha", f"""
<div class="aviso">Confira a antena antes de pôr a pilha: o modem não pode ligar sem ela.</div>
<ol class="faz">
  <li>Ponha a pilha no suporte, conferindo o + e o − gravados nele.</li>
  <li>Encaixe o conjunto da PCB (com a tela e o teclado) nos pinos da LILYGO. Desça reto e aperte sobre o teclado, sem
  pressa, até a PCB assentar nos pilares e a ponta da tela, nas torres.</li>
  <li>No arquivo <code>include/config.h</code>, troque para <code>#define MONTAGEM_PLACA 2</code> e grave o firmware pelo
  USB-C da abertura da direita.</li>
  <li>Ensine o teclado: segure o 1 enquanto aparece "Ligando o modem…" e aperte as teclas que a tela pedir. (Ou mande
  <code>teclado aprender</code> no Monitor Serial.)</li>
  <li>Teste antes de fechar: as 12 teclas, o verde e o vermelho, uma ligação (para ouvir e falar) e uma mensagem.</li>
</ol>
<div class="nota">O vermelho segurado desliga o Tijolo; segurado de novo por 1 segundo, liga. A chave da LILYGO só precisa ser
desligada para guardar o Tijolo por muitos dias: ela fica atrás, na fenda com ON e OFF, e se mexe com um palito.</div>""",
        ["A tela acende com a imagem em pé.",
         "As 12 teclas e os 2 botões respondem.",
         "Numa ligação, você ouve e é ouvido."]))

    passos.append(passo(8, "fechar", "Fechar", "10 minutos", f"""
<ol class="faz">
  <li>Ponha os dois botões impressos nos poços da frente, por dentro: o verde do lado esquerdo de quem olha o Tijolo de frente.
  Prenda cada um com um pedacinho de fita por fora, só para não caírem.</li>
  <li>Desça a frente reta sobre o fundo. As duas linguetas de cima entram por dentro da parede e fazem clic. O teclado passa pela
  abertura.</li>
  <li>Aperte os 2 parafusos M2 × 8 por trás, embaixo, sem forçar. Tire as fitas dos botões.</li>
</ol>
<div class="nota">Para abrir: tire os 2 parafusos, levante a frente por baixo e puxe para soltar as linguetas. Para trocar a
pilha, puxe o conjunto da PCB reto para cima: ele desencaixa dos pinos e a LILYGO fica presa no fundo.</div>""",
        ["As linguetas fizeram clic e não sobrou fresta em cima.",
         "Os dois botões fazem clic quando apertados e voltam sozinhos.",
         "O cabo USB-C entra até o fim pela abertura da direita."]))

    # ------------------------------------------------------------------ as medidas que o Felipe mandou
    falta_falante = "falante" in P.FALTA_MEDIR
    falta_html = "" if not falta_falante else f"""<div class="aviso"><b>Confira antes de imprimir: o lado curto do alto-falante.</b>
Você mandou {mm(P.FALANTE[0])} mm como altura. Eu li como o lado comprido, o mesmo que antes tinha dado 35,1, e usei o maior dos
dois. O outro lado, que pelo nome da peça (3525) tem uns {mm(P.FALANTE[1], 0)} mm, eu não tenho medido. O alto-falante fica em pé,
apoiado no lado comprido, e cabe se o lado curto tiver até {P.FALANTE_ALTURA_MAX} mm. Se o seu tiver uns 35 mm dos dois lados,
não imprima: me avise, que eu mudo o lugar dele.</div>"""
    suas_medidas = tabela(
        ["O que você mediu", "Sua medida", "O que mudou nos arquivos"],
        [("Teclado: largura do corpo", f"{mm(P.K.largura)} mm",
          f"Na faixa. Sobra {mm(P.FOLGA_TECLADO)} mm de cada lado na abertura da frente; a largura da caixa não mudou."),
         ("Teclado: comprimento, sem a aba", f"{mm(P.K.altura)} mm",
          f"Na faixa. É {mm(SEM_MEDIR['teclado_altura'] - P.K.altura)} mm mais curto que no desenho: na PCB, os 9 furos da aba "
          f"subiram esse tanto, e a abertura da frente encolheu o mesmo."),
         ("Teclado: espessura", f"{mm(P.K.espessura)} mm",
          f"Na faixa. As teclas passam {mm(P.Z_TECLAS - P.Z_TOPO)} mm da frente da caixa (eram {mm(SEM_MEDIR['teclas'])})."),
         {"classe": "fora", "celulas": (
          "Pilha na LILYGO: da placa até o topo", f"<b>{mm(P.L.pilha_altura)} mm</b>",
          f"<b>Fora da faixa</b>: eu tinha reservado {mm(SEM_MEDIR['pilha'])}. A PCB, a tela e a frente subiram "
          f"{mm(P.L.pilha_altura - SEM_MEDIR['pilha'])} mm, e a caixa passou de {mm(SEM_MEDIR['espessura'])} para "
          f"{mm(P.Z_TOPO)} mm de espessura. Entre a pilha e a PCB continuam {mm(P.FOLGA_PILHA)} mm de folga.")},
         ("Tela: VCC à esquerda", "sim (você escreveu 1)", "Nada muda."),
         ("Alto-falante: lado comprido e espessura (25/11)", f"{mm(P.FALANTE[0])} e {mm(P.FALANTE[2])} mm",
          f"Na faixa. A calha ficou com {mm(P.FALANTE[0] + 2 * P.FALANTE_FOLGA)} × {mm(P.FALANTE[2] + 2 * P.FALANTE_FOLGA)} mm: "
          f"{mm(P.FALANTE_FOLGA)} mm de folga de cada lado."
          + (" <b>Falta conferir o lado curto.</b>" if falta_falante else "")),
         ("Chave táctil: altura", f"{mm(P.CHAVE_TACTIL_ALTURA)} mm",
          f"Na faixa. A aba do botão impresso passou de {mm(SEM_MEDIR['aba_botao'])} para {mm(aba_botao)} mm, para o botão "
          f"não ficar frouxo."),
         ("Barra fêmea: perna", f"{mm(P.FEMEA_PERNA)} mm",
          f"Serve, no limite: o mínimo é {P.PERNA_MIN_TXT}. A ponta da perna passa {mm(P.FEMEA_SOBRA)} mm da PCB, o que basta "
          f"para soldar (<a href='#barras'>passo 4</a>).")])

    medidas_revisadas = tabela(
        ["Peça", "O que eu uso", "Conferido com"],
        [("LILYGO",
          f"placa de {mm(P.L.largura, 2)} × {mm(P.L.altura, 2)} × {mm(P.L.espessura)} mm; as duas barras de 16 pinos a "
          f"{mm(P.L.barra_x[1] - P.L.barra_x[0], 2)} mm uma da outra, com o plástico do lado da pilha; 4 furos de Ø "
          f"{mm(P.L.furo_fixacao_d, 2)}; USB-C com {mm(P.L.usb_altura, 2)} mm de altura; conector SOLAR com 6,25 (por isso sai)",
          "modelo 3D oficial, desenho com cotas (DXF) e foto oficial do lado da pilha"),
         ("Pilha no suporte",
          f"suporte com {mm(P.L.suporte_altura)} mm; o topo da pilha fica a {mm(P.L.pilha_altura)} mm da placa",
          "<b>a sua medida</b> (pelo modelo 3D oficial eu esperava 19 ou menos)"),
         ("Tela 2,4\"",
          f"plaquinha de {mm(P.T.largura, 2)} × {mm(P.T.altura, 2)} mm; os pinos embaixo, a {mm(P.T.pinos_v, 2)} mm da borda, com "
          f"o VCC à esquerda; vidro de {mm(P.T.vidro_v[0], 2)} a {mm(P.T.vidro_v[1], 2)} mm; imagem de "
          f"{mm(P.T.imagem_u[1] - P.T.imagem_u[0], 2)} × {mm(P.T.imagem_v[1] - P.T.imagem_v[0], 2)} mm; soquete do cartão SD "
          f"atrás, com {mm(P.T.sd_altura)} mm",
          "desenho do fabricante (LCDWiki MSP2401)"),
         ("Teclado 3×4",
          f"corpo de {mm(P.K.largura)} × {mm(P.K.altura)} mm; aba de {mm(P.K.aba[0])} × {mm(P.K.aba[1])} mm com 9 furos a 2,54; "
          f"{mm(P.K.espessura)} mm de espessura",
          "<b>as suas medidas</b> (corpo e espessura); a aba, pela foto com cotas do anúncio"),
         ("Alto-falante e microfone",
          f"{mm(P.FALANTE[0])} × {mm(P.FALANTE[1], 0)} × {mm(P.FALANTE[2])} mm; Ø {mm(P.MIC_D)} × {mm(P.MIC_L)} mm",
          "o lado comprido e a espessura do alto-falante são <b>as suas medidas</b>; o resto, pelos nomes das peças (3525 e "
          "9767)" + ("; <b>falta conferir o lado curto do alto-falante</b>" if falta_falante else "")),
         ("Chaves tácteis e barras fêmea",
          f"chave com {mm(P.CHAVE_TACTIL_ALTURA)} mm de altura; barra com perna de {mm(P.FEMEA_PERNA)} mm e corpo de "
          f"{mm(P.FEMEA_ALTURA)} mm",
          "<b>as suas medidas</b> (a altura do corpo da barra é a comum)")])

    camadas = tabela(
        ["Montagem", "Espessura", "Fios soltos"],
        [("Placa ilhada e fios (o desenho anterior)", f"{mm(ANTES['espessura'])} mm", str(ANTES["fios"])),
         {"classe": "total", "celulas": ("<b>PCB da frente (esta)</b>", f"<b>{mm(P.Z_TOPO)} mm</b>", "<b>nenhum</b>")},
         ("PCB e bateria chata no lugar da 18650", f"uns {mm(sem_suporte, 0)} mm", "nenhum")],
        ["", "num", "num"])

    comprar = tabela(
        ["Peça", "Quantas", ""],
        [(f"<b>PCB da frente</b> ({mm(PCB_L)} × {mm(PCB_A)} mm, 2 camadas, <b>0,8 mm</b>): veja <a href='#pcb'>Encomendar a PCB</a>",
          "5 (o mínimo)", "<span class='comprar'>encomendar</span>"),
         (f"<b>Barra fêmea de perna longa</b> (\"empilhável\", a dos shields de Arduino), 2,54 mm, 1 × 16. Perna de "
          f"{P.PERNA_MIN_TXT} mm ou mais: as suas, de {mm(P.FEMEA_PERNA)}, servem. Quatro de 1 × 8 também servem.",
          "2", "<span class='tem'>tem</span>"),
         ("<b>Fita dupla face de espuma</b>, de 1 a 1,5 mm de espessura", "20 cm", "<span class='comprar'>comprar</span>"),
         ("Capacitor cerâmico de 100 pF (C4, opcional)", "1", "<span class='comprar'>se quiser</span>"),
         ("Fita isolante comum (ou Kapton)", "um pouco", "<span class='comprar'>se não tiver</span>"),
         ("Impressão 3D: fundo, frente e dois botões: veja <a href='#imprimir'>Imprimir a caixa</a>", "1 jogo",
          "<span class='comprar'>encomendar</span>"),
         (f"Chave táctil 6 × 6 mm, com {mm(P.CHAVE_TACTIL_ALTURA)} mm de altura (kit Eletrogate)", "2",
          "<span class='tem'>tem</span>"),
         ("Capacitores: 100 µF e 100 nF (kit Eletrogate)", "1 + 2", "<span class='tem'>tem</span>"),
         ("Parafusos M2 de cabeça chata: 2 de 8 mm (fechar a caixa) e 2 de 5 mm (prender a LILYGO). Veja se o seu kit "
          "sortido tem esses comprimentos.", "4", "<span class='tem'>tem</span>"),
         ("Cabo JST SH 1.0 de 2 vias (alto-falante e microfone)", "2", "<span class='tem'>tem</span>"),
         ("Fio 30 AWG (emenda do microfone) e pernas de resistor (aba do teclado)", "um pouco", "<span class='tem'>tem</span>"),
         ("Teclado 3×4, tela 2,4\", alto-falante 3525, microfone 9767, antena 4G, pilha 18650", "", "<span class='tem'>tem</span>")])

    imprimir = tabela(
        ["Arquivo", "O que é", "Como imprimir"],
        [("<span class='mono'>fundo.stl</span>", f"a metade de trás ({mm(LARGURA)} × {mm(COMPRIMENTO)} × {mm(P.Z_TELA - 0.05)} mm)",
          "a face de trás na mesa; sem suporte"),
         ("<span class='mono'>frente.stl</span>", "a metade da frente, com a janela da tela e a abertura do teclado",
          "a face da frente na mesa (já está virada); sem suporte"),
         ("<span class='mono'>botao_verde.stl</span>", "o botão verde (✓)", "em pé, como está; filamento verde, ou pinte"),
         ("<span class='mono'>botao_vermelho.stl</span>", "o botão vermelho (✕)", "em pé, como está; filamento vermelho, ou pinte")])

    cabos = tabela(
        ["Ligação", "Com o quê", "Comprimento"],
        [("Alto-falante → conector SPEK", "cabo JST SH 1.0, emendado nos dois fios do alto-falante", "uns 6 cm"),
         ("Microfone → conector MIC", "cabo JST SH 1.0 soldado no microfone, com 6 cm de fio 30 AWG emendados no meio", "uns 15 cm"),
         ("Antena 4G → conector LTE", "o cabo que já vem na antena", "o que vier"),
         ("Aba do teclado → PCB", "7 perninhas de fio nu (pernas de resistor)", "8 mm cada"),
         ("Tela, botões e teclado → LILYGO", "trilhas da PCB e as duas barras de 16 pinos", "nenhum fio"),
         ("Carregar e gravar", "cabo USB-C comum, pela abertura da direita", "")])

    corpo = f"""
<title>Montagem do Tijolo</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Condensed:wght@600;700&family=IBM+Plex+Mono:wght@400;500&family=IBM+Plex+Sans:ital,wght@0,400;0,600;1,400&display=swap">
<style>{CSS}</style>
<div class="pagina" lang="pt-BR">
<nav class="indice" aria-label="Índice">{nav}</nav>
<main>
<header>
  <div class="eyebrow">Tijolo · montagem com a PCB da frente</div>
  <h1>Montagem do Tijolo</h1>
  <p class="lead">Uma PCB encomendada encaixa direto nos pinos da LILYGO e leva a tela, os dois botões e a ligação do teclado.
  Sobram dois cabos, o do alto-falante e o do microfone, e a antena. A caixa impressa em 3D fecha tudo.</p>
  <ul class="fichas">
    <li>caixa {mm(LARGURA)} × {mm(COMPRIMENTO)} × {mm(P.Z_TOPO)} mm</li><li>PCB {mm(PCB_L)} × {mm(PCB_A)} mm · 0,8 mm</li>
    <li>2 cabos e a antena</li><li>7 perninhas de fio</li><li>umas 3 horas de bancada</li>
  </ul>
  <div class="vista" id="vista">
    <img src="capa.png" alt="O Tijolo montado, visto de frente e de lado: a tela em cima, os botões verde e vermelho e o teclado embaixo." id="vista-capa">
    <div class="dica" id="vista-dica" hidden>arraste para girar</div>
    <div class="controles" id="vista-controles" role="group" aria-label="Vista 3D" hidden>
      <button class="chip" type="button" data-modo="fechado" aria-pressed="true">Fechado</button>
      <button class="chip" type="button" data-modo="aberto" aria-pressed="false">Sem a frente</button>
      <button class="chip" type="button" data-modo="dentro" aria-pressed="false">Só o fundo e a LILYGO</button>
      <button class="chip" type="button" data-modo="corte" aria-pressed="false">Cortado ao meio</button>
    </div>
  </div>
  <p class="pequeno">A caixa em 3D. O miolo aparece em blocos simples: a LILYGO com a pilha, a PCB da frente, a tela, o teclado, o
  alto-falante e o microfone.</p>
</header>

<h2 id="conferir">As suas medidas</h2>
<p>Refiz a PCB e a caixa com as medidas que você mandou em {P.MEDIDO_EM}, e a calha do alto-falante com as de 25/11. Só a
pilha estava fora da faixa: ela chega a
{mm(P.L.pilha_altura)} mm da placa e eu tinha reservado {mm(SEM_MEDIR['pilha'])}. Tudo o que fica acima dela subiu
{mm(P.L.pilha_altura - SEM_MEDIR['pilha'])} mm.</p>
{suas_medidas}
{falta_html}
<p>Li o "1" que você escreveu na pergunta da tela como "sim": de frente, com os pinos para baixo, o VCC é o pino da esquerda.
Se não for isso, me avise antes de encomendar a PCB.</p>

<h2 id="medidas">As medidas revisadas</h2>
<p>As medidas vêm dos desenhos dos fabricantes; onde você mediu a peça, vale a sua medida. Três coisas mudaram em relação ao
desenho anterior, o da placa ilhada:</p>
<ul>
  <li><b>A tela fica com os pinos para baixo</b>, que é a posição normal dela: a imagem não precisa girar. O vidro não fica no
  meio da plaquinha, e o soquete do cartão SD, atrás, tem {mm(P.T.sd_altura)} mm de altura.</li>
  <li><b>O teclado entra inteiro.</b> Não precisa abrir nem medir por dentro. A caixa ficou da largura dele.</li>
  <li><b>Nenhuma solda fica em cima da pilha.</b> Por isso os 4 pinos do toque da tela são cortados e os botões ficam perto das
  bordas.</li>
</ul>
{medidas_revisadas}

<h2 id="espessura">A espessura e a PCB</h2>
<p>Vale a pena encomendar a PCB. Ela tira {mm(ganho)} mm da espessura e acaba com os {ANTES["fios"]} fios soltos. O que manda
na espessura, porém, é a pilha: a LILYGO com a 18650 no suporte soma {mm(P.L.espessura + P.L.pilha_altura)} mm, mais da metade do
total.</p>
<p>De frente, a caixa cresceu: era {mm(ANTES["largura"])} × {mm(ANTES["comprimento"])} mm com as 14 chaves soldadas e passou a
{mm(LARGURA)} × {mm(COMPRIMENTO)} mm. É o preço de usar o teclado pronto, que tem {mm(P.K.largura)} mm de largura e fica embaixo
da tela.</p>
<div class="lado-a-lado">
  <figure style="margin:0"><div class="desenho-caixa">{desenho_espessura()}</div>
  <figcaption>As camadas, de trás para a frente, em escala. As teclas passam {mm(P.Z_TECLAS - P.Z_TOPO)} mm da frente da
  caixa.</figcaption></figure>
  <div>
    {camadas}
    <p>Para afinar mais, só trocando a 18650 por uma bateria chata de lítio. Isso pede dessoldar o suporte da pilha da LILYGO e
    soldar os fios da bateria nova. Não recomendo agora: é mexer na parte mais delicada da placa para ganhar uns
    {mm(P.Z_TOPO - sem_suporte, 0)} mm. Fica como um passo para depois, com o Tijolo já funcionando.</p>
    <p>A sua pilha chega a {mm(P.L.pilha_altura)} mm da placa, {mm(P.L.pilha_altura - SEM_MEDIR['pilha'])} mm a mais do que
    eu tinha reservado. Por isso a caixa passou de {mm(SEM_MEDIR['espessura'])} para {mm(P.Z_TOPO)} mm.</p>
  </div>
</div>

<h2 id="comprar">O que comprar</h2>
{comprar}

<h2 id="pcb">Encomendar a PCB</h2>
<p>O arquivo é o <span class="mono">tijolo-pcb-{ETIQUETA}-gerbers-jlcpcb.zip</span>, que mandei na conversa. Ele também está
no repositório, em <code>montagem/fabricar/pcb/</code>. <b>Use este, com {ETIQUETA} no nome</b>: foi feito com as suas medidas,
e na placa vem escrito <b>{VERSAO_PCB}</b>. No anterior, sem {ETIQUETA} no nome, os furos da aba do teclado ficavam 0,5 mm mais
abaixo: apague-o. Na JLCPCB (ou parecida), envie o zip do jeito que está e mude só a espessura:</p>
<ul>
  <li>2 camadas, material FR-4;</li>
  <li><b>espessura da placa: 0,8 mm</b> (o padrão é 1,6; com 1,6 a frente da caixa não fecha);</li>
  <li>quantidade 5, qualquer cor, acabamento HASL sem chumbo.</li>
</ul>
<figure>
  <div class="duas">
    <div class="prancha"><div class="titulo">A frente</div><img src="pcb-frente.svg" alt="A face da frente da PCB: em cima a fileira de furos da tela, com os nomes dos pinos, e as duas chaves, VERDE e VERMELHO; no meio as duas fileiras J1 e J2 e os dois retângulos da fita de espuma; embaixo os 9 furos da aba do teclado."></div>
    <div class="prancha"><div class="titulo">Atrás (virada)</div><img src="pcb-tras.svg" alt="A face de trás da PCB, vista por trás: as duas barras fêmea com os nomes dos pinos, os quatro capacitores perto da fileira da tela e os 4 furos sem solda."></div>
  </div>
  <figcaption>As duas faces: em cor de cobre, onde vai solda; em escuro, o que vem escrito na placa. Na frente vão a tela, as
  duas chaves e o teclado. Atrás vão as duas barras fêmea e os capacitores.</figcaption>
</figure>
<h3>O que a PCB liga</h3>
{tabela_ligacoes()}
<p class="pequeno">Os nomes J1-1, J2-3 contam os pinos a partir da ponta da ESP32. O teste de regras do KiCad passa sem erros e
sem pinos soltos.</p>

<h2 id="imprimir">Imprimir a caixa</h2>
<p>Quatro arquivos STL, já virados do jeito de imprimir. Estão no
<span class="mono">tijolo-caixa-3d-{P.CAIXA_VERSAO}.zip</span>, que mandei na conversa, e no repositório, em
<code>montagem/fabricar/caixa/</code>. <b>Use este, com a data {P.CAIXA_VERSAO} no nome</b>, e apague os anteriores: no primeiro, a
caixa é {mm(P.Z_TOPO - SEM_MEDIR['espessura'])} mm mais fina e não fecha com a sua pilha; no v3, a calha do alto-falante ficou
justa demais. O zip leva um LEIA-ME com estas instruções para a gráfica 3D:</p>
{imprimir}
<p><b>Material</b>: PETG ou PLA. As linguetas da frente dobram para encaixar, e o PETG aguenta melhor. Camada de 0,2 mm,
3 perímetros, 20% de preenchimento, <b>sem suportes</b>. Se os cantos do fundo levantarem da mesa, use aba (brim).</p>
<p>A caixa fecha com duas linguetas em cima e dois parafusos M2 × 8 embaixo, que entram por trás.</p>

<h2 id="cabos">Cabos e fios</h2>
<p>Com a PCB, a tela, os botões e o teclado não usam nenhum fio solto. Sobra isto:</p>
<div class="lado-a-lado">
  <figure style="margin:0"><div class="desenho-caixa">{desenho_cabos()}</div>
  <figcaption>O fundo visto de frente, sem a PCB. Em laranja, o cabo do alto-falante; em verde, o do microfone; tracejado, o da
  antena, que passa por baixo da LILYGO.</figcaption></figure>
  <div>
    {cabos}
    <p>Os conectores SPEK e MIC ficam do lado da pilha, perto da ponta de cima. Cada um tem um <b>+</b> marcado na placa. No
    alto-falante tanto faz o lado; no microfone, o + vai no terminal que não toca a carcaça.</p>
  </div>
</div>

{''.join(passos)}

<h2 id="problemas">Se algo não funcionar</h2>
<ul>
  <li><b>A PCB não assenta nos pilares</b>: veja se sobrou algum dos 4 pinos da tela sem cortar e se alguma perna atrás passou
  de 1,5 mm. Se um capacitor de trás bater no suporte da pilha, deite-o um pouco para o lado. Se a pilha encostar na PCB, me
  mande a medida dela.</li>
  <li><b>A frente não fecha</b>: confira a espessura da PCB (tem de ser 0,8 mm), a tela assentada nas duas torres e os cabos,
  que não podem ficar em cima dos pilares.</li>
  <li><b>A tela fica branca ou sem imagem</b>: confira o firmware (<code>MONTAGEM_PLACA 2</code>) e as soldas dos 9 primeiros
  pinos. Se a imagem aparecer de cabeça para baixo, mude <code>TELA_ROTACAO</code> no <code>config.h</code>.</li>
  <li><b>Uma tecla, ou uma fileira inteira, não responde</b>: é uma perninha da aba do teclado. Refaça a solda e ensine o
  teclado de novo.</li>
  <li><b>As pontas das pernas das barras não aparecem na frente da PCB</b> (passo 4): uma barra fêmea não está encaixada até o
  fim nos pinos da LILYGO, ou a LILYGO não assentou nos 4 pilares.</li>
  <li><b>Um botão fica sempre apertado</b>: a chave táctil é mais alta que {mm(P.CHAVE_TACTIL_ALTURA + P.BOTAO_JOGO - 0.1)} mm.
  Lixe um pouco a aba do botão impresso, por cima.</li>
  <li><b>Sem som, ou ninguém ouve você</b>: os cabos podem estar trocados entre SPEK e MIC. No microfone, confira o lado do
  +.</li>
  <li><b>Sinal fraco</b>: a antena tem de estar no conector LTE. O conector GNSS, ao lado, é do GPS, que a sua placa não
  tem.</li>
  <li><b>O computador não acha o Tijolo para gravar</b>: ele pode estar desligado pelo vermelho. Segure o vermelho por 1 segundo
  para ligar. Se ainda assim não aparecer, abra, tire o conjunto da PCB e use os botõezinhos BOOT e RST da LILYGO, do lado da
  pilha, na ponta da ESP32: segure o BOOT, aperte e solte o RST, solte o BOOT e grave.</li>
</ul>

<h2 id="fontes">De onde vêm as medidas</h2>
<ul>
  <li>LILYGO: o modelo 3D, o desenho com cotas e as fotos do
  <a href="https://github.com/Xinyuan-LilyGO/LilyGo-Modem-Series/tree/main/dimensions/Standard">repositório oficial</a>.</li>
  <li>Tela: o desenho <a href="https://www.lcdwiki.com/res/MSP2402/MSP2401_Size.pdf">MSP2401_Size.pdf</a> e a página do módulo
  na <a href="https://www.lcdwiki.com/2.4inch_SPI_Module_ILI9341_SKU:MSP2402">LCDWiki</a>.</li>
  <li>Teclado: a ficha da <a href="https://www.adafruit.com/product/3845">Adafruit 3845</a>, a da
  <a href="https://www.universal-solder.ca/product/3x4-matrix-keypad-hard-keys-black/">Universal Solder</a> e a foto com cotas
  do anúncio.</li>
  <li>Medidos por você em {P.MEDIDO_EM}: o corpo e a espessura do teclado, a altura da pilha na LILYGO, a chave táctil, a perna
  das barras fêmea; em 25/11, o lado comprido e a espessura do alto-falante.</li>
</ul>
<p class="pequeno">A PCB, a caixa e este guia saem dos scripts da pasta <code>montagem/</code> do repositório (projeto.py,
placa/placa.py, caixa/caixa.py e guia.py). Mudando uma medida em projeto.py, tudo se refaz igual.</p>
</main>
</div>
<script src="{CDN}/build/three.min.js"></script>
<script src="{CDN}/examples/js/controls/OrbitControls.js"></script>
<script>{JS}</script>
"""
    return corpo


JS = r"""
(function () {
  // ---------- guardar as conferências (só neste navegador) ----------
  function ler(k, padrao) { try { var v = localStorage.getItem(k); return v === null ? padrao : v; } catch (e) { return padrao; } }
  function gravar(k, v) { try { localStorage.setItem(k, v); } catch (e) {} }
  document.querySelectorAll('input[data-confere]').forEach(function (c) {
    c.checked = ler('tijolo2-' + c.dataset.confere, '0') === '1';
    c.addEventListener('change', function () { gravar('tijolo2-' + c.dataset.confere, c.checked ? '1' : '0'); });
  });

  // ---------- a vista 3D ----------
  var caixa = document.getElementById('vista');
  if (!window.THREE || !THREE.OrbitControls || !window.fetch) return;   // fica a imagem
  var W = caixa.clientWidth, H = caixa.clientHeight;
  var r;
  try { r = new THREE.WebGLRenderer({ antialias: true, alpha: true }); } catch (e) { return; }
  r.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
  r.setSize(W, H, false);
  r.localClippingEnabled = true;
  var cena = new THREE.Scene();
  var cam = new THREE.PerspectiveCamera(26, W / H, 1, 3000);
  cam.up.set(0, 0, 1);
  cena.add(new THREE.HemisphereLight(0xffffff, 0x4a5650, 0.85));
  var luz = new THREE.DirectionalLight(0xffffff, 0.65); luz.position.set(-80, -140, 220); cena.add(luz);
  var luz2 = new THREE.DirectionalLight(0xffffff, 0.3); luz2.position.set(120, 90, -80); cena.add(luz2);
  var corte = new THREE.Plane(new THREE.Vector3(-1, 0, 0), 35.8);      // pelo eixo da pilha
  // de onde a câmera olha em cada modo: [alvo, deslocamento da câmera]
  var olhar = {
    fechado: [[26, 80, 14], [135, -265, 250]],
    aberto: [[26, 80, 36], [150, -300, 250]],
    dentro: [[26, 80, 8], [95, -190, 320]],
    corte: [[36, 80, 18], [370, -60, 70]]
  };
  var ctl = new THREE.OrbitControls(cam, r.domElement);
  ctl.enableDamping = true; ctl.enablePan = false; ctl.minDistance = 130; ctl.maxDistance = 640;
  function enquadrar(m) {
    var o = olhar[m];
    ctl.target.set(o[0][0], o[0][1], o[0][2]);
    cam.position.set(o[0][0] + o[1][0], o[0][1] + o[1][1], o[0][2] + o[1][2]);
  }
  enquadrar('fechado');
  var pecas = [
    { arq: 'fundo', cor: 0x4b535b, grupo: 'fundo' }, { arq: 'frente', cor: 0x687280, grupo: 'frente' },
    { arq: 'botao_verde', cor: 0x2f9e58, grupo: 'frente' }, { arq: 'botao_vermelho', cor: 0xcf3b33, grupo: 'frente' },
    { arq: 'miolo_lilygo', cor: 0x27485a, grupo: 'lilygo' }, { arq: 'miolo_encaixe', cor: 0x16181a, grupo: 'lilygo' },
    { arq: 'miolo_pilha', cor: 0xd9a441, grupo: 'lilygo' }, { arq: 'miolo_som', cor: 0x8a6f4e, grupo: 'lilygo' },
    { arq: 'miolo_pcb', cor: 0x2f7a4f, grupo: 'pcb' }, { arq: 'miolo_tela', cor: 0xa8483c, grupo: 'pcb' },
    { arq: 'miolo_vidro', cor: 0x14171c, grupo: 'pcb' }, { arq: 'miolo_teclado', cor: 0x1d1d20, grupo: 'pcb' },
    { arq: 'miolo_teclas', cor: 0x3a3a41, grupo: 'pcb' }
  ];
  var malhas = [], modo = 'fechado', pronto = false;
  // 3d/pecas.json: cada peça é uma lista de triângulos, com as coordenadas em
  // passos de 'passo' mm a partir de 'min' (uint16, little-endian, em base64)
  function geometria(p, passo) {
    var bin = atob(p.dados), n = bin.length >> 1, pos = new Float32Array(n);
    for (var i = 0; i < n; i++) pos[i] = p.min[i % 3] + (bin.charCodeAt(2 * i) | (bin.charCodeAt(2 * i + 1) << 8)) * passo;
    var g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
    g.computeVertexNormals();
    return g;
  }
  fetch('3d/pecas.json').then(function (resp) {
    if (!resp.ok) throw new Error(resp.status);
    return resp.json();
  }).then(function (d) {
    pecas.forEach(function (p) {
      if (!d.pecas[p.arq]) return;
      var m = new THREE.Mesh(geometria(d.pecas[p.arq], d.passo),
        new THREE.MeshStandardMaterial({ color: p.cor, roughness: 0.78, metalness: 0, side: THREE.DoubleSide }));
      m.userData.grupo = p.grupo; malhas.push(m); cena.add(m);
    });
    comecar();
  }).catch(function () {});              // sem as peças, fica a imagem
  function aplicar() {
    malhas.forEach(function (m) {
      var g = m.userData.grupo;
      m.material.clippingPlanes = modo === 'corte' ? [corte] : [];
      m.visible = !(modo === 'dentro' && (g === 'frente' || g === 'pcb'));
      m.position.z = (modo === 'aberto' && g === 'frente') ? 46 : 0;
    });
  }
  function comecar() {
    caixa.insertBefore(r.domElement, caixa.firstChild);
    document.getElementById('vista-capa').hidden = true;
    document.getElementById('vista-dica').hidden = false;
    document.getElementById('vista-controles').hidden = false;
    pronto = true; aplicar(); animar();
    document.documentElement.setAttribute('data-vista', 'pronta');
  }
  document.querySelectorAll('button[data-modo]').forEach(function (b) {
    b.addEventListener('click', function () {
      modo = b.dataset.modo;
      document.querySelectorAll('button[data-modo]').forEach(function (x) { x.setAttribute('aria-pressed', x === b ? 'true' : 'false'); });
      if (pronto) { aplicar(); enquadrar(modo); }
    });
  });
  // desenha só quando a vista está na tela (poupa a bateria do celular)
  var visivel = true;
  if ('IntersectionObserver' in window) {
    new IntersectionObserver(function (e) { visivel = e[0].isIntersecting; }).observe(caixa);
  }
  function animar() {
    if (visivel) { ctl.update(); r.render(cena, cam); }
    requestAnimationFrame(animar);
  }
  window.addEventListener('resize', function () {
    W = caixa.clientWidth; H = caixa.clientHeight; r.setSize(W, H, false); cam.aspect = W / H; cam.updateProjectionMatrix();
  });
})();
"""


PASSO_3D = 0.005          # mm: a resolução das coordenadas da vista 3D


def triangulos_stl(caminho):
    """Os vértices de um STL binário: (n_triângulos * 3, 3)"""
    with open(caminho, "rb") as f:
        dados = f.read()
    n = struct.unpack_from("<I", dados, 80)[0]
    if 84 + 50 * n != len(dados):
        raise ValueError(f"{caminho}: não é um STL binário")
    reg = np.frombuffer(dados, dtype=np.dtype([("n", "<f4", 3), ("v", "<f4", 9), ("a", "<u2")]), count=n, offset=84)
    return reg["v"].reshape(-1, 3).astype(np.float64)


def pacote_3d(pasta):
    """Os STL leves da caixa (caixa.py visual) num JSON só, com as coordenadas em uint16: os artifacts não
    servem .stl, e assim fica com um terço do tamanho"""
    pecas = {}
    for arq in sorted(os.listdir(pasta)):
        if not arq.endswith(".stl"):
            continue
        v = triangulos_stl(os.path.join(pasta, arq))
        mn = v.min(axis=0)
        q = np.round((v - mn) / PASSO_3D)
        if q.max() > 65535:
            raise ValueError(f"{arq}: grande demais para o passo de {PASSO_3D} mm")
        pecas[arq[:-4]] = {"min": [round(float(x), 4) for x in mn],
                           "dados": base64.b64encode(q.astype("<u2").tobytes()).decode("ascii")}
    return {"passo": PASSO_3D, "pecas": pecas}


def gerar():
    os.makedirs(os.path.join(SAIDA, "3d"), exist_ok=True)
    pag = pagina()
    with open(os.path.join(SAIDA, "index.html"), "w", encoding="utf-8") as f:
        f.write(pag)
    for lado in ("frente", "tras"):
        with open(os.path.join(SAIDA, f"pcb-{lado}.svg"), "w", encoding="utf-8") as f:
            f.write(svg_da_pcb(lado))
    vis = os.path.join(AQUI, "saida", "caixa", "visual")
    with open(os.path.join(SAIDA, "3d", "pecas.json"), "w", encoding="ascii") as f:
        json.dump(pacote_3d(vis), f, separators=(",", ":"))
    return os.path.join(SAIDA, "index.html")


if __name__ == "__main__":
    print(gerar(), os.path.getsize(os.path.join(SAIDA, "index.html")) // 1024, "KB")
