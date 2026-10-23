"""
O guia de montagem (uma página HTML, publicada como artifact):

    python3 guia.py          (da pasta montagem/; antes: caixa/caixa.py, que faz saida/caixa/visual/)

Sai em saida/guia/: index.html (os desenhos de desenho.py vão dentro) e 3d/pecas.json
(as peças da vista 3D). A imagem de capa (capa.png, o que aparece antes da vista 3D
carregar) é uma foto da própria vista 3D, tirada com o navegador.
"""
import base64
import html
import json
import os
import struct

import numpy as np

import projeto as P
import fiacao as F
import desenho as D

AQUI = os.path.dirname(os.path.abspath(__file__))
SAIDA = os.path.join(AQUI, "saida", "guia")
CDN = "https://cdn.jsdelivr.net/npm/three@0.128.0"


def esc(s):
    return html.escape(str(s), quote=True)


def svg(nome):
    with open(os.path.join(AQUI, "saida", nome + ".svg"), encoding="utf-8") as f:
        return f.read()


def mm(v, casas=1):
    return f"{v:.{casas}f}".replace(".", ",")


# ---------------------------------------------------------------------------
# tabelas a partir da fiação
# ---------------------------------------------------------------------------
COR_NOME = {"vermelho": "vermelho", "preto": "preto", "laranja": "laranja", "amarelo": "amarelo", "verde": "verde",
            "azul": "azul", "roxo": "roxo", "cinza": "cinza", "branco": "branco", "marrom": "marrom"}


def bolinha(cor):
    if not cor:
        return ""
    return f'<span class="cor" style="--c:{D.COR_FIO.get(cor, "#888")}"></span>{esc(COR_NOME.get(cor, cor))}'


def caminho_curto(c):
    """os furos de passagem principais (sem repetir as colunas do meio)"""
    if len(c) <= 4:
        return " → ".join(c)
    pontos = [c[0]]
    for i in range(1, len(c) - 1):
        a, b, n = c[i - 1], c[i], c[i + 1]
        mesma_col = a[0] == b[0] == n[0]
        mesma_lin = a[1:] == b[1:] == n[1:]
        if not (mesma_col or mesma_lin):
            pontos.append(b)
    pontos.append(c[-1])
    return " → ".join(pontos)


def tabela_matriz():
    linhas = []
    for l in F.LIG:
        if l.etapa != "matriz":
            continue
        tipo = "ponte de solda" if l.tipo == "ponte" else "fio nu"
        linhas.append(f"<tr><td class='furo'>{esc(l.de)}</td><td class='furo'>{esc(l.ate)}</td><td>{tipo}</td>"
                      f"<td>{esc(l.sinal)}</td></tr>")
    return ("<div class='rolagem'><table><thead><tr><th>De</th><th>Até</th><th>Como</th><th>O que liga</th></tr></thead>"
            f"<tbody>{''.join(linhas)}</tbody></table></div>")


def tabela_fios(etapa, espelhada=False):
    import importlib
    os.environ["TELA_ESPELHADA"] = "1" if espelhada else "0"
    importlib.reload(F)
    linhas = []
    for l in F.LIG:
        if l.etapa != etapa:
            continue
        ponte = next((m for m in F.LIG if m.lado == "frente" and m.sinal == l.sinal and m.de == l.ate), None)
        pino = ""
        if ponte:
            pino = f"ponte <span class='furo'>{esc(ponte.de)}</span>–<span class='furo'>{esc(ponte.ate)}</span>"
        if l.sinal == "GND dos botões" and etapa == "esquerda":
            pino = "ponte <span class='furo'>D10</span>–<span class='furo'>E10</span>–<span class='furo'>F10</span>"
        alvo = ""
        if l.sinal.startswith("tela "):
            g = next(g for p, s, g, c in F.TELA_LIGA if f"tela {s}" == l.sinal)
            alvo = g
        elif l.sinal in F.GPIO_TECLADO:
            alvo = F.GPIO_TECLADO[l.sinal]
        elif l.sinal == "botão vermelho":
            alvo = F.GPIO_VERMELHO
        elif l.sinal == "botão verde":
            alvo = F.GPIO_VERDE
        elif l.sinal.startswith("GND"):
            alvo = "GND"
        linhas.append(
            f"<tr><td>{esc(l.sinal)}</td><td class='furo'>{esc(l.de)}</td>"
            f"<td class='mono pequeno'>{esc(caminho_curto(l.caminho))}</td>"
            f"<td class='furo'>{esc(l.ate)}</td><td>{pino}</td><td class='mono'>{esc(alvo)}</td><td>{bolinha(l.cor)}</td></tr>")
    os.environ["TELA_ESPELHADA"] = "0"
    importlib.reload(F)
    return ("<div class='rolagem'><table><thead><tr><th>Sinal</th><th>Começa</th><th>Por onde (atrás)</th><th>Atravessa</th>"
            "<th>Na frente</th><th>Pino</th><th>Cor</th></tr></thead>"
            f"<tbody>{''.join(linhas)}</tbody></table></div>")


def tabela_chaves():
    linhas = []
    for ch in F.CHAVES:
        (a, b), (c, d) = ch.pernas()
        nome = {"verde": "verde", "vermelho": "vermelho"}.get(ch.rotulo, ch.rotulo)
        linhas.append(f"<tr><td class='tecla-nome'>{esc(nome)}</td><td class='furo'>{a}</td><td class='furo'>{b}</td>"
                      f"<td class='furo'>{c}</td><td class='furo'>{d}</td></tr>")
    return ("<div class='rolagem'><table class='compacta'><thead><tr><th>Tecla</th><th>Em cima, esq.</th><th>Em cima, dir.</th>"
            "<th>Embaixo, esq.</th><th>Embaixo, dir.</th></tr></thead>"
            f"<tbody>{''.join(linhas)}</tbody></table></div>")


def tabela_teste():
    fil = {"1": "fileira 1 2 3", "2": "fileira 1 2 3", "3": "fileira 1 2 3", "4": "fileira 4 5 6", "5": "fileira 4 5 6",
           "6": "fileira 4 5 6", "7": "fileira 7 8 9", "8": "fileira 7 8 9", "9": "fileira 7 8 9", "*": "fileira * 0 #",
           "0": "fileira * 0 #", "#": "fileira * 0 #"}
    col = {"1": "coluna 1 4 7 *", "4": "coluna 1 4 7 *", "7": "coluna 1 4 7 *", "*": "coluna 1 4 7 *",
           "2": "coluna 2 5 8 0", "5": "coluna 2 5 8 0", "8": "coluna 2 5 8 0", "0": "coluna 2 5 8 0",
           "3": "coluna 3 6 9 #", "6": "coluna 3 6 9 #", "9": "coluna 3 6 9 #", "#": "coluna 3 6 9 #"}

    def ponto(sinal):
        if sinal == "coluna 1 4 7 *":
            return "C38 (atrás)"
        return f"Q{F.DESTINO[sinal]} (ponta na frente)"
    linhas = []
    for t in ["1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"]:
        linhas.append(f"<tr><td class='tecla-nome'>{t}</td><td class='mono'>{ponto(fil[t])}</td>"
                      f"<td class='mono'>{ponto(col[t])}</td></tr>")
    linhas.append("<tr><td class='tecla-nome'>verde</td><td class='mono'>B34 (atrás)</td><td class='mono'>B32 (atrás)</td></tr>")
    linhas.append(f"<tr><td class='tecla-nome'>vermelho</td><td class='mono'>Q{F.DESTINO['botão vermelho']} (ponta na frente)</td>"
                  "<td class='mono'>B32 (atrás)</td></tr>")
    return ("<div class='rolagem'><table class='compacta'><thead><tr><th>Aperte</th><th>Ponta 1</th><th>Ponta 2</th></tr></thead>"
            f"<tbody>{''.join(linhas)}</tbody></table></div>")


