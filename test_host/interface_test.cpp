// ============================================================
//  interface_test — as telas do Tijolo no computador, sem placa
//
//  Liga a Interface a um telefone de mentira, aperta teclas, simula
//  chamadas e mensagens, confere o que a interface pediu e salva cada
//  tela como imagem em test_host/saida/telas/ (abre em qualquer
//  visualizador de imagens).
//
//  Da raiz do projeto (uma linha só):
//  g++ -std=c++17 -I lib/Texto -I lib/Tela -I lib/Teclado -I lib/Interface
//      test_host/interface_test.cpp lib/Tela/Quadro.cpp lib/Tela/Fontes.cpp
//      lib/Teclado/Teclado.cpp lib/Interface/Interface.cpp -o /tmp/ui && /tmp/ui
// ============================================================
#include <stdio.h>
#include <string>
#include <vector>
#include <sys/stat.h>
#include "Interface.h"

static int falhas = 0, verificacoes = 0;
#define CONFERE(cond)                                                       \
  do {                                                                      \
    verificacoes++;                                                         \
    if (!(cond)) { falhas++; printf("  FALHOU (linha %d): %s\n", __LINE__, #cond); } \
  } while (0)

// ---------- telefone de mentira ----------
struct TelefoneFalso : Telefone {
  std::vector<std::string> pedidos;
  bool ligarOk = true, ussdOk = true;
  uint8_t mapaSalvo[Teclado::TECLAS] = {};
  bool salvou = false, aprendendo = false;
  bool ligar(const std::string& n) override { pedidos.push_back("ligar " + n); return ligarOk; }
  bool atender() override { pedidos.push_back("atender"); return true; }
  bool encerrar() override { pedidos.push_back("encerrar"); return true; }
  void tom(char t) override { pedidos.push_back(std::string("tom ") + t); }
  bool ussd(const std::string& c) override { pedidos.push_back("ussd " + c); return ussdOk; }
  void cancelarUssd() override { pedidos.push_back("cancelarUssd"); }
  void volume(int v) override { pedidos.push_back("volume " + std::to_string(v)); }
  std::string imei() override { return "861234567890123"; }
  std::string erro() override { return "+CME ERROR: 30"; }
  void aprenderTeclado(bool s) override { aprendendo = s; pedidos.push_back(s ? "aprender sim" : "aprender nao"); }
  void salvarTeclado(const uint8_t m[Teclado::TECLAS]) override {
    for (int i = 0; i < Teclado::TECLAS; i++) mapaSalvo[i] = m[i];
    salvou = true;
  }
  std::vector<std::vector<int>> lidas;
  void mensagemLida(const std::vector<int>& p) override { lidas.push_back(p); }
  bool pediu(const std::string& p) const {
    for (const auto& x : pedidos) if (x == p) return true;
    return false;
  }
};

// ---------- imagem ----------
static std::vector<uint16_t> memoria(Quadro::LARGURA * Quadro::ALTURA);
static Quadro quadro;

static void salvar(const char* nome) {
  mkdir("test_host/saida", 0755);
  mkdir("test_host/saida/telas", 0755);
  std::string caminho = std::string("test_host/saida/telas/") + nome + ".bmp";
  FILE* f = fopen(caminho.c_str(), "wb");
  if (!f) { printf("não consegui salvar %s\n", caminho.c_str()); return; }
  const int w = Quadro::LARGURA, h = Quadro::ALTURA, linha = w * 3, tam = 54 + linha * h;
  uint8_t cab[54] = {'B', 'M'};
  auto u32 = [&](int pos, uint32_t v) { for (int i = 0; i < 4; i++) cab[pos + i] = (uint8_t)(v >> (8 * i)); };
  u32(2, tam); u32(10, 54); u32(14, 40); u32(18, w); u32(22, h);
  cab[26] = 1; cab[28] = 24; u32(34, linha * h);
  fwrite(cab, 1, 54, f);
  std::vector<uint8_t> l(linha);
  for (int y = h - 1; y >= 0; y--) {
    for (int x = 0; x < w; x++) {
      const uint16_t c = quadro.ler(x, y);
      l[x * 3 + 2] = (uint8_t)(((c >> 11) * 255 + 15) / 31);
      l[x * 3 + 1] = (uint8_t)((((c >> 5) & 63) * 255 + 31) / 63);
      l[x * 3 + 0] = (uint8_t)(((c & 31) * 255 + 15) / 31);
    }
    fwrite(l.data(), 1, linha, f);
  }
  fclose(f);
}

// ---------- relógio e teclas ----------
static uint32_t agora = 1000;
static Interface ui;
static TelefoneFalso tel;

static void passar(uint32_t ms) {          // o loop() da placa, em passos de 10 ms
  for (uint32_t t = 0; t < ms; t += 10) {
    agora += 10;
    ui.tique(agora);
    if (ui.precisaDesenhar(agora)) ui.desenhar(quadro, agora);
  }
}
static void evento(EventoTecla::Tipo tipo, char k, bool segurada = false) {
  EventoTecla ev;
  ev.tipo = tipo;
  ev.tecla = k;
  ev.segurada = segurada;
  ui.tecla(ev, agora);
  passar(20);
}
static void toque(char k) { evento(EventoTecla::Apertou, k); evento(EventoTecla::Soltou, k); }
static void segura(char k) {
  evento(EventoTecla::Apertou, k);
  passar(800);
  evento(EventoTecla::Segurou, k);
  evento(EventoTecla::Soltou, k, true);
}
static void digitar(const char* s) { for (; *s; s++) toque(*s); }
static void foto(const char* nome) { ui.desenhar(quadro, agora); salvar(nome); }

static Situacao situacaoBoa() {
  Situacao s;
  s.barras = 3; s.rede = "4G"; s.operadora = "TIM"; s.bateria = 87;
  s.horaValida = true; s.ano = 2026; s.mes = 10; s.dia = 21; s.hora = 18; s.minuto = 5;
  return s;
}

int main() {
  quadro.usar(memoria.data());
  static const char* const nomes[7] = {"IO40", "IO39", "IO38", "IO15", "IO16", "IO37", "IO21"};

  printf("0. tela de ligar\n");
  Interface::desenharLigando(quadro, "Ligando o modem…");
  salvar("00_ligando");

  ui.begin(tel, 5, nomes, agora);
  Situacao s = situacaoBoa();
  ui.situacao(s, agora);

  printf("1. tela inicial\n");
  passar(50);
  CONFERE(ui.tela() == Tela::Inicio);
  foto("01_inicio");

  printf("2. discador, ligar\n");
  digitar("11999998888");
  CONFERE(ui.tela() == Tela::Discador);
  CONFERE(ui.digitado() == "11999998888");
  foto("02_discador");
  toque(TECLA_VERMELHA);                       // apaga um
  CONFERE(ui.digitado() == "1199999888");
  toque('8');
  toque(TECLA_VERDE);
  CONFERE(ui.tela() == Tela::Chamando);
  CONFERE(tel.pediu("ligar 11999998888"));    // só depois de desenhar a tela "Chamando"
  s.chamada = 1; ui.situacao(s, agora);
  passar(400);
  foto("03_chamando");
  s.chamada = 2; ui.situacao(s, agora);
  ui.chamadaConectada("", agora);
  s.chamada = 4; ui.situacao(s, agora);
  CONFERE(ui.tela() == Tela::EmLigacao);
  passar(42000);
  toque('1');
  CONFERE(tel.pediu("tom 1"));
  segura('#');                                 // volume +
  CONFERE(ui.volume() == 6);
  CONFERE(tel.pediu("volume 6"));
  CONFERE(!tel.pediu("tom #"));                // segurado não manda tom
  toque('#');                                  // curto manda tom
  CONFERE(tel.pediu("tom #"));
  segura('#');
  foto("04_em_ligacao_volume");
  passar(2500);
  foto("05_em_ligacao");
  toque(TECLA_VERMELHA);
  CONFERE(tel.pediu("encerrar"));
  ui.chamadaEncerrada("você desligou, duração 0:46", agora);
  s.chamada = 0; ui.situacao(s, agora);
  CONFERE(ui.tela() == Tela::Encerrada);
  foto("06_encerrada");
  passar(3000);
  CONFERE(ui.tela() == Tela::Inicio);

  printf("3. chamada recebida e perdida\n");
  ui.chamadaRecebida("+5521988887777", agora);
  s.chamada = 3; ui.situacao(s, agora);
  CONFERE(ui.tela() == Tela::Recebendo);
  passar(300);
  foto("07_recebendo");
  passar(20000);                                // tocando não apaga a luz
  CONFERE(ui.brilho(agora) == 255);
  ui.chamadaPerdida("+5521988887777", agora);
  s.chamada = 0; ui.situacao(s, agora);
  CONFERE(ui.tela() == Tela::Perdida);
  foto("08_perdida");
  passar(7000);
  CONFERE(ui.tela() == Tela::Inicio);
  CONFERE(ui.chamadasPerdidas() == 1);

  printf("4. mensagem longa com acentos\n");
  const std::string sms =
      "Oi! Chegou o tijolo? Amanhã às 9h passo aí para vermos a ligação do teclado e da tela. "
      "Não esqueça a bateria 18650 carregada. Também levo o ferro de solda e os cabos. "
      "Se der tempo, montamos a caixa de madeira com a janelinha para a tela. Até amanhã! "
      "PS: o código do portão é 4471.";
  ui.mensagem("+5511977776666", "26/10/21,18:07:30-12", sms, {4, 5}, agora);
  CONFERE(ui.tela() == Tela::Mensagem);
  passar(50);
  foto("09_mensagem");
  toque('8'); toque('8');
  foto("10_mensagem_rolada");
  toque(TECLA_VERDE);                           // "Mais": desce uma página
  toque(TECLA_VERDE);                           // lida
  CONFERE(ui.mensagensNaoLidas() == 0);
  CONFERE(!tel.lidas.empty() && tel.lidas.back() == std::vector<int>({4, 5}));
  CONFERE(ui.tela() == Tela::Inicio);
  ui.mensagem("TIM", "26/10/21,18:09:00-12", "TIM: Sua recarga de R$ 20,00 foi efetuada com sucesso. 🎉", {6}, agora);
  toque(TECLA_VERMELHA);                         // fecha (lida)
  ui.mensagem("+5511977776666", "26/10/20,08:00:00-12", "Bom dia ☀️", {7}, agora);
  ui.aviso("Bateria fraca (10%). Ligue o carregador.", agora);
  foto("10b_aviso");
  toque(TECLA_VERMELHA);
  CONFERE(ui.tela() == Tela::Inicio);
  passar(4500);                                  // o aviso some sozinho
  ui.mensagem("+5511977776666", "26/10/21,18:10:00-12", "Outra", {8}, agora);
  toque(TECLA_VERMELHA);
  passar(10);
  foto("11_inicio_notificacoes");

  printf("5. USSD (saldo) com menu\n");
  digitar("*222#");
  CONFERE(ui.tela() == Tela::Discador);
  foto("12_discador_codigo");
  toque(TECLA_VERDE);
  CONFERE(ui.tela() == Tela::UssdEspera);
  CONFERE(tel.pediu("ussd *222#"));
  foto("13_ussd_espera");
  ui.ussd("Seu saldo é R$ 12,34, válido até 30/11/2026.\n1 - Recarga\n2 - Pacotes de internet\n3 - Falar com atendente", true, agora);
  CONFERE(ui.tela() == Tela::Ussd);
  toque('2');
  foto("14_ussd_menu");
  toque(TECLA_VERDE);
  CONFERE(tel.pediu("ussd 2"));
  ui.ussd("Pacote de 1 GB por R$ 9,90 ativado.", false, agora);
  foto("15_ussd_resposta");
  toque(TECLA_VERDE);
  CONFERE(ui.tela() == Tela::Inicio);

  printf("6. IMEI, +, rediscar, segurar o vermelho no discador\n");
  digitar("*#06#");
  CONFERE(ui.tela() == Tela::Imei);
  foto("16_imei");
  toque(TECLA_VERDE);
  evento(EventoTecla::Apertou, '0');
  passar(800);
  evento(EventoTecla::Segurou, '0');
  evento(EventoTecla::Soltou, '0', true);
  CONFERE(ui.digitado() == "+");
  digitar("4930123456");
  foto("17_discador_internacional");
  toque('7');
  evento(EventoTecla::Apertou, TECLA_VERMELHA);  // apaga o último...
  passar(800);
  evento(EventoTecla::Segurou, TECLA_VERMELHA);  // ...e segurar apaga tudo
  evento(EventoTecla::Soltou, TECLA_VERMELHA, true);
  CONFERE(ui.tela() == Tela::Inicio);
  CONFERE(ui.digitado().empty());
  toque('5');                                    // número de um dígito: vermelho segurado
  evento(EventoTecla::Apertou, TECLA_VERMELHA);  // apaga e volta ao início...
  passar(800);
  evento(EventoTecla::Segurou, TECLA_VERMELHA);  // ...mas NÃO pede para desligar
  evento(EventoTecla::Soltou, TECLA_VERMELHA, true);
  CONFERE(ui.tela() == Tela::Inicio);
  toque(TECLA_VERDE);                            // a chamada perdida da parte 3
  CONFERE(ui.tela() == Tela::Perdidas);
  foto("17b_perdidas");
  toque(TECLA_VERMELHA);                         // fecha e esquece
  CONFERE(ui.chamadasPerdidas() == 0);
  toque(TECLA_VERDE);                            // nada novo: rediscar
  CONFERE(ui.tela() == Tela::Discador);
  CONFERE(ui.digitado() == "11999998888");
  toque(TECLA_VERMELHA);                         // apaga um por um
  CONFERE(ui.digitado() == "1199999888");
  for (int i = 0; i < 9; i++) toque(TECLA_VERMELHA);
  CONFERE(ui.tela() == Tela::Discador);
  toque(TECLA_VERMELHA);                         // apagou o último: volta ao início
  CONFERE(ui.tela() == Tela::Inicio);

  printf("7. desligar\n");
  segura(TECLA_VERMELHA);
  CONFERE(ui.tela() == Tela::Desligar);
  foto("18_desligar");
  toque(TECLA_VERMELHA);
  CONFERE(ui.tela() == Tela::Inicio);
  CONFERE(!ui.pediuDesligar());
  segura(TECLA_VERMELHA);
  toque(TECLA_VERDE);
  CONFERE(ui.pediuDesligar());
  CONFERE(ui.tela() == Tela::Desligando);
  foto("19_desligando");

  printf("8. luz da tela\n");
  {
    Interface u2;
    TelefoneFalso t2;
    u2.begin(t2, 5, nomes, agora);
    u2.situacao(s, agora);
    CONFERE(u2.brilho(agora) == 255);
    CONFERE(u2.brilho(agora + 20000) < 255 && u2.brilho(agora + 20000) > 0);
    CONFERE(u2.brilho(agora + 31000) == 0);
    // tela apagada: a primeira tecla só acende
    EventoTecla ev; ev.tipo = EventoTecla::Apertou; ev.tecla = '5';
    u2.tecla(ev, agora + 40000);
    CONFERE(u2.tela() == Tela::Inicio);
    ev.tipo = EventoTecla::Soltou;
    u2.tecla(ev, agora + 40100);
    ev.tipo = EventoTecla::Apertou;
    u2.tecla(ev, agora + 40200);
    CONFERE(u2.tela() == Tela::Discador);
    // tela apagou no discador: volta ao início
    u2.tique(agora + 80000);
    CONFERE(u2.tela() == Tela::Inicio);
    // chamada recebida com a tela apagada: verde atende na hora
    u2.chamadaRecebida("190", agora + 90000);
    ev.tipo = EventoTecla::Apertou; ev.tecla = TECLA_VERDE;
    u2.tecla(ev, agora + 200000);
    CONFERE(t2.pediu("atender"));
  }

  printf("9. modem diz que a chamada acabou, sem aviso\n");
  {
    Interface u3;
    TelefoneFalso t3;
    u3.begin(t3, 5, nomes, agora);
    Situacao s3 = situacaoBoa();
    u3.situacao(s3, agora);
    u3.chamadaRecebida("11988887777", agora);
    u3.tique(agora + 100);
    u3.tique(agora + 3000);
    CONFERE(u3.tela() == Tela::Recebendo);
    u3.tique(agora + 4500);
    CONFERE(u3.tela() == Tela::Encerrada);
  }

  printf("10. falha ao ligar\n");
  {
    Interface u4;
    TelefoneFalso t4;
    t4.ligarOk = false;
    u4.begin(t4, 5, nomes, agora);
    u4.situacao(s, agora);
    EventoTecla ev; ev.tipo = EventoTecla::Apertou;
    for (char c : std::string("190")) { ev.tecla = c; u4.tecla(ev, agora); }
    ev.tecla = TECLA_VERDE; u4.tecla(ev, agora);
    u4.tique(agora);                        // ainda não desenhou: não liga
    CONFERE(!t4.pediu("ligar 190"));
    u4.desenhar(quadro, agora);
    u4.tique(agora + 10);
    CONFERE(t4.pediu("ligar 190"));
    CONFERE(u4.tela() == Tela::Encerrada);
    u4.desenhar(quadro, agora + 20);
    salvar("20_falha_ao_ligar");
  }

  printf("11. aprender o teclado\n");
  {
    Interface u5;
    TelefoneFalso t5;
    u5.begin(t5, 5, nomes, agora);
    u5.situacao(s, agora);
    u5.aprenderTeclado(agora);
    CONFERE(t5.aprendendo);
    // fios na ordem do teclado da Adafruit: C2 R1 C1 R4 C3 R3 R2
    const int linhaFio[4] = {1, 6, 5, 3}, colunaFio[3] = {2, 0, 4};
    auto par = [&](int i) {
      int a = linhaFio[i / 3], b = colunaFio[i % 3];
      if (a > b) { int t = a; a = b; b = t; }
      EventoTecla ev; ev.tipo = EventoTecla::Par; ev.a = (uint8_t)a; ev.b = (uint8_t)b;
      u5.tecla(ev, agora);
    };
    for (int i = 0; i < 5; i++) par(i);
    par(2);                                  // repetiu o "3"
    u5.desenhar(quadro, agora);
    salvar("21_teclado_aprendendo");
    for (int i = 5; i < 12; i++) par(i);
    CONFERE(t5.salvou);
    CONFERE(!t5.aprendendo);
    int lin[4], col[3];
    CONFERE(Teclado::validarGrade(t5.mapaSalvo, lin, col));
    CONFERE(lin[0] == 1 && lin[3] == 3 && col[0] == 2 && col[2] == 4);
    u5.desenhar(quadro, agora);
    salvar("22_teclado_pronto");
    u5.tique(agora + 3000);
    CONFERE(u5.tela() == Tela::Inicio);
  }

  printf("12. correções da revisão\n");
  {
    Interface u6;
    TelefoneFalso t6;
    u6.begin(t6, 5, nomes, agora);
    Situacao s6 = situacaoBoa();
    u6.situacao(s6, agora);
    uint32_t t = agora;
    auto ev6 = [&](EventoTecla::Tipo tipo, char k) {
      EventoTecla e; e.tipo = tipo; e.tecla = k; u6.tecla(e, t);
    };
    // "+" com o discador aberto a partir da tela de chamada encerrada
    u6.chamadaEncerrada("ocupado", t);
    ev6(EventoTecla::Apertou, '0');
    CONFERE(u6.tela() == Tela::Discador && u6.digitado() == "0");
    t += 800;
    ev6(EventoTecla::Segurou, '0');
    CONFERE(u6.digitado() == "+");
    ev6(EventoTecla::Soltou, '0');
    for (int i = 0; i < 2; i++) ev6(EventoTecla::Apertou, TECLA_VERMELHA), ev6(EventoTecla::Soltou, TECLA_VERMELHA);
    CONFERE(u6.tela() == Tela::Inicio);
    // muitas mensagens de uma vez: a aberta fica na tela, e as que saem da
    // memória NÃO contam como lidas (continuam protegidas no chip)
    u6.mensagem("111", "", "primeira", {1}, t);
    CONFERE(u6.tela() == Tela::Mensagem);
    for (int i = 2; i <= 25; i++) u6.mensagem(std::to_string(i), "", "m" + std::to_string(i), {i}, t);
    CONFERE(u6.mensagensNaoLidas() == 20);
    CONFERE(t6.lidas.empty());
    ev6(EventoTecla::Apertou, TECLA_VERMELHA);
    CONFERE(!t6.lidas.empty() && t6.lidas.back() == std::vector<int>({1}));   // fechou a aberta
    ev6(EventoTecla::Soltou, TECLA_VERMELHA);
    // chamada feita pelo Monitor Serial: a tela acompanha
    s6.chamada = 1; s6.numero = "+5511912345678";
    u6.situacao(s6, t);
    u6.tique(t);
    CONFERE(u6.tela() == Tela::Inicio);
    t += 1200;
    u6.tique(t);
    CONFERE(u6.tela() == Tela::Chamando);
    u6.desenhar(quadro, t);
    salvar("23_chamada_pelo_monitor");
    s6.chamada = 0; s6.numero.clear();
    u6.situacao(s6, t);
    u6.tique(t);                                     // começa a contar "modem livre"
    t += 4500;
    u6.tique(t);
    CONFERE(u6.tela() == Tela::Encerrada);
    // sem tela: "desenhado" deixa a ligação acontecer
    t += 3000;
    u6.tique(t);
    ev6(EventoTecla::Apertou, '1'); ev6(EventoTecla::Apertou, '9'); ev6(EventoTecla::Apertou, '0');
    ev6(EventoTecla::Apertou, TECLA_VERDE);
    u6.tique(t);
    CONFERE(!t6.pediu("ligar 190"));
    u6.desenhado(t);
    u6.tique(t + 10);
    CONFERE(t6.pediu("ligar 190"));
    // configurar o teclado não deixa a luz acesa para sempre
    Interface u7;
    TelefoneFalso t7;
    u7.begin(t7, 5, nomes, t);
    u7.aprenderTeclado(t);
    CONFERE(u7.brilho(t + 1000) == 255);
    CONFERE(u7.brilho(t + 31000) == 0);
    u7.tique(t + 31000);
    CONFERE(u7.tela() == Tela::Teclado);                 // apagada, mas continua esperando as teclas
    // Com a tela apagada, a primeira tecla só acende: não vira o passo "1".
    const int linhaFio[4] = {1, 6, 5, 3}, colunaFio[3] = {2, 0, 4};
    auto par7 = [&](int i, uint32_t quando) {
      int a = linhaFio[i / 3], b = colunaFio[i % 3];
      if (a > b) { int x = a; a = b; b = x; }
      EventoTecla e; e.tipo = EventoTecla::Par; e.a = (uint8_t)a; e.b = (uint8_t)b;
      u7.tecla(e, quando);
    };
    par7(4, t + 32000);                                  // um "5" qualquer, só para acender
    CONFERE(u7.brilho(t + 32000) == 255);
    for (int i = 0; i < 12; i++) par7(i, t + 33000 + i * 300);
    CONFERE(t7.salvou);
    int lin7[4], col7[3];
    CONFERE(Teclado::validarGrade(t7.mapaSalvo, lin7, col7));
  }

  printf("14. sem botões: segurar 1 = verde, segurar 3 = vermelho (Teclado de verdade)\n");
  {
    // Teclado de mentira: 7 fios, linhas nos fios 0 a 3 e colunas nos 4 a 6
    // (o mapa padrão). `apertada` é a tecla encostando os dois fios dela.
    static char apertada = 0;
    static bool baixo[16] = {};
    struct Pinos {
      static void entrada(int p) { baixo[p] = false; }
      static void puxar(int p) { baixo[p] = true; }
      static void esperar(uint32_t) {}
      static bool ler(int p) {
        if (!apertada) return true;
        int i = 0;
        while (Teclado::ORDEM[i] != apertada) i++;
        const int linha = i / 3, coluna = 4 + i % 3;
        if (p == linha) return !baixo[coluna];
        if (p == coluna) return !baixo[linha];
        return true;
      }
    };
    static const int fios[7] = {0, 1, 2, 3, 4, 5, 6};
    Teclado tec;
    tec.begin(fios, 7, -1, -1, AcessoPinos{Pinos::entrada, Pinos::puxar, Pinos::ler, Pinos::esperar});
    tec.teclasComoBotoes('1', '3');
    Interface u8;
    TelefoneFalso t8;
    uint32_t t = agora;
    u8.begin(t8, 5, nomes, t);
    u8.teclasComoBotoes('1', '3', "Para ligar de novo, segure a tecla 3.");
    u8.situacao(situacaoBoa(), t);
    auto rodar = [&](uint32_t ms) {
      for (uint32_t k = 0; k < ms; k++) {
        t++;
        tec.varrer(t);
        EventoTecla e;
        while (tec.proximoEvento(e)) u8.tecla(e, t);
        u8.tique(t);
        if (u8.precisaDesenhar(t)) u8.desenhar(quadro, t);
      }
    };
    auto tocar = [&](char k, uint32_t ms = 120) { apertada = k; rodar(ms); apertada = 0; rodar(60); };
    for (char k : std::string("1133")) tocar(k);          // toques rápidos digitam 1 e 3
    CONFERE(u8.tela() == Tela::Discador && u8.digitado() == "1133");
    u8.desenhar(quadro, t);
    salvar("24_discador_teclas_1_e_3");
    tocar('3', 1000);                                      // segurar o 3: apaga um
    CONFERE(u8.digitado() == "113");
    tocar('9');
    tocar('1', 1000);                                      // segurar o 1: liga
    CONFERE(u8.tela() == Tela::Chamando);
    u8.desenhar(quadro, t);
    u8.tique(t + 1);
    CONFERE(t8.pediu("ligar 1139"));
    salvar("25_chamando_tecla_3");
    tocar('3', 1000);                                      // segurar o 3: encerra
    CONFERE(t8.pediu("encerrar"));
    u8.chamadaEncerrada("você desligou", t);
    rodar(3000);
    CONFERE(u8.tela() == Tela::Inicio);
    u8.chamadaRecebida("+5511988887777", t);
    rodar(300);
    u8.desenhar(quadro, t);
    salvar("26_recebendo_teclas_1_e_3");
    tocar('1', 1000);                                      // segurar o 1: atende
    CONFERE(t8.pediu("atender"));
    u8.chamadaConectada("", t);
    tocar('1');                                            // na ligação, toque rápido é tom
    CONFERE(t8.pediu("tom 1"));
    rodar(400);
    u8.desenhar(quadro, t);
    salvar("27_em_ligacao_tecla_3");
    u8.chamadaEncerrada("", t);
    rodar(3000);
    tocar('3', 1000);                                      // 1 s na tela inicial: nada
    CONFERE(u8.tela() == Tela::Inicio);
    tocar('3', 1800);                                      // 1,6 s: pergunta se quer desligar
    CONFERE(u8.tela() == Tela::Desligar);
    u8.desenhar(quadro, t);
    salvar("28_desligar_tecla_3");
    tocar('3');                                            // toque rápido: cancela
    CONFERE(u8.tela() == Tela::Inicio && !u8.pediuDesligar());
    tocar('3', 1800);
    tocar('1', 1000);                                      // confirma com o 1 segurado
    CONFERE(u8.pediuDesligar());
    // ligou segurando o 3: nada acontece até soltar
    Interface u9;
    TelefoneFalso t9;
    u9.begin(t9, 5, nomes, t);
    u9.teclasComoBotoes('1', '3', "");
    Teclado tec9;
    tec9.begin(fios, 7, -1, -1, AcessoPinos{Pinos::entrada, Pinos::puxar, Pinos::ler, Pinos::esperar});
    tec9.teclasComoBotoes('1', '3');
    apertada = '3';
    tec9.ignorarAteSoltar();
    for (int k = 0; k < 3000; k++) {
      t++;
      tec9.varrer(t);
      EventoTecla e;
      while (tec9.proximoEvento(e)) u9.tecla(e, t);
    }
    CONFERE(u9.tela() == Tela::Inicio);
    apertada = 0;
    u8.desenhar(quadro, t);
  }

  printf("15. configurar o teclado sem botões\n");
  {
    Interface u10;
    TelefoneFalso t10;
    uint32_t t = agora;
    u10.begin(t10, 5, nomes, t);
    u10.teclasComoBotoes('1', '3', "");
    u10.situacao(situacaoBoa(), t);
    u10.aprenderTeclado(t);
    auto par = [&](int i, EventoTecla::Tipo tipo = EventoTecla::Par) {
      EventoTecla e; e.tipo = tipo; e.a = (uint8_t)(i / 3); e.b = (uint8_t)(4 + i % 3);
      u10.tecla(e, t += 300);
    };
    par(0); par(1);
    par(1, EventoTecla::ParSegurado);                // segurar o 2: não é botão, nada muda
    CONFERE(u10.tela() == Tela::Teclado);
    par(0); par(0, EventoTecla::ParSegurado);        // segurar o 1 (já aprendido): recomeça
    u10.desenhar(quadro, t);
    salvar("29_teclado_sem_botoes");
    for (int i = 0; i < 3; i++) par(i);
    par(2); par(2, EventoTecla::ParSegurado);        // segurar o 3: pula (fica o mapa antigo)
    CONFERE(u10.tela() == Tela::Inicio && !t10.salvou && !t10.aprendendo);
    // grade errada no fim: segurar o 3 = "usar assim"
    u10.aprenderTeclado(t);
    for (int i = 0; i < 12; i++) {
      EventoTecla e; e.tipo = EventoTecla::Par;
      const int j = i == 4 ? 5 : i == 5 ? 4 : i;      // 5 e 6 trocados
      e.a = (uint8_t)(j / 3); e.b = (uint8_t)(4 + j % 3);
      u10.tecla(e, t += 300);
    }
    CONFERE(u10.tela() == Tela::Teclado && !t10.salvou);
    u10.desenhar(quadro, t);
    salvar("30_teclado_nao_bateu_sem_botoes");
    par(2, EventoTecla::ParSegurado);
    CONFERE(t10.salvou && u10.tela() == Tela::Inicio);
  }

  printf("16. revisão: toque lento no aprendizado, tela que muda no meio do aperto, confirmar desligar\n");
  {
    Interface u11;
    TelefoneFalso t11;
    uint32_t t = agora;
    u11.begin(t11, 5, nomes, t);
    u11.teclasComoBotoes('1', '3', "");
    u11.situacao(situacaoBoa(), t);
    u11.aprenderTeclado(t);
    auto par = [&](int i, EventoTecla::Tipo tipo = EventoTecla::Par) {
      EventoTecla e; e.tipo = tipo; e.a = (uint8_t)(i / 3); e.b = (uint8_t)(4 + i % 3);
      u11.tecla(e, t += 300);
    };
    par(0); par(0, EventoTecla::ParSegurado);       // o 1, apertado devagar: ensina, não recomeça
    par(1);
    par(2); par(2, EventoTecla::ParSegurado);       // o 3, devagar: ensina, não pula
    CONFERE(u11.tela() == Tela::Teclado);
    for (int i = 3; i < 12; i++) par(i);
    CONFERE(t11.salvou);
    int lin[4], col[3];
    CONFERE(Teclado::validarGrade(t11.mapaSalvo, lin, col));

    // Segurando o 1 (para rediscar) quando chega uma chamada: não atende
    Interface u12;
    TelefoneFalso t12;
    u12.begin(t12, 5, nomes, t);
    u12.teclasComoBotoes('1', '3', "");
    u12.situacao(situacaoBoa(), t);
    auto ev12 = [&](EventoTecla::Tipo tipo, char k, char origem, uint32_t quando) {
      EventoTecla e; e.tipo = tipo; e.tecla = k; e.origem = origem; e.quando = quando;
      u12.tecla(e, t);
    };
    const uint32_t desceu = t;
    t += 500;
    u12.chamadaRecebida("+5511977776666", t);
    t += 300;
    ev12(EventoTecla::Apertou, TECLA_VERDE, '1', desceu);
    t += 800;
    ev12(EventoTecla::Segurou, TECLA_VERDE, '1', desceu);
    ev12(EventoTecla::Soltou, TECLA_VERDE, '1', desceu);
    CONFERE(!t12.pediu("atender") && u12.tela() == Tela::Recebendo);
    t += 100;
    ev12(EventoTecla::Apertou, TECLA_VERDE, '1', t - 800);   // um aperto novo, já com a chamada na tela
    CONFERE(t12.pediu("atender"));
    // Segurando o 3 para desistir e a pessoa atende bem nessa hora: encerra
    u12.chamadaConectada("", t);
    u12.chamadaEncerrada("", t);
    t += 3000;
    u12.tique(t);
    Interface u13;
    TelefoneFalso t13;
    u13.begin(t13, 5, nomes, t);
    u13.teclasComoBotoes('1', '3', "");
    u13.situacao(situacaoBoa(), t);
    {
      EventoTecla e; e.tipo = EventoTecla::Apertou;
      for (char c : std::string("190")) { e.tecla = c; e.quando = t; u13.tecla(e, t); }
      t += 1300;                                   // o 1 desceu 0,5 s depois e foi segurado
      e.tecla = TECLA_VERDE; e.origem = '1'; e.quando = t - 800; u13.tecla(e, t);
      u13.desenhar(quadro, t);
      u13.tique(t + 1);
      CONFERE(t13.pediu("ligar 190") && u13.tela() == Tela::Chamando);
      const uint32_t desceu3 = t + 100;
      t += 600;
      u13.chamadaConectada("", t);                 // atendeu enquanto o 3 descia
      CONFERE(u13.tela() == Tela::EmLigacao);
      t += 300;
      e.tecla = TECLA_VERMELHA; e.origem = '3'; e.quando = desceu3; u13.tecla(e, t);
      CONFERE(t13.pediu("encerrar"));
    }
    // Desligar: um toque curto no 1 (não chegou a segurar) não cancela
    Interface u14;
    TelefoneFalso t14;
    u14.begin(t14, 5, nomes, t);
    u14.teclasComoBotoes('1', '3', "");
    u14.situacao(situacaoBoa(), t);
    {
      EventoTecla e; e.tipo = EventoTecla::Apertou; e.tecla = TECLA_VERMELHA; e.origem = '3'; e.quando = t;
      u14.tecla(e, t + 800);
      e.tipo = EventoTecla::Segurou; u14.tecla(e, t + 1600);
      e.tipo = EventoTecla::Soltou; u14.tecla(e, t + 1700);
      CONFERE(u14.tela() == Tela::Desligar);
      t += 2000;
      e = EventoTecla(); e.tipo = EventoTecla::Apertou; e.tecla = '1'; e.origem = '1'; e.quando = t;
      u14.tecla(e, t + 150);
      e.tipo = EventoTecla::Soltou; u14.tecla(e, t + 150);
      CONFERE(u14.tela() == Tela::Desligar && !u14.pediuDesligar());
      e = EventoTecla(); e.tipo = EventoTecla::Apertou; e.tecla = TECLA_VERDE; e.origem = '1'; e.quando = t + 300;
      u14.tecla(e, t + 1100);
      CONFERE(u14.pediuDesligar());
    }
  }

  printf("13. textos\n");
  CONFERE(Interface::formatarNumero("+5511999998888") == "(11) 99999-8888");
  CONFERE(Interface::formatarNumero("1133334444") == "(11) 3333-4444");
  CONFERE(Interface::formatarNumero("999998888") == "99999-8888");
  CONFERE(Interface::formatarNumero("08001234567") == "0800 123 4567");
  CONFERE(Interface::formatarNumero("+14155550100") == "+14155550100");
  CONFERE(Interface::formatarNumero("TIM") == "TIM");
  CONFERE(Interface::formatarNumero("*222#") == "*222#");
  CONFERE(Interface::formatarNumero("190") == "190");
  CONFERE(Interface::ehCodigoUssd("*222#"));
  CONFERE(Interface::ehCodigoUssd("#100#"));
  CONFERE(!Interface::ehCodigoUssd("*144"));
  CONFERE(!Interface::ehCodigoUssd("#"));
  {
    const auto l = Quadro::quebrar("um dois três\n\nquatro", fontes::fonteTexto, 400);
    CONFERE(l.size() == 3 && l[0] == "um dois três" && l[1].empty() && l[2] == "quatro");
    const auto l2 = Quadro::quebrar("https://exemplo.com.br/um/endereco/bem/comprido/demais", fontes::fonteTexto, 100);
    CONFERE(l2.size() > 2);
    bool cabem = true;
    for (const auto& x : l2) cabem = cabem && Quadro::largura(x, fontes::fonteTexto) <= 100;
    CONFERE(cabem);
    const std::string c = Quadro::caber("Mensagem comprida demais para caber", fontes::fonteTexto, 120);
    CONFERE(Quadro::largura(c, fontes::fonteTexto) <= 120);
    CONFERE(c.size() >= 3 && c.compare(c.size() - 3, 3, "\xE2\x80\xA6") == 0);
    CONFERE(Quadro::largura("", fontes::fonteTexto) == 0);
    CONFERE(Quadro::largura("ã", fontes::fonteTexto) > 0);
    CONFERE(Quadro::largura("\xF0\x9F\x8E\x89", fontes::fonteTexto) > 0);   // emoji vira □
    CONFERE(Quadro::largura("\xEF\xB8\x8F", fontes::fonteTexto) == 0);      // seletor de variação: some
  }

  printf("\n%d verificações, %d falha(s)\n", verificacoes, falhas);
  return falhas ? 1 : 0;
}
