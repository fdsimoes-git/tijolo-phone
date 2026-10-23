#include "Multitoque.h"
#include "Texto.h"

namespace {

struct Lista {
  const char* const* opcoes;
  int n;
};

const char* const T0[] = {" ", "0"};
const char* const T1[] = {".", ",", "?", "!", ":", ";", "'", "\"", "-", "(", ")", "@",
                          "/", "+", "=", "&", "%", "*", "#", "$", "_", "<", ">", "1"};
const char* const T2[] = {"a", "b", "c", "á", "à", "â", "ã", "ç", "2"};
const char* const T3[] = {"d", "e", "f", "é", "ê", "3"};
const char* const T4[] = {"g", "h", "i", "í", "4"};
const char* const T5[] = {"j", "k", "l", "5"};
const char* const T6[] = {"m", "n", "o", "ó", "ô", "õ", "6"};
const char* const T7[] = {"p", "q", "r", "s", "7"};
const char* const T8[] = {"t", "u", "v", "ú", "8"};
const char* const T9[] = {"w", "x", "y", "z", "9"};

#define LISTA(t) Lista{t, (int)(sizeof(t) / sizeof(t[0]))}

bool lista(char k, Lista& l) {
  switch (k) {
    case '0': l = LISTA(T0); return true;
    case '1': l = LISTA(T1); return true;
    case '2': l = LISTA(T2); return true;
    case '3': l = LISTA(T3); return true;
    case '4': l = LISTA(T4); return true;
    case '5': l = LISTA(T5); return true;
    case '6': l = LISTA(T6); return true;
    case '7': l = LISTA(T7); return true;
    case '8': l = LISTA(T8); return true;
    case '9': l = LISTA(T9); return true;
    default: return false;
  }
}

// Maiúscula de uma letra (a-z e as acentuadas do português, U+00E0 a U+00FE)
uint32_t maiuscula(uint32_t cp) {
  if (cp >= 'a' && cp <= 'z') return cp - 32;
  if (cp >= 0xE0 && cp <= 0xFE && cp != 0xF7) return cp - 0x20;
  return cp;
}

}  // namespace

void Multitoque::begin(const std::string& texto, size_t limiteBytes) {
  _texto = texto;
  _limite = limiteBytes;
  _modo = Modo::Frase;
  _pendente = false;
  _teclaPendente = 0;
  _indice = 0;
  _ultimo = Toque::Nenhum;
  _recusou = false;
}

const char* Multitoque::nomeModo() const {
  switch (_modo) {
    case Modo::Frase: return "Abc";
    case Modo::Minusculas: return "abc";
    case Modo::Maiusculas: return "ABC";
    case Modo::Numeros: return "123";
  }
  return "";
}

// Começo de frase: texto vazio, nova linha, ou ". ", "! ", "? " antes
bool Multitoque::_inicioDeFrase() const {
  size_t i = _texto.size(), espacos = 0;
  while (i > 0 && _texto[i - 1] == ' ') { i--; espacos++; }
  if (i == 0) return true;
  const char c = _texto[i - 1];
  if (c == '\n') return true;
  return espacos > 0 && (c == '.' || c == '!' || c == '?');
}

bool Multitoque::proximaMaiuscula() const {
  return _modo == Modo::Maiusculas || (_modo == Modo::Frase && _inicioDeFrase());
}

std::string Multitoque::_comCaixa(const char* opcao) const {
  if (!_maiuscula) return opcao;
  std::string s(opcao), r;
  size_t i = 0;
  while (i < s.size()) texto::escreverUtf8(r, maiuscula(texto::lerCodepoint(s, i)));
  return r;
}

// Cabe? O limite de bytes e, se quem usa pediu, o dele (o tamanho em SMS)
bool Multitoque::_cabeTexto(const std::string& t) const {
  return t.size() <= _limite && (!_cabe || _cabe(t));
}

bool Multitoque::_acrescentar(const std::string& s) {
  if (!_cabeTexto(_texto + s)) return false;
  _inicioPendente = _texto.size();
  _texto += s;
  return true;
}

void Multitoque::_fixar() {
  _pendente = false;
  _teclaPendente = 0;
}

void Multitoque::confirmar() {
  _fixar();
  _ultimo = Toque::Nenhum;   // depois disto, segurar a tecla já não mexe na letra
}

void Multitoque::proximoModo() {
  confirmar();
  _modo = _modo == Modo::Frase ? Modo::Minusculas
        : _modo == Modo::Minusculas ? Modo::Maiusculas
        : _modo == Modo::Maiusculas ? Modo::Numeros : Modo::Frase;
}