def tabela_tela(espelhada=False):
    linhas = []
    for p, s, g, c in F.TELA_LIGA:
        col = P.letra(P.TELA_COL0 + (13 - (p - 1) if espelhada else p - 1))
        n = F.linha_j1(g)
        linhas.append(f"<tr><td>{p}</td><td>{esc(s)}</td><td class='furo'>{col}1</td><td class='furo'>E{n}</td>"
                      f"<td class='mono'>{esc(g)}</td><td>{bolinha(c)}</td></tr>")
    return ("<div class='rolagem'><table class='compacta'><thead><tr><th>Pino</th><th>Na tela</th><th>Furo</th>"
            "<th>Fio até</th><th>LILYGO</th><th>Cor</th></tr></thead>"
            f"<tbody>{''.join(linhas)}</tbody></table></div>")


# ---------------------------------------------------------------------------
# a página
# ---------------------------------------------------------------------------
CSS = r"""
/* Manual de bancada: uma coluna de texto, figuras largas e a placa como protagonista. */
:root {
  --papel: #eef0eb;  --cartao: #f8f9f6;  --tinta: #1c2420;  --tinta-2: #4c5852;  --linha: #c9cfc7;
  --verde: #2f6247;  --cobre: #b06a1f;  --cobre-claro: #f3e2cd;  --alerta: #a3341f;  --alerta-fundo: #f6dfd8;
  --ok: #2f7a4a;  --sombra: rgba(28, 36, 32, .08);
  --display: "Barlow Condensed", "Arial Narrow", sans-serif;
  --texto: "IBM Plex Sans", "Segoe UI", system-ui, sans-serif;
  --mono: "IBM Plex Mono", ui-monospace, "SFMono-Regular", Menlo, monospace;
  --fundo-3d: #e2e6e0;
}
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) {
  --papel: #121815;  --cartao: #19211d;  --tinta: #e1e7e1;  --tinta-2: #a5b1aa;  --linha: #2f3a34;
  --verde: #5fae80;  --cobre: #e0a060;  --cobre-claro: #3a2a17;  --alerta: #f08a72;  --alerta-fundo: #3a1d16;
  --ok: #6fc98f;  --sombra: rgba(0, 0, 0, .35);  --fundo-3d: #1b2420;  color-scheme: dark; } }
:root[data-theme="dark"] {
  --papel: #121815;  --cartao: #19211d;  --tinta: #e1e7e1;  --tinta-2: #a5b1aa;  --linha: #2f3a34;
  --verde: #5fae80;  --cobre: #e0a060;  --cobre-claro: #3a2a17;  --alerta: #f08a72;  --alerta-fundo: #3a1d16;
  --ok: #6fc98f;  --sombra: rgba(0, 0, 0, .35);  --fundo-3d: #1b2420;  color-scheme: dark; }

* { box-sizing: border-box; }
body { margin: 0; background: var(--papel); color: var(--tinta); font: 400 16px/1.6 var(--texto);
  padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px)); }
.pagina { max-width: 1120px; margin: 0 auto; padding-block: 28px 80px; display: grid;
  grid-template-columns: 220px minmax(0, 1fr); gap: 40px; }
@media (max-width: 900px) { .pagina { grid-template-columns: minmax(0, 1fr); gap: 0; } nav.indice { display: none; } }
nav.indice { position: sticky; top: calc(env(safe-area-inset-top, 0px) + 20px); align-self: start;
  font-size: 14px; line-height: 1.45; padding-top: 6px; }
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
.mono, .furo { font-family: var(--mono); }
.furo { font-weight: 500; white-space: nowrap; }
.pequeno { font-size: 13px; color: var(--tinta-2); }
kbd, code { font-family: var(--mono); font-size: .9em; background: var(--cartao); border: 1px solid var(--linha);
  border-radius: 4px; padding: 0 5px; }
.fichas { display: flex; flex-wrap: wrap; gap: 8px 10px; margin: 20px 0 6px; padding: 0; list-style: none; }
.fichas li { font-family: var(--mono); font-size: 13px; border: 1px solid var(--linha); border-radius: 999px;
  padding: 4px 12px; background: var(--cartao); }

/* a vista 3D */
.vista { position: relative; margin: 26px 0 8px; border-radius: 10px; overflow: hidden; background: var(--fundo-3d);
  aspect-ratio: 16 / 10; max-width: 100%; }
.vista img, .vista canvas { position: absolute; inset: 0; width: 100%; height: 100%; object-fit: contain; display: block; }
.vista canvas { touch-action: none; cursor: grab; }
.vista .controles { position: absolute; left: 12px; bottom: 12px; display: flex; flex-wrap: wrap; gap: 6px; z-index: 2; }
.vista .dica { position: absolute; right: 12px; top: 10px; font-size: 12px; color: var(--tinta-2); z-index: 2; }
button.chip { font: 500 13px/1 var(--texto); color: var(--tinta); background: var(--cartao); border: 1px solid var(--linha);
  border-radius: 999px; padding: 8px 12px; cursor: pointer; }
button.chip[aria-pressed="true"] { background: var(--tinta); color: var(--papel); border-color: var(--tinta); }
button.chip:focus-visible, button.botao:focus-visible, input:focus-visible, select:focus-visible { outline: 2px solid var(--cobre); outline-offset: 2px; }

/* figuras */
figure { margin: 22px 0; }
figure .desenhos { display: grid; grid-template-columns: repeat(auto-fit, minmax(260px, 1fr)); gap: 18px; align-items: start; }
figure .desenho-caixa { background: var(--cartao); border: 1px solid var(--linha); border-radius: 8px; padding: 10px; min-width: 0; }
figcaption { font-size: 14px; color: var(--tinta-2); margin-top: 8px; max-width: 75ch; }
.desenho { width: 100%; height: auto; display: block; }
.corte-caixa { overflow-x: auto; }
.corte-caixa .desenho { min-width: 640px; }

/* tabelas */
.rolagem { overflow-x: auto; margin: 14px 0; border: 1px solid var(--linha); border-radius: 8px; background: var(--cartao); }
table { border-collapse: collapse; width: 100%; font-size: 14px; font-variant-numeric: tabular-nums; }
th, td { text-align: left; padding: 7px 10px; border-bottom: 1px solid var(--linha); vertical-align: top; }
th { font-family: var(--mono); font-weight: 500; font-size: 12px; letter-spacing: .6px; text-transform: uppercase; color: var(--tinta-2); }
tbody tr:last-child td { border-bottom: 0; }
table.compacta { width: auto; min-width: 100%; }
.tecla-nome { font-family: var(--display); font-size: 18px; font-weight: 600; }
.cor { display: inline-block; width: 11px; height: 11px; border-radius: 50%; background: var(--c); margin-right: 6px;
  vertical-align: -1px; box-shadow: 0 0 0 1px var(--linha); }
.tem { color: var(--ok); font-weight: 600; }
.comprar { color: var(--cobre); font-weight: 600; }

/* passos */
.passo { margin-top: 56px; }
.passo-cab { display: flex; gap: 16px; align-items: baseline; border-top: 2px solid var(--tinta); padding-top: 16px; }
.passo-num { font-family: var(--display); font-size: 54px; line-height: .8; color: var(--cobre); font-weight: 700; min-width: 1.4em; }
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
.progresso { font-family: var(--mono); font-size: 13px; color: var(--tinta-2); }

/* alternância da tela */
.alterna { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; margin: 12px 0; }
.alterna span { font-size: 14px; color: var(--tinta-2); }

/* formulário das medidas */
form.medidas { display: grid; gap: 12px; margin: 16px 0; max-width: 720px; }
.campo { display: grid; grid-template-columns: minmax(0, 1fr) 150px; gap: 12px; align-items: center;
  padding: 8px 0; border-bottom: 1px solid var(--linha); }
.campo label { font-size: 15px; }
.campo .pequeno { display: block; }
.campo input, .campo select { font: 500 15px var(--mono); padding: 7px 9px; border: 1px solid var(--linha);
  border-radius: 6px; background: var(--cartao); color: var(--tinta); width: 100%; }
@media (max-width: 520px) { .campo { grid-template-columns: minmax(0, 1fr); } }
button.botao { font: 600 15px var(--texto); background: var(--verde); color: var(--papel); border: 0; border-radius: 8px;
  padding: 11px 18px; cursor: pointer; justify-self: start; }
textarea#medidas-texto { width: 100%; min-height: 150px; font: 13px/1.5 var(--mono); padding: 10px; border-radius: 8px;
  border: 1px solid var(--linha); background: var(--cartao); color: var(--tinta); }
.copiado { font-size: 14px; color: var(--ok); }

/* cores do desenho (as do metal e da placa não mudam com o tema; as letras sim) */
.desenho text { font-family: var(--texto); }
.desenho .pcb { fill: #2f6247; stroke: #1d3f2d; stroke-width: .35; }
.desenho .ilha { fill: #c9974c; } .desenho .buraco { fill: #173224; }
.desenho .eixo { font-size: 1.45px; fill: var(--tinta-2); } .desenho .eixoLinha { font-size: 1.3px; }
.desenho .tituloFolha { font-size: 2.6px; font-weight: 600; letter-spacing: .25px; fill: var(--tinta); font-family: var(--display); }
.desenho .guia { stroke: var(--linha); stroke-width: .18; } .desenho .guiaUsada { stroke: var(--tinta-2); stroke-width: .22; }
.desenho .rotPino { font-size: 1.35px; fill: var(--tinta-2); opacity: .7; }
.desenho .rotPinoUsado { fill: var(--tinta); font-weight: 600; opacity: 1; }
.desenho .pinoLilygo { fill: #e9e6dc; stroke: #4c4b45; stroke-width: .25; }
.desenho .chave { fill: #3b3d40; stroke: #151617; stroke-width: .25; } .desenho .chave_verde { fill: #2f9e58; } .desenho .chave_vermelho { fill: #cf3b33; }
.desenho .botaozinho { fill: #151617; } .desenho .botao_verde { fill: #1b5e35; } .desenho .botao_vermelho { fill: #7d1e19; }
.desenho .rotTecla { font-size: 2.1px; fill: #fff; font-weight: 600; }
.desenho .pernaFrente { fill: #d8d4c8; }
.desenho .chaveSombra { fill: none; stroke: #e9e6dc; stroke-width: .25; stroke-dasharray: .7 .5; opacity: .85; }
.desenho .sombra_verde { stroke: #3fd07a; stroke-width: .4; } .desenho .sombra_vermelho { stroke: #ff6b5f; stroke-width: .4; }
.desenho .rotTeclaTras { font-size: 1.7px; fill: #e9e6dc; opacity: .9; }
.desenho .soldaPerna { fill: #d6d9de; stroke: #7d838b; stroke-width: .15; } .desenho .soldaPernaApagada { opacity: .5; }
.desenho .telaContorno { fill: #20325a; fill-opacity: .14; stroke: #e9e6dc; stroke-width: .35; stroke-dasharray: 1.2 .7; }
.desenho .telaVidro { fill: #20325a; fill-opacity: .12; }
.desenho .rotTela { font-size: 2.1px; fill: #f0efe9; font-weight: 600; } .desenho .rotTelaP { font-size: 1.6px; fill: #f0efe9; }
.desenho .pinoTela { fill: #f2c230; stroke: #6b5410; stroke-width: .2; } .desenho .pinoTelaLivre { fill: #b9b6ab; }
.desenho .rotPinoTela { font-size: 1.3px; fill: var(--tinta); }
.desenho .ponteFrente, .desenho .ponteSolda { stroke: #d6d9de; stroke-width: 1.3; stroke-linecap: round; }
.desenho .pontaFio { stroke: #1d1d1d; stroke-width: .12; }
.desenho .fioNu { fill: none; stroke: #d6d9de; stroke-width: .42; stroke-linecap: round; } .desenho .soldaNu { fill: #d6d9de; }
.desenho .fio { fill: none; stroke-width: .5; stroke-linecap: round; stroke-linejoin: round; }
.desenho .fioBorda { fill: none; stroke: #111; stroke-width: .72; stroke-linecap: round; stroke-linejoin: round; opacity: .55; }
.desenho .soldaFio { fill: #d6d9de; stroke-width: .35; } .desenho .atravessa { fill: #111; stroke-width: .45; }
.desenho .lilygoContorno { fill: none; stroke: #e9e6dc; stroke-width: .35; stroke-dasharray: 1.4 .8; }
.desenho .lilygoTapa { fill: #1e2b36; fill-opacity: .84; stroke: #0e151b; stroke-width: .3; }
.desenho .rotLilygoGrande { font-size: 4px; fill: #e9e6dc; font-weight: 700; letter-spacing: .4px; font-family: var(--display); }
.desenho .rotLilygoP { font-size: 1.9px; fill: #c9c6bb; }
.desenho .pecaAlta { fill: #e0553e; fill-opacity: .38; stroke: #e0553e; stroke-width: .2; }
.desenho .pecaMedia { fill: #e8a43a; fill-opacity: .28; stroke: #e8a43a; stroke-width: .2; }
.desenho .rotPeca { font-size: 1.25px; fill: #fff; font-weight: 600; } .desenho .fundoRotulo { fill: #1d2b24; fill-opacity: .72; }
.desenho .plasticoBarra { fill: #141414; opacity: .75; } .desenho .rotBarra { font-size: 1.6px; fill: #fff; font-weight: 600; }
.desenho .rotMaco { font-size: 1.4px; fill: #fff; }
.desenho .falantePlanta { fill: #a6592e; fill-opacity: .35; stroke: #a6592e; stroke-width: .25; stroke-dasharray: 1 .6; }
.desenho .rotFalante { font-size: 1.4px; fill: #fff; }
.desenho .capCeram { fill: #c8913f; stroke: #6e4f1c; stroke-width: .15; } .desenho .capEletro { fill: #2f4f8f; }
.desenho .rotCap { font-size: 1.3px; fill: var(--tinta); }
.desenho .caixaFundo { fill: #8f969e; } .desenho .caixaFrente { fill: #6f767e; } .desenho .cavidade { fill: var(--cartao); }
.desenho .bateria { fill: #4f8f5a; } .desenho .suporte { fill: #7b7a74; }
.desenho .lilygoCorte { fill: #1f4f3b; } .desenho .pecaCorte { fill: #6b6f75; } .desenho .pinoCorte { stroke: #c9a227; stroke-width: .35; }
.desenho .plastico { fill: #222; } .desenho .plastico2 { fill: #474747; } .desenho .placaCorte { fill: #2f6247; }
.desenho .telaCortePcb { fill: #b23a35; } .desenho .telaCorteVidro { fill: #20325a; }
.desenho .chaveCorte { fill: #3b3d40; } .desenho .teclaCorte { fill: #c9c4b4; }
.desenho .falante { fill: #a6592e; } .desenho .mic { fill: #6b6f75; }
.desenho .cota { stroke: var(--tinta-2); stroke-width: .22; } .desenho .rotCota { font-size: 2.1px; fill: var(--tinta); }
.desenho .rotCortePeq { font-size: 1.9px; fill: var(--tinta); } .desenho .rotCorteClaro { font-size: 2.1px; fill: #fff; }
.desenho .rotCorteEixo { font-size: 2px; fill: var(--tinta-2); }
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


def alterna_tela():
    """Os botões da ordem dos pinos da tela (todos ficam iguais: o script sincroniza)"""
    return """<div class="alterna" role="group" aria-label="De que lado fica o VCC da tela">
  <span>Na minha tela (de frente, com os pinos em cima), o VCC fica:</span>
  <button class="chip" type="button" data-tela="normal" aria-pressed="true">à esquerda</button>
  <button class="chip" type="button" data-tela="espelhada" aria-pressed="false">à direita</button>
