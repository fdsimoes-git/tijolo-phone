#include "Quadro.h"
#include <math.h>
#include <algorithm>
#include "Texto.h"

namespace {

inline int menor(int a, int b) { return a < b ? a : b; }
inline int maior(int a, int b) { return a > b ? a : b; }
inline float entre01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
inline uint32_t alfaDe(float cobertura) { return (uint32_t)(cobertura * 255.f + 0.5f); }

// 1/16 de ponto -> ponto inteiro mais próximo (funciona com negativos)
inline int arredondar16(int32_t v) { return (int)floorf((v + 8) / 16.0f); }

// Letras que não ocupam espaço: controles, hífen invisível, marcas de
// junção e de variação de emoji, tons de pele.
bool invisivel(uint32_t cp) {
  return cp < 0x20 || cp == 0x7F || cp == 0xAD || (cp >= 0x200B && cp <= 0x200F) ||
         (cp >= 0x2060 && cp <= 0x2064) || (cp >= 0xFE00 && cp <= 0xFE0F) || cp == 0xFEFF ||
         (cp >= 0x1F3FB && cp <= 0x1F3FF) || (cp >= 0xE0000 && cp <= 0xE007F);
}

const char* const RETICENCIAS = "\xE2\x80\xA6";   // "…"

}  // namespace

