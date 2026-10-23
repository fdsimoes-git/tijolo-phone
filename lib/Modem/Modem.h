// ============================================================
//  Modem — conversa com o SIMCom A7670SA por comandos AT
//
//  O modem é um computadorzinho à parte: a ESP32 manda texto
//  ("ATD+5511999998888;" = ligar) pela serial e ele responde
//  ("OK", "ERROR"...). Ele também fala SEM ser perguntado:
//  "RING" quando chega chamada, "+CMTI: ..." quando chega SMS.
//  Essas falas espontâneas se chamam URC (unsolicited result code).
//
//  Esta classe:
//   - liga o modem e o configura (e reconfigura se ele reiniciar sozinho);
//   - manda comandos e espera a resposta (sem perder URCs no meio);
//   - transforma URCs em eventos (chamada chegando, SMS novo...);
//   - acompanha o estado da chamada (livre, discando, tocando, em ligação);
//   - manda e lê SMS no modo PDU (lib/Pdu), que entende SMS longos.
//
//  Não lê o config.h: recebe pinos e ajustes no begin(), como os
//  módulos do robô de mesa.
// ============================================================
#pragma once
#include <Arduino.h>
#include <vector>

struct PinosModem {
  int rx;        // ESP recebe aqui (TX do modem)
  int tx;        // ESP transmite aqui (RX do modem)
  int pwrkey;    // pulso liga o modem
  int dtr;       // LOW = modem acordado
  int ring;      // LOW = chegou chamada ou SMS (entrada)
};

struct Sms {
  int indice = -1;
  String estado;      // "REC UNREAD" (nova), "REC READ" (lida), "STO SENT" (enviada)...
  String numero;      // remetente (ou destino, nas guardadas); pode ser texto: "TIM"
  String dataHora;    // "26/10/21,18:05:12-12" (aa/mm/dd,hh:mm:ss±fuso em 1/4 h)
  String texto;       // UTF-8 (com acentos)
  bool enviada = false;
  bool binaria = false;
  int ref = 0, total = 1, parte = 1;   // SMS longo: esta é a parte `parte` de `total`
};

enum class EstadoChamada : uint8_t { Livre, Discando, Chamando, Tocando, EmLigacao };

struct EventoModem {
  enum Tipo : uint8_t {
    ChamadaRecebida,   // texto = número de quem liga ("" = privado)
    ChamadaConectada,  // a outra pessoa atendeu / você atendeu
    ChamadaEncerrada,  // texto = motivo e/ou duração
    ChamadaPerdida,    // texto = número
    SmsNovo,           // valor = posição do SMS no chip
    ChipPronto,
    ChipPedeSenha,     // chip com PIN: use o comando "pin 1234"
    SemChip,
    Ussd,              // texto = resposta da operadora; valor = 1 se espera resposta
    Aviso,             // texto = tensão baixa, modem reiniciou, outra chamada...
    RedeMudou,         // trocou de tecnologia (4G ↔ 2G): texto = "4G", "2G", "sem serviço"; valor = como no AT+CNSMOD
  };
  Tipo tipo;
  String texto;
  int valor = 0;
};

class Modem {
public:
  // Liga e configura. Devolve false se o modem não respondeu.
  bool begin(HardwareSerial& uart, uint32_t baud, const PinosModem& pinos);

  // Chame sempre no loop(): lê o que o modem falou e acompanha a chamada.
  void loop();

  // Próximo evento da fila (true se havia um).
  bool proximoEvento(EventoModem& ev);
  // Chamada na hora em que o evento acontece, antes de ir para a fila
  // (até no meio de um comando que ainda espera resposta). Para reações
  // que não podem esperar a volta do loop, como apagar a luz da tela. Não
  // mande comandos ao modem de dentro dela.
  void avisarNaHora(void (*f)(const EventoModem&)) { _naHora = f; }

  // Mostra no destino todo o tráfego AT (">> " enviado, "<< " recebido).
  void espelharAt(Stream* destino) { _espelho = destino; }

  // Comando genérico: "AT+CSQ" ou só "+CSQ". Junta as linhas de resposta
  // (sem o OK) em `resposta`, separadas por '\n'. true = OK.
  // 9 s é o tempo máximo de resposta da maioria dos comandos no manual.
  bool comando(const String& at, String* resposta = nullptr, uint32_t timeoutMs = 9000);
  const String& ultimoErro() const { return _erro; }
  bool tempoEsgotado() const { return _ultimoEsgotou; }   // o último comando ficou sem resposta

  // Tempo máximo das consultas de informação abaixo (sinal, operadora...).
  // A tela consulta de tempos em tempos e não pode ficar travada 9 s se o
  // modem estiver ocupado ou reiniciando.
  void prazoDasConsultas(uint32_t ms) { _prazoConsulta = ms; }

  // ----- Informações -----
  int sinal();                 // 0 a 31 (99 = sem leitura)
  String operadora();
  String tipoDeRede();         // "LTE", "GSM", "sem serviço"...
  String registro();           // "registrado", "roaming", "procurando", "negado"...
  String estadoChip();         // "pronto", "pede PIN", "sem chip"...
  int milivolts();             // tensão na entrada do modem (AT+CBC)
  String horaDaRede();         // "26/10/21,18:05:12-12" (precisa de rede)
  String imei();               // os 15 dígitos (AT+CGSN); "" se falhar

