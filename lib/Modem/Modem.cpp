// ============================================================
//  Modem.cpp — veja o Modem.h para a explicação geral
//  Comandos conferidos no "A76XX Series AT Command Manual V1.12"
//  (pasta datasheet/A76XX do repositório LilyGO-T-A76XX).
// ============================================================
#include "Modem.h"
#include <Pdu.h>
#include <Texto.h>

// ---------- pontes entre String (Arduino) e std::string ----------
static std::string S(const String& s) { return std::string(s.c_str(), s.length()); }
static String A(const std::string& s) { return String(s.c_str()); }

static String depoisDosDoisPontos(const String& linha) {
  int p = linha.indexOf(':');
  if (p < 0) return "";
  String r = linha.substring(p + 1);
  r.trim();
  return r;
}

// Percorre as linhas de uma resposta (separadas por '\n')
template <typename F>
static void cadaLinha(const String& r, F f) {
  int ini = 0;
  while (ini < (int)r.length()) {
    int fim = r.indexOf('\n', ini);
    if (fim < 0) fim = r.length();
    f(r.substring(ini, fim));
    ini = fim + 1;
  }
}

// ============================================================
//  Ligar e configurar
// ============================================================
bool Modem::begin(HardwareSerial& uart, uint32_t baud, const PinosModem& pinos) {
  _uart = &uart;
  _p = pinos;
  _baud = baud;
  _iniciado = false;

  pinMode(_p.dtr, OUTPUT);
  digitalWrite(_p.dtr, LOW);          // LOW = modem não dorme
  pinMode(_p.ring, INPUT_PULLUP);
  pinMode(_p.pwrkey, OUTPUT);
  digitalWrite(_p.pwrkey, LOW);

  _uart->setRxBufferSize(4096);       // listas de SMS chegam de uma vez
  _uart->begin(_baud, SERIAL_8N1, _p.rx, _p.tx);

  // Se a ESP reiniciou no meio de um SMS, o modem ainda espera o texto
  // depois do "> ": um ESC cancela, senão todo "AT" viraria mensagem.
  _uart->write(0x1B);
  delay(100);

  // A ESP pode ter reiniciado com o modem ainda ligado: testa antes.
  bool jaEstavaLigado = _responde(1500);
  if (!jaEstavaLigado) {
    _pulsoPwrkey();
    if (!_responde(25000)) {
      _pulsoPwrkey();                 // segunda tentativa
      if (!_responde(25000)) return false;
    }
    // Depois de ligar, o modem leva uns segundos para ler o chip
    // e avisa com "SMS DONE". Espera até 15 s, tratando o que chegar.
    uint32_t inicio = millis();
    while (!_smsPronto && millis() - inicio < 15000) { _drenar(); delay(10); }
  }
  bool ok = _configurar();
  _iniciado = true;
  return ok;
}

void Modem::_pulsoPwrkey() {
  // Sequência da LilyGo para o A7670 (o transistor da placa inverte o sinal).
  // Ligar pede pulso de pelo menos 50 ms; desligar, 2,5 s: este não desliga.
  digitalWrite(_p.pwrkey, LOW);
  delay(100);
  digitalWrite(_p.pwrkey, HIGH);
  delay(100);
  digitalWrite(_p.pwrkey, LOW);
}

bool Modem::_responde(uint32_t timeoutMs) {
  uint32_t inicio = millis();
  while (millis() - inicio < timeoutMs) {
    _drenar();
    _ultimoComando = "AT";
    _enviarLinha("AT");
    if (_esperar(500, nullptr, "") == OK) { _ultimoComando = ""; return true; }
  }
  _ultimoComando = "";
  return false;
}

bool Modem::_configurar() {
  _charsetUcs2 = -1;
  comando("ATE0");                     // sem eco: o modem não repete o que mandamos
  comando("AT+CMEE=2");                // erros por extenso ("SIM not inserted")
  comando("AT+CLIP=1");                // mostra o número de quem liga
  comando("AT+CLCC=1");                // avisa sozinho toda mudança na chamada
  comando("AT+CVHU=0");                // ATH também desliga chamada de voz
  comando("AT+COPS=3,0");              // nome da operadora por extenso
  comando("AT+CTZU=1");                // acerta o relógio pela rede
  comando("AT+CNSMOD=1");              // avisa quando a rede troca de tecnologia (4G ↔ 2G)
  // Avisos de registro: "+CEREG: <stat>" no 4G e "+CREG: <stat>" no de voz.
  // Numa chamada recebida o modem cai do 4G para o 2G sem mandar +CNSMOD,
  // mas manda "+CEREG: 0" (saiu do 4G) antes do RING: é o primeiro aviso.
  comando("AT+CREG=1");
  comando("AT+CEREG=1");
  comando("AT+CMGF=0");                // SMS em modo PDU (veja lib/Pdu)
  _charset(false);                     // textos dos comandos em ASCII simples

  String r;
  // Onde está registrado agora (os avisos só vêm nas mudanças)
  _lte = comando("AT+CEREG?", &r) && _registradoNaResposta(r);
  _cs = comando("AT+CREG?", &r) && _registradoNaResposta(r);
  if (_lte) _sistema = 4;
  else if (_cs) _sistema = 2;
  if (comando("AT+CPIN?", &r)) {
    String estado = depoisDosDoisPontos(r);
    _chipPronto = (estado == "READY");
    if (estado.startsWith("SIM PIN") || estado.startsWith("SIM PUK"))
      _publicar(EventoModem::ChipPedeSenha, estado);
  } else {
    _chipPronto = false;
    _publicar(EventoModem::SemChip, _erro);
  }
  if (_chipPronto) _configurarSms();
  _aplicarAudio();
  return true;
}

void Modem::_configurarSms() {
  _configSmsPendente = false;
  comando("AT+CPMS=\"SM\",\"SM\",\"SM\"");   // SMS guardados no chip
  comando("AT+CNMI=2,1,0,0,0");              // avisa "+CMTI" quando chega SMS
}