</div>"""


def figura(svgs, legenda, corte=False):
    caixas = "".join(f"<div class='desenho-caixa{' corte-caixa' if corte else ''}'>{s}</div>" for s in svgs)
    return f"<figure><div class='desenhos'>{caixas}</div><figcaption>{legenda}</figcaption></figure>"


def pagina():
    frente_n, frente_e = svg("frente"), svg("frente_espelhada")
    tras_a = svg("tras_antes")
    tras_d_n, tras_d_e = svg("tras_depois"), svg("tras_depois_espelhada")
    corte = svg("corte")
    larg_caixa = P.CAV_X1 - P.CAV_X0 + 2 * P.PAREDE
    comp_caixa = P.CAV_Y1 - P.CAV_Y0 + 2 * P.PAREDE
    n_esq = len([l for l in F.LIG if l.etapa == "esquerda"])
    n_dir = len([l for l in F.LIG if l.etapa == "direita"])

    indice = [("conferir", "0", "Antes de começar"), ("comprar", "", "O que comprar"), ("como-fica", "", "Como fica"),
              ("lilygo", "1", "Preparar a LILYGO"), ("cortar", "2", "Cortar a placa"), ("chaves", "3", "As 14 chaves"),
              ("matriz", "4", "A matriz atrás"), ("direita", "5", "Os fios da direita"), ("multimetro", "6", "Teste com o multímetro"),
              ("juntar", "7", "Juntar com a LILYGO"), ("pontes", "8", "As pontes da direita"), ("esquerda", "9", "Os fios da esquerda"),
              ("tela", "10", "A tela"), ("firmware", "11", "Firmware e teste"), ("fechar", "12", "Fechar a caixa"),
              ("caixa", "", "Imprimir a caixa"), ("medidas", "", "Medidas para a caixa"), ("depois", "", "Depois: a placa de circuito"),
              ("problemas", "", "Se algo não funcionar")]
    nav = "".join(f"<a href='#{a}'>{f'<span class=num>{n}</span>' if n else ''}{t}</a>" for a, n, t in indice)

    passos = []
    passos.append(passo(1, "lilygo", "Preparar a LILYGO", "antes de tudo · sem pilha, chave em OFF, sem USB", f"""
