// ============================================================
//  Quadro — a imagem da tela, montada na memória
//
//  Tudo é desenhado primeiro aqui (240 x 320 pontos, 16 bits por ponto,
//  o formato RGB565 da tela) e só depois copiado para a tela de verdade.
//  Assim nada pisca, e a mesma imagem pode ser salva num arquivo no
//  computador para conferir o desenho sem a placa (test_host/).
//
//  Não depende do Arduino: compila na placa e no computador.
// ============================================================
#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "Fontes.h"

// Cor RGB565: 5 bits de vermelho, 6 de verde, 5 de azul.
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

enum class Alinha : uint8_t { Esquerda, Centro, Direita };

class Quadro {
public:
  static constexpr int LARGURA = 240;
  static constexpr int ALTURA = 320;
  static constexpr size_t BYTES = (size_t)LARGURA * ALTURA * 2;

  // Usa uma área de LARGURA*ALTURA pontos já reservada (na placa, na PSRAM).
  void usar(uint16_t* pontos) { _p = pontos; semRecorte(); }
  uint16_t* pontos() { return _p; }
  const uint16_t* pontos() const { return _p; }
  uint16_t ler(int x, int y) const { return _p[y * LARGURA + x]; }

  // Só desenha dentro deste retângulo (por exemplo, o texto que rola).
  void recorte(int x, int y, int w, int h);
  void semRecorte();

  void limpar(uint16_t cor);
  void ponto(int x, int y, uint16_t cor);
  void misturar(int x, int y, uint16_t cor, uint32_t alfa);   // alfa 0 a 255
  void retangulo(int x, int y, int w, int h, uint16_t cor);
  void moldura(int x, int y, int w, int h, uint16_t cor, int espessura = 1);
  void linhaH(int x, int y, int w, uint16_t cor) { retangulo(x, y, w, 1, cor); }
  void linhaV(int x, int y, int h, uint16_t cor) { retangulo(x, y, 1, h, cor); }
  // Formas com borda suave (anti-aliasing)
  void retanguloRedondo(int x, int y, int w, int h, int raio, uint16_t cor);
  void circulo(float cx, float cy, float raio, uint16_t cor);
  void anel(float cx, float cy, float raio, float espessura, uint16_t cor);
  void triangulo(float x0, float y0, float x1, float y1, float x2, float y2, uint16_t cor);

  // Texto UTF-8 na linha de base `base`; devolve a largura desenhada.
  int texto(int x, int base, const std::string& s, const Fonte& f, uint16_t cor,
            Alinha alinha = Alinha::Esquerda);

  // Medidas e cortes de texto (não precisam de um quadro)
  static int largura(const std::string& s, const Fonte& f);
  // Corta o fim com "…" para caber em `larguraMax`.
  static std::string caber(const std::string& s, const Fonte& f, int larguraMax);
  // Quebra em linhas de no máximo `larguraMax` (respeita "\n"; corta
  // palavras compridas demais).
  static std::vector<std::string> quebrar(const std::string& s, const Fonte& f, int larguraMax);

  static uint16_t misturar565(uint16_t fundo, uint16_t cor, uint32_t alfa);

private:
  void _letra(int x, int base, const Glifo& g, const Fonte& f, uint16_t cor);
  static const Glifo* _glifo(const Fonte& f, uint32_t cp);   // nullptr = não desenha (largura zero)

  uint16_t* _p = nullptr;
  int _x0 = 0, _y0 = 0, _x1 = LARGURA, _y1 = ALTURA;   // recorte: [x0, x1) x [y0, y1)
};