  // ----- Chamadas -----
  EstadoChamada estado() const { return _estado; }
  const String& numeroDaChamada() const { return _numero; }
  bool ligar(const String& numero);
  bool atender();
  bool desligar();                    // sempre manda AT+CHUP (também solta chamadas "presas")
  bool tons(const String& digitos);   // DTMF: "1", "123#" (menus de atendimento)
  bool chipPronto() const { return _chipPronto; }
  bool senhaDoChip(const String& pin);  // AT+CPIN=1234

  // ----- Áudio (guardado e reaplicado se o modem reiniciar) -----
  void gpioDoAmplificador(int gpioModem) { _gpioAmplificador = gpioModem; }
  bool amplificador(bool ligado);     // amplificador do alto-falante (GPIO do modem)
  bool volume(int nivel);             // 0 a 7
  bool microfone(int nivel);          // 0 a 7
  bool vivaVoz(bool ligado);          // perfil viva-voz (true) ou fone no ouvido (false)

  // ----- SMS (modo PDU) -----
  bool enviarSms(const String& numero, const String& texto, int* partes = nullptr);
  bool lerSms(int indice, Sms& sms);
  bool listarSms(std::vector<Sms>& lista);
  bool apagarSms(int indice);
  bool apagarTodosSms();
  bool ocupacaoSms(int& usados, int& total);   // quantos cabem no chip

  // ----- USSD (*222#, recarga, saldo) -----
  bool ussd(const String& codigo);    // não espera: a resposta chega como evento Ussd
  bool cancelarUssd();
  bool esperandoUssd() const { return _ussdEsperando; }

  // ----- Arquivos do modem (memória C:) e toque -----
  bool gravarArquivo(const char* caminho, const uint8_t* dados, size_t tamanho);
  bool definirToque(const char* caminho);
  bool tocarArquivo(const char* caminho);

  // ----- Liga/desliga -----
  bool reiniciar();                   // AT+CRESET e reconfigura
  // Desliga o modem de verdade (AT+CPOF; se ele continuar respondendo,
  // segura o PWRKEY por 3 s). begin() liga de novo. true = parou de responder.
  bool desligarModem();

private:
  enum Resultado { OK, ERRO, TEMPO_ESGOTADO };

  HardwareSerial* _uart = nullptr;
  PinosModem _p{};
  uint32_t _baud = 115200;
  Stream* _espelho = nullptr;
  String _linha;              // linha em construção
  String _erro;
  String _ultimoComando;      // para reconhecer o eco
  std::vector<EventoModem> _fila;
  void (*_naHora)(const EventoModem&) = nullptr;
  bool _iniciado = false;     // begin() terminou
  bool _reconfigurar = false; // o modem reiniciou sozinho

  // chamada
  EstadoChamada _estado = EstadoChamada::Livre;
  int _idChamada = -1;        // número da chamada no +CLCC que estamos acompanhando
  String _numero;
  bool _avisouChamada = false;
  uint32_t _ultimoRing = 0;
  uint32_t _ultimaConsulta = 0;
  uint32_t _inicioLigacao = 0;
  bool _consultando = false;
  String _motivoPendente;     // "recusada"/"você desligou", até o modem confirmar
  String _motivoUrc;          // "ocupado"/"não atendeu", dito pelo modem

  bool _chipPronto = false;
  bool _smsPronto = false;
  int _sistema = -1;                   // geração da rede pelos avisos (0, 2, 3 ou 4; -1 = não sei)
  bool _lte = false, _cs = false;      // registrado no 4G (+CEREG) e no de voz (+CREG)
  bool _configSmsPendente = false;   // chip ficou pronto depois do begin()
  bool _ussdChegou = false;
  bool _ussdEsperando = false;       // pediu um USSD e a resposta ainda não veio
  uint32_t _ussdDesde = 0;
  bool _voltarCharset = false;       // a resposta veio: voltar para IRA no loop()
  uint32_t _prazoConsulta = 9000;
  bool _ultimoEsgotou = false;
  int _charsetUcs2 = -1;             // -1 = não sei, 0 = IRA, 1 = UCS2

  // áudio desejado (-1 = não mexer)
  int _gpioAmplificador = -1;
  int _amplificador = -1, _volume = -1, _microfone = -1, _vivaVoz = -1;

  // baixo nível
  void _enviarLinha(const String& cmd);
  bool _lerLinha(String& out);
  void _drenar();
  int _esperar(uint32_t timeoutMs, String* resposta, const String& prefixo, bool finaisDeChamada = false);
  void _descartarAtrasado(uint32_t timeoutMs = 3000);
  bool _esperarPrompt(uint32_t timeoutMs);
  bool _esperarLinha(const char* texto, uint32_t timeoutMs);
  bool _responde(uint32_t timeoutMs);
  void _pulsoPwrkey();
  bool _configurar();
  void _configurarSms();
  void _aplicarAudio();
  bool _aplicarAmplificador();
  static String _prefixoDe(const String& cmd);

  // URCs e chamada
  bool _ehUrc(const String& linha) const;
  void _tratarUrc(const String& linha);
  void _aplicarClcc(int id, int stat, const String& numero);
  void _publicar(EventoModem::Tipo tipo, const String& texto = "", int valor = 0);
  void _mudouGeracao(int geracao, int valor);
  static bool _registradoNaResposta(const String& resposta);
  void _mudarEstado(EstadoChamada novo, const String& motivo = "");
  void _consultarChamadas();
  String _duracao() const;

  bool _charset(bool ucs2);
  bool _preencherSms(int indice, int stat, const String& pduHex, Sms& sms);
};