<p>A LILYGO fica atrás da placa da frente, com os componentes virados para ela e a pilha para o fundo da caixa. Entre as duas placas
ficam {mm(P.VAO, 2)} mm: dois plásticos de barra de pinos, um em cima do outro.</p>
<ol class="faz">
  <li><b>Tire o conector SOLAR.</b> Ele tem 6,25 mm de altura e não cabe entre as placas (o painel solar não é usado). Corte o corpo
  de plástico com o alicate de corte, aqueça cada perninha pelo lado da pilha e puxe com a pinça; limpe os furos com a malha.</li>
  <li><b>As barras de pinos</b>: o plástico fica do lado dos componentes e os pinos compridos apontam para a frente, para a placa
  ilhada. A ponta curta atravessa a LILYGO e é soldada do lado da pilha. Se as suas estão ao contrário, veja a
  <a href="#conferir">conferência A</a>.</li>
  <li><b>O segundo plástico</b>: de uma barra nova de 40 pinos, corte dois pedaços de 16. Puxe os pinos para fora, um por um, com o
  alicate de bico, e deslize cada plástico vazio pelos pinos compridos da LILYGO até encostar no primeiro.</li>
  <li><b>Fita Kapton</b> em cima da lata do modem (o quadrado metálico perto da ponta de baixo) e do suporte do chip, sem tapar a
  entrada do chip, na borda esquerda. As soldas das teclas ficam a uns 2,5 mm deles.</li>
</ol>
<div class="nota">Dica: para as barras ficarem retas e paralelas enquanto você solda, encaixe as pontas compridas num pedaço de placa
ilhada que sobrou, nas colunas F e R (12 furos de distância).</div>""",
        ["O conector SOLAR saiu e os furos estão limpos.",
         "Nas duas barras, os pinos compridos saem do lado dos componentes, com dois plásticos empilhados.",
         "Sobram uns 3,5 mm de pino acima do segundo plástico.",
         "A lata do modem e o suporte do chip estão cobertos com Kapton, e a entrada do chip continua livre."]))

    passos.append(passo(2, "cortar", "Cortar a placa", "15 minutos", f"""
<p>Uma placa ilhada <b>dupla face, com furos metalizados</b>, de 18 colunas × 51 linhas. As colunas são A a R, da esquerda para a
direita, e as linhas 1 a 51, de cima para baixo, olhando de frente. Todos os nomes de furos deste guia (como <span class="furo">F12</span>)
usam essa conta.</p>
<ol class="faz">
  <li>Risque com estilete, várias vezes e dos dois lados, pela fileira de furos <i>seguinte</i> à última que fica. Quebre na quina da
  mesa.</li>
  <li><b>Lixe as bordas</b> até sobrar meio passo (1,3 mm) do centro dos furos da beirada até a borda, em volta toda. A placa
  pronta mede {mm(P.PLACA_LARGURA, 1)} × {mm(P.PLACA_ALTURA, 1)} mm. Não deixe mais de 1,5 mm: dentro da caixa sobram só 0,3 mm dos
  lados, e embaixo, depois da linha 51, ficam os pilares dos parafusos, a 0,4 mm.</li>
  <li>Escolha a frente e marque o furo <span class="furo">A1</span> (em cima, à esquerda) com caneta.</li>
</ol>""",
        [f"A placa mede {mm(P.PLACA_LARGURA, 1)} mm de largura (no máximo 46) e {mm(P.PLACA_ALTURA, 1)} mm de altura (no máximo 130).",
         "Em volta toda, da borda até o centro dos furos da beirada, dá no máximo 1,5 mm.",
         "O A1 está marcado na frente."]))

    passos.append(passo(3, "chaves", "As 14 chaves", "1 hora", f"""
<p>Chaves tácteis de 6 × 6 × 5 mm, com 4 perninhas. As perninhas saem por dois lados do corpo, e cada perninha está sempre ligada à
que fica de frente para ela, do outro lado. Apertar liga as de cima às de baixo. <b>Coloque as chaves com os lados de onde saem as
perninhas para a esquerda e para a direita.</b></p>
<ol class="faz">
  <li>Abra as perninhas com o alicate de bico, só um pouco: de um lado ao outro elas ficam a 3 passos de furo (um furo, dois
  livres, o outro) e de cima para baixo, a 2 passos (um furo, um livre, o outro).</li>
  <li>Encaixe pela frente, nos furos da tabela, com o corpo encostado na placa. Solde atrás.</li>
  <li>Corte as perninhas atrás deixando no máximo 1 mm.</li>
</ol>
{tabela_chaves()}
<div class="nota">Na primeira chave, confira com o multímetro em continuidade antes de soldar: sem apertar, ele apita entre as duas
perninhas de cima (e entre as duas de baixo); apertando, apita também entre a de cima e a de baixo.</div>""",
        ["As 14 chaves estão retas e encostadas na placa.",
         "Atrás, nenhuma perninha passa de 1 mm.",
         "As perninhas que se ligam por dentro estão à esquerda e à direita de cada chave."]))

    passos.append(passo(4, "matriz", "A matriz atrás", "antes de juntar com a LILYGO · 45 minutos", f"""
