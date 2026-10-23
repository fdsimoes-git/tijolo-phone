// Testa o lib/Modem no computador contra um modem A7670 de mentira.
// Da raiz do projeto:
//   g++ -std=c++17 -I test_host/shim -I lib/Texto -I lib/Pdu -I lib/Modem
//       test_host/modem_sim_test.cpp lib/Modem/Modem.cpp -o /tmp/sim && /tmp/sim
#include <Arduino.h>
#include <Modem.h>
#include <Pdu.h>
#include <Texto.h>
#include <deque>
#include <map>
#include <vector>

// ---------- relógio simulado ----------
static uint32_t g_agora = 0;
struct FakeModem;
static FakeModem* g_fake = nullptr;
static void entregarAgendados();
uint32_t millis() { return g_agora; }
void delay(uint32_t ms) { g_agora += ms; entregarAgendados(); }

// ---------- modem falso ----------
struct FakeModem : public HardwareSerial {
  enum Modo { LINHA, CORPO, BRUTO } modo = LINHA;
  std::deque<char> rx;                               // o que a ESP vai ler
  std::vector<std::pair<uint32_t, std::string>> agenda;
  std::string cmd, corpo;
  size_t brutoFalta = 0, arquivoTotal = 0, perderBytes = 0;
  std::vector<std::string> recebidos;                // comandos e corpos que chegaram
  std::map<std::string, std::string> respostas;      // comando exato → texto
  std::function<bool(const std::string&)> especial;  // devolve true se tratou
  std::string clcc;                                  // resposta atual do AT+CLCC
  bool esperandoTextoAntigo = false;                 // ficou no "> " de um SMS antigo

  void diz(const std::string& t, uint32_t daquiMs = 0) { agenda.push_back({g_agora + daquiMs, t}); entregar(); }
  void entregar() {
    for (size_t i = 0; i < agenda.size();) {
      if (agenda[i].first <= g_agora) { for (char c : agenda[i].second) rx.push_back(c); agenda.erase(agenda.begin() + i); }
      else i++;
    }
  }
  int available() override { entregar(); return (int)rx.size(); }
  int read() override { entregar(); if (rx.empty()) return -1; char c = rx.front(); rx.pop_front(); return (uint8_t)c; }
  size_t write(uint8_t b) override {
    if (esperandoTextoAntigo) {                      // tudo vira texto até ESC ou Ctrl+Z
      if (b == 0x1B) { esperandoTextoAntigo = false; diz("\r\nOK\r\n", 5); }
      return 1;
    }
    if (modo == BRUTO) {
      if (perderBytes) { perderBytes--; brutoFalta--; }   // simula bytes perdidos
      else { corpo += (char)b; brutoFalta--; }
      if (brutoFalta == 0) {
        arquivoTotal += corpo.size();
        recebidos.push_back("<bruto " + std::to_string(corpo.size()) + " bytes>");
        modo = LINHA;
        diz("\r\n+FSWRITE: 0," + std::to_string(arquivoTotal) + "\r\n\r\nOK\r\n", 30);
      }
      return 1;
    }
    if (modo == CORPO) {
      if (b == 0x1A) {
        recebidos.push_back("<corpo>" + corpo);
        modo = LINHA;
        diz("\r\n+CMGS: 17\r\n\r\nOK\r\n", 200);
      } else if (b == 0x1B) { modo = LINHA; diz("\r\nOK\r\n"); }
      else corpo += (char)b;
      return 1;
    }
    if (b == '\r') { std::string c = cmd; cmd.clear(); comando(c); }
    else if (b != '\n') cmd += (char)b;
    return 1;
  }
  void comando(const std::string& c) {
    recebidos.push_back(c);
    if (especial && especial(c)) return;
    if (c.rfind("AT+CMGS=", 0) == 0) { modo = CORPO; corpo.clear(); diz("\r\n> ", 20); return; }
    if (c.rfind("AT+FSOPEN=", 0) == 0) { arquivoTotal = 0; }
    if (c.rfind("AT+FSWRITE=", 0) == 0) {
      auto f = texto::campos(c.substr(11));
      brutoFalta = (size_t)atoi(f[1].c_str()); corpo.clear(); modo = BRUTO;
      diz("\r\nCONNECT\r\n", 10);
      return;
    }
    if (c == "AT+CLCC") { diz(clcc + "\r\nOK\r\n", 5); return; }
    auto it = respostas.find(c);
    if (it != respostas.end()) { diz(it->second, 5); return; }
    diz("\r\nOK\r\n", 5);
  }
  bool recebeu(const std::string& c) const { for (auto& r : recebidos) if (r == c) return true; return false; }
  int indice(const std::string& c) const { for (size_t i = 0; i < recebidos.size(); i++) if (recebidos[i] == c) return (int)i; return -1; }
  int quantos(const std::string& prefixo) const { int n = 0; for (auto& r : recebidos) if (r.rfind(prefixo, 0) == 0) n++; return n; }
};
static void entregarAgendados() { if (g_fake) g_fake->entregar(); }

// ---------- utilidades do teste ----------
static int falhas = 0, verificacoes = 0;
#define CONFERE(c) do { verificacoes++; if (!(c)) { printf("  FALHOU (linha %d): %s\n", __LINE__, #c); falhas++; } } while (0)

static std::vector<EventoModem> coletar(Modem& m, uint32_t ms = 50) {
  std::vector<EventoModem> v;
  for (uint32_t t = 0; t < ms; t += 5) { m.loop(); delay(5); }
  EventoModem ev;
  while (m.proximoEvento(ev)) v.push_back(ev);
  return v;
}
static int contar(const std::vector<EventoModem>& v, EventoModem::Tipo t) {
  int n = 0;
  for (auto& e : v) if (e.tipo == t) n++;
  return n;
}
static const EventoModem* achar(const std::vector<EventoModem>& v, EventoModem::Tipo t) {
  for (auto& e : v) if (e.tipo == t) return &e;
  return nullptr;
}

