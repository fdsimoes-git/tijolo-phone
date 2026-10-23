// ============================================================
//  Fontes — as letras do Tijolo, já desenhadas
//
//  Cada letra (glifo) é uma imagem pequena com 16 tons de cinza, dois
//  pontos por byte. Os desenhos estão em Fontes.cpp, gerado por
//  ferramentas/gerar_fontes.py a partir da fonte Inter.
//
//  Medidas em pontos (pixels), contadas a partir da "linha de base",
//  a linha imaginária onde as letras se apoiam (o "p" desce abaixo dela).
// ============================================================
#pragma once
#include <stdint.h>

struct Glifo {
  uint16_t codigo;    // Unicode: 0x00E3 = "ã"
  uint16_t avanco;    // quanto a caneta anda depois da letra, em 1/16 de ponto
  uint8_t largura;    // tamanho do desenho
  uint8_t altura;
  int8_t dx;          // canto de cima, à esquerda, do desenho em relação à caneta
  int8_t dy;          // (dy negativo = acima da linha de base)
  uint32_t inicio;    // onde o desenho começa em `bits`
};

struct Fonte {
  const Glifo* glifos;   // em ordem de código, para a busca binária
  uint16_t total;
  const uint8_t* bits;
  int8_t ascendente;     // da linha de base até o topo da linha
  int8_t descendente;    // da linha de base até o fundo da linha
  int8_t maiuscula;      // altura do "H"
  int8_t minuscula;      // altura do "x"
  uint16_t substituto;   // o "□", para letras que a fonte não tem (emoji...)

  int alturaLinha() const { return ascendente + descendente; }

  // nullptr se a fonte não tem a letra
  const Glifo* procurar(uint32_t codigo) const {
    int ini = 0, fim = (int)total - 1;
    while (ini <= fim) {
      const int meio = (ini + fim) / 2;
      const uint32_t c = glifos[meio].codigo;
      if (c == codigo) return &glifos[meio];
      if (c < codigo) ini = meio + 1; else fim = meio - 1;
    }
    return nullptr;
  }
};

namespace fontes {
extern const Fonte fontePequena;   // 14 px: barra de cima, dicas
extern const Fonte fonteTexto;     // 18 px: mensagens
extern const Fonte fonteTitulo;    // 18 px seminegrito: títulos
extern const Fonte fonteGrande;    // 26 px: números e nomes nas chamadas
extern const Fonte fonteDiscador;  // 38 px, só números, * # + ( ) -
extern const Fonte fonteRelogio;   // 68 px, só números, : - * #
}  // namespace fontes
