// ============================================================
//  Tijolo — Fase 3: escrever SMS e a caixa de mensagens
//
//  O telefone funciona sozinho: tela inicial com relógio, menu,
//  discador, chamadas, SMS (escrever pelo teclado, enviar, ler, responder),
//  saldo (USSD) e liga/desliga. Sem botões: segurar o 1 vale o verde e
//  segurar o 3 vale o vermelho (config.h). O Monitor Serial continua
//  aceitando os comandos (digite "ajuda").
//
//  Quem faz o quê:
//   - lib/Modem: conversa com o modem (chamadas, SMS, USSD)
//   - lib/Teclado: lê o teclado 3x4 (e os botões, se houver)
//   - lib/Interface: decide a tela e o que cada tecla faz
//   - lib/Escrita: o multitoque (2 = a b c...)
//   - lib/Caixa: as mensagens guardadas na memória flash (LittleFS)
//   - lib/Tela: desenha (Quadro) e manda para a tela (TelaIli9341)
//   - este arquivo: liga tudo, lê o estado do modem de tempos em
//     tempos e cuida de desligar/ligar o aparelho.
// ============================================================
#include <Arduino.h>
#include <Preferences.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#include <esp_sleep.h>
#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include "config.h"
#include <Caixa.h>
#include <PastaLittleFS.h>
#include <Interface.h>
#include <Medidor.h>
#include <Modem.h>
#include <Pdu.h>
#include <Sentinela.h>
#include <Quadro.h>
#include <Teclado.h>
#include <TelaIli9341.h>
#include <Texto.h>
#include <Toque.h>

Modem modem;
pdu::Montador montador;                  // junta as partes de SMS longos que chegam
constexpr uint32_t ESPERA_PARTES_MS = 180000;   // depois de 3 min mostra o que chegou

TelaIli9341 tela;
Quadro quadro;
bool temTela = false;
Teclado teclado;
Interface ui;
Preferences memoria;                     // guarda o mapa do teclado e o volume
bool modemOk = false;
bool mostrarTeclas = false;              // comando "teclado teste"
uint32_t ultimaTecla = 0;

// Diagnóstico da tela branca (comandos "tela diag", "tensao", "radio"...)
Medidor medidor;                         // 3V3 e bateria, milhares de vezes por segundo
bool radioLigado = true;                 // "radio desligar" é só para teste
bool altoFalanteLigado = true;           // "amplificador desligar" também
uint32_t modemLigadoEm = 0;
uint32_t monitorAte = 0, proximaLinhaMonitor = 0;   // comando "tensao"
SentinelaPsram sentinela;                // avisa se a PSRAM perder os dados
uint32_t refazerTelaEm = 0;              // RESET da tela agendado (0 = nenhum)
const char* motivoRefazer = "";
uint32_t telasRefeitas = 0;
bool refazerNaTroca = TELA_REFAZER_NA_TROCA_DE_REDE;   // "tela rede desligar" para testes
                                         // (no teste, a tela também fica sempre acesa)
uint8_t brilhoMaximo = BRILHO_MAXIMO;    // comando "brilho"
uint32_t redeEm = 0;                     // perguntar a rede (AT+CPSI?) depois de uma troca
int redeTentativas = 0;
String ultimaRede;                       // a última resposta do AT+CPSI? depois de uma troca
bool escuroAteRefazer = false;           // troca de rede: luz apagada até o RESET agendado
uint32_t escuroDesde = 0, ultimaTrocaEm = 0;
bool quadroNaPsram = false;              // onde mora a imagem da tela (comando "memoria")

// ---------- Linha de comando com eco e Backspace ----------
String linhaDigitada;
bool ultimoFoiCR = false;

void mostrarPrompt() {
  Serial.print("\ntijolo> ");
  Serial.print(linhaDigitada);
}

// Imprime uma mensagem "por cima" do que está sendo digitado
void avisar(const String& msg) {
  Serial.print("\r\n");
  Serial.println(msg);
  mostrarPrompt();
}

// ---------- Números de telefone ----------
// "(11) 99999-8888" → "+5511999998888" · "*222#" fica como está
String normalizarNumero(String n) {
  String limpo;
  for (unsigned i = 0; i < n.length(); i++) {
    char c = n[i];
    if (c == ' ' || c == '-' || c == '(' || c == ')' || c == '.') continue;
    limpo += c;
  }
  bool soDigitos = limpo.length() > 0;
  for (unsigned i = 0; i < limpo.length(); i++) if (!isdigit((unsigned char)limpo[i])) soDigitos = false;
  if (COMPLETAR_MAIS55 && soDigitos && (limpo.length() == 10 || limpo.length() == 11) && limpo[0] != '0')
    limpo = "+55" + limpo;
  return limpo;
}

int barrasDoSinal(int csq) {
  if (csq == 99 || csq < 0) return -1;
  return csq >= 20 ? 4 : csq >= 15 ? 3 : csq >= 10 ? 2 : csq >= 5 ? 1 : 0;
}

String barrasDeSinal(int csq) {
  if (csq == 99 || csq < 0) return "sem leitura";
  int dbm = -113 + 2 * csq;
  int barras = barrasDoSinal(csq);
  String b;
  for (int i = 0; i < 4; i++) b += i < barras ? "#" : ".";
  return "[" + b + "] " + String(dbm) + " dBm (CSQ " + String(csq) + ")";
}

// Bateria pelo divisor 1:2 da placa (a leitura do modem é a da fonte
// interna dele, não a da bateria).
int milivoltsBateria() {
  uint32_t soma = 0;
  Medidor::travarAdc();                  // o Medidor também usa o ADC, no outro núcleo
  for (int i = 0; i < 16; i++) soma += analogReadMilliVolts(PINO_BATERIA);
  Medidor::soltarAdc();
  return (int)(soma / 16) * 2;
}

int porcentagemBateria(int mv) {
  // Descarga típica de uma 18650 em repouso (mV, %)
  static const int tabela[][2] = {{3450, 0}, {3680, 10}, {3740, 20}, {3770, 30}, {3790, 40}, {3820, 50},
                                  {3870, 60}, {3920, 70}, {3980, 80}, {4060, 90}, {4150, 100}};
  if (mv < 2500) return -1;                      // sem bateria ou leitura estranha: não sei
  if (mv <= tabela[0][0]) return 0;
  for (int i = 1; i < 11; i++)
    if (mv <= tabela[i][0])
      return tabela[i - 1][1] + (mv - tabela[i - 1][0]) * (tabela[i][1] - tabela[i - 1][1]) /
                                    (tabela[i][0] - tabela[i - 1][0]);
  return 100;
}

// ============================================================
//  O que a interface sabe do aparelho
// ============================================================
// Datas: dias desde 1/1/1970 e de volta (algoritmos de Howard Hinnant)
int32_t diasDesde1970(int a, int m, int d) {
  a -= m <= 2;
  const int era = (a >= 0 ? a : a - 399) / 400;
  const int ano = a - era * 400;
  const int dia = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int diaDaEra = ano * 365 + ano / 4 - ano / 100 + dia;
  return era * 146097 + diaDaEra - 719468;
}

void dataDosDias(int32_t z, int& a, int& m, int& d) {
  z += 719468;
  const int era = (z >= 0 ? z : z - 146096) / 146097;
  const int diaDaEra = z - era * 146097;
  const int ano = (diaDaEra - diaDaEra / 1460 + diaDaEra / 36524 - diaDaEra / 146096) / 365;
  const int diaDoAno = diaDaEra - (365 * ano + ano / 4 - ano / 100);
  const int mp = (5 * diaDoAno + 2) / 153;
  d = diaDoAno - (153 * mp + 2) / 5 + 1;
  m = mp + (mp < 10 ? 3 : -9);
  a = ano + era * 400 + (m <= 2);
}

struct {
  int csq = -1;
  String rede, operadora, registro;
  String chip = "";
  int bateria = -1;
  bool horaOk = false;
  int64_t segundosBase = 0;               // hora local da rede, em segundos desde 1970
  uint32_t millisBase = 0;
} cache;

// Consultas ao modem: uma por vez, espaçadas, e nenhuma enquanto alguém
// digita (cada consulta trava o loop por alguns milissegundos).
struct Consulta { uint32_t intervalo; uint32_t ultima; bool feita; };
enum { C_SINAL, C_REDE, C_OPERADORA, C_BATERIA, C_HORA, N_CONSULTAS };
Consulta consultas[N_CONSULTAS] = {
    {10000, 0, false}, {30000, 0, false}, {60000, 0, false}, {20000, 0, false}, {60000, 0, false}};
uint32_t ultimaConsulta = 0;

String nomeCurto(String op) {
  op.trim();
  for (const char* fim : {" BRASIL", " Brasil", " BR"})
    if (op.endsWith(fim)) op = op.substring(0, op.length() - strlen(fim));
  return op;
}

void lerHora() {
  String h = modem.horaDaRede();         // "26/10/21,18:05:12-12"
  int aa, mm, dd, hh, mi, ss;
  // Antes de a rede mandar a hora, o modem diz uma data padrão (ano 70,
  // 80...): só vale um ano entre 2024 e 2069.
  if (sscanf(h.c_str(), "%d/%d/%d,%d:%d:%d", &aa, &mm, &dd, &hh, &mi, &ss) == 6 && aa >= 24 && aa < 70 &&
      mm >= 1 && mm <= 12 && dd >= 1 && dd <= 31) {
    cache.segundosBase = (int64_t)diasDesde1970(2000 + aa, mm, dd) * 86400 + hh * 3600 + mi * 60 + ss;
    cache.millisBase = millis();
    cache.horaOk = true;
  }
}

void consultar(int c) {
  switch (c) {
    case C_SINAL: cache.csq = modem.sinal(); break;
    case C_REDE: {
      String r = modem.tipoDeRede();
      if (modem.tempoEsgotado()) break;
      cache.rede = r == "LTE" ? "4G" : (r == "GSM" || r == "EDGE" || r == "GPRS") ? "2G" : r == "WCDMA" ? "3G" : "";
      cache.registro = modem.registro();
      break;
    }
    case C_OPERADORA: {
      String op = modem.operadora();
      if (modem.tempoEsgotado()) break;
      cache.operadora = nomeCurto(op);
      String chip = modem.estadoChip();
      if (chip == "pronto") cache.chip = "";
      else if (chip == "sem chip") cache.chip = "Sem chip";
      else if (chip.startsWith("pede PIN")) cache.chip = "Chip pede PIN";
      else if (chip.startsWith("BLOQUEADO")) cache.chip = "Chip bloqueado (PUK)";
      else if (!chip.startsWith("erro")) cache.chip = chip;   // erro passageiro: fica o que era
      break;
    }
    case C_BATERIA: cache.bateria = porcentagemBateria(milivoltsBateria()); break;
    case C_HORA: lerHora(); break;
  }
}

uint32_t consultasPausadasAte = 0;     // o modem não respondeu: deixa ele em paz um pouco

void atualizarSituacao(uint32_t agora) {
  if (agora - ultimaTecla < 2000 || agora - ultimaConsulta < 1200) return;
  // Durante uma chamada (e tocando) não pergunta nada: o modem está
  // ocupado e a bateria cai com o rádio transmitindo.
  if (modem.estado() != EstadoChamada::Livre) return;
  const bool modemDisponivel = modemOk && !modem.esperandoUssd() &&
                               (!consultasPausadasAte || (int32_t)(agora - consultasPausadasAte) >= 0);
  // a consulta mais atrasada
  int escolhida = -1;
  uint32_t maiorAtraso = 0;
  for (int i = 0; i < N_CONSULTAS; i++) {
    if (i != C_BATERIA && !modemDisponivel) continue;      // a bateria é lida pela ESP32
    const uint32_t passou = agora - consultas[i].ultima;
    if (consultas[i].feita && passou < consultas[i].intervalo) continue;
    const uint32_t atraso = consultas[i].feita ? passou - consultas[i].intervalo : UINT32_MAX;
    if (escolhida < 0 || atraso > maiorAtraso) { escolhida = i; maiorAtraso = atraso; }
  }
  if (escolhida < 0) return;
  consultar(escolhida);
  consultas[escolhida].ultima = millis();
  consultas[escolhida].feita = true;
  ultimaConsulta = millis();
  if (escolhida != C_BATERIA) consultasPausadasAte = modem.tempoEsgotado() ? (millis() + 30000) | 1 : 0;
}

