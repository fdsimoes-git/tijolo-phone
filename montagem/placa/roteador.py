"""
Um roteador simples para placa de 2 camadas: grade fina e A* (com vias).

Tudo em mm, nas coordenadas do Tijolo (X para a direita, Y para cima). As
camadas: 0 = frente (F.Cu), 1 = trás (B.Cu).

Como funciona: cada célula da grade guarda de quem ela é. Uma célula vale
para o centro de uma trilha da net N se nenhum cobre de outra net estiver a
menos de (folga + meia trilha) dela; o mesmo para o centro de uma via, com
(folga + meia via). O cobre de cada net vai sendo "pintado" na grade, já
inflado; onde duas nets se encostam, a célula fica proibida para todas.

As nets com vários pinos viram uma árvore: começa num pino e liga o pino que
falta mais perto, partindo de qualquer ponto do que já está ligado.
"""
import heapq
import math

import numpy as np
import shapely
from shapely.geometry import LineString, Point

FRENTE, TRAS = 0, 1
RAIZ2 = math.sqrt(2.0)
VIZINHOS = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
            (1, 1, RAIZ2), (1, -1, RAIZ2), (-1, 1, RAIZ2), (-1, -1, RAIZ2)]


class SemCaminho(Exception):
    pass


class Grade:
    def __init__(self, contorno, passo=0.2, trilha=0.3, folga=0.2, via=0.6, borda=0.35, custo_via=12.0,
                 furo_via=0.3):
        self.passo = passo
        self.furo_via = furo_via
        self.trilha, self.folga, self.via = trilha, folga, via
        self.custo_via = custo_via
        x0, y0, x1, y1 = contorno.bounds
        self.x0, self.y0 = x0, y0
        self.nx = int(math.ceil((x1 - x0) / passo)) + 1
        self.ny = int(math.ceil((y1 - y0) / passo)) + 1
        xs = x0 + np.arange(self.nx) * passo
        ys = y0 + np.arange(self.ny) * passo
        self.XX, self.YY = np.meshgrid(xs, ys)
        # a margem da rasterização: o centro da célula pode estar até meio passo da linha
        self.inf_trilha = folga + trilha / 2 + passo * 0.75
        self.inf_via = folga + via / 2 + passo * 0.75
        dentro = shapely.contains_xy(contorno.buffer(-(borda + trilha / 2)), self.XX, self.YY)
        dentro_via = shapely.contains_xy(contorno.buffer(-(borda + via / 2)), self.XX, self.YY)
        self.dono = [np.where(dentro, 0, -1).astype(np.int32) for _ in range(2)]
        self.dono_via = [np.where(dentro_via, 0, -1).astype(np.int32) for _ in range(2)]
        # para cada net: a geometria de cobre por camada (para saber onde ela já está)
        self.cobre_net = {}
        # o custo extra de passar por uma célula (camada, j, i): para empurrar trilhas para longe de áreas
        self.custo = [np.zeros((self.ny, self.nx), dtype=np.float32) for _ in range(2)]

    # ------------------------------------------------------------------ grade
    def _janela(self, geom):
        x0, y0, x1, y1 = geom.bounds
        i0 = max(0, int((x0 - self.x0) / self.passo) - 1)
        i1 = min(self.nx, int((x1 - self.x0) / self.passo) + 2)
        j0 = max(0, int((y0 - self.y0) / self.passo) - 1)
        j1 = min(self.ny, int((y1 - self.y0) / self.passo) + 2)
        return j0, j1, i0, i1

    def _pinta(self, mapa, geom, net):
        if geom.is_empty:
            return
        j0, j1, i0, i1 = self._janela(geom)
        if j1 <= j0 or i1 <= i0:
            return
        sub = mapa[j0:j1, i0:i1]
        m = shapely.contains_xy(geom, self.XX[j0:j1, i0:i1], self.YY[j0:j1, i0:i1])
        if net < 0:
            sub[m] = -1
            return
        livre = m & (sub == 0)
        outro = m & (sub > 0) & (sub != net)
        sub[livre] = net
        sub[outro] = -1

    def cobre(self, geom, net, camadas):
        """o cobre de uma net (pad, trilha, via) nas camadas dadas"""
        for c in camadas:
            self._pinta(self.dono[c], geom.buffer(self.inf_trilha, 16), net)
            self._pinta(self.dono_via[c], geom.buffer(self.inf_via, 16), net)
            d = self.cobre_net.setdefault(net, [[], []])
            d[c].append(geom)

    def proibe(self, geom, camadas=(FRENTE, TRAS), so_vias=False):
        """área onde nada passa (furo sem cobre, peça alta, etc.)"""
        for c in camadas:
            if not so_vias:
                self._pinta(self.dono[c], geom.buffer(self.folga + self.trilha / 2), -1)
            self._pinta(self.dono_via[c], geom.buffer(self.folga + self.via / 2), -1)

    def sem_via(self, geom):
        """nenhuma via aqui (perto de furos: a distância entre furos)"""
        for c in (FRENTE, TRAS):
            self._pinta(self.dono_via[c], geom, -1)

    def evita(self, geom, camadas, custo):
        """deixa mais caro passar por uma área (não proíbe)"""
        for c in camadas:
            j0, j1, i0, i1 = self._janela(geom)
            m = shapely.contains_xy(geom, self.XX[j0:j1, i0:i1], self.YY[j0:j1, i0:i1])
            self.custo[c][j0:j1, i0:i1][m] += custo

    def celula(self, x, y):
        return int(round((y - self.y0) / self.passo)), int(round((x - self.x0) / self.passo))

    def ponto(self, j, i):
        return float(self.x0 + i * self.passo), float(self.y0 + j * self.passo)

    def celulas_de(self, geom, camadas):
        """as células cujo centro cai dentro da geometria"""
        j0, j1, i0, i1 = self._janela(geom)
        m = shapely.contains_xy(geom, self.XX[j0:j1, i0:i1], self.YY[j0:j1, i0:i1])
        js, is_ = np.nonzero(m)
        out = []
        for c in camadas:
            out.extend((c, int(j + j0), int(i + i0)) for j, i in zip(js, is_))
        return out

    # ------------------------------------------------------------------ busca
    def _livre(self, c, j, i, net):
        if j < 0 or i < 0 or j >= self.ny or i >= self.nx:
            return False
        v = self.dono[c][j, i]
        return v == 0 or v == net

    def _via_livre(self, j, i, net):
        a, b = self.dono_via[0][j, i], self.dono_via[1][j, i]
        return (a == 0 or a == net) and (b == 0 or b == net)

    def busca(self, net, origens, alvos, preferencia=None):
        """A* das células de origem até qualquer célula de alvo. origens, alvos: [(c, j, i)].
        preferencia: {camada: "h" ou "v"}: andar na outra direção custa um pouco mais."""
        alvo = set(alvos)
        if not alvo:
            raise SemCaminho("sem alvo")
        cj = [a[1] for a in alvos]
        ci = [a[2] for a in alvos]
        jmin, jmax, imin, imax = min(cj), max(cj), min(ci), max(ci)

        def h(j, i):
            dj = max(0, jmin - j, j - jmax)
            di = max(0, imin - i, i - imax)
            return (dj + di) + (RAIZ2 - 2) * min(dj, di)

        n = 2 * self.ny * self.nx
        g = np.full(n, np.inf, dtype=np.float64)
        pai = np.full(n, -1, dtype=np.int64)
        fila = []
        for c, j, i in origens:
            k = (c * self.ny + j) * self.nx + i
            if g[k] > 0:
                g[k] = 0.0
                heapq.heappush(fila, (h(j, i), 0.0, k))
        pen = {FRENTE: (1.0, 1.0), TRAS: (1.0, 1.0)}
        if preferencia:
            for c, d in preferencia.items():
                pen[c] = (1.0, 1.25) if d == "h" else (1.25, 1.0)    # (custo em x, custo em y)
        while fila:
            f, gk, k = heapq.heappop(fila)
            if gk > g[k]:
                continue
            c, resto = divmod(k, self.ny * self.nx)
            j, i = divmod(resto, self.nx)
            if (c, j, i) in alvo:
                caminho = []
                while k >= 0:
                    c2, r2 = divmod(k, self.ny * self.nx)
                    j2, i2 = divmod(r2, self.nx)
                    caminho.append((c2, j2, i2))
                    k = pai[k]
                return caminho[::-1]
            px, py = pen[c]
            for dj, di, passo in VIZINHOS:
                jj, ii = j + dj, i + di
                if not self._livre(c, jj, ii, net):
                    continue
                if dj and di and not (self._livre(c, j, ii, net) and self._livre(c, jj, i, net)):
                    continue
                custo = passo * (px if di and not dj else py if dj and not di else (px + py) / 2)
                custo += self.custo[c][jj, ii]
                kk = (c * self.ny + jj) * self.nx + ii
                ng = gk + custo
                if ng < g[kk]:
                    g[kk] = ng
                    pai[kk] = k
                    heapq.heappush(fila, (ng + h(jj, ii), ng, kk))
            # via
            if self._via_livre(j, i, net):
                cc = 1 - c
                kk = (cc * self.ny + j) * self.nx + i
                ng = gk + self.custo_via
                if ng < g[kk]:
                    g[kk] = ng
                    pai[kk] = k
                    heapq.heappush(fila, (ng + h(j, i), ng, kk))
        raise SemCaminho(f"net {net}: sem caminho")

    # ------------------------------------------------------- caminho → peças
    def _reta_livre(self, c, a, b, net):
        """a trilha reta de a até b (centros) cabe na camada c?"""
        (xa, ya), (xb, yb) = a, b
        d = math.hypot(xb - xa, yb - ya)
        n = max(2, int(d / (self.passo * 0.4)) + 2)
        t = np.linspace(0.0, 1.0, n)
        fi = (xa + (xb - xa) * t - self.x0) / self.passo
        fj = (ya + (yb - ya) * t - self.y0) / self.passo
        i = np.rint(fi).astype(np.int64)
        j = np.rint(fj).astype(np.int64)
        di = np.where(fi - i > 0, 1, -1) * (np.abs(fi - i) > 0.25)
        dj = np.where(fj - j > 0, 1, -1) * (np.abs(fj - j) > 0.25)
        mapa = self.dono[c]
        for jj, ii in ((j, i), (j, i + di), (j + dj, i), (j + dj, i + di)):
            if (jj < 0).any() or (ii < 0).any() or (jj >= self.ny).any() or (ii >= self.nx).any():
                return False
            v = mapa[jj, ii]
            if not np.all((v == 0) | (v == net)):
                return False
        return True

    @staticmethod
    def _conectores(a, b):
        """jeitos de ir de a até b só com retas a 0°, 45° e 90° (uma ou duas)"""
        (xa, ya), (xb, yb) = a, b
        dx, dy = xb - xa, yb - ya
        adx, ady = abs(dx), abs(dy)
        if adx < 1e-6 or ady < 1e-6 or abs(adx - ady) < 1e-6:
            return [[b]]
        sx = 1 if dx > 0 else -1
        sy = 1 if dy > 0 else -1
        m = min(adx, ady)
        diag_primeiro = (xa + sx * m, ya + sy * m)
        reta_primeiro = (xb - sx * m, yb - sy * m)
        return [[diag_primeiro, b], [reta_primeiro, b]]

    def _octilinear(self, c, pts, net):
        """estica o caminho em trechos de 0°, 45° e 90°, indo o mais longe que dá a cada vez"""
        out = [pts[0]]
        k, n = 0, len(pts)

        def chega(m):
            for conn in self._conectores(pts[k], pts[m]):
                anteriores = [pts[k]] + conn[:-1]
                if all(self._reta_livre(c, p, q, net) for p, q in zip(anteriores, conn)):
                    return conn
            return None

        while k < n - 1:
            # galope: dobra o passo enquanto dá, depois busca binária
            melhor, conn_melhor = k + 1, chega(k + 1) or [pts[k + 1]]
            passo = 1
            while True:
                m = min(n - 1, k + passo * 2)
                cm = chega(m)
                if cm is None:
                    lo, hi = k + passo, m
                    break
                melhor, conn_melhor = m, cm
                if m == n - 1:
                    lo = hi = m
                    break
                passo *= 2
            while hi - lo > 1:
                mid = (lo + hi) // 2
                cm = chega(mid)
                if cm is None:
                    hi = mid
                else:
                    lo, melhor, conn_melhor = mid, mid, cm
            out.extend(conn_melhor)
            k = melhor
        # junta trechos na mesma direção
        limpo = [out[0]]
        for p in out[1:]:
            if math.hypot(p[0] - limpo[-1][0], p[1] - limpo[-1][1]) < 1e-6:
                continue
            if len(limpo) >= 2:
                a, b = limpo[-2], limpo[-1]
                v1 = (b[0] - a[0], b[1] - a[1])
                v2 = (p[0] - b[0], p[1] - b[1])
                if abs(v1[0] * v2[1] - v1[1] * v2[0]) < 1e-6 and v1[0] * v2[0] + v1[1] * v2[1] > 0:
                    limpo[-1] = p
                    continue
            limpo.append(p)
        return limpo

    def pecas(self, caminho, net):
        """o caminho de células → trilhas [(camada, [(x, y), ...])] e vias [(x, y)], em trechos de 45°"""
        trechos, vias = [], []
        atual = [caminho[0]]
        for a, b in zip(caminho, caminho[1:]):
            if a[0] != b[0]:
                vias.append(self.ponto(a[1], a[2]))
                trechos.append(atual)
                atual = [b]
            else:
                atual.append(b)
        trechos.append(atual)
        trilhas = []
        for t in trechos:
            if len(t) < 2:
                continue
            c = t[0][0]
            pts = [self.ponto(j, i) for _, j, i in t]
            trilhas.append((c, self._octilinear(c, pts, net)))
        return trilhas, vias

    def marca_rota(self, net, trilhas, vias):
        """pinta o cobre de verdade da rota (a trilha com a largura dela, a via inteira)"""
        for c, pts in trilhas:
            linha = LineString(pts) if len(pts) > 1 else Point(pts[0])
            self.cobre(linha.buffer(self.trilha / 2, 8), net, [c])
        for x, y in vias:
            self.cobre(Point(x, y).buffer(self.via / 2, 16), net, [FRENTE, TRAS])
            self.sem_via(Point(x, y).buffer(self.furo_via + 0.25 + self.passo, 16))
