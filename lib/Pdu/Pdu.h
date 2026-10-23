// ============================================================
//  Pdu — SMS no "modo PDU": a mensagem como os bytes que viajam na rede
//
//  No modo texto o modem converte o SMS para nós, mas tropeça em SMS
//  longos: cada parte chega com um cabeçalho binário (UDH) misturado ao
//  texto, e no alfabeto de 7 bits vira lixo. No modo PDU recebemos e
//  mandamos os bytes crus e fazemos a conversão aqui:
//
//   - alfabeto GSM de 7 bits (160 letras por SMS; 7 bits por letra,
//     "espremidos" em bytes) ou UCS2 (70 por SMS, para acentos e emoji);
//   - remetente em número (+5511...) ou em texto ("TIM", "Bradesco");
//   - data e hora da operadora;
//   - SMS longo: cabeçalho com referência, total de partes e número da parte.
//
//  Só std::string e std::vector: dá para testar no computador
//  (test_host/pdu_test.cpp confere contra uma biblioteca Python independente).
// ============================================================
#pragma once
#include <Texto.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

namespace pdu {

// ---------- Alfabeto GSM 03.38 (tabela básica: índice → Unicode) ----------
static const uint16_t GSM7[128] = {
  '@', 0xA3, '$', 0xA5, 0xE8, 0xE9, 0xF9, 0xEC, 0xF2, 0xC7, '\n', 0xD8, 0xF8, '\r', 0xC5, 0xE5,
  0x394, '_', 0x3A6, 0x393, 0x39B, 0x3A9, 0x3A0, 0x3A8, 0x3A3, 0x398, 0x39E, 0xFFFF, 0xC6, 0xE6, 0xDF, 0xC9,
  ' ', '!', '"', '#', 0xA4, '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
  0xA1, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
  'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 0xC4, 0xD6, 0xD1, 0xDC, 0xA7,
  0xBF, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
  'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 0xE4, 0xF6, 0xF1, 0xFC, 0xE0,
};
// Tabela de extensão: depois do ESC (0x1B) vem um destes códigos
struct Ext { uint8_t codigo; uint16_t unicode; };
static const Ext GSM7_EXT[] = {
  {0x0A, 0x0C}, {0x14, '^'}, {0x28, '{'}, {0x29, '}'}, {0x2F, '\\'},
  {0x3C, '['}, {0x3D, '~'}, {0x3E, ']'}, {0x40, '|'}, {0x65, 0x20AC},
};

inline uint32_t gsmParaUnicode(uint8_t c, bool extensao) {
  if (extensao) {
    for (auto& e : GSM7_EXT) if (e.codigo == c) return e.unicode;
    return ' ';                                   // código de extensão desconhecido
  }
  return GSM7[c & 0x7F];
}

// Septetos de um caractere no alfabeto GSM (1 ou 2). Vazio = não existe.
inline std::vector<uint8_t> unicodeParaGsm(uint32_t cp) {
  if (cp != 0xFFFF)
    for (int i = 0; i < 128; i++) if (GSM7[i] == cp && i != 0x1B) return {(uint8_t)i};
  for (auto& e : GSM7_EXT) if (e.unicode == cp) return {0x1B, e.codigo};
  return {};
}

// ---------- 7 bits espremidos em bytes ----------
// Escreve os septetos a partir do bit `bitInicial` (os bits antes ficam 0).
inline void empacotar(std::vector<uint8_t>& saida, size_t bitInicial, const std::vector<uint8_t>& septetos) {
  size_t totalBits = bitInicial + septetos.size() * 7;
  saida.resize((totalBits + 7) / 8, 0);
  size_t bit = bitInicial;
  for (uint8_t s : septetos)
    for (int b = 0; b < 7; b++, bit++)
      if (s & (1 << b)) saida[bit / 8] |= (uint8_t)(1 << (bit % 8));
}

inline std::vector<uint8_t> desempacotar(const uint8_t* dados, size_t nBytes, size_t bitInicial, size_t nSeptetos) {
  std::vector<uint8_t> out;
  size_t bit = bitInicial;
  for (size_t i = 0; i < nSeptetos; i++) {
    if (bit + 7 > nBytes * 8) break;              // não lê além do fim
    uint8_t s = 0;
    for (int b = 0; b < 7; b++, bit++)
      if (dados[bit / 8] & (1 << (bit % 8))) s |= (uint8_t)(1 << b);
    out.push_back(s);
  }
  return out;
}

inline std::string septetosParaUtf8(const std::vector<uint8_t>& s) {
  std::string out;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == 0x1B && i + 1 < s.size()) { texto::escreverUtf8(out, gsmParaUnicode(s[++i], true)); continue; }
    if (s[i] == 0x1B) break;                      // ESC solto no fim
    texto::escreverUtf8(out, gsmParaUnicode(s[i], false));
  }
  return out;
}