Situacao situacaoAtual() {
  Situacao s;
  const bool registrado = cache.registro.startsWith("registrado") || cache.registro.startsWith("roaming");
  s.barras = (cache.csq >= 0 && cache.csq != 99 && (registrado || cache.registro.length() == 0))
                 ? barrasDoSinal(cache.csq) : -1;
  s.rede = cache.rede.c_str();
  s.operadora = cache.operadora.c_str();
  s.chip = cache.chip.c_str();
  s.bateria = cache.bateria;
  if (cache.horaOk) {
    const int64_t t = cache.segundosBase + (millis() - cache.millisBase) / 1000;
    int a, m, d;
    dataDosDias((int32_t)(t / 86400), a, m, d);
    s.horaValida = true;
    s.ano = a; s.mes = m; s.dia = d;
    s.hora = (int)(t % 86400 / 3600);
    s.minuto = (int)(t % 3600 / 60);
  }
  switch (modem.estado()) {
    case EstadoChamada::Livre: s.chamada = 0; break;
    case EstadoChamada::Discando: s.chamada = 1; break;
    case EstadoChamada::Chamando: s.chamada = 2; break;
    case EstadoChamada::Tocando: s.chamada = 3; break;
    case EstadoChamada::EmLigacao: s.chamada = 4; break;
  }
  s.numero = modem.numeroDaChamada().c_str();
  return s;
}

// ============================================================
//  SMS: a caixa de mensagens fica na memória do Tijolo
// ============================================================
// Cada mensagem é um arquivo na partição de dados da flash (LittleFS, ver
// lib/Caixa). Se o sistema de arquivos não montar, a caixa fica na RAM:
// funciona, mas some ao desligar, e aí as mensagens também ficam no chip.
struct PastaNaRam : Pasta {
  std::map<std::string, std::string> arquivos;
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
  bool escrever(const std::string& n, const std::string& c) override { arquivos[n] = c; return true; }
  bool renomear(const std::string& de, const std::string& para) override {
    auto it = arquivos.find(de);
    if (it == arquivos.end()) return false;
    arquivos[para] = it->second;
    arquivos.erase(it);
    return true;
  }
  bool apagar(const std::string& n) override { return arquivos.erase(n) > 0; }
};

PastaLittleFS pastaFlash;
PastaNaRam pastaRam;
Caixa caixa;
bool caixaNaFlash = false;               // false: a caixa está na RAM (a flash não montou)

// Depois de guardada na caixa, a mensagem sai do chip (que guarda poucas e,
// cheio, para de receber). Só se a caixa estiver na flash.
bool podeApagarDoChip() { return APAGAR_SMS_DO_CHIP && caixaNaFlash; }

void apagarDoChip(const std::vector<int>& posicoes) {
  for (int i : posicoes)
    if (!modem.apagarSms(i))
      avisar("Não consegui tirar do chip o SMS da posição " + String(i) + " (" + modem.ultimoErro() + ").");
}

// ============================================================
//  O que a interface pede ao aparelho
// ============================================================
class TelefoneReal : public Telefone {
public:
  bool ligar(const std::string& n) override {
    const String num = normalizarNumero(String(n.c_str()));
    avisar("Ligando para " + num + "...");
    return modem.ligar(num);
  }
  bool atender() override { return modem.atender(); }
  bool encerrar() override { return modem.desligar(); }
  void tom(char t) override { modem.tons(String(t)); }
  bool ussd(const std::string& c) override { return modem.ussd(String(c.c_str())); }
  void cancelarUssd() override { modem.cancelarUssd(); }
  void volume(int v) override {
    modem.volume(v);
    memoria.putUChar("volume", (uint8_t)v);
  }
  std::string imei() override { return modem.imei().c_str(); }
  std::string erro() override { return modem.ultimoErro().c_str(); }
  void aprenderTeclado(bool sim) override { teclado.aprender(sim); }
  void salvarTeclado(const uint8_t m[Teclado::TECLAS]) override {
    teclado.definirMapa(m);
    memoria.putBytes("teclado", m, Teclado::TECLAS);
    avisar("Mapa do teclado guardado.");
  }
  bool enviarSms(const std::string& n, const std::string& t) override {
    const String num = normalizarNumero(String(n.c_str()));
    avisar("Enviando SMS para " + num + "...");
    int partes = 0;
    const bool ok = modem.enviarSms(num, String(t.c_str()), &partes);
    if (ok) avisar("SMS enviado" + (partes > 1 ? " em " + String(partes) + " partes" : String("")) + ".");
    else avisar("O SMS não foi: " + modem.ultimoErro());
    return ok;
  }
  std::string numeroCompleto(const std::string& n) override { return normalizarNumero(String(n.c_str())).c_str(); }
} telefone;

// Acesso aos pinos para o Teclado
void pinoEntrada(int p) { pinMode(p, INPUT_PULLUP); }
void pinoBaixo(int p) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
bool pinoLer(int p) { return digitalRead(p) == HIGH; }
void esperarUs(uint32_t us) { delayMicroseconds(us); }

// ============================================================
//  Desligar (sono profundo) e ligar segurando a tecla 3 (ou o botão vermelho)
// ============================================================
// Sem o botão vermelho, quem acorda a ESP32 é a tecla que faz as vezes
// dele (o 3): um dos dois fios dela fica preso em LOW durante o sono e o
// outro, que tem de ser um pino RTC (GPIO 0 a 21, os únicos que acordam a
// ESP32 do sono profundo), espera descer. Ficam na memória RTC, que
// sobrevive ao sono.
RTC_DATA_ATTR int8_t pinoAcordar = -1;   // entrada: desce quando a tecla é apertada
RTC_DATA_ATTR int8_t pinoPreso = -1;     // o outro fio da tecla, preso em LOW

// Pinos que ficam presos durante o sono e o nível de cada um
struct PinoPreso { int pino; int nivel; };
const PinoPreso PRESOS[] = {
  {TELA_LUZ, LOW},            // luz da tela apagada
  {TELA_CS, HIGH},            // tela não selecionada (ruído não a acorda)
  {PINO_FONTE_FORTE, LOW},    // fonte da placa em modo economia
  {PINO_MODEM_PWRKEY, LOW},   // ninguém "aperta" o botão do modem
};

void prenderPinos() {
  for (const auto& p : PRESOS) {
    if (p.pino < 0) continue;
    pinMode(p.pino, OUTPUT);
    digitalWrite(p.pino, p.nivel);
    gpio_hold_en((gpio_num_t)p.pino);
  }
  if (BOTAO_VERMELHO < 0 && pinoPreso >= 0) {   // um fio da tecla de ligar em LOW
    pinMode(pinoPreso, OUTPUT);
    digitalWrite(pinoPreso, LOW);
    gpio_hold_en((gpio_num_t)pinoPreso);
  }
  gpio_deep_sleep_hold_en();
}

// Põe cada pino no estado de trabalho e só então solta (sem piscar nada).
void soltarPinos() {
  for (const auto& p : PRESOS) {
    if (p.pino < 0) continue;
    pinMode(p.pino, OUTPUT);
    digitalWrite(p.pino, p.pino == PINO_FONTE_FORTE ? HIGH : p.nivel);
  }
  gpio_deep_sleep_hold_dis();
  for (const auto& p : PRESOS)
    if (p.pino >= 0) gpio_hold_dis((gpio_num_t)p.pino);
  // Os fios do teclado: um deles pode ter ficado preso em LOW para a tecla
  // de ligar. Depois de um reinício (sono leve, RST, queda de tensão) a
  // memória RTC volta ao começo e não diz qual: solta todos.
  for (int p : TECLADO_FIOS) gpio_hold_dis((gpio_num_t)p);
}

// Sem botão vermelho: os fios da tecla que faz as vezes dele. Devolve
// false se nenhum dos dois é pino RTC (aí o Tijolo dorme o sono leve).
bool escolherFiosDeLigar() {
  pinoAcordar = pinoPreso = -1;
  int a, b;
  if (!teclado.fiosDaTecla(TECLA_VERMELHA, a, b)) return false;
  const bool rtcA = rtc_gpio_is_valid_gpio((gpio_num_t)a), rtcB = rtc_gpio_is_valid_gpio((gpio_num_t)b);
  pinoAcordar = rtcB || !rtcA ? b : a;
  pinoPreso = pinoAcordar == b ? a : b;
  return rtcA || rtcB;
}

// Texto para quem quer ligar o Tijolo de novo
String comoLigar() {
  if (BOTAO_VERMELHO >= 0) return "segure o botão vermelho";
  if (TECLA_COMO_VERMELHO) return String("segure a tecla ") + TECLA_COMO_VERMELHO;
  return "use a chave da placa ou o botão RST";
}

[[noreturn]] void entrarNoSono() {
  // O vermelho (ou a tecla 3) acorda a ESP32. Se ele ainda estiver
  // apertado (segurado demais, emperrado), acorda quando SOLTAR; aí a ESP32
  // vê que foi só um toque e volta a dormir, agora esperando o próximo aperto.
  const int pino = BOTAO_VERMELHO >= 0 ? BOTAO_VERMELHO : pinoAcordar;
  if (pino >= 0) {
    pinMode(pino, INPUT_PULLUP);
    delay(50);                            // contato parar de tremer
    const bool solto = digitalRead(pino) == HIGH;
    esp_sleep_enable_ext0_wakeup((gpio_num_t)pino, solto ? 0 : 1);
    rtc_gpio_pullup_en((gpio_num_t)pino);
    rtc_gpio_pulldown_dis((gpio_num_t)pino);
  }                                       // sem pino: só a chave ou o RST ligam de novo
  // Os pinos 26 a 37, 47 e 48 são alimentados pelo VDD_SPI (o da memória
  // flash), que desliga no sono profundo: a luz da tela (IO36), o CS dela
  // (IO47) e um fio do teclado no IO37 ficariam soltos. Com ele ligado o
  // sono gasta um pouco mais (a flash e a PSRAM em espera).
  esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_ON);
  Serial.flush();
  esp_deep_sleep_start();
  while (true) {}
}

// A tecla de ligar sem pino RTC: sono leve. Gasta mais (perto de 1 mA, em
// vez de alguns µA), mas qualquer pino acorda a ESP32. Segurada por 1 s,
// a placa reinicia e o Tijolo liga como sempre.
[[noreturn]] void dormirLeve() {
  pinMode(pinoAcordar, INPUT_PULLUP);
  gpio_wakeup_enable((gpio_num_t)pinoAcordar, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  while (true) {
    while (digitalRead(pinoAcordar) == LOW) delay(10);   // espera soltar
    Serial.flush();
    esp_light_sleep_start();
    const uint32_t t0 = millis();
    while (digitalRead(pinoAcordar) == LOW) {
      if (millis() - t0 >= SEGURAR_PARA_LIGAR_MS) esp_restart();
      delay(10);
    }
  }
}

[[noreturn]] void dormir() {
  if (temTela && TELA_LUZ >= 0) ledcDetach(TELA_LUZ);
  const bool profundo = BOTAO_VERMELHO >= 0 || escolherFiosDeLigar();
  if (BOTAO_VERMELHO < 0 && pinoAcordar < 0)
    Serial.println("Nenhuma tecla liga o Tijolo (TECLA_COMO_VERMELHO = 0): ligue pela chave ou pelo RST.");
  else if (!profundo)
    Serial.printf("Os fios da tecla %c (IO%d e IO%d) não acordam a ESP32 do sono profundo (só IO15, IO16 e IO21):\n"
                  "dormindo o sono leve (~1 mA). Trocando dois fios do teclado de lugar, ele volta ao profundo.\n",
                  TECLA_COMO_VERMELHO, (int)pinoAcordar, (int)pinoPreso);
  for (uint32_t t = millis(); teclado.segurando(TECLA_VERMELHA) && millis() - t < 10000;)
    delay(10);                            // espera soltar (senão acorda na hora)
  prenderPinos();
  if (!profundo && pinoAcordar >= 0) dormirLeve();
  entrarNoSono();
}

[[noreturn]] void desligarAparelho() {
  Serial.println("\nDesligando o Tijolo. Para ligar: " + comoLigar() + " por 1 s.");
  if (temTela) {
    ui.desenhar(quadro, millis());
    tela.enviar(quadro);
  }
  if (!modem.desligarModem()) Serial.println("O modem não desligou: ele continua ligado (desligue pela chave).");
  if (temTela) tela.dormir();
  dormir();
}

// Acordou pelo vermelho (ou pela tecla 3): só liga se continuar apertado.
// Roda antes de soltar os pinos presos: um toque no bolso não mexe em nada.
void conferirTeclaDeLigar() {
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) return;
  const int pino = BOTAO_VERMELHO >= 0 ? BOTAO_VERMELHO : pinoAcordar;
  if (pino < 0) return;
  rtc_gpio_deinit((gpio_num_t)pino);
  pinMode(pino, INPUT_PULLUP);
  if (BOTAO_VERMELHO < 0 && pinoPreso >= 0) {   // o outro fio da tecla em LOW (se já não estiver preso)
    pinMode(pinoPreso, OUTPUT);
    digitalWrite(pinoPreso, LOW);
  }
  while (millis() < SEGURAR_PARA_LIGAR_MS) {
    if (digitalRead(pino) == HIGH) {
      prenderPinos();                     // os pinos continuam presos como no último sono
      entrarNoSono();                     // foi só um toque: volta a dormir
    }
    delay(10);
  }
}