static PinosModem PINOS{5, 4, 46, 7, 6};

static void novoModem(FakeModem& f, Modem& m) {
  g_fake = &f;
  f.respostas["AT+CPIN?"] = "\r\n+CPIN: READY\r\n\r\nOK\r\n";
  bool ok = m.begin(f, 115200, PINOS);
  CONFERE(ok);
  EventoModem ev;
  while (m.proximoEvento(ev)) {}
  f.recebidos.clear();
}

// PDU de um SMS recebido (SMS-DELIVER), montado a partir do nosso PDU de envio
static std::vector<std::string> pdusRecebidos(const std::string& de, const std::string& txt, uint8_t ref) {
  std::vector<std::string> out;
  for (auto& e : pdu::codificar(de, txt, ref)) {
    std::vector<uint8_t> b;
    pdu::hexParaBytes(e.hex, b);
    // SUBMIT: 00 fo mr [da] pid dcs vp udl ud  →  DELIVER: 00 fo' [oa] pid dcs scts udl ud
    size_t i = 1;
    uint8_t fo = b[i++];
    i++;                                             // mr
    size_t nDa = 2 + (b[i] + 1) / 2;
    std::vector<uint8_t> d = {0x00, (uint8_t)(0x04 | (fo & 0x40))};
    d.insert(d.end(), b.begin() + i, b.begin() + i + nDa);
    i += nDa;
    d.push_back(b[i++]);                             // pid
    d.push_back(b[i++]);                             // dcs
    i++;                                             // vp
    for (uint8_t x : {0x62, 0x01, 0x12, 0x81, 0x50, 0x21, 0x29}) d.push_back(x);   // 26/10/21 18:05:12 -12
    d.insert(d.end(), b.begin() + i, b.end());
    out.push_back(pdu::bytesParaHex(d));
  }
  return out;
}

