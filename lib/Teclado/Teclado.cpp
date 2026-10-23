#include "Teclado.h"
#include <initializer_list>

void Teclado::begin(const int* fios, int nFios, int verde, int vermelho, const AcessoPinos& acesso) {
  _io = acesso;
  _nFios = nFios > MAX_FIOS ? MAX_FIOS : nFios;
  for (int i = 0; i < _nFios; i++) {
    _fios[i] = fios[i];
    _io.entrada(_fios[i]);
  }
  _verde = verde;
  _vermelho = vermelho;
  if (_verde >= 0) _io.entrada(_verde);
  if (_vermelho >= 0) _io.entrada(_vermelho);
  mapaPadrao(_mapa);
  _temMapa = false;
  _primeira = true;
}

void Teclado::mapaPadrao(uint8_t mapa[TECLAS]) {
  for (int i = 0; i < TECLAS; i++) {
    const int linha = i / 3, coluna = i % 3;    // ORDEM segue o desenho do teclado
    mapa[i] = (uint8_t)((linha << 4) | (4 + coluna));
  }
}

void Teclado::definirMapa(const uint8_t mapa[TECLAS]) {
  for (int i = 0; i < TECLAS; i++) _mapa[i] = mapa[i];
  _temMapa = true;
}

void Teclado::aprender(bool sim) {
  _aprender = sim;
  _fTeclado = Firmeza();     // esquece o que estava apertado no outro modo...
  ignorarAteSoltar();        // ...e a tecla que trocou o modo não conta de novo
}

void Teclado::varrer(uint32_t agora) {
  if (!_primeira && agora - _ultima < VARRER_MS) return;
  // O loop ficou parado (comando lento ao modem, RESET da tela): esse tempo
  // não conta como "segurando", senão um toque rápido no meio viraria botão.
  const uint32_t passou = agora - _ultima;
  if (!_primeira && passou > LOOP_PARADO_MS)
    for (Firmeza* f : {&_fTeclado, &_fVerde, &_fVermelho})
      if (f->valendo) f->apertouEm += passou - VARRER_MS;
  _primeira = false;
  _ultima = agora;
  bool algo = false;
  const int tecla = _lerTeclado(algo);
  const int verde = (_verde >= 0 && !_io.ler(_verde)) ? TECLA_VERDE : 0;
  const int vermelho = (_vermelho >= 0 && !_io.ler(_vermelho)) ? TECLA_VERMELHA : 0;
  if (_ignorar) {
    if (algo || verde || vermelho) {          // ainda tem algo apertado
      _soltoMarcado = false;
      return;
    }
    if (!_soltoMarcado) {
      _soltoMarcado = true;
      _soltoDesde = agora;
    }
    if (agora - _soltoDesde < FIRME_MS) return;   // solto, mas pode ser tremido
    _ignorar = false;                         // tudo solto: daqui em diante vale
    _fTeclado = _fVerde = _fVermelho = Firmeza();
  }
  _filtrar(_fTeclado, tecla, agora, _aprender);
  if (_verde >= 0) _filtrar(_fVerde, verde, agora, false);
  if (_vermelho >= 0) _filtrar(_fVermelho, vermelho, agora, false);
}

bool Teclado::fiosDaTecla(char tecla, int& pinoA, int& pinoB) const {
  if (tecla == TECLA_VERDE) tecla = _comoVerde;
  else if (tecla == TECLA_VERMELHA) tecla = _comoVermelho;
  for (int i = 0; tecla && i < TECLAS; i++) {
    if (ORDEM[i] != tecla) continue;
    const uint8_t par = _mapa[i];
    const int a = par >> 4, b = par & 0x0F;
    if (par == SEM_PAR || a >= _nFios || b >= _nFios) return false;
    pinoA = _fios[a];
    pinoB = _fios[b];
    return true;
  }
  return false;
}

bool Teclado::segurando(char tecla) {
  if (tecla == TECLA_VERDE && _verde >= 0 && !_io.ler(_verde)) return true;
  if (tecla == TECLA_VERMELHA && _vermelho >= 0 && !_io.ler(_vermelho)) return true;
  int a, b;
  if (!fiosDaTecla(tecla, a, b)) return false;
  _io.puxarParaBaixo(a);
  _io.esperarUs(10);
  const bool encostou = !_io.ler(b);
  _io.entrada(a);
  _io.esperarUs(10);
  return encostou;
}

int Teclado::_lerTeclado(bool& algo) {
  // Puxa um fio por vez para baixo e vê quais dos seguintes descem junto.
  int achados = 0;
  uint8_t par = SEM_PAR;
  for (int a = 0; a < _nFios - 1; a++) {
    _io.puxarParaBaixo(_fios[a]);
    _io.esperarUs(10);
    for (int b = a + 1; b < _nFios; b++) {
      if (!_io.ler(_fios[b])) {
        achados++;
        par = (uint8_t)((a << 4) | b);
      }
    }
    _io.entrada(_fios[a]);
    _io.esperarUs(10);
  }
  algo = achados > 0;
  if (achados != 1) return 0;               // nada, ou mais de uma tecla ao mesmo tempo
  if (_aprender) return 1 + par;
  for (int i = 0; i < TECLAS; i++)
    if (_mapa[i] == par) return ORDEM[i];
  return 0;                                 // par que não está no mapa
}

// A tecla faz as vezes de qual botão (0 = de nenhum)
char Teclado::_botaoDe(int tecla) const {
  if (!tecla) return 0;
  if (tecla == _comoVerde) return TECLA_VERDE;
  if (tecla == _comoVermelho) return TECLA_VERMELHA;
  return 0;
}