// ============================================================
//  Comandos do Monitor Serial
// ============================================================
void ajuda() {
  Serial.println(F(
    "\nComandos (digite e aperte Enter):\n"
    "  status                 sinal, operadora, rede, chip, bateria e hora\n"
    "  ligar <numero>         ex.: ligar 11999998888  (vira +5511999998888)\n"
    "  atender | desligar     (desligar também recusa uma chamada tocando)\n"
    "  tons <digitos>         teclas durante a ligação, p/ menus: tons 1\n"
    "  sms <numero> <texto>   ex.: sms 11999998888 Olá! Chegou?  (número sem espaços)\n"
    "  lista [tudo]           mensagens na memória do Tijolo (recebidas e enviadas)\n"
    "  ler <n> | apagar <n> | apagar tudo   (n: o número entre [ ] na lista)\n"
    "  chip                   SMS que estão no chip (normalmente nenhum: vão para a memória)\n"
    "  chip importar | chip ler <posição> | chip apagar <posição>|tudo\n"
    "  caixa formatar         apaga a memória das mensagens (só se ela não montar)\n"
    "  ussd <codigo>          saldo e recarga: ussd *222#  (menu: ussd 1)\n"
    "  volume <0-7> | mic <0-7> | vivavoz sim|nao\n"
    "  toque instalar         grava o trrrim no modem (uma vez só)\n"
    "  toque testar           toca o trrrim agora\n"
    "  pin <senha>            se o chip pedir PIN\n"
    "  teclado aprender       a tela pede as 12 teclas, uma por uma\n"
    "  teclado teste          mostra aqui cada tecla (e botão) (de novo: desliga)\n"
    "  tela teste             desenha cores e letras para conferir a tela\n"
    "  tela reiniciar         reconfigura a tela (se ela ficar toda branca)\n"
    "  tela diag              diagnóstico da tela branca (veja o README)\n"
    "  tela vigia <ms>        sem o fio MISO: reenvio às cegas a cada <ms> (0 = desliga)\n"
    "  tela spi <MHz>         velocidade da tela, ex.: tela spi 10 (reinicia a placa)\n"
    "  tela detector desligar|ligar   detector de tensão baixa da própria tela (teste)\n"
    "  tela faixa             pinta 3 faixas direto, sem a imagem do Tijolo, e lê de volta\n"
    "  tela rede desligar|ligar   refazer a tela nas trocas de rede 4G/2G (desligar só para testes)\n"
    "  memoria                onde está a imagem da tela e se a PSRAM perdeu os dados\n"
    "  memoria interna|psram  muda onde fica a imagem da tela (reinicia a placa)\n"
    "  tensao [segundos]      3V3 e bateria a cada segundo (precisa do resistor no IO2); tensao parar\n"
    "  radio desligar|ligar   rádio do modem (teste: desligado não há chamadas)\n"
    "  amplificador desligar|ligar   amplificador do alto-falante (teste)\n"
    "  brilho <0-255>         luz da tela (até reiniciar)\n"
    "  dormir                 desliga o Tijolo (para ligar: segure o 3 ou o vermelho)\n"
    "  at <comando>           comando AT direto. ex.: at AT+CPSI?\n"
    "  log sim|nao            mostra todo o tráfego AT (para aprender)\n"
    "  reiniciar              reinicia o modem\n"));
}

void status() {
  Serial.println();
  Serial.println("Chip:      " + modem.estadoChip());
  Serial.println("Sinal:     " + barrasDeSinal(modem.sinal()));
  Serial.println("Rede:      " + modem.tipoDeRede() + " · " + modem.registro());
  String op = modem.operadora();
  Serial.println("Operadora: " + (op.length() ? op : String("(nenhuma)")));
  Serial.printf("Mensagens: %u na memória do Tijolo (%u nova(s))", (unsigned)caixa.total(), (unsigned)caixa.novas());
  if (caixaNaFlash)
    Serial.printf(" · flash: %u de %u KB usados\n", (unsigned)(PastaLittleFS::usados() / 1024),
                  (unsigned)(PastaLittleFS::total() / 1024));
  else
    Serial.println(" · NA RAM (a flash não montou: some ao desligar)");
  int usados, total;
  if (modem.ocupacaoSms(usados, total))
    Serial.printf("Chip:      %d de %d posições de SMS ocupadas\n", usados, total);
  const int mv = milivoltsBateria();
  const int pct = porcentagemBateria(mv);
  Serial.printf("Bateria:   %.2f V", mv / 1000.0f);
  if (pct >= 0) Serial.printf(" (%d%%)", pct);
  Serial.println("  [com a USB ligada, a leitura é a da carga]");
  String h = modem.horaDaRede();
  if (h.length()) Serial.println("Hora:      " + h + "  (aa/mm/dd, fuso em quartos de hora)");
  Serial.printf("Tela:      %s%s · teclado: %s\n", temTela ? "ok" : "não iniciou",
                tela.temLeitura() ? " (com leitura pelo MISO)" : "",
                teclado.temMapa() ? "mapa guardado" : "sem mapa (teclado aprender)");
}

// ---------- SMS ----------
pdu::Mensagem paraMensagem(const Sms& s) {
  pdu::Mensagem m;
  m.tipo = s.enviada ? 1 : 0;
  m.numero = s.numero.c_str();
  m.dataHora = s.dataHora.c_str();
  m.texto = s.texto.c_str();
  m.binaria = s.binaria;
  m.ref = s.ref;
  m.total = s.total;
  m.parte = s.parte;
  return m;
}

String listaDeIndices(const std::vector<int>& v) {
  String r;
  for (size_t i = 0; i < v.size(); i++) { if (i) r += ","; r += String(v[i]); }
  return r;
}

// Cabeçalho + texto. `indices` = posições no chip (várias num SMS longo).
void mostrarMensagem(const pdu::Mensagem& m, const String& indices, const String& estado, const char* nota) {
  String quem = (m.tipo == 1 ? "Para " : "De ") + String(m.numero.c_str());
  Serial.printf("\n[%s] %s", indices.c_str(), quem.c_str());
  if (m.dataHora.size()) Serial.printf(" · %s", m.dataHora.c_str());
  if (estado == "REC UNREAD") Serial.print(" · NOVA");
  if (nota && nota[0]) Serial.printf(" · %s", nota);
  Serial.println();
  Serial.println("    " + String(m.texto.c_str()));
}

void mostrarSms(const Sms& s) {
  String nota = s.total > 1 ? "parte " + String(s.parte) + " de " + String(s.total) : String("");
  mostrarMensagem(paraMensagem(s), String(s.indice), s.estado, nota.c_str());
}

// ---------- A caixa de mensagens no monitor ----------
// "26/10/21,18:05:00", como o modem escreve a data (vazio sem a hora da rede)
std::string dataHoraAgora() {
  const Situacao s = situacaoAtual();
  if (!s.horaValida) return "";
  char b[24];
  snprintf(b, sizeof b, "%02d/%02d/%02d,%02d:%02d:00", s.ano % 100, s.mes, s.dia, s.hora, s.minuto);
  return b;
}

const char* nomeDoEstado(Caixa::Estado e) {
  switch (e) {
    case Caixa::Nova: return "NOVA";
    case Caixa::Lida: return "lida";
    case Caixa::Enviada: return "enviada";
    case Caixa::NaoEnviada: return "NÃO ENVIADA";
    case Caixa::Rascunho: return "rascunho";
  }
  return "?";
}

void mostrarDaCaixa(const Caixa::Mensagem& m, bool inteira) {
  const bool recebida = m.estado == Caixa::Nova || m.estado == Caixa::Lida;
  Serial.printf("\n[%lu] %s %s", (unsigned long)m.id, recebida ? "De" : "Para", m.numero.c_str());
  if (!m.dataHora.empty()) Serial.printf(" · %s", m.dataHora.c_str());
  Serial.printf(" · %s\n", nomeDoEstado(m.estado));
  std::string t = m.texto;
  if (!inteira && t.size() > 80) {             // só o começo (cortado num caractere inteiro)
    size_t n = 80;
    while (n > 0 && (((uint8_t)t[n]) & 0xC0) == 0x80) n--;
    t = t.substr(0, n) + "…";
  }
  String linha = t.c_str();
  linha.replace("\n", "\n    ");
  Serial.println("    " + linha);
}

// As últimas 15 (ou todas), da mais antiga para a mais nova: a mais nova
// fica embaixo, perto de onde se digita
void listarCaixa(bool tudo) {
  const auto& itens = caixa.itens();
  if (itens.empty()) { Serial.println("Nenhuma mensagem na memória do Tijolo."); return; }
  const size_t mostrar = tudo ? itens.size() : std::min<size_t>(itens.size(), 15);
  for (size_t i = mostrar; i-- > 0;) {
    Caixa::Mensagem m;
    if (caixa.ler(itens[i].id, m)) mostrarDaCaixa(m, false);
  }
  Serial.printf("\n%u mensagem(ns), %u nova(s)%s. ler <n> mostra inteira; apagar <n> apaga.\n",
                (unsigned)itens.size(), (unsigned)caixa.novas(),
                mostrar < itens.size() ? " (estas são as 15 últimas; lista tudo mostra todas)" : "");
}

// SMS completo (ou um longo que desistiu de esperar as partes): vai para a
// caixa como novo, aparece na tela e sai do chip.
bool jaNaCaixa(const std::string& numero, const std::string& dataHora, const std::string& texto, int quantas);

void entregarSms(const pdu::Mensagem& m, const std::vector<int>& posicoes, const char* nota) {
  std::string texto = m.texto;
  if (nota && nota[0]) texto += std::string("\n[") + nota + "]";
  if (jaNaCaixa(m.numero, m.dataHora, texto, 10)) {     // a mesma de novo (já guardada ao ligar)
    if (podeApagarDoChip()) apagarDoChip(posicoes);
    return;
  }
  const uint32_t id = ui.mensagem(m.numero, m.dataHora, texto, millis());
  if (!id) {
    avisar("A mensagem não coube na memória do Tijolo: ficou no chip (posição " + listaDeIndices(posicoes) + ").");
    return;
  }
  if (podeApagarDoChip()) apagarDoChip(posicoes);
}

// ---------- SMS que já estavam no chip ----------
// Ao ligar (ou quando o chip fica pronto) e com "chip importar": os SMS
// recebidos que estão no chip vão para a caixa e saem dele. Normalmente o
// chip está vazio; sobram lá os que chegaram com a placa reiniciando (sem
// o aviso +CMTI) e os que não deu para tirar do chip da outra vez.
// Na primeira vez com a Fase 3, os que a Fase 2 sabia não lidos (a lista
// "naolidas") entram como novos e os outros como lidos (já foram vistos).
bool importarPendente = true;
uint32_t importarEm = 0;

// Já está na caixa? (guardada, mas não deu para tirar do chip)
// (só as `quantas` recebidas mais recentes: cada uma é um arquivo para ler)
bool jaNaCaixa(const std::string& numero, const std::string& dataHora, const std::string& texto, int quantas) {
  int conferidas = 0;
  for (const auto& it : caixa.itens()) {
    if (it.estado != Caixa::Nova && it.estado != Caixa::Lida) continue;
    if (++conferidas > quantas) break;
    Caixa::Mensagem c;
    if (caixa.ler(it.id, c) && c.numero == numero && c.dataHora == dataHora && c.texto == texto) return true;
  }
  return false;
}

// "26/10/21,18:05:12-12" → "261021180512": a ordem das datas
std::string chaveDaData(const std::string& d) {
  std::string k;
  for (char c : d)
    if (c >= '0' && c <= '9' && k.size() < 12) k += c;
  return k;
}

// As posições do chip das mensagens não lidas da Fase 2 ("naolidas": grupos
// [quantas][posições...])
std::set<int> lerNaoLidas() {
  std::set<int> r;
  const size_t n = memoria.isKey("naolidas") ? memoria.getBytesLength("naolidas") : 0;
  if (n == 0 || n > 1024) return r;
  std::vector<uint8_t> b(n);
  memoria.getBytes("naolidas", b.data(), n);
  for (size_t i = 0; i < n;) {
    const size_t k = b[i++];
    for (size_t j = 0; j < k && i < n; j++) r.insert(b[i++]);
  }
  return r;
}

void gravarNaoLidas(const std::set<int>& posicoes) {
  std::vector<uint8_t> b;
  std::vector<int> v(posicoes.begin(), posicoes.end());
  for (size_t i = 0; i < v.size(); i += 255) {
    const size_t k = std::min<size_t>(255, v.size() - i);
    b.push_back((uint8_t)k);
    for (size_t j = 0; j < k; j++) b.push_back((uint8_t)v[i + j]);
  }
  if (!b.empty()) memoria.putBytes("naolidas", b.data(), b.size());
}