// ---------- hexadecimal ----------
inline bool hexParaBytes(const std::string& hex, std::vector<uint8_t>& out) {
  out.clear();
  if (hex.size() % 2) return false;
  for (size_t i = 0; i < hex.size(); i += 2) {
    int a = texto::valorHex(hex[i]), b = texto::valorHex(hex[i + 1]);
    if (a < 0 || b < 0) return false;
    out.push_back((uint8_t)(a << 4 | b));
  }
  return true;
}
inline std::string bytesParaHex(const std::vector<uint8_t>& b) {
  static const char* H = "0123456789ABCDEF";
  std::string s;
  for (uint8_t x : b) { s += H[x >> 4]; s += H[x & 0xF]; }
  return s;
}

// ============================================================
//  Leitura
// ============================================================
struct Mensagem {
  int tipo = -1;           // 0 = recebida (DELIVER), 1 = enviada/guardada (SUBMIT), 2 = relatório
  std::string numero;      // remetente ou destino ("+5511...", "27888" ou "TIM")
  std::string dataHora;    // "26/10/21,18:05:12-12" (só nas recebidas)
  std::string texto;       // UTF-8
  bool binaria = false;    // dados de 8 bits (não é texto para pessoas)
  int ref = 0, total = 1, parte = 1;   // SMS longo
};

namespace detalhe {
struct Leitor {
  const std::vector<uint8_t>& b;
  size_t p = 0;
  bool ok = true;
  explicit Leitor(const std::vector<uint8_t>& v) : b(v) {}
  uint8_t u8() { if (p >= b.size()) { ok = false; return 0; } return b[p++]; }
  const uint8_t* ptr(size_t n) { if (p + n > b.size()) { ok = false; return nullptr; } const uint8_t* r = &b[p]; p += n; return r; }
};

// Endereço: <nº de dígitos> <tipo> <dígitos em BCD invertido>
inline std::string endereco(Leitor& l) {
  uint8_t nDigitos = l.u8();
  uint8_t tipo = l.u8();
  size_t nBytes = (nDigitos + 1) / 2;
  const uint8_t* d = l.ptr(nBytes);
  if (!d) return "";
  if ((tipo & 0x70) == 0x50) {                    // alfanumérico ("TIM", "Bradesco")
    std::vector<uint8_t> v(d, d + nBytes);
    return septetosParaUtf8(desempacotar(v.data(), v.size(), 0, (nDigitos * 4) / 7));
  }
  static const char* DIG = "0123456789*#abc";
  std::string s = (tipo & 0x70) == 0x10 ? "+" : "";
  size_t postos = 0;                              // dígitos já escritos
  for (size_t i = 0; i < nBytes; i++) {
    uint8_t lo = d[i] & 0x0F, hi = d[i] >> 4;    // o dígito "de baixo" vem primeiro
    if (lo < 15 && postos < nDigitos) { s += DIG[lo]; postos++; }
    if (hi < 15 && postos < nDigitos) { s += DIG[hi]; postos++; }
  }
  return s;
}

inline std::string dataHora(const uint8_t* t) {
  auto dd = [](uint8_t x) { return (char)('0' + (x & 0x0F)) + std::string(1, (char)('0' + (x >> 4))); };
  std::string s = dd(t[0]) + "/" + dd(t[1]) + "/" + dd(t[2]) + "," + dd(t[3]) + ":" + dd(t[4]) + ":" + dd(t[5]);
  int quartos = (t[6] & 0x07) * 10 + (t[6] >> 4);  // fuso em quartos de hora
  char buf[8];
  snprintf(buf, sizeof buf, "%c%02d", (t[6] & 0x08) ? '-' : '+', quartos);
  return s + buf;
}

// 0 = GSM 7 bits, 1 = 8 bits, 2 = UCS2
inline int alfabeto(uint8_t dcs) {
  if ((dcs & 0xC0) == 0x00 || (dcs & 0xC0) == 0x40) { int a = (dcs >> 2) & 0x03; return a == 3 ? 0 : a; }
  if ((dcs & 0xF0) == 0xF0) return (dcs & 0x04) ? 1 : 0;
  if ((dcs & 0xF0) == 0xE0) return 2;
  return 0;
}
}  // namespace detalhe

