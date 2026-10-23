"""
Desenho de placa ilhada (perfboard) em SVG: furos, peças e fios.

As posições são em furos: coluna (A, B, C... da esquerda para a direita,
vista pela frente) e linha (1, 2, 3... de cima para baixo). "C7" = coluna C,
linha 7. A vista de trás é espelhada (a placa virada de lado), com as
mesmas letras: é assim que a placa fica na mão na hora de soldar os fios.
"""
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

PASSO = 2.54          # mm entre furos
ESCALA = 8.0          # px por mm no desenho


def letra(c: int) -> str:
    """0 → A, 25 → Z, 26 → AA..."""
    s = ""
    c += 1
    while c:
        c, r = divmod(c - 1, 26)
        s = chr(65 + r) + s
    return s


def furo(nome: str) -> Tuple[int, int]:
    """"C7" → (2, 6)"""
    i = 0
    while i < len(nome) and nome[i].isalpha():
        i += 1
    col = 0
    for ch in nome[:i].upper():
        col = col * 26 + (ord(ch) - 64)
    return col - 1, int(nome[i:]) - 1


@dataclass
class Peca:
    nome: str
    furos: List[str]                      # os pinos que entram na placa
    contorno: Tuple[float, float, float, float]   # x0, y0, x1, y1 em furos (pode ser fracionário)
    lado: str = "frente"                  # "frente" ou "tras"
    cor: str = "#3b4252"
    texto: str = ""                       # rótulo dentro do contorno
    rotulos: Optional[List[str]] = None   # nome de cada pino (na mesma ordem de `furos`)
    arredondar: float = 0.6
    opaca: bool = False


@dataclass
class Fio:
    caminho: List[str]                    # furos por onde passa (o primeiro e o último são as pontas)
    cor: str = "#d08770"
    lado: str = "tras"
    nome: str = ""
    largura: float = 0.5                  # mm