// manual: "chip importar" (importa mesmo com os SMS ficando no chip)
void importarDoChip(bool manual) {
  if (!modemOk || !modem.chipPronto()) return;          // fica para quando o chip estiver pronto
  importarPendente = false;
  const bool primeiraVez = !memoria.isKey("caixa");
  // Com os SMS ficando também no chip (APAGAR_SMS_DO_CHIP = false), importar
  // a cada vez que liga traria de volta os que você apagou da caixa: só na
  // primeira vez, ou pedindo ("chip importar").
  if (!podeApagarDoChip() && !primeiraVez && !manual) return;
  std::vector<Sms> lista;
  if (!modem.listarSms(lista)) {
    avisar("Não consegui ler os SMS do chip (" + modem.ultimoErro() + "); tento de novo em 30 s.");
    importarPendente = true;
    importarEm = (millis() + 30000) | 1;
    return;
  }
  // Na migração da Fase 2, só as que ninguém viu entram como novas. A lista
  // do chip acabou de marcar todas como lidas: as não lidas ficam anotadas
  // antes de importar, para o caso de a placa reiniciar no meio.
  std::set<int> naoLidas;
  if (primeiraVez) {
    naoLidas = lerNaoLidas();
    const size_t antes = naoLidas.size();
    for (const auto& s : lista)
      if (s.estado == "REC UNREAD") naoLidas.insert(s.indice);
    if (naoLidas.size() != antes) gravarNaoLidas(naoLidas);
  }
  // As completas, da mais antiga para a mais nova (a ordem da caixa é a de chegada)
  std::vector<pdu::Montador::Pronta> prontas;
  for (const auto& s : lista) {
    if (s.enviada) continue;                            // guardadas por outro aparelho: ficam no chip
    if (montador.temIndice(s.indice)) continue;         // já está esperando as outras partes
    pdu::Montador::Pronta p;
    if (montador.adicionar(paraMensagem(s), s.indice, millis(), p)) prontas.push_back(p);   // longo: espera o resto
  }
  std::stable_sort(prontas.begin(), prontas.end(), [](const pdu::Montador::Pronta& a, const pdu::Montador::Pronta& b) {
    return chaveDaData(a.msg.dataHora) < chaveDaData(b.msg.dataHora);
  });
  int novas = 0, lidas = 0, repetidas = 0, ficaram = 0;
  for (const auto& p : prontas) {
    if (jaNaCaixa(p.msg.numero, p.msg.dataHora, p.msg.texto, 30)) {
      repetidas++;
    } else {
      bool nova = true;
      if (primeiraVez) {
        nova = false;
        for (int i : p.indices) nova = nova || naoLidas.count(i) > 0;
      }
      if (!caixa.guardar(nova ? Caixa::Nova : Caixa::Lida, p.msg.numero, p.msg.dataHora, p.msg.texto)) {
        ficaram++;
        continue;                                       // não coube: fica no chip
      }
      (nova ? novas : lidas)++;
    }
    if (podeApagarDoChip()) apagarDoChip(p.indices);
  }
  if (caixaNaFlash) {
    memoria.putUChar("caixa", 1);                       // a migração da Fase 2 já foi feita
    if (memoria.isKey("naolidas")) memoria.remove("naolidas");
  }
  if (novas || lidas || repetidas || ficaram) {
    String r = "SMS do chip para a memória do Tijolo: " + String(novas) + " nova(s), " + String(lidas) + " lida(s)";
    if (repetidas) r += ", " + String(repetidas) + " que já estava(m) lá";
    if (ficaram) r += ", " + String(ficaram) + " NÃO couberam (ficaram no chip)";
    avisar(r + (podeApagarDoChip() ? ". O chip ficou livre." : "."));
    ui.caixaMudou(millis());
  }
}


uint32_t telaDeTesteAte = 0;   // enquanto não zera, a interface não desenha
bool telaApagada = false;      // a luz estava apagada no loop anterior

void telaDeTeste() {
  if (!temTela) { Serial.println("A tela não iniciou: confira os fios e o config.h."); return; }
  tela.reiniciar();
  const uint16_t cores[6] = {rgb(255, 0, 0), rgb(0, 255, 0), rgb(0, 0, 255),
                             rgb(255, 255, 0), rgb(255, 255, 255), rgb(0, 0, 0)};
  const char* nomes[6] = {"vermelho", "verde", "azul", "amarelo", "branco", "preto"};
  quadro.semRecorte();
  quadro.limpar(rgb(40, 40, 40));
  for (int i = 0; i < 6; i++) {
    quadro.retangulo(0, 20 + i * 40, Quadro::LARGURA, 36, cores[i]);
    quadro.texto(12, 20 + i * 40 + 25, nomes[i], fontes::fonteTitulo, i == 4 || i == 3 ? rgb(0, 0, 0) : rgb(255, 255, 255));
  }
  quadro.texto(Quadro::LARGURA / 2, 296, "Ação, ç, ã, é: tudo certo?", fontes::fontePequena, rgb(255, 255, 255), Alinha::Centro);
  quadro.moldura(0, 0, Quadro::LARGURA, Quadro::ALTURA, rgb(255, 0, 255));
  tela.enviarTudo(quadro);
  tela.brilho(BRILHO_MAXIMO);
  telaDeTesteAte = (millis() + 20000) | 1;
  Serial.println("Na tela: faixas vermelho, verde, azul, amarelo, branco e preto, de cima para baixo,\n"
                 "com moldura rosa nas bordas. Cores trocadas ou de cabeça para baixo? Veja o config.h.\n"
                 "Volta sozinha em 20 s (ou aperte uma tecla).");
}

// ============================================================
//  Diagnóstico da tela branca
//  Com o fio MISO, cada vez que a tela se perde o Tijolo anota o tipo de
//  falha junto com o que estava acontecendo (tensão, rádio, rede). Os
//  comandos radio/amplificador/tela spi mudam uma coisa de cada vez.
// ============================================================
String volts(int mv) {
  char b[12];
  snprintf(b, sizeof b, "%.2f V", mv / 1000.0f);
  return b;
}

String instante(uint32_t ms) {          // "3:12.4" desde que a placa ligou
  char b[20];
  const uint32_t s = ms / 1000;
  snprintf(b, sizeof b, "%lu:%02lu.%lu", (unsigned long)(s / 60), (unsigned long)(s % 60),
           (unsigned long)(ms % 1000 / 100));
  return b;
}

String contextoDoModem() {
  String s = "rede " + (cache.rede.length() ? cache.rede : String("?"));
  if (cache.registro.length()) s += " " + cache.registro;
  s += ", sinal " + ((cache.csq >= 0 && cache.csq != 99) ? String(cache.csq) : String("?"));
  s += radioLigado ? " · rádio ligado" : " · RÁDIO DESLIGADO";
  if (!altoFalanteLigado) s += " · amplificador desligado";
  if (modemOk) s += " · modem ligado há " + String((millis() - modemLigadoEm) / 1000) + " s";
  else s += " · modem não respondeu";
  return s;
}

String contextoDaTensao(uint32_t ms) {
  if (!medidor.ativo()) return "";
  const Janela v = medidor.tensao3v3(ms), b = medidor.bateria(ms);
  if (v.n == 0) return "";
  return "3V3 mín " + volts(v.minimo) + " máx " + volts(v.maximo) + " (média " + volts(v.media()) + ")" +
         " · bateria mín " + volts(b.minimo) + " (média " + volts(b.media()) + ")";
}

// "+CPSI: GSM,Online,724-05,0x1a2b,12345,52,-67,..." → "2G, canal 52 (900 MHz), RxLev -67".
// No 2G, a potência máxima do modem depende da faixa: 2 W em 850/900 MHz,
// 1 W em 1800 MHz. Saber a faixa de cada troca ajuda a achar a causa.
String descreverRede(const String& resposta) {
  String s = resposta;
  const int p = s.indexOf(':');
  if (p >= 0) s = s.substring(p + 1);
  s.trim();
  std::vector<String> f;
  for (int ini = 0;;) {
    const int v = s.indexOf(',', ini);
    String c = s.substring(ini, v < 0 ? s.length() : v);
    c.trim();
    f.push_back(c);
    if (v < 0) break;
    ini = v + 1;
  }
  String d;
  if (f[0] == "GSM" && f.size() >= 7) {
    bool numero = f[5].length() > 0;
    for (unsigned i = 0; i < f[5].length(); i++) numero = numero && isdigit((unsigned char)f[5][i]);
    const long n = f[5].toInt();
    const char* faixa = !numero ? "" : (n <= 124 || (n >= 975 && n <= 1023)) ? " (900 MHz)"
                        : (n >= 128 && n <= 251) ? " (850 MHz)" : (n >= 512 && n <= 885) ? " (1800 MHz)" : "";
    d = "2G, canal " + f[5] + faixa + ", RxLev " + f[6];
  } else if (f[0] == "LTE" && f.size() >= 7) {
    d = "4G, " + f[6];
  } else {
    d = f[0];
  }
  return d + " · " + s;
}

void relatarOcorrencia(const OcorrenciaTela& o) {
  using namespace estado_tela;
  char lido[160];
  descrever(o.estado, lido, sizeof lido);
  String t = "TELA [" + instante(o.quando) + "]: ";
  if (o.falha == Falha::SemResposta) {
    if (o.consertou)
      t += "o fio MISO voltou a responder: o Tijolo voltou a conferir a tela pelo boletim.";
    else
      t += "o fio MISO parou de responder (leituras todas 0 ou todas 1).\n"
           "    O Tijolo passou para a vigia às cegas e confere o fio de novo a cada 15 s.\n"
           "    pista: " + String(pista(o.falha)) + " (se voltar sozinho, era a tela sem energia: mau contato no VCC/GND)";
    t += "\n    " + contextoDoModem();
    avisar(t);
    return;
  }
  t += String(nome(o.falha)) + " → ";
  if (o.consertou) {
    t += o.conserto == 0 ? "voltou ao normal sem conserto na hora"
         : o.conserto == 1 ? "consertada (reenvio da configuração)" : "consertada (RESET completo)";
    if (o.tentativas > 1) t += ", depois de " + String(o.tentativas) + " tentativas";
  } else {
    char depois[160];
    descrever(o.depois, depois, sizeof depois);
    t += "NÃO consertou nem com RESET. O Tijolo tenta de novo, cada vez mais espaçado (até 1 min).";
    t += "\n    depois do RESET: " + String(depois);
  }
  t += "\n    a tela disse: " + String(lido);
  t += "\n    pista: " + String(pista(o.falha));
  const String v = contextoDaTensao(1000);
  if (v.length()) t += "\n    último 1 s: " + v;
  t += "\n    " + contextoDoModem();
  avisar(t);
}

// ---------- Tela travada branca pela troca de rede (4G ↔ 2G) ----------
// O controlador da tela não percebe (diz "tudo certo"), então o Tijolo refaz
// a tela, no escuro, quando o modem avisa que trocou de rede (o que inclui
// as chamadas: o modem cai para o 2G para tocar ou ligar e volta depois).
void refazerTela(const char* motivo) {
  escuroAteRefazer = false;                // o loop acende a luz depois de redesenhar
  refazerTelaEm = 0;                       // um RESET agora vale pelo que estava agendado
  if (!temTela || telaDeTesteAte) return;
  tela.refazerNoEscuro();
  ui.redesenhar();
  telasRefeitas++;
  avisar("[tela refeita às " + instante(millis()) + ": " + motivo + "]");
}

// Chamada pelo modem NA HORA de cada evento (até no meio de um comando).
// Na troca de rede, apaga a luz já: o painel travado ficaria branco até o
// loop voltar. O RESET vem 1,5 s depois do último aviso (numa chamada
// recebida chegam três seguidos: saiu do 4G, sem serviço, entrou no 2G), e
// a luz só volta depois dele, ou antes, quando a chamada chega (RING). Nunca
// mais de 5 s no escuro.
bool setupPronto = false;                // o loop() já está rodando

void naHoraDoModem(const EventoModem& ev) {
  if (ev.tipo != EventoModem::RedeMudou) return;
  ultimaTrocaEm = millis();
  if (!refazerNaTroca || !temTela || telaDeTesteAte) return;
  if (!setupPronto) {                    // ainda ligando: refaz logo que o loop começar
    refazerTelaEm = millis() | 1;
    motivoRefazer = "rede mudou enquanto ligava";
    return;
  }
  tela.brilho(0);
  if (!escuroAteRefazer) {
    escuroAteRefazer = true;
    escuroDesde = millis();
  }
  uint32_t prazo = millis() + 1500;
  if ((int32_t)(prazo - (escuroDesde + 5000)) > 0) prazo = escuroDesde + 5000;
  refazerTelaEm = prazo | 1;
  motivoRefazer = ev.valor == 0 ? "rede caiu" : ev.valor <= 3 ? "rede mudou para 2G" : "rede mudou para 4G";
}

