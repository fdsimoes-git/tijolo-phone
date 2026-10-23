// ============================================================
//  Teclado — o teclado 3x4 e os botões verde e vermelho
//
//  O teclado de 12 teclas tem 7 fios: 4 linhas e 3 colunas. Apertar
//  uma tecla encosta o fio da linha dela no fio da coluna. A ordem dos
//  fios muda de fabricante para fabricante, então este módulo não
//  supõe nada: puxa um fio por vez para o GND e vê qual outro fio
//  "desce" junto. Cada tecla vira um PAR de fios.
//
//  Para saber qual par é qual tecla, o Tijolo aprende uma vez (a tela
//  pede "aperte o 1", "aperte o 2"...) e guarda o mapa na memória.
//  Por isso os 7 fios podem ir nos 7 pinos em qualquer ordem.
//
//  Botões verde e vermelho: podem ser botões de verdade (entre o pino e
//  o GND, com o pull-up interno da ESP32) ou duas teclas do teclado
//  (teclasComoBotoes): tocada rápido, a tecla é o número; segurada, vira
//  o botão. Exemplo com o 3 no lugar do vermelho:
//    toque rápido             → Apertou '3', Soltou '3' (quando solta)
//    segurada 0,8 s           → Apertou vermelho (ainda segurando)
//    segurada até 1,6 s       → Segurou vermelho
//    soltou                   → Soltou vermelho
//  O número só sai quando a tecla sobe, porque até lá não dá para saber
//  se ela vai ser segurada. Só conta como segurada o tempo em que o
//  teclado foi lido de fato: se o loop ficou parado (um comando lento ao
//  modem), um toque rápido no meio não vira botão.
//
//  Não depende do Arduino: os pinos são lidos por funções passadas no
//  begin(), o que permite testar tudo no computador (test_host/).
// ============================================================
#pragma once
#include <stdint.h>

constexpr char TECLA_VERDE = 'V';
constexpr char TECLA_VERMELHA = 'R';

struct EventoTecla {
  enum Tipo : uint8_t {
    Apertou,      // tecla desceu
    Segurou,      // continua apertada há SEGURAR_MS (só uma vez por toque)
    Soltou,       // tecla subiu; `segurada` diz se houve Segurou
    Par,          // modo aprender: os fios `a` e `b` se encostaram
    ParSegurado,  // modo aprender: o par `a`-`b` continua encostado há SEGURAR_MS
  };
  Tipo tipo = Apertou;
  char tecla = 0;          // '0'..'9', '*', '#', TECLA_VERDE, TECLA_VERMELHA
  bool segurada = false;
  uint8_t a = 0, b = 0;    // Par: índices dos fios (a < b)
  char origem = 0;         // a tecla que fez as vezes do botão, ou cujo número saiu ao soltar ('1', '3')
  uint32_t quando = 0;     // quando a tecla desceu (o evento pode chegar bem depois)
};

// Como mexer nos pinos (na placa: pinMode, digitalRead...)
struct AcessoPinos {
  void (*entrada)(int pino);        // entrada com pull-up
  void (*puxarParaBaixo)(int pino); // saída em nível baixo
  bool (*ler)(int pino);            // true = nível alto
  void (*esperarUs)(uint32_t us);
};

class Teclado {
public:
  static constexpr int MAX_FIOS = 8;
  static constexpr int TECLAS = 12;
  static constexpr const char* ORDEM = "123456789*0#";   // ordem das teclas no mapa
  static constexpr uint8_t SEM_PAR = 0xFF;
  static constexpr uint32_t VARRER_MS = 5;      // uma varredura a cada 5 ms
  static constexpr uint32_t FIRME_MS = 20;      // leitura igual por 20 ms = vale (tira o "tremido")
  static constexpr uint32_t SEGURAR_MS = 800;   // apertada por mais que isso = "segurou"
  static constexpr uint32_t SEGURAR_BOTAO_MS = 1600;   // tecla no lugar de botão: botão "segurado"

  // fios: os pinos dos 7 fios do teclado; verde/vermelho: -1 = sem botão
  void begin(const int* fios, int nFios, int verde, int vermelho, const AcessoPinos& acesso);