inline bool decodificar(const std::string& hex, Mensagem& m) {
  std::vector<uint8_t> b;
  if (!hexParaBytes(hex, b) || b.empty()) return false;
  detalhe::Leitor l(b);
  uint8_t tamSca = l.u8();                        // central de mensagens: pula
  l.ptr(tamSca);
  uint8_t fo = l.u8();
  if (!l.ok) return false;
  m = Mensagem();
  int mti = fo & 0x03;
  bool temCabecalho = fo & 0x40;
  if (mti == 0) {                                 // SMS-DELIVER (recebida)
    m.tipo = 0;
    m.numero = detalhe::endereco(l);
  } else if (mti == 1) {                          // SMS-SUBMIT guardada no chip
    m.tipo = 1;
    l.u8();                                       // referência
    m.numero = detalhe::endereco(l);
  } else {
    m.tipo = 2;
    m.texto = "(relatório de entrega)";
    return l.ok;
  }
  l.u8();                                         // PID
  uint8_t dcs = l.u8();
  if (m.tipo == 0) {
    const uint8_t* t = l.ptr(7);
    if (t) m.dataHora = detalhe::dataHora(t);
  } else {
    int vpf = (fo >> 3) & 0x03;                   // validade: 0 nenhuma, 2 relativa (1 byte), 1/3 (7 bytes)
    l.ptr(vpf == 0 ? 0 : vpf == 2 ? 1 : 7);
  }
  uint8_t udl = l.u8();
  if (!l.ok) return false;
  const size_t resto = b.size() - l.p;
  const uint8_t* ud = b.data() + l.p;
  const int alf = detalhe::alfabeto(dcs);

  size_t bytesCabecalho = 0;
  if (temCabecalho && resto > 0) {
    bytesCabecalho = (size_t)ud[0] + 1;
    if (bytesCabecalho > resto) return false;
    for (size_t i = 1; i + 1 < bytesCabecalho;) { // elementos do cabeçalho
      uint8_t id = ud[i], len = ud[i + 1];
      if (i + 2 + len > bytesCabecalho) break;
      const uint8_t* v = ud + i + 2;
      if (id == 0x00 && len == 3 && v[1] > 0) { m.ref = v[0]; m.total = v[1]; m.parte = v[2]; }
      if (id == 0x08 && len == 4 && v[2] > 0) { m.ref = (v[0] << 8) | v[1]; m.total = v[2]; m.parte = v[3]; }
      i += 2 + len;
    }
  }

  if (alf == 0) {
    size_t bitInicio = 0, septetosCab = 0;
    if (bytesCabecalho) {
      size_t bits = bytesCabecalho * 8;
      size_t enchimento = (7 - bits % 7) % 7;   // alinha o texto em septetos
      bitInicio = bits + enchimento;
      septetosCab = bitInicio / 7;
    }
    size_t n = udl > septetosCab ? udl - septetosCab : 0;
    m.texto = septetosParaUtf8(desempacotar(ud, resto, bitInicio, n));
  } else {
    size_t n = udl < resto ? udl : resto;
    if (bytesCabecalho > n) return false;
    std::vector<uint8_t> dados(ud + bytesCabecalho, ud + n);
    if (alf == 2) {
      if (dados.size() % 2) dados.pop_back();
      m.texto = texto::ucs2HexParaUtf8(bytesParaHex(dados));
    } else {
      m.binaria = true;
      m.texto = "(mensagem de dados, " + std::to_string(dados.size()) + " bytes)";
    }
  }
  return true;
}

// ============================================================
//  Envio
// ============================================================
struct Envio {
  std::string hex;   // PDU completo, já com "00" (central de mensagens do chip)
  int tamanho;       // bytes do TPDU (sem a central): vai no AT+CMGS=<tamanho>
};