bool Modem::reiniciar() {
  _iniciado = false;                         // os avisos de boot agora são esperados
  if (_estado != EstadoChamada::Livre) _mudarEstado(EstadoChamada::Livre, "o modem reiniciou");
  comando("AT+CRESET", nullptr, 9000);
  _smsPronto = false;
  _chipPronto = false;
  delay(3000);
  bool ok = _responde(30000);
  if (ok) {
    uint32_t inicio = millis();
    while (!_smsPronto && millis() - inicio < 15000) { _drenar(); delay(10); }
    ok = _configurar();
  }
  _iniciado = true;
  return ok;
}

bool Modem::desligarModem() {
  if (!_uart) return false;
  _ussdEsperando = false;
  if (!_responde(1000)) {                    // já está desligado (ou mudo)
    _iniciado = false;
    return true;
  }
  if (_estado != EstadoChamada::Livre) desligar();
  _iniciado = false;                         // o que vier agora não é "reiniciou sozinho"
  comando("AT+CPOF", nullptr, 9000);
  _smsPronto = false;
  _chipPronto = false;
  // Como no exemplo DeepSleep da LilyGo: espera ele parar de responder.
  uint32_t inicio = millis();
  while (millis() - inicio < 8000) {
    if (!_responde(600) && !_responde(600)) return true;
    delay(400);
  }
  // Ainda responde: segura o PWRKEY por 3 s. Só neste caso, porque o mesmo
  // toque longo LIGA um modem que já está desligado.
  digitalWrite(_p.pwrkey, HIGH);
  delay(3000);
  digitalWrite(_p.pwrkey, LOW);
  inicio = millis();
  while (millis() - inicio < 8000) {
    if (!_responde(600) && !_responde(600)) return true;
    delay(400);
  }
  return false;
}

// ============================================================
//  Baixo nível: mandar, ler linhas, esperar respostas
// ============================================================
void Modem::_enviarLinha(const String& cmd) {
  if (_espelho) { _espelho->print(">> "); _espelho->println(cmd); }
  _uart->print(cmd);
  _uart->print('\r');
}

// Junta bytes até um fim de linha. As linhas vazias são ignoradas.
bool Modem::_lerLinha(String& out) {
  while (_uart->available()) {
    char c = (char)_uart->read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (_linha.length() == 0) continue;
      out = _linha;
      _linha = "";
      if (_espelho) { _espelho->print("<< "); _espelho->println(out); }
      return true;
    }
    if (_linha.length() < 4096) _linha += c;
  }
  return false;
}

// Trata as linhas completas que já chegaram (fora de qualquer comando).
void Modem::_drenar() {
  String l;
  while (_lerLinha(l)) {
    if (_ehUrc(l)) _tratarUrc(l);
    // "OK"/"ERROR" atrasados de um comando que estourou o tempo: ignora
  }
}

// "AT+CSQ" → "+CSQ:"  ·  "AT+CPIN?" → "+CPIN:"  ·  "ATD123;" → ""
String Modem::_prefixoDe(const String& cmd) {
  String c = cmd;
  c.toUpperCase();
  if (c.length() < 4 || !c.startsWith("AT")) return "";
  char t = c[2];
  if (t != '+' && t != '*') return "";
  unsigned fim = 3;
  while (fim < c.length() && (isalnum((unsigned char)c[fim]) || c[fim] == '_')) fim++;
  return c.substring(2, fim) + ":";
}

bool Modem::comando(const String& at, String* resposta, uint32_t timeoutMs) {
  if (!_uart) return false;
  _drenar();
  String cmd = at;
  cmd.trim();
  if (!(cmd.startsWith("AT") || cmd.startsWith("at") || cmd.startsWith("At") || cmd.startsWith("aT")))
    cmd = "AT" + cmd;
  _ultimoComando = cmd;
  _enviarLinha(cmd);
  if (resposta) *resposta = "";
  int r = _esperar(timeoutMs, resposta, _prefixoDe(cmd));
  _ultimoComando = "";
  _ultimoEsgotou = (r == TEMPO_ESGOTADO);
  if (r == TEMPO_ESGOTADO) _descartarAtrasado(timeoutMs < 3000 ? timeoutMs : 3000);
  return r == OK;
}

static const char* traduzirFinalDeChamada(const String& l) {
  if (l == "BUSY") return "ocupado";
  if (l == "NO ANSWER") return "não atendeu";
  if (l == "NO DIALTONE") return "sem rede para ligar";
  return "a chamada não completou (NO CARRIER)";
}

// Lê linhas até OK/ERROR. As que começam com `prefixo` são resposta
// (mesmo que pareçam URC, como "+CPIN: READY" em resposta ao AT+CPIN?).
// Com `finaisDeChamada`, NO CARRIER/BUSY/NO ANSWER/NO DIALTONE também
// encerram (é como o ATD e o ATA respondem quando falham).
int Modem::_esperar(uint32_t timeoutMs, String* resposta, const String& prefixo, bool finaisDeChamada) {
  uint32_t inicio = millis();
  String linha;
  while (millis() - inicio < timeoutMs) {
    if (!_lerLinha(linha)) { delay(1); continue; }
    if (linha == "OK") return OK;
    if (linha == "ERROR" || linha.startsWith("+CME ERROR") || linha.startsWith("+CMS ERROR")) {
      _erro = linha;
      return ERRO;
    }
    if (finaisDeChamada && (linha == "NO CARRIER" || linha == "BUSY" || linha == "NO ANSWER" || linha == "NO DIALTONE")) {
      _erro = traduzirFinalDeChamada(linha);
      return ERRO;
    }
    if (prefixo.length() && linha.startsWith(prefixo)) {
      if (resposta) { if (resposta->length()) *resposta += '\n'; *resposta += linha; }
      continue;
    }
    if (_ehUrc(linha)) { _tratarUrc(linha); continue; }
    if (_ultimoComando.length() && linha.equalsIgnoreCase(_ultimoComando)) continue;   // eco
    if (resposta) { if (resposta->length()) *resposta += '\n'; *resposta += linha; }
  }
  _erro = "sem resposta (tempo esgotado)";
  return TEMPO_ESGOTADO;
}

