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
//      -I lib/Caixa -I lib/Escrita -I lib/Pdu test_host/interface_test.cpp
//      lib/Tela/Quadro.cpp lib/Tela/Fontes.cpp lib/Teclado/Teclado.cpp
//      lib/Caixa/Caixa.cpp lib/Escrita/Multitoque.cpp lib/Interface/Interface.cpp
//      -o /tmp/ui && /tmp/ui
// ============================================================
#include <stdio.h>
#include <map>
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

static uint32_t agora = 1000;              // o relógio dos testes

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
  bool smsOk = true;
  uint32_t atrasoSms = 0;                  // quanto o "modem" demora para enviar (o loop fica parado)
  bool enviarSms(const std::string& n, const std::string& t) override {
    pedidos.push_back("sms " + n + " " + t);
    agora += atrasoSms;
    return smsOk;
  }
  // Como o main: 10 ou 11 dígitos sem o 0 na frente ganham o +55
  std::string numeroCompleto(const std::string& n) override {
    bool digitos = !n.empty();
    for (char c : n) digitos = digitos && c >= '0' && c <= '9';
    if (digitos && (n.size() == 10 || n.size() == 11) && n[0] != '0') return "+55" + n;
    return n;
  }
  bool pediu(const std::string& p) const {
    for (const auto& x : pedidos) if (x == p) return true;
    return false;
  }
  int quantos(const std::string& comeco) const {
    int n = 0;
    for (const auto& x : pedidos) n += x.compare(0, comeco.size(), comeco) == 0;
    return n;
  }
};