void relatarSentinela() {
  String t = "PSRAM [" + instante(millis()) + "]: ";
  if (!sentinela.ok()) {
    char b[160];
    snprintf(b, sizeof b, "PERDEU OS DADOS (%lu de %lu palavras erradas; leu 0x%08lX onde era 0x%08lX).",
             (unsigned long)sentinela.erradas(), (unsigned long)SentinelaPsram::PALAVRAS,
             (unsigned long)sentinela.lido(), (unsigned long)sentinela.esperado());
    t += b;
    t += quadroNaPsram ? "\n    A imagem da tela mora nela: a tela fica branca até reiniciar a placa (botão RST)."
                       : "\n    A imagem da tela está na RAM interna: a tela não sente.";
  } else {
    t += "voltou a guardar os dados (o que estava nela antes continua perdido).";
  }
  const String v = contextoDaTensao(1000);
  if (v.length()) t += "\n    último 1 s: " + v;
  t += "\n    " + contextoDoModem();
  avisar(t);
}

void infoMemoria() {
  const uint32_t interna = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t maior = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  Serial.println("\n--- Memória ---");
  Serial.printf("Imagem da tela: %s%s\n", quadroNaPsram ? "na PSRAM" : "na RAM interna",
                memoria.getUChar("quadropsram", 0) ? " (escolhida com 'memoria psram')" : "");
  Serial.printf("RAM interna: %lu KB livres (maior pedaço %lu KB) · PSRAM: %lu KB livres de %lu KB\n",
                (unsigned long)(interna / 1024), (unsigned long)(maior / 1024),
                (unsigned long)(ESP.getFreePsram() / 1024), (unsigned long)(ESP.getPsramSize() / 1024));
  if (!sentinela.ativa())
    Serial.println("Sentinela da PSRAM: sem PSRAM.");
  else if (sentinela.perdas() == 0)
    Serial.printf("Sentinela da PSRAM: nenhuma perda (conferida %lu vezes, a cada %lu ms).\n",
                  (unsigned long)sentinela.conferencias(), (unsigned long)SentinelaPsram::A_CADA_MS);
  else
    Serial.printf("Sentinela da PSRAM: PERDEU OS DADOS %lu vez(es), a 1ª às %s; agora está %s.\n",
                  (unsigned long)sentinela.perdas(), instante(sentinela.primeiraPerda()).c_str(),
                  sentinela.ok() ? "guardando de novo" : "RUIM");
  if (temTela) {
    const uint16_t* p = quadro.pontos();
    uint32_t brancos = 0;
    for (int i = 0; i < Quadro::LARGURA * Quadro::ALTURA; i++) brancos += p[i] == 0xFFFF;
    Serial.printf("Pontos brancos na imagem da tela: %lu%% (as telas do Tijolo são escuras; perto de 100%% = virou lixo)\n",
                  (unsigned long)(brancos * 100 / (Quadro::LARGURA * Quadro::ALTURA)));
  }
}

void faixaDeTeste() {
  if (!temTela) { Serial.println("A tela não iniciou: confira os fios e o config.h."); return; }
  uint64_t lidos[3] = {};
  const bool leu = tela.faixasDeTeste(lidos);
  tela.brilho(BRILHO_MAXIMO);
  telaDeTesteAte = (millis() + 15000) | 1;
  Serial.println("Pintei três faixas, vermelha, verde e azul, direto da RAM interna, sem a imagem do Tijolo.\n"
                 "Ficam 15 s (ou até uma tecla).");
  if (leu) {
    const int o = estado_tela::atrasoComCores(lidos);
    if (o >= 0) {
      Serial.printf("Lendo pela MISO: a memória da tela TEM as três faixas (atraso de %d bits).\n", o);
    } else {
      Serial.print("Lendo pela MISO: a memória da tela NÃO tem as faixas. Lido:");
      for (int k = 0; k < 3; k++)
        Serial.printf(" %04lX%08lX", (unsigned long)(lidos[k] >> 32), (unsigned long)(lidos[k] & 0xFFFFFFFFu));
      Serial.println();
    }
  } else {
    Serial.println("Sem o fio MISO não dá para ler a memória da tela: vale só o que você vê.");
  }
  Serial.println("Como ler:\n"
                 "  as faixas aparecem: a tela e os fios estão bons; o problema é a imagem que o Tijolo\n"
                 "    manda (veja 'memoria');\n"
                 "  não aparecem, mas a memória da tela tem as faixas: o painel não mostra o que tem\n"
                 "    (problema no módulo da tela);\n"
                 "  não aparecem e a memória não tem: a imagem não chega pelos fios SCK e SDI.");
}

// O SPI do S3 divide 80 MHz por um número inteiro (a LovyanGFX escolhe o
// maior valor que não passa do pedido): 40, 26,7, 20, 16, 13,3, 11,4, 10...
float mhzDeVerdade(uint32_t hz) {
  if (hz >= 80000000UL) return 80.0f;
  return 80.0f / (80000000UL / (hz + 1) + 1);
}

uint32_t totalDeFalhas() {
  uint32_t t = 0;
  for (int i = 1; i < estado_tela::N_FALHAS; i++) t += tela.contagem().falhas[i];
  return t;
}

void diagTela() {
  using namespace estado_tela;
  Serial.println("\n--- Diagnóstico da tela ---");
  if (!temTela) { Serial.println("A tela não iniciou: confira os fios e o config.h."); return; }
  Serial.printf("SPI da tela: %.1f MHz%s · detector de tensão baixa da tela: %s\n", mhzDeVerdade(tela.spiHz()),
                memoria.getUChar("telaspi", 0) ? " (escolhido com tela spi)" : "",
                tela.detectorLigado() ? "ligado (padrão)" : "DESLIGADO (experimento)");
  const ContagemTela& c = tela.contagem();
  char txt[160];
  if (tela.temLeitura())
    Serial.printf("Fio MISO: funcionando; o Tijolo confere a tela a cada %lu ms.\n", (unsigned long)TELA_CONFERIR_MS);
  else if (tela.temFio())
    Serial.println("Fio MISO: respondia no começo e PAROU (o Tijolo confere de novo a cada 15 s).");
  else
    Serial.println("Fio MISO: não responde. Sem ele o Tijolo não sabe quando a tela se perde.\n"
                   "  Ligue o SDO(MISO) da tela no J1-15 (MISO) e reinicie a placa.");
  if (tela.temFio()) {
    uint32_t e = 0;
    tela.lerEstado(e);
    descrever(e, txt, sizeof txt);
    Serial.printf("Agora:      0x%08lX %s → %s\n", (unsigned long)e, txt, nome(classificar(tela.referencia(), e)));
    descrever(tela.referencia(), txt, sizeof txt);
    Serial.printf("Referência: 0x%08lX %s\n", (unsigned long)tela.referencia(), txt);
    Serial.printf("Falhas desde que a placa ligou (há %s):\n", instante(millis()).c_str());
    for (int i = 1; i < N_FALHAS; i++)
      if (c.falhas[i])
        Serial.printf("  %4lu × %s\n         pista: %s\n", (unsigned long)c.falhas[i], nome((Falha)i), pista((Falha)i));
    if (totalDeFalhas() == 0) Serial.println("  nenhuma");
    Serial.printf("Consertos: %lu com reenvio, %lu com RESET, %lu rodadas sem sucesso.\n",
                  (unsigned long)c.consertosLeves, (unsigned long)c.consertosCompletos, (unsigned long)c.semConserto);
    Serial.printf("Leituras com ruído (não se confirmaram): %lu · reenvios de segurança: %lu\n",
                  (unsigned long)c.leiturasRuins, (unsigned long)c.reforcosDeSeguranca);
  }
  if (!tela.temLeitura()) {
    if (tela.vigiaMs())
      Serial.printf("Às cegas: reenvia a configuração e a imagem a cada %lu ms com a luz acesa (%lu vezes).\n",
                    (unsigned long)tela.vigiaMs(), (unsigned long)c.reenviosAsCegas);
    else
      Serial.println("Às cegas: DESLIGADO (tela vigia 2000 religa). Tela branca fica branca até 'tela reiniciar'.");
  }
  if (medidor.ativo()) {
    Serial.println("Tensões, últimos 10 s: " + contextoDaTensao(10000));
    Serial.printf("  (%lu leituras por segundo em cada pino)\n", (unsigned long)medidor.leiturasPorSegundo());
  } else {
    Serial.printf("3V3: sem medição. O IO2 leu %d mV; com o resistor de 10 kΩ entre J2-4 e J2-5 ele lê ~1650 mV.\n",
                  medidor.leituraInicial());
  }
  if (tela.temFio()) {
    uint32_t v1 = 0, v2 = 0;
    if (!tela.temVarredura())
      Serial.println("Varredura do painel (comando 45h): este módulo não responde; o Tijolo não vê o painel parar.");
    else if (tela.lerVarredura(v1, v2))
      Serial.printf("Varredura do painel (comando 45h): %s (0x%08lX → 0x%08lX)\n",
                    v1 != v2 ? "andando" : "PARADA (o painel travou: a tela deve estar branca)", (unsigned long)v1,
                    (unsigned long)v2);
  }
  Serial.printf("Tela refeita por troca de rede: %lu vez(es) · %s\n", (unsigned long)telasRefeitas,
                refazerNaTroca ? "ligado" : "DESLIGADO (tela rede ligar)");
  if (ultimaTrocaEm) Serial.printf("Última troca de rede: às %s (agora: %s)\n", instante(ultimaTrocaEm).c_str(),
                                   instante(millis()).c_str());
  Serial.printf("Imagem da tela: %s · PSRAM: %s\n", quadroNaPsram ? "na PSRAM" : "na RAM interna",
                !sentinela.ativa() ? "sem" : sentinela.perdas() ? "PERDEU OS DADOS (veja 'memoria')" : "nenhuma perda");
  String cpsi;
  if (modemOk && modem.comando("AT+CPSI?", &cpsi, 1500)) Serial.println("Rede agora: " + descreverRede(cpsi));
  if (ultimaRede.length()) Serial.println("Depois da última troca: " + descreverRede(ultimaRede));
  Serial.println("Modem: " + contextoDoModem());
}

void linhaDoMonitor() {
  const Janela v = medidor.tensao3v3(1000), b = medidor.bateria(1000);
  if (v.n == 0) { avisar("(sem leituras no último segundo)"); return; }
  String t = "3V3 " + volts(v.media()) + " (mín " + volts(v.minimo) + " · máx " + volts(v.maximo) + ")" +
             " · bateria " + volts(b.media()) + " (mín " + volts(b.minimo) + ")";
  if (tela.temLeitura()) t += " · falhas da tela: " + String(totalDeFalhas());
  t += " · " + (cache.rede.length() ? cache.rede : String("?")) + " sinal " +
       ((cache.csq >= 0 && cache.csq != 99) ? String(cache.csq) : String("?"));
  if (!radioLigado) t += " · rádio desligado";
  avisar(t);
}

// "ligar", "liga", "sim", "on", "1" → true; "desligar", "nao", "off", "0" → false; outra coisa → -1
int ligarOuDesligar(const String& r) {
  if (r.startsWith("des") || r.startsWith("n") || r == "off" || r == "0") return 0;
  if (r.startsWith("lig") || r.startsWith("s") || r == "on" || r == "1") return 1;
  return -1;
}

