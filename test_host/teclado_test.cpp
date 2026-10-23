// ============================================================
//  teclado_test — o Teclado contra um teclado 3x4 de mentira
//
//  Simula os 7 fios ligados em ordem embaralhada, teclas apertadas
//  (com tremido de contato), duas teclas juntas, os dois botões e as
//  teclas 1 e 3 fazendo as vezes deles (seguradas).
//
//  Da raiz do projeto:
//  g++ -std=c++17 -I lib/Teclado test_host/teclado_test.cpp lib/Teclado/Teclado.cpp -o /tmp/tec && /tmp/tec
// ============================================================
#include <stdio.h>
#include <string>
#include <vector>
#include "Teclado.h"

static int falhas = 0, verificacoes = 0;
#define CONFERE(cond)                                                       \
  do {                                                                      \
    verificacoes++;                                                         \
    if (!(cond)) { falhas++; printf("  FALHOU (linha %d): %s\n", __LINE__, #cond); } \
  } while (0)

// ---------- o teclado de mentira ----------
// Pinos da placa (como no config.h) e o que cada um é no teclado.
static const int PINOS[7] = {40, 39, 38, 15, 16, 37, 21};
static const int VERDE = 35, VERMELHO = 17;
// Ordem dos fios do teclado da Adafruit (C2 R1 C1 R4 C3 R3 R2):
// fio i do teclado vai no PINOS[i].
static const int LINHA_DO_FIO[7] = {-1, 0, -1, 3, -1, 2, 1};
static const int COLUNA_DO_FIO[7] = {1, -1, 0, -1, 2, -1, -1};

static bool baixo[64];                  // pino puxado para baixo pela ESP32
static bool apertada[4][3];             // tecla (linha, coluna) apertada agora
static bool botaoVerde = false, botaoVermelho = false;

static int fioDoPino(int pino) {
  for (int i = 0; i < 7; i++) if (PINOS[i] == pino) return i;
  return -1;
}
static void entrada(int p) { baixo[p] = false; }
static void puxar(int p) { baixo[p] = true; }
static void esperar(uint32_t) {}
static bool ler(int pino) {
  if (pino == VERDE) return !botaoVerde;
  if (pino == VERMELHO) return !botaoVermelho;
  const int fio = fioDoPino(pino);
  if (fio < 0) return true;
  // Os fios ligados entre si pelas teclas apertadas formam grupos; o
  // grupo inteiro desce se qualquer fio dele está puxado para baixo.
  bool visto[7] = {}, mudou = true;
  visto[fio] = true;
  while (mudou) {
    mudou = false;
    for (int r = 0; r < 4; r++)
      for (int c = 0; c < 3; c++) {
        if (!apertada[r][c]) continue;
        int fr = -1, fc = -1;
        for (int i = 0; i < 7; i++) {
          if (LINHA_DO_FIO[i] == r) fr = i;
          if (COLUNA_DO_FIO[i] == c) fc = i;
        }
        if (visto[fr] != visto[fc]) { visto[fr] = visto[fc] = true; mudou = true; }
      }
  }
  for (int i = 0; i < 7; i++) if (visto[i] && baixo[PINOS[i]]) return false;
  return true;
}

static Teclado tec;
static uint32_t agora = 0;
static std::vector<EventoTecla> eventos;

static void passar(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t++) {
    agora++;
    tec.varrer(agora);
    EventoTecla ev;
    while (tec.proximoEvento(ev)) eventos.push_back(ev);
  }
}
static void posicao(char k, int& r, int& c) {
  const char* ordem = Teclado::ORDEM;
  for (int i = 0; i < 12; i++) if (ordem[i] == k) { r = i / 3; c = i % 3; }
}
static void apertar(char k, bool sim) { int r = 0, c = 0; posicao(k, r, c); apertada[r][c] = sim; }
static void toque(char k, uint32_t ms = 80) {
  // contato tremendo nos primeiros 6 ms
  for (int i = 0; i < 3; i++) { apertar(k, true); passar(1); apertar(k, false); passar(1); }
  apertar(k, true);
  passar(ms);
  apertar(k, false);
  passar(60);
}
static std::string resumo() {
  std::string s;
  for (const auto& e : eventos) {
    if (!s.empty()) s += " ";
    switch (e.tipo) {
      case EventoTecla::Apertou: s += std::string("+") + e.tecla; break;
      case EventoTecla::Segurou: s += std::string("!") + e.tecla; break;
      case EventoTecla::Soltou: s += std::string("-") + e.tecla + (e.segurada ? "s" : ""); break;
      case EventoTecla::Par: s += "p" + std::to_string(e.a) + std::to_string(e.b); break;
      case EventoTecla::ParSegurado: s += "P" + std::to_string(e.a) + std::to_string(e.b); break;
    }
  }
  eventos.clear();
  return s;
}

int main() {
  AcessoPinos io{entrada, puxar, ler, esperar};
  tec.begin(PINOS, 7, VERDE, VERMELHO, io);

  printf("1. aprender: 12 teclas na ordem\n");
  tec.aprender(true);
  passar(30);                                     // trocar de modo espera tudo solto por 20 ms
  uint8_t mapa[12];
  for (int i = 0; i < 12; i++) {
    toque(Teclado::ORDEM[i]);
    CONFERE(eventos.size() == 1 && eventos[0].tipo == EventoTecla::Par);
    if (!eventos.empty()) mapa[i] = (uint8_t)((eventos[0].a << 4) | eventos[0].b);
    eventos.clear();
  }
  int lin[4], col[3];
  CONFERE(Teclado::validarGrade(mapa, lin, col));
  CONFERE(lin[0] == 1 && lin[1] == 6 && lin[2] == 5 && lin[3] == 3);
  CONFERE(col[0] == 2 && col[1] == 0 && col[2] == 4);
  tec.definirMapa(mapa);
  tec.aprender(false);
  passar(30);

  printf("2. teclas normais, com tremido\n");
  toque('5');
  CONFERE(resumo() == "+5 -5");
  toque('#');
  toque('0');
  toque('*');
  CONFERE(resumo() == "+# -# +0 -0 +* -*");

  printf("3. segurar\n");
  toque('0', 900);
  CONFERE(resumo() == "+0 !0 -0s");

  printf("4. duas teclas juntas não valem\n");
  apertar('1', true); apertar('2', true);
  passar(200);
  apertar('1', false); apertar('2', false);
  passar(60);
  CONFERE(resumo() == "");
  apertar('1', true); apertar('9', true);       // linhas e colunas diferentes
  passar(200);
  apertar('1', false); apertar('9', false);
  passar(60);
  CONFERE(resumo() == "");

  printf("5. toque curto demais (5 ms) não vale\n");
  apertar('7', true); passar(5); apertar('7', false); passar(60);
  CONFERE(resumo() == "");

  printf("6. botões\n");
  botaoVermelho = true; passar(100); botaoVermelho = false; passar(60);
  CONFERE(resumo() == "+R -R");
  botaoVerde = true; passar(900); botaoVerde = false; passar(60);
  CONFERE(resumo() == "+V !V -Vs");
  // botão e tecla ao mesmo tempo
  botaoVermelho = true; apertar('3', true);
  passar(100);
  botaoVermelho = false; apertar('3', false);
  passar(60);
  {
    const std::string r = resumo();
    CONFERE(r.find("+3") != std::string::npos && r.find("+R") != std::string::npos);
  }

  printf("7. mapa padrão e grades erradas\n");
  uint8_t padrao[12];
  Teclado::mapaPadrao(padrao);
  CONFERE(Teclado::validarGrade(padrao, lin, col));
  CONFERE(lin[0] == 0 && col[0] == 4);
  uint8_t errado[12];
  for (int i = 0; i < 12; i++) errado[i] = mapa[i];
  const uint8_t t = errado[4]; errado[4] = errado[5]; errado[5] = t;  // "5" e "6" trocados
  CONFERE(!Teclado::validarGrade(errado, lin, col));
  errado[5] = errado[4];                                               // par repetido
  CONFERE(!Teclado::validarGrade(errado, lin, col));
  errado[5] = Teclado::SEM_PAR;
  CONFERE(!Teclado::validarGrade(errado, lin, col));

  printf("8. muitos eventos sem ler: a fila não estoura\n");
  for (int i = 0; i < 40; i++) {
    apertar('8', true); for (int k = 0; k < 30; k++) { agora++; tec.varrer(agora); }
    apertar('8', false); for (int k = 0; k < 30; k++) { agora++; tec.varrer(agora); }
  }
  int n = 0;
  EventoTecla ev;
  while (tec.proximoEvento(ev)) n++;
  CONFERE(n == 16);

  printf("9. segurar 1 = verde, segurar 3 = vermelho\n");
  while (tec.proximoEvento(ev)) {}
  eventos.clear();
  tec.teclasComoBotoes('1', '3');
  toque('1');                                     // rápido: é o número, quando solta
  CONFERE(resumo() == "+1 -1");
  toque('3', 300);
  CONFERE(resumo() == "+3 -3");
  apertar('1', true);
  passar(700);
  CONFERE(resumo() == "");                        // ainda não dá para saber
  passar(200);
  CONFERE(resumo() == "+V");                      // 0,8 s: é o verde (ainda segurando)
  apertar('1', false);
  passar(60);
  CONFERE(resumo() == "-V");
  toque('3', 1700);                               // 1,6 s: vermelho "segurado"
  CONFERE(resumo() == "+R !R -Rs");
  toque('5', 900);                                // as outras não mudam
  CONFERE(resumo() == "+5 !5 -5s");
  toque('0');
  CONFERE(resumo() == "+0 -0");
  botaoVerde = true; passar(100); botaoVerde = false; passar(60);   // o botão de verdade continua valendo
  CONFERE(resumo() == "+V -V");
  // do 1 direto para outra tecla, sem soltar no meio
  apertar('1', true); passar(100); apertar('1', false); apertar('4', true); passar(100);
  apertar('4', false); passar(60);
  CONFERE(resumo() == "+1 -1 +4 -4");

  printf("10. aprendendo: segurar dá ParSegurado (sem virar botão)\n");
  tec.aprender(true);
  passar(30);
  toque('1', 900);
  {
    const std::string r = resumo();
    CONFERE(r.size() == 7 && r[0] == 'p' && r[4] == 'P' && r.substr(1, 2) == r.substr(5, 2));
  }
  toque('3');
  CONFERE(resumo().size() == 3);                  // só o Par
  tec.aprender(false);

  printf("11. segurando desde o começo: só vale depois de soltar\n");
  apertar('3', true);
  passar(100);
  resumo();
  tec.ignorarAteSoltar();
  passar(2000);
  CONFERE(resumo() == "");                        // nem "segurou", nem o vermelho
  apertar('3', false);
  passar(60);
  CONFERE(resumo() == "");
  toque('3');
  CONFERE(resumo() == "+3 -3");

  printf("12. ler uma tecla na hora e os fios de cada tecla\n");
  CONFERE(!tec.segurando('3'));
  apertar('3', true);
  CONFERE(tec.segurando('3'));
  CONFERE(!tec.segurando('1'));
  CONFERE(tec.segurando(TECLA_VERMELHA));         // o 3 faz as vezes do vermelho
  apertar('3', false);
  CONFERE(!tec.segurando(TECLA_VERMELHA));
  botaoVermelho = true;                           // e o botão de verdade também vale
  CONFERE(tec.segurando(TECLA_VERMELHA));
  botaoVermelho = false;
  passar(60);
  resumo();
  {
    int a = 0, b = 0;
    CONFERE(tec.fiosDaTecla('3', a, b));          // 3 = linha 1 (fio 1, IO39) e coluna 3 (fio 4, IO16)
    CONFERE(a == 39 && b == 16);
    CONFERE(tec.fiosDaTecla(TECLA_VERMELHA, a, b) && a == 39 && b == 16);
    CONFERE(tec.fiosDaTecla(TECLA_VERDE, a, b) && a == 39 && b == 38);   // 1 = linha 1 e coluna 1 (fio 2, IO38)
    // sem botões de verdade: o vermelho é o 3
    Teclado t2;
    t2.begin(PINOS, 7, -1, -1, io);
    t2.definirMapa(mapa);
    t2.teclasComoBotoes('1', '3');
    apertar('3', true);
    CONFERE(t2.segurando(TECLA_VERMELHA));
    CONFERE(!t2.segurando(TECLA_VERDE));
    apertar('3', false);
    CONFERE(!t2.segurando(TECLA_VERMELHA));
    Teclado t3;                                    // sem tecla no lugar do botão
    t3.begin(PINOS, 7, -1, -1, io);
    CONFERE(!t3.fiosDaTecla(TECLA_VERMELHA, a, b));
    CONFERE(!t3.segurando(TECLA_VERMELHA));
  }

  printf("13. loop parado no meio de um toque: não vira botão\n");
  while (tec.proximoEvento(ev)) {}
  eventos.clear();
  apertar('1', true);
  passar(30);                                     // desceu e firmou
  agora += 120;                                   // o loop parou (comando lento ao modem)...
  apertar('1', false);                            // ...e a pessoa soltou no meio
  agora += 780;
  passar(60);
  CONFERE(resumo() == "+1 -1");                   // foi um toque: é o número
  apertar('3', true);
  passar(30);
  agora += 900;                                   // parou com a tecla segurada
  passar(10);
  CONFERE(resumo() == "");                        // o tempo parado não conta
  passar(800);
  {
    bool achou = false;
    for (const auto& e : eventos)
      if (e.tipo == EventoTecla::Apertou && e.tecla == TECLA_VERMELHA) {
        achou = true;
        CONFERE(e.origem == '3');
        CONFERE(agora - e.quando >= 1700 && agora - e.quando <= 1760);   // quando desceu de verdade
      }
    CONFERE(achou);
  }
  CONFERE(resumo() == "+R");
  apertar('3', false);
  passar(60);
  CONFERE(resumo() == "-R");

  printf("14. ignorar até soltar: tremido e duas teclas\n");
  apertar('3', true);
  passar(50);
  resumo();
  tec.ignorarAteSoltar();
  passar(500);
  apertar('3', false); passar(5); apertar('3', true);   // tremido de 5 ms
  passar(2000);
  CONFERE(resumo() == "");                        // continua ignorando (nada de vermelho)
  apertar('5', true);                             // duas teclas: ainda tem coisa apertada
  passar(100);
  apertar('5', false);
  passar(100);
  CONFERE(resumo() == "");
  apertar('3', false);
  passar(60);
  CONFERE(resumo() == "");
  toque('3');
  CONFERE(resumo() == "+3 -3");

  printf("15. trocar de modo com a tecla segurada: ela não conta de novo\n");
  tec.aprender(true);
  passar(30);
  apertar('3', true);
  passar(900);
  CONFERE(resumo().size() == 7);                  // Par e ParSegurado
  tec.aprender(false);                            // "pular", com o 3 ainda segurado
  passar(2500);
  CONFERE(resumo() == "");                        // nada de vermelho nem de "desligar"
  apertar('3', false);
  passar(60);
  toque('3');
  CONFERE(resumo() == "+3 -3");
  apertar('1', true);
  passar(900);
  resumo();
  tec.aprender(true);                             // "recomeçar", com o 1 ainda segurado
  passar(1000);
  CONFERE(resumo() == "");                        // não vira o passo "1" sozinho
  apertar('1', false);
  passar(60);
  toque('1');
  CONFERE(resumo().size() == 3);                  // agora sim: um Par
  tec.aprender(false);
  passar(60);
  resumo();

  printf("\n%d verificações, %d falha(s)\n", verificacoes, falhas);
  return falhas ? 1 : 0;
}