@dataclass
class Placa:
    colunas: int
    linhas: int
    nome: str = ""
    pecas: List[Peca] = field(default_factory=list)
    fios: List[Fio] = field(default_factory=list)
    marcas: List[Tuple[str, str]] = field(default_factory=list)   # (furo, texto) notas
    cortes: List[Tuple[int, int, int, int]] = field(default_factory=list)

    # ---------- geometria ----------
    def xy(self, nome: str, espelhar: bool) -> Tuple[float, float]:
        c, l = furo(nome)
        if espelhar:
            c = self.colunas - 1 - c
        return (MARGEM + c * PASSO) * ESCALA, (MARGEM + l * PASSO) * ESCALA

    def xyf(self, c: float, l: float, espelhar: bool) -> Tuple[float, float]:
        if espelhar:
            c = self.colunas - 1 - c
        return (MARGEM + c * PASSO) * ESCALA, (MARGEM + l * PASSO) * ESCALA

    def tamanho_mm(self) -> Tuple[float, float]:
        return (self.colunas - 1) * PASSO + 2 * BORDA, (self.linhas - 1) * PASSO + 2 * BORDA

    # ---------- desenho ----------
    def svg(self, lado: str = "frente", titulo: str = "") -> str:
        esp = lado == "tras"
        w = ((self.colunas - 1) * PASSO + 2 * MARGEM) * ESCALA
        h = ((self.linhas - 1) * PASSO + 2 * MARGEM) * ESCALA
        p = []
        p.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w:.0f} {h:.0f}" '
                 f'class="placa" role="img" aria-label="{titulo or self.nome} ({lado})">')
        # a placa
        x0 = (MARGEM - BORDA) * ESCALA
        y0 = (MARGEM - BORDA) * ESCALA
        pw = ((self.colunas - 1) * PASSO + 2 * BORDA) * ESCALA
        ph = ((self.linhas - 1) * PASSO + 2 * BORDA) * ESCALA
        p.append(f'<rect x="{x0:.1f}" y="{y0:.1f}" width="{pw:.1f}" height="{ph:.1f}" rx="6" class="pcb"/>')
        # letras e números
        for c in range(self.colunas):
            if c % 2 == 0 or self.colunas <= 26:
                x, _ = self.xyf(c, 0, esp)
                p.append(f'<text x="{x:.1f}" y="{(MARGEM - BORDA - 1.2) * ESCALA:.1f}" class="eixo">{letra(c)}</text>')
        for l in range(self.linhas):
            _, y = self.xyf(0, l, esp)
            xl = (MARGEM - BORDA - 1.6) * ESCALA
            p.append(f'<text x="{xl:.1f}" y="{y + 4:.1f}" class="eixo eixoL">{l + 1}</text>')
        # furos
        for c in range(self.colunas):
            for l in range(self.linhas):
                x, y = self.xyf(c, l, esp)
                p.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{0.85 * ESCALA:.1f}" class="ilha"/>'
                         f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{0.45 * ESCALA:.1f}" class="buraco"/>')
        # peças: as do lado que está à vista, cheias; as do outro lado, só o contorno tracejado
        for pc in self.pecas:
            visivel = pc.lado == lado
            xa, ya = self.xyf(pc.contorno[0], pc.contorno[1], esp)
            xb, yb = self.xyf(pc.contorno[2], pc.contorno[3], esp)
            xa, xb = min(xa, xb), max(xa, xb)
            ya, yb = min(ya, yb), max(ya, yb)
            r = pc.arredondar * ESCALA
            if visivel:
                op = "1" if pc.opaca else "0.82"
                p.append(f'<rect x="{xa:.1f}" y="{ya:.1f}" width="{xb - xa:.1f}" height="{yb - ya:.1f}" rx="{r:.1f}" '
                         f'fill="{pc.cor}" fill-opacity="{op}" class="peca"/>')
            else:
                p.append(f'<rect x="{xa:.1f}" y="{ya:.1f}" width="{xb - xa:.1f}" height="{yb - ya:.1f}" rx="{r:.1f}" '
                         f'class="pecaOutroLado"/>')
            if pc.texto:
                cls = "rotPeca" if visivel else "rotPeca rotFraco"
                linhas = pc.texto.split("\n")
                yc = (ya + yb) / 2 - (len(linhas) - 1) * 7
                for i, t in enumerate(linhas):
                    p.append(f'<text x="{(xa + xb) / 2:.1f}" y="{yc + i * 14 + 4:.1f}" class="{cls}">{t}</text>')
        # pinos das peças (os dois lados mostram onde o pino atravessa)
        for pc in self.pecas:
            for i, f in enumerate(pc.furos):
                x, y = self.xy(f, esp)
                p.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{0.95 * ESCALA:.1f}" class="pino"/>')
                if pc.rotulos and pc.lado == lado and pc.rotulos[i]:
                    p.append(f'<text x="{x:.1f}" y="{y - 1.2 * ESCALA:.1f}" class="rotPino">{pc.rotulos[i]}</text>')
        # fios
        for fo in self.fios:
            pts = [self.xy(f, esp) for f in fo.caminho]
            d = " ".join(f"{'M' if i == 0 else 'L'}{x:.1f},{y:.1f}" for i, (x, y) in enumerate(pts))
            if fo.lado == lado:
                p.append(f'<path d="{d}" stroke="{fo.cor}" stroke-width="{fo.largura * ESCALA:.1f}" class="fio"/>')
                for x, y in (pts[0], pts[-1]):
                    p.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{0.75 * ESCALA:.1f}" fill="{fo.cor}" class="solda"/>')
            else:
                p.append(f'<path d="{d}" stroke="{fo.cor}" stroke-width="{0.25 * ESCALA:.1f}" class="fioOutroLado"/>')
        for f, t in self.marcas:
            x, y = self.xy(f, esp)
            p.append(f'<text x="{x + 1.3 * ESCALA:.1f}" y="{y + 4:.1f}" class="nota">{t}</text>')
        p.append("</svg>")
        return "\n".join(p)


MARGEM = 4.0    # mm do primeiro furo até a borda do desenho (cabem as letras)
BORDA = 1.8     # mm do primeiro furo até a borda da placa