// Depois de um tempo esgotado, o OK/ERROR daquele comando ainda pode
// chegar. Se chegasse no meio do próximo comando, seria confundido com
// a resposta dele: então esperamos um pouco e jogamos fora.
void Modem::_descartarAtrasado(uint32_t timeoutMs) {
  uint32_t inicio = millis();
  String linha;
  while (millis() - inicio < timeoutMs) {
    if (!_lerLinha(linha)) { delay(1); continue; }
    if (linha == "OK" || linha == "ERROR" || linha.startsWith("+CME ERROR") || linha.startsWith("+CMS ERROR")) return;
    if (_ehUrc(linha)) _tratarUrc(linha);
  }
}

// Espera o "> " que o modem manda antes de receber o SMS.
// O "> " não tem fim de linha, por isso olhamos a linha em construção.
bool Modem::_esperarPrompt(uint32_t timeoutMs) {
  uint32_t inicio = millis();
  String linha;
  while (millis() - inicio < timeoutMs) {
    while (_lerLinha(linha)) {
      if (linha == "ERROR" || linha.startsWith("+CME ERROR") || linha.startsWith("+CMS ERROR")) {
        _erro = linha;
        return false;
      }
      if (_ehUrc(linha)) _tratarUrc(linha);
    }
    if (_linha.startsWith(">")) {
      if (_espelho) _espelho->println("<< >");
      _linha = "";
      return true;
    }
    delay(1);
  }
  _erro = "o modem não pediu o texto";
  return false;
}

bool Modem::_esperarLinha(const char* texto, uint32_t timeoutMs) {
  uint32_t inicio = millis();
  String linha;
  while (millis() - inicio < timeoutMs) {
    if (!_lerLinha(linha)) { delay(1); continue; }
    if (linha.startsWith(texto)) return true;
    if (linha == "ERROR" || linha.startsWith("+CME ERROR")) { _erro = linha; return false; }
    if (_ehUrc(linha)) _tratarUrc(linha);
  }
  _erro = String("não chegou ") + texto;
  return false;
}

// AT+CSCS é gravado na memória do modem a cada troca: só troca quando precisa.
bool Modem::_charset(bool ucs2) {
  if (_charsetUcs2 == (ucs2 ? 1 : 0)) return true;
  bool ok = comando(ucs2 ? "AT+CSCS=\"UCS2\"" : "AT+CSCS=\"IRA\"");
  _charsetUcs2 = ok ? (ucs2 ? 1 : 0) : -1;
  return ok;
}

// ============================================================
//  URCs: o que o modem fala sozinho
// ============================================================
bool Modem::_ehUrc(const String& l) const {
  if (l == "RING" || l == "NO CARRIER" || l == "BUSY" || l == "NO ANSWER" ||
      l == "NO DIALTONE" || l == "SMS DONE" || l == "PB DONE" || l == "RDY")
    return true;
  static const char* PREFIXOS[] = {
    "+CLIP:", "+CRING:", "+CLCC:", "VOICE CALL:", "MISSED_CALL:", "+CMTI:", "+CDSI:",
    "+CPIN:", "+CREG:", "+CEREG:", "+CGREG:", "+CUSD:", "+CTZV:", "+CTZE:", "*PSUTTZ",
    "DST:", "+CGEV:", "*ATREADY", "UNDER-VOLTAGE", "OVER-VOLTAGE", "NORMAL POWER DOWN",
    "+CMTE:", "+CCMXPLAY:", "+CCMXSTOP:", "+AUDIOSTATE:", "+CPMVT", "+CSSU:", "+CSSI:",
    "+CNSMOD:",
  };
  for (auto p : PREFIXOS) if (l.startsWith(p)) return true;
  return false;
}

void Modem::_publicar(EventoModem::Tipo tipo, const String& txt, int valor) {
  if (_fila.size() >= 32) _fila.erase(_fila.begin());   // não deixa a fila crescer sem fim
  EventoModem ev;
  ev.tipo = tipo;
  ev.texto = txt;
  ev.valor = valor;
  if (_naHora) _naHora(ev);
  _fila.push_back(ev);
}

bool Modem::proximoEvento(EventoModem& ev) {
  if (_fila.empty()) return false;
  ev = _fila.front();
  _fila.erase(_fila.begin());
  return true;
}