// Divide o texto em partes e monta um PDU por parte.
// ref: mesmo número em todas as partes de um SMS longo (1 a 255).
// Devolve vazio (e preenche *erro) se o número ou o texto não servem.
inline std::vector<Envio> codificar(const std::string& numero, const std::string& txt, uint8_t ref,
                                    uint8_t validade = 0xA7, std::string* erro = nullptr) {
  std::vector<Envio> saida;
  auto falha = [&](const char* e) { if (erro) *erro = e; return std::vector<Envio>(); };

  // Destino
  std::string digitos = numero;
  bool internacional = !digitos.empty() && digitos[0] == '+';
  if (internacional) digitos.erase(0, 1);
  if (digitos.empty() || digitos.size() > 20) return falha("número inválido");
  for (char c : digitos) if (c < '0' || c > '9') return falha("número inválido");

  // Alfabeto: GSM se todos os caracteres existirem nele
  std::vector<std::vector<uint8_t>> gsm;          // septetos de cada caractere
  bool usaGsm = true;
  for (size_t i = 0; i < txt.size();) {
    auto s = unicodeParaGsm(texto::lerCodepoint(txt, i));
    if (s.empty()) { usaGsm = false; break; }
    gsm.push_back(s);
  }

  // Partes: 160 septetos (ou 70 unidades UCS2) numa só; 153 / 67 por parte num SMS longo
  std::vector<std::vector<uint8_t>> partesGsm;
  std::vector<std::string> partesUcs2;
  if (usaGsm) {
    size_t total = 0;
    for (auto& c : gsm) total += c.size();
    size_t limite = total <= 160 ? 160 : 153;
    std::vector<uint8_t> atual;
    for (auto& c : gsm) {
      if (atual.size() + c.size() > limite) { partesGsm.push_back(atual); atual.clear(); }
      atual.insert(atual.end(), c.begin(), c.end());   // ESC + código nunca se separam
    }
    partesGsm.push_back(atual);
  } else {
    size_t unidades = texto::tamanhoSms(txt, false);
    partesUcs2 = texto::dividir(txt, unidades <= 70 ? 70 : 67, false);
  }
  const size_t nPartes = usaGsm ? partesGsm.size() : partesUcs2.size();
  if (nPartes > 255) return falha("texto longo demais");

  for (size_t i = 0; i < nPartes; i++) {
    const bool longo = nPartes > 1;
    std::vector<uint8_t> p;
    p.push_back(0x00);                            // central de mensagens: a do chip
    p.push_back((uint8_t)(0x01 | 0x10 | (longo ? 0x40 : 0)));   // SUBMIT, validade relativa, [cabeçalho]
    p.push_back(0x00);                            // referência (o modem escolhe)
    p.push_back((uint8_t)digitos.size());
    p.push_back(internacional ? 0x91 : 0x81);
    for (size_t k = 0; k < digitos.size(); k += 2) {
      uint8_t lo = (uint8_t)(digitos[k] - '0');
      uint8_t hi = k + 1 < digitos.size() ? (uint8_t)(digitos[k + 1] - '0') : 0x0F;
      p.push_back((uint8_t)(hi << 4 | lo));
    }
    p.push_back(0x00);                            // PID
    p.push_back(usaGsm ? 0x00 : 0x08);            // DCS: GSM 7 bits ou UCS2
    p.push_back(validade);                        // 0xA7 = 24 horas

    std::vector<uint8_t> cab;
    if (longo) cab = {0x05, 0x00, 0x03, ref, (uint8_t)nPartes, (uint8_t)(i + 1)};
    std::vector<uint8_t> ud = cab;
    uint8_t udl;
    if (usaGsm) {
      size_t bits = cab.size() * 8;
      size_t enchimento = cab.empty() ? 0 : (7 - bits % 7) % 7;
      empacotar(ud, bits + enchimento, partesGsm[i]);
      udl = (uint8_t)((bits + enchimento) / 7 + partesGsm[i].size());
    } else {
      std::vector<uint8_t> dados;
      hexParaBytes(texto::utf8ParaUcs2Hex(partesUcs2[i]), dados);
      ud.insert(ud.end(), dados.begin(), dados.end());
      udl = (uint8_t)ud.size();
    }
    p.push_back(udl);
    p.insert(p.end(), ud.begin(), ud.end());
    saida.push_back({bytesParaHex(p), (int)p.size() - 1});
  }
  return saida;
}