<p>Atrás, as teclas viram uma matriz de 4 fileiras e 3 colunas. As fileiras ligam as perninhas de cima das 3 teclas de cada fila. As
colunas ligam as perninhas de baixo das 4 teclas de cada coluna por um fio nu que desce entre elas, nas colunas C, I e O da placa.
Use fio nu fino, como perna de resistor cortada, deitado na placa e soldado nos furos.</p>
{figura([tras_a], "Atrás, antes de juntar (a vista é espelhada: a coluna A fica à direita). As manchas vermelhas e laranja são as peças da LILYGO que ficarão logo atrás desta placa, com a altura em mm: os fios passam pelo vale da coluna O, onde as peças são baixas.")}
{tabela_matriz()}
<div class="aviso">Tudo atrás fica deitado, com no máximo 1,2 mm de altura: a LILYGO vai ficar a {mm(P.VAO, 2)} mm, e embaixo das
teclas estão a lata do modem e o suporte do chip.</div>""",
        ["Os 8 fios nus das fileiras (E–H e K–N nas linhas 36, 40, 44 e 48) estão soldados.",
         "Os fios nus das colunas C, I e O descem da linha 38 à 50, com as 12 pontes até as perninhas.",
         "O fio nu E32–N32 liga as perninhas de cima do verde e do vermelho.",
         "Nada atrás passa de 1,2 mm de altura."]))

    passos.append(passo(5, "direita", f"Os {n_dir} fios da direita", "antes de juntar · 45 minutos", f"""
<p>Sete fios encapados (30 AWG) levam as linhas do teclado e o botão vermelho até a barra da direita da LILYGO. Eles sobem juntos
pela coluna O, onde as peças da LILYGO embaixo são baixas, e viram para a direita na linha do pino. Cada fio <b>atravessa o furo
Q</b> e fica com 1 cm para fora, na frente: depois de juntar, ele vai ser soldado no pino da barra R, ao lado.</p>
{tabela_fios("direita")}
<p>Os furos de passagem são só o caminho, por trás: o fio passa por cima deles, sem entrar nem ser soldado. Deixe os 7 fios
lado a lado, deitados, e prenda o maço na placa com tiras de Kapton (cola quente fica alta demais aqui).</p>""",
        ["Os 7 fios começam nas perninhas certas (tabela) e saem 1 cm pela frente em Q16 a Q22.",
         "O maço está deitado na coluna O, sem cruzar por cima de outro maço."]))

    passos.append(passo(6, "multimetro", "Teste com o multímetro", "antes de juntar · 10 minutos", f"""
<p>Depois de juntar, o lado de trás fica entre as placas e não dá mais para consertar. Teste agora: multímetro em continuidade
(o que apita), uma ponta em cada ponto da tabela. Apertando a tecla tem que apitar; soltando, não.</p>
{tabela_teste()}
<p>Sem apertar nada, nenhum par de pontas pode apitar: nem duas de Q16 a Q22 entre si, nem uma delas com C38 ou B32.</p>""",
        ["As 14 teclas apitam só quando apertadas.",
         "Sem apertar, nada apita entre Q16, Q17, Q18, Q19, Q20, Q21, Q22, C38 e B32."]))

    passos.append(passo(7, "juntar", "Juntar com a LILYGO", "30 minutos", f"""
<p>Depois de juntar, o lado dos componentes da LILYGO fica coberto. Antes, faça as duas coisas que ficam lá dentro:</p>
<ol class="faz">
  <li><b>O chip</b>: ponha o nano-SIM no suporte, se ainda não estiver lá. Ele entra pela borda esquerda da LILYGO. Depois de juntar
  ainda dá para tirar e pôr com a pinça, por baixo da faixa da esquerda da placa, mas fica apertado.</li>
  <li><b>A antena</b>: encaixe o cabo dela no conector LTE da LILYGO, o conectorzinho redondo no meio da ponta do modem (aperte
  reto até fazer clic). Vire o cabo para a esquerda, rente à ponta da placa: ele vai sair entre as placas pela borda esquerda
  da LILYGO. A antena fica solta até o passo 12.</li>
  <li>Encaixe a placa nos pinos da LILYGO: os pinos entram por trás nas colunas F e R, linhas 9 a 24. As teclas ficam para fora e o
  ESP32 da LILYGO fica do lado de cima (perto da linha 1); os USB-C ficam à direita, olhando de frente.</li>
  <li>Aperte uma placa contra a outra (os dois plásticos encostados) e solde só os 4 pinos dos cantos: <span class="furo">F9</span>,
  <span class="furo">F24</span>, <span class="furo">R9</span> e <span class="furo">R24</span>. Confira que as placas estão paralelas.</li>
  <li>Solde os outros 28. Corte as sobras deixando 1 mm: a tela vai deitar por cima.</li>
</ol>""",
        ["O chip está no suporte e a antena, encaixada, com o cabo saindo pela esquerda.",
         "As placas estão paralelas, com os plásticos encostados nas duas.",
         "Os 32 pinos estão soldados na frente e cortados a 1 mm.",
         "As 7 pontas dos fios continuam saindo em Q16 a Q22."]))

    passos.append(passo(8, "pontes", "As pontes da direita", "15 minutos", f"""
<p>Na frente, dobre cada ponta que sai de Q16 a Q22 até encostar no pino da barra ao lado, na coluna R, e solde junto: uma ponte de
solda Q–R por linha. Corte a sobra.</p>
{alterna_tela()}
<div data-tela-mostra="normal">{figura([frente_n], "A frente: as chaves, os pinos da LILYGO (os nomes em negrito são os usados) e as pontes de solda (em cinza claro, com a cor do fio que chega). A tela entra por último, por cima de tudo.")}</div>
<div data-tela-mostra="espelhada" hidden>{figura([frente_e], "A frente, para a tela com o VCC à direita: só os pinos da tela mudam de ordem.")}</div>""",
        ["Q16–R16, Q17–R17, Q18–R18, Q19–R19, Q20–R20, Q21–R21 e Q22–R22 estão em ponte.",
         "Nenhuma ponte encosta na vizinha de cima ou de baixo."]))

    passos.append(passo(9, "esquerda", "Os fios da esquerda", "1 hora", f"""
<p>A faixa da esquerda, colunas A a D, fica fora da LILYGO: atrás dela ainda dá para soldar depois de juntar. Ali passam
{n_esq} fios: os 9 da tela, o do botão verde, o GND dos botões e o da coluna 1 4 7 *. Cada fio entra por trás num furo da coluna E
(o GND, na D) e, na frente, é dobrado até o pino da barra F ao lado e soldado junto, como na direita.</p>
<ol class="faz">
  <li>Os 3 de baixo: solde a ponta atrás nas perninhas (<span class="furo">B34</span>, <span class="furo">B32</span>) e no fio nu da
  coluna C (<span class="furo">C38</span>); suba pela coluna A e entre no furo da tabela.</li>
  <li>Os 9 da tela: comece pela ponta de baixo (entra no furo E, ponte com o F na frente) e leve o fio pelo caminho da tabela até a
  linha 1. Corte com 5 mm a mais além do furo da linha 1 de onde vai sair o pino dele: essa ponta fica solta até o passo 10.</li>
  <li>Na linha 10, a ponte junta três furos: <span class="furo">D10</span>–<span class="furo">E10</span>–<span class="furo">F10</span>
  (os dois fios de GND e o pino GND da LILYGO).</li>
</ol>
{alterna_tela()}
<div data-tela-mostra="normal">{figura([tras_d_n], "Atrás, depois de juntar (espelhada). A LILYGO tapa quase tudo; os fios ficam na faixa da esquerda e na ponta de cima. O alto-falante fica embaixo das linhas 4 a 6, da coluna D para a direita: ali não passa fio.")}{tabela_fios("esquerda")}</div>
<div data-tela-mostra="espelhada" hidden>{figura([tras_d_e], "A mesma vista para a tela com o VCC à direita: os fios são os mesmos, só saem de outras colunas.")}{tabela_fios("esquerda", True)}</div>""",
        ["Os 3 fios de baixo estão soldados atrás e em ponte na frente (E16–F16, E18–F18, D10–E10–F10).",
         "Os 9 fios da tela estão em ponte na frente (E9, E10, E12, E13, E14, E15, E21, E22, E23 com o F ao lado).",
         "Nenhum fio passa por cima da LILYGO, nem nas linhas 4 a 6 da coluna D para a direita."]))

    passos.append(passo(10, "tela", "A tela", "40 minutos", f"""