String Modem::_duracao() const {
  if (_inicioLigacao == 0) return "";
  uint32_t s = (millis() - _inicioLigacao) / 1000;
  char buf[16];
  snprintf(buf, sizeof buf, "%u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
  return String(buf);
}

void Modem::_mudarEstado(EstadoChamada novo, const String& motivo) {
  EstadoChamada antes = _estado;
  if (novo == antes) return;
  _estado = novo;

  if (antes == EstadoChamada::Livre) {          // começou uma chamada nova
    _motivoPendente = "";
    _motivoUrc = "";
    _inicioLigacao = 0;
    _avisouChamada = false;
  }
  if (novo == EstadoChamada::Tocando) _ultimoRing = millis();
  if (novo == EstadoChamada::EmLigacao) {
    _inicioLigacao = millis();
    _publicar(EventoModem::ChamadaConectada, _numero);
  }
  if (novo == EstadoChamada::Livre) {
    // Prioridade do motivo: o que nós fizemos (desligar/recusar), o que
    // o código passou, o que o modem disse (ocupado, não atendeu).
    String m = _motivoPendente.length() ? _motivoPendente : motivo.length() ? motivo : _motivoUrc;
    if (antes == EstadoChamada::Tocando && m.length() == 0) {
      _publicar(EventoModem::ChamadaPerdida, _numero);           // ninguém atendeu
    } else {
      if (m.length() == 0 && _inicioLigacao == 0 &&
          (antes == EstadoChamada::Discando || antes == EstadoChamada::Chamando))
        m = "não completou";
      String d = _duracao();
      if (d.length()) {
        if (m.length()) m += ", ";
        m += "duração " + d;
      }
      _publicar(EventoModem::ChamadaEncerrada, m);
    }
    _numero = "";
    _avisouChamada = false;
    _inicioLigacao = 0;
    _idChamada = -1;
    _motivoPendente = "";
    _motivoUrc = "";
  }
  if (novo != EstadoChamada::Livre) _ultimaConsulta = millis();
}

// +CLCC: <id>,<dir>,<stat>,<mode>,<mpty>[,<number>,<type>...]
static bool lerClcc(const String& linha, int& id, int& stat, String& numero) {
  auto f = texto::campos(S(depoisDosDoisPontos(linha)));
  if (f.size() < 5) return false;
  id = atoi(f[0].c_str());
  stat = atoi(f[2].c_str());
  numero = f.size() > 5 ? A(texto::decodificarCampo(f[5], true)) : String("");
  return true;
}

// Cada chamada tem um número no +CLCC. Acompanhamos uma só: avisos de
// outra chamada (uma segunda chegando durante a ligação) não mexem nela.
void Modem::_aplicarClcc(int id, int stat, const String& num) {
  if (stat == 5) {                                 // outra chamada esperando
    if (id != _idChamada) _publicar(EventoModem::Aviso, "outra chamada esperando: " + num);
    return;
  }
  if (_idChamada >= 0 && id != _idChamada) return;
  if (stat == 6) {                                 // desligou
    if (_estado != EstadoChamada::Livre) _mudarEstado(EstadoChamada::Livre);
    return;
  }
  if (_idChamada < 0) _idChamada = id;
  if (num.length()) _numero = num;
  switch (stat) {
    case 0: case 1: _mudarEstado(EstadoChamada::EmLigacao); break;   // ativa / em espera
    case 2: _mudarEstado(EstadoChamada::Discando); break;
    case 3: _mudarEstado(EstadoChamada::Chamando); break;            // tocando do outro lado
    case 4:
      _mudarEstado(EstadoChamada::Tocando);
      _ultimoRing = millis();
      if (!_avisouChamada && _numero.length()) { _avisouChamada = true; _publicar(EventoModem::ChamadaRecebida, _numero); }
      break;
  }
}

void Modem::_tratarUrc(const String& l) {
  // ----- chamadas -----
  if (l.startsWith("+CLCC:")) {
    int id, stat;
    String num;
    if (lerClcc(l, id, stat, num)) _aplicarClcc(id, stat, num);
    return;
  }
  if (l == "RING" || l.startsWith("+CRING:")) {
    if (_estado == EstadoChamada::Livre) _mudarEstado(EstadoChamada::Tocando);
    _ultimoRing = millis();
    return;
  }
  if (l.startsWith("+CLIP:")) {
    auto f = texto::campos(S(depoisDosDoisPontos(l)));
    String num = f.empty() ? String("") : A(texto::decodificarCampo(f[0], true));
    if (_estado == EstadoChamada::Livre) _mudarEstado(EstadoChamada::Tocando);
    _ultimoRing = millis();
    if (_estado == EstadoChamada::Tocando && !_avisouChamada) {
      _numero = num;
      _avisouChamada = true;
      _publicar(EventoModem::ChamadaRecebida, num);
    }
    return;
  }
  // Os avisos abaixo não dizem QUAL chamada mudou. Em vez de concluir
  // sozinhos, pedimos ao AT+CLCC a lista atual (no próximo loop).
  if (l.startsWith("VOICE CALL:")) { _ultimaConsulta = 0; return; }
  if (l == "NO CARRIER" || l == "BUSY" || l == "NO ANSWER" || l == "NO DIALTONE") {
    if (_estado == EstadoChamada::Livre) return;
    if (l == "BUSY") _motivoUrc = "ocupado";
    if (l == "NO ANSWER") _motivoUrc = "não atendeu";
    _ultimaConsulta = 0;
    return;
  }
  if (l.startsWith("MISSED_CALL:")) {
    // "MISSED_CALL: 09:42PM 11999998888": o número é a última palavra
    String r = depoisDosDoisPontos(l);
    int esp = r.lastIndexOf(' ');
    if (_estado == EstadoChamada::Tocando && _numero.length() == 0) _numero = esp >= 0 ? r.substring(esp + 1) : r;
    _ultimaConsulta = 0;
    return;
  }

  // ----- SMS -----
  if (l.startsWith("+CMTI:")) {
    auto f = texto::campos(S(depoisDosDoisPontos(l)));
    if (f.size() >= 2) _publicar(EventoModem::SmsNovo, A(f[0]), atoi(f[1].c_str()));
    return;
  }

  // ----- chip e prontidão -----
  if (l.startsWith("+CPIN:")) {
    String e = depoisDosDoisPontos(l);
    if (e == "READY") {
      if (!_chipPronto) {
        _chipPronto = true;
        _configSmsPendente = true;          // o loop() configura o SMS (aqui não dá: estamos lendo)
        _publicar(EventoModem::ChipPronto);
      }
    } else if (e.startsWith("SIM PIN") || e.startsWith("SIM PUK")) {
      _chipPronto = false;
      _publicar(EventoModem::ChipPedeSenha, e);
    } else {
      _chipPronto = false;
      _publicar(EventoModem::SemChip, e);
    }
    return;
  }
  if (l == "SMS DONE") { _smsPronto = true; return; }
  if (l == "RDY" || l.startsWith("*ATREADY")) {
    // Aviso de boot do modem. Depois do begin(), quer dizer que ele
    // reiniciou sozinho (bateria fraca, pico de corrente) e esqueceu a
    // configuração: o loop() refaz tudo.
    if (_iniciado) { _reconfigurar = true; _smsPronto = false; _chipPronto = false; }
    return;
  }

  // ----- USSD: +CUSD: <m>[,"<texto>",<dcs>] -----
  if (l.startsWith("+CUSD:")) {
    auto f = texto::campos(S(depoisDosDoisPontos(l)));
    int m = f.empty() ? 2 : atoi(f[0].c_str());
    String txt = f.size() > 1 ? A(texto::decodificarCampo(f[1])) : String("");
    if (txt.length() == 0) {
      txt = m == 2 ? "sessão encerrada pela operadora" : m == 4 ? "operação não suportada" :
            m == 5 ? "a operadora não respondeu a tempo" : "";
    }
    _publicar(EventoModem::Ussd, txt, m == 1 ? 1 : 0);
    _ussdChegou = true;
    if (_ussdEsperando) {          // não dá para mandar comandos aqui dentro
      _ussdEsperando = false;
      _voltarCharset = true;
    }
    return;
  }

  // ----- avisos de energia e temperatura -----
  if (l.startsWith("UNDER-VOLTAGE") || l.startsWith("OVER-VOLTAGE") ||
      l.startsWith("NORMAL POWER DOWN") || l.startsWith("+CMTE:") || l.startsWith("+CPMVT")) {
    _publicar(EventoModem::Aviso, l);
    return;
  }
  // ----- tecnologia da rede (AT+CNSMOD=1) -----
  // "+CNSMOD: 8" (aviso) ou "+CNSMOD: 1,8" (resposta): o número final é o sistema.
  if (l.startsWith("+CNSMOD:")) {
    const int sep = l.lastIndexOf(',');
    String n = l.substring(sep >= 0 ? sep + 1 : 8);
    n.trim();
    const int v = n.toInt();
    // Só a geração conta: GSM, GPRS e EDGE (1 a 3) são todos 2G, e o
    // modem às vezes alterna entre eles sem trocar de rede.
    _mudouGeracao(v == 0 ? 0 : v <= 3 ? 2 : v <= 7 ? 3 : 4, v);
    return;
  }
  // Registro (AT+CREG=1, AT+CEREG=1): o aviso tem um campo só, "<stat>". Com
  // vírgula é a resposta a um AT+CREG? que chegou atrasada ("<n>,<stat>").
  // Registrado no 4G = 4G; fora dele, registrado no de voz = 2G (o A7670SA
  // não tem 3G). Numa chamada recebida: "+CEREG: 0" (2G), "+CREG: 0" (sem
  // serviço), "+CREG: 1" (2G), RING; quando ela acaba, "+CEREG: 1" (4G).
  if (l.startsWith("+CEREG:") || l.startsWith("+CREG:")) {
    const String v = depoisDosDoisPontos(l);
    if (v.indexOf(',') >= 0) return;
    const int st = v.toInt();
    const bool registrado = st == 1 || st == 5;
    if (l.startsWith("+CEREG:")) _lte = registrado;
    else _cs = registrado;
    const int geracao = _lte ? 4 : _cs ? 2 : 0;
    _mudouGeracao(geracao, geracao == 4 ? 8 : geracao == 2 ? 1 : 0);
    return;
  }
  // Demais URCs (fuso horário, áudio...) não precisam de ação.
}

// A rede mudou de geração (0 sem serviço, 2 = 2G, 3 = 3G, 4 = 4G)? Avisa
// uma vez. `valor` segue o AT+CNSMOD (0, 1 = GSM, 8 = LTE...).
void Modem::_mudouGeracao(int geracao, int valor) {
  if (geracao == _sistema) return;
  _sistema = geracao;
  const char* nome = geracao == 0 ? "sem serviço" : geracao == 2 ? "2G" : geracao == 3 ? "3G" : "4G";
  _publicar(EventoModem::RedeMudou, nome, valor);
}

// "+CREG: 1,1" ou "+CEREG: 2,5,..." (a resposta: <n>,<stat>,...): registrado?
bool Modem::_registradoNaResposta(const String& r) {
  const String v = depoisDosDoisPontos(r);
  const int virgula = v.indexOf(',');
  if (virgula < 0) return false;
  const int st = v.substring(virgula + 1).toInt();
  return st == 1 || st == 5;
}

// ============================================================
//  loop(): URCs, reconfiguração e conferência da chamada
// ============================================================
void Modem::loop() {
  if (!_uart) return;
  _drenar();

  if (_reconfigurar) {
    _reconfigurar = false;
    _ussdEsperando = false;
    _voltarCharset = false;
    if (_estado != EstadoChamada::Livre) _mudarEstado(EstadoChamada::Livre, "o modem reiniciou");
    uint32_t inicio = millis();
    while (!_smsPronto && millis() - inicio < 15000) { _drenar(); delay(10); }
    _configurar();
    _publicar(EventoModem::Aviso, "o modem reiniciou sozinho (bateria fraca?); configuração refeita");
  }
  if (_configSmsPendente) _configurarSms();
  if (_voltarCharset) {
    _voltarCharset = false;
    _charset(false);
  }
  if (_ussdEsperando && millis() - _ussdDesde > 25000) {
    _ussdEsperando = false;
    _publicar(EventoModem::Ussd, "sem resposta da operadora", 0);
    _charset(false);
  }
  if (_estado == EstadoChamada::Livre) return;

  // Tocando sem número depois de 1,5 s: avisa assim mesmo (número privado)
  if (_estado == EstadoChamada::Tocando && !_avisouChamada && millis() - _ultimoRing > 1500) {
    _avisouChamada = true;
    _publicar(EventoModem::ChamadaRecebida, _numero);
  }
  // A cada 2 s (ou já, se algum aviso pediu) pergunta ao modem como está
  // a chamada. É a fonte da verdade quando os avisos são ambíguos.
  if (_ultimaConsulta == 0 || millis() - _ultimaConsulta > 2000) _consultarChamadas();
}

void Modem::_consultarChamadas() {
  if (_consultando) return;
  _consultando = true;
  _ultimaConsulta = millis();
  String r;
  if (comando("AT+CLCC", &r)) {
    bool achou = false;
    cadaLinha(r, [&](const String& linha) {
      int id, stat;
      String num;
      if (achou || !linha.startsWith("+CLCC:") || !lerClcc(linha, id, stat, num)) return;
      if (stat == 5 || stat == 6) return;            // em espera / já desligada
      if (_idChamada >= 0 && id != _idChamada) return;
      achou = true;
      _aplicarClcc(id, stat, num);
    });
    if (!achou && _estado != EstadoChamada::Livre) _mudarEstado(EstadoChamada::Livre);
  }
  _ultimaConsulta = millis();
  if (_ultimaConsulta == 0) _ultimaConsulta = 1;     // 0 quer dizer "consulte já"
  _consultando = false;
}

// ============================================================
//  Informações
// ============================================================
int Modem::sinal() {
  String r;
  if (!comando("AT+CSQ", &r, _prazoConsulta)) return 99;
  return atoi(depoisDosDoisPontos(r).c_str());       // "+CSQ: 20,99" → 20
}

String Modem::operadora() {
  String r;
  if (!comando("AT+COPS?", &r, _prazoConsulta)) return "";
  auto f = texto::campos(S(depoisDosDoisPontos(r)));  // 0,0,"TIM BRASIL",7
  return f.size() >= 3 ? A(texto::decodificarCampo(f[2], true)) : String("");
}

String Modem::tipoDeRede() {
  String r;
  if (!comando("AT+CPSI?", &r, _prazoConsulta)) return "?";
  auto f = texto::campos(S(depoisDosDoisPontos(r)));  // LTE,Online,724-04,...
  if (f.empty()) return "?";
  if (f[0] == "NO SERVICE") return "sem serviço";
  return A(f[0]);
}

String Modem::registro() {
  // LTE primeiro (CEREG), depois GSM (CREG)
  const char* cmds[] = {"AT+CEREG?", "AT+CREG?"};
  int melhor = -1;
  for (auto c : cmds) {
    String r;
    if (!comando(c, &r, _prazoConsulta)) { if (tempoEsgotado()) break; continue; }
    auto f = texto::campos(S(depoisDosDoisPontos(r)));  // <n>,<stat>[,...]
    if (f.size() < 2) continue;
    int st = atoi(f[1].c_str());
    // Aproveita para acertar o que os avisos de registro dizem (se um se perdeu)
    if (c[4] == 'E') _lte = st == 1 || st == 5;
    else _cs = st == 1 || st == 5;
    if (st == 1 || st == 5) { melhor = st; break; }
    if (melhor < 0 || st == 2) melhor = st;
  }
  switch (melhor) {
    case 1: return "registrado";
    case 5: return "registrado (roaming)";
    case 2: return "procurando rede";
    case 3: return "registro negado pela operadora";
    case 0: return "não está procurando rede";
    default: return "desconhecido";
  }
}

String Modem::estadoChip() {
  String r;
  if (!comando("AT+CPIN?", &r, _prazoConsulta)) {
    if (_erro.indexOf("not inserted") >= 0) return "sem chip";
    return "erro: " + _erro;
  }
  String e = depoisDosDoisPontos(r);
  if (e == "READY") return "pronto";
  if (e == "SIM PIN") return "pede PIN (use: pin 1234)";
  if (e == "SIM PUK") return "BLOQUEADO: precisa do PUK da operadora";
  return e;
}

int Modem::milivolts() {
  String r;
  if (!comando("AT+CBC", &r)) return -1;
  return (int)(depoisDosDoisPontos(r).toFloat() * 1000.0f);   // "+CBC: 3.950V"
}

String Modem::imei() {
  String r;
  if (!comando("AT+CGSN", &r, _prazoConsulta)) return "";
  String d;
  for (unsigned i = 0; i < r.length(); i++)
    if (isdigit((unsigned char)r[i])) d += r[i];
  return d.length() >= 14 && d.length() <= 17 ? d : String("");
}

String Modem::horaDaRede() {
  String r;
  if (!comando("AT+CCLK?", &r, _prazoConsulta)) return "";
  auto f = texto::campos(S(depoisDosDoisPontos(r)));
  return f.empty() ? String("") : A(f[0]);
}

// ============================================================
//  Chamadas
// ============================================================
bool Modem::ligar(const String& numero) {
  if (!texto::numeroValido(S(numero), true)) { _erro = "número inválido"; return false; }
  if (_estado != EstadoChamada::Livre) { _erro = "já existe uma chamada"; return false; }
  _numero = numero;
  String cmd = "ATD" + numero + ";";
  _drenar();
  _ultimoComando = cmd;
  _enviarLinha(cmd);
  int r = _esperar(20000, nullptr, "", true);       // NO CARRIER/BUSY também respondem ao ATD
  _ultimoComando = "";
  if (r != OK) {
    if (r == TEMPO_ESGOTADO) _descartarAtrasado();
    if (_estado == EstadoChamada::Livre) _numero = "";
    else _ultimaConsulta = 0;
    return false;
  }
  // O aviso +CLCC pode ter chegado antes do OK e já mudado o estado.
  if (_estado == EstadoChamada::Livre) _mudarEstado(EstadoChamada::Discando);
  return true;
}

bool Modem::atender() {
  if (_estado != EstadoChamada::Tocando) { _erro = "não há chamada tocando"; return false; }
  _drenar();
  _ultimoComando = "ATA";
  _enviarLinha("ATA");
  int r = _esperar(20000, nullptr, "", true);
  _ultimoComando = "";
  if (r == OK) {
    _mudarEstado(EstadoChamada::EmLigacao);
    return true;
  }
  if (r == TEMPO_ESGOTADO) _descartarAtrasado();
  _ultimaConsulta = 0;                               // quem ligou pode ter desistido: confere
  return false;
}

bool Modem::desligar() {
  if (_estado != EstadoChamada::Livre)
    _motivoPendente = _estado == EstadoChamada::Tocando ? "recusada" : "você desligou";
  // "Se não houver chamada, só responde OK" (manual): mandar sempre também
  // solta uma chamada que o firmware tenha perdido de vista.
  bool ok = comando("AT+CHUP");
  if (ok && _estado != EstadoChamada::Livre) _mudarEstado(EstadoChamada::Livre);
  return ok;
}

bool Modem::tons(const String& digitos) {
  if (digitos.length() == 0 || digitos.length() > 32) { _erro = "1 a 32 dígitos"; return false; }
  for (unsigned i = 0; i < digitos.length(); i++)
    if (!strchr("0123456789*#ABCD", digitos[i])) { _erro = "use 0-9, * e #"; return false; }
  // Um USSD esperando resposta deixa o modem em UCS2, e aí o texto do
  // AT+VTS seria lido como hexadecimal: volta para IRA (a resposta do USSD,
  // se ainda vier, chega sem acentos).
  if (_charsetUcs2 == 1) {
    _ussdEsperando = false;
    _voltarCharset = false;
    _charset(false);
  }
  return comando("AT+VTS=\"" + digitos + "\"");
}

bool Modem::senhaDoChip(const String& pin) {
  if (pin.length() < 4 || pin.length() > 8) { _erro = "o PIN tem de 4 a 8 dígitos"; return false; }
  for (unsigned i = 0; i < pin.length(); i++)
    if (!isdigit((unsigned char)pin[i])) { _erro = "o PIN só tem números"; return false; }
  String r;
  bool ok = comando("AT+CPIN=" + pin, &r);
  // O "+CPIN: READY" pode chegar antes do OK e ser lido como resposta.
  if (r.indexOf("+CPIN: READY") >= 0) _tratarUrc("+CPIN: READY");
  return ok;
}

// ============================================================
//  Áudio
// ============================================================
bool Modem::_aplicarAmplificador() {
  if (_gpioAmplificador < 0 || _amplificador < 0) return false;
  String g = String(_gpioAmplificador);
  return comando("AT+CGDRT=" + g + ",1") && comando("AT+CGSETV=" + g + "," + String(_amplificador));
}

void Modem::_aplicarAudio() {
  if (_amplificador >= 0) _aplicarAmplificador();
  if (_volume >= 0) comando("AT+COUTGAIN=" + String(_volume));
  if (_microfone >= 0) comando("AT+CMICGAIN=" + String(_microfone));
  if (_vivaVoz >= 0) comando(_vivaVoz ? "AT+CSDVC=3" : "AT+CSDVC=1");
}

bool Modem::amplificador(bool ligado) { _amplificador = ligado ? 1 : 0; return _aplicarAmplificador(); }
bool Modem::volume(int nivel)    { _volume = constrain(nivel, 0, 7);    return comando("AT+COUTGAIN=" + String(_volume)); }
bool Modem::microfone(int nivel) { _microfone = constrain(nivel, 0, 7); return comando("AT+CMICGAIN=" + String(_microfone)); }
bool Modem::vivaVoz(bool ligado) { _vivaVoz = ligado ? 1 : 0;           return comando(ligado ? "AT+CSDVC=3" : "AT+CSDVC=1"); }

// ============================================================
//  SMS no modo PDU
// ============================================================
bool Modem::enviarSms(const String& numero, const String& txt, int* partes) {
  if (!texto::numeroValido(S(numero), false)) { _erro = "número inválido"; return false; }
  if (!_chipPronto) { _erro = "chip não está pronto"; return false; }
  std::string erro;
  auto envios = pdu::codificar(S(numero), S(txt), (uint8_t)random(1, 256), 0xA7, &erro);
  if (envios.empty()) { _erro = A(erro); return false; }
  if (envios.size() > 15) { _erro = "texto longo demais (máximo 15 partes)"; return false; }
  if (partes) *partes = (int)envios.size();

  for (size_t i = 0; i < envios.size(); i++) {
    // Chegou uma chamada no meio de um SMS longo: para aqui, e o telefone
    // fica livre para atender (cada parte pode levar até 60 s)
    if (_estado == EstadoChamada::Tocando) {
      _erro = "chegou uma chamada no meio do envio (" + String((unsigned)i) + " de " +
              String((unsigned)envios.size()) + " partes foram)";
      return false;
    }
    const auto& e = envios[i];
    String cmd = "AT+CMGS=" + String(e.tamanho);
    _drenar();
    _ultimoComando = cmd;
    _enviarLinha(cmd);
    if (!_esperarPrompt(10000)) {
      _uart->write(0x1B);                             // ESC cancela
      _ultimoComando = "";
      _descartarAtrasado(2000);
      return false;
    }
    if (_espelho) { _espelho->print(">> "); _espelho->print(A(e.hex)); _espelho->println(" <Ctrl+Z>"); }
    _uart->print(A(e.hex));
    _uart->write(0x1A);                               // Ctrl+Z = "pode enviar"
    int r = _esperar(60000, nullptr, "+CMGS:");       // a rede pode demorar
    _ultimoComando = "";
    if (r != OK) {
      if (r == TEMPO_ESGOTADO) _descartarAtrasado();
      return false;
    }
  }
  return true;
}

static const char* estadoDoSms(int st) {
  switch (st) {
    case 0: return "REC UNREAD";
    case 1: return "REC READ";
    case 2: return "STO UNSENT";
    case 3: return "STO SENT";
    default: return "?";
  }
}

bool Modem::_preencherSms(int indice, int stat, const String& pduHex, Sms& sms) {
  sms = Sms();
  sms.indice = indice;
  sms.estado = estadoDoSms(stat);
  pdu::Mensagem m;
  String hex = pduHex;
  hex.trim();
  if (!pdu::decodificar(S(hex), m)) {
    sms.texto = "(não consegui decodificar: " + hex + ")";
    return true;
  }
  sms.enviada = m.tipo == 1;
  sms.numero = A(m.numero);
  sms.dataHora = A(m.dataHora);
  sms.texto = A(m.texto);
  sms.binaria = m.binaria;
  sms.ref = m.ref;
  sms.total = m.total;
  sms.parte = m.parte;
  return true;
}

bool Modem::lerSms(int indice, Sms& sms) {
  String r;
  if (!comando("AT+CMGR=" + String(indice), &r)) return false;
  // "+CMGR: <stat>,[<alpha>],<tamanho>" e, na linha de baixo, o PDU
  int nl = r.indexOf('\n');
  if (!r.startsWith("+CMGR:") || nl < 0) { _erro = "posição vazia"; return false; }
  auto f = texto::campos(S(depoisDosDoisPontos(r.substring(0, nl))));
  int stat = f.empty() ? -1 : atoi(f[0].c_str());
  return _preencherSms(indice, stat, r.substring(nl + 1), sms);
}

bool Modem::listarSms(std::vector<Sms>& lista) {
  lista.clear();
  String r;
  if (!comando("AT+CMGL=4", &r, 20000)) return false;   // 4 = todas
  int indice = -1, stat = -1;
  cadaLinha(r, [&](const String& linha) {
    if (linha.startsWith("+CMGL:")) {
      auto f = texto::campos(S(depoisDosDoisPontos(linha)));
      indice = f.size() > 0 ? atoi(f[0].c_str()) : -1;
      stat = f.size() > 1 ? atoi(f[1].c_str()) : -1;
    } else if (indice >= 0) {
      Sms s;
      _preencherSms(indice, stat, linha, s);
      lista.push_back(s);
      indice = -1;
    }
  });
  return true;
}

bool Modem::apagarSms(int indice) { return comando("AT+CMGD=" + String(indice)); }
bool Modem::apagarTodosSms()      { return comando("AT+CMGD=1,4", nullptr, 25000); }

bool Modem::ocupacaoSms(int& usados, int& total) {
  String r;
  if (!comando("AT+CPMS?", &r, _prazoConsulta)) return false;
  auto f = texto::campos(S(depoisDosDoisPontos(r)));   // "SM",3,30,"SM",3,30,"SM",3,30
  if (f.size() < 3) return false;
  usados = atoi(f[1].c_str());
  total = atoi(f[2].c_str());
  return true;
}

// ============================================================
//  USSD
// ============================================================
bool Modem::ussd(const String& codigo) {
  if (codigo.length() == 0 || codigo.length() > 60) { _erro = "código inválido"; return false; }
  // Em UCS2 a resposta chega com acentos. Ela vem alguns segundos depois
  // do OK; até lá o modem fica em UCS2, e o loop() continua rodando (o
  // telefone não trava esperando a operadora).
  _charset(true);
  _ussdChegou = false;
  _ussdEsperando = false;
  String r;
  bool ok = comando("AT+CUSD=1,\"" + A(texto::utf8ParaUcs2Hex(S(codigo))) + "\",15", &r);
  // Alguns firmwares mandam o +CUSD antes do OK: aí ele veio na resposta.
  cadaLinha(r, [&](const String& linha) { if (linha.startsWith("+CUSD:")) _tratarUrc(linha); });
  if (ok && !_ussdChegou) {
    _ussdEsperando = true;
    _ussdDesde = millis();
  } else {
    _voltarCharset = false;
    _charset(false);
  }
  return ok;
}

bool Modem::cancelarUssd() {
  _ussdEsperando = false;
  _voltarCharset = false;
  const bool ok = comando("AT+CUSD=2");
  _charset(false);
  return ok;
}

// ============================================================
//  Arquivos e toque
// ============================================================
bool Modem::gravarArquivo(const char* caminho, const uint8_t* dados, size_t tamanho) {
  String r;
  // O manual e o exemplo SetRingMusic da LilyGo usam o caminho SEM aspas;
  // com aspas fica de reserva. ",1" = cria ou apaga o que havia.
  bool aberto = comando(String("AT+FSOPEN=") + caminho + ",1", &r);
  if (!aberto) aberto = comando(String("AT+FSOPEN=\"") + caminho + "\",1", &r);
  if (!aberto) return false;
  int h = r.length() ? depoisDosDoisPontos(r).toInt() : 1;
  if (h <= 0) h = 1;

  // Em blocos de 2 KB, mandados aos poucos: a placa não liga o controle
  // de fluxo (RTS/CTS) e o modem pode perder bytes se vierem rápido demais.
  const size_t BLOCO = 2048;
  bool ok = true;
  for (size_t ini = 0; ini < tamanho && ok; ini += BLOCO) {
    size_t n = tamanho - ini < BLOCO ? tamanho - ini : BLOCO;
    String cmd = "AT+FSWRITE=" + String(h) + "," + String((unsigned)n) + ",10";
    _drenar();
    _ultimoComando = cmd;
    _enviarLinha(cmd);
    if (!_esperarLinha("CONNECT", 9000)) { ok = false; break; }
    for (size_t k = 0; k < n; k += 256) {
      size_t m = n - k < 256 ? n - k : 256;
      _uart->write(dados + ini + k, m);
      _uart->flush();
      delay(5);
    }
    String resp;
    ok = _esperar(15000, &resp, "+FSWRITE:") == OK;
    // "+FSWRITE: <...>,<tamanho total do arquivo>": confere o total
    int virgula = resp.lastIndexOf(',');
    unsigned totalArquivo = virgula >= 0 ? (unsigned)resp.substring(virgula + 1).toInt() : 0;
    if (ok && totalArquivo != ini + n) {
      _erro = "o modem gravou " + String(totalArquivo) + " de " + String((unsigned)(ini + n)) + " bytes";
      ok = false;
    }
  }
  _ultimoComando = "";
  String erroDaGravacao = _erro;
  comando("AT+FSCLOSE=" + String(h));
  if (!ok) _erro = erroDaGravacao;
  return ok;
}

bool Modem::definirToque(const char* caminho) {
  if (comando(String("AT+CRINGSET=\"") + caminho + "\",1")) return true;
  return comando(String("AT+CRINGSET=\"") + caminho + "\"");
}

bool Modem::tocarArquivo(const char* caminho) {
  return comando(String("AT+CCMXPLAY=\"") + caminho + "\",0,0");
}