// ---------- pasta de mentira para a caixa de mensagens ----------
struct PastaFalsa : Pasta {
  std::map<std::string, std::string> arquivos;
  bool falharEscrita = false;
  bool listar(std::vector<std::string>& nomes) override {
    for (auto& a : arquivos) nomes.push_back(a.first);
    return true;
  }
  bool ler(const std::string& n, std::string& c) override {
    auto it = arquivos.find(n);
    if (it == arquivos.end()) return false;
    c = it->second;
    return true;
  }
  bool escrever(const std::string& n, const std::string& c) override {
    if (falharEscrita) return false;
    arquivos[n] = c;
    return true;
  }
  bool renomear(const std::string& de, const std::string& para) override {
    auto it = arquivos.find(de);
    if (it == arquivos.end()) return false;
    arquivos[para] = it->second;
    arquivos.erase(it);
    return true;
  }
  bool apagar(const std::string& n) override { return arquivos.erase(n) > 0; }
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
static Interface ui;
static TelefoneFalso tel;
static PastaFalsa pasta;
static Caixa caixa;

static Interface* u = &ui;                 // a interface que as teclas e o relógio movem

static void passar(uint32_t ms) {          // o loop() da placa, em passos de 10 ms
  for (uint32_t t = 0; t < ms; t += 10) {
    agora += 10;
    u->tique(agora);
    if (u->precisaDesenhar(agora)) u->desenhar(quadro, agora);
  }
}
static void evento(EventoTecla::Tipo tipo, char k, bool segurada = false) {
  EventoTecla ev;
  ev.tipo = tipo;
  ev.tecla = k;
  ev.segurada = segurada;
  u->tecla(ev, agora);
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
// Escrevendo: "_" = espera 1,2 s (a letra pendente fica)
static void escrever(const char* s) {
  for (; *s; s++) {
    if (*s == '_') passar(1200);
    else toque(*s);
  }
}
static void foto(const char* nome) { u->desenhar(quadro, agora); salvar(nome); }

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

  caixa.begin(pasta);
  ui.begin(tel, 5, nomes, agora);
  ui.usarCaixa(caixa);
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

  printf("4. mensagens recebidas: abrem sozinhas e ficam na caixa\n");
  const std::string sms =
      "Oi! Chegou o tijolo? Amanhã às 9h passo aí para vermos a ligação do teclado e da tela. "
      "Não esqueça a bateria 18650 carregada. Também levo o ferro de solda e os cabos. "
      "Se der tempo, montamos a caixa de madeira com a janelinha para a tela. Até amanhã! "
      "PS: o código do portão é 4471.";
  const uint32_t longa = ui.mensagem("+5511977776666", "26/10/21,18:07:30-12", sms, agora);
  CONFERE(longa != 0 && ui.tela() == Tela::Ler && ui.lendo() == longa);
  CONFERE(ui.mensagensNaoLidas() == 1);          // abriu sozinha: só conta como lida quando alguém mexer
  passar(50);
  foto("09_mensagem");
  toque('8');                                    // uma página para baixo (agora está lida)
  CONFERE(ui.mensagensNaoLidas() == 0);
  foto("10_mensagem_rolada");
  toque(TECLA_VERMELHA);
  CONFERE(ui.tela() == Tela::Inicio);
  // Abriu sozinha e ninguém mexeu: a tela apaga, volta ao início e ela continua nova
  const uint32_t tim = ui.mensagem("TIM", "26/10/21,18:09:00-12", "TIM: Sua recarga de R$ 20,00 foi efetuada com sucesso. 🎉", agora);
  CONFERE(ui.tela() == Tela::Ler);
  passar(31000);
  CONFERE(ui.tela() == Tela::Inicio && ui.mensagensNaoLidas() == 1);
  toque('5');                                    // tela apagada: só acende
  CONFERE(ui.tela() == Tela::Inicio);
  ui.mensagem("+5511977776666", "26/10/20,08:00:00-12", "Bom dia ☀️", agora);
  CONFERE(ui.tela() == Tela::Ler);
  toque(TECLA_VERMELHA);                         // vista e fechada
  ui.mensagem("+5511977776666", "26/10/21,18:10:00-12", "Outra", agora);
  CONFERE(ui.tela() == Tela::Ler);
  const uint32_t maisUma = ui.mensagem("+5521955554444", "26/10/21,18:11:00-12", "Mais uma, e esta é de outra pessoa.", agora);
  CONFERE(ui.tela() == Tela::Ler && ui.lendo() != maisUma);   // chegou com outra aberta: só o aviso
  ui.aviso("Bateria fraca (10%). Ligue o carregador.", agora);
  foto("10b_aviso");
  toque(TECLA_VERMELHA);
  CONFERE(ui.tela() == Tela::Inicio);
  CONFERE(ui.mensagensNaoLidas() == 2);          // a da TIM e a "mais uma"
  passar(4500);                                  // o aviso some sozinho
  foto("11_inicio_notificacoes");
  toque(TECLA_VERDE);                            // "Ver" com duas novas: a lista
  CONFERE(ui.tela() == Tela::Mensagens && ui.selecionada() == maisUma);
  foto("31_lista");
  toque('5');                                    // abre (e fica lida)
  CONFERE(ui.tela() == Tela::Ler && ui.lendo() == maisUma && ui.mensagensNaoLidas() == 1);
  toque(TECLA_VERMELHA);                         // volta para a lista
  CONFERE(ui.tela() == Tela::Mensagens && ui.selecionada() == maisUma);
  toque('8'); toque('8'); toque('8');
  CONFERE(ui.selecionada() == tim);
  toque('5');
  CONFERE(ui.tela() == Tela::Ler && ui.mensagensNaoLidas() == 0);
  foto("32_ler_tim");
  toque(TECLA_VERDE);                            // "TIM" não é número: o verde abre as opções
  CONFERE(ui.tela() == Tela::Opcoes);
  foto("33_opcoes_tim");
  toque(TECLA_VERMELHA);
  CONFERE(ui.tela() == Tela::Ler);
  toque(TECLA_VERMELHA);
  toque(TECLA_VERMELHA);                         // lista → início (veio de lá)
  CONFERE(ui.tela() == Tela::Inicio);

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
  toque(TECLA_VERDE);                            // nada novo: o menu
  CONFERE(ui.tela() == Tela::Menu);
  toque('8'); toque('8');                        // Escrever, Mensagens, Rediscar
  toque('5');
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
    // sem caixa (a memória não montou): a mensagem fica no chip, só o aviso
    CONFERE(u6.mensagem("111", "", "primeira", t) == 0);
    CONFERE(u6.tela() == Tela::Inicio && u6.mensagensNaoLidas() == 0);
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

  // ---------- Fase 3: mensagens ----------
  PastaFalsa p17;
  Caixa c17;
  c17.begin(p17);
  Interface us;
  TelefoneFalso ts;
  us.begin(ts, 5, nomes, agora);
  us.usarCaixa(c17);
  us.teclasComoBotoes('1', '3', "");
  us.situacao(situacaoBoa(), agora);
  u = &us;
  passar(50);

  printf("17. escrever e enviar um SMS (multitoque)\n");
  {
    toque(TECLA_VERDE);                                   // nada novo: o menu
    CONFERE(us.tela() == Tela::Menu);
    foto("34_menu");
    toque('5');                                           // "Escrever mensagem"
    CONFERE(us.tela() == Tela::Escrever && us.escrita().vazio());
    foto("35_escrever_vazio");
    escrever("666" "444" "1111" "0" "222" "44" "33" "4" "666" "0" "22222" "7777" "0");   // "Oi! Chego às "
    segura('8');                                          // segurado: o número
    escrever("441");                                      // "h."
    passar(1100);
    CONFERE(us.escrita().texto() == "Oi! Chego às 8h.");
    foto("36_escrever");
    toque('#');                                           // # apaga a última letra
    CONFERE(us.escrita().texto() == "Oi! Chego às 8h");
    segura('#');                                          // segurado: a palavra
    CONFERE(us.escrita().texto() == "Oi! Chego às ");
    segura('8');
    escrever("441");
    toque('*');                                           // * troca o modo
    CONFERE(std::string(us.escrita().nomeModo()) == "abc");
    toque('*'); toque('*'); toque('*');                   // ABC, 123, Abc
    CONFERE(std::string(us.escrita().nomeModo()) == "Abc");
    passar(1100);
    CONFERE(us.escrita().texto() == "Oi! Chego às 8h.");
    toque(TECLA_VERDE);                                   // para quem
    CONFERE(us.tela() == Tela::Para && us.destinatario().empty());
    foto("37_para_vazio");
    toque(TECLA_VERDE);                                   // sem número: não envia
    CONFERE(us.tela() == Tela::Para && ts.quantos("sms ") == 0);
    digitar("119888877770");
    toque('#');                                           // apaga o 0 a mais
    CONFERE(us.destinatario() == "11988887777");
    foto("38_para");
    EventoTecla e;
    e.tipo = EventoTecla::Apertou;
    e.tecla = TECLA_VERDE;
    us.tecla(e, agora);
    CONFERE(us.tela() == Tela::Enviando);
    us.tique(agora);                                      // ainda não desenhou: não envia
    CONFERE(ts.quantos("sms ") == 0);
    foto("39_enviando");
    us.tique(agora + 10);
    CONFERE(ts.pediu("sms +5511988887777 Oi! Chego às 8h."));
    CONFERE(us.tela() == Tela::Enviado);
    e.tipo = EventoTecla::Soltou;
    us.tecla(e, agora + 20);
    passar(30);
    foto("40_enviada");
    CONFERE(us.escrita().vazio() && us.destinatario().empty());   // o rascunho já foi
    CONFERE(c17.total() == 1 && c17.itens()[0].estado == Caixa::Enviada);
    Caixa::Mensagem m;
    CONFERE(c17.ler(c17.itens()[0].id, m) && m.numero == "+5511988887777" && m.texto == "Oi! Chego às 8h." &&
            m.dataHora == "26/10/21,18:05:00");
    passar(3000);
    CONFERE(us.tela() == Tela::Inicio);
  }

  printf("18. envio que falha: fica guardado para tentar de novo\n");
  {
    ts.smsOk = false;
    toque(TECLA_VERDE);
    toque('5');
    escrever("8337777833");                               // "Teste"
    passar(1100);
    CONFERE(us.escrita().texto() == "Teste");
    toque(TECLA_VERDE);
    digitar("190");
    toque(TECLA_VERDE);
    CONFERE(us.tela() == Tela::Enviado && ts.pediu("sms 190 Teste"));
    foto("41_nao_enviada");
    CONFERE(c17.total() == 2 && c17.itens()[0].estado == Caixa::NaoEnviada);
    CONFERE(us.escrita().vazio());                        // está guardada na caixa
    passar(5000);
    CONFERE(us.tela() == Tela::Enviado);                  // a falha espera alguém ver
    ts.smsOk = true;
    toque(TECLA_VERDE);                                   // tentar de novo
    CONFERE(ts.quantos("sms 190 Teste") == 2 && us.tela() == Tela::Enviado);
    CONFERE(c17.total() == 2 && c17.itens()[0].estado == Caixa::Enviada);   // trocou de lugar
    passar(3000);
    CONFERE(us.tela() == Tela::Inicio);
    // Falhou e foi fechada: depois, pela caixa, "Editar e enviar"
    ts.smsOk = false;
    toque(TECLA_VERDE);
    toque('5');
    escrever("666_444");                                  // "Oi"
    toque(TECLA_VERDE);
    digitar("190");
    toque(TECLA_VERDE);
    toque(TECLA_VERMELHA);                                // "Fechar"
    CONFERE(us.tela() == Tela::Inicio && c17.total() == 3);
    const uint32_t falhou = c17.itens()[0].id;
    toque(TECLA_VERDE);                                   // menu
    toque('8');                                           // Mensagens
    toque('5');
    CONFERE(us.tela() == Tela::Mensagens && us.selecionada() == falhou);
    foto("42_lista_com_falha");
    toque('5');
    CONFERE(us.tela() == Tela::Ler);
    foto("43_ler_nao_enviada");
    toque('5');                                           // opções: Tentar de novo, Editar e enviar...
    toque('8');
    toque('5');
    CONFERE(us.tela() == Tela::Escrever && us.escrita().texto() == "Oi" && us.destinatario() == "190");
    escrever("1");                                        // "Oi."
    ts.smsOk = true;
    toque(TECLA_VERDE);
    toque(TECLA_VERDE);
    CONFERE(ts.pediu("sms 190 Oi."));
    CONFERE(c17.total() == 3 && c17.posicao(falhou) < 0);   // a versão nova tomou o lugar
    passar(3000);
  }

  printf("19. caixa: ler, responder, apagar\n");
  {
    CONFERE(us.tela() == Tela::Inicio);
    const uint32_t recebida = us.mensagem("+5521955554444", "26/10/21,17:00:00-12", "Vem jantar aqui hoje?", agora);
    CONFERE(us.tela() == Tela::Ler && us.lendo() == recebida);
    foto("44_ler");
    toque(TECLA_VERDE);                                   // responder
    CONFERE(us.tela() == Tela::Escrever && us.destinatario() == "+5521955554444");
    CONFERE(us.mensagensNaoLidas() == 0);                 // tocou: está lida
    escrever("777744461111");                             // "Sim!"
    passar(1100);
    CONFERE(us.escrita().texto() == "Sim!");
    foto("45_responder");
    toque(TECLA_VERDE);                                   // o número já está lá
    CONFERE(us.tela() == Tela::Para && us.destinatario() == "+5521955554444");
    toque(TECLA_VERDE);
    CONFERE(ts.pediu("sms +5521955554444 Sim!"));
    passar(3000);
    toque(TECLA_VERDE);                                   // menu → Mensagens
    toque('8');
    toque('5');
    CONFERE(us.tela() == Tela::Mensagens && c17.total() == 5);
    foto("46_lista");
    toque('8');                                           // a recebida
    CONFERE(us.selecionada() == recebida);
    toque('5');
    toque('5');                                           // opções
    CONFERE(us.tela() == Tela::Opcoes);
    foto("47_opcoes");
    toque('8'); toque('8'); toque('8');                   // Responder, Ligar, Encaminhar, Apagar
    toque('5');
    CONFERE(us.tela() == Tela::Apagar);
    foto("48_apagar");
    toque(TECLA_VERMELHA);                                // cancelar
    CONFERE(us.tela() == Tela::Ler && c17.total() == 5);
    toque('5');
    toque('8'); toque('8'); toque('8');
    toque('5');
    const int posicao = c17.posicao(recebida);
    const uint32_t vizinha = c17.itens()[posicao + 1].id;
    toque(TECLA_VERDE);                                   // apagar de verdade
    CONFERE(c17.total() == 4 && c17.posicao(recebida) < 0);
    CONFERE(us.tela() == Tela::Mensagens && us.selecionada() == vizinha);
    // Apagada pelo Monitor Serial enquanto estava aberta: a tela volta para a lista
    toque('5');
    CONFERE(us.tela() == Tela::Ler);
    c17.apagar(us.lendo());
    us.caixaMudou(agora);
    CONFERE(us.tela() == Tela::Mensagens && c17.total() == 3);
    // "Ligar" pelas opções
    toque('2');
    toque('5');
    toque('5');
    toque('2');                                           // já está na primeira
    toque('8');                                           // Ligar (a enviada: Ligar, Encaminhar, Apagar)
    toque('2');
    toque('5');
    CONFERE(us.tela() == Tela::Chamando);
    us.desenhar(quadro, agora);
    us.tique(agora + 1);
    CONFERE(ts.quantos("ligar ") == 1);
    us.chamadaEncerrada("", agora);
    passar(3000);
  }

  printf("20. rascunho: sair (o texto vai para a caixa) e continuar depois\n");
  {
    CONFERE(us.tela() == Tela::Inicio);
    const size_t antes = c17.total();
    toque(TECLA_VERDE);
    toque('5');                                           // Escrever
    escrever("44_444");                                   // "Hi"
    passar(1100);
    toque(TECLA_VERMELHA);                                // sai: vira rascunho na caixa
    CONFERE(us.tela() == Tela::Menu && us.escrita().vazio());
    CONFERE(c17.total() == antes + 1 && c17.itens()[0].estado == Caixa::Rascunho);
    const uint32_t rasc = c17.itens()[0].id;
    foto("49_menu_rascunho");
    toque('8');                                           // Escrever, Continuar rascunho, ...
    toque('5');
    CONFERE(us.tela() == Tela::Escrever && us.escrita().texto() == "Hi");
    escrever("0222");                                     // "Hi c"
    passar(1100);
    toque(TECLA_VERMELHA);                                // guarda de novo: a versão nova toma o lugar
    CONFERE(c17.total() == antes + 1 && c17.posicao(rasc) < 0 && c17.itens()[0].estado == Caixa::Rascunho);
    Caixa::Mensagem mr;
    CONFERE(c17.ler(c17.itens()[0].id, mr) && mr.texto == "Hi c" && mr.numero.empty());
    toque('2');
    toque('5');                                           // "Escrever mensagem" começa do zero
    CONFERE(us.tela() == Tela::Escrever && us.escrita().vazio());
    toque(TECLA_VERMELHA);                                // vazio: nada a guardar
    CONFERE(c17.total() == antes + 1);
    // Pela lista: abrir o rascunho, continuar, apagar o texto todo e sair: ele sai da caixa
    toque('8'); toque('8');                               // Mensagens
    toque('5');
    CONFERE(us.tela() == Tela::Mensagens && us.selecionada() == c17.itens()[0].id);
    foto("50_lista_com_rascunho");
    toque('5');
    CONFERE(us.tela() == Tela::Ler);
    foto("51_ler_rascunho");
    toque(TECLA_VERDE);                                   // "Continuar"
    CONFERE(us.tela() == Tela::Escrever && us.escrita().texto() == "Hi c");
    segura('#');
    segura('#');                                          // apaga as duas palavras
    CONFERE(us.escrita().vazio());
    toque(TECLA_VERMELHA);
    CONFERE(us.tela() == Tela::Mensagens && c17.total() == antes);
    toque(TECLA_VERMELHA);                                // lista → menu
    toque('2'); toque('2');
    toque('5');                                           // Escrever
    escrever("2");
    passar(31000);                                        // a tela apaga, mas continua escrevendo
    CONFERE(us.tela() == Tela::Escrever && us.escrita().texto() == "A");
    toque('3');                                           // só acende
    CONFERE(us.escrita().texto() == "A");
    toque('3');
    CONFERE(us.escrita().texto() == "Ad");
  }

  printf("21. tamanho máximo\n");
  {
    us.maximoDePartes(1);
    for (int i = 0; i < 79; i++) { toque('2'); toque('3'); }
    passar(1100);
    CONFERE(us.escrita().texto().size() == 160);
    toque('2');                                           // a 161ª não cabe
    CONFERE(us.escrita().texto().size() == 160 && us.escrita().recusou());
    foto("52_tamanho_maximo");
    toque('#'); toque('#');
    toque('2'); toque('2'); toque('2'); toque('2');       // a b c, e o á (só 70 com acento) é pulado: à,
    CONFERE(us.escrita().texto().size() == 160 &&                // que é do alfabeto GSM (160)
            us.escrita().texto().compare(158, 2, "\xc3\xa0") == 0 && !us.escrita().recusou());
    us.maximoDePartes(Interface::MAX_PARTES_PADRAO);
    toque('#');
    toque('2'); toque('2'); toque('2'); toque('2');       // agora cabe: a b c á
    passar(1100);
    CONFERE(us.escrita().texto().size() == 160 && us.escrita().texto().back() == '\xa1');
    foto("53_escrever_longo");
  }

  printf("22. limpar as novidades da tela inicial\n");
  {
    toque(TECLA_VERMELHA);                                // sai do editor (vira rascunho)
    us.mensagem("+5511900001111", "", "Me liga quando puder", agora);
    CONFERE(us.tela() == Tela::Menu && us.mensagensNaoLidas() == 1);   // fora do início: só o aviso
    toque(TECLA_VERMELHA);
    us.chamadaPerdida("+5511900001111", agora);
    passar(7000);
    CONFERE(us.tela() == Tela::Inicio && us.chamadasPerdidas() == 1);
    foto("54_inicio_novidades");
    segura(TECLA_VERMELHA);                               // segurar: desligar (não limpa)
    CONFERE(us.tela() == Tela::Desligar);
    toque(TECLA_VERMELHA);
    CONFERE(us.tela() == Tela::Inicio && us.chamadasPerdidas() == 1);
    evento(EventoTecla::Apertou, TECLA_VERMELHA);
    evento(EventoTecla::Soltou, TECLA_VERMELHA);          // "Limpar"
    CONFERE(us.chamadasPerdidas() == 0 && us.mensagensNaoLidas() == 1);   // continua nova na lista
    toque(TECLA_VERDE);                                   // sem novidades na tela: o menu
    CONFERE(us.tela() == Tela::Menu);
    foto("55_menu_com_nova");
    toque(TECLA_VERMELHA);
    us.mensagem("+5511900001111", "", "Oi?", agora);      // uma nova traz o cartão de volta
    toque(TECLA_VERMELHA);
    CONFERE(us.tela() == Tela::Inicio && us.mensagensNaoLidas() == 1);
    toque(TECLA_VERDE);                                   // uma só: abre direto
    CONFERE(us.tela() == Tela::Ler && us.mensagensNaoLidas() == 0);
  }

  printf("23. revisão da Fase 3\n");
  {
    toque(TECLA_VERMELHA);
    CONFERE(us.tela() == Tela::Inicio);
    // Envio demorado (5 s): a tela "Mensagem enviada" conta a partir do fim do envio
    us.usarRelogio([]() -> uint32_t { return agora; });
    ts.atrasoSms = 5000;
    toque(TECLA_VERDE);
    toque('5');
    escrever("666");                                      // "O"
    toque(TECLA_VERDE);
    digitar("190");
    toque(TECLA_VERDE);
    CONFERE(us.tela() == Tela::Enviado && ts.pediu("sms 190 O"));
    passar(1500);
    CONFERE(us.tela() == Tela::Enviado);
    passar(1500);
    CONFERE(us.tela() == Tela::Inicio);
    // Falha que demora 40 s: a tela da falha aparece, e acesa
    ts.smsOk = false;
    ts.atrasoSms = 40000;
    toque(TECLA_VERDE);
    toque('5');
    escrever("666");
    toque(TECLA_VERDE);
    digitar("190");
    toque(TECLA_VERDE);
    CONFERE(us.tela() == Tela::Enviado && us.brilho(agora) > 0);
    passar(200);
    CONFERE(us.tela() == Tela::Enviado);
    ts.atrasoSms = 0;
    ts.smsOk = true;
    toque(TECLA_VERMELHA);                                // fechar (ela ficou guardada)
    CONFERE(us.tela() == Tela::Inicio);

    // Chamada chegando antes de o SMS sair: ele não sai no meio da chamada e fica guardado
    const size_t total0 = c17.total();
    const int smsAntes = ts.quantos("sms ");
    toque(TECLA_VERDE);
    toque('5');
    escrever("666");
    toque(TECLA_VERDE);
    digitar("190");
    EventoTecla e;
    e.tipo = EventoTecla::Apertou;
    e.tecla = TECLA_VERDE;
    us.tecla(e, agora);                                   // "Enviando…", ainda não desenhada
    CONFERE(us.tela() == Tela::Enviando);
    us.chamadaRecebida("+5511977776666", agora);
    passar(50);
    CONFERE(us.tela() == Tela::Recebendo && ts.quantos("sms ") == smsAntes);
    CONFERE(c17.total() == total0 + 1 && c17.itens()[0].estado == Caixa::NaoEnviada && us.escrita().vazio());
    const uint32_t naoFoi = c17.itens()[0].id;
    us.chamadaPerdida("+5511977776666", agora);
    passar(7000);
    CONFERE(us.tela() == Tela::Inicio && us.ultimoAviso().find("chegou uma chamada") != std::string::npos);
    evento(EventoTecla::Apertou, TECLA_VERMELHA);
    evento(EventoTecla::Soltou, TECLA_VERMELHA);          // "Limpar" a chamada perdida
    CONFERE(us.chamadasPerdidas() == 0);

    // Falhou e nem deu para guardar: o texto fica; tentando de novo e dando certo, ele sai
    ts.smsOk = false;
    p17.falharEscrita = true;
    toque(TECLA_VERDE);
    toque('5');
    escrever("666");
    toque(TECLA_VERDE);
    digitar("190");
    toque(TECLA_VERDE);
    CONFERE(us.tela() == Tela::Enviado && us.escrita().texto() == "O");
    p17.falharEscrita = false;
    ts.smsOk = true;
    toque(TECLA_VERDE);                                   // tentar de novo
    CONFERE(us.tela() == Tela::Enviado && us.escrita().vazio());
    passar(3000);

    // Reenviar uma "não enviada" com a memória falhando: a de antes não some
    toque(TECLA_VERDE);                                   // menu: Escrever, Rascunho, Mensagens...
    toque('8'); toque('8');
    toque('5');
    CONFERE(us.tela() == Tela::Mensagens);
    for (int i = 0; i < c17.posicao(naoFoi); i++) toque('8');
    CONFERE(us.selecionada() == naoFoi);
    toque('5');
    p17.falharEscrita = true;
    ts.smsOk = false;
    toque(TECLA_VERDE);                                   // "Reenviar": falha, e não grava
    CONFERE(us.tela() == Tela::Enviado && c17.posicao(naoFoi) >= 0);
    p17.falharEscrita = false;
    ts.smsOk = true;
    toque(TECLA_VERDE);                                   // de novo: foi, e ela vira "enviada"
    CONFERE(c17.posicao(naoFoi) < 0 && c17.itens()[0].estado == Caixa::Enviada);
    passar(3000);

    // Encaminhar uma mensagem maior que o máximo (7 partes): não envia
    const std::string longa(1000, 'x');
    const uint32_t grande = us.mensagem("+5511911112222", "", longa, agora);
    CONFERE(us.tela() == Tela::Ler && us.lendo() == grande);
    toque('5');                                           // Responder, Ligar, Encaminhar, Apagar
    toque('8'); toque('8');
    toque('5');
    CONFERE(us.tela() == Tela::Escrever && us.escrita().texto() == longa);
    toque(TECLA_VERDE);
    CONFERE(us.tela() == Tela::Escrever && us.ultimoAviso().find("7 partes") != std::string::npos);
    segura('#');                                          // apaga a "palavra" inteira
    toque(TECLA_VERMELHA);                                // vazio: volta para a mensagem
    CONFERE(us.tela() == Tela::Ler);
    us.usarRelogio(nullptr);
  }
  u = &ui;

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
    // Quebra do editor: nenhum caractere se perde (nem os espaços repetidos)
    const std::string t = "Oi tudo bem  com   você? Este é um teste de quebra de linha bem comprido.";
    const auto tr = Quadro::quebrarTudo(t, fontes::fonteTexto, 120);
    std::string junto;
    bool cabe = true;
    for (const auto& x : tr) {
      const std::string l = t.substr(x.inicio, x.fim - x.inicio);
      junto += l;
      std::string semEspacoNoFim = l;
      while (!semEspacoNoFim.empty() && semEspacoNoFim.back() == ' ') semEspacoNoFim.pop_back();
      cabe = cabe && Quadro::largura(semEspacoNoFim, fontes::fonteTexto) <= 120;
    }
    CONFERE(junto == t && tr.size() > 3 && cabe);
    const std::string t2 = "a\n\nb";
    const auto tr2 = Quadro::quebrarTudo(t2, fontes::fonteTexto, 200);
    CONFERE(tr2.size() == 3 && tr2[0].fim == 1 && tr2[1].inicio == tr2[1].fim && tr2[2].inicio == 3);
    const std::string t3 = "httpsexemplocombrumenderecobemcompridodemais";
    const auto tr3 = Quadro::quebrarTudo(t3, fontes::fonteTexto, 100);
    std::string junto3;
    for (const auto& x : tr3) junto3 += t3.substr(x.inicio, x.fim - x.inicio);
    CONFERE(tr3.size() > 2 && junto3 == t3);
    CONFERE(Quadro::quebrarTudo("", fontes::fonteTexto, 100).size() == 1);
    CONFERE(Interface::explicarErro("+CMS ERROR: 330") == "o chip não tem o número da central de mensagens");
    CONFERE(Interface::explicarErro("+CMS ERROR: 3300") == "a rede recusou (código 3300)");
    CONFERE(Interface::explicarErro("+CME ERROR: 30") == "sem rede");
    CONFERE(Interface::explicarErro("+CME ERROR: SIM not inserted") == "sem chip");
    CONFERE(Interface::explicarErro("sem resposta (tempo esgotado)") == "sem resposta (tempo esgotado)");
    CONFERE(Interface::ehNumero("+5511999998888") && Interface::ehNumero("190") && !Interface::ehNumero("TIM") &&
            !Interface::ehNumero("12") && !Interface::ehNumero("+") && !Interface::ehNumero("*222#"));
  }

  printf("\n%d verificações, %d falha(s)\n", verificacoes, falhas);
  return falhas ? 1 : 0;
}