<ol class="faz">
  <li>Confira a <a href="#conferir">conferência B</a> (de que lado fica o VCC) e use a vista certa nos passos 8 a 10.</li>
  <li>Encaixe os 14 pinos da tela na linha 1, de <span class="furo">C1</span> a <span class="furo">P1</span>, pela frente. A tela fica
  deitada sobre o plástico da barra, encostado na placa. Solde atrás os 14 (os 5 do toque não ligam em nada, só seguram).</li>
  <li>Atrás, solde a ponta de cada fio no pino dele, rente à placa.</li>
  <li>Os capacitores, entre o pino VCC e o GND da tela, também rente à placa: o de 100 nF deitado entre os dois pinos; o de 100 µF
  com a perninha mais curta (a do lado da faixa clara) no GND. Ele fica em pé, com o corpo apontando para o fundo da caixa, e todo
  entre a borda de cima da placa e a linha 4: acima da borda desce a lingueta da frente, e abaixo da linha 4 fica o alto-falante.
  Dobre as perninhas para o corpo ficar uns 2 mm mais para baixo que os pinos. Deitado ele não cabe.</li>
  <li>Se você chegou a testar os cerâmicos do diagnóstico da tela branca (README, "Atacar a causa"), eles também vão aqui, nos
  pinos: 100 nF entre o RESET e o GND, e 100 pF (ou 1 nF) junto do de 100 nF do VCC.</li>
  <li>Corte as sobras dos 14 pinos a uns 2 mm da placa (só os pinos, não as perninhas dos capacitores).</li>
  <li>Opcional: um quadradinho de espuma adesiva de 2 mm na placa, embaixo da ponta de baixo da tela, num ponto onde atrás da tela
  não há peça. Ele apoia a tela quando você aperta o vidro.</li>
</ol>
{alterna_tela()}
<div data-tela-mostra="normal">{tabela_tela()}</div>
<div data-tela-mostra="espelhada" hidden>{tabela_tela(True)}</div>
{figura([corte], "O corte de lado da montagem dentro da caixa, em escala.", corte=True)}""",
        ["Os 14 pinos da tela estão soldados, com a tela paralela à placa.",
         "Os 9 fios estão nos pinos certos (tabela), e os capacitores, entre VCC e GND.",
         "O capacitor de 100 µF está em pé, entre a borda de cima da placa e a linha 4."]))

    passos.append(passo(11, "firmware", "Firmware e primeiro teste", "15 minutos · com a pilha e o USB", f"""
<ol class="faz">
  <li>No <code>include/config.h</code>, mude para <code>#define MONTAGEM_PLACA 1</code> e grave o firmware. Isso troca os pinos: a
  coluna 1 4 7 * vai no IO17, o verde no IO35 e o vermelho no IO21.</li>
  <li>Ensine o teclado: segure o 1 enquanto aparece "Ligando o modem…" (ou mande <code>teclado aprender</code> no Monitor Serial) e
  aperte as teclas que a tela pedir.</li>
  <li>Teste: o verde liga, o vermelho encerra. Segurado, o vermelho desliga o Tijolo; segurado de novo por 1 s, liga.</li>
</ol>""",
        ["A tela acende com a imagem certa (se ficar de cabeça para baixo, TELA_ROTACAO = 2 no config.h).",
         "As 12 teclas e os 2 botões funcionam.",
         "O vermelho segurado desliga o Tijolo e, segurado de novo, liga."]))

    passos.append(passo(12, "fechar", "Fechar a caixa", "15 minutos", f"""
<ol class="faz">
  <li><b>Antena</b>: tire o papel da fita e cole na parede da esquerda, por dentro, no rebaixo de 0,3 mm. O cabo já está na
  LILYGO (passo 7); acomode a sobra embaixo da faixa da esquerda da placa.</li>
  <li><b>Alto-falante</b>: em pé no berço de cima, com o cone virado para a grade da parede de cima.</li>
  <li><b>Microfone</b>: deitado no berço de baixo, com a face de feltro contra os três furinhos.</li>
  <li>Encaixe os cabos do alto-falante e do microfone nos conectores SPEK e MIC, do lado da pilha da LILYGO, perto da ponta do
  modem. O do alto-falante desce pela esquerda, ao lado do suporte da pilha, e precisa de uns 12 cm: se o seu ficou curto, emende.</li>
  <li>Baixe o sanduíche reto: os 4 pilares do fundo entram nos furos da LILYGO.</li>
  <li>Baixe a frente reta: as duas linguetas de cima fazem clic. Aperte os 2 parafusos M2 × 8 por trás, embaixo.</li>