int main() {
  // ===== 1. begin com o modem já ligado =====
  printf("1. begin (modem já ligado)\n");
  {
    FakeModem f; Modem m; g_fake = &f;
    f.respostas["AT+CPIN?"] = "\r\n+CPIN: READY\r\n\r\nOK\r\n";
    CONFERE(m.begin(f, 115200, PINOS));
    CONFERE(m.chipPronto());
    CONFERE(f.recebeu("ATE0"));
    CONFERE(f.recebeu("AT+CLCC=1"));
    CONFERE(f.recebeu("AT+CMGF=0"));
    CONFERE(f.recebeu("AT+CNMI=2,1,0,0,0"));
    CONFERE(f.recebeu("AT+CPMS=\"SM\",\"SM\",\"SM\""));
  }

  // ===== 2. begin com o modem desligado: precisa do pulso e espera SMS DONE =====
  printf("2. begin (modem desligado)\n");
  {
    FakeModem f; Modem m; g_fake = &f;
    uint32_t ligaEm = g_agora + 5000;                // só responde depois de 5 s
    f.especial = [&](const std::string& c) {
      if (g_agora < ligaEm) return true;             // mudo
      if (c == "AT" && !f.recebeu("ATE0")) {
        f.diz("\r\nOK\r\n\r\n*ATREADY: 1\r\n\r\n+CPIN: READY\r\n\r\nSMS DONE\r\n\r\nPB DONE\r\n", 5);
        return true;
      }
      return false;
    };
    f.respostas["AT+CPIN?"] = "\r\n+CPIN: READY\r\n\r\nOK\r\n";
    uint32_t t0 = g_agora;
    CONFERE(m.begin(f, 115200, PINOS));
    CONFERE(g_agora - t0 < 9000);                    // não ficou esperando 15 s à toa
    CONFERE(m.chipPronto());
    auto ev = coletar(m, 100);
    CONFERE(contar(ev, EventoModem::Aviso) == 0);    // *ATREADY do boot não é "reiniciou sozinho"
  }

  // ===== 2b. ESP reiniciou no meio de um SMS: o modem está no "> " =====
  printf("2b. modem preso esperando texto de SMS\n");
  {
    FakeModem f; Modem m; g_fake = &f;
    f.esperandoTextoAntigo = true;
    f.respostas["AT+CPIN?"] = "\r\n+CPIN: READY\r\n\r\nOK\r\n";
    uint32_t t0 = g_agora;
    CONFERE(m.begin(f, 115200, PINOS));
    CONFERE(g_agora - t0 < 3000);
  }

  // ===== 3. chamada recebida, atendida e encerrada pelo outro lado =====
  printf("3. chamada recebida → atender → outro lado desliga\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    f.clcc = "\r\n+CLCC: 1,1,4,0,0,\"+5511999998888\",145";
    f.diz("\r\nRING\r\n\r\n+CLIP: \"+5511999998888\",145,,,,0\r\n\r\n+CLCC: 1,1,4,0,0,\"+5511999998888\",145\r\n");
    auto ev = coletar(m, 200);
    CONFERE(contar(ev, EventoModem::ChamadaRecebida) == 1);
    CONFERE(achar(ev, EventoModem::ChamadaRecebida) && achar(ev, EventoModem::ChamadaRecebida)->texto == "+5511999998888");
    CONFERE(m.estado() == EstadoChamada::Tocando);
    f.diz("\r\nRING\r\n\r\n+CLIP: \"+5511999998888\",145,,,,0\r\n", 6000);
    ev = coletar(m, 6500);
    CONFERE(contar(ev, EventoModem::ChamadaRecebida) == 0);   // não avisa duas vezes
    CONFERE(m.estado() == EstadoChamada::Tocando);

    f.especial = [&](const std::string& c) {
      if (c == "ATA") {
        f.clcc = "\r\n+CLCC: 1,1,0,0,0,\"+5511999998888\",145";
        f.diz("\r\nOK\r\n\r\nVOICE CALL: BEGIN\r\n\r\n+CLCC: 1,1,0,0,0,\"+5511999998888\",145\r\n", 50);
        return true;
      }
      return false;
    };
    CONFERE(m.atender());
    ev = coletar(m, 3000);
    CONFERE(m.estado() == EstadoChamada::EmLigacao);
    CONFERE(contar(ev, EventoModem::ChamadaConectada) == 1);
    ev = coletar(m, 42000);                          // 42 s de conversa
    CONFERE(m.estado() == EstadoChamada::EmLigacao);
    f.clcc = "";
    f.diz("\r\n+CLCC: 1,1,6,0,0,\"+5511999998888\",145\r\n\r\nVOICE CALL: END: 000045\r\n\r\nNO CARRIER\r\n");
    ev = coletar(m, 1000);
    CONFERE(m.estado() == EstadoChamada::Livre);
    CONFERE(contar(ev, EventoModem::ChamadaEncerrada) == 1);
    CONFERE(contar(ev, EventoModem::ChamadaPerdida) == 0);
    auto e = achar(ev, EventoModem::ChamadaEncerrada);
    CONFERE(e && e->texto.indexOf("duração 0:4") >= 0);
    if (e) printf("   encerrada: \"%s\"\n", e->texto.c_str());
  }

  // ===== 3b. segunda chamada durante a ligação (chamada em espera) =====
  printf("3b. chamada em espera não derruba a ligação\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    f.clcc = "\r\n+CLCC: 1,1,4,0,0,\"+5511999998888\",145";
    f.diz("\r\n+CLCC: 1,1,4,0,0,\"+5511999998888\",145\r\n");
    coletar(m, 100);
    f.especial = [&](const std::string& c) {
      if (c == "ATA") {
        f.clcc = "\r\n+CLCC: 1,1,0,0,0,\"+5511999998888\",145";
        f.diz("\r\nOK\r\n\r\n+CLCC: 1,1,0,0,0,\"+5511999998888\",145\r\n", 50);
        return true;
      }
      return false;
    };
    CONFERE(m.atender());
    coletar(m, 500);
    // chega a chamada 2, espera e desiste
    f.clcc = "\r\n+CLCC: 1,1,0,0,0,\"+5511999998888\",145\r\n+CLCC: 2,1,5,0,0,\"+5521988887777\",145";
    f.diz("\r\n+CLCC: 1,1,0,0,0,\"+5511999998888\",145\r\n+CLCC: 2,1,5,0,0,\"+5521988887777\",145\r\n");
    auto ev = coletar(m, 3000);
    CONFERE(contar(ev, EventoModem::Aviso) == 1);
    f.clcc = "\r\n+CLCC: 1,1,0,0,0,\"+5511999998888\",145";
    f.diz("\r\n+CLCC: 2,1,6,0,0,\"+5521988887777\",145\r\n\r\nVOICE CALL: END: 000000\r\n");
    ev = coletar(m, 3000);
    CONFERE(m.estado() == EstadoChamada::EmLigacao);
    CONFERE(contar(ev, EventoModem::ChamadaEncerrada) == 0);
    // desligar continua funcionando
    f.especial = [&](const std::string& c) {
      if (c == "AT+CHUP") { f.clcc = ""; f.diz("\r\nVOICE CALL: END: 000004\r\n\r\nOK\r\n", 20); return true; }
      return false;
    };
    CONFERE(m.desligar());
    ev = coletar(m, 200);
    CONFERE(m.estado() == EstadoChamada::Livre);
    CONFERE(contar(ev, EventoModem::ChamadaEncerrada) == 1);
    // sem chamada, desligar ainda manda AT+CHUP e responde OK
    f.recebidos.clear();
    CONFERE(m.desligar());
    CONFERE(f.recebeu("AT+CHUP"));
  }

  // ===== 4. chamada perdida (quem ligou desistiu) =====
  printf("4. chamada perdida\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    f.clcc = "\r\n+CLCC: 1,1,4,0,0,\"11988887777\",129";
    f.diz("\r\nRING\r\n\r\n+CLIP: \"11988887777\",129,,,,0\r\n");
    auto ev = coletar(m, 100);
    CONFERE(contar(ev, EventoModem::ChamadaRecebida) == 1);
    f.clcc = "";
    f.diz("\r\n+CLCC: 1,1,6,0,0,\"11988887777\",129\r\n\r\nMISSED_CALL: 09:42PM 11988887777\r\n", 3000);
    ev = coletar(m, 4000);
    CONFERE(m.estado() == EstadoChamada::Livre);
    CONFERE(contar(ev, EventoModem::ChamadaPerdida) == 1);
    CONFERE(contar(ev, EventoModem::ChamadaEncerrada) == 0);
  }

  // ===== 5. recusar: o aviso de fim chega ANTES do OK do AT+CHUP =====
  printf("5. recusar chamada (corrida entre aviso e OK)\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    f.clcc = "\r\n+CLCC: 1,1,4,0,0,\"+5511999998888\",145";
    f.diz("\r\nRING\r\n\r\n+CLIP: \"+5511999998888\",145,,,,0\r\n");
    auto ev = coletar(m, 100);
    f.especial = [&](const std::string& c) {
      if (c == "AT+CHUP") {
        f.clcc = "";
        f.diz("\r\n+CLCC: 1,1,6,0,0,\"+5511999998888\",145\r\n\r\nVOICE CALL: END: 000000\r\n\r\nOK\r\n", 20);
        return true;
      }
      return false;
    };
    CONFERE(m.desligar());
    ev = coletar(m, 500);
    CONFERE(m.estado() == EstadoChamada::Livre);
    CONFERE(contar(ev, EventoModem::ChamadaPerdida) == 0);
    CONFERE(contar(ev, EventoModem::ChamadaEncerrada) == 1);
    auto e = achar(ev, EventoModem::ChamadaEncerrada);
    CONFERE(e && e->texto == "recusada");
  }

  // ===== 6. ligar, chamar, atender do outro lado, eu desligo =====
  printf("6. ligar → atendem → desligar\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    f.especial = [&](const std::string& c) {
      if (c == "ATD+5511999998888;") {
        f.clcc = "\r\n+CLCC: 1,0,2,0,0,\"+5511999998888\",145";
        f.diz("\r\nOK\r\n\r\n+CLCC: 1,0,2,0,0,\"+5511999998888\",145\r\n", 30);
        f.diz("\r\n+CLCC: 1,0,3,0,0,\"+5511999998888\",145\r\n", 1500);
        f.diz("\r\nVOICE CALL: BEGIN\r\n\r\n+CLCC: 1,0,0,0,0,\"+5511999998888\",145\r\n", 6000);
        return true;
      }
      if (c == "AT+CHUP") { f.clcc = ""; f.diz("\r\nVOICE CALL: END: 000010\r\n\r\nOK\r\n", 20); return true; }
      return false;
    };
    CONFERE(m.ligar("+5511999998888"));
    CONFERE(m.estado() == EstadoChamada::Discando);
    auto ev = coletar(m, 2000);
    CONFERE(m.estado() == EstadoChamada::Chamando);
    f.clcc = "\r\n+CLCC: 1,0,0,0,0,\"+5511999998888\",145";
    ev = coletar(m, 5000);
    CONFERE(m.estado() == EstadoChamada::EmLigacao);
    CONFERE(contar(ev, EventoModem::ChamadaConectada) == 1);
    delay(10000);
    CONFERE(m.desligar());
    ev = coletar(m, 200);
    CONFERE(m.estado() == EstadoChamada::Livre);
    auto e = achar(ev, EventoModem::ChamadaEncerrada);
    CONFERE(e && e->texto.startsWith("você desligou"));
    if (e) printf("   encerrada: \"%s\"\n", e->texto.c_str());
  }

  // ===== 6b. discagem que falha na hora (NO CARRIER / BUSY no lugar do OK) =====
  printf("6b. discagem recusada pela rede\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    f.respostas["ATD11999998888;"] = "\r\nNO CARRIER\r\n";
    f.respostas["ATD11977776666;"] = "\r\nBUSY\r\n";
    uint32_t t0 = g_agora;
    CONFERE(!m.ligar("11999998888"));
    CONFERE(g_agora - t0 < 1000);                    // não esperou 20 s
    CONFERE(m.ultimoErro().indexOf("NO CARRIER") >= 0);
    CONFERE(m.estado() == EstadoChamada::Livre);
    CONFERE(!m.ligar("11977776666"));
    CONFERE(m.ultimoErro() == "ocupado");
    // e depois de um OK, ocupado chega como aviso
    f.especial = [&](const std::string& c) {
      if (c == "ATD11955554444;") {
        f.clcc = "\r\n+CLCC: 1,0,2,0,0,\"11955554444\",129";
        f.diz("\r\nOK\r\n", 20);
        f.diz("\r\nBUSY\r\n", 1500);
        return true;
      }
      return false;
    };
    CONFERE(m.ligar("11955554444"));
    coletar(m, 1000);
    f.clcc = "";
    auto ev = coletar(m, 3000);
    CONFERE(m.estado() == EstadoChamada::Livre);
    auto e = achar(ev, EventoModem::ChamadaEncerrada);
    CONFERE(e && e->texto == "ocupado");
    if (e) printf("   encerrada: \"%s\"\n", e->texto.c_str());
  }

  // ===== 7. a outra pessoa nunca atende; a rede derruba (sem aviso!) =====
  printf("7. chamada some sem aviso: a consulta AT+CLCC percebe\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    f.especial = [&](const std::string& c) {
      if (c == "ATD11999998888;") { f.clcc = "\r\n+CLCC: 1,0,3,0,0,\"11999998888\",129"; f.diz("\r\nOK\r\n", 20); return true; }
      return false;
    };
    CONFERE(m.ligar("11999998888"));
    auto ev = coletar(m, 3000);
    CONFERE(m.estado() == EstadoChamada::Chamando);
    f.clcc = "";                                     // sumiu
    ev = coletar(m, 3000);
    CONFERE(m.estado() == EstadoChamada::Livre);
    auto e = achar(ev, EventoModem::ChamadaEncerrada);
    CONFERE(e && e->texto == "não completou");
  }

  // ===== 8. SMS chegando (PDU) com acentos e emoji =====
  printf("8. SMS recebido\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    auto pdus = pdusRecebidos("+5511999998888", "Olá! Tudo certo? Pão de açúcar 👍", 0);
    f.respostas["AT+CMGR=3"] = "\r\n+CMGR: 0,,40\r\n" + pdus[0] + "\r\n\r\nOK\r\n";
    f.diz("\r\n+CMTI: \"SM\",3\r\n");
    auto ev = coletar(m, 50);
    auto e = achar(ev, EventoModem::SmsNovo);
    CONFERE(e && e->valor == 3);
    Sms s;
    CONFERE(m.lerSms(3, s));
    CONFERE(s.numero == "+5511999998888");
    CONFERE(s.texto == "Olá! Tudo certo? Pão de açúcar 👍");
    CONFERE(s.dataHora == "26/10/21,18:05:12-12");
    CONFERE(s.estado == "REC UNREAD");
    CONFERE(!f.recebeu("AT+CSCS=\"UCS2\""));          // modo PDU: sem trocar de charset
    printf("   %s: %s\n", s.numero.c_str(), s.texto.c_str());
  }

  // ===== 9. enviar SMS simples (GSM, 160 caracteres) =====
  printf("9. SMS sem acentos\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    int partes = 0;
    CONFERE(m.enviarSms("+5511999998888", "Oi, chego as 8h. Ok?", &partes));
    CONFERE(partes == 1);
    auto esperado = pdu::codificar("+5511999998888", "Oi, chego as 8h. Ok?", 1);
    CONFERE(f.recebeu("AT+CMGS=" + std::to_string(esperado[0].tamanho)));
    // a referência é aleatória, mas sem SMS longo ela nem entra no PDU
    CONFERE(f.recebeu("<corpo>" + esperado[0].hex));
  }

  // ===== 10. enviar SMS com acentos e emoji (UCS2) =====
  printf("10. SMS com acentos\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    CONFERE(m.enviarSms("+5511999998888", "Já cheguei, até já! 😀"));
    auto esperado = pdu::codificar("+5511999998888", "Já cheguei, até já! 😀", 1);
    CONFERE(f.recebeu("<corpo>" + esperado[0].hex));
    CONFERE(!f.recebeu("AT+CSCS=\"UCS2\""));
  }

  // ===== 11. SMS longo em partes =====
  printf("11. SMS longo (200 letras = 2 partes)\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    int partes = 0;
    CONFERE(m.enviarSms("+5511999998888", String(std::string(200, 'a')), &partes));
    CONFERE(partes == 2);
    CONFERE(f.quantos("AT+CMGS=") == 2);
    // o que saiu decodifica de volta, nas duas partes, com a mesma referência
    std::string junto;
    int ref = -1;
    for (auto& r : f.recebidos) {
      if (r.rfind("<corpo>", 0) != 0) continue;
      pdu::Mensagem msg;
      CONFERE(pdu::decodificar(r.substr(7), msg));
      CONFERE(msg.total == 2);
      if (ref < 0) ref = msg.ref; else CONFERE(msg.ref == ref);
      junto += msg.texto;
    }
    CONFERE(junto == std::string(200, 'a'));
  }

  // ===== 12. listar SMS (com um longo em duas partes) =====
  printf("12. lista de SMS\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    auto um = pdusRecebidos("+5511999998888", "primeira", 0);
    std::string longo;
    for (int i = 0; i < 4; i++) longo += "Recarga de R$ 20,00 confirmada. Obrigado por usar a nossa operadora! ";
    auto dois = pdusRecebidos("27888", longo, 33);
    CONFERE(dois.size() == 2);
    f.respostas["AT+CMGL=4"] =
      "\r\n+CMGL: 1,1,,30\r\n" + um[0] +
      "\r\n+CMGL: 4,0,,140\r\n" + dois[1] +
      "\r\n+CMGL: 5,0,,140\r\n" + dois[0] + "\r\n\r\nOK\r\n";
    std::vector<Sms> l;
    CONFERE(m.listarSms(l));
    CONFERE(l.size() == 3);
    if (l.size() == 3) {
      CONFERE(l[0].indice == 1 && l[0].texto == "primeira" && l[0].numero == "+5511999998888" && l[0].estado == "REC READ");
      CONFERE(l[1].indice == 4 && l[1].numero == "27888" && l[1].parte == 2 && l[1].total == 2 && l[1].ref == 33);
      CONFERE(l[2].parte == 1);
      CONFERE(l[2].texto + l[1].texto == longo);
    }
  }

  // ===== 13. USSD: resposta chega 3 s depois do OK =====
  printf("13. USSD\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    std::string pedido = "AT+CUSD=1,\"" + texto::utf8ParaUcs2Hex("*222#") + "\",15";
    f.especial = [&](const std::string& c) {
      if (c == pedido) {
        f.diz("\r\nOK\r\n", 10);
        f.diz("\r\n+CUSD: 0,\"" + texto::utf8ParaUcs2Hex("Seu saldo é R$ 5,00. Válido até 30/10.") + "\",72\r\n", 3000);
        return true;
      }
      return false;
    };
    uint32_t t00 = g_agora;
    CONFERE(m.ussd("*222#"));
    CONFERE(g_agora - t00 < 1000);                   // não fica esperando a operadora
    CONFERE(m.esperandoUssd());
    CONFERE(f.recebeu(pedido));
    CONFERE(f.recebeu("AT+CSCS=\"UCS2\""));
    CONFERE(!f.recebeu("AT+CSCS=\"IRA\""));        // continua em UCS2 até a resposta
    auto ev = coletar(m, 4000);
    auto e = achar(ev, EventoModem::Ussd);
    CONFERE(e && e->texto == "Seu saldo é R$ 5,00. Válido até 30/10." && e->valor == 0);
    CONFERE(!m.esperandoUssd());
    CONFERE(f.recebidos.back() == "AT+CSCS=\"IRA\"");
    f.especial = [&](const std::string& c) {
      if (c.rfind("AT+CUSD=1,", 0) == 0) {
        f.diz("\r\n+CUSD: 1,\"" + texto::utf8ParaUcs2Hex("1-Saldo\n2-Recarga") + "\",72\r\n\r\nOK\r\n", 10);
        return true;
      }
      return false;
    };
    uint32_t t0 = g_agora;
    CONFERE(m.ussd("*222#"));
    CONFERE(g_agora - t0 < 2000);                    // não esperou 20 s à toa
    ev = coletar(m, 20);
    e = achar(ev, EventoModem::Ussd);
    CONFERE(e && e->valor == 1 && e->texto == "1-Saldo\n2-Recarga");
    CONFERE(!m.esperandoUssd());
    // A operadora não responde: depois de 25 s vem o aviso e o modem volta para IRA.
    f.especial = [&](const std::string& c) {
      if (c.rfind("AT+CUSD=1,", 0) == 0) { f.diz("\r\nOK\r\n", 10); return true; }
      return false;
    };
    f.recebidos.clear();
    CONFERE(m.ussd("*100#"));
    ev = coletar(m, 10000);
    CONFERE(!achar(ev, EventoModem::Ussd));
    ev = coletar(m, 16000);
    e = achar(ev, EventoModem::Ussd);
    CONFERE(e && e->texto == "sem resposta da operadora");
    CONFERE(f.recebeu("AT+CSCS=\"IRA\""));
    // Cancelar no meio
    f.recebidos.clear();
    CONFERE(m.ussd("*100#"));
    CONFERE(m.cancelarUssd());
    CONFERE(!m.esperandoUssd());
    CONFERE(f.recebeu("AT+CUSD=2") && f.recebeu("AT+CSCS=\"IRA\""));
  }

  // ===== 14. URC no meio da resposta de um comando =====
  printf("14. aviso no meio de uma resposta\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    f.respostas["AT+CSQ"] = "\r\nRING\r\n\r\n+CSQ: 21,99\r\n\r\n+CMTI: \"SM\",7\r\n\r\nOK\r\n";
    CONFERE(m.sinal() == 21);
    auto ev = coletar(m, 20);
    CONFERE(contar(ev, EventoModem::SmsNovo) == 1);
    CONFERE(m.estado() == EstadoChamada::Tocando);
  }

  // ===== 15. chip com PIN; o READY chega antes do OK =====
  printf("15. PIN e configuração tardia do SMS\n");
  {
    FakeModem f; Modem m; g_fake = &f;
    f.respostas["AT+CPIN?"] = "\r\n+CPIN: SIM PIN\r\n\r\nOK\r\n";
    CONFERE(m.begin(f, 115200, PINOS));
    CONFERE(!m.chipPronto());
    EventoModem ev0;
    bool pediu = false;
    while (m.proximoEvento(ev0)) if (ev0.tipo == EventoModem::ChipPedeSenha) pediu = true;
    CONFERE(pediu);
    CONFERE(!f.recebeu("AT+CNMI=2,1,0,0,0"));
    f.especial = [&](const std::string& c) {
      if (c == "AT+CPIN=1234") { f.diz("\r\n+CPIN: READY\r\n\r\nOK\r\n\r\nSMS DONE\r\n", 10); return true; }
      return false;
    };
    CONFERE(m.senhaDoChip("1234"));
    auto ev = coletar(m, 100);
    CONFERE(contar(ev, EventoModem::ChipPronto) == 1);
    CONFERE(m.chipPronto());
    CONFERE(f.recebeu("AT+CNMI=2,1,0,0,0"));
    CONFERE(!m.senhaDoChip("12a4"));
  }

  // ===== 16. gravar arquivo (toque) =====
  printf("16. gravar arquivo no modem\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    f.respostas["AT+FSOPEN=C:/tijolo.wav,1"] = "\r\n+FSOPEN: 2\r\n\r\nOK\r\n";
    std::vector<uint8_t> dados(38444, 0x55);
    CONFERE(m.gravarArquivo("C:/tijolo.wav", dados.data(), dados.size()));
    CONFERE(f.recebeu("AT+FSOPEN=C:/tijolo.wav,1"));
    CONFERE(f.quantos("AT+FSWRITE=2,") == 19);       // 18 blocos de 2048 + 1 de 1580
    CONFERE(f.recebeu("AT+FSWRITE=2,1580,10"));
    CONFERE(f.recebeu("AT+FSCLOSE=2"));
    CONFERE(f.arquivoTotal == 38444);
    // bytes perdidos no caminho: o total não bate e a gravação falha
    f.respostas["AT+FSOPEN=C:/tijolo.wav,1"] = "\r\n+FSOPEN: 1\r\n\r\nOK\r\n";
    f.especial = [&](const std::string& c) {
      if (c.rfind("AT+FSWRITE=1,", 0) == 0 && f.quantos("AT+FSWRITE=1,") == 3) f.perderBytes = 10;
      return false;
    };
    CONFERE(!m.gravarArquivo("C:/tijolo.wav", dados.data(), dados.size()));
    CONFERE(m.ultimoErro().indexOf("gravou") >= 0);
    CONFERE(f.recebidos.back() == "AT+FSCLOSE=1");
  }

  // ===== 17. erros =====
  printf("17. erros\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    f.respostas["AT+CMGR=9"] = "\r\n+CMS ERROR: invalid memory index\r\n";
    Sms s;
    CONFERE(!m.lerSms(9, s));
    CONFERE(m.ultimoErro() == "+CMS ERROR: invalid memory index");
    CONFERE(!m.ligar("11 9999"));
    CONFERE(!m.enviarSms("abc", "oi"));
    // comando sem resposta: o OK atrasado não pode virar resposta do próximo
    f.especial = [&](const std::string& c) {
      if (c == "AT+CPSI?") { f.diz("\r\nOK\r\n", 10500); return true; }   // chega depois dos 9 s
      return false;
    };
    CONFERE(m.tipoDeRede() == "?");
    f.especial = nullptr;
    f.respostas["AT+CSQ"] = "\r\n+CSQ: 18,99\r\n\r\nOK\r\n";
    CONFERE(m.sinal() == 18);
  }

  // ===== 18. modem reinicia sozinho: refaz configuração e áudio =====
  printf("18. modem reiniciou sozinho no meio de uma ligação\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    m.gpioDoAmplificador(3);
    CONFERE(m.amplificador(true));
    CONFERE(m.volume(6));
    CONFERE(m.vivaVoz(true));
    f.clcc = "\r\n+CLCC: 1,1,0,0,0,\"+5511999998888\",145";
    f.diz("\r\n+CLCC: 1,1,0,0,0,\"+5511999998888\",145\r\n");
    coletar(m, 100);
    CONFERE(m.estado() == EstadoChamada::EmLigacao);
    f.recebidos.clear();
    f.clcc = "";
    f.diz("\r\nRDY\r\n\r\n*ATREADY: 1\r\n\r\n+CPIN: READY\r\n\r\nSMS DONE\r\n", 100);
    auto ev = coletar(m, 2000);
    CONFERE(m.estado() == EstadoChamada::Livre);
    CONFERE(contar(ev, EventoModem::Aviso) == 1);
    auto e = achar(ev, EventoModem::ChamadaEncerrada);
    CONFERE(e && e->texto.startsWith("o modem reiniciou"));
    CONFERE(f.recebeu("ATE0") && f.recebeu("AT+CLCC=1") && f.recebeu("AT+CMGF=0"));
    CONFERE(f.recebeu("AT+CGDRT=3,1") && f.recebeu("AT+CGSETV=3,1"));
    CONFERE(f.recebeu("AT+COUTGAIN=6") && f.recebeu("AT+CSDVC=3"));
    CONFERE(f.recebeu("AT+CNMI=2,1,0,0,0"));
  }

  // ===== 19. IMEI e desligar o modem (Fase 2) =====
  printf("19. IMEI e desligar o modem\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    f.respostas["AT+CGSN"] = "\r\n861234567890123\r\n\r\nOK\r\n";
    CONFERE(m.imei() == "861234567890123");
    f.respostas["AT+CGSN"] = "\r\n+CME ERROR: 10\r\n";
    CONFERE(m.imei() == "");
    // no meio de uma ligação: desliga a chamada antes
    f.clcc = "\r\n+CLCC: 1,1,0,0,0,\"+5511999998888\",145";
    f.diz("\r\n+CLCC: 1,1,0,0,0,\"+5511999998888\",145\r\n");
    coletar(m, 100);
    CONFERE(m.estado() == EstadoChamada::EmLigacao);
    f.recebidos.clear();
    f.clcc = "";
    bool desligado = false;
    f.especial = [&](const std::string& c) {
      if (desligado) return true;                   // desligado: não responde nada
      if (c == "AT+CPOF") { desligado = true; f.diz("\r\nOK\r\n\r\nNORMAL POWER DOWN\r\n", 5); return true; }
      return false;
    };
    escritasDePinos().clear();
    CONFERE(m.desligarModem());
    CONFERE(f.recebeu("AT+CHUP"));
    CONFERE(f.recebeu("AT+CPOF"));
    CONFERE(f.indice("AT+CHUP") < f.indice("AT+CPOF"));
    bool apertou = false;
    for (auto& e : escritasDePinos()) if (e.pino == 46 && e.nivel == HIGH) apertou = true;
    CONFERE(!apertou);                              // desligou pelo AT: não precisou do PWRKEY
    // Já desligado: nem manda CPOF (e não aperta o PWRKEY, que o ligaria)
    f.recebidos.clear();
    CONFERE(m.desligarModem());
    CONFERE(!f.recebeu("AT+CPOF"));
  }

  printf("19b. modem ignora o AT+CPOF: segura o PWRKEY por 3 s\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    bool desligado = false;
    escritasDePinos().clear();
    f.especial = [&](const std::string&) {
      if (desligado) return true;
      // depois de 3 s de PWRKEY apertado, o modem se desliga
      uint32_t inicio = 0; bool apertado = false;
      for (auto& e : escritasDePinos())
        if (e.pino == 46) { if (e.nivel == HIGH) { apertado = true; inicio = e.quando; } else if (apertado && e.quando - inicio >= 2500) desligado = true; }
      if (desligado) return true;
      return false;                                 // responde OK a tudo, inclusive AT+CPOF
    };
    CONFERE(m.desligarModem());
    uint32_t alto = 0, baixo = 0;
    for (auto& e : escritasDePinos()) if (e.pino == 46) { if (e.nivel == HIGH) alto = e.quando; else if (alto && !baixo) baixo = e.quando; }
    CONFERE(alto && baixo - alto >= 3000);
  }

  printf("20. consultas com prazo curto\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    m.prazoDasConsultas(1500);
    f.especial = [&](const std::string& c) { return c == "AT+CSQ" || c == "AT+COPS?"; };   // fica mudo
    uint32_t t0 = g_agora;
    CONFERE(m.sinal() == 99);
    CONFERE(m.tempoEsgotado());
    CONFERE(g_agora - t0 <= 3100);                  // 1,5 s esperando + 1,5 s descartando atrasados
    f.especial = nullptr;
    f.respostas["AT+CSQ"] = "\r\n+CSQ: 21,99\r\n\r\nOK\r\n";
    CONFERE(m.sinal() == 21);
    CONFERE(!m.tempoEsgotado());
  }

  printf("21. tom com USSD esperando: volta para IRA antes do AT+VTS\n");
  {
    FakeModem f; Modem m; novoModem(f, m);
    f.especial = [&](const std::string& c) {
      if (c.rfind("AT+CUSD=1,", 0) == 0) { f.diz("\r\nOK\r\n", 10); return true; }
      return false;
    };
    CONFERE(m.ussd("*222#"));
    CONFERE(m.esperandoUssd());
    f.recebidos.clear();
    CONFERE(m.tons("1"));
    const int ira = f.indice("AT+CSCS=\"IRA\""), vts = f.indice("AT+VTS=\"1\"");
    CONFERE(ira >= 0 && vts > ira);
    CONFERE(!m.esperandoUssd());
  }

  printf("22. troca de tecnologia (+CNSMOD): avisa uma vez a cada troca\n");
  {
    FakeModem f; Modem m;
    g_fake = &f;
    f.respostas["AT+CPIN?"] = "\r\n+CPIN: READY\r\n\r\nOK\r\n";
    CONFERE(m.begin(f, 115200, PINOS));
    CONFERE(f.indice("AT+CNSMOD=1") >= 0);          // pediu o aviso na configuração
    coletar(m, 10);
    f.diz("\r\n+CNSMOD: 8\r\n");                     // 4G
    auto ev = coletar(m, 100);
    CONFERE(ev.size() == 1 && ev[0].tipo == EventoModem::RedeMudou && ev[0].texto == "4G" && ev[0].valor == 8);
    f.diz("\r\n+CNSMOD: 8\r\n");                     // repetido: nada
    CONFERE(coletar(m, 100).empty());
    f.diz("\r\n+CNSMOD: 1\r\n");                     // caiu para o 2G (chamada chegando)
    ev = coletar(m, 100);
    CONFERE(ev.size() == 1 && ev[0].texto == "2G" && ev[0].valor == 1);
    f.diz("\r\n+CNSMOD: 3\r\n");                     // EDGE: continua no 2G, nada muda
    CONFERE(coletar(m, 100).empty());
    f.diz("\r\n+CNSMOD: 1,8\r\n");                   // formato da resposta, com o "1," na frente
    ev = coletar(m, 100);
    CONFERE(ev.size() == 1 && ev[0].texto == "4G");
    // Aviso na hora: chega no meio de um comando, antes da fila
    static int naHora = 0;
    static String rede;
    m.avisarNaHora([](const EventoModem& e) {
      if (e.tipo == EventoModem::RedeMudou) { naHora++; rede = e.texto; }
    });
    f.respostas["AT+CSQ"] = "\r\n+CNSMOD: 1\r\n\r\n+CSQ: 20,99\r\n\r\nOK\r\n";
    CONFERE(m.sinal() == 20);
    CONFERE(naHora == 1 && rede == "2G");          // já avisou, antes de alguém ler a fila
    ev = coletar(m, 10);
    CONFERE(ev.size() == 1 && ev[0].texto == "2G"); // e o evento continua na fila
  }

  printf("23. chamada caindo do 4G para o 2G: os avisos de registro (+CEREG/+CREG) dizem antes do RING\n");
  {
    FakeModem f; Modem m;
    g_fake = &f;
    f.respostas["AT+CPIN?"] = "\r\n+CPIN: READY\r\n\r\nOK\r\n";
    f.respostas["AT+CEREG?"] = "\r\n+CEREG: 1,1\r\n\r\nOK\r\n";   // já registrado no 4G
    f.respostas["AT+CREG?"] = "\r\n+CREG: 1,1\r\n\r\nOK\r\n";
    CONFERE(m.begin(f, 115200, PINOS));
    CONFERE(f.indice("AT+CREG=1") >= 0 && f.indice("AT+CEREG=1") >= 0);
    coletar(m, 10);
    f.diz("\r\n+CNSMOD: 8\r\n");                     // continua no 4G: nada
    f.diz("\r\n+CREG: 1\r\n");                       // voz registrada pelo 4G: nada
    CONFERE(coletar(m, 100).empty());
    f.diz("\r\n+CEREG: 0\r\n");                      // saiu do 4G (a chamada vem pelo 2G)
    auto ev = coletar(m, 100);
    CONFERE(ev.size() == 1 && ev[0].tipo == EventoModem::RedeMudou && ev[0].texto == "2G");
    f.diz("\r\n+CREG: 0\r\n");
    ev = coletar(m, 100);
    CONFERE(ev.size() == 1 && ev[0].texto == "sem serviço");
    f.diz("\r\n+CREG: 1\r\n");
    ev = coletar(m, 100);
    CONFERE(ev.size() == 1 && ev[0].texto == "2G");
    f.diz("\r\n+CEREG: 1,1\r\n");                    // resposta atrasada a um AT+CEREG?: não é aviso
    CONFERE(coletar(m, 100).empty());
    f.diz("\r\n+CEREG: 1\r\n");                      // a chamada acabou: de volta ao 4G
    ev = coletar(m, 100);
    CONFERE(ev.size() == 1 && ev[0].texto == "4G" && ev[0].valor == 8);
    f.diz("\r\n+CEREG: 1\r\n");                      // de novo: nada
    CONFERE(coletar(m, 100).empty());
    CONFERE(m.registro() == "registrado");           // a consulta continua entendendo a resposta
  }

  printf("\n%d verificações, %d falha(s)\n", verificacoes, falhas);
  return falhas ? 1 : 0;
}