bool Multitoque::tecla(char k, uint32_t agora) {
  Lista l;
  if (!lista(k, l)) return false;
  _recusou = false;
  if (_modo == Modo::Numeros) {               // cada tecla é o seu número
    confirmar();
    if (_acrescentar(std::string(1, k))) return true;
    _recusou = true;
    return false;
  }
  if (_pendente && k == _teclaPendente && agora - _quando < CICLO_MS) {
    // A mesma tecla de novo: a próxima letra que cabe (as que não cabem são
    // puladas: perto do limite, "á" pode não caber onde "c" cabia)
    _quando = agora;
    _ultimaTecla = k;
    const std::string antes = _texto.substr(0, _inicioPendente);
    for (int passo = 1; passo < l.n; passo++) {
      const int i = (_indice + passo) % l.n;
      const std::string novo = antes + _comCaixa(l.opcoes[i]);
      if (!_cabeTexto(novo)) continue;
      _anterior = _indice;
      _indice = i;
      _texto = novo;
      _ultimo = Toque::Ciclou;
      return true;
    }
    _ultimo = Toque::Recusado;                // nenhuma outra cabe: fica a que está
    _recusou = true;
    return false;
  }
  confirmar();
  _maiuscula = proximaMaiuscula();
  _indice = 0;
  _anterior = -1;
  if (!_acrescentar(_comCaixa(l.opcoes[0]))) {
    _recusou = true;
    return false;
  }
  _pendente = true;
  _teclaPendente = k;
  _ultimaTecla = k;
  _quando = agora;
  _ultimo = Toque::Novo;
  return true;
}

// Segurar vale para o último toque, e só se nada mudou depois dele (o
// "segurou" pode chegar depois de 1 s, com a letra já confirmada).
bool Multitoque::segurou(char k, uint32_t) {
  Lista l;
  if (_ultimo == Toque::Nenhum || k != _ultimaTecla || !lista(k, l)) return false;
  const Toque ultimo = _ultimo;
  _ultimo = Toque::Nenhum;
  _recusou = false;
  const std::string numero = l.opcoes[l.n - 1];   // o número da tecla é a última opção
  const std::string antes = _texto.substr(0, _inicioPendente);
  std::string novo;
  if (ultimo == Toque::Novo) {
    novo = antes + numero;                      // o número no lugar da letra deste toque
  } else if (ultimo == Toque::Ciclou) {
    // Este toque só tinha trocado a letra (j → k): a de antes fica e o número
    // vem depois dela — tocar 5 e logo segurar o 5 dá "j5"
    const std::string letra = _comCaixa(l.opcoes[_anterior]);
    novo = antes + letra + numero;
    if (!_cabeTexto(novo)) {                    // o número não cabe: só volta a letra de antes
      _texto = antes + letra;
      _fixar();
      _recusou = true;
      return true;
    }
  } else {                                      // o toque não coube: o número depois da letra
    novo = _texto + numero;
  }
  _fixar();
  if (!_cabeTexto(novo)) {
    _recusou = true;
    return true;                                // (a letra ficou confirmada)
  }
  _texto = novo;
  return true;
}

bool Multitoque::tique(uint32_t agora) {
  if (!_pendente || agora - _quando < CICLO_MS) return false;
  _fixar();             // (um "segurou" atrasado ainda vale para este toque)
  return true;
}

bool Multitoque::apagar() {
  _ultimo = Toque::Nenhum;
  _recusou = false;
  if (_pendente) {
    _texto.resize(_inicioPendente);
    confirmar();
    return true;
  }
  if (_texto.empty()) return false;
  size_t i = _texto.size() - 1;
  while (i > 0 && (((uint8_t)_texto[i]) & 0xC0) == 0x80) i--;   // o começo do caractere UTF-8
  _texto.resize(i);
  return true;
}

bool Multitoque::apagarPalavra() {
  const size_t antes = _texto.size();
  confirmar();                                // a letra pendente conta como escrita
  size_t i = _texto.size();
  while (i > 0 && (_texto[i - 1] == ' ' || _texto[i - 1] == '\n')) i--;   // os espaços do fim
  while (i > 0 && _texto[i - 1] != ' ' && _texto[i - 1] != '\n') i--;     // e a palavra
  _texto.resize(i);
  return _texto.size() != antes;
}

void Multitoque::limpar() {
  _texto.clear();
  confirmar();
}