</ol>
<div class="nota">Para abrir: tire os 2 parafusos, levante a frente por baixo e puxe firme para soltar as linguetas. A chave
liga/desliga da LILYGO fica por dentro: no dia a dia o vermelho desliga; para guardar por muitos dias, abra e desligue a chave.</div>""",
        ["A antena está colada e o cabo dela não ficou preso entre as peças.",
         "Os cabos do alto-falante e do microfone estão encaixados.",
         "As linguetas fizeram clic e os 2 parafusos estão apertados sem forçar.",
         "O cabo USB-C entra até o fim pela abertura da direita (é o USB da ESP32, o de gravar e carregar).",
         "Com a caixa fechada, numa ligação a tela apaga um instante e volta (o normal do Tijolo na troca 4G ↔ 2G), sem ficar branca."]))

    medidas_campos = P.MEDIDAS
    campos_html = "".join(
        f"<div class='campo'><label for='m-{i}'>{esc(t)}{f'<span class=pequeno>{esc(d)}</span>' if d else ''}</label>"
        f"<input id='m-{i}' name='{i}' value='{esc(v)}' inputmode='decimal' autocomplete='off'></div>"
        for i, t, d, v in medidas_campos)

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
  <div class="eyebrow">Tijolo · Fase 4 · montagem</div>
  <h1>Montagem do Tijolo</h1>
  <p class="lead">Uma placa ilhada na frente, com as 14 teclas e a tela, encaixada direto nas barras de pinos da LILYGO. Na frente
  da placa não corre nenhum fio: só pontes de solda. Os fios ficam atrás, curtos e escondidos, e a caixa impressa em 3D fecha tudo.</p>
  <ul class="fichas">
    <li>placa 18 × 51 furos</li><li>{len(F.CHAVES)} chaves tácteis</li><li>{n_dir + n_esq} fios, todos atrás</li>
    <li>caixa {mm(larg_caixa)} × {mm(comp_caixa)} × {mm(P.Z_TOPO)} mm</li><li>6 a 8 horas de bancada</li>
  </ul>
  <div class="vista" id="vista">
    <img src="capa.png" alt="O Tijolo montado, visto de frente e de lado: a tela em cima e o teclado embaixo." id="vista-capa">
    <div class="dica" id="vista-dica" hidden>arraste para girar</div>
    <div class="controles" id="vista-controles" role="group" aria-label="Vista 3D" hidden>
      <button class="chip" type="button" data-modo="fechado" aria-pressed="true">Fechado</button>
      <button class="chip" type="button" data-modo="aberto" aria-pressed="false">Aberto</button>
      <button class="chip" type="button" data-modo="corte" aria-pressed="false">Cortado ao meio</button>
    </div>
  </div>
  <p class="pequeno">A caixa em 3D. Em "Aberto" e "Cortado ao meio" aparece o miolo, em blocos simples: a LILYGO, a placa da
  frente com as chaves, a tela, a pilha, o alto-falante e o microfone.</p>
</header>

<h2 id="conferir">Antes de começar: três conferências</h2>
<p>As três decidem detalhes da montagem. Olhe as peças agora, antes de comprar e de soldar.</p>
<h3>A. As barras de pinos da LILYGO</h3>
<ul>
  <li><b>Ainda soltas</b>: ótimo, elas vão soldadas no passo 1.</li>
  <li><b>Soldadas com o plástico do lado dos componentes</b> (o lado do ESP32 e do modem), pinos compridos para cima: ótimo, falta só
  o segundo plástico.</li>
  <li><b>Soldadas com o plástico do lado da pilha</b>: precisam sair. Corte o plástico entre os pinos com o alicate de corte, aqueça
  e puxe um pino de cada vez com a pinça, limpe os furos com a malha e solde barras novas do jeito certo.</li>
</ul>
<h3>B. De que lado fica o VCC da tela</h3>
<p>Vire a tela e leia os nomes da fileira de pinos, atrás da plaquinha: o <b>VCC</b> fica numa das pontas. Depois desvire: olhando a
tela de frente, com os pinos em cima, de que lado ficou o VCC? Os fios são os mesmos nas duas montagens, só saem de outras colunas.
Escolha aqui; os desenhos e as tabelas dos passos 8 a 10 seguem a escolha.</p>
{alterna_tela()}
<h3>C. O cartão micro SD atrás da tela</h3>
<p>A tela fica a {mm(P.T.espaco, 2)} mm da placa (um plástico de barra). Embaixo dela, a placa só tem soldas baixas, longe do meio. Se a
peça mais alta atrás da tela passar de 2,3 mm, ou se o cartão SD ficar perto dos pinos e não na outra ponta, me avise: a tela passa
a usar dois plásticos e a caixa fica 2,5 mm mais grossa.</p>

<h2 id="comprar">O que comprar</h2>
<div class="rolagem"><table>
<thead><tr><th>Peça</th><th>Quantas</th><th></th></tr></thead>
<tbody>
<tr><td>Placa ilhada <b>dupla face, furos metalizados</b>, 9 × 15 cm ou maior (precisa de 18 × 51 furos)</td><td>1</td><td class="comprar">comprar</td></tr>
<tr><td>Chave táctil 6 × 6 × 5 mm, 4 terminais (todas iguais)</td><td>14 (+ sobra)</td><td class="comprar">comprar</td></tr>
<tr><td>Barra de pinos macho 1 × 40, 2,54 mm (para os dois plásticos extras)</td><td>1</td><td class="comprar">comprar</td></tr>
<tr><td>Fita Kapton, 10 mm</td><td>1 rolo</td><td class="comprar">comprar</td></tr>
<tr><td>Multímetro com bip de continuidade</td><td>1</td><td class="comprar">se não tiver</td></tr>
<tr><td>Fio 30 AWG (você tem o de silicone em 3 cores; mais cores ajudam a seguir as tabelas)</td><td>~2 m</td><td class="tem">tem</td></tr>
<tr><td>Fio nu fino: pernas de resistor cortadas</td><td>~30 cm</td><td class="tem">tem</td></tr>
<tr><td>Capacitores de 100 nF e 100 µF (kit Eletrogate)</td><td>1 + 1</td><td class="tem">tem</td></tr>
<tr><td>Parafusos M2 × 8, cabeça chata (sortidos)</td><td>2</td><td class="tem">tem</td></tr>
<tr><td>Impressão 3D: fundo, frente, 12 teclas, verde e vermelha</td><td>1 jogo</td><td class="comprar">encomendar</td></tr>
<tr><td>Espuma adesiva de 2 mm (um quadradinho, para apoiar a ponta de baixo da tela)</td><td>um pouco</td><td>opcional</td></tr>
</tbody></table></div>

<h2 id="como-fica">Como fica</h2>
<p>De trás para a frente: o fundo da caixa, a pilha 18650 no suporte da própria LILYGO, a LILYGO com os componentes para a frente,
{mm(P.VAO, 2)} mm de vão (os dois plásticos das barras), a placa da frente, e por cima dela as teclas e a tela. A LILYGO fica
encostada na borda direita, com os USB-C na parede da direita; a faixa da esquerda da placa (colunas A a D) fica livre atrás, para os
fios.</p>
<p>A regra que deixa tudo limpo: <b>na frente da placa não corre fio</b>. Um fio que vem de trás atravessa um furo vizinho do pino e
uma ponte de solda o liga ao pino. A ordem dos passos segue isso: o que fica entre as placas é feito antes de juntar, e testado.</p>

{''.join(passos)}

<h2 id="caixa">Imprimir a caixa</h2>
<p>Cinco arquivos STL, já virados do jeito de imprimir. Mande para a gráfica 3D junto com estas instruções:</p>
<div class="rolagem"><table>
<thead><tr><th>Arquivo</th><th>O que é</th><th>Como imprimir</th></tr></thead>
<tbody>
<tr><td class="mono">fundo.stl</td><td>a metade de trás</td><td>a face de trás na mesa; sem suporte</td></tr>
<tr><td class="mono">frente.stl</td><td>a metade da frente, com a janela da tela e os poços das teclas</td><td>a face da frente na mesa; sem suporte</td></tr>
<tr><td class="mono">teclas.stl</td><td>as 12 teclas, com as letras gravadas</td><td>em pé, como estão; camada de 0,12 mm</td></tr>
<tr><td class="mono">tecla_verde.stl</td><td>o botão verde</td><td>filamento verde</td></tr>
<tr><td class="mono">tecla_vermelha.stl</td><td>o botão vermelho</td><td>filamento vermelho</td></tr>
</tbody></table></div>
<p><b>Material</b>: PETG (as linguetas de cima vergam para encaixar e o PETG não trinca) ou PLA. Camada de 0,2 mm, 3 perímetros,
20% de preenchimento. As letras das teclas ficam bonitas pintadas: passe tinta e limpe o topo com pano.</p>
<div class="aviso"><b>Não encomende ainda.</b> A caixa foi desenhada com as medidas do desenho do fabricante da tela e do alto-falante.
Meça as suas peças e me mande as medidas abaixo; se alguma for diferente, eu ajusto os arquivos.</div>

<h2 id="medidas">Medidas para a caixa</h2>
<p>Com paquímetro, em milímetros. Os valores que já estão nos campos são os que a caixa usa hoje. Troque o que for diferente e
copie o texto para me mandar na conversa.</p>
<form class="medidas" id="form-medidas">
{campos_html}
<div class="campo"><label for="m-vcc">Tela: o VCC, de frente com os pinos em cima, fica</label>
<select id="m-vcc" name="vcc"><option>à esquerda</option><option>à direita</option></select></div>
<div class="campo"><label for="m-barras">Barras de pinos da LILYGO</label>
<select id="m-barras" name="barras"><option>soltas</option><option>plástico do lado dos componentes</option><option>plástico do lado da pilha</option></select></div>
<button class="botao" type="submit">Copiar as medidas</button>
<textarea id="medidas-texto" readonly aria-label="As medidas, para copiar"></textarea>
<span class="copiado" id="copiado" aria-live="polite"></span>
</form>

<h2 id="depois">Depois: a placa de circuito</h2>
<p>Com a placa ilhada funcionando, o mesmo desenho vira uma placa de circuito de verdade: o mesmo tamanho, os mesmos furos para as
barras da LILYGO, as teclas e a tela nos mesmos lugares, e as trilhas no lugar dos fios. Ela se encomenda pronta (JLCPCB e
parecidas) e cabe na mesma caixa. Fica para quando a placa ilhada estiver testada.</p>

<h2 id="problemas">Se algo não funcionar</h2>
<ul>
  <li><b>Uma tecla só não responde</b>: refaça o teste do passo 6 para ela, agora com as pontas nos pinos da barra R e no fio C38.
  Normalmente é a solda de uma perninha.</li>
  <li><b>Uma fileira inteira (ou uma coluna) não responde</b>: a ponte de solda do lado direito daquela linha, ou o fio dela.</li>
  <li><b>A tela fica branca ou preta</b>: confira os 9 fios da tela com o multímetro, do pino da tela até o pino da LILYGO, e o lado
  do VCC (conferência B).</li>
  <li><b>A tela fica branca nas ligações mais vezes do que antes</b>: a antena fica na parede da esquerda, a poucos milímetros dos
  fios da tela (o maço da coluna B). Teste com a antena fora da caixa, longe. Se melhorar, me avise: dá para mudar o lugar dela ou o
  caminho dos fios. O firmware continua apagando a luz e refazendo a tela na troca de rede, como hoje.</li>
  <li><b>O vermelho não liga o Tijolo</b>: ele precisa estar no IO21 e o firmware com <code>MONTAGEM_PLACA 1</code>.</li>
  <li><b>A frente não fecha</b>: veja se o capacitor de 100 µF passou da borda de cima da placa (passo 10), se algum pino da
  linha 1 ficou comprido ou se a placa passou da medida (passo 2).</li>
  <li><b>O sanduíche não desce até os pilares</b>: a pilha pode ser mais gorda que a do desenho. Meça e me mande (o campo
  "LILYGO com a pilha", em <a href="#medidas">Medidas</a>).</li>
  <li><b>O computador não acha o Tijolo para gravar</b>: com a caixa fechada não dá para apertar o BOOT e o RST da LILYGO. Abra,
  tire o sanduíche e use os dois botõezinhos do lado da pilha, na ponta do ESP32: segure o BOOT, aperte e solte o RST, solte o
  BOOT e grave.</li>
</ul>
<p class="pequeno">Os desenhos e a caixa saem de <code>montagem/</code> no repositório (projeto.py, fiacao.py, desenho.py, caixa/caixa.py):
mudando uma medida lá, tudo se refaz igual.</p>
</main>
</div>
<script src="{CDN}/build/three.min.js"></script>
<script src="{CDN}/examples/js/controls/OrbitControls.js"></script>
<script>{JS}</script>
"""
    return corpo