void executar(String linha) {
  linha.trim();
  if (linha.length() == 0) return;
  int esp = linha.indexOf(' ');
  String cmd = esp < 0 ? linha : linha.substring(0, esp);
  String resto = esp < 0 ? String("") : linha.substring(esp + 1);
  resto.trim();
  cmd.toLowerCase();
  if (cmd == "tela") resto.toLowerCase();       // subcomandos da tela não têm texto livre

  auto falhou = [](const char* oque) { Serial.printf("Falhou: %s (%s)\n", oque, modem.ultimoErro().c_str()); };

  if (cmd == "ajuda" || cmd == "?" || cmd == "help") { ajuda(); return; }
  if (cmd == "status") { status(); return; }

  if (cmd == "ligar") {
    String n = normalizarNumero(resto);
    if (n.length() == 0) { Serial.println("Uso: ligar <numero>"); return; }
    Serial.println("Ligando para " + n + "... ('desligar' cancela)");
    if (!modem.ligar(n)) falhou("ligar");
    return;
  }
  if (cmd == "atender")  { if (!modem.atender()) falhou("atender"); return; }
  if (cmd == "desligar") {
    bool havia = modem.estado() != EstadoChamada::Livre;
    if (!modem.desligar()) falhou("desligar");
    else if (!havia) Serial.println("Não havia chamada (mandei desligar assim mesmo).");
    return;
  }
  if (cmd == "tons")     { if (!modem.tons(resto)) falhou("tons"); else Serial.println("ok"); return; }

  if (cmd == "sms") {
    int e = resto.indexOf(' ');
    if (e < 0) { Serial.println("Uso: sms <numero> <texto>"); return; }
    String n = normalizarNumero(resto.substring(0, e));
    String t = resto.substring(e + 1);
    int digitos = 0;
    for (unsigned i = 0; i < n.length(); i++) if (isdigit((unsigned char)n[i])) digitos++;
    if (digitos < 3) {
      Serial.println("Número curto demais. Escreva o número sem espaços: sms 11999998888 texto");
      return;
    }
    Serial.println("Enviando para " + n + "...");
    int partes = 0;
    const bool ok = modem.enviarSms(n, t, &partes);
    if (ok) Serial.printf("Enviado%s.\n", partes > 1 ? (" em " + String(partes) + " partes").c_str() : "");
    else falhou("enviar SMS");
    // Fica na caixa, como as enviadas pela tela
    caixa.guardar(ok ? Caixa::Enviada : Caixa::NaoEnviada, n.c_str(), dataHoraAgora(), t.c_str());
    ui.caixaMudou(millis());
    return;
  }
  if (cmd == "lista") { listarCaixa(resto == "tudo"); return; }
  if (cmd == "ler") {
    Caixa::Mensagem m;
    const uint32_t id = (uint32_t)resto.toInt();
    if (!id || !caixa.ler(id, m)) { Serial.println("Não há a mensagem " + resto + ". Veja a lista."); return; }
    mostrarDaCaixa(m, true);
    if (m.estado == Caixa::Nova && caixa.mudar(id, Caixa::Lida)) ui.caixaMudou(millis());
    return;
  }
  if (cmd == "apagar") {
    if (resto == "tudo") {
      Serial.printf("Apaguei %u mensagem(ns) da memória do Tijolo.\n", (unsigned)caixa.apagarTodas());
    } else {
      const uint32_t id = (uint32_t)resto.toInt();
      if (id && caixa.apagar(id)) Serial.println("Apagada.");
      else Serial.println("Não há a mensagem " + resto + ". Veja a lista.");
    }
    ui.caixaMudou(millis());
    return;
  }
  if (cmd == "chip") {                          // os SMS que estão no chip (normalmente nenhum)
    resto.toLowerCase();
    if (resto.length() == 0) {
      std::vector<Sms> lista;
      if (!modem.listarSms(lista)) { falhou("listar"); return; }
      if (lista.empty()) { Serial.println("Nenhum SMS no chip (as mensagens ficam na memória do Tijolo: lista)."); return; }
      pdu::Montador juntar;                     // SMS longos aparecem inteiros
      for (auto& s : lista) {
        pdu::Montador::Pronta p;
        if (juntar.adicionar(paraMensagem(s), s.indice, 0, p))
          mostrarMensagem(p.msg, listaDeIndices(p.indices), s.estado, "");
      }
      for (auto& p : juntar.pendentes())
        mostrarMensagem(p.msg, listaDeIndices(p.indices), "", "incompleto: faltam partes");
      return;
    }
    if (resto.startsWith("ler")) {
      Sms s;
      if (modem.lerSms(resto.substring(3).toInt(), s)) mostrarSms(s); else falhou("ler");
      return;
    }
    if (resto.startsWith("apagar")) {
      String r = resto.substring(6);
      r.trim();
      const bool ok = r == "tudo" ? modem.apagarTodosSms() : modem.apagarSms(r.toInt());
      if (ok) Serial.println("Apagado do chip."); else falhou("apagar");
      return;
    }
    if (resto == "importar") {
      importarDoChip(true);
      Serial.println(importarPendente ? "O chip ainda não está pronto." : "Pronto.");
      return;
    }
    Serial.println("Uso: chip | chip ler <posição> | chip apagar <posição>|tudo | chip importar");
    return;
  }
  if (cmd == "caixa") {                         // a memória das mensagens
    resto.toLowerCase();
    if (resto == "formatar") {
      Serial.println("Isto APAGA todas as mensagens da memória do Tijolo (as que estão no chip voltam).\n"
                     "Para confirmar: caixa formatar sim");
      return;
    }
    if (resto == "formatar sim") {
      Serial.println("Formatando a memória das mensagens (alguns segundos)...");
      if (!pastaFlash.formatar() || !caixa.begin(pastaFlash, CAIXA_MAXIMO)) {
        Serial.println("Não consegui. Confira board_build.partitions = default_16MB.csv no platformio.ini.");
        return;
      }
      caixaNaFlash = true;
      memoria.remove("caixa");                  // como na primeira vez: os SMS do chip voltam para a caixa
      Serial.println("Pronto: a memória das mensagens está vazia. Trazendo os SMS do chip...");
      importarDoChip(true);
      ui.caixaMudou(millis());
      return;
    }
    Serial.println("Uso: caixa formatar");
    return;
  }
  if (cmd == "ussd") {
    if (resto.length() == 0) { Serial.println("Uso: ussd *222#   (ou ussd 1 para responder um menu)"); return; }
    Serial.println("Perguntando à operadora (até 20 s)...");
    if (!modem.ussd(resto)) falhou("USSD");
    return;
  }
  if (cmd == "volume")  {
    if (modem.volume(resto.toInt())) { memoria.putUChar("volume", (uint8_t)resto.toInt()); Serial.println("ok"); }
    else falhou("volume");
    return;
  }
  if (cmd == "mic")     { if (modem.microfone(resto.toInt())) Serial.println("ok"); else falhou("microfone"); return; }
  if (cmd == "vivavoz") {
    bool liga = resto.startsWith("s") || resto == "on" || resto == "1";
    if (modem.vivaVoz(liga)) Serial.println(liga ? "Perfil viva-voz." : "Perfil fone no ouvido.");
    else falhou("vivavoz");
    return;
  }
  if (cmd == "toque") {
    if (resto == "instalar") {
      Serial.println("Gerando o trrrim e gravando no modem (uns 5 s)...");
      std::vector<uint8_t> wav = toque::gerarWav(VOLUME_TOQUE);
      if (!modem.gravarArquivo(ARQUIVO_TOQUE, wav.data(), wav.size())) { falhou("gravar o arquivo"); return; }
      if (!modem.definirToque(ARQUIVO_TOQUE)) { falhou("definir o toque"); return; }
      Serial.printf("Pronto: %u bytes em %s. Ligue para o chip para ouvir.\n", (unsigned)wav.size(), ARQUIVO_TOQUE);
    } else if (resto == "testar") {
      if (!modem.tocarArquivo(ARQUIVO_TOQUE)) falhou("tocar (rodou 'toque instalar'?)");
    } else {
      Serial.println("Uso: toque instalar | toque testar");
    }
    return;
  }
  if (cmd == "pin") {
    if (modem.senhaDoChip(resto)) Serial.println("PIN aceito."); else falhou("PIN (cuidado: 3 erros bloqueiam o chip)");
    return;
  }
  if (cmd == "teclado") {
    if (resto == "aprender") {
      ui.aprenderTeclado(millis());
      Serial.println("Siga a tela: aperte as teclas na ordem pedida (1, 2, 3... * 0 #).");
    } else if (resto == "teste") {
      mostrarTeclas = !mostrarTeclas;
      Serial.println(mostrarTeclas ? "Mostrando teclas e botões aqui (\"teclado teste\" de novo para parar)."
                                   : "Parei de mostrar as teclas.");
    } else {
      Serial.println("Uso: teclado aprender | teclado teste");
    }
    return;
  }
  if (cmd == "tela" && resto == "teste") { telaDeTeste(); return; }
  if (cmd == "tela" && resto == "reiniciar") {
    if (!temTela) { Serial.println("A tela não iniciou no começo: confira os fios e reinicie a placa."); return; }
    tela.reiniciar();
    ui.redesenhar();
    Serial.println("Tela reconfigurada. Se ela fica branca com frequência, veja \"Diagnóstico da tela\n"
                   "branca\" no README (e o comando tela diag).");
    return;
  }
  if (cmd == "tela" && resto == "diag") { diagTela(); return; }
  if (cmd == "tela" && resto.startsWith("rede")) {
    String v = resto.substring(4);
    v.trim();
    const int liga = ligarOuDesligar(v);
    if (liga < 0) { Serial.println("Uso: tela rede desligar | tela rede ligar"); return; }
    refazerNaTroca = liga;
    if (!liga) {
      refazerTelaEm = 0;
      escuroAteRefazer = false;
    }
    Serial.println(liga ? "A tela volta a ser refeita a cada troca de rede (e a apagar quando ninguém mexe)."
                        : "Modo de teste: a tela NÃO é refeita nas trocas de rede e fica SEMPRE ACESA, para o branco\n"
                          "aparecer (até reiniciar a placa ou 'tela rede ligar').\n"
                          "Para testar: at AT+CNMP=13 (só 2G) e veja se ela fica branca; 'tela diag' mostra se a\n"
                          "varredura do painel parou; 'tela reiniciar' traz a imagem; at AT+CNMP=2 volta ao normal.\n"
                          "Depois de cada troca, o monitor mostra a faixa em que o modem ficou.");
    return;
  }
  if (cmd == "tela" && resto == "faixa") { faixaDeTeste(); return; }
  if (cmd == "memoria" || cmd == "memória") {
    resto.toLowerCase();
    if (resto.length() == 0) { infoMemoria(); return; }
    if (resto != "psram" && !resto.startsWith("intern")) {
      Serial.println("Uso: memoria  |  memoria interna  |  memoria psram");
      return;
    }
    if (resto == "psram") memoria.putUChar("quadropsram", 1);
    else memoria.remove("quadropsram");
    Serial.printf("A imagem da tela vai para a %s. Reiniciando a placa...\n",
                  resto == "psram" ? "PSRAM (como era antes)" : "RAM interna, se couber");
    Serial.flush();
    delay(300);
    ESP.restart();
  }
  if (cmd == "tela" && resto.startsWith("detector")) {
    String v = resto.substring(8);
    v.trim();
    v.toLowerCase();
    const int liga = ligarOuDesligar(v);
    if (liga < 0) { Serial.println("Uso: tela detector desligar | tela detector ligar"); return; }
    if (!temTela) { Serial.println("A tela não iniciou."); return; }
    if (liga) memoria.remove("telalvd");
    else memoria.putUChar("telalvd", 1);
    tela.detectorDeTensao(liga);
    ui.redesenhar();
    Serial.println(liga ? "Detector de tensão baixa da tela ligado de novo (padrão de fábrica; a tela foi refeita)."
                        : "Detector de tensão baixa da tela DESLIGADO (fica guardado; 'tela detector ligar' volta).\n"
                          "Se as falhas do tipo \"conversor parou\" sumirem, era o detector desligando a tela nas quedas.");
    return;
  }
  if (cmd == "tela" && resto.startsWith("vigia")) {
    String v = resto.substring(5);
    v.trim();
    if (v.length() == 0 || !isdigit((unsigned char)v[0])) { Serial.println("Uso: tela vigia <ms>  (0 = desliga)"); return; }
    const long ms = v.toInt();
    tela.definirVigia(ms <= 0 ? 0 : (uint32_t)max(ms, 500L));
    if (tela.temLeitura())
      Serial.println("Com o fio MISO a vigia às cegas não é usada: o Tijolo lê o estado da tela.");
    else if (tela.vigiaMs())
      Serial.printf("Reenvio às cegas a cada %lu ms (até reiniciar a placa).\n", (unsigned long)tela.vigiaMs());
    else
      Serial.println("Reenvio às cegas desligado: se a tela ficar branca, ela fica (até 'tela reiniciar').");
    return;
  }
  if (cmd == "tela" && resto.startsWith("spi")) {
    String v = resto.substring(3);
    v.trim();
    v.toLowerCase();
    const long mhz = v.toInt();
    if (v == "padrao" || v == "padrão") {
      memoria.remove("telaspi");
      Serial.printf("SPI da tela volta ao do config.h (%lu MHz). Reiniciando a placa...\n",
                    (unsigned long)(TELA_SPI_HZ / 1000000));
    } else if (mhz >= 8 && mhz <= 80) {
      memoria.putUChar("telaspi", (uint8_t)mhz);
      Serial.printf("SPI da tela: pedido %ld MHz, roda a %.1f MHz (fica guardado; 'tela spi padrao' volta ao normal).\n"
                    "Reiniciando a placa...\n", mhz, mhzDeVerdade((uint32_t)mhz * 1000000UL));
    } else {
      Serial.println("Uso: tela spi <MHz>  (8 a 80; ex.: tela spi 10)  ou  tela spi padrao");
      return;
    }
    Serial.flush();
    delay(300);
    ESP.restart();
  }
  if (cmd == "tensao" || cmd == "tensão") {
    resto.toLowerCase();
    if (resto.startsWith("par") || resto == "0") {
      monitorAte = 0;
      Serial.println("Monitor de tensão parado.");
      return;
    }
    if (!medidor.ativo()) {
      Serial.printf("Sem medição do 3V3: falta o resistor de 10 kΩ entre J2-4 (GND) e J2-5 (IO2).\n"
                    "(O IO2 leu %d mV no começo; com o resistor, ~1650 mV. Depois de pôr, reinicie a placa.)\n",
                    medidor.leituraInicial());
      return;
    }
    const long s = resto.length() ? constrain(resto.toInt(), 5L, 600L) : 30L;
    monitorAte = (millis() + (uint32_t)s * 1000) | 1;
    proximaLinhaMonitor = millis();
    Serial.printf("Mostrando 3V3 e bateria a cada segundo por %ld s ('tensao parar' para antes).\n"
                  "mín/máx = a menor e a maior leitura do último segundo. Dá para usar os outros\n"
                  "comandos enquanto isso (radio desligar, radio ligar, ligar...).\n", s);
    return;
  }
  if (cmd == "radio" || cmd == "rádio") {
    resto.toLowerCase();
    const int liga = ligarOuDesligar(resto);
    if (liga < 0) { Serial.println("Uso: radio desligar | radio ligar"); return; }
    if (!modemOk) { Serial.println("O modem não respondeu no começo."); return; }
    Serial.println(liga ? "Ligando o rádio do modem (AT+CFUN=1); ele volta a procurar a rede..."
                        : "Desligando o rádio do modem (AT+CFUN=4): sem transmissão, sem chamadas...");
    if (!modem.comando(liga ? "AT+CFUN=1" : "AT+CFUN=4", nullptr, 15000)) { falhou("rádio"); return; }
    radioLigado = liga;
    Serial.println(liga ? "Rádio ligado." : "Rádio desligado. Lembre de religar: radio ligar");
    return;
  }
  if (cmd == "amplificador") {
    resto.toLowerCase();
    const int liga = ligarOuDesligar(resto);
    if (liga < 0) { Serial.println("Uso: amplificador desligar | amplificador ligar"); return; }
    if (!modem.amplificador(liga)) { falhou("amplificador"); return; }
    altoFalanteLigado = liga;
    Serial.println(liga ? "Amplificador do alto-falante ligado." : "Amplificador desligado (o alto-falante fica mudo).");
    return;
  }
  if (cmd == "brilho") {
    int b = constrain(resto.toInt(), 0, 255);
    brilhoMaximo = (uint8_t)b;
    ui.brilhoMaximo(brilhoMaximo);
    Serial.printf("Brilho máximo: %d (vale até reiniciar; o fixo fica no config.h).\n", b);
    return;
  }
  if (cmd == "dormir") { desligarAparelho(); return; }
  if (cmd == "at") {
    String r;
    bool ok = modem.comando(resto, &r, 10000);
    if (r.length()) Serial.println(r);
    Serial.println(ok ? "OK" : "ERRO: " + modem.ultimoErro());
    return;
  }
  if (cmd == "log") {
    bool liga = resto.startsWith("s") || resto == "on" || resto == "1";
    modem.espelharAt(liga ? &Serial : nullptr);
    Serial.println(liga ? "Tráfego AT visível (>> enviado, << recebido)." : "Tráfego AT escondido.");
    return;
  }
  if (cmd == "reiniciar") {
    Serial.println("Reiniciando o modem (até 45 s)...");
    Serial.println(modem.reiniciar() ? "Modem de volta." : "O modem não voltou.");
    return;
  }
  Serial.println("Não conheço \"" + cmd + "\". Digite ajuda.");
}

