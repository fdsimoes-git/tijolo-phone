#!/usr/bin/env python3
"""
Gera lib/Tela/Fontes.cpp: as letras do Tijolo já desenhadas, prontas para
a ESP32 só copiar para a tela.

Cada letra vira uma imagem pequena com 16 tons de cinza (4 bits por ponto),
o que deixa as bordas suaves (anti-aliasing). O conjunto inclui o alfabeto
latino com acentos (ã, õ, ç, é...), símbolos comuns (…, –, “ ”, €) e setas.

Fonte: Inter (https://rsms.me/inter), licença SIL Open Font License 1.1.
Só precisa rodar de novo se mudar a fonte, os tamanhos ou as letras:

    python3 ferramentas/gerar_fontes.py [pasta_da_inter]

Precisa de Python com Pillow (pip install pillow). O Pillow precisa do
"raqm" para os números com largura fixa; sem ele, o script iguala a largura
dos números sozinho.
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont, features

PASTA = sys.argv[1] if len(sys.argv) > 1 else "/usr/share/fonts/opentype/inter"
RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SAIDA = os.path.join(RAIZ, "lib", "Tela", "Fontes.cpp")
PREVIA = os.path.join(RAIZ, "test_host", "saida", "fontes.png")

ASCII = [chr(c) for c in range(0x20, 0x7F)]
LATIN1 = [chr(c) for c in range(0xA0, 0x100) if c != 0xAD]   # sem o hífen invisível
EXTRAS = list("…–—‘’“”•€▲▼◀▶●○←→↑↓✓✗□")
TEXTO = ASCII + LATIN1 + EXTRAS
DISCADOR = list(" 0123456789*#+()-.:□")
RELOGIO = list(" 0123456789:-*#□")

# nome no C++, arquivo, tamanho em pixels, letras, números com largura fixa
FONTES = [
    ("fontePequena", "Inter-Medium.otf", 14, TEXTO, False),
    ("fonteTexto", "Inter-Regular.otf", 18, TEXTO, False),
    ("fonteTitulo", "Inter-SemiBold.otf", 18, TEXTO, False),
    ("fonteGrande", "Inter-SemiBold.otf", 26, TEXTO, False),
    ("fonteDiscador", "InterDisplay-Medium.otf", 38, DISCADOR, True),
    ("fonteRelogio", "InterDisplay-Light.otf", 68, RELOGIO, True),
]

TEM_RAQM = features.check("raqm")


def carregar(arquivo, tamanho):
    caminho = os.path.join(PASTA, arquivo)
    motor = ImageFont.Layout.RAQM if TEM_RAQM else ImageFont.Layout.BASIC
    return ImageFont.truetype(caminho, tamanho, layout_engine=motor)


def glifo(fonte, ch, tabular):
    feats = ["tnum"] if (tabular and TEM_RAQM) else None
    kw = {"anchor": "ls"}
    if feats:
        kw["features"] = feats
    avanco = fonte.getlength(ch, features=feats) if feats else fonte.getlength(ch)
    x0, y0, x1, y1 = fonte.getbbox(ch, **kw)
    w, h = max(0, x1 - x0), max(0, y1 - y0)
    pontos = []
    if w and h:
        img = Image.new("L", (w, h), 0)
        ImageDraw.Draw(img).text((-x0, -y0), ch, font=fonte, fill=255, **kw)
        pontos = list(img.tobytes())
    return {"ch": ch, "avanco": avanco, "dx": x0, "dy": y0, "w": w, "h": h, "pontos": pontos}


def gerar(nome, arquivo, tamanho, letras, tabular):
    fonte = carregar(arquivo, tamanho)
    gs = [glifo(fonte, ch, tabular) for ch in letras]
    if tabular:
        # Mesmo com "tnum", garante que todos os números tenham a mesma largura
        # (o relógio e o cronômetro não podem "dançar").
        nums = [g for g in gs if g["ch"].isdigit()]
        maior = max(g["avanco"] for g in nums)
        for g in nums:
            folga = (maior - g["avanco"]) / 2
            g["dx"] += round(folga)
            g["avanco"] = maior
    asc, desc = fonte.getmetrics()
    _, hy0, _, _ = fonte.getbbox("H", anchor="ls")
    _, xy0, _, _ = fonte.getbbox("x", anchor="ls")
    maiuscula = -hy0
    minuscula = -xy0 if "x" in letras else maiuscula
    gs.sort(key=lambda g: ord(g["ch"]))
    return {
        "nome": nome, "arquivo": arquivo, "tamanho": tamanho, "glifos": gs,
        "asc": asc, "desc": desc, "maiuscula": maiuscula, "minuscula": minuscula,
    }


def empacotar(pontos):
    """16 tons: dois pontos por byte, o primeiro nos 4 bits de cima."""
    tons = [min(15, (p * 15 + 127) // 255) for p in pontos]
    if len(tons) % 2:
        tons.append(0)
    return bytes((tons[i] << 4) | tons[i + 1] for i in range(0, len(tons), 2))


def cpp(fontes):
    out = []
    out.append("// ============================================================")
    out.append("//  Fontes.cpp — GERADO por ferramentas/gerar_fontes.py; não edite à mão.")
    out.append("//  Letras da fonte Inter (c) 2016 The Inter Project Authors,")
    out.append("//  SIL Open Font License 1.1 (ver LICENCA-Inter.txt nesta pasta).")
    out.append("// ============================================================")
    out.append('#include "Fontes.h"')
    out.append("")
    out.append("namespace fontes {")
    for f in fontes:
        bits = bytearray()
        linhas = []
        substituto = 0
        for i, g in enumerate(f["glifos"]):
            ini = len(bits)
            bits += empacotar(g["pontos"])
            cod = ord(g["ch"])
            if g["ch"] == "□":
                substituto = i
            nome_ch = g["ch"] if 0x21 <= cod < 0x7F and g["ch"] not in "\\" else f"U+{cod:04X}"
            linhas.append(
                f"  {{0x{cod:04X}, {round(g['avanco'] * 16)}, {g['w']}, {g['h']}, "
                f"{g['dx']}, {g['dy']}, {ini}}},  // {nome_ch}"
            )
        n = f["nome"]
        out.append("")
        out.append(f"// {n}: {f['arquivo']} {f['tamanho']} px, {len(f['glifos'])} letras, {len(bits)} bytes")
        out.append(f"static const uint8_t bits_{n}[] = {{")
        for i in range(0, len(bits), 24):
            out.append("  " + ",".join(f"0x{b:02X}" for b in bits[i:i + 24]) + ",")
        out.append("};")
        out.append(f"static const Glifo glifos_{n}[] = {{")
        out += linhas
        out.append("};")
        out.append(
            f"const Fonte {n} = {{glifos_{n}, {len(f['glifos'])}, bits_{n}, "
            f"{f['asc']}, {f['desc']}, {f['maiuscula']}, {f['minuscula']}, {substituto}}};"
        )
    out.append("")
    out.append("}  // namespace fontes")
    out.append("")
    return "\n".join(out)


def previa(fontes):
    os.makedirs(os.path.dirname(PREVIA), exist_ok=True)
    altura = sum(f["asc"] + f["desc"] + 8 for f in fontes) + 8
    img = Image.new("L", (900, altura), 0)
    y = 4
    for f in fontes:
        x = 4
        base = y + f["asc"]
        for g in f["glifos"]:
            if x + g["w"] + 4 > 900:
                break
            if g["w"]:
                gl = Image.new("L", (g["w"], g["h"]))
                gl.putdata(g["pontos"])
                img.paste(gl, (round(x) + g["dx"], base + g["dy"]))
            x += g["avanco"]
        y += f["asc"] + f["desc"] + 8
    img.save(PREVIA)


def main():
    fontes = [gerar(*f) for f in FONTES]
    with open(SAIDA, "w", encoding="utf-8") as arq:
        arq.write(cpp(fontes))
    previa(fontes)
    total = sum(len(empacotar(g["pontos"])) for f in fontes for g in f["glifos"])
    print(f"{SAIDA}: {len(fontes)} fontes, {total} bytes de desenho (raqm: {TEM_RAQM})")
    for f in fontes:
        print(f"  {f['nome']:14s} asc {f['asc']:2d} desc {f['desc']:2d} maiúscula {f['maiuscula']:2d} minúscula {f['minuscula']:2d}")


if __name__ == "__main__":
    main()