uint16_t Quadro::misturar565(uint16_t f, uint16_t c, uint32_t a) {
  if (a >= 255) return c;
  if (a == 0) return f;
  const uint32_t na = 255 - a;
  const uint32_t r = ((uint32_t)(c >> 11) * a + (uint32_t)(f >> 11) * na + 127) / 255;
  const uint32_t g = ((uint32_t)((c >> 5) & 63) * a + (uint32_t)((f >> 5) & 63) * na + 127) / 255;
  const uint32_t b = ((uint32_t)(c & 31) * a + (uint32_t)(f & 31) * na + 127) / 255;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

void Quadro::recorte(int x, int y, int w, int h) {
  _x0 = maior(0, x);
  _y0 = maior(0, y);
  _x1 = maior(_x0, menor(LARGURA, x + w));
  _y1 = maior(_y0, menor(ALTURA, y + h));
}

void Quadro::semRecorte() {
  _x0 = 0; _y0 = 0; _x1 = LARGURA; _y1 = ALTURA;
}

void Quadro::limpar(uint16_t cor) { retangulo(0, 0, LARGURA, ALTURA, cor); }

void Quadro::ponto(int x, int y, uint16_t cor) {
  if (x < _x0 || x >= _x1 || y < _y0 || y >= _y1) return;
  _p[y * LARGURA + x] = cor;
}

void Quadro::misturar(int x, int y, uint16_t cor, uint32_t alfa) {
  if (alfa == 0 || x < _x0 || x >= _x1 || y < _y0 || y >= _y1) return;
  uint16_t& d = _p[y * LARGURA + x];
  d = misturar565(d, cor, alfa);
}

void Quadro::retangulo(int x, int y, int w, int h, uint16_t cor) {
  const int xa = maior(x, _x0), xb = menor(x + w, _x1);
  const int ya = maior(y, _y0), yb = menor(y + h, _y1);
  if (xa >= xb || ya >= yb) return;
  for (int yy = ya; yy < yb; yy++) {
    uint16_t* linha = _p + yy * LARGURA;
    for (int xx = xa; xx < xb; xx++) linha[xx] = cor;
  }
}

void Quadro::moldura(int x, int y, int w, int h, uint16_t cor, int e) {
  if (w <= 0 || h <= 0 || e <= 0) return;
  retangulo(x, y, w, e, cor);
  retangulo(x, y + h - e, w, e, cor);
  retangulo(x, y + e, e, h - 2 * e, cor);
  retangulo(x + w - e, y + e, e, h - 2 * e, cor);
}

void Quadro::retanguloRedondo(int x, int y, int w, int h, int r, uint16_t cor) {
  if (w <= 0 || h <= 0) return;
  r = menor(r, menor(w, h) / 2);
  if (r <= 0) { retangulo(x, y, w, h, cor); return; }
  retangulo(x, y + r, w, h - 2 * r, cor);                 // faixa do meio
  for (int i = 0; i < r; i++) {                           // faixas de cima e de baixo
    const int yCima = y + i, yBaixo = y + h - 1 - i;
    retangulo(x + r, yCima, w - 2 * r, 1, cor);
    retangulo(x + r, yBaixo, w - 2 * r, 1, cor);
    const float dy = r - i - 0.5f;
    for (int j = 0; j < r; j++) {                         // os quatro cantos
      const float dx = r - j - 0.5f;
      const float cob = entre01(r - sqrtf(dx * dx + dy * dy) + 0.5f);
      if (cob <= 0.f) continue;
      const uint32_t a = alfaDe(cob);
      misturar(x + j, yCima, cor, a);
      misturar(x + w - 1 - j, yCima, cor, a);
      misturar(x + j, yBaixo, cor, a);
      misturar(x + w - 1 - j, yBaixo, cor, a);
    }
  }
}

void Quadro::circulo(float cx, float cy, float r, uint16_t cor) {
  const int xa = (int)floorf(cx - r - 1), xb = (int)ceilf(cx + r + 1);
  const int ya = (int)floorf(cy - r - 1), yb = (int)ceilf(cy + r + 1);
  for (int yy = ya; yy <= yb; yy++) {
    const float dy = yy + 0.5f - cy;
    for (int xx = xa; xx <= xb; xx++) {
      const float dx = xx + 0.5f - cx;
      const float cob = entre01(r - sqrtf(dx * dx + dy * dy) + 0.5f);
      if (cob > 0.f) misturar(xx, yy, cor, alfaDe(cob));
    }
  }
}

void Quadro::anel(float cx, float cy, float r, float e, uint16_t cor) {
  const int xa = (int)floorf(cx - r - 1), xb = (int)ceilf(cx + r + 1);
  const int ya = (int)floorf(cy - r - 1), yb = (int)ceilf(cy + r + 1);
  for (int yy = ya; yy <= yb; yy++) {
    const float dy = yy + 0.5f - cy;
    for (int xx = xa; xx <= xb; xx++) {
      const float dx = xx + 0.5f - cx;
      const float d = sqrtf(dx * dx + dy * dy);
      const float cob = entre01(r - d + 0.5f) - entre01(r - e - d + 0.5f);
      if (cob > 0.f) misturar(xx, yy, cor, alfaDe(cob));
    }
  }
}

void Quadro::triangulo(float x0, float y0, float x1, float y1, float x2, float y2, uint16_t cor) {
  // Cobertura aproximada pela distância até a borda mais próxima.
  const float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
  if (area == 0.f) return;
  const float sinal = area > 0.f ? 1.f : -1.f;
  const float xs[3] = {x0, x1, x2}, ys[3] = {y0, y1, y2};
  float nx[3], ny[3], c[3];
  for (int k = 0; k < 3; k++) {
    const int m = (k + 1) % 3;
    const float ex = xs[m] - xs[k], ey = ys[m] - ys[k];
    const float len = sqrtf(ex * ex + ey * ey);
    nx[k] = -ey / len * sinal;
    ny[k] = ex / len * sinal;
    c[k] = -(nx[k] * xs[k] + ny[k] * ys[k]);
  }
  const int xa = (int)floorf(fminf(x0, fminf(x1, x2))) - 1, xb = (int)ceilf(fmaxf(x0, fmaxf(x1, x2))) + 1;
  const int ya = (int)floorf(fminf(y0, fminf(y1, y2))) - 1, yb = (int)ceilf(fmaxf(y0, fmaxf(y1, y2))) + 1;
  for (int yy = ya; yy <= yb; yy++) {
    for (int xx = xa; xx <= xb; xx++) {
      const float px = xx + 0.5f, py = yy + 0.5f;
      float dist = 1e9f;
      for (int k = 0; k < 3; k++) dist = fminf(dist, nx[k] * px + ny[k] * py + c[k]);
      const float cob = entre01(dist + 0.5f);
      if (cob > 0.f) misturar(xx, yy, cor, alfaDe(cob));
    }
  }
}

// ------------------------------------------------------------
// Texto
// ------------------------------------------------------------

const Glifo* Quadro::_glifo(const Fonte& f, uint32_t cp) {
  if (cp == '\t') cp = ' ';
  if (invisivel(cp)) return nullptr;
  const Glifo* g = f.procurar(cp);
  return g ? g : &f.glifos[f.substituto];
}

void Quadro::_letra(int x, int base, const Glifo& g, const Fonte& f, uint16_t cor) {
  if (!g.largura || !g.altura) return;
  const int x0 = x + g.dx, y0 = base + g.dy;
  if (x0 >= _x1 || y0 >= _y1 || x0 + g.largura <= _x0 || y0 + g.altura <= _y0) return;
  const uint8_t* bits = f.bits + g.inicio;
  for (int yy = 0; yy < g.altura; yy++) {
    const int py = y0 + yy;
    if (py < _y0 || py >= _y1) continue;
    uint16_t* linha = _p + py * LARGURA;
    for (int xx = 0; xx < g.largura; xx++) {
      const int px = x0 + xx;
      if (px < _x0 || px >= _x1) continue;
      const int i = yy * g.largura + xx;
      const uint8_t tom = (i & 1) ? (bits[i >> 1] & 0x0F) : (bits[i >> 1] >> 4);
      if (!tom) continue;
      linha[px] = (tom == 15) ? cor : misturar565(linha[px], cor, tom * 17u);
    }
  }
}

int Quadro::texto(int x, int base, const std::string& s, const Fonte& f, uint16_t cor, Alinha alinha) {
  const int w = largura(s, f);
  if (alinha == Alinha::Centro) x -= w / 2;
  else if (alinha == Alinha::Direita) x -= w;
  int32_t caneta = x * 16;
  size_t i = 0;
  while (i < s.size()) {
    const Glifo* g = _glifo(f, texto::lerCodepoint(s, i));
    if (!g) continue;
    _letra(arredondar16(caneta), base, *g, f, cor);
    caneta += g->avanco;
  }
  return w;
}

int Quadro::largura(const std::string& s, const Fonte& f) {
  int32_t total = 0;
  size_t i = 0;
  while (i < s.size()) {
    const Glifo* g = _glifo(f, texto::lerCodepoint(s, i));
    if (g) total += g->avanco;
  }
  return arredondar16(total);
}

std::string Quadro::caber(const std::string& s, const Fonte& f, int larguraMax) {
  if (largura(s, f) <= larguraMax) return s;
  // As contas em 1/16 de ponto: arredondar cada pedaço pode passar 1 ponto.
  const std::string ret = RETICENCIAS;
  int32_t ret16 = 0;
  for (size_t k = 0; k < ret.size();) {
    const Glifo* g = _glifo(f, texto::lerCodepoint(ret, k));
    if (g) ret16 += g->avanco;
  }
  const int32_t limite16 = (int32_t)larguraMax * 16 - ret16;
  if (limite16 < 0) return "";                 // nem as reticências cabem
  int32_t total = 0;
  size_t i = 0, corte = 0;
  while (i < s.size()) {
    const size_t inicio = i;
    const Glifo* g = _glifo(f, texto::lerCodepoint(s, i));
    const int32_t novo = total + (g ? g->avanco : 0);
    if (novo > limite16) { corte = inicio; break; }
    total = novo;
    corte = i;
  }
  std::string r = s.substr(0, corte);
  while (!r.empty() && r.back() == ' ') r.pop_back();
  return r + RETICENCIAS;
}

std::vector<Quadro::Trecho> Quadro::quebrarTudo(const std::string& s, const Fonte& f, int larguraMax) {
  std::vector<Trecho> linhas;
  const int32_t max16 = (int32_t)larguraMax * 16;
  auto avanco = [&](uint32_t cp) -> int32_t {
    const Glifo* g = _glifo(f, cp);
    return g ? g->avanco : 0;
  };
  size_t inicio = 0;           // começo da linha atual
  size_t depoisDoEspaco = 0;   // onde dá para quebrar (logo depois do último espaço)
  bool podeQuebrar = false;
  int32_t x = 0;               // largura da linha atual, em 1/16 de ponto
  size_t i = 0;
  while (i < s.size()) {
    const size_t aqui = i;
    const uint32_t cp = texto::lerCodepoint(s, i);
    if (cp == '\n') {
      linhas.push_back({inicio, aqui});
      inicio = i;
      x = 0;
      podeQuebrar = false;
      continue;
    }
    const int32_t w = avanco(cp);
    if (cp != ' ' && x + w > max16 && aqui > inicio) {
      if (podeQuebrar) {
        // A palavra desce inteira para a linha de baixo
        linhas.push_back({inicio, depoisDoEspaco});
        inicio = depoisDoEspaco;
        podeQuebrar = false;
        x = 0;
        for (size_t k = inicio; k < aqui;) x += avanco(texto::lerCodepoint(s, k));
      }
      if (x + w > max16 && aqui > inicio) {   // palavra maior que a linha: corta aqui
        linhas.push_back({inicio, aqui});
        inicio = aqui;
        x = 0;
      }
    }
    x += w;
    if (cp == ' ') {
      depoisDoEspaco = i;
      podeQuebrar = true;
    }
  }
  linhas.push_back({inicio, s.size()});
  return linhas;
}

std::vector<std::string> Quadro::quebrar(const std::string& s, const Fonte& f, int larguraMax) {
  std::vector<std::string> linhas;
  size_t ini = 0;
  while (ini <= s.size()) {
    size_t fim = s.find('\n', ini);
    if (fim == std::string::npos) fim = s.size();
    std::string par = s.substr(ini, fim - ini);
    for (char& c : par) if (c == '\t') c = ' ';
    par.erase(std::remove(par.begin(), par.end(), '\r'), par.end());

    std::string linha;
    size_t i = 0;
    bool teveAlgo = false;
    while (i < par.size()) {
      while (i < par.size() && par[i] == ' ') i++;
      size_t j = i;
      while (j < par.size() && par[j] != ' ') j++;
      if (j == i) break;
      const std::string palavra = par.substr(i, j - i);
      i = j;
      teveAlgo = true;
      const std::string tentativa = linha.empty() ? palavra : linha + " " + palavra;
      if (largura(tentativa, f) <= larguraMax) { linha = tentativa; continue; }
      if (!linha.empty()) { linhas.push_back(linha); linha.clear(); }
      if (largura(palavra, f) <= larguraMax) { linha = palavra; continue; }
      // Palavra maior que a linha (um link, por exemplo): corta letra a letra.
      size_t k = 0;
      while (k < palavra.size()) {
        const size_t antes = k;
        texto::lerCodepoint(palavra, k);
        const std::string letra = palavra.substr(antes, k - antes);
        if (!linha.empty() && largura(linha + letra, f) > larguraMax) {
          linhas.push_back(linha);
          linha.clear();
        }
        linha += letra;
      }
    }
    if (!linha.empty() || !teveAlgo) linhas.push_back(linha);
    if (fim >= s.size()) break;
    ini = fim + 1;
  }
  return linhas;
}
