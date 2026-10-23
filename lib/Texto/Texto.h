// ============================================================
//  Texto — conversões de texto para SMS e USSD
//
//  O modem fala "UCS2": cada caractere vira 4 dígitos hexadecimais
//  ("ã" = "00E3", "A" = "0041"). É o único jeito de mandar e receber
//  acentos do português (ã, õ, ê...), que não existem no alfabeto
//  GSM de 7 bits do SMS comum.
//
//  Tudo aqui usa std::string e não depende do Arduino: dá para
//  testar no computador (veja test_host/texto_test.cpp).
// ============================================================
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>

namespace texto {

// ---------- UTF-8 → pontos de código ----------
// Lê um caractere UTF-8 a partir de s[i] e avança i.
// Sequências inválidas viram U+FFFD (o "losango com ?").
inline uint32_t lerCodepoint(const std::string& s, size_t& i) {
  const uint8_t c = (uint8_t)s[i];
  if (c < 0x80) { i += 1; return c; }
  int extras;
  uint32_t cp;
  if ((c & 0xE0) == 0xC0)      { extras = 1; cp = c & 0x1F; }
  else if ((c & 0xF0) == 0xE0) { extras = 2; cp = c & 0x0F; }
  else if ((c & 0xF8) == 0xF0) { extras = 3; cp = c & 0x07; }
  else { i += 1; return 0xFFFD; }
  for (int k = 1; k <= extras; k++) {
    if (i + k >= s.size()) { i = s.size(); return 0xFFFD; }
    const uint8_t cc = (uint8_t)s[i + k];
    if ((cc & 0xC0) != 0x80) { i += k; return 0xFFFD; }
    cp = (cp << 6) | (cc & 0x3F);
  }
  i += extras + 1;
  if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0xFFFD;
  return cp;
}

inline void escreverUtf8(std::string& out, uint32_t cp) {
  if (cp < 0x80) { out += (char)cp; }
  else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
  else if (cp < 0x10000) {
    out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F));
    out += (char)(0x80 | (cp & 0x3F));
  } else {
    out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F));
    out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F));
  }
}

// Quantas "unidades" UTF-16 o caractere ocupa no SMS em UCS2
// (emoji e outros fora do plano básico ocupam 2).
inline int unidadesUtf16(uint32_t cp) { return cp > 0xFFFF ? 2 : 1; }

// ---------- UCS2 (hexadecimal) ----------
inline void hex4(std::string& out, uint16_t u) {
  static const char* H = "0123456789ABCDEF";
  out += H[(u >> 12) & 0xF]; out += H[(u >> 8) & 0xF];
  out += H[(u >> 4) & 0xF];  out += H[u & 0xF];
}

// "Olá" → "004F006C00E1"
inline std::string utf8ParaUcs2Hex(const std::string& s) {
  std::string out;
  size_t i = 0;
  while (i < s.size()) {
    uint32_t cp = lerCodepoint(s, i);
    if (cp > 0xFFFF) {                       // par substituto UTF-16
      cp -= 0x10000;
      hex4(out, (uint16_t)(0xD800 | (cp >> 10)));
      hex4(out, (uint16_t)(0xDC00 | (cp & 0x3FF)));
    } else {
      hex4(out, (uint16_t)cp);
    }
  }
  return out;
}

inline int valorHex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// Uma string parece UCS2 em hexadecimal se tem tamanho múltiplo de 4,
// só dígitos hex, e cada grupo de 4 começa com "00" ou é um caractere
// alto plausível. Para números de telefone a regra "todo grupo começa
// com 00" basta e evita confundir "11987654321" com UCS2.
inline bool pareceUcs2Hex(const std::string& s, bool exigir00 = false) {
  if (s.empty() || s.size() % 4 != 0) return false;
  for (char c : s) if (valorHex(c) < 0) return false;
  if (exigir00) {
    for (size_t i = 0; i < s.size(); i += 4)
      if (s[i] != '0' || s[i + 1] != '0') return false;
  }
  return true;
}

// "004F006C00E1" → "Olá"
inline std::string ucs2HexParaUtf8(const std::string& hex) {
  std::string out;
  for (size_t i = 0; i + 3 < hex.size(); i += 4) {
    uint32_t u = 0;
    for (int k = 0; k < 4; k++) u = (u << 4) | (uint32_t)valorHex(hex[i + k]);
    if (u >= 0xD800 && u <= 0xDBFF && i + 7 < hex.size()) {      // início de par
      uint32_t baixo = 0;
      for (int k = 4; k < 8; k++) baixo = (baixo << 4) | (uint32_t)valorHex(hex[i + k]);
      if (baixo >= 0xDC00 && baixo <= 0xDFFF) {
        escreverUtf8(out, 0x10000 + ((u - 0xD800) << 10) + (baixo - 0xDC00));
        i += 4;
        continue;
      }
    }
    if (u >= 0xD800 && u <= 0xDFFF) u = 0xFFFD;                 // par quebrado
    escreverUtf8(out, u);
  }
  return out;
}

