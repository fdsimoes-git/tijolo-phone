// ============================================================
//  Interface — as telas do Tijolo e o que cada tecla faz em cada uma
//
//  Recebe as teclas e os acontecimentos do modem (chamada chegando,
//  SMS, resposta da operadora), decide qual tela mostrar e a desenha
//  num Quadro. Quem manda nos fios é o main.cpp: a interface só pede
//  coisas por meio da classe Telefone ("ligue para tal número").
//
//  Não depende do Arduino nem do modem: no computador, test_host/
//  liga a interface a um telefone de mentira e salva cada tela numa
//  imagem.
// ============================================================
#pragma once
#include <stdint.h>
#include <deque>
#include <string>
#include <vector>
#include "Quadro.h"
#include "Teclado.h"

// O que a interface sabe do aparelho; o main atualiza de tempos em tempos.
struct Situacao {
  int barras = -1;          // sinal: 0 a 4; -1 = sem sinal ou sem serviço
  std::string rede;         // "4G", "2G" ou ""
  std::string operadora;    // "TIM"
  std::string chip;         // "" = chip ok; senão o problema ("sem chip", "chip pede PIN")
  int bateria = -1;         // 0 a 100 %; -1 = ainda não sei
  bool horaValida = false;  // a hora vem da rede; antes disso, "--:--"
  int ano = 0, mes = 0, dia = 0, hora = 0, minuto = 0;
  int chamada = 0;          // 0 livre, 1 discando, 2 chamando, 3 tocando, 4 em ligação
  std::string numero;       // de quem é a chamada, segundo o modem
};

// O que a interface pede ao resto do aparelho.
class Telefone {
public:
  virtual ~Telefone() = default;
  virtual bool ligar(const std::string& numero) = 0;
  virtual bool atender() = 0;
  virtual bool encerrar() = 0;
  virtual void tom(char tecla) = 0;                    // DTMF durante a ligação
  virtual bool ussd(const std::string& codigo) = 0;    // a resposta chega por Interface::ussd()
  virtual void cancelarUssd() = 0;
  virtual void volume(int nivel) = 0;                  // 0 a 7
  virtual std::string imei() = 0;
  virtual std::string erro() = 0;                      // último erro do modem
  virtual void aprenderTeclado(bool sim) = 0;
  virtual void salvarTeclado(const uint8_t mapa[Teclado::TECLAS]) = 0;
  // A mensagem (nas posições do chip) foi vista e fechada: sai da lista
  // de não lidas que sobrevive a desligar o aparelho.
  virtual void mensagemLida(const std::vector<int>& posicoes) = 0;
};

enum class Tela : uint8_t {
  Inicio, Discador, Chamando, Recebendo, EmLigacao, Encerrada, Perdida, Perdidas,
  Mensagem, UssdEspera, Ussd, Imei, Desligar, Desligando, Teclado
};

class Interface {
public:
  // nomesFios: nomes dos pinos do teclado ("IO40"...), só para mostrar.
  void begin(Telefone& tel, int volume, const char* const* nomesFios, uint32_t agora);
  void brilhoMaximo(uint8_t b) { _brilhoMax = b; }
  // Teclas que, seguradas, fazem as vezes dos botões ('1' e '3'): as dicas
  // da tela mostram o número dentro da bolinha verde e da vermelha.
  // comoLigar: o que a tela de desligar diz para ligar de novo.
  void teclasComoBotoes(char verde, char vermelho, const std::string& comoLigar) {
    _digVerde = verde;
    _digVermelho = vermelho;
    _comoLigar = comoLigar;
  }

  // ----- O que acontece no aparelho -----
  void situacao(const Situacao& s, uint32_t agora);
  void tecla(const EventoTecla& ev, uint32_t agora);
  void chamadaRecebida(const std::string& numero, uint32_t agora);
  void chamadaConectada(const std::string& numero, uint32_t agora);
  void chamadaEncerrada(const std::string& motivo, uint32_t agora);
  void chamadaPerdida(const std::string& numero, uint32_t agora);
  void mensagem(const std::string& numero, const std::string& dataHora, const std::string& texto,
                const std::vector<int>& posicoes, uint32_t agora);
  void ussd(const std::string& texto, bool esperaResposta, uint32_t agora);
  void aviso(const std::string& texto, uint32_t agora);
  void aprenderTeclado(uint32_t agora);
  void tique(uint32_t agora);               // relógios internos; chame sempre

  // ----- Desenho -----
  bool precisaDesenhar(uint32_t agora) const;
  void desenhar(Quadro& q, uint32_t agora);
  // Sem tela: conta como desenhado (ligar e USSD esperam o desenho para agir).
  void desenhado(uint32_t agora);
  static void desenharLigando(Quadro& q, const std::string& texto);
  static void desenharDesligado(Quadro& q);

  void redesenhar() { _sujo = true; }      // a tela foi usada por outro (teste): desenhe de novo
  uint8_t brilho(uint32_t agora) const;     // luz da tela: 0 a 255
  bool pediuDesligar() const { return _pediuDesligar; }

  // ----- Para os testes -----
  Tela tela() const { return _tela; }
  const std::string& digitado() const { return _digitado; }
  int volume() const { return _volume; }
  size_t mensagensNaoLidas() const { return _mensagens.size(); }
  size_t chamadasPerdidas() const { return _perdidas.size(); }

  // Ajudantes de texto (também usados no main)
  static std::string formatarNumero(const std::string& n);
  static bool ehCodigoUssd(const std::string& s);

private:
  struct Perdida { std::string numero, quando; };
  struct Mensagem { std::string numero, quando, texto; std::vector<int> posicoes; };
  enum class Acao : uint8_t { Nenhuma, Ligar, Ussd };