JS = r"""
(function () {
  // ---------- guardar as conferências e a escolha da tela (só neste navegador) ----------
  function ler(k, padrao) { try { var v = localStorage.getItem(k); return v === null ? padrao : v; } catch (e) { return padrao; } }
  function gravar(k, v) { try { localStorage.setItem(k, v); } catch (e) {} }
  document.querySelectorAll('input[data-confere]').forEach(function (c) {
    c.checked = ler('tijolo-' + c.dataset.confere, '0') === '1';
    c.addEventListener('change', function () { gravar('tijolo-' + c.dataset.confere, c.checked ? '1' : '0'); });
  });
  function tela(modo) {
    document.querySelectorAll('[data-tela-mostra]').forEach(function (el) { el.hidden = el.dataset.telaMostra !== modo; });
    document.querySelectorAll('button[data-tela]').forEach(function (b) { b.setAttribute('aria-pressed', b.dataset.tela === modo ? 'true' : 'false'); });
    var sel = document.getElementById('m-vcc'); if (sel) sel.value = modo === 'espelhada' ? 'à direita' : 'à esquerda';
    gravar('tijolo-tela', modo);
  }
  document.querySelectorAll('button[data-tela]').forEach(function (b) { b.addEventListener('click', function () { tela(b.dataset.tela); }); });
  tela(ler('tijolo-tela', 'normal'));

  // ---------- medidas ----------
  var form = document.getElementById('form-medidas');
  var saida = document.getElementById('medidas-texto');
  function texto() {
    var linhas = ['Medidas do Tijolo (mm):'];
    form.querySelectorAll('.campo').forEach(function (c) {
      var l = c.querySelector('label'), i = c.querySelector('input, select');
      var nome = l.childNodes[0].textContent.trim();
      linhas.push('- ' + nome + ': ' + i.value);
    });
    return linhas.join('\n');
  }
  function atualizar() { saida.value = texto(); }
  form.addEventListener('input', atualizar);
  form.addEventListener('change', function (e) { if (e.target.id === 'm-vcc') tela(e.target.value === 'à direita' ? 'espelhada' : 'normal'); atualizar(); });
  form.addEventListener('submit', function (e) {
    e.preventDefault(); atualizar();
    var aviso = document.getElementById('copiado');
    function feito() { aviso.textContent = 'Copiado. Cole na conversa.'; }
    function selecionar() { saida.focus(); saida.select(); aviso.textContent = 'Selecionei o texto: copie com Ctrl+C (ou toque e segure).'; }
    try { navigator.clipboard.writeText(saida.value).then(feito, selecionar); } catch (err) { selecionar(); }
  });
  atualizar();

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
  var cam = new THREE.PerspectiveCamera(28, W / H, 1, 3000);
  cam.up.set(0, 0, 1);
  cena.add(new THREE.HemisphereLight(0xffffff, 0x4a5650, 0.85));
  var luz = new THREE.DirectionalLight(0xffffff, 0.65); luz.position.set(-80, -140, 220); cena.add(luz);
  var luz2 = new THREE.DirectionalLight(0xffffff, 0.3); luz2.position.set(120, 90, -80); cena.add(luz2);
  var corte = new THREE.Plane(new THREE.Vector3(-1, 0, 0), 22.86);
  var centro = new THREE.Vector3(23, 52, 10);
  // de onde a câmera olha em cada modo: [alvo, deslocamento da câmera]
  var olhar = {
    fechado: [[23, 52, 10], [-112, -196, 166]],
    aberto: [[23, 56, 30], [-128, -224, 196]],
    corte: [[23, 66, 18], [255, -25, 45]]
  };
  function enquadrar(m) {
    var o = olhar[m];
    ctl.target.set(o[0][0], o[0][1], o[0][2]);
    cam.position.set(o[0][0] + o[1][0], o[0][1] + o[1][1], o[0][2] + o[1][2]);
  }
  cam.position.set(centro.x - 112, centro.y - 196, centro.z + 166);
  var ctl = new THREE.OrbitControls(cam, r.domElement);
  ctl.target.copy(centro); ctl.enableDamping = true; ctl.enablePan = false; ctl.minDistance = 120; ctl.maxDistance = 520;
  var pecas = [
    { arq: 'fundo', cor: 0x3b4046, grupo: 'caixa' }, { arq: 'frente', cor: 0x4f565e, grupo: 'frente' },
    { arq: 'teclas', cor: 0xe6e2d8, grupo: 'frente' }, { arq: 'tecla_verde', cor: 0x2f9e58, grupo: 'frente' },
    { arq: 'tecla_vermelha', cor: 0xcf3b33, grupo: 'frente' },
    { arq: 'miolo_lilygo', cor: 0x2f6247, grupo: 'miolo' }, { arq: 'miolo_placa', cor: 0x3c7a57, grupo: 'miolo' },
    { arq: 'miolo_tela', cor: 0x1a2740, grupo: 'tela' }, { arq: 'miolo_pilha', cor: 0xc46a2b, grupo: 'miolo' },
    { arq: 'miolo_som', cor: 0xb08840, grupo: 'miolo' }
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
      m.visible = !(modo === 'fechado' && g === 'miolo');
      m.position.z = (modo === 'aberto' && g === 'frente') ? 42 : 0;
    });
  }
  function comecar() {
    caixa.insertBefore(r.domElement, caixa.firstChild);
    document.getElementById('vista-capa').hidden = true;
    document.getElementById('vista-dica').hidden = false;
    document.getElementById('vista-controles').hidden = false;
    pronto = true; aplicar(); animar();
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
    D.gerar(os.path.join(AQUI, "saida"))
    pag = pagina()
    with open(os.path.join(SAIDA, "index.html"), "w", encoding="utf-8") as f:
        f.write(pag)
    vis = os.path.join(AQUI, "saida", "caixa", "visual")
    with open(os.path.join(SAIDA, "3d", "pecas.json"), "w", encoding="ascii") as f:
        json.dump(pacote_3d(vis), f, separators=(",", ":"))
    for velho in os.listdir(os.path.join(SAIDA, "3d")):
        if velho.endswith(".stl"):
            os.remove(os.path.join(SAIDA, "3d", velho))
    return os.path.join(SAIDA, "index.html")


if __name__ == "__main__":
    print(gerar(), os.path.getsize(os.path.join(SAIDA, "index.html")) // 1024, "KB")