// Se o campo veio em UCS2, decodifica; senão devolve como veio.
inline std::string decodificarCampo(const std::string& s, bool exigir00 = false) {
  return pareceUcs2Hex(s, exigir00) ? ucs2HexParaUtf8(s) : s;
}

// ---------- Escolha da codificação do SMS ----------
// Caracteres que passam sem susto no modo texto comum (alfabeto GSM,
// 160 por SMS). Qualquer outro (acentos, emoji, colchetes...) manda
// o SMS em UCS2 (70 por SMS).
inline bool caractereGsmSeguro(uint32_t cp) {
  if (cp >= 'A' && cp <= 'Z') return true;
  if (cp >= 'a' && cp <= 'z') return true;
  if (cp >= '0' && cp <= '9') return true;
  switch (cp) {
    case ' ': case '.': case ',': case ';': case ':': case '!': case '?':
    case '\'': case '"': case '(': case ')': case '+': case '-': case '*':
    case '/': case '=': case '%': case '&': case '#': case '<': case '>':
    case '@': case '\n':
      return true;
  }
  return false;
}

inline bool ehGsmSeguro(const std::string& s) {
  size_t i = 0;
  while (i < s.size()) if (!caractereGsmSeguro(lerCodepoint(s, i))) return false;
  return true;
}

// Tamanho do texto na unidade que conta para o SMS:
// caracteres (GSM) ou unidades UTF-16 (UCS2).
inline size_t tamanhoSms(const std::string& s, bool gsm) {
  size_t n = 0, i = 0;
  while (i < s.size()) { uint32_t cp = lerCodepoint(s, i); n += gsm ? 1 : unidadesUtf16(cp); }
  return n;
}

// Divide o texto em pedaços de no máximo `limite` unidades, sem nunca
// partir um caractere ao meio (nem um emoji, que ocupa 2 unidades).
inline std::vector<std::string> dividir(const std::string& s, size_t limite, bool gsm) {
  std::vector<std::string> partes;
  std::string atual;
  size_t usado = 0, i = 0;
  while (i < s.size()) {
    size_t inicio = i;
    uint32_t cp = lerCodepoint(s, i);
    size_t custo = gsm ? 1 : (size_t)unidadesUtf16(cp);
    if (usado + custo > limite) { partes.push_back(atual); atual.clear(); usado = 0; }
    atual.append(s, inicio, i - inicio);
    usado += custo;
  }
  if (!atual.empty() || partes.empty()) partes.push_back(atual);
  return partes;
}

// ---------- Campos separados por vírgula, respeitando aspas ----------
// +CMGR: "REC UNREAD","+5511...","","26/10/21,18:00:00-12"
//   → [REC UNREAD] [+5511...] [] [26/10/21,18:00:00-12]
inline std::vector<std::string> campos(const std::string& linha) {
  std::vector<std::string> out;
  std::string atual;
  bool dentroDeAspas = false;
  for (char c : linha) {
    if (c == '"') { dentroDeAspas = !dentroDeAspas; continue; }
    if (c == ',' && !dentroDeAspas) { out.push_back(atual); atual.clear(); continue; }
    atual += c;
  }
  out.push_back(atual);
  for (auto& f : out) {                         // tira espaços nas pontas
    size_t a = f.find_first_not_of(' ');
    size_t b = f.find_last_not_of(' ');
    f = (a == std::string::npos) ? std::string() : f.substr(a, b - a + 1);
  }
  return out;
}

// Número de telefone aceitável para discar ou mandar SMS:
// dígitos, opcionalmente começando com "+"; * e # para códigos (*222#).
inline bool numeroValido(const std::string& n, bool permitirCodigos) {
  if (n.empty() || n.size() > 20) return false;
  for (size_t i = 0; i < n.size(); i++) {
    char c = n[i];
    if (c >= '0' && c <= '9') continue;
    if (c == '+' && i == 0) continue;
    if (permitirCodigos && (c == '*' || c == '#')) continue;
    return false;
  }
  return true;
}

}  // namespace texto