  void _ir(Tela t, uint32_t agora);
  void _acordar(uint32_t agora) { _atividade = agora; }
  void _pedir(Acao a, const std::string& arg);
  void _executar(uint32_t agora);
  void _ligar(std::string numero, uint32_t agora);       // por cópia: quem chama pode
  void _enviarUssd(std::string codigo, uint32_t agora);  // estar passando um membro
  void _fimTeclado();
  void _parTeclado(uint8_t a, uint8_t b, uint32_t agora);
  void _parSegurado(uint8_t a, uint8_t b, uint32_t agora);
  void _mudarVolume(int delta, uint32_t agora);
  std::string _agoraTexto() const;           // "18:05" (ou "")
  std::string _quando(const std::string& dataHora) const;
  uint32_t _chave(uint32_t agora) const;
  bool _emChamada() const;

  // teclas, por tela
  void _teclaInicio(const EventoTecla& ev, uint32_t agora);
  void _teclaDiscador(const EventoTecla& ev, uint32_t agora);
  void _teclaChamada(const EventoTecla& ev, uint32_t agora);
  void _teclaPerdidas(const EventoTecla& ev, uint32_t agora);
  void _teclaMensagem(const EventoTecla& ev, uint32_t agora);
  void _teclaUssd(const EventoTecla& ev, uint32_t agora);
  void _teclaTeclado(const EventoTecla& ev, uint32_t agora);

  // desenho
  void _barra(Quadro& q, bool relogio) const;
  void _dicas(Quadro& q, const std::string& verde, const std::string& vermelha,
              const std::string& meio = "") const;
  void _pilula(Quadro& q, int x, int y, int w, int h, uint16_t cor, const std::string& texto,
               char digito = 0) const;
  void _bolinha(Quadro& q, float cx, float cy, uint16_t cor, char digito) const;
  void _pontinhos(Quadro& q, int y, uint32_t agora) const;
  int _textoLongo(Quadro& q, const std::vector<std::string>& linhas, const Fonte& f, int y0,
                  int y1, int rolagem, uint16_t cor) const;
  void _desenharInicio(Quadro& q) const;
  void _desenharDiscador(Quadro& q) const;
  void _desenharChamando(Quadro& q, uint32_t agora) const;
  void _desenharRecebendo(Quadro& q, uint32_t agora) const;
  void _desenharEmLigacao(Quadro& q, uint32_t agora) const;
  void _desenharEncerrada(Quadro& q) const;
  void _desenharPerdida(Quadro& q) const;
  void _desenharPerdidas(Quadro& q) const;
  void _desenharMensagem(Quadro& q);
  void _desenharUssdEspera(Quadro& q, uint32_t agora) const;
  void _desenharUssd(Quadro& q);
  void _desenharImei(Quadro& q) const;
  void _desenharDesligar(Quadro& q) const;
  void _desenharTeclado(Quadro& q) const;
  void _desenharAviso(Quadro& q) const;
  void _desenharVolume(Quadro& q) const;

  Telefone* _tel = nullptr;
  const char* const* _nomesFios = nullptr;
  Situacao _s;
  Tela _tela = Tela::Inicio;
  uint32_t _desde = 0;            // quando a tela atual apareceu
  bool _sujo = true;              // precisa redesenhar
  uint32_t _chaveDesenhada = 0;
  uint32_t _desenhos = 0;

  // discador e chamada
  std::string _digitado, _ultimoDiscado;
  std::string _numeroChamada, _motivo, _tons;
  uint32_t _conectouEm = 0;
  bool _foiRecebida = false;
  bool _falhouAoLigar = false;    // o modem recusou o ATD
  uint32_t _semChamadaDesde = 0;  // o modem diz "livre", mas a tela mostra chamada
  uint32_t _comChamadaDesde = 0;  // o modem diz "chamada", mas a tela não mostra
  void _fecharMensagem();         // a da frente foi vista: sai da lista

  // notificações
  std::vector<Perdida> _perdidas;
  int _selecao = 0;
  std::deque<Mensagem> _mensagens;
  int _rolagem = 0, _linhasTotal = 0, _linhasVisiveis = 0;

  // operadora (USSD)
  std::string _ussdCodigo, _ussdTexto, _ussdResposta;
  bool _ussdMenu = false;

  // ação que espera a tela ser desenhada (ligar e USSD travam o loop)
  Acao _acao = Acao::Nenhuma;
  std::string _acaoArg;
  uint32_t _acaoDesenho = 0;

  std::string _aviso;
  uint32_t _avisoAte = 0;
  int _volume = 5;
  uint32_t _volumeAte = 0;
  std::string _imei;

  // aprender o teclado
  int _passo = 0;
  uint8_t _pares[Teclado::TECLAS] = {};
  uint8_t _parNovo = Teclado::SEM_PAR;   // o par que o último toque acabou de ensinar
  std::string _msgTeclado;
  bool _tecladoFim = false, _tecladoOk = false;
  int _linhasFio[4] = {}, _colunasFio[3] = {};

  // teclas no lugar dos botões (0 = botões de verdade)
  char _digVerde = 0, _digVermelho = 0;
  std::string _comoLigar = "Para ligar de novo, segure o botão vermelho.";

  // luz e energia
  uint32_t _atividade = 0;
  uint8_t _brilhoMax = 255;
  char _engolir = 0;              // tecla que só acendeu a tela
  Tela _telaDoAperto[128] = {};   // em que tela cada tecla desceu
  bool _pediuDesligar = false;
  uint32_t _bateriaAcabandoDesde = 0;
  bool _avisouBateria = false;
};
