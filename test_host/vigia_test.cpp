// Testes da VigiaTela com uma tela de mentira: ruído de rádio na leitura
// (aleatório e o pior caso, sempre igual dentro das janelas de transmissão
// do 2G), falhas que o reenvio conserta, que só o RESET conserta, que não
// se consertam, fio MISO solto (e voltando), rede de segurança, relógio
// dando a volta e o painel que para de varrer com o boletim certo.
//   g++ -std=c++17 -I lib/Tela test_host/vigia_test.cpp -o /tmp/vigia && /tmp/vigia
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "VigiaTela.h"

using namespace estado_tela;

static int verificacoes = 0, falhas = 0;
#define CONFERE(cond)                                                     \
  do {                                                                    \
    verificacoes++;                                                       \
    if (!(cond)) { falhas++; printf("FALHOU linha %d: %s\n", __LINE__, #cond); } \
  } while (0)

constexpr uint32_t BOM = 0x84530400u;                     // tela configurada pelo Tijolo
constexpr uint32_t FABRICA = (6u << 20) | NORMAL;         // depois de um reset
constexpr uint32_t DORMIU = BOM & ~(ACORDADA | BOOSTER);
constexpr uint32_t SEM_CONVERSOR = BOM & ~BOOSTER;

// A tela de mentira, com relógio em microssegundos
struct TelaFalsa : AcoesTela {
  uint64_t us = 0;
  uint32_t estado = BOM;
  bool reforcoConserta = true;  // o reenvio devolve a tela ao normal?
  bool resetConserta = true;    // e o RESET?
  bool misoSolto = false;       // com o pull-up: lê tudo 1
  bool ruidoAleatorio = false;  // cada leitura sai com lixo diferente
  int lixoNasProximas = 0;      // as próximas N leituras saem com o mesmo lixo
  int gsmJanelas = 0;           // 2G transmitindo: N janelas de 577 µs a cada 4615 µs...
  uint32_t bomDeMentiraCada = 0;// ...ou, com a tela quebrada, 1 leitura em N parece boa
  int leituras = 0, reforcos = 0, resets = 0;
  uint32_t ultimo = 0;

  uint32_t agora() override { return (uint32_t)(us / 1000); }
  void esperarUs(uint32_t t) override { us += t; }
  uint32_t ultimoReforco() override { return ultimo; }
  uint32_t ler() override {
    leituras++;
    us += 10;                   // uma leitura leva uns 10 µs
    if (misoSolto) return 0xFFFFFFFFu;
    if (ruidoAleatorio) return (uint32_t)rand() * 2654435761u;
    if (lixoNasProximas > 0) { lixoNasProximas--; return 0x12345678u; }
    // Pior caso do rádio: dentro da janela de transmissão a leitura sai
    // errada, e SEMPRE do mesmo jeito (o bit da imagem ligada some).
    if (gsmJanelas && us % 4615 < (uint64_t)577 * gsmJanelas) return estado & ~LIGADA;
    if (bomDeMentiraCada && estado != BOM && leituras % bomDeMentiraCada == 0) return BOM;
    return estado;
  }
  void reforcar() override {
    reforcos++;
    us += 45000;                // 5 ms de comandos + ~40 ms da imagem inteira
    ultimo = agora();
    if (reforcoConserta && estado != SEM_CONVERSOR && !(estado & PARADA)) estado = BOM;
  }
  void reiniciar() override {
    resets++;
    us += 420000;
    ultimo = agora();
    if (resetConserta) estado = BOM;
  }
};

// Roda a vigia por `ms`, chamando passo() como o loop do Tijolo: a cada
// 0,9 a 1,6 ms (o loop não tem passo fixo)
static void rodar(VigiaTela& v, TelaFalsa& tela, uint32_t ms) {
  const uint64_t fim = tela.us + (uint64_t)ms * 1000;
  while (tela.us < fim) {
    v.passo(tela, tela.agora());
    tela.us += 900 + rand() % 700;
  }
}

static int ocorrencias(VigiaTela& v, OcorrenciaTela* ultima = nullptr) {
  int n = 0;
  OcorrenciaTela o;
  while (v.proximaOcorrencia(o)) { n++; if (ultima) *ultima = o; }
  return n;
}

static uint32_t totalFalhas(const VigiaTela& v) {
  uint32_t t = 0;
  for (int i = 0; i < N_FALHAS; i++) t += v.contagem().falhas[i];
  return t;
}

// Reenvios feitos para consertar (sem contar os de segurança)
static int reforcosDeConserto(const VigiaTela& v, const TelaFalsa& tela) {
  return tela.reforcos - (int)v.contagem().reforcosDeSeguranca;
}

int main() {
  srand(7);

  // 1. Tela boa: lê a cada ~100 ms, não conserta nada; rede de segurança a cada minuto
  {
    TelaFalsa tela; VigiaTela v; v.comecar(BOM, 100, tela.agora());
    rodar(v, tela, 300000);                // 5 min
    CONFERE(tela.leituras >= 2800 && tela.leituras <= 3050);
    CONFERE(tela.resets == 0 && reforcosDeConserto(v, tela) == 0);
    CONFERE(v.contagem().reforcosDeSeguranca >= 4 && v.contagem().reforcosDeSeguranca <= 5);
    CONFERE(ocorrencias(v) == 0 && totalFalhas(v) == 0 && v.contagem().leiturasRuins == 0);
    // Ao acender a tela: confere já e reenvia depois da leitura boa
    v.conferirJa(tela.agora());
    v.pedirReforco();
    const int antes = tela.reforcos;
    rodar(v, tela, 5);
    CONFERE(tela.reforcos == antes + 1);
  }

  // 2. Um surto estraga uma leitura: é ruído, não falha
  {
    TelaFalsa tela; VigiaTela v; v.comecar(BOM, 100, tela.agora());
    rodar(v, tela, 1000);
    tela.lixoNasProximas = 1;
    rodar(v, tela, 2000);
    CONFERE(v.contagem().leiturasRuins == 1);
    CONFERE(totalFalhas(v) == 0 && tela.resets == 0 && reforcosDeConserto(v, tela) == 0 && ocorrencias(v) == 0);
  }

  // 3. Ruído forte (lixo diferente a cada leitura) por 20 s: não é falha
  {
    TelaFalsa tela; VigiaTela v; v.comecar(BOM, 100, tela.agora());
    rodar(v, tela, 1000);
    tela.ruidoAleatorio = true;
    rodar(v, tela, 20000);
    tela.ruidoAleatorio = false;
    rodar(v, tela, 2000);
    CONFERE(totalFalhas(v) == 0 && tela.resets == 0 && reforcosDeConserto(v, tela) == 0);
    CONFERE(v.contagem().leiturasRuins > 50);
    CONFERE(v.ativa());
  }

  // 4. 2G transmitindo (pior caso: a leitura errada é sempre igual dentro da
  //    janela), com 1 janela (voz) e 4 janelas (dados) por quadro, 30 min
  for (int janelas : {1, 2, 4}) {
    TelaFalsa tela; VigiaTela v; v.comecar(BOM, 100, tela.agora());
    tela.gsmJanelas = janelas;
    rodar(v, tela, 1800000);
    CONFERE(totalFalhas(v) == 0);
    CONFERE(tela.resets == 0 && reforcosDeConserto(v, tela) == 0);
    CONFERE(v.contagem().leiturasRuins > 100);   // viu o ruído, e só contou como ruído
  }

  // 5. A tela reinicia (queda de tensão): o reenvio conserta, conta uma falha
  {
    TelaFalsa tela; VigiaTela v; v.comecar(BOM, 100, tela.agora());
    rodar(v, tela, 1000);
    tela.estado = FABRICA;
    const uint32_t quebrou = tela.agora();
    rodar(v, tela, 1000);
    OcorrenciaTela o;
    CONFERE(ocorrencias(v, &o) == 1);
    CONFERE(o.falha == Falha::Reiniciou && o.consertou && o.conserto == 1 && o.tentativas == 1 && o.depois == BOM);
    CONFERE(o.quando - quebrou <= 110);    // percebe em até ~0,1 s
    CONFERE(reforcosDeConserto(v, tela) == 1 && tela.resets == 0);
    CONFERE(v.contagem().falhas[(int)Falha::Reiniciou] == 1 && v.contagem().consertosLeves == 1);
    CONFERE(!v.consertando());
  }

  // 6. A mesma falha com o 2G transmitindo: continua contada uma vez e consertada
  {
    TelaFalsa tela; VigiaTela v; v.comecar(BOM, 100, tela.agora());
    tela.gsmJanelas = 1;
    rodar(v, tela, 1000);
    tela.estado = FABRICA;
    rodar(v, tela, 2000);
    OcorrenciaTela o;
    CONFERE(ocorrencias(v, &o) == 1 && o.consertou && o.falha == Falha::Reiniciou);
    CONFERE(v.contagem().falhas[(int)Falha::Reiniciou] == 1 && totalFalhas(v) == 1);
  }

  // 7. Conversor interno parado: vai direto para o RESET
  {
    TelaFalsa tela; VigiaTela v; v.comecar(BOM, 100, tela.agora());
    rodar(v, tela, 500);
    tela.estado = SEM_CONVERSOR;
    rodar(v, tela, 2000);
    OcorrenciaTela o;
    CONFERE(ocorrencias(v, &o) == 1);
    CONFERE(o.falha == Falha::ConversorParou && o.consertou && o.conserto == 2);
    CONFERE(reforcosDeConserto(v, tela) == 0 && tela.resets == 1);
  }

  // 8. Só o RESET conserta: reenvio, confere, RESET, confere
  {
    TelaFalsa tela; VigiaTela v; v.comecar(BOM, 100, tela.agora());
    tela.reforcoConserta = false;
    rodar(v, tela, 500);
    tela.estado = DORMIU;
    rodar(v, tela, 3000);
    OcorrenciaTela o;
    CONFERE(ocorrencias(v, &o) == 1);
    CONFERE(o.falha == Falha::Dormiu && o.consertou && o.conserto == 2 && o.tentativas == 2);
    CONFERE(reforcosDeConserto(v, tela) == 1 && tela.resets == 1);
    CONFERE(v.contagem().consertosCompletos == 1 && v.contagem().falhas[(int)Falha::Dormiu] == 1);
  }

  // 9. Nada conserta: avisa uma vez, conta uma falha, tenta cada vez mais espaçado
  {
    TelaFalsa tela; VigiaTela v; v.comecar(BOM, 100, tela.agora());
    tela.reforcoConserta = tela.resetConserta = false;
    rodar(v, tela, 500);
    tela.estado = DORMIU;
    rodar(v, tela, 300000);                // 5 minutos
    OcorrenciaTela o;
    CONFERE(ocorrencias(v, &o) == 1);      // um aviso só
    CONFERE(!o.consertou && o.falha == Falha::Dormiu && o.depois == DORMIU);
    CONFERE(v.contagem().falhas[(int)Falha::Dormiu] == 1);   // contada uma vez
    // 2, 4, 8, 16, 32, 60, 60, 60 s...: umas 9 rodadas em 5 min, não centenas
    CONFERE(tela.resets >= 6 && tela.resets <= 12);
    CONFERE(reforcosDeConserto(v, tela) == tela.resets);
    // tempo travado em consertos: bem menos de 2% dos 5 minutos
    CONFERE((tela.resets * 420 + tela.reforcos * 45) < 300000 / 50);
    // Uma leitura boa de mentira no meio não fecha a falha (precisa de duas)
    tela.bomDeMentiraCada = 7;
    rodar(v, tela, 120000);
    CONFERE(ocorrencias(v) == 0);
    tela.bomDeMentiraCada = 0;
    // Depois volta sozinha (a tensão normalizou): fecha a falha e avisa
    tela.estado = BOM;
    rodar(v, tela, 61000);
    CONFERE(ocorrencias(v, &o) == 1);
    CONFERE(o.consertou && o.conserto == 0 && o.tentativas >= 12);
    const int resetsAntes = tela.resets;
    rodar(v, tela, 60000);
    CONFERE(tela.resets == resetsAntes && ocorrencias(v) == 0);
    // Uma falha nova depois disso volta a ser contada e a espera recomeça curta
    tela.reforcoConserta = true;
    tela.estado = FABRICA;
    rodar(v, tela, 1000);
    CONFERE(ocorrencias(v, &o) == 1 && o.consertou && o.falha == Falha::Reiniciou && o.tentativas == 1);
    CONFERE(v.contagem().falhas[(int)Falha::Reiniciou] == 1);
  }

  // 10. Falha que dura horas: as tentativas não dão a volta no contador
  {
    TelaFalsa tela; VigiaTela v; v.comecar(BOM, 100, tela.agora());
    tela.reforcoConserta = tela.resetConserta = false;
    rodar(v, tela, 500);
    tela.estado = DORMIU;
    rodar(v, tela, 6 * 3600 * 1000u);      // 6 horas
    tela.estado = BOM;
    rodar(v, tela, 61000);
    OcorrenciaTela o;
    while (v.proximaOcorrencia(o)) {}
    CONFERE(o.consertou && o.tentativas > 600);
  }

  // 11. Fio MISO solto: não "conserta" nada, desiste em ~3 s, avisa, e
  //     volta quando o fio volta
  {
    TelaFalsa tela; VigiaTela v; v.comecar(BOM, 100, tela.agora());
    rodar(v, tela, 500);
    tela.misoSolto = true;
    rodar(v, tela, 10000);
    OcorrenciaTela o;
    CONFERE(!v.ativa());
    CONFERE(ocorrencias(v, &o) == 1 && o.falha == Falha::SemResposta && !o.consertou);
    CONFERE(tela.resets == 0 && reforcosDeConserto(v, tela) == 0);
    // desistiu: passo() não lê mais; tentarVoltar() lê a cada 15 s
    int lidas = tela.leituras;
    rodar(v, tela, 10000);
    CONFERE(tela.leituras == lidas);
    for (int i = 0; i < 40000; i++) { v.tentarVoltar(tela, tela.agora()); tela.us += 1000; }
    CONFERE(tela.leituras - lidas >= 2 && tela.leituras - lidas <= 4);   // 2 ou 3 tentativas em 40 s
    // A tela ficou sem energia e voltou "de fábrica": a vigia volta e conserta
    tela.misoSolto = false;
    tela.estado = FABRICA;
    for (int i = 0; i < 16000 && !v.ativa(); i++) { v.tentarVoltar(tela, tela.agora()); tela.us += 1000; }
    CONFERE(v.ativa());
    CONFERE(ocorrencias(v, &o) == 1 && o.falha == Falha::SemResposta && o.consertou);
    rodar(v, tela, 1000);
    CONFERE(ocorrencias(v, &o) == 1 && o.falha == Falha::Reiniciou && o.consertou);
  }

  // 12. Um SemResposta isolado (o fio encostou) não desiste
  {
    TelaFalsa tela; VigiaTela v; v.comecar(BOM, 100, tela.agora());
    rodar(v, tela, 500);
    tela.misoSolto = true;
    rodar(v, tela, 150);                   // uma confirmação de SemResposta
    tela.misoSolto = false;
    rodar(v, tela, 5000);
    CONFERE(v.ativa());
    CONFERE(tela.resets == 0 && reforcosDeConserto(v, tela) == 0 && ocorrencias(v) == 0);
  }

  // 13. O fio solta no meio de um conserto: não dá RESET à toa e desiste
  {
    TelaFalsa tela; VigiaTela v; v.comecar(BOM, 100, tela.agora());
    tela.reforcoConserta = false;
    rodar(v, tela, 500);
    tela.estado = DORMIU;
    while (reforcosDeConserto(v, tela) == 0) rodar(v, tela, 1);   // acabou de reenviar
    tela.misoSolto = true;
    rodar(v, tela, 10000);
    CONFERE(tela.resets == 0);
    CONFERE(!v.ativa() && !v.consertando());
  }

  // 14. Ruído forte bem na hora de conferir um conserto: não fica preso
  {
    TelaFalsa tela; VigiaTela v; v.comecar(BOM, 100, tela.agora());
    tela.reforcoConserta = false;
    rodar(v, tela, 500);
    tela.estado = DORMIU;
    while (reforcosDeConserto(v, tela) == 0) rodar(v, tela, 1);
    tela.ruidoAleatorio = true;
    rodar(v, tela, 1500);
    CONFERE(v.consertando());              // esperando o ruído passar...
    rodar(v, tela, 60000);
    tela.ruidoAleatorio = false;
    rodar(v, tela, 130000);                // ...e quando passa, termina o conserto
    CONFERE(tela.resets >= 1);
    CONFERE(!v.consertando());
    OcorrenciaTela o;
    CONFERE(ocorrencias(v, &o) >= 1 && o.consertou);
  }

  // 15. conferirJa não atropela um conserto em andamento
  {
    TelaFalsa tela; VigiaTela v; v.comecar(BOM, 100, tela.agora());
    tela.reforcoConserta = false;
    rodar(v, tela, 500);
    tela.estado = DORMIU;
    while (reforcosDeConserto(v, tela) == 0) rodar(v, tela, 1);
    for (int i = 0; i < 100; i++) { v.conferirJa(tela.agora()); rodar(v, tela, 1); }
    CONFERE(tela.resets == 0);             // ainda esperando os 150 ms
    rodar(v, tela, 1000);
    CONFERE(tela.resets == 1);
  }

  // 16. Relógio dando a volta (millis() estoura em ~49 dias)
  {
    TelaFalsa tela; tela.us = (uint64_t)0xFFFFFF00u * 1000;
    tela.ultimo = tela.agora();
    VigiaTela v; v.comecar(BOM, 100, tela.agora());
    rodar(v, tela, 100);
    tela.estado = FABRICA;
    rodar(v, tela, 1000);
    OcorrenciaTela o;
    CONFERE(ocorrencias(v, &o) == 1 && o.consertou);
    CONFERE(reforcosDeConserto(v, tela) == 1);
    rodar(v, tela, 5000);
    CONFERE(tela.leituras > 40 && ocorrencias(v) == 0);
  }

  // 17. Painel parado (boletim certo, varredura parada): RESET direto, como o conversor
  {
    TelaFalsa tela; VigiaTela v; v.comecar(BOM, 100, tela.agora());
    rodar(v, tela, 500);
    tela.estado = BOM | PARADA;
    rodar(v, tela, 2000);
    OcorrenciaTela o;
    CONFERE(ocorrencias(v, &o) == 1);
    CONFERE(o.falha == Falha::PainelParado && o.consertou && o.conserto == 2 && o.tentativas == 1);
    CONFERE(reforcosDeConserto(v, tela) == 0 && tela.resets == 1);
    CONFERE(v.contagem().falhas[(int)Falha::PainelParado] == 1 && totalFalhas(v) == 1);
    // e, se o RESET não destrava, não fica preso: espera crescente
    tela.resetConserta = false;
    tela.estado = BOM | PARADA;
    rodar(v, tela, 20000);
    CONFERE(tela.resets >= 3 && tela.resets <= 6);
  }

  printf("%d verificações, %d falha(s)\n", verificacoes, falhas);
  return falhas ? 1 : 0;
}