void Teclado::_filtrar(Firmeza& f, int leitura, uint32_t agora, bool pares) {
  if (leitura != f.lida) {
    f.lida = leitura;
    f.desde = agora;
  } else if (leitura != f.valendo && agora - f.desde >= FIRME_MS) {
    if (f.valendo && !pares) _soltou(f);
    f.valendo = leitura;
    f.apertouEm = f.desceuEm = agora;
    f.segurou = f.virouBotao = f.segurouBotao = false;
    if (leitura) {
      EventoTecla ev;
      ev.quando = agora;
      if (pares) {
        const int p = leitura - 1;
        ev.tipo = EventoTecla::Par;
        ev.a = (uint8_t)(p >> 4);
        ev.b = (uint8_t)(p & 0x0F);
        _publicar(ev);
      } else if (!_botaoDe(leitura)) {   // tecla-botão: só se sabe ao soltar ou ao segurar
        ev.tipo = EventoTecla::Apertou;
        ev.tecla = (char)leitura;
        _publicar(ev);
      }
    }
  }
  // Só conta como segurada se a última leitura ainda é a mesma tecla
  if (!f.valendo || f.lida != f.valendo) return;
  const uint32_t tempo = agora - f.apertouEm;
  if (!f.segurou && tempo >= SEGURAR_MS) {
    f.segurou = true;
    EventoTecla ev;
    ev.quando = f.desceuEm;
    if (pares) {
      const int p = f.valendo - 1;
      ev.tipo = EventoTecla::ParSegurado;
      ev.a = (uint8_t)(p >> 4);
      ev.b = (uint8_t)(p & 0x0F);
    } else if (const char b = _botaoDe(f.valendo)) {
      f.virouBotao = true;               // segurou: é o botão
      ev.tipo = EventoTecla::Apertou;
      ev.tecla = b;
      ev.origem = (char)f.valendo;
    } else {
      ev.tipo = EventoTecla::Segurou;
      ev.tecla = (char)f.valendo;
    }
    _publicar(ev);
  }
  if (f.virouBotao && !f.segurouBotao && tempo >= SEGURAR_BOTAO_MS) {
    f.segurouBotao = true;
    EventoTecla ev;
    ev.tipo = EventoTecla::Segurou;
    ev.tecla = _botaoDe(f.valendo);
    ev.origem = (char)f.valendo;
    ev.quando = f.desceuEm;
    _publicar(ev);
  }
}

void Teclado::_soltou(const Firmeza& f) {
  EventoTecla ev;
  ev.quando = f.desceuEm;
  const char b = _botaoDe(f.valendo);
  if (b) ev.origem = (char)f.valendo;
  if (b && !f.virouBotao) {          // tecla-botão tocada rápido: é o número
    ev.tipo = EventoTecla::Apertou;
    ev.tecla = (char)f.valendo;
    _publicar(ev);
    ev.tipo = EventoTecla::Soltou;
    _publicar(ev);
    return;
  }
  ev.tipo = EventoTecla::Soltou;
  ev.tecla = b ? b : (char)f.valendo;
  ev.segurada = b ? f.segurouBotao : f.segurou;
  _publicar(ev);
}

void Teclado::_publicar(const EventoTecla& ev) {
  if (_n == FILA) {                 // fila cheia: perde o mais antigo
    _ini = (_ini + 1) % FILA;
    _n--;
  }
  _fila[(_ini + _n) % FILA] = ev;
  _n++;
}

bool Teclado::proximoEvento(EventoTecla& ev) {
  if (!_n) return false;
  ev = _fila[_ini];
  _ini = (_ini + 1) % FILA;
  _n--;
  return true;
}

bool Teclado::validarGrade(const uint8_t m[TECLAS], int linhas[4], int colunas[3]) {
  for (int i = 0; i < TECLAS; i++) {
    if (m[i] == SEM_PAR) return false;
    const int a = m[i] >> 4, b = m[i] & 0x0F;
    if (a >= b || b >= MAX_FIOS) return false;
    for (int j = 0; j < i; j++)
      if (m[j] == m[i]) return false;       // duas teclas com o mesmo par
  }
  // O fio que aparece em todos os pares de um grupo de teclas
  auto comum = [&](const int* idx, int n) -> int {
    for (int fio = 0; fio < MAX_FIOS; fio++) {
      bool todos = true;
      for (int k = 0; k < n && todos; k++) {
        const int a = m[idx[k]] >> 4, b = m[idx[k]] & 0x0F;
        todos = (a == fio || b == fio);
      }
      if (todos) return fio;
    }
    return -1;
  };
  for (int r = 0; r < 4; r++) {
    const int idx[3] = {r * 3, r * 3 + 1, r * 3 + 2};
    if ((linhas[r] = comum(idx, 3)) < 0) return false;
  }
  for (int c = 0; c < 3; c++) {
    const int idx[4] = {c, c + 3, c + 6, c + 9};
    if ((colunas[c] = comum(idx, 4)) < 0) return false;
  }
  const int todos[7] = {linhas[0], linhas[1], linhas[2], linhas[3], colunas[0], colunas[1], colunas[2]};
  for (int i = 0; i < 7; i++)
    for (int j = 0; j < i; j++)
      if (todos[i] == todos[j]) return false;
  for (int i = 0; i < TECLAS; i++) {
    const int l = linhas[i / 3], c = colunas[i % 3];
    const int a = l < c ? l : c, b = l < c ? c : l;
    if (m[i] != (uint8_t)((a << 4) | b)) return false;
  }
  return true;
}