// Lê o teclado do computador sem travar o loop
void lerConsole() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' && ultimoFoiCR) { ultimoFoiCR = false; continue; }   // CR+LF conta uma vez
    ultimoFoiCR = (c == '\r');
    if (c == '\r' || c == '\n') {
      Serial.println();
      String l = linhaDigitada;
      linhaDigitada = "";
      executar(l);
      mostrarPrompt();
      continue;
    }
    if (c == 0x08 || c == 0x7F) {                 // Backspace (apaga um caractere UTF-8 inteiro)
      if (linhaDigitada.length() == 0) continue;
      int i = linhaDigitada.length() - 1;
      while (i > 0 && (((uint8_t)linhaDigitada[i]) & 0xC0) == 0x80) i--;
      linhaDigitada.remove(i);
      Serial.print("\b \b");
      continue;
    }
    if ((uint8_t)c < 0x20) continue;              // outros controles: ignora
    if (linhaDigitada.length() < 600) { linhaDigitada += c; Serial.print(c); }
  }
}

// ---------- Eventos do modem: vão para o monitor e para a tela ----------
void tratarEventos() {
  EventoModem ev;
  while (modem.proximoEvento(ev)) {
    const uint32_t agora = millis();
    switch (ev.tipo) {
      case EventoModem::ChamadaRecebida:
        // A chamada chega pelo 2G: se a troca apagou a tela, refaz já (em vez
        // de esperar o prazo) e ela acende mostrando quem liga.
        if (escuroAteRefazer) refazerTela("chamada chegando");
        avisar(">>> CHAMADA de " + (ev.texto.length() ? ev.texto : String("número privado")) + " às " +
               instante(millis()) + "   (atender | desligar)");
        ui.chamadaRecebida(ev.texto.c_str(), agora);
        break;
      case EventoModem::ChamadaConectada:
        avisar("--- Em ligação" + (ev.texto.length() ? " com " + ev.texto : String("")) +
               ".  (desligar | tons <dígitos> | volume <0-7>)");
        ui.chamadaConectada(ev.texto.c_str(), agora);
        break;
      case EventoModem::ChamadaEncerrada:
        avisar("--- Ligação encerrada" + (ev.texto.length() ? " (" + ev.texto + ")" : String("")) + ".");
        ui.chamadaEncerrada(ev.texto.c_str(), agora);
        break;
      case EventoModem::ChamadaPerdida:
        avisar("--- Chamada perdida de " + (ev.texto.length() ? ev.texto : String("número privado")) + ".");
        ui.chamadaPerdida(ev.texto.c_str(), agora);
        break;
      case EventoModem::SmsNovo: {
        Sms s;
        if (!modem.lerSms(ev.valor, s)) {
          const String& e = modem.ultimoErro();
          if (e == "posição vazia" || e.indexOf("321") >= 0) break;   // já foi para a caixa (ao ligar)
          avisar(">>> SMS novo na posição " + String(ev.valor) + ", mas não consegui ler (" + e +
                 "); tento de novo em 10 s.");
          importarPendente = true;                // a importação do chip pega ele
          importarEm = (millis() + 10000) | 1;
          break;
        }
        pdu::Montador::Pronta p;
        if (montador.adicionar(paraMensagem(s), s.indice, millis(), p)) {
          Serial.print("\r\n>>> SMS NOVO");
          mostrarMensagem(p.msg, listaDeIndices(p.indices), "", "");
          mostrarPrompt();
          entregarSms(p.msg, p.indices, "");
        } else {
          avisar(">>> Chegou a parte " + String(s.parte) + " de " + String(s.total) + " de um SMS longo de " +
                 s.numero + "; esperando o resto.");
        }
        break;
      }
      case EventoModem::ChipPronto:
        cache.chip = "";
        avisar("Chip pronto.");
        importarPendente = true;                  // os SMS que estiverem no chip (o modem pode ter
        importarEm = (millis() + 3000) | 1;       // reiniciado e recebido sem avisar), logo que der
        break;
      case EventoModem::ChipPedeSenha:
        cache.chip = "Chip pede PIN";
        avisar("O chip pede senha (" + ev.texto + "): digite pin <senha>.");
        break;
      case EventoModem::SemChip:
        cache.chip = "Sem chip";
        avisar("Chip não encontrado (" + ev.texto + "). Desligue a placa, encaixe o chip e ligue de novo.");
        break;
      case EventoModem::Ussd:
        avisar("Operadora: " + ev.texto + (ev.valor ? "\n(responda com: ussd <opção>)" : ""));
        ui.ussd(ev.texto.c_str(), ev.valor == 1, agora);
        break;
      case EventoModem::Aviso:
        avisar("Aviso do modem: " + ev.texto);
        ui.aviso(ev.texto.c_str(), agora);
        break;
      case EventoModem::RedeMudou:
        // A luz já apagou e o RESET já foi agendado (naHoraDoModem)
        cache.rede = ev.texto == "sem serviço" ? String("") : ev.texto;
        avisar("[rede: " + ev.texto + " às " + instante(ultimaTrocaEm) + "]");
        redeEm = (millis() + 3000) | 1;          // daqui a 3 s, em que faixa ficou (AT+CPSI?)
        redeTentativas = 5;
        break;
    }
  }
}

void mostrarTecla(const EventoTecla& ev) {
  String t;
  switch (ev.tipo) {
    case EventoTecla::Apertou: t = "apertou "; break;
    case EventoTecla::Segurou: t = "segurou "; break;
    case EventoTecla::Soltou: t = "soltou "; break;
    case EventoTecla::Par:
      avisar(String("fios ") + NOMES_FIOS[ev.a] + " + " + NOMES_FIOS[ev.b]);
      return;
    case EventoTecla::ParSegurado:
      avisar(String("fios ") + NOMES_FIOS[ev.a] + " + " + NOMES_FIOS[ev.b] + " (segurados)");
      return;
  }
  String nome;
  if (ev.tecla == TECLA_VERDE)
    nome = BOTAO_VERDE >= 0 ? String("verde") : String("verde (tecla ") + TECLA_COMO_VERDE + " segurada)";
  else if (ev.tecla == TECLA_VERMELHA)
    nome = BOTAO_VERMELHO >= 0 ? String("vermelho") : String("vermelho (tecla ") + TECLA_COMO_VERMELHO + " segurada)";
  else
    nome = String("tecla ") + ev.tecla;
  avisar(t + nome);
}

