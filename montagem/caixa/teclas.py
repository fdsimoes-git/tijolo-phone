"""
As teclas do Tijolo para imprimir: 12 teclas (1 a 0, * e #) e os botões
verde e vermelho. Cada tecla tem uma aba embaixo (fica presa pela frente da
caixa) e um pino que aperta a chave táctil 6×6 mm da placa.

As medidas que mudam com as peças estão em `Medidas` (montagem/caixa/medidas.py).
"""
import cadquery as cq

FONTE = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

LEGENDAS = [
    ("1", ".,?"), ("2", "abc"), ("3", "def"),
    ("4", "ghi"), ("5", "jkl"), ("6", "mno"),
    ("7", "pqrs"), ("8", "tuv"), ("9", "wxyz"),
    ("*", ""), ("0", "␣"), ("#", ""),
]


def tecla(m, numero: str, letras: str, largura=None, altura=None, simbolo=None):
    """Uma tecla, de pé (o topo para cima, em +Z), com a base em z = 0.

    - aba: m.aba_folga em volta, m.aba_espessura de altura (fica por dentro da frente)
    - corpo: atravessa a frente (m.frente_espessura) e sai m.tecla_saliencia para fora
    - pino: desce da aba até encostar no botão da chave (m.pino_comprimento)
    """
    lg = largura or m.tecla_largura
    al = altura or m.tecla_altura
    r = min(m.tecla_raio, al / 2 - 0.01)
    corpo_h = m.frente_espessura + m.tecla_saliencia
    aba = (cq.Workplane("XY")
           .rect(lg + 2 * m.aba_folga, al + 2 * m.aba_folga)
           .extrude(m.aba_espessura)
           .edges("|Z").fillet(r + m.aba_folga * 0.6))
    corpo = (cq.Workplane("XY").workplane(offset=m.aba_espessura)
             .rect(lg, al).extrude(corpo_h)
             .edges("|Z").fillet(r)
             .faces(">Z").edges().fillet(0.6))
    pino = (cq.Workplane("XY").circle(m.pino_diametro / 2)
            .extrude(-m.pino_comprimento))
    t = aba.union(corpo).union(pino)
    topo = m.aba_espessura + corpo_h
    # legenda gravada no topo (0,4 mm de fundo): o número grande e as letras pequenas
    fundo = 0.4
    if simbolo is not None:
        txt = (cq.Workplane("XY").workplane(offset=topo - fundo)
               .text(simbolo, al * 0.62, fundo + 0.2, fontPath=FONTE, halign="center", valign="center"))
        t = t.cut(txt)
    else:
        n = (cq.Workplane("XY").workplane(offset=topo - fundo)
             .center(-lg * 0.18 if letras else 0, 0)
             .text(numero, al * 0.6, fundo + 0.2, fontPath=FONTE, halign="center", valign="center"))
        t = t.cut(n)
        if letras:
            l = (cq.Workplane("XY").workplane(offset=topo - fundo)
                 .center(lg * 0.2, -al * 0.08)
                 .text(letras, al * 0.3, fundo + 0.2, fontPath=FONTE, halign="center", valign="center"))
            t = t.cut(l)
    return t
