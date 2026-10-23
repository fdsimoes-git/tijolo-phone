// ============================================================
//  Multitoque — escrever no teclado de telefone, como nos celulares antigos
//
//  Cada tecla tem várias letras: toque o 2 uma vez para "a", duas para "b",
//  três para "c"... Enquanto a mesma tecla é tocada de novo em menos de 1 s,
//  a última letra vai mudando (fica "pendente", sublinhada na tela); outra
//  tecla, ou 1 s parado, confirma.
//
//    1  . , ? ! : ; ' " - ( ) @ / + = & % * # $ _ < > 1
//    2  a b c á à â ã ç 2              6  m n o ó ô õ 6
//    3  d e f é ê 3                    7  p q r s 7
//    4  g h i í 4                      8  t u v ú 8
//    5  j k l 5                        9  w x y z 9
//    0  espaço 0
//
//  Modos (proximoModo): Abc (maiúscula no começo de cada frase), abc, ABC
//  e 123 (cada tecla é o seu número). Segurar uma tecla dá o número dela.
//  O * e o # ficam para quem usa a classe (no Tijolo: * troca o modo,
//  # apaga).
//
//  Só lógica, sem Arduino: os testes rodam no computador.
// ============================================================
#pragma once
#include <stdint.h>
#include <functional>
#include <string>

class Multitoque {
public:
  enum class Modo : uint8_t { Frase, Minusculas, Maiusculas, Numeros };
  static constexpr uint32_t CICLO_MS = 1000;

  void begin(const std::string& texto = "", size_t limiteBytes = 2000);
  // Um limite a mais, de quem usa (no Tijolo: caber em 6 SMS). As letras que
  // deixariam o texto fora do limite são puladas no ciclo da tecla.
  void limitar(std::function<bool(const std::string&)> cabe) { _cabe = std::move(cabe); }
  // Tecla '0'..'9' tocada. true se o texto mudou (false: outra tecla, ou
  // não cabe mais nada: aí recusou() diz true).
  bool tecla(char k, uint32_t agora);
  // Tecla '0'..'9' segurada (logo depois do toque dela): o número no lugar
  // da letra que esse toque pôs.
  bool segurou(char k, uint32_t agora);
  // A última tecla não entrou porque o texto chegou ao limite
  bool recusou() const { return _recusou; }
  // Chame sempre: depois de CICLO_MS, a letra pendente fica de vez.
  // true se confirmou agora (a tela tira o sublinhado).
  bool tique(uint32_t agora);
  bool apagar();          // o último caractere (a letra pendente, se houver)
  bool apagarPalavra();   // a última palavra (e os espaços depois dela)
  void limpar();
  void confirmar();       // a letra pendente fica de vez
  void proximoModo();     // Abc → abc → ABC → 123 → Abc

  const std::string& texto() const { return _texto; }
  bool vazio() const { return _texto.empty(); }
  bool pendente() const { return _pendente; }
  size_t inicioPendente() const { return _inicioPendente; }   // byte em que a pendente começa
  Modo modo() const { return _modo; }
  void definirModo(Modo m) { confirmar(); _modo = m; }
  const char* nomeModo() const;
  // A próxima letra vai sair maiúscula? (para o indicador da tela)
  bool proximaMaiuscula() const;

private:
  enum class Toque : uint8_t { Nenhum, Novo, Ciclou, Recusado };   // o que o último toque fez
  bool _acrescentar(const std::string& s);
  bool _cabeTexto(const std::string& t) const;
  void _fixar();
  std::string _comCaixa(const char* opcao) const;
  bool _inicioDeFrase() const;

  std::function<bool(const std::string&)> _cabe;
  Toque _ultimo = Toque::Nenhum;
  char _ultimaTecla = 0;
  bool _recusou = false;

  std::string _texto;
  size_t _limite = 2000;
  Modo _modo = Modo::Frase;
  bool _pendente = false;
  char _teclaPendente = 0;
  int _indice = 0;
  int _anterior = -1;     // a letra antes deste toque (-1: este toque começou a letra)
  bool _maiuscula = false;
  size_t _inicioPendente = 0;
  uint32_t _quando = 0;
};