// Quantos SMS o texto ocupa, com o mesmo corte que codificar() faz.
// Para o contador da tela de escrever.
struct Contagem {
  bool gsm = true;     // alfabeto GSM (160 por SMS); false = UCS2, com acentos (70)
  int partes = 1;
  int usados = 0;      // septetos (GSM, o "€" vale 2) ou unidades UTF-16 (UCS2)
  int porParte = 160;  // 160 numa só; 153 por parte num SMS longo (UCS2: 70 e 67)
};
inline Contagem contar(const std::string& txt) {
  Contagem c;
  std::vector<int> custos;
  for (size_t i = 0; i < txt.size();) {
    const auto s = unicodeParaGsm(texto::lerCodepoint(txt, i));
    if (s.empty()) { c.gsm = false; break; }
    custos.push_back((int)s.size());
  }
  if (c.gsm) {
    for (int x : custos) c.usados += x;
    c.porParte = c.usados <= 160 ? 160 : 153;
    int atual = 0;
    for (int x : custos) {
      if (atual + x > c.porParte) { c.partes++; atual = 0; }
      atual += x;
    }
  } else {
    c.usados = (int)texto::tamanhoSms(txt, false);
    c.porParte = c.usados <= 70 ? 70 : 67;
    c.partes = (int)texto::dividir(txt, (size_t)c.porParte, false).size();
  }
  return c;
}

// ============================================================
//  Montador de SMS longos: junta as partes quando todas chegarem
// ============================================================
class Montador {
public:
  struct Pronta { Mensagem msg; std::vector<int> indices; bool incompleta = false; };

  // Guarda uma parte. Se completou, devolve true e a mensagem inteira em `pronta`.
  bool adicionar(const Mensagem& m, int indice, uint32_t agoraMs, Pronta& pronta) {
    if (m.total <= 1) { pronta = Pronta{m, {indice}, false}; return true; }
    Grupo* g = nullptr;
    for (auto& x : _grupos) if (x.chave.numero == m.numero && x.chave.ref == m.ref && x.chave.total == m.total) g = &x;
    if (!g) {
      _grupos.push_back(Grupo{m, std::vector<std::string>(m.total), std::vector<bool>(m.total, false), {}, agoraMs});
      g = &_grupos.back();
    }
    if (m.parte >= 1 && m.parte <= m.total) {
      g->textos[m.parte - 1] = m.texto;
      g->tem[m.parte - 1] = true;
      if (m.parte == 1) g->chave.dataHora = m.dataHora;
    }
    g->indices.push_back(indice);
    for (bool t : g->tem) if (!t) return false;
    pronta = _juntar(*g, false);
    _grupos.erase(_grupos.begin() + (g - _grupos.data()));
    return true;
  }

  // Grupo esperando há mais de `limiteMs`: entrega o que chegou (com aviso).
  bool expirada(uint32_t agoraMs, uint32_t limiteMs, Pronta& pronta) {
    for (size_t i = 0; i < _grupos.size(); i++) {
      if (agoraMs - _grupos[i].desde < limiteMs) continue;
      pronta = _juntar(_grupos[i], true);
      _grupos.erase(_grupos.begin() + i);
      return true;
    }
    return false;
  }

  // Esta posição do chip já está aqui (uma parte esperando as outras)?
  bool temIndice(int indice) const {
    for (const auto& g : _grupos)
      for (int i : g.indices)
        if (i == indice) return true;
    return false;
  }

  // Esvazia tudo o que está pendente (para listas).
  std::vector<Pronta> pendentes() {
    std::vector<Pronta> v;
    for (auto& g : _grupos) v.push_back(_juntar(g, true));
    _grupos.clear();
    return v;
  }

private:
  struct Grupo {
    Mensagem chave;
    std::vector<std::string> textos;
    std::vector<bool> tem;
    std::vector<int> indices;
    uint32_t desde;
  };
  std::vector<Grupo> _grupos;

  Pronta _juntar(const Grupo& g, bool incompleta) {
    Pronta p;
    p.msg = g.chave;
    p.msg.texto.clear();
    for (size_t i = 0; i < g.textos.size(); i++) p.msg.texto += g.tem[i] ? g.textos[i] : std::string("[…]");
    p.msg.parte = 1;
    p.indices = g.indices;
    p.incompleta = incompleta;
    return p;
  }
};

}  // namespace pdu
