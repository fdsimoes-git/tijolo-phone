// ============================================================
//  EstadoTela — o "boletim" que o ILI9341 dá de si mesmo
//
//  O comando RDDST (09h) devolve 32 bits com o estado da tela: se o
//  conversor de tensão interno dela (booster) está ligado, se ela está
//  em repouso, se a imagem está ligada, o formato das cores e a
//  orientação. Comparando com o que o Tijolo configurou, dá para saber
//  SE a tela se perdeu e DE QUE JEITO — e o jeito aponta a causa:
//
//   - voltou ao estado de fábrica: ela reiniciou (queda na alimentação
//     dela, ou um pulso no fio RESET);
//   - entrou em repouso, desligou a imagem ou mudou a configuração
//     sozinha: chegou um comando que ninguém mandou (ruído nos fios de
//     dados, SCK/SDI/DC/CS);
//   - o conversor interno parou com ela acordada: tensão baixa ou ruído
//     na alimentação;
//   - leitura toda em 0 ou toda em 1: o fio MISO não responde (solto, ou
//     a tela sem energia nenhuma);
//   - o painel parou de varrer as linhas (comando 45h, abaixo) com o
//     boletim dizendo que está tudo certo: a tela fica branca sem o
//     controlador perceber. É o que a troca de rede do modem (4G ↔ 2G)
//     pode estar fazendo; o RESET destrava.
//
//  Só lógica, sem Arduino: roda no computador nos testes.
// ============================================================
#pragma once
#include <cstdint>
#include <cstdio>