  // Teclas que, seguradas, valem os botões verde e vermelho ('1' e '3';
  // 0 = nenhuma). Funciona junto com os botões de verdade, se houver.
  void teclasComoBotoes(char verde, char vermelho) { _comoVerde = verde; _comoVermelho = vermelho; }
  char teclaComoVerde() const { return _comoVerde; }
  char teclaComoVermelho() const { return _comoVermelho; }

  // O mapa: para cada tecla de ORDEM, o par de fios (a << 4 | b, a < b).
  void definirMapa(const uint8_t mapa[TECLAS]);
  const uint8_t* mapa() const { return _mapa; }
  bool temMapa() const { return _temMapa; }
  // Mapa "de fábrica" para quando nada foi aprendido: fios 1 a 4 = linhas,
  // 5 a 7 = colunas (o comum nos teclados de membrana).
  static void mapaPadrao(uint8_t mapa[TECLAS]);

  // Modo aprender: o teclado passa a mandar eventos Par em vez de teclas.
  void aprender(bool sim);
  bool aprendendo() const { return _aprender; }

  void varrer(uint32_t agoraMs);           // chame sempre no loop()
  bool proximoEvento(EventoTecla& ev);

  // O que estiver apertado agora não conta até tudo ficar solto por
  // FIRME_MS (quem ligou o Tijolo segurando uma tecla não abre nada sem
  // querer). aprender() também faz isso, na troca de modo.
  void ignorarAteSoltar() { _ignorar = true; _soltoMarcado = false; }
  // Alguma tecla (ou botão) está apertada agora, mesmo que ainda sem evento
  // (o 1 e o 3 só dão evento ao soltar ou ao completar 0,8 s).
  bool algumaApertada() const {
    return _fTeclado.lida || _fTeclado.valendo || _fVerde.valendo || _fVermelho.valendo || _ignorar;
  }
  // Lê na hora se a tecla está apertada ('0'..'#', ou TECLA_VERDE/
  // TECLA_VERMELHA: o botão de verdade, ou a tecla que faz as vezes dele).
  bool segurando(char tecla);
  // Os dois pinos do par de fios da tecla (pelo mapa). TECLA_VERDE e
  // TECLA_VERMELHA dão o par da tecla que faz as vezes do botão.
  bool fiosDaTecla(char tecla, int& pinoA, int& pinoB) const;

  // Confere se os 12 pares formam a grade 4x3 de um teclado de telefone.
  // Devolve os fios das linhas (1 2 3 / 4 5 6 / 7 8 9 / * 0 #) e das colunas.
  static bool validarGrade(const uint8_t mapa[TECLAS], int linhas[4], int colunas[3]);
  static char teclaDoIndice(int i) { return ORDEM[i]; }

private:
  struct Firmeza {          // tira o tremido de uma leitura
    int lida = 0, valendo = 0;
    uint32_t desde = 0;
    uint32_t apertouEm = 0;   // conta o tempo segurada (sem os trechos em que o loop parou)
    uint32_t desceuEm = 0;    // quando desceu de verdade (vai no evento)
    bool segurou = false;
    bool virouBotao = false, segurouBotao = false;   // tecla no lugar de botão
  };
  static constexpr uint32_t LOOP_PARADO_MS = 50;   // intervalo entre leituras maior que isso: o loop parou
  void _filtrar(Firmeza& f, int leitura, uint32_t agora, bool pares);
  void _soltou(const Firmeza& f);
  char _botaoDe(int tecla) const;
  // 0 = nada (ou várias teclas juntas); senão (par + 1) no modo aprender, ou
  // a tecla. `algo`: algum par de fios encostado (até várias teclas ou um
  // par fora do mapa).
  int _lerTeclado(bool& algo);
  void _publicar(const EventoTecla& ev);

  int _fios[MAX_FIOS] = {};
  int _nFios = 0;
  int _verde = -1, _vermelho = -1;
  char _comoVerde = 0, _comoVermelho = 0;
  AcessoPinos _io{};
  uint8_t _mapa[TECLAS];
  bool _temMapa = false;
  bool _aprender = false;
  bool _ignorar = false;
  bool _soltoMarcado = false;   // ignorando: já viu tudo solto, desde _soltoDesde
  uint32_t _soltoDesde = 0;
  uint32_t _ultima = 0;
  bool _primeira = true;
  Firmeza _fTeclado, _fVerde, _fVermelho;

  static constexpr int FILA = 16;
  EventoTecla _fila[FILA];
  int _ini = 0, _n = 0;
};