// ============================================================
void setup() {
  conferirTeclaDeLigar();   // acordou do sono? só segue se o 3 (ou o vermelho) continua apertado
  soltarPinos();            // fonte forte em HIGH antes de ligar o modem; luz apagada; CS em HIGH

  Serial.setTxBufferSize(4096);
  Serial.begin(115200);
  // Com a USB no computador e o monitor fechado, ninguém lê o que o Tijolo
  // escreve: sem este limite, cada aviso poderia travar a tela por 2 s.
  Serial.setTxTimeoutMs(5);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 1500) delay(10);   // dá tempo de abrir o monitor
  pinMode(PINO_SD_CS, OUTPUT);
  digitalWrite(PINO_SD_CS, HIGH);         // o cartão SD da placa fica fora da conversa
  analogReadResolution(12);
  memoria.begin("tijolo", false);
  // Medição do 3V3 para o diagnóstico (só com o resistor no IO2): começa
  // antes do modem, para pegar o que acontece quando ele liga.
  medidor.begin(PINO_MEDIR_3V3, PINO_BATERIA);

  Serial.println(F("\n=== Tijolo · Fase 2: tela, teclado e botões ==="));

  // Memória. A PSRAM pode perder os dados quando a alimentação oscila (o
  // modem e o alto-falante puxando corrente), sem a ESP32 reiniciar. Por
  // isso, a não ser com "memoria psram", nada vai para lá sem pedir: o
  // malloc usa a RAM interna (e só cai para a PSRAM se ela acabar) e a
  // imagem da tela (240 x 320 x 2 bytes = 150 KB) fica na RAM interna se
  // couber com folga. A sentinela vigia a PSRAM do mesmo jeito.
  const bool imagemNaPsram = memoria.getUChar("quadropsram", 0);
  // (SIZE_MAX desligaria a PSRAM de vez; com SIZE_MAX / 2, ela ainda é a reserva.)
  if (!imagemNaPsram) heap_caps_malloc_extmem_enable(SIZE_MAX / 2);
  uint16_t* pontos = nullptr;
  if (!imagemNaPsram &&
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) >= Quadro::BYTES + 1024 &&
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) >= Quadro::BYTES + 64 * 1024)
    pontos = (uint16_t*)heap_caps_malloc(Quadro::BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!pontos) pontos = (uint16_t*)ps_malloc(Quadro::BYTES);
  if (!pontos) pontos = (uint16_t*)malloc(Quadro::BYTES);
  quadroNaPsram = pontos && esp_ptr_external_ram(pontos);
  if (pontos)
    Serial.printf("Imagem da tela na %s (RAM interna livre: %lu KB).\n", quadroNaPsram ? "PSRAM" : "RAM interna",
                  (unsigned long)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) / 1024));
  sentinela.begin();
  if (pontos) {
    quadro.usar(pontos);
    uint8_t spiMhz = memoria.getUChar("telaspi", 0);         // comando "tela spi"
    if (spiMhz < 8 || spiMhz > 80) spiMhz = 0;
    ConfigTela ct{TELA_SCK, TELA_MOSI, TELA_CS, TELA_DC, TELA_RST, TELA_LUZ,
                  spiMhz ? spiMhz * 1000000UL : TELA_SPI_HZ, TELA_ROTACAO, TELA_INVERTER_CORES,
                  TELA_TROCAR_RGB, TELA_LUZ_HZ, TELA_VIGIA_MS, TELA_MISO, TELA_CONFERIR_MS};
    temTela = tela.begin(ct);
    if (temTela && memoria.getUChar("telalvd", 0)) {        // comando "tela detector"
      tela.detectorDeTensao(false);
      Serial.println("Detector de tensão baixa da tela: DESLIGADO (experimento; 'tela detector ligar' volta).");
    }
    if (temTela)
      Serial.printf("Tela: SPI a %.1f MHz · %s\n", mhzDeVerdade(tela.spiHz()),
                    tela.temLeitura() ? "leitura pelo fio MISO funcionando (tela diag)"
                                      : "sem leitura pelo MISO (vigia às cegas)");
  }
  if (medidor.ativo())
    Serial.printf("Medindo o 3V3 pelo IO2 (resistor encontrado: %d mV no pino).\n", medidor.leituraInicial());
  if (temTela) {
    Interface::desenharLigando(quadro, "Ligando o modem…");
    tela.enviar(quadro);
    tela.brilho(BRILHO_MAXIMO);
  } else {
    Serial.println(F("A tela não iniciou (sem memória ou sem resposta). O resto funciona pelo monitor."));
  }

  // Teclado e botões
  AcessoPinos acesso{pinoEntrada, pinoBaixo, pinoLer, esperarUs};
  teclado.begin(TECLADO_FIOS, 7, BOTAO_VERDE, BOTAO_VERMELHO, acesso);
  teclado.teclasComoBotoes(TECLA_COMO_VERDE, TECLA_COMO_VERMELHO);
  uint8_t mapa[Teclado::TECLAS];
  const bool temMapa = memoria.isKey("teclado") &&
                       memoria.getBytes("teclado", mapa, Teclado::TECLAS) == Teclado::TECLAS;
  if (temMapa) teclado.definirMapa(mapa);
  // Verde (o 1) segurado enquanto o Tijolo liga: aprende o teclado de novo
  bool verdeNoBoot = teclado.segurando(TECLA_VERDE);

  // Modem
  Serial.println(F("Ligando o modem (até 1 minuto na primeira vez)..."));
  PinosModem pinos{PINO_MODEM_RX, PINO_MODEM_TX, PINO_MODEM_PWRKEY, PINO_MODEM_DTR, PINO_MODEM_RING};
  modemOk = modem.begin(Serial1, MODEM_BAUD, pinos);
  modemLigadoEm = millis();
  if (temTela) {
    // O modem ligando puxa picos de corrente e começa a transmitir: é a
    // hora em que a tela mais se perde (fica branca).
    if (tela.temLeitura()) {
      // Confere já, e anota, se ela se perdeu enquanto o modem ligava.
      tela.conferirJa();
      for (uint32_t t0 = millis(); millis() - t0 < 250;) {
        tela.vigiar(quadro, millis());
        delay(1);
      }
    } else {
      tela.brilho(0);                     // sem leitura: refaz no escuro, por garantia
      tela.reiniciar();
    }
    Interface::desenharLigando(quadro, modemOk ? "Procurando a rede…" : "O modem não respondeu");
    tela.enviar(quadro);
    tela.brilho(BRILHO_MAXIMO);
  }
  modem.avisarNaHora(naHoraDoModem);      // daqui em diante, a troca de rede apaga a luz na hora
  verdeNoBoot = verdeNoBoot || teclado.segurando(TECLA_VERDE);
  if (modemOk) {
    // Um "radio desligar" de antes (o modem continua ligado quando a placa
    // reinicia) deixaria o Tijolo sem rede: confere e religa.
    String r;
    if (modem.comando("AT+CFUN?", &r, 3000) && r.indexOf(": 1") < 0) {
      Serial.println("O rádio do modem estava desligado (de um teste): religando.");
      modem.comando("AT+CFUN=1", nullptr, 15000);
    }
  }
  const int volume = memoria.getUChar("volume", VOLUME_PADRAO);
  if (!modemOk) {
    Serial.println(F(
      "\nO modem não respondeu. Confira:\n"
      "  - a placa está na USB marcada ESP-USB (a Modem-USB é só para atualizar o modem)\n"
      "  - com bateria: a chave ON/OFF da placa está em ON\n"
      "  - a fonte/porta USB aguenta picos de 2 A (use a bateria junto)\n"
      "Depois aperte RST na placa."));
  } else {
    Serial.println(F("Modem respondeu."));
    modem.gpioDoAmplificador(MODEM_GPIO_AMPLIFICADOR);
    if (!modem.amplificador(true)) Serial.println(F("Aviso: não liguei o amplificador do alto-falante."));
    modem.volume(volume);
    modem.microfone(MICROFONE_PADRAO);
    modem.vivaVoz(VIVA_VOZ_PADRAO);
    modem.prazoDasConsultas(1500);
    for (int i = 0; i < N_CONSULTAS; i++) {          // a tela inicial já começa completa
      consultar(i);
      consultas[i].ultima = millis();
      consultas[i].feita = true;
    }
  }

  ui.begin(telefone, volume, NOMES_FIOS, millis());
  ui.brilhoMaximo(brilhoMaximo);
  // A caixa de mensagens, na partição de dados da flash (na primeira vez,
  // ela é formatada: uns segundos a mais para ligar)
  const auto montagem = pastaFlash.begin("/caixa", [] {
    Serial.println(F("Preparando a memória das mensagens (só na primeira vez; pode levar alguns segundos)..."));
    if (temTela) {
      Interface::desenharLigando(quadro, "Preparando a memória das mensagens (só na primeira vez)…");
      tela.enviar(quadro);
    }
  });
  caixaNaFlash = montagem != PastaLittleFS::Montagem::NaoMontou && caixa.begin(pastaFlash, CAIXA_MAXIMO);
  if (caixaNaFlash) {
    Serial.printf("Mensagens na memória do Tijolo: %u (%u nova(s)).\n", (unsigned)caixa.total(), (unsigned)caixa.novas());
  } else {
    caixa.begin(pastaRam, CAIXA_MAXIMO);
    Serial.println(F("A memória das mensagens (LittleFS) não montou, e nada foi apagado. Por enquanto as mensagens\n"
                     "ficam na RAM e também no chip. Se acontecer sempre: 'caixa formatar' (apaga a memória delas)."));
  }
  ui.usarCaixa(caixa);
  ui.maximoDePartes(SMS_MAXIMO_DE_PARTES);
  ui.usarRelogio([]() -> uint32_t { return millis(); });
  if (!caixaNaFlash) ui.aviso("A memória das mensagens não montou: elas ficam no chip. Veja o monitor.", millis());
  // As dicas da tela mostram o 1 e o 3 quando eles fazem as vezes dos botões
  ui.teclasComoBotoes(BOTAO_VERDE < 0 ? TECLA_COMO_VERDE : 0, BOTAO_VERMELHO < 0 ? TECLA_COMO_VERMELHO : 0,
                      ("Para ligar de novo, " + comoLigar() + ".").c_str());
  ui.situacao(situacaoAtual(), millis());
  if (!modemOk) ui.aviso("O modem não respondeu. Veja o Monitor Serial.", millis());
  if (!temMapa || verdeNoBoot) ui.aprenderTeclado(millis());
  if (modemOk) importarDoChip(false);    // os SMS que estão no chip vão para a caixa
  else cache.bateria = porcentagemBateria(milivoltsBateria());

  if (modemOk) status();
  if (medidor.ativo()) {
    medidor.colher();                     // tudo desde o começo do setup numa janela só
    const Janela v = medidor.tensao3v3(100), b = medidor.bateria(100);
    if (v.n)
      Serial.printf("Desde que a placa ligou (modem ligando junto): 3V3 mín %s máx %s · bateria mín %s\n",
                    volts(v.minimo).c_str(), volts(v.maximo).c_str(), volts(b.minimo).c_str());
  }
  ajuda();
  tratarEventos();
  mostrarPrompt();
  // Quem ligou segurando o 3 (ou o vermelho) pode ainda estar segurando:
  // até soltar não vale, senão o "segurou" abriria a tela de desligar.
  teclado.ignorarAteSoltar();
  setupPronto = true;
}

uint32_t proximaColheita = 0;

void loop() {
  const uint32_t agora = millis();
  modem.loop();
  tratarEventos();
  if ((int32_t)(agora - proximaColheita) >= 0) {   // Medidor: uma janela a cada 100 ms
    proximaColheita = agora + 100;
    medidor.colher();
  }
  if (sentinela.conferir(agora)) relatarSentinela();

  teclado.varrer(agora);
  // O 1 e o 3 só dão evento ao soltar: tecla descida já conta como alguém
  // digitando (nada de consultas lentas ao modem no meio do toque). Até
  // 10 s: uma tecla emperrada não pode parar as consultas para sempre.
  static uint32_t apertadaDesde = 0;
  if (!teclado.algumaApertada()) apertadaDesde = 0;
  else if (!apertadaDesde) apertadaDesde = agora | 1;
  if (apertadaDesde && agora - apertadaDesde < 10000) ultimaTecla = agora;
  EventoTecla ev;
  while (teclado.proximoEvento(ev)) {
    ultimaTecla = agora;
    if (mostrarTeclas) mostrarTecla(ev);
    if (telaDeTesteAte) { telaDeTesteAte = 0; ui.redesenhar(); continue; }
    ui.tecla(ev, agora);
  }
  if (telaDeTesteAte && (int32_t)(agora - telaDeTesteAte) >= 0) { telaDeTesteAte = 0; ui.redesenhar(); }

  if (refazerTelaEm && (int32_t)(millis() - refazerTelaEm) >= 0) {
    refazerTelaEm = 0;
    refazerTela(motivoRefazer);
  }
  // Depois de uma troca de rede: em que tecnologia e faixa o modem ficou.
  // Enquanto ele ainda procura ("NO SERVICE"), pergunta de novo a cada 3 s.
  if (redeEm && modemOk && (int32_t)(millis() - redeEm) >= 0) {
    String r;
    const bool ok = modem.comando("AT+CPSI?", &r, 1500);
    if (ok && r.indexOf("NO SERVICE") >= 0 && --redeTentativas > 0) {
      redeEm = (millis() + 3000) | 1;
    } else {
      redeEm = 0;
      if (ok) {
        ultimaRede = r;
        avisar("[rede às " + instante(millis()) + ": " + descreverRede(r) + "]");
      }
    }
  }

  atualizarSituacao(agora);
  ui.situacao(situacaoAtual(), millis());
  ui.tique(millis());
  if (temTela && !telaDeTesteAte) {
    uint8_t luz = ui.brilho(millis());
    if (!refazerNaTroca) luz = brilhoMaximo;   // teste da troca de rede: sempre acesa, para ver o branco
    if (escuroAteRefazer) luz = 0;       // troca de rede: apagada até o RESET agendado
    if (luz == 0 && !telaApagada) {
      tela.brilho(0);
      // Aproveita o escuro para refazer a tela por completo (pulso no
      // RESET, ~0,4 s): sem o fio MISO não dá para saber se ela está bem, e
      // com ele o painel pode travar branco sem o controlador perceber.
      // "tela vigia 0" (sem o fio) desliga também isto, para contar as falhas a olho.
      // (Na troca de rede o RESET já está agendado: não precisa de dois.)
      if (!escuroAteRefazer) tela.aoApagar();
    } else if (luz > 0 && telaApagada) {
      // Acordando: reconfirma a configuração antes de acender (às cegas) ou
      // confere o estado na hora (com o fio MISO).
      tela.aoAcender();
      ui.redesenhar();
    }
    telaApagada = (luz == 0);
    if (ui.precisaDesenhar(millis())) {
      ui.desenhar(quadro, millis());
      tela.enviar(quadro);
    }
    tela.vigiar(quadro, millis());     // tela branca de repente volta sozinha
    tela.brilho(luz);
  } else if (!temTela && ui.precisaDesenhar(millis())) {
    ui.desenhado(millis());   // sem tela, ligar e USSD não podem ficar esperando um desenho
  }
  if (ui.pediuDesligar()) desligarAparelho();

  OcorrenciaTela ocorrencia;             // a tela se perdeu (só com o fio MISO)
  while (tela.proximaOcorrencia(ocorrencia)) relatarOcorrencia(ocorrencia);
  if (monitorAte) {                      // comando "tensao"
    if ((int32_t)(millis() - monitorAte) >= 0) {
      monitorAte = 0;
      avisar("Fim do monitor de tensão.");
    } else if ((int32_t)(millis() - proximaLinhaMonitor) >= 0) {
      proximaLinhaMonitor = millis() + 1000;
      linhaDoMonitor();
    }
  }

  // Tarefas demoradas com o SMS: só fora de chamada e sem ninguém digitando.
  const bool folga = modemOk && modem.estado() == EstadoChamada::Livre && millis() - ultimaTecla > 3000;
  if (folga && importarPendente && importarEm && (int32_t)(millis() - importarEm) >= 0) {
    importarEm = 0;
    importarDoChip(false);
  }

  lerConsole();

  // SMS longo com parte faltando há 3 min: mostra o que chegou
  pdu::Montador::Pronta p;
  if (montador.expirada(millis(), ESPERA_PARTES_MS, p)) {
    Serial.print("\r\n>>> SMS (faltou parte)");
    mostrarMensagem(p.msg, listaDeIndices(p.indices), "", "incompleto");
    mostrarPrompt();
    entregarSms(p.msg, p.indices, "faltou uma parte");
  }
  delay(1);
}