namespace estado_tela {

// Bits do RDDST (datasheet do ILI9341, comando 09h, D31..D0)
constexpr uint32_t BOOSTER   = 1u << 31;     // conversor de tensão interno ligado
constexpr uint32_t MADCTL    = 0x7Eu << 24;  // D30..D25 = bits 7..2 do MADCTL (orientação, RGB/BGR)
constexpr uint32_t FORMATO   = 7u << 20;     // D22..D20: bits por ponto
constexpr uint32_t OCIOSO    = 1u << 19;     // modo de 8 cores
constexpr uint32_t PARCIAL   = 1u << 18;     // só um pedaço da tela
constexpr uint32_t ACORDADA  = 1u << 17;     // fora do repouso (sleep out)
constexpr uint32_t NORMAL    = 1u << 16;     // modo normal (sem parcial)
constexpr uint32_t ROLAGEM   = 1u << 15;     // rolagem vertical ligada
constexpr uint32_t LIGADA    = 1u << 10;     // imagem ligada (display on)
// D13..D11 (inversão etc.) aparecem como "não definidos" no datasheet V1.11,
// e D9..D5 (TE, gama) mudam com coisas que o Tijolo não usa: ficam de fora.
// D4..D0 não são usados pelo ILI9341 (vêm em 0): o Tijolo usa o D0 para
// anotar junto com o boletim o que ele viu no comando Get_Scanline (45h).
constexpr uint32_t PARADA    = 1u << 0;      // a linha que o painel está varrendo não andou

// Os bits que o Tijolo confere
constexpr uint32_t CONFERIR = BOOSTER | MADCTL | FORMATO | OCIOSO | PARCIAL | ACORDADA | NORMAL | ROLAGEM | LIGADA | PARADA;

constexpr uint32_t FORMATO_16 = 5u << 20;    // o que o Tijolo usa
constexpr uint32_t FORMATO_18 = 6u << 20;    // o padrão de fábrica (depois de um RESET)

// O MADCTL (registrador 36h) que está nos bits D30..D25
inline uint8_t madctl(uint32_t e) { return (uint8_t)(((e >> 25) & 0x3F) << 2); }

enum class Falha : uint8_t {
  Nenhuma,
  SemResposta,      // leu tudo 0 ou tudo 1
  Reiniciou,        // repouso + imagem desligada + conversor parado: estado de fábrica
  Dormiu,           // entrou em repouso, o resto igual
  ImagemDesligada,  // acordada, mas com a imagem desligada
  ConversorParou,   // acordada e com imagem, mas sem o conversor interno
  ConfigMudou,      // orientação, cores ou modo diferentes
  PainelParado,     // boletim certo, mas a varredura das linhas parou (tela branca)
};
constexpr int N_FALHAS = 8;

inline bool respondeu(uint32_t e) { return e != 0 && e != 0xFFFFFFFFu; }

// Uma leitura que parece de tela funcionando (acordada, imagem ligada,
// conversor ligado, sem os modos especiais).
inline bool funcionando(uint32_t e) {
  return respondeu(e) && (e & BOOSTER) && (e & ACORDADA) && (e & LIGADA) && !(e & (OCIOSO | PARCIAL));
}

// Compara o que foi lido com a referência (lida logo depois de configurar).
inline Falha classificar(uint32_t referencia, uint32_t lido) {
  if (!respondeu(lido)) return Falha::SemResposta;
  if (((referencia ^ lido) & CONFERIR) == 0) return Falha::Nenhuma;
  const bool acordada = lido & ACORDADA, ligada = lido & LIGADA, conversor = lido & BOOSTER;
  // Voltou ao estado de fábrica: o formato das cores volta a 18 bits (o
  // Tijolo usa 16), mesmo que depois chegue um comando solto que a acorde.
  const bool formatoDeFabrica = (lido & FORMATO) == FORMATO_18 && (referencia & FORMATO) != FORMATO_18;
  if (formatoDeFabrica || (!acordada && !ligada && !conversor)) return Falha::Reiniciou;
  if (!acordada) return Falha::Dormiu;
  if (!ligada) return Falha::ImagemDesligada;
  if (!conversor) return Falha::ConversorParou;
  if (lido & PARADA) return Falha::PainelParado;
  return Falha::ConfigMudou;
}

inline const char* nome(Falha f) {
  switch (f) {
    case Falha::Nenhuma:         return "tudo certo";
    case Falha::SemResposta:     return "não respondeu (leitura toda 0 ou toda 1)";
    case Falha::Reiniciou:       return "voltou ao estado de fábrica (reiniciou)";
    case Falha::Dormiu:          return "entrou em repouso sozinha";
    case Falha::ImagemDesligada: return "desligou a imagem sozinha";
    case Falha::ConversorParou:  return "conversor de tensão interno parou";
    case Falha::ConfigMudou:     return "configuração mudou sozinha";
    case Falha::PainelParado:    return "painel parou de varrer as linhas (boletim certo, tela branca)";
  }
  return "?";
}

// O que cada tipo sugere (para o relatório do monitor)
inline const char* pista(Falha f) {
  switch (f) {
    case Falha::SemResposta:     return "fio MISO solto, ou a tela ficou sem energia";
    case Falha::Reiniciou:       return "queda na alimentação da tela, ou pulso no fio RESET";
    case Falha::Dormiu:
    case Falha::ImagemDesligada:
    case Falha::ConfigMudou:     return "comando falso: ruído nos fios de dados (SCK, SDI, DC, CS)";
    case Falha::ConversorParou:  return "tensão baixa ou ruído na alimentação da tela";
    case Falha::PainelParado:    return "perturbação do rádio do modem (2G): alimentação, terra ou fios da tela";
    default:                     return "";
  }
}

// "acordada · imagem ligada · conversor ligado · 16 bits · MADCTL 0x08"
// em `saida` (tamanho >= 160).
inline void descrever(uint32_t e, char* saida, int tamanho) {
  if (!respondeu(e)) {
    snprintf(saida, tamanho, "sem resposta (0x%08X)", (unsigned)e);
    return;
  }
  const unsigned fmt = (e & FORMATO) >> 20;
  const char* bits = fmt == 5 ? "16 bits" : fmt == 6 ? "18 bits (padrão de fábrica)" : "formato ?";
  snprintf(saida, tamanho, "%s · %s · conversor %s · %s · MADCTL 0x%02X%s%s%s",
           (e & ACORDADA) ? "acordada" : "EM REPOUSO", (e & LIGADA) ? "imagem ligada" : "IMAGEM DESLIGADA",
           (e & BOOSTER) ? "ligado" : "PARADO", bits, (unsigned)madctl(e),
           (e & (OCIOSO | PARCIAL)) ? " · modo especial" : "", (e & ROLAGEM) ? " · rolagem" : "",
           (e & PARADA) ? " · VARREDURA PARADA" : "");
}

// Teste das faixas (TelaIli9341::faixasDeTeste): o Tijolo pinta uma faixa
// vermelha, uma verde e uma azul e lê de volta um ponto de cada uma com o
// RAMRD (2Eh), que devolve R, G e B com 6 bits cada (nos bits altos de cada
// byte), depois de um atraso que muda de módulo para módulo (1 clock, 1
// byte...). `lidos` são os 48 bits lidos logo depois do comando, para cada
// faixa. Devolve o atraso (em bits) em que as três cores aparecem certas,
// ou -1 se em nenhum (a memória da tela não tem as faixas).
inline int atrasoComCores(const uint64_t lidos[3]) {
  for (int o = 0; o <= 16; o++) {
    bool certo = true;
    for (int k = 0; k < 3 && certo; k++) {
      const uint32_t rgb = (uint32_t)(lidos[k] >> (48 - 24 - o)) & 0xFFFFFF;
      const uint8_t c[3] = {(uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb};
      for (int i = 0; i < 3; i++) certo = certo && (i == k ? c[i] >= 0xE0 : c[i] < 0x20);
    }
    if (certo) return o;
  }
  return -1;
}

}  // namespace estado_tela
