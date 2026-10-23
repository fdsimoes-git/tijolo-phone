// Testes do diagnóstico da tela (EstadoTela.h) e das janelas do Medidor
// (Historico, em Medidor.h). Rodam no computador:
//   g++ -std=c++17 -I lib/Tela -I lib/Medidor -I lib/Memoria test_host/diagnostico_test.cpp -o /tmp/diag && /tmp/diag
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "EstadoTela.h"
#include "Medidor.h"
#include "Sentinela.h"

using namespace estado_tela;

static int verificacoes = 0, falhas = 0;
#define CONFERE(cond)                                                     \
  do {                                                                    \
    verificacoes++;                                                       \
    if (!(cond)) { falhas++; printf("FALHOU linha %d: %s\n", __LINE__, #cond); } \
  } while (0)

int main() {
  // Uma tela configurada pelo Tijolo: conversor ligado, MADCTL 0x08 (BGR),
  // 16 bits, acordada, modo normal, imagem ligada.
  const uint32_t madctl08 = (0x08u >> 2) << 25;
  const uint32_t bom = BOOSTER | madctl08 | (5u << 20) | ACORDADA | NORMAL | LIGADA;
  CONFERE(funcionando(bom));
  CONFERE(classificar(bom, bom) == Falha::Nenhuma);
  // bits que não são conferidos (TE, gama) podem mudar
  CONFERE(classificar(bom, bom | (1u << 9) | (3u << 6)) == Falha::Nenhuma);

  // Exemplo montado a partir do datasheet: MADCTL 48h, 16 bits, acordada,
  // imagem ligada → A4 53 04 00
  CONFERE(funcionando(0xA4530400u));
  CONFERE(madctl(0xA4530400u) == 0x48);
  CONFERE(((0xA4530400u & FORMATO) >> 20) == 5);

  // Estado de fábrica depois de um reset: repouso, imagem desligada,
  // conversor parado, 18 bits, MADCTL zerado, modo normal
  const uint32_t fabrica = (6u << 20) | NORMAL;
  CONFERE(!funcionando(fabrica));
  CONFERE(classificar(bom, fabrica) == Falha::Reiniciou);

  // Comando falso de repouso (SLPIN): para o conversor, a imagem continua "ligada"
  CONFERE(classificar(bom, bom & ~(ACORDADA | BOOSTER)) == Falha::Dormiu);
  // DISPOFF falso
  CONFERE(classificar(bom, bom & ~LIGADA) == Falha::ImagemDesligada);
  // Conversor parou com a tela acordada e ligada
  CONFERE(classificar(bom, bom & ~BOOSTER) == Falha::ConversorParou);
  // MADCTL, formato, inversão, modos especiais
  CONFERE(classificar(bom, bom ^ (1u << 30)) == Falha::ConfigMudou);
  // formato de volta a 18 bits = reiniciou, mesmo acordada depois por um comando solto
  CONFERE(classificar(bom, (bom & ~FORMATO) | (6u << 20)) == Falha::Reiniciou);
  CONFERE(classificar(bom, fabrica | ACORDADA | BOOSTER) == Falha::Reiniciou);
  CONFERE(classificar(bom, (bom & ~FORMATO) | (3u << 20)) == Falha::ConfigMudou);
  // referência esperada para este Tijolo (rotação 0, BGR): 0x84530400
  CONFERE(bom == 0x84530400u);
  // bits que o datasheet não define (D13..D11) não contam
  CONFERE(classificar(bom, bom | (1u << 13)) == Falha::Nenhuma);
  CONFERE(madctl(bom) == 0x08);
  CONFERE(madctl(bom ^ (1u << 30)) == 0x88);
  CONFERE(classificar(bom, bom | OCIOSO) == Falha::ConfigMudou);
  CONFERE(classificar(bom, bom | ROLAGEM) == Falha::ConfigMudou);
  // Varredura parada (D0, anotado pelo Tijolo): o boletim está certo, mas o painel parou
  CONFERE(classificar(bom, bom | PARADA) == Falha::PainelParado);
  CONFERE(classificar(bom, (bom & ~LIGADA) | PARADA) == Falha::ImagemDesligada);   // o resto vem antes
  CONFERE(classificar(bom, fabrica | PARADA) == Falha::Reiniciou);
  CONFERE((bom & PARADA) == 0 && (0x84530400u & PARADA) == 0);                   // o RDDST de verdade tem D0 = 0
  // Fio solto / tela sem energia
  CONFERE(classificar(bom, 0) == Falha::SemResposta);
  CONFERE(classificar(bom, 0xFFFFFFFFu) == Falha::SemResposta);
  CONFERE(!funcionando(0xFFFFFFFFu));
  CONFERE(!respondeu(0));

  // Todos os tipos têm nome; os de falha têm pista
  for (int i = 0; i < N_FALHAS; i++) CONFERE(strlen(nome((Falha)i)) > 0);
  for (int i = 1; i < N_FALHAS; i++) CONFERE(strlen(pista((Falha)i)) > 0);

  // Descrição legível
  char t[160];
  descrever(bom, t, sizeof t);
  CONFERE(strstr(t, "acordada") && strstr(t, "imagem ligada") && strstr(t, "16 bits") && strstr(t, "MADCTL 0x08"));
  descrever(fabrica, t, sizeof t);
  CONFERE(strstr(t, "EM REPOUSO") && strstr(t, "IMAGEM DESLIGADA") && strstr(t, "PARADO") && strstr(t, "18 bits"));
  descrever(0xFFFFFFFFu, t, sizeof t);
  CONFERE(strstr(t, "sem resposta"));
  descrever(bom | PARADA, t, sizeof t);
  CONFERE(strstr(t, "VARREDURA PARADA") && strstr(t, "imagem ligada"));
  descrever(bom, t, 20);                        // buffer pequeno: corta, não estoura
  CONFERE(strlen(t) == 19);

  // ----- Historico (janelas de 100 ms do Medidor) -----
  Historico h;
  CONFERE(h.ultimas(10).n == 0);
  CONFERE(h.ultimas(10).media() == 0);
  for (int i = 0; i < 5; i++) {
    Janela j;
    j.somar(3300);
    j.somar(3300 - 10 * i);                     // a janela i tem mínimo 3300 - 10i
    h.colocar(j);
  }
  Janela r = h.ultimas(2);                      // janelas 3 e 4
  CONFERE(r.n == 4 && r.minimo == 3260 && r.maximo == 3300);
  r = h.ultimas(100);                           // pede mais do que tem: junta as 5
  CONFERE(r.n == 10 && r.minimo == 3260 && r.maximo == 3300);
  CONFERE(r.media() == (3300 * 5 + 3300 + 3290 + 3280 + 3270 + 3260) / 10);
  // Janela vazia no meio (a tarefa não leu nada) não estraga o mínimo
  h.colocar(Janela{});
  r = h.ultimas(2);
  CONFERE(r.n == 2 && r.minimo == 3260);
  // Dá a volta no anel: só as 100 mais novas contam
  for (int i = 0; i < 150; i++) {
    Janela j;
    j.somar(i < 50 ? 1000 : 3000);
    h.colocar(j);
  }
  r = h.ultimas(100);
  CONFERE(r.n == 100 && r.minimo == 3000 && r.maximo == 3000);
  // Muitas leituras: a soma não estoura
  Janela grande;
  for (int i = 0; i < 2000000; i++) grande.somar(4200);
  CONFERE(grande.media() == 4200);

  // ----- Faixas de teste (RAMRD): acha o atraso em que as três cores aparecem -----
  {
    // vermelho, verde e azul em 6 bits (nos bits altos de cada byte), como o ILI9341 devolve
    const uint32_t cores[3] = {0xFC0000, 0x00FC00, 0x0000FC};
    for (int atraso : {1, 8, 9}) {
      uint64_t lidos[3];
      for (int k = 0; k < 3; k++) {
        // depois do atraso vem o ponto (24 bits); o resto, lixo de 1s
        const uint64_t lixoAntes = (atraso ? ((1ull << atraso) - 1) : 0) << (48 - atraso);
        lidos[k] = lixoAntes | ((uint64_t)cores[k] << (48 - 24 - atraso)) | ((1ull << (48 - 24 - atraso)) - 1);
      }
      CONFERE(atrasoComCores(lidos) == atraso);
    }
    uint64_t branco[3] = {0xFFFFFFFFFFFFull, 0xFFFFFFFFFFFFull, 0xFFFFFFFFFFFFull};
    CONFERE(atrasoComCores(branco) == -1);   // tela com tudo branco na memória
    uint64_t zero[3] = {0, 0, 0};
    CONFERE(atrasoComCores(zero) == -1);
  }

  // ----- Sentinela da PSRAM: padrão que não se confunde com memória "tudo 1" ou "tudo 0" -----
  {
    int iguais = 0;
    for (uint32_t i = 0; i < 1024; i++) {
      const uint32_t v = valorSentinela(i);
      CONFERE(v != 0xFFFFFFFFu && v != 0);
      if (i && v == valorSentinela(i - 1)) iguais++;
    }
    CONFERE(iguais == 0);
  }

  printf("%d verificações, %d falha(s)\n", verificacoes, falhas);
  return falhas ? 1 : 0;
}
