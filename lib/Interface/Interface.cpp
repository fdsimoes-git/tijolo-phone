#include "Interface.h"
#include <stdio.h>
#include <algorithm>
#include "Pdu.h"

using namespace fontes;

namespace {

namespace cor {
constexpr uint16_t FUNDO = rgb(0, 0, 0);
constexpr uint16_t TEXTO = rgb(242, 244, 247);
constexpr uint16_t SUAVE = rgb(150, 158, 170);
constexpr uint16_t DISCRETO = rgb(105, 113, 124);
constexpr uint16_t APAGADO = rgb(58, 64, 74);
constexpr uint16_t CARTAO = rgb(28, 32, 39);
constexpr uint16_t AVISO = rgb(52, 58, 68);
constexpr uint16_t DIVISOR = rgb(44, 49, 58);
constexpr uint16_t VERDE = rgb(48, 209, 88);
constexpr uint16_t VERMELHO = rgb(255, 69, 58);
constexpr uint16_t AMBAR = rgb(255, 184, 48);
constexpr uint16_t AZUL = rgb(100, 190, 255);
constexpr uint16_t TIJOLO = rgb(205, 92, 60);
}  // namespace cor

constexpr int L = Quadro::LARGURA;
constexpr int MARGEM = 16;

// Quanto tempo cada coisa fica na tela (ms)
constexpr uint32_t TEMPO_ENCERRADA = 2500;
constexpr uint32_t TEMPO_ENVIADA = 2500;
constexpr uint32_t TEMPO_PERDIDA = 6000;
constexpr uint32_t TEMPO_CONFIRMAR = 10000;
constexpr uint32_t TEMPO_IMEI = 20000;
constexpr uint32_t TEMPO_TECLADO_PRONTO = 2500;
constexpr uint32_t ESPERA_USSD = 30000;
constexpr uint32_t TEMPO_AVISO = 4000;
constexpr uint32_t TEMPO_VOLUME = 2000;
// Luz da tela
constexpr uint32_t LUZ_CHEIA = 15000, LUZ_FRACA = 30000, LUZ_LIGACAO = 10000;
// Bateria: abaixo de 2 % por 1 minuto, o telefone se desliga sozinho
constexpr uint32_t BATERIA_ACABANDO = 60000;

const char* const DIAS[7] = {"domingo", "segunda", "terça", "quarta", "quinta", "sexta", "sábado"};
const char* const MESES[12] = {"janeiro", "fevereiro", "março", "abril", "maio", "junho",
                               "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"};

int diaDaSemana(int a, int m, int d) {   // 0 = domingo (Sakamoto)
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) a -= 1;
  return (a + a / 4 - a / 100 + a / 400 + t[(m - 1) % 12] + d) % 7;
}

std::string dois(int v) {
  char b[8];
  snprintf(b, sizeof b, "%02d", v);
  return b;
}

std::string duracao(uint32_t ms) {
  const uint32_t s = ms / 1000;
  char b[16];
  if (s >= 3600) snprintf(b, sizeof b, "%u:%02u:%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
  else snprintf(b, sizeof b, "%u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
  return b;
}

bool ehDigito(char k) { return k >= '0' && k <= '9'; }
bool ehTeclaDeNumero(char k) { return ehDigito(k) || k == '*' || k == '#'; }
bool antes(uint32_t agora, uint32_t ate) { return ate && (int32_t)(agora - ate) < 0; }

std::string maiuscula(std::string s) {
  if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = (char)(s[0] - 32);
  return s;
}

// O começo do texto, numa linha só (para as prévias): sem quebras de linha e
// cortado num caractere inteiro
std::string comecoDoTexto(const std::string& t, size_t maxBytes = 160) {
  size_t n = t.size() < maxBytes ? t.size() : maxBytes;
  while (n > 0 && n < t.size() && (((uint8_t)t[n]) & 0xC0) == 0x80) n--;
  std::string r = t.substr(0, n);
  for (char& c : r)
    if (c == '\n' || c == '\r') c = ' ';
  return r;
}

const std::vector<Caixa::Item> NENHUMA;

}  // namespace

// ============================================================
// Começo e acontecimentos
// ============================================================

void Interface::begin(Telefone& tel, int volume, const char* const* nomesFios, uint32_t agora) {
  _tel = &tel;
  _volume = volume;
  _nomesFios = nomesFios;
  _tela = Tela::Inicio;
  _desde = agora;
  _atividade = agora;
  _sujo = true;
  // A letra que deixaria a mensagem maior que o máximo de SMS não entra
  // (no ciclo da tecla, ela é pulada)
  _mt.limitar([this](const std::string& t) { return pdu::contar(t).partes <= _maxPartes; });
}

uint32_t Interface::_relogioAgora(uint32_t antes) const {
  if (!_relogio) return antes;
  const uint32_t r = _relogio();
  return (int32_t)(r - antes) > 0 ? r : antes;
}

void Interface::_ir(Tela t, uint32_t agora) {
  if (_tela == Tela::Teclado && t != Tela::Teclado && _tel) _tel->aprenderTeclado(false);
  // Saindo do texto (o 3 segurado, uma chamada, a bateria acabando...): ele
  // vira rascunho na caixa, nunca se perde
  const bool escrevendo = _tela == Tela::Escrever || _tela == Tela::Para;
  const bool continua = t == Tela::Escrever || t == Tela::Para || t == Tela::Enviando;
  if (escrevendo && !continua) _deixarRascunho(t, agora);
  _tela = t;
  _desde = agora;
  _rolagem = 0;
  _sujo = true;
  _acordar(agora);
  if (t == Tela::Inicio && !_avisoDepois.empty()) {      // o aviso que esperou a chamada acabar
    aviso(_avisoDepois, agora);
    _avisoDepois.clear();
  }
}

void Interface::_pedir(Acao a, const std::string& arg) {
  _acao = a;
  _acaoArg = arg;
  _acaoDesenho = _desenhos;
}

void Interface::situacao(const Situacao& s, uint32_t) {
  const Situacao& v = _s;
  if (s.barras != v.barras || s.rede != v.rede || s.operadora != v.operadora || s.chip != v.chip ||
      s.bateria != v.bateria || s.horaValida != v.horaValida || s.hora != v.hora ||
      s.minuto != v.minuto || s.dia != v.dia || s.chamada != v.chamada)
    _sujo = true;
  _s = s;
}

void Interface::chamadaRecebida(const std::string& numero, uint32_t agora) {
  _numeroChamada = numero;
  _foiRecebida = true;
  _tons.clear();
  _motivo.clear();
  _conectouEm = 0;
  if (_acao == Acao::EnviarSms) _envioAdiado();   // o SMS ainda não tinha saído
  _acao = Acao::Nenhuma;
  _ir(Tela::Recebendo, agora);
}

void Interface::chamadaConectada(const std::string& numero, uint32_t agora) {
  if (!numero.empty()) _numeroChamada = numero;
  _conectouEm = agora;
  _ir(Tela::EmLigacao, agora);
}

void Interface::chamadaEncerrada(const std::string& motivo, uint32_t agora) {
  _motivo = motivo;
  _falhouAoLigar = false;
  _volumeAte = 0;
  _ir(Tela::Encerrada, agora);
}

void Interface::chamadaPerdida(const std::string& numero, uint32_t agora) {
  _perdidas.insert(_perdidas.begin(), Perdida{numero, _agoraTexto()});
  if (_perdidas.size() > 20) _perdidas.pop_back();
  _numeroChamada = numero;
  _ir(Tela::Perdida, agora);
}

uint32_t Interface::mensagem(const std::string& numero, const std::string& dataHora, const std::string& texto,
                             uint32_t agora) {
  const uint32_t id = _caixa ? _caixa->guardar(Caixa::Nova, numero, dataHora, texto) : 0;
  if (!id) {
    aviso("Chegou mensagem de " + formatarNumero(numero) + ", mas ela não coube na memória: ficou no chip.",
          agora);
    return 0;
  }
  _cartaoMensagens = true;
  if (_tela == Tela::Inicio) {
    _abrirMensagem(id, Tela::Inicio, false, agora);   // abre sozinha; lida só quando alguém mexer
  } else {
    caixaMudou(agora);                              // (cheia, a caixa pode ter tirado a que está aberta)
    if (!_emChamada()) aviso("Nova mensagem de " + formatarNumero(numero), agora);
  }
  return id;
}

void Interface::caixaMudou(uint32_t agora) {
  _sujo = true;
  if (!_caixa) return;
  const bool comMensagem = _tela == Tela::Ler || _tela == Tela::Opcoes || _tela == Tela::Apagar;
  if (comMensagem && _caixa->posicao(_lendo) < 0) {        // a aberta foi apagada
    if (_lerVolta == Tela::Mensagens) _abrirLista(_listaVolta, 0, agora);
    else _ir(Tela::Inicio, agora);
  }
}

void Interface::ussd(const std::string& texto, bool esperaResposta, uint32_t agora) {
  _ussdTexto = texto;
  _ussdMenu = esperaResposta;
  _ussdResposta.clear();
  if (_emChamada()) {
    aviso("Operadora: " + texto, agora);
    return;
  }
  _ir(Tela::Ussd, agora);
}

void Interface::aviso(const std::string& texto, uint32_t agora) {
  _aviso = texto;
  _avisoAte = (agora + TEMPO_AVISO) | 1;
  _sujo = true;
  _acordar(agora);
}

void Interface::aprenderTeclado(uint32_t agora) {
  _passo = 0;
  _parNovo = Teclado::SEM_PAR;
  _tecladoFim = _tecladoOk = false;
  _msgTeclado.clear();
  _ir(Tela::Teclado, agora);
  _tel->aprenderTeclado(true);
}

bool Interface::_emChamada() const {
  return _tela == Tela::Chamando || _tela == Tela::Recebendo || _tela == Tela::EmLigacao;
}

bool Interface::_temNotificacoes() const {
  return !_perdidas.empty() || (_cartaoMensagens && _caixa && _caixa->novas() > 0);
}

// ============================================================
// Relógios internos
// ============================================================

void Interface::tique(uint32_t agora) {
  if (_executar(agora)) agora = _relogioAgora(agora);   // a ação travou o loop: o relógio andou
  const uint32_t naTela = agora - _desde;
  switch (_tela) {
    case Tela::Encerrada: if (naTela > TEMPO_ENCERRADA) _ir(Tela::Inicio, agora); break;
    case Tela::Perdida: if (naTela > TEMPO_PERDIDA) _ir(Tela::Inicio, agora); break;
    case Tela::Desligar: if (naTela > TEMPO_CONFIRMAR) _ir(Tela::Inicio, agora); break;
    case Tela::Imei: if (naTela > TEMPO_IMEI) _ir(Tela::Inicio, agora); break;
    case Tela::Escrever: if (_mt.tique(agora)) _sujo = true; break;   // a letra pendente ficou
    case Tela::Enviado: if (_envioOk && naTela > TEMPO_ENVIADA) _ir(Tela::Inicio, agora); break;
    case Tela::Teclado:
      if (_tecladoFim && _tecladoOk && naTela > TEMPO_TECLADO_PRONTO) _ir(Tela::Inicio, agora);
      break;
    case Tela::UssdEspera:
      if (_acao == Acao::Nenhuma && naTela > ESPERA_USSD) {
        _ussdTexto = "A operadora não respondeu.";
        _ussdMenu = false;
        _ir(Tela::Ussd, agora);
      }
      break;
    default: break;
  }

  // A tela apagou: volta para o início, como o bloqueio de um celular.
  // Escrevendo, não: a próxima tecla acende a tela no mesmo lugar.
  if (brilho(agora) == 0 && !_emChamada() && _tela != Tela::Inicio && _tela != Tela::Teclado &&
      _tela != Tela::UssdEspera && _tela != Tela::Desligando && _tela != Tela::Escrever &&
      _tela != Tela::Para && _tela != Tela::Enviando) {
    if (_tela == Tela::Ussd && _ussdMenu) _tel->cancelarUssd();
    _digitado.clear();
    _tela = Tela::Inicio;
    _desde = agora;
    _sujo = true;
  }

  // O modem é quem sabe se a chamada acabou: se ele diz "livre" por um
  // tempo e a tela ainda mostra chamada, algum aviso se perdeu.
  if (_emChamada() && _s.chamada == 0 && _acao == Acao::Nenhuma) {
    if (!_semChamadaDesde) _semChamadaDesde = agora | 1;
    else if (agora - _semChamadaDesde > 4000) {
      _semChamadaDesde = 0;
      chamadaEncerrada("", agora);
    }
  } else {
    _semChamadaDesde = 0;
  }
  // E o contrário: chamada feita ou atendida pelo Monitor Serial.
  const bool livreParaChamada = !_emChamada() && _tela != Tela::Encerrada && _tela != Tela::Perdida &&
                                _tela != Tela::Desligando && _acao == Acao::Nenhuma;
  if (livreParaChamada && (_s.chamada == 1 || _s.chamada == 2 || _s.chamada == 4)) {
    if (!_comChamadaDesde) {
      _comChamadaDesde = agora | 1;
    } else if (agora - _comChamadaDesde > 1000) {
      _comChamadaDesde = 0;
      _numeroChamada = _s.numero;
      _foiRecebida = false;
      _falhouAoLigar = false;
      _tons.clear();
      if (_s.chamada == 4) {
        _conectouEm = agora;
        _ir(Tela::EmLigacao, agora);
      } else {
        _ir(Tela::Chamando, agora);
      }
    }
  } else {
    _comChamadaDesde = 0;
  }

  // Bateria
  if (_s.bateria >= 0) {
    if (_s.bateria <= 10 && !_avisouBateria) {
      _avisouBateria = true;
      aviso("Bateria fraca (" + std::to_string(_s.bateria) + "%). Ligue o carregador.", agora);
    }
    if (_s.bateria > 15) _avisouBateria = false;
    if (_s.bateria <= 2 && !_emChamada() && _tela != Tela::Desligando) {
      if (!_bateriaAcabandoDesde) _bateriaAcabandoDesde = agora | 1;
      else if (agora - _bateriaAcabandoDesde > BATERIA_ACABANDO) {
        _ir(Tela::Desligando, agora);
        _pediuDesligar = true;
      }
    } else {
      _bateriaAcabandoDesde = 0;
    }
  }
}

// Só depois que a tela nova foi desenhada: ligar, USSD e SMS seguram o loop.
// Depois, as telas contam o tempo a partir de quando a ação acabou.
bool Interface::_executar(uint32_t agora) {
  if (_acao == Acao::Nenhuma || _desenhos == _acaoDesenho) return false;
  const Acao a = _acao;
  const std::string arg = _acaoArg;
  _acao = Acao::Nenhuma;
  if (a == Acao::Ligar) {
    const bool ok = _tel->ligar(arg);
    agora = _relogioAgora(agora);
    _acordar(agora);
    if (!ok) {
      _motivo = _tel->erro();
      _falhouAoLigar = true;
      _ir(Tela::Encerrada, agora);
    }
  } else if (a == Acao::Ussd) {
    const bool ok = _tel->ussd(arg);
    agora = _relogioAgora(agora);
    if (_tela == Tela::UssdEspera) _desde = agora;     // a espera pela resposta começa agora
    if (!ok && _tela == Tela::UssdEspera) {
      _ussdTexto = "A operadora não respondeu.";
      const std::string e = _tel->erro();
      if (!e.empty()) _ussdTexto += " (" + e + ")";
      _ussdMenu = false;
      _ir(Tela::Ussd, agora);
    }
  } else if (a == Acao::EnviarSms) {
    _enviarAgora(agora);
  }
  return true;
}

void Interface::_ligar(std::string numero, uint32_t agora) {
  _numeroChamada = numero;
  _ultimoDiscado = numero;
  _foiRecebida = false;
  _falhouAoLigar = false;
  _tons.clear();
  _motivo.clear();
  _conectouEm = 0;
  _ir(Tela::Chamando, agora);
  _pedir(Acao::Ligar, numero);
}

void Interface::_enviarUssd(std::string codigo, uint32_t agora) {
  _ussdCodigo = codigo;
  _ussdResposta.clear();
  _ir(Tela::UssdEspera, agora);
  _pedir(Acao::Ussd, codigo);
}

void Interface::_mudarVolume(int delta, uint32_t agora) {
  int v = _volume + delta;
  if (v < 0) v = 0;
  if (v > 7) v = 7;
  if (v != _volume) {
    _volume = v;
    _tel->volume(v);
  }
  _volumeAte = (agora + TEMPO_VOLUME) | 1;
  _sujo = true;
}

uint8_t Interface::brilho(uint32_t agora) const {
  if (_tela == Tela::Recebendo || _tela == Tela::Desligando || _tela == Tela::UssdEspera ||
      _tela == Tela::Enviando)
    return _brilhoMax;
  const uint32_t parado = agora - _atividade;
  if (_tela == Tela::Chamando || _tela == Tela::EmLigacao) return parado < LUZ_LIGACAO ? _brilhoMax : 0;
  if (parado < LUZ_CHEIA) return _brilhoMax;
  if (parado < LUZ_FRACA) return (uint8_t)(_brilhoMax / 6 + 1);
  return 0;
}

// ============================================================
// Teclas
// ============================================================

void Interface::tecla(const EventoTecla& ev, uint32_t agora) {
  if (ev.tipo == EventoTecla::Par) {
    if (_tela != Tela::Teclado) return;
    if (brilho(agora) == 0) {      // tela apagada: a tecla só acende, não conta como passo
      _acordar(agora);
      _sujo = true;
      return;
    }
    _parTeclado(ev.a, ev.b, agora);
    return;
  }
  if (ev.tipo == EventoTecla::ParSegurado) {
    if (_tela == Tela::Teclado && brilho(agora) > 0) _parSegurado(ev.a, ev.b, agora);
    return;
  }
  const char k = ev.tecla;
  const uint8_t ik = (uint8_t)k & 0x7F;
  // O 1 e o 3 só dão evento ao soltar ou depois de 0,8 s segurados. Se a
  // tela mudou depois que a tecla desceu (chegou uma chamada, uma mensagem),
  // o aperto não vale nela: segurar o 1 para rediscar não atende quem acabou
  // de ligar. O "segurou" e o "soltou" dessa tecla também ficam de fora.
  // Exceção: o vermelho com a ligação em andamento (a pessoa atendeu bem na
  // hora em que você segurava o 3 para desistir: encerra).
  if (ev.tipo == EventoTecla::Apertou && ev.origem && (int32_t)(_desde - ev.quando) > 0 &&
      !(k == TECLA_VERMELHA && (_tela == Tela::Chamando || _tela == Tela::EmLigacao))) {
    _telaDoAperto[ik] = static_cast<Tela>(0xFF);
    return;
  }
  if (_engolir) {                 // a tecla que só acendeu a tela
    if (k == _engolir) {
      if (ev.tipo == EventoTecla::Soltou) _engolir = 0;
      return;
    }
  }
  if (ev.tipo == EventoTecla::Apertou) {
    if (brilho(agora) == 0 && !_emChamada()) {
      _engolir = k;
      _acordar(agora);
      _sujo = true;
      return;
    }
  } else if (_telaDoAperto[ik] != _tela) {
    return;    // a tecla desceu em outra tela: o "segurou" e o "soltou" não valem aqui
  }
  _acordar(agora);

  const Tela antesDaTecla = _tela;
  switch (_tela) {
    case Tela::Inicio: _teclaInicio(ev, agora); break;
    case Tela::Discador: _teclaDiscador(ev, agora); break;
    case Tela::Chamando:
    case Tela::Recebendo:
    case Tela::EmLigacao: _teclaChamada(ev, agora); break;
    case Tela::Encerrada:
      if (ev.tipo == EventoTecla::Apertou) {
        _ir(Tela::Inicio, agora);
        if (ehTeclaDeNumero(k)) _teclaInicio(ev, agora);
      }
      break;
    case Tela::Perdida:
      if (ev.tipo != EventoTecla::Apertou) break;
      if (k == TECLA_VERDE && !_perdidas.empty()) {
        const std::string n = _perdidas.front().numero;
        _perdidas.erase(_perdidas.begin());
        if (!n.empty()) _ligar(n, agora);
        else _ir(Tela::Inicio, agora);
      } else if (k == TECLA_VERMELHA) {
        _ir(Tela::Inicio, agora);
      } else if (ehTeclaDeNumero(k)) {
        _ir(Tela::Inicio, agora);
        _teclaInicio(ev, agora);
      }
      break;
    case Tela::Perdidas: _teclaPerdidas(ev, agora); break;
    case Tela::Menu: _teclaMenu(ev, agora); break;
    case Tela::Escrever: _teclaEscrever(ev, agora); break;
    case Tela::Para: _teclaPara(ev, agora); break;
    case Tela::Enviando: break;    // o loop está parado esperando o modem
    case Tela::Enviado: _teclaEnviado(ev, agora); break;
    case Tela::Mensagens: _teclaLista(ev, agora); break;
    case Tela::Ler: _teclaLer(ev, agora); break;
    case Tela::Opcoes: _teclaOpcoes(ev, agora); break;
    case Tela::Apagar: _teclaApagar(ev, agora); break;
    case Tela::UssdEspera:
      if (ev.tipo == EventoTecla::Apertou && k == TECLA_VERMELHA) {
        _tel->cancelarUssd();
        _ir(Tela::Inicio, agora);
      }
      break;
    case Tela::Ussd: _teclaUssd(ev, agora); break;
    case Tela::Imei:
      if (ev.tipo == EventoTecla::Apertou) _ir(Tela::Inicio, agora);
      break;
    case Tela::Desligar:
      if (ev.tipo != EventoTecla::Apertou) break;
      if (k == TECLA_VERDE) {
        _ir(Tela::Desligando, agora);
        _pediuDesligar = true;
      } else if (!_digVerde || k != _digVerde) {   // um toque curto no 1 só não chegou a segurar
        _ir(Tela::Inicio, agora);
      }
      break;
    case Tela::Desligando: break;
    case Tela::Teclado: _teclaTeclado(ev, agora); break;
  }
  if (ev.tipo == EventoTecla::Apertou) {
    // Lembra em que tela a tecla desceu. Exceção: a tecla que abriu o
    // discador conta como do discador (segurar o 0 dá o "+").
    _telaDoAperto[ik] = (_tela == Tela::Discador && antesDaTecla != Tela::Discador) ? _tela : antesDaTecla;
  }
}

void Interface::_teclaInicio(const EventoTecla& ev, uint32_t agora) {
  const char k = ev.tecla;
  if (ev.tipo == EventoTecla::Apertou) {
    if (ehTeclaDeNumero(k)) {
      _digitado = std::string(1, k);
      _ir(Tela::Discador, agora);
    } else if (k == TECLA_VERDE) {
      // "Ver" as novidades (primeiro as mensagens) ou abrir o menu
      const size_t novas = _cartaoMensagens && _caixa ? _caixa->novas() : 0;
      if (novas == 1) {
        _abrirMensagem(_caixa->primeiraNova(), Tela::Inicio, true, agora);
      } else if (novas > 1) {
        _abrirLista(Tela::Inicio, _novaMaisRecente(), agora);
      } else if (!_perdidas.empty()) {
        _selecao = 0;
        _ir(Tela::Perdidas, agora);
      } else {
        _menuSel = 0;
        _ir(Tela::Menu, agora);
      }
    }
  } else if (ev.tipo == EventoTecla::Soltou && k == TECLA_VERMELHA && !ev.segurada) {
    // "Limpar": as novidades saem da tela inicial (as mensagens continuam
    // marcadas como novas na lista). Ao soltar, para não limpar quem só
    // estava segurando para desligar.
    if (_temNotificacoes()) {
      _perdidas.clear();
      _cartaoMensagens = false;
      _sujo = true;
    }
  } else if (ev.tipo == EventoTecla::Segurou && k == TECLA_VERMELHA) {
    _ir(Tela::Desligar, agora);
  }
}

void Interface::_teclaDiscador(const EventoTecla& ev, uint32_t agora) {
  const char k = ev.tecla;
  if (ev.tipo == EventoTecla::Apertou) {
    if (ehTeclaDeNumero(k)) {
      if (_digitado.size() < 40) _digitado += k;
      _sujo = true;
      if (_digitado == "*#06#") {          // código universal: mostra o IMEI
        _digitado.clear();
        _imei = _tel->imei();
        _ir(Tela::Imei, agora);
      }
    } else if (k == TECLA_VERMELHA) {
      if (!_digitado.empty()) _digitado.pop_back();
      _sujo = true;
      if (_digitado.empty()) _ir(Tela::Inicio, agora);
    } else if (k == TECLA_VERDE) {
      if (_digitado.empty()) return;
      const std::string n = _digitado;
      _digitado.clear();
      if (ehCodigoUssd(n)) _enviarUssd(n, agora);
      else _ligar(n, agora);
    }
  } else if (ev.tipo == EventoTecla::Segurou) {
    if (k == '0' && _digitado == "0") {
      _digitado = "+";
      _sujo = true;
    } else if (k == TECLA_VERMELHA) {
      _digitado.clear();
      _ir(Tela::Inicio, agora);
    }
  }
}

void Interface::_teclaChamada(const EventoTecla& ev, uint32_t agora) {
  const char k = ev.tecla;
  if (ev.tipo == EventoTecla::Apertou) {
    if (k == TECLA_VERMELHA) {
      _tel->encerrar();          // o modem avisa quando a chamada acabar de fato
      _sujo = true;
    } else if (k == TECLA_VERDE && _tela == Tela::Recebendo) {
      if (!_tel->atender()) aviso("Não consegui atender: " + _tel->erro(), agora);
    } else if (_tela == Tela::EmLigacao && ehDigito(k)) {
      _tel->tom(k);
      _tons += k;
      if (_tons.size() > 20) _tons.erase(0, _tons.size() - 20);
      _sujo = true;
    }
  } else if (ev.tipo == EventoTecla::Segurou && _tela == Tela::EmLigacao) {
    if (k == '*') _mudarVolume(-1, agora);
    else if (k == '#') _mudarVolume(+1, agora);
  } else if (ev.tipo == EventoTecla::Soltou && _tela == Tela::EmLigacao && !ev.segurada &&
             (k == '*' || k == '#')) {
    _tel->tom(k);                // * e # curtos são tons; segurados mudam o volume
    _tons += k;
    if (_tons.size() > 20) _tons.erase(0, _tons.size() - 20);
    _sujo = true;
  }
}

void Interface::_teclaPerdidas(const EventoTecla& ev, uint32_t agora) {
  if (ev.tipo != EventoTecla::Apertou) return;
  const char k = ev.tecla;
  if (k == '2' && _selecao > 0) {
    _selecao--;
    _sujo = true;
  } else if (k == '8' && _selecao + 1 < (int)_perdidas.size()) {
    _selecao++;
    _sujo = true;
  } else if (k == TECLA_VERDE && _selecao < (int)_perdidas.size()) {
    const std::string n = _perdidas[_selecao].numero;
    _perdidas.clear();
    if (!n.empty()) _ligar(n, agora);
    else _ir(Tela::Inicio, agora);
  } else if (k == TECLA_VERMELHA) {
    _perdidas.clear();
    _ir(Tela::Inicio, agora);
  }
}

// ============================================================
// Mensagens: menu, escrever, enviar, lista, ler
// ============================================================

std::vector<Interface::Item> Interface::_itensMenu() const {
  std::vector<Item> v;
  v.push_back(Item::Escrever);
  if (!_mt.vazio() || _rascunhoMaisRecente()) v.push_back(Item::Rascunho);
  v.push_back(Item::Mensagens);
  if (!_ultimoDiscado.empty()) v.push_back(Item::Rediscar);
  return v;
}

uint32_t Interface::_rascunhoMaisRecente() const {
  if (!_caixa) return 0;
  for (const auto& it : _caixa->itens())
    if (it.estado == Caixa::Rascunho) return it.id;
  return 0;
}

void Interface::_teclaMenu(const EventoTecla& ev, uint32_t agora) {
  if (ev.tipo != EventoTecla::Apertou) return;
  const auto itens = _itensMenu();
  const int n = (int)itens.size();
  if (_menuSel >= n) _menuSel = n - 1;
  const char k = ev.tecla;
  if (k == '2') {
    if (_menuSel > 0) { _menuSel--; _sujo = true; }
  } else if (k == '8') {
    if (_menuSel + 1 < n) { _menuSel++; _sujo = true; }
  } else if (k == '5' || k == TECLA_VERDE) {
    switch (itens[_menuSel]) {
      case Item::Escrever: _escrever("", "", Tela::Menu, agora); break;
      case Item::Rascunho: {
        if (!_mt.vazio()) {                     // o que não deu para guardar na caixa
          _escreverVolta = Tela::Menu;
          _ir(Tela::Escrever, agora);
          break;
        }
        Caixa::Mensagem m;
        const uint32_t id = _rascunhoMaisRecente();
        if (!_caixa->ler(id, m)) {
          aviso("Não consegui abrir o rascunho.", agora);
          break;
        }
        _escrever(m.numero, m.texto, Tela::Menu, agora);
        _substituir = id;                       // enviado ou guardado de novo, este sai
        break;
      }
      case Item::Mensagens: _abrirLista(Tela::Menu, 0, agora); break;
      case Item::Rediscar:
        _digitado = _ultimoDiscado;
        _ir(Tela::Discador, agora);
        break;
    }
  } else if (k == TECLA_VERMELHA) {
    _ir(Tela::Inicio, agora);
  }
}

// Começa uma mensagem (nova, resposta, encaminhada, ou um rascunho da caixa)
void Interface::_escrever(std::string para, std::string texto, Tela volta, uint32_t agora) {
  if (!_mt.vazio()) _deixarRascunho(Tela::Escrever, agora);   // o que havia vai para a caixa
  _mt.begin(texto);
  _para = para;
  _substituir = 0;
  _escreverVolta = volta;
  _ir(Tela::Escrever, agora);
}

// Saindo do texto: ele vira rascunho na caixa (o de antes, se era um
// rascunho aberto da caixa, dá lugar a ele); sem texto, esse rascunho sai.
// `destino`: para onde a tela vai (numa chamada, o aviso espera ela acabar).
void Interface::_deixarRascunho(Tela destino, uint32_t agora) {
  _mt.confirmar();
  const bool eraRascunho = _substituir && _caixa && _caixa->posicao(_substituir) >= 0 &&
                           _caixa->estado(_substituir) == Caixa::Rascunho;
  if (_mt.vazio()) {
    if (eraRascunho) {
      _caixa->apagar(_substituir);
      if (_lendo == _substituir) _lendo = 0;
    }
    _para.clear();
    _substituir = 0;
    return;
  }
  const uint32_t id = _caixa ? _caixa->guardar(Caixa::Rascunho, _para, _dataHoraAgora(), _mt.texto()) : 0;
  if (!id) {                                    // fica na memória, no "Continuar rascunho" do menu
    aviso("Não consegui guardar o rascunho na caixa.", agora);
    return;
  }
  if (eraRascunho) {
    _caixa->apagar(_substituir);
    if (_lendo == _substituir) {               // a tela "Ler" volta mostrando a versão nova
      _lendo = id;
      _listaSel = id;
      _caixa->ler(id, _msg);
    }
  }
  _mt.begin();
  _para.clear();
  _substituir = 0;
  const bool chamada = destino == Tela::Recebendo || destino == Tela::Chamando || destino == Tela::EmLigacao;
  if (chamada) _avisoDepois = "O texto que você escrevia ficou nos rascunhos (Menu).";
  else if (destino != Tela::Escrever) aviso("Rascunho guardado.", agora);
  else aviso("O texto de antes ficou nos rascunhos.", agora);
}

// Uma letra (ou o número, segurando). Se não couber no máximo de SMS, avisa.
void Interface::_digitar(char k, bool segurou, uint32_t agora) {
  const bool mudou = segurou ? _mt.segurou(k, agora) : _mt.tecla(k, agora);
  if (_mt.recusou()) aviso("A mensagem chegou ao tamanho máximo (" + std::to_string(_maxPartes) + " SMS).", agora);
  if (mudou) _sujo = true;
}

bool Interface::_cabeNoMaximo(const std::string& texto, uint32_t agora) {
  const int partes = pdu::contar(texto).partes;
  if (partes <= _maxPartes) return true;
  aviso("A mensagem tem " + std::to_string(partes) + " partes; o máximo é " + std::to_string(_maxPartes) +
            ". Apague um pouco.", agora);
  return false;
}

// Escrevendo: as teclas são letras (multitoque); * troca o modo (Abc, abc,
// ABC, 123), # apaga (segurado: a palavra), segurar o verde vai para o
// destinatário e segurar o vermelho sai (o texto fica nos rascunhos).
void Interface::_teclaEscrever(const EventoTecla& ev, uint32_t agora) {
  const char k = ev.tecla;
  if (ev.tipo == EventoTecla::Apertou) {
    if (ehDigito(k)) {
      _digitar(k, false, agora);
    } else if (k == '*') {
      _mt.proximoModo();
      _sujo = true;
    } else if (k == TECLA_VERDE) {
      _mt.confirmar();
      if (_mt.vazio()) aviso("Escreva a mensagem primeiro.", agora);
      else if (_cabeNoMaximo(_mt.texto(), agora)) _ir(Tela::Para, agora);
    } else if (k == TECLA_VERMELHA) {
      _mt.confirmar();
      Tela volta = _escreverVolta;
      if (volta == Tela::Ler) {
        // Volta para a mensagem que estava aberta, se ela ainda existir (um
        // rascunho que ficou sem texto sai da caixa)
        const bool rascunhoSai = _mt.vazio() && _substituir == _lendo;
        if (rascunhoSai || !_caixa || _caixa->posicao(_lendo) < 0)
          volta = _lerVolta == Tela::Mensagens ? Tela::Mensagens : Tela::Inicio;
      }
      _ir(volta, agora);
    }
  } else if (ev.tipo == EventoTecla::Segurou) {
    if (ehDigito(k)) _digitar(k, true, agora);
    else if (k == '#' && _mt.apagarPalavra()) _sujo = true;
  } else if (ev.tipo == EventoTecla::Soltou) {
    if (k == '#' && !ev.segurada && _mt.apagar()) _sujo = true;   // # tocado (não segurado)
  }
}

void Interface::_teclaPara(const EventoTecla& ev, uint32_t agora) {
  const char k = ev.tecla;
  if (ev.tipo == EventoTecla::Apertou) {
    if (ehDigito(k)) {
      if (_para.size() < 20) _para += k;
      _sujo = true;
    } else if (k == TECLA_VERDE) {
      int digitos = 0;
      for (char c : _para) digitos += ehDigito(c) ? 1 : 0;
      if (digitos < 3) aviso("Digite o número de quem vai receber.", agora);
      else if (_cabeNoMaximo(_mt.texto(), agora)) _enviar(_para, _mt.texto(), true, _substituir, agora);
    } else if (k == TECLA_VERMELHA) {
      _ir(Tela::Escrever, agora);
    }
  } else if (ev.tipo == EventoTecla::Segurou) {
    if (k == '0' && _para == "0") {
      _para = "+";
      _sujo = true;
    } else if (k == '#') {
      _para.clear();
      _sujo = true;
    }
  } else if (ev.tipo == EventoTecla::Soltou && k == '#' && !ev.segurada && !_para.empty()) {
    _para.pop_back();
    _sujo = true;
  }
}

// Mostra "Enviando…" e, depois que a tela foi desenhada, manda (o envio trava o loop).
// substitui: a mensagem da caixa que esta tentativa substitui (a "não enviada"
// ou o rascunho), que sai quando o resultado for guardado.
void Interface::_enviar(const std::string& numero, const std::string& texto, bool doEditor, uint32_t substitui,
                        uint32_t agora) {
  _envioPara = _tel->numeroCompleto(numero);
  _envioTexto = texto;
  _envioDoEditor = doEditor;
  _envioSubstitui = substitui;
  _envioPartes = pdu::contar(texto).partes;
  _envioOk = false;
  _envioErro.clear();
  _envioId = 0;
  _avisoAte = 0;                          // um "digite o número" de antes já não vale
  _ir(Tela::Enviando, agora);
  _pedir(Acao::EnviarSms, "");
}

// Guarda o resultado: primeiro a mensagem nova, depois sai a que ela substitui
// (se a gravação falhar, a de antes continua lá)
uint32_t Interface::_guardarEnvio(Caixa::Estado estado) {
  if (!_caixa) return 0;
  const uint32_t antiga = _envioSubstitui;
  _envioSubstitui = 0;
  const uint32_t id = _caixa->guardar(estado, _envioPara, _dataHoraAgora(), _envioTexto);
  if (id && antiga) _caixa->apagar(antiga);
  if (!id && antiga && _caixa->posicao(antiga) >= 0) return antiga;   // continua sendo "esta"
  return id;
}

void Interface::_enviarAgora(uint32_t agora) {
  _envioOk = _tel->enviarSms(_envioPara, _envioTexto);
  agora = _relogioAgora(agora);                 // o envio pode ter levado muitos segundos
  if (!_envioOk) _envioErro = explicarErro(_tel->erro());
  _envioId = _guardarEnvio(_envioOk ? Caixa::Enviada : Caixa::NaoEnviada);
  // Enviada, ou guardada para tentar de novo: o texto já não precisa ficar no editor
  if (_envioDoEditor && (_envioOk || _envioId)) {
    _mt.begin();
    _para.clear();
    _substituir = 0;
  }
  _ir(Tela::Enviado, agora);
}

// Chegou uma chamada antes de o SMS sair (ele espera a tela "Enviando" ser
// desenhada): fica guardado como não enviado, para reenviar depois
void Interface::_envioAdiado() {
  _envioErro = "chegou uma chamada";
  _envioId = _guardarEnvio(Caixa::NaoEnviada);
  if (_envioDoEditor && _envioId) {
    _mt.begin();
    _para.clear();
    _substituir = 0;
  }
  _avisoDepois = _envioId ? "A mensagem não saiu (chegou uma chamada). Ela está em Mensagens, para reenviar."
                          : "A mensagem não saiu (chegou uma chamada).";
}

void Interface::_teclaEnviado(const EventoTecla& ev, uint32_t agora) {
  if (ev.tipo != EventoTecla::Apertou) return;
  if (_envioOk) {
    _ir(Tela::Inicio, agora);
  } else if (ev.tecla == TECLA_VERDE) {
    _enviar(_envioPara, _envioTexto, _envioDoEditor, _envioId, agora);   // tentar de novo
  } else if (ev.tecla == TECLA_VERMELHA) {
    _ir(Tela::Inicio, agora);
  }
}

uint32_t Interface::_novaMaisRecente() const {
  if (!_caixa) return 0;
  for (const auto& it : _caixa->itens())
    if (it.estado == Caixa::Nova) return it.id;
  return 0;
}

void Interface::_abrirLista(Tela volta, uint32_t selecionar, uint32_t agora) {
  _listaVolta = volta;
  _listaSel = selecionar;
  if (_caixa && (!_listaSel || _caixa->posicao(_listaSel) < 0))
    _listaSel = _caixa->itens().empty() ? 0 : _caixa->itens()[0].id;
  _ir(Tela::Mensagens, agora);
}

void Interface::_teclaLista(const EventoTecla& ev, uint32_t agora) {
  if (ev.tipo != EventoTecla::Apertou) return;
  const auto& itens = _caixa ? _caixa->itens() : NENHUMA;
  int i = _caixa ? _caixa->posicao(_listaSel) : -1;
  if (i < 0 && !itens.empty()) {
    i = 0;
    _listaSel = itens[0].id;
  }
  const char k = ev.tecla;
  if (k == '2') {
    if (i > 0) { _listaSel = itens[i - 1].id; _sujo = true; }
  } else if (k == '8') {
    if (i >= 0 && i + 1 < (int)itens.size()) { _listaSel = itens[i + 1].id; _sujo = true; }
  } else if (k == '5' || k == TECLA_VERDE) {
    if (i >= 0) _abrirMensagem(_listaSel, Tela::Mensagens, true, agora);
    else if (k == TECLA_VERDE) _escrever("", "", Tela::Mensagens, agora);   // lista vazia: "Escrever"
  } else if (k == TECLA_VERMELHA) {
    _ir(_listaVolta, agora);
  }
}

void Interface::_abrirMensagem(uint32_t id, Tela volta, bool marcarJa, uint32_t agora) {
  Caixa::Mensagem m;
  if (!_caixa || !id || !_caixa->ler(id, m)) {
    aviso("Não consegui abrir a mensagem.", agora);
    return;
  }
  _msg = m;
  _lendo = id;
  _listaSel = id;             // voltando para a lista, ela continua selecionada
  _lerVolta = volta;
  _marcarAoTocar = false;
  if (m.estado == Caixa::Nova) {
    if (marcarJa) _marcarLida();
    else _marcarAoTocar = true;
  }
  _ir(Tela::Ler, agora);
}

void Interface::_marcarLida() {
  _marcarAoTocar = false;
  if (_caixa && _caixa->posicao(_lendo) >= 0 && _caixa->estado(_lendo) == Caixa::Nova &&
      _caixa->mudar(_lendo, Caixa::Lida))
    _msg.estado = Caixa::Lida;
}

std::vector<Interface::Opcao> Interface::_opcoes() const {
  std::vector<Opcao> v;
  if (_msg.estado == Caixa::Rascunho) {
    v.push_back(Opcao::Continuar);
    v.push_back(Opcao::Apagar);
    return v;
  }
  const bool numero = ehNumero(_msg.numero);
  if (_msg.estado == Caixa::NaoEnviada) {
    if (numero) v.push_back(Opcao::TentarDeNovo);
    v.push_back(Opcao::Editar);
  } else if ((_msg.estado == Caixa::Nova || _msg.estado == Caixa::Lida) && numero) {
    v.push_back(Opcao::Responder);
  }
  if (numero) v.push_back(Opcao::Ligar);
  v.push_back(Opcao::Encaminhar);
  v.push_back(Opcao::Apagar);
  return v;
}

void Interface::_teclaLer(const EventoTecla& ev, uint32_t agora) {
  if (ev.tipo != EventoTecla::Apertou) return;
  if (_marcarAoTocar) _marcarLida();          // alguém está olhando
  const char k = ev.tecla;
  const int pagina = _linhasVisiveis > 1 ? _linhasVisiveis - 1 : 1;
  const int maxRol = _linhasTotal > _linhasVisiveis ? _linhasTotal - _linhasVisiveis : 0;
  if (k == '2') {
    if (_rolagem > 0) { _rolagem = std::max(0, _rolagem - pagina); _sujo = true; }
  } else if (k == '8') {
    if (_rolagem < maxRol) { _rolagem = std::min(maxRol, _rolagem + pagina); _sujo = true; }
  } else if (k == '5') {
    _opcaoSel = 0;
    _ir(Tela::Opcoes, agora);
  } else if (k == TECLA_VERDE) {
    const auto ops = _opcoes();             // responder, tentar de novo, continuar; senão, as opções
    if (ops[0] == Opcao::Responder || ops[0] == Opcao::TentarDeNovo || ops[0] == Opcao::Continuar) {
      _executarOpcao(ops[0], agora);
    } else {
      _opcaoSel = 0;
      _ir(Tela::Opcoes, agora);
    }
  } else if (k == TECLA_VERMELHA) {
    _ir(_lerVolta == Tela::Mensagens ? Tela::Mensagens : Tela::Inicio, agora);
  }
}

void Interface::_executarOpcao(Opcao o, uint32_t agora) {
  switch (o) {
    case Opcao::Responder: _escrever(_msg.numero, "", Tela::Ler, agora); break;
    case Opcao::Ligar: _ligar(_msg.numero, agora); break;
    case Opcao::Encaminhar: _escrever("", _msg.texto, Tela::Ler, agora); break;
    case Opcao::TentarDeNovo: _enviar(_msg.numero, _msg.texto, false, _lendo, agora); break;
    case Opcao::Editar:
    case Opcao::Continuar:
      _escrever(_msg.numero, _msg.texto, Tela::Ler, agora);
      _substituir = _lendo;               // enviando (ou guardando de novo) a versão nova, esta sai
      break;
    case Opcao::Apagar: _ir(Tela::Apagar, agora); break;
  }
}

void Interface::_teclaOpcoes(const EventoTecla& ev, uint32_t agora) {
  if (ev.tipo != EventoTecla::Apertou) return;
  const auto ops = _opcoes();
  const int n = (int)ops.size();
  if (_opcaoSel >= n) _opcaoSel = n - 1;
  const char k = ev.tecla;
  if (k == '2') {
    if (_opcaoSel > 0) { _opcaoSel--; _sujo = true; }
  } else if (k == '8') {
    if (_opcaoSel + 1 < n) { _opcaoSel++; _sujo = true; }
  } else if (k == '5' || k == TECLA_VERDE) {
    _executarOpcao(ops[_opcaoSel], agora);
  } else if (k == TECLA_VERMELHA) {
    _ir(Tela::Ler, agora);
  }
}

void Interface::_teclaApagar(const EventoTecla& ev, uint32_t agora) {
  if (ev.tipo != EventoTecla::Apertou) return;
  if (ev.tecla == TECLA_VERDE) {
    // Na lista, a seleção passa para a vizinha (a mais antiga; ou a mais nova, se era a última)
    uint32_t vizinha = 0;
    if (_caixa) {
      const auto& itens = _caixa->itens();
      const int i = _caixa->posicao(_lendo);
      if (i >= 0 && i + 1 < (int)itens.size()) vizinha = itens[i + 1].id;
      else if (i > 0) vizinha = itens[i - 1].id;
    }
    if (!_caixa || !_caixa->apagar(_lendo)) {
      aviso("Não consegui apagar a mensagem.", agora);
      _ir(Tela::Ler, agora);
      return;
    }
    _lendo = 0;
    aviso("Mensagem apagada.", agora);
    if (_lerVolta == Tela::Mensagens) {
      _listaSel = vizinha;
      _ir(Tela::Mensagens, agora);
    } else {
      _ir(Tela::Inicio, agora);
    }
  } else if (ev.tecla == TECLA_VERMELHA) {
    _ir(Tela::Ler, agora);
  }
}

void Interface::_teclaUssd(const EventoTecla& ev, uint32_t agora) {
  if (ev.tipo != EventoTecla::Apertou) return;
  const char k = ev.tecla;
  if (_ussdMenu) {
    if (ehTeclaDeNumero(k)) {
      if (_ussdResposta.size() < 20) _ussdResposta += k;
      _sujo = true;
    } else if (k == TECLA_VERDE) {
      if (!_ussdResposta.empty()) _enviarUssd(_ussdResposta, agora);
    } else if (k == TECLA_VERMELHA) {
      if (!_ussdResposta.empty()) {
        _ussdResposta.pop_back();
        _sujo = true;
      } else {
        _tel->cancelarUssd();
        _ir(Tela::Inicio, agora);
      }
    }
    return;
  }
  if (k == '2') {
    if (_rolagem > 0) { _rolagem--; _sujo = true; }
  } else if (k == '8') {
    if (_rolagem + _linhasVisiveis < _linhasTotal) { _rolagem++; _sujo = true; }
  } else if (k == TECLA_VERDE || k == TECLA_VERMELHA) {
    _ir(Tela::Inicio, agora);
  }
}

void Interface::_teclaTeclado(const EventoTecla& ev, uint32_t agora) {
  if (ev.tipo != EventoTecla::Apertou) return;
  if (ev.tecla == TECLA_VERDE) {
    if (_tecladoFim && _tecladoOk) { _ir(Tela::Inicio, agora); return; }
    _passo = 0;                    // recomeçar
    _parNovo = Teclado::SEM_PAR;
    _tecladoFim = false;
    _msgTeclado.clear();
    _tel->aprenderTeclado(true);
    _sujo = true;
  } else if (ev.tecla == TECLA_VERMELHA) {
    if (_tecladoFim && !_tecladoOk) _tel->salvarTeclado(_pares);   // "usar assim mesmo"
    _ir(Tela::Inicio, agora);      // ou "pular": fica o mapa que já havia
  }
}

// Configurando o teclado sem botões: segurar a tecla que já foi aprendida
// como 1 vale o verde (recomeçar / refazer) e a aprendida como 3 vale o
// vermelho (pular / usar assim).
void Interface::_parSegurado(uint8_t a, uint8_t b, uint32_t agora) {
  const uint8_t par = (uint8_t)((a << 4) | b);
  if (par == _parNovo) return;   // o mesmo toque que acabou de ensinar a tecla: não é botão
  auto aprendida = [&](char tecla) {
    for (int i = 0; i < _passo && i < Teclado::TECLAS; i++)
      if (Teclado::teclaDoIndice(i) == tecla) return _pares[i] == par;
    return false;
  };
  EventoTecla e;
  e.tipo = EventoTecla::Apertou;
  if (_digVerde && aprendida(_digVerde)) e.tecla = TECLA_VERDE;
  else if (_digVermelho && aprendida(_digVermelho)) e.tecla = TECLA_VERMELHA;
  else return;
  _acordar(agora);
  _teclaTeclado(e, agora);
}

void Interface::_parTeclado(uint8_t a, uint8_t b, uint32_t agora) {
  _acordar(agora);
  _parNovo = Teclado::SEM_PAR;
  if (_tecladoFim) return;
  const uint8_t par = (uint8_t)((a << 4) | b);
  for (int i = 0; i < _passo; i++) {
    if (_pares[i] == par) {
      _msgTeclado = std::string("Essa foi a tecla ") + Teclado::teclaDoIndice(i) +
                    ". Agora aperte a tecla " + Teclado::teclaDoIndice(_passo) + ".";
      _sujo = true;
      return;
    }
  }
  _pares[_passo++] = par;
  _parNovo = par;
  _msgTeclado.clear();
  _sujo = true;
  if (_passo == Teclado::TECLAS) {
    _tecladoFim = true;
    _tecladoOk = Teclado::validarGrade(_pares, _linhasFio, _colunasFio);
    _desde = agora;
    if (_tecladoOk) {
      _tel->salvarTeclado(_pares);
      _tel->aprenderTeclado(false);
    }
  }
}

// ============================================================
// Textos
// ============================================================

std::string Interface::formatarNumero(const std::string& n) {
  std::string nac = n;
  if (n.size() > 3 && n.compare(0, 3, "+55") == 0) nac = n.substr(3);
  else if (!n.empty() && n[0] == '+') return n;               // de fora do Brasil
  if (nac.empty()) return n;
  for (char c : nac)
    if (c < '0' || c > '9') return n;                        // códigos, nomes ("TIM")
  const size_t t = nac.size();
  if (t == 11 && (nac.compare(0, 4, "0800") == 0 || nac.compare(0, 4, "0300") == 0 ||
                  nac.compare(0, 4, "0500") == 0 || nac.compare(0, 4, "0900") == 0))
    return nac.substr(0, 4) + " " + nac.substr(4, 3) + " " + nac.substr(7);
  if (t == 11 && nac[0] != '0') return "(" + nac.substr(0, 2) + ") " + nac.substr(2, 5) + "-" + nac.substr(7);
  if (t == 10 && nac[0] != '0') return "(" + nac.substr(0, 2) + ") " + nac.substr(2, 4) + "-" + nac.substr(6);
  if (t == 9) return nac.substr(0, 5) + "-" + nac.substr(5);
  if (t == 8) return nac.substr(0, 4) + "-" + nac.substr(4);
  return n;
}

bool Interface::ehCodigoUssd(const std::string& s) {
  return s.size() >= 3 && (s[0] == '*' || s[0] == '#') && s.back() == '#';
}

bool Interface::ehNumero(const std::string& n) {
  size_t i = (!n.empty() && n[0] == '+') ? 1 : 0;
  if (n.size() - i < 3) return false;
  for (; i < n.size(); i++)
    if (!ehDigito(n[i])) return false;
  return true;
}

// "+CMS ERROR: 330" → "o chip não tem o número da central de mensagens".
// O modem responde com números (+CMS) ou por extenso, em inglês (+CME, com
// AT+CMEE=2); o que não está aqui aparece como veio.
std::string Interface::explicarErro(const std::string& e) {
  struct Codigo { int n; const char* texto; };
  static const Codigo cms[] = {
      {8, "a operadora barrou o envio (falta crédito?)"},
      {10, "a operadora barrou o envio"},
      {21, "a central de mensagens recusou (falta crédito?)"},
      {27, "o número de destino não está atendendo"},
      {28, "número de destino desconhecido"},
      {29, "a operadora recusou o envio"},
      {38, "a rede está fora de serviço"},
      {41, "falha momentânea da rede"},
      {42, "a rede está congestionada"},
      {47, "a rede está sem recursos agora"},
      {50, "o chip não tem o serviço de SMS"},
      {69, "a rede não oferece este serviço"},
      {96, "a rede achou a mensagem inválida"},
      {111, "a rede achou a mensagem inválida"},
      {301, "o SMS está bloqueado neste chip"},
      {302, "o modem não deixou enviar agora"},
      {304, "o modem achou a mensagem inválida"},
      {310, "sem chip"},
      {311, "o chip pede o PIN"},
      {313, "falha no chip"},
      {330, "o chip não tem o número da central de mensagens"},
      {331, "sem rede"},
      {332, "a rede não respondeu a tempo"},
      {500, "erro desconhecido no modem"},
  };
  static const Codigo cme[] = {
      {3, "o modem não deixou enviar agora"}, {4, "o modem não faz isso"},  {10, "sem chip"},
      {11, "o chip pede o PIN"},              {13, "falha no chip"},        {14, "o chip está ocupado"},
      {30, "sem rede"},                       {31, "a rede não respondeu a tempo"}, {100, "erro desconhecido no modem"},
  };
  int n;
  const size_t p = e.find("CMS ERROR:");
  if (p != std::string::npos && sscanf(e.c_str() + p + 10, "%d", &n) == 1) {
    for (const auto& c : cms)
      if (c.n == n) return c.texto;
    return "a rede recusou (código " + std::to_string(n) + ")";
  }
  const size_t q = e.find("CME ERROR:");
  if (q != std::string::npos && sscanf(e.c_str() + q + 10, "%d", &n) == 1) {
    for (const auto& c : cme)
      if (c.n == n) return c.texto;
    return "o modem recusou (código " + std::to_string(n) + ")";
  }
  // Por extenso (em inglês)
  std::string m;
  for (char c : e) m += (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
  static const struct { const char* trecho; const char* texto; } extenso[] = {
      {"no network", "sem rede"},           {"not inserted", "sem chip"},
      {"pin required", "o chip pede o PIN"}, {"timeout", "a rede não respondeu a tempo"},
      {"not allowed", "o modem não deixou enviar agora"}, {"busy", "o chip está ocupado"},
  };
  for (const auto& x : extenso)
    if (m.find(x.trecho) != std::string::npos) return x.texto;
  return e;
}

std::string Interface::_dataHoraAgora() const {
  if (!_s.horaValida) return "";
  char b[24];
  snprintf(b, sizeof b, "%02d/%02d/%02d,%02d:%02d:00", _s.ano % 100, _s.mes, _s.dia, _s.hora, _s.minuto);
  return b;
}

std::string Interface::_agoraTexto() const {
  if (!_s.horaValida) return "";
  return dois(_s.hora) + ":" + dois(_s.minuto);
}

std::string Interface::_quando(const std::string& dataHora) const {
  // "26/10/21,18:05:12-12" -> "18:05" (hoje) ou "21/10 18:05"
  int aa, mm, dd, h, mi;
  if (sscanf(dataHora.c_str(), "%d/%d/%d,%d:%d", &aa, &mm, &dd, &h, &mi) != 5) return "";
  if (_s.horaValida && _s.dia == dd && _s.mes == mm && _s.ano % 100 == aa) return dois(h) + ":" + dois(mi);
  return dois(dd) + "/" + dois(mm) + " " + dois(h) + ":" + dois(mi);
}

// ============================================================
// Desenho
// ============================================================

uint32_t Interface::_chave(uint32_t agora) const {
  uint32_t k = 0;
  switch (_tela) {
    case Tela::EmLigacao: k = (agora - _conectouEm) / 1000 + 1; break;   // cronômetro
    case Tela::Chamando:
    case Tela::UssdEspera:
    case Tela::Enviando: k = agora / 400 + 1; break;                      // pontinhos
    case Tela::Recebendo: k = agora / 100 + 1; break;                     // anel pulsando
    default: break;
  }
  return k * 4 + (antes(agora, _avisoAte) ? 1 : 0) + (antes(agora, _volumeAte) ? 2 : 0);
}

bool Interface::precisaDesenhar(uint32_t agora) const {
  return _sujo || _chave(agora) != _chaveDesenhada;
}

void Interface::desenhado(uint32_t agora) {
  _sujo = false;
  _chaveDesenhada = _chave(agora);
  _desenhos++;
}

void Interface::desenhar(Quadro& q, uint32_t agora) {
  _sujo = false;
  _chaveDesenhada = _chave(agora);
  _desenhos++;
  q.semRecorte();
  q.limpar(cor::FUNDO);
  switch (_tela) {
    case Tela::Inicio: _desenharInicio(q); break;
    case Tela::Discador: _desenharDiscador(q); break;
    case Tela::Chamando: _desenharChamando(q, agora); break;
    case Tela::Recebendo: _desenharRecebendo(q, agora); break;
    case Tela::EmLigacao: _desenharEmLigacao(q, agora); break;
    case Tela::Encerrada: _desenharEncerrada(q); break;
    case Tela::Perdida: _desenharPerdida(q); break;
    case Tela::Perdidas: _desenharPerdidas(q); break;
    case Tela::Menu: _desenharMenu(q); break;
    case Tela::Escrever: _desenharEscrever(q); break;
    case Tela::Para: _desenharPara(q); break;
    case Tela::Enviando: _desenharEnviando(q, agora); break;
    case Tela::Enviado: _desenharEnviado(q); break;
    case Tela::Mensagens: _desenharLista(q); break;
    case Tela::Ler: _desenharLer(q); break;
    case Tela::Opcoes: _desenharOpcoes(q); break;
    case Tela::Apagar: _desenharApagar(q); break;
    case Tela::UssdEspera: _desenharUssdEspera(q, agora); break;
    case Tela::Ussd: _desenharUssd(q); break;
    case Tela::Imei: _desenharImei(q); break;
    case Tela::Desligar: _desenharDesligar(q); break;
    case Tela::Desligando:
      q.texto(L / 2, 166, "Desligando…", fonteTitulo, cor::SUAVE, Alinha::Centro);
      break;
    case Tela::Teclado: _desenharTeclado(q); break;
  }
  if (_tela == Tela::EmLigacao && antes(agora, _volumeAte)) _desenharVolume(q);
  if (antes(agora, _avisoAte) && !_aviso.empty()) _desenharAviso(q);
}

void Interface::_barra(Quadro& q, bool relogio) const {
  const int base = 17;
  for (int i = 0; i < 4; i++) {                  // sinal: 4 barrinhas
    const int h = 4 + i * 3;
    q.retangulo(10 + i * 5, base - h, 3, h, _s.barras > i ? cor::TEXTO : cor::APAGADO);
  }
  const std::string rede = _s.barras < 0 ? "sem serviço" : _s.rede;
  q.texto(34, base, rede, fontePequena, cor::SUAVE);
  if (relogio) q.texto(L / 2, base, _agoraTexto(), fontePequena, cor::TEXTO, Alinha::Centro);

  const int bx = L - 34, by = 7;                 // bateria: corpo 22 x 11 + ponta
  q.moldura(bx, by, 22, 11, cor::SUAVE);
  q.retangulo(bx + 22, by + 3, 2, 5, cor::SUAVE);
  if (_s.bateria >= 0) {
    int w = (18 * _s.bateria + 50) / 100;
    if (_s.bateria > 0 && w < 1) w = 1;
    q.retangulo(bx + 2, by + 2, w, 7, _s.bateria <= 15 ? cor::VERMELHO : cor::TEXTO);
    q.texto(bx - 5, base, std::to_string(_s.bateria) + "%", fontePequena, cor::SUAVE, Alinha::Direita);
  }
}

void Interface::_dicas(Quadro& q, const std::string& verde, const std::string& vermelha,
                       const std::string& meio) const {
  const int base = 309;
  // Com as teclas 1 e 3 no lugar dos botões, a bolinha mostra o número
  // (a tecla fica logo abaixo da tela: o 1 à esquerda, o 3 à direita).
  const int recuo = _digVerde || _digVermelho ? 32 : 28;
  int esquerda = 0, direita = L;     // até onde vão os textos das pontas
  if (!verde.empty()) {
    _bolinha(q, 18.f, 304.f, cor::VERDE, _digVerde);
    esquerda = recuo + q.texto(recuo, base, verde, fontePequena, cor::TEXTO);
  }
  if (!vermelha.empty()) {
    _bolinha(q, L - 18.f, 304.f, cor::VERMELHO, _digVermelho);
    direita = L - recuo - q.texto(L - recuo, base, vermelha, fontePequena, cor::TEXTO, Alinha::Direita);
  }
  if (!meio.empty()) {
    // No meio da tela; se esbarrar num dos lados, no vão entre os dois; se
    // nem assim couber, fica de fora
    const int w = Quadro::largura(meio, fontePequena), folga = 8;
    int x = L / 2 - w / 2;
    if (x < esquerda + folga) x = esquerda + folga;
    if (x + w > direita - folga) x = direita - folga - w;
    if (x >= esquerda + folga) q.texto(x, base, meio, fontePequena, cor::SUAVE);
  }
}

// Bolinha das dicas: lisa (botões de verdade) ou com o número da tecla
void Interface::_bolinha(Quadro& q, float cx, float cy, uint16_t c, char digito) const {
  if (!digito) {
    q.circulo(cx, cy, 5.f, c);
    return;
  }
  q.circulo(cx, cy, 9.f, c);
  q.texto((int)cx, (int)cy + (fontePequena.maiuscula + 1) / 2, std::string(1, digito), fontePequena, cor::FUNDO,
          Alinha::Centro);
}

void Interface::_pilula(Quadro& q, int x, int y, int w, int h, uint16_t c, const std::string& t, char digito) const {
  q.retanguloRedondo(x, y, w, h, h / 2, c);
  const int base = y + (h + fonteTitulo.maiuscula) / 2;
  if (!digito) {
    q.texto(x + w / 2, base, t, fonteTitulo, cor::TEXTO, Alinha::Centro);
    return;
  }
  // o número da tecla numa bolinha branca, à esquerda do texto
  const float cx = x + h / 2.f, cy = y + h / 2.f;
  q.circulo(cx, cy, 11.f, cor::TEXTO);
  q.texto((int)cx, base, std::string(1, digito), fonteTitulo, c, Alinha::Centro);
  q.texto(x + w / 2 + h / 4, base, t, fonteTitulo, cor::TEXTO, Alinha::Centro);
}

void Interface::_pontinhos(Quadro& q, int y, uint32_t agora) const {
  const int fase = (int)((agora / 400) % 3);
  for (int i = 0; i < 3; i++)
    q.circulo(L / 2 - 16 + i * 16 + 0.5f, y + 0.5f, 4.f, i == fase ? cor::TEXTO : cor::APAGADO);
}

int Interface::_textoLongo(Quadro& q, const std::vector<std::string>& linhas, const Fonte& f, int y0,
                           int y1, int rolagem, uint16_t c) const {
  const int alt = f.alturaLinha();
  const int visiveis = (y1 - y0) / alt;
  q.recorte(0, y0, L, y1 - y0);
  int base = y0 + f.ascendente - rolagem * alt;
  for (const auto& l : linhas) {
    if (base + f.descendente > y1) break;        // só linhas inteiras
    if (base - f.ascendente >= y0) q.texto(MARGEM, base, l, f, c);
    base += alt;
  }
  q.semRecorte();
  const int total = (int)linhas.size();
  if (total > visiveis) {                         // barra de rolagem
    const int trilho = y1 - y0, x = L - 7;
    q.retangulo(x, y0, 2, trilho, cor::APAGADO);
    int h = trilho * visiveis / total;
    if (h < 14) h = 14;
    const int maxRol = total - visiveis;
    const int y = y0 + (trilho - h) * (rolagem < maxRol ? rolagem : maxRol) / (maxRol > 0 ? maxRol : 1);
    q.retanguloRedondo(x - 1, y, 4, h, 2, cor::SUAVE);
  }
  return visiveis;
}

static void numeroGrande(Quadro& q, const std::string& numero, int base, uint16_t c) {
  const std::string n = numero.empty() ? "Número privado" : Interface::formatarNumero(numero);
  if (Quadro::largura(n, fonteGrande) <= L - 24) {
    q.texto(L / 2, base, n, fonteGrande, c, Alinha::Centro);
    return;
  }
  const auto linhas = Quadro::quebrar(n, fonteTitulo, L - 24);
  int b = base - (int)(linhas.size() - 1) * 11;
  for (size_t i = 0; i < linhas.size() && i < 3; i++, b += 22) q.texto(L / 2, b, linhas[i], fonteTitulo, c, Alinha::Centro);
}

static void cartao(Quadro& q, int y, uint16_t marca, const std::string& titulo, const std::string& sub) {
  // Largo o bastante para "2 chamadas perdidas" e "a última de (21) 95555-4444"
  q.retanguloRedondo(8, y, L - 16, 48, 10, cor::CARTAO);
  q.circulo(19.5f, y + 24.f, 4.5f, marca);
  q.texto(30, y + 21, Quadro::caber(titulo, fonteTitulo, L - 40), fonteTitulo, cor::TEXTO);
  q.texto(30, y + 39, Quadro::caber(sub, fontePequena, L - 40), fontePequena, cor::SUAVE);
}

void Interface::_desenharInicio(Quadro& q) const {
  _barra(q, false);
  std::string linha = _s.operadora;
  uint16_t c = cor::SUAVE;
  if (!_s.chip.empty()) { linha = _s.chip; c = cor::AMBAR; }
  q.texto(L / 2, 50, Quadro::caber(linha, fonteTitulo, L - 32), fonteTitulo, c, Alinha::Centro);

  const std::string hora = _s.horaValida ? dois(_s.hora) + ":" + dois(_s.minuto) : "--:--";
  q.texto(L / 2, 130, hora, fonteRelogio, cor::TEXTO, Alinha::Centro);
  if (_s.horaValida && _s.mes >= 1 && _s.mes <= 12) {
    const std::string data = std::string(DIAS[diaDaSemana(_s.ano, _s.mes, _s.dia)]) + ", " +
                             std::to_string(_s.dia) + " de " + MESES[_s.mes - 1];
    q.texto(L / 2, 158, Quadro::caber(data, fonteTexto, L - 24), fonteTexto, cor::SUAVE, Alinha::Centro);
  }

  int y = 182;
  if (!_perdidas.empty()) {
    const size_t n = _perdidas.size();
    std::string sub = formatarNumero(_perdidas.front().numero.empty() ? "privado" : _perdidas.front().numero);
    if (!_perdidas.front().quando.empty()) sub += ", " + _perdidas.front().quando;
    cartao(q, y, cor::VERMELHO, n == 1 ? "1 chamada perdida" : std::to_string(n) + " chamadas perdidas", sub);
    y += 56;
  }
  const size_t novas = _cartaoMensagens && _caixa ? _caixa->novas() : 0;
  if (novas > 0) {
    std::string de;
    Caixa::Mensagem m;
    if (_caixa->ler(_novaMaisRecente(), m)) de = m.numero.empty() ? "número privado" : formatarNumero(m.numero);
    cartao(q, y, cor::AZUL, novas == 1 ? "1 mensagem nova" : std::to_string(novas) + " mensagens novas",
           de.empty() ? "" : (novas == 1 ? "de " : "a última de ") + de);
  }
  const bool novidades = _temNotificacoes();
  _dicas(q, novidades ? "Ver" : "Menu", novidades ? "Limpar" : "Segure: desligar");
}

void Interface::_desenharDiscador(Quadro& q) const {
  _barra(q, true);
  q.texto(MARGEM, 52, "Discar", fontePequena, cor::SUAVE);
  const std::string n = formatarNumero(_digitado);
  if (Quadro::largura(n, fonteDiscador) <= L - 24) {
    q.texto(L / 2, 156, n, fonteDiscador, cor::TEXTO, Alinha::Centro);
  } else {
    const auto linhas = Quadro::quebrar(n, fonteGrande, L - 24);
    int b = 150 - (int)(linhas.size() - 1) * 16;
    for (size_t i = 0; i < linhas.size() && i < 4; i++, b += 32)
      q.texto(L / 2, b, linhas[i], fonteGrande, cor::TEXTO, Alinha::Centro);
  }
  const bool codigo = ehCodigoUssd(_digitado);
  if (codigo) q.texto(L / 2, 200, "Código da operadora", fontePequena, cor::SUAVE, Alinha::Centro);
  _dicas(q, codigo ? "Enviar" : "Ligar", "Apagar");
}

void Interface::_desenharChamando(Quadro& q, uint32_t agora) const {
  _barra(q, true);
  q.texto(L / 2, 72, _s.chamada == 2 ? "Chamando…" : "Discando…", fonteTitulo, cor::AMBAR, Alinha::Centro);
  numeroGrande(q, _numeroChamada, 132, cor::TEXTO);
  _pontinhos(q, 180, agora);
  _pilula(q, 40, 238, 160, 46, cor::VERMELHO, "Encerrar", _digVermelho);
}

void Interface::_desenharRecebendo(Quadro& q, uint32_t agora) const {
  _barra(q, true);
  q.texto(L / 2, 64, "Chamada recebida", fonteTitulo, cor::SUAVE, Alinha::Centro);
  numeroGrande(q, _numeroChamada, 112, cor::TEXTO);
  // anel verde que pulsa
  const int fase = (int)((agora / 100) % 14);
  const uint16_t c = Quadro::misturar565(cor::FUNDO, cor::VERDE, (uint32_t)(255 - fase * 17));
  q.anel(L / 2.f, 178.f, 16.f + fase * 1.6f, 3.f, c);
  q.circulo(L / 2.f, 178.f, 11.f, cor::VERDE);
  _pilula(q, 12, 238, 104, 46, cor::VERDE, "Atender");
  _pilula(q, 124, 238, 104, 46, cor::VERMELHO, "Recusar");
  // Sem botões: embaixo de cada pílula, a tecla que se segura para ela
  const char digitos[2] = {_digVerde, _digVermelho};
  const uint16_t cores[2] = {cor::VERDE, cor::VERMELHO};
  for (int i = 0; i < 2; i++) {
    if (!digitos[i]) continue;
    const int meio = 12 + 52 + i * 112;
    const int lt = Quadro::largura("segure o", fontePequena), w = lt + 5 + 18;
    q.texto(meio - w / 2, 309, "segure o", fontePequena, cor::SUAVE);
    _bolinha(q, meio - w / 2 + lt + 5 + 9.f, 304.f, cores[i], digitos[i]);
  }
}

void Interface::_desenharEmLigacao(Quadro& q, uint32_t agora) const {
  _barra(q, true);
  q.texto(L / 2, 64, "Em ligação", fonteTitulo, cor::VERDE, Alinha::Centro);
  numeroGrande(q, _numeroChamada, 108, cor::TEXTO);
  q.texto(L / 2, 168, duracao(agora - _conectouEm), fonteDiscador, cor::TEXTO, Alinha::Centro);
  if (!_tons.empty()) q.texto(L / 2, 200, "Tons: " + _tons, fontePequena, cor::SUAVE, Alinha::Centro);
  q.texto(L / 2, 226, "Segure * ou # para o volume", fontePequena, cor::DISCRETO, Alinha::Centro);
  _pilula(q, 40, 238, 160, 46, cor::VERMELHO, "Encerrar", _digVermelho);
}

void Interface::_desenharEncerrada(Quadro& q) const {
  _barra(q, true);
  q.texto(L / 2, 88, _falhouAoLigar ? "Não foi possível ligar" : "Chamada encerrada", fonteTitulo,
          _falhouAoLigar ? cor::AMBAR : cor::SUAVE, Alinha::Centro);
  numeroGrande(q, _numeroChamada, 132, cor::TEXTO);
  const auto linhas = Quadro::quebrar(maiuscula(_motivo), fonteTexto, L - 32);
  int b = 172;
  for (size_t i = 0; i < linhas.size() && i < 4; i++, b += fonteTexto.alturaLinha())
    q.texto(L / 2, b, linhas[i], fonteTexto, cor::SUAVE, Alinha::Centro);
}

void Interface::_desenharPerdida(Quadro& q) const {
  _barra(q, true);
  q.texto(L / 2, 96, "Chamada perdida", fonteTitulo, cor::VERMELHO, Alinha::Centro);
  numeroGrande(q, _numeroChamada, 140, cor::TEXTO);
  if (!_perdidas.empty() && !_perdidas.front().quando.empty())
    q.texto(L / 2, 170, _perdidas.front().quando, fontePequena, cor::SUAVE, Alinha::Centro);
  _dicas(q, _numeroChamada.empty() ? "" : "Ligar de volta", "Fechar");
}

void Interface::_desenharPerdidas(Quadro& q) const {
  _barra(q, true);
  q.texto(MARGEM, 52, "Chamadas perdidas", fonteTitulo, cor::TEXTO);
  const int linhas = 5, alt = 42, y0 = 64;
  const int n = (int)_perdidas.size();
  const int primeiro = _selecao >= linhas ? _selecao - linhas + 1 : 0;
  for (int i = primeiro; i < n && i < primeiro + linhas; i++) {
    const int y = y0 + (i - primeiro) * alt;
    if (i == _selecao) q.retanguloRedondo(8, y, L - 16, alt - 4, 9, cor::CARTAO);
    const std::string num = _perdidas[i].numero.empty() ? "Número privado" : formatarNumero(_perdidas[i].numero);
    q.texto(20, y + 25, Quadro::caber(num, fonteTitulo, 150), fonteTitulo, cor::TEXTO);
    q.texto(L - 20, y + 25, _perdidas[i].quando, fontePequena, cor::SUAVE, Alinha::Direita);
  }
  _dicas(q, "Ligar", "Fechar", n > 1 ? "2 ▲  8 ▼" : "");
}

// ----- Mensagens -----

static std::string contagemDePartes(int partes) {
  return partes == 1 ? "1 SMS" : std::to_string(partes) + " SMS";
}

void Interface::_desenharMenu(Quadro& q) const {
  _barra(q, true);
  q.texto(MARGEM, 50, "Menu", fonteTitulo, cor::TEXTO);
  const auto itens = _itensMenu();
  const int alt = 52, y0 = 62;
  const int sel = _menuSel < (int)itens.size() ? _menuSel : (int)itens.size() - 1;
  for (int i = 0; i < (int)itens.size(); i++) {
    const int y = y0 + i * alt;
    if (i == sel) q.retanguloRedondo(8, y, L - 16, alt - 6, 10, cor::CARTAO);
    std::string titulo, sub;
    uint16_t marca = cor::AZUL;
    switch (itens[i]) {
      case Item::Escrever:
        titulo = "Escrever mensagem";
        sub = "Um SMS novo";
        break;
      case Item::Rascunho: {
        titulo = "Continuar rascunho";
        Caixa::Mensagem m;
        if (!_mt.vazio()) sub = comecoDoTexto(_mt.texto());
        else if (_caixa && _caixa->ler(_rascunhoMaisRecente(), m)) sub = comecoDoTexto(m.texto);
        marca = cor::AMBAR;
        break;
      }
      case Item::Mensagens: {
        titulo = "Mensagens";
        const size_t total = _caixa ? _caixa->total() : 0, novas = _caixa ? _caixa->novas() : 0;
        if (novas) sub = std::to_string(novas) + (novas == 1 ? " nova" : " novas") + " · " + std::to_string(total) + " ao todo";
        else if (total) sub = std::to_string(total) + (total == 1 ? " guardada" : " guardadas");
        else sub = "Nenhuma ainda";
        break;
      }
      case Item::Rediscar:
        titulo = "Rediscar";
        sub = formatarNumero(_ultimoDiscado);
        marca = cor::VERDE;
        break;
    }
    q.circulo(26.f, y + 23.f, 5.f, marca);
    q.texto(42, y + 20, titulo, fonteTitulo, cor::TEXTO);
    q.texto(42, y + 38, Quadro::caber(sub, fontePequena, L - 42 - 18), fontePequena, cor::SUAVE);
  }
  _dicas(q, "Abrir", "Voltar", itens.size() > 1 ? "2 ▲  8 ▼" : "");
}

void Interface::_desenharEscrever(Quadro& q) const {
  _barra(q, true);
  // Para quem (ou "Nova mensagem") e, à direita, o modo das letras
  const std::string modo = _mt.nomeModo();
  const int wm = Quadro::largura(modo, fontePequena) + 14;
  q.retanguloRedondo(L - MARGEM - wm, 33, wm, 19, 9, cor::AVISO);
  q.texto(L - MARGEM - wm / 2, 47, modo, fontePequena, cor::TEXTO, Alinha::Centro);
  const std::string cab = _para.empty() ? "Nova mensagem" : "Para " + formatarNumero(_para);
  q.texto(MARGEM, 47, Quadro::caber(cab, fontePequena, L - 2 * MARGEM - wm - 8), fontePequena, cor::SUAVE);

  // O texto, com a última linha sempre à vista (o cursor fica no fim)
  const Fonte& f = fonteTexto;
  const int x0 = 18, y0 = 60, y1 = 267, alt = f.alturaLinha();
  q.retanguloRedondo(8, y0 - 5, L - 16, y1 - y0 + 9, 10, cor::CARTAO);
  const std::string& t = _mt.texto();
  if (t.empty()) {
    q.texto(x0 + 4, y0 + f.ascendente, "Escreva aqui…", f, cor::DISCRETO);
    q.retangulo(x0, y0 + 3, 2, alt - 5, cor::AZUL);
  } else {
    const auto linhas = Quadro::quebrarTudo(t, f, L - 2 * x0 - 4);
    const int n = (int)linhas.size(), visiveis = (y1 - y0) / alt;
    const int primeiro = n > visiveis ? n - visiveis : 0;
    int base = y0 + f.ascendente;
    for (int i = primeiro; i < n; i++, base += alt)
      q.texto(x0, base, t.substr(linhas[i].inicio, linhas[i].fim - linhas[i].inicio), f, cor::TEXTO);
    const Quadro::Trecho& ult = linhas.back();
    const int baseUlt = base - alt;
    const int xFim = x0 + Quadro::largura(t.substr(ult.inicio, ult.fim - ult.inicio), f);
    if (_mt.pendente() && _mt.inicioPendente() >= ult.inicio) {   // a letra que ainda pode mudar
      const int xp = x0 + Quadro::largura(t.substr(ult.inicio, _mt.inicioPendente() - ult.inicio), f);
      q.retangulo(xp, baseUlt + 3, std::max(xFim - xp, 5), 2, cor::AZUL);
    }
    q.retangulo(std::min(xFim + 1, L - 14), baseUlt - f.ascendente + 4, 2, alt - 5, cor::AZUL);   // cursor
    if (primeiro > 0) q.texto(L - 18, y0 + 9, "▲", fontePequena, cor::DISCRETO, Alinha::Centro);
  }

  // Tamanho: letras usadas / cabem, e em quantos SMS vai
  const pdu::Contagem c = pdu::contar(t);
  std::string tam = std::to_string(c.usados) + "/" + std::to_string(c.porParte * c.partes);
  if (c.partes > 1) tam = contagemDePartes(c.partes) + " · " + tam;
  q.texto(L - MARGEM, 287, tam, fontePequena, c.partes > 1 ? cor::AMBAR : cor::SUAVE, Alinha::Direita);
  q.texto(MARGEM, 287, "* modo   # apaga", fontePequena, cor::DISCRETO);
  _dicas(q, "Enviar", "Voltar");
}

void Interface::_desenharPara(Quadro& q) const {
  _barra(q, true);
  q.texto(MARGEM, 52, "Enviar para", fontePequena, cor::SUAVE);
  if (_para.empty()) {
    q.texto(L / 2, 122, "Digite o número", fonteTexto, cor::DISCRETO, Alinha::Centro);
  } else {
    const std::string n = formatarNumero(_para);
    if (Quadro::largura(n, fonteDiscador) <= L - 24) q.texto(L / 2, 134, n, fonteDiscador, cor::TEXTO, Alinha::Centro);
    else q.texto(L / 2, 128, Quadro::caber(n, fonteGrande, L - 24), fonteGrande, cor::TEXTO, Alinha::Centro);
  }
  // A mensagem, pequena, para conferir
  q.retanguloRedondo(12, 166, L - 24, 96, 10, cor::CARTAO);
  const auto linhas = Quadro::quebrar(_mt.texto(), fontePequena, L - 48);
  for (size_t i = 0; i < linhas.size() && i < 3; i++) {
    std::string l = linhas[i];
    if (i == 2 && linhas.size() > 3) l = Quadro::caber(l + " …", fontePequena, L - 48);
    q.texto(24, 188 + (int)i * 18, l, fontePequena, cor::SUAVE);
  }
  q.texto(L - 24, 254, contagemDePartes(pdu::contar(_mt.texto()).partes), fontePequena, cor::DISCRETO,
          Alinha::Direita);
  q.texto(MARGEM, 287, "# apaga", fontePequena, cor::DISCRETO);
  _dicas(q, "Enviar", "Voltar");
}

void Interface::_desenharEnviando(Quadro& q, uint32_t agora) const {
  _barra(q, true);
  q.texto(L / 2, 92, "Enviando mensagem", fonteTitulo, cor::SUAVE, Alinha::Centro);
  numeroGrande(q, _envioPara, 136, cor::TEXTO);
  _pontinhos(q, 180, agora);
  if (_envioPartes > 1)
    q.texto(L / 2, 222, "Em " + std::to_string(_envioPartes) + " partes", fontePequena, cor::SUAVE, Alinha::Centro);
}

void Interface::_desenharEnviado(Quadro& q) const {
  _barra(q, true);
  if (_envioOk) {
    q.circulo(L / 2.f, 112.f, 30.f, cor::VERDE);
    q.texto(L / 2, 112 + fonteGrande.maiuscula / 2, "✓", fonteGrande, cor::FUNDO, Alinha::Centro);
    q.texto(L / 2, 178, "Mensagem enviada", fonteTitulo, cor::TEXTO, Alinha::Centro);
    q.texto(L / 2, 204, Quadro::caber(formatarNumero(_envioPara), fonteTexto, L - 32), fonteTexto, cor::SUAVE,
            Alinha::Centro);
    return;                                    // some sozinha
  }
  q.circulo(L / 2.f, 92.f, 28.f, cor::AMBAR);
  q.texto(L / 2, 92 + fonteGrande.maiuscula / 2, "!", fonteGrande, cor::FUNDO, Alinha::Centro);
  q.texto(L / 2, 150, "Não foi possível enviar", fonteTitulo, cor::AMBAR, Alinha::Centro);
  q.texto(L / 2, 174, Quadro::caber(formatarNumero(_envioPara), fonteTexto, L - 32), fonteTexto, cor::TEXTO,
          Alinha::Centro);
  std::string explica = _envioErro.empty() ? "" : maiuscula(_envioErro) + ". ";
  explica += _envioId ? "A mensagem ficou guardada em Mensagens." : "Não deu para guardar a mensagem.";
  const auto linhas = Quadro::quebrar(explica, fontePequena, L - 40);
  int b = 202;
  for (size_t i = 0; i < linhas.size() && i < 4; i++, b += 18)
    q.texto(L / 2, b, linhas[i], fontePequena, cor::SUAVE, Alinha::Centro);
  _dicas(q, "Tentar de novo", "Fechar");
}

void Interface::_desenharLista(Quadro& q) const {
  _barra(q, true);
  q.texto(MARGEM, 50, "Mensagens", fonteTitulo, cor::TEXTO);
  const auto& itens = _caixa ? _caixa->itens() : NENHUMA;
  const size_t novas = _caixa ? _caixa->novas() : 0;
  if (novas) q.texto(L - MARGEM, 50, std::to_string(novas) + (novas == 1 ? " nova" : " novas"), fontePequena, cor::AZUL, Alinha::Direita);
  else if (!itens.empty()) q.texto(L - MARGEM, 50, std::to_string(itens.size()), fontePequena, cor::SUAVE, Alinha::Direita);
  if (itens.empty()) {
    q.texto(L / 2, 150, "Nenhuma mensagem", fonteTexto, cor::SUAVE, Alinha::Centro);
    _dicas(q, "Escrever", "Voltar");
    return;
  }
  const int linhas = 5, alt = 46, y0 = 60, n = (int)itens.size();
  int sel = _caixa->posicao(_listaSel);
  if (sel < 0) sel = 0;
  const int primeiro = sel >= linhas ? sel - linhas + 1 : 0;
  for (int i = primeiro; i < n && i < primeiro + linhas; i++) {
    const int y = y0 + (i - primeiro) * alt;
    if (i == sel) q.retanguloRedondo(6, y, L - 12, alt - 4, 9, cor::CARTAO);
    const Caixa::Estado e = itens[i].estado;
    // A marca: nova (bolinha azul), enviada (seta), não enviada (!) ou rascunho (…)
    if (e == Caixa::Nova) q.circulo(18.5f, y + 14.5f, 4.f, cor::AZUL);
    else if (e == Caixa::Enviada) q.texto(18, y + 20, "↑", fontePequena, cor::SUAVE, Alinha::Centro);
    else if (e == Caixa::NaoEnviada) q.texto(18, y + 20, "!", fonteTitulo, cor::AMBAR, Alinha::Centro);
    else if (e == Caixa::Rascunho) q.texto(18, y + 20, "…", fontePequena, cor::AMBAR, Alinha::Centro);
    Caixa::Mensagem m;
    const bool ok = _caixa->ler(itens[i].id, m);
    const std::string quando = ok ? _quando(m.dataHora) : "";
    const int wq = Quadro::largura(quando, fontePequena);
    const std::string num = !ok ? "(não consegui ler)"
                            : !m.numero.empty() ? formatarNumero(m.numero)
                            : e == Caixa::Rascunho ? "(sem destinatário)" : "Número privado";
    const Fonte& fn = e == Caixa::Nova ? fonteTitulo : fonteTexto;   // as novas em negrito
    q.texto(30, y + 20, Quadro::caber(num, fn, L - 30 - 12 - wq - 6), fn, cor::TEXTO);
    q.texto(L - 12, y + 20, quando, fontePequena, cor::SUAVE, Alinha::Direita);
    std::string previa = ok ? comecoDoTexto(m.texto) : "";
    if (e == Caixa::NaoEnviada) previa = "Não enviada · " + previa;
    else if (e == Caixa::Rascunho) previa = "Rascunho · " + previa;
    q.texto(30, y + 37, Quadro::caber(previa, fontePequena, L - 30 - 16), fontePequena,
            e == Caixa::Nova ? cor::TEXTO : (e == Caixa::NaoEnviada || e == Caixa::Rascunho) ? cor::AMBAR : cor::SUAVE);
  }
  if (n > linhas) {                                   // onde estamos na lista
    const int trilho = linhas * alt - 4, x = L - 3;
    q.retangulo(x, y0, 2, trilho, cor::APAGADO);
    int h = trilho * linhas / n;
    if (h < 12) h = 12;
    const int yb = y0 + (trilho - h) * primeiro / (n - linhas);
    q.retanguloRedondo(x - 1, yb, 4, h, 2, cor::SUAVE);
  }
  _dicas(q, "Abrir", "Voltar", n > 1 ? "2 ▲  8 ▼" : "");
}

void Interface::_desenharLer(Quadro& q) {
  _barra(q, true);
  const bool recebida = _msg.estado == Caixa::Nova || _msg.estado == Caixa::Lida;
  const bool falhou = _msg.estado == Caixa::NaoEnviada, rascunho = _msg.estado == Caixa::Rascunho;
  q.texto(MARGEM, 46,
          recebida ? "De" : falhou ? "Para · não enviada" : rascunho ? "Rascunho, para" : "Para · enviada",
          fontePequena, (falhou || rascunho) ? cor::AMBAR : cor::SUAVE);
  q.texto(L - MARGEM, 46, _quando(_msg.dataHora), fontePequena, cor::SUAVE, Alinha::Direita);
  const std::string num = !_msg.numero.empty() ? formatarNumero(_msg.numero)
                          : rascunho ? "(sem destinatário)" : "Número privado";
  q.texto(MARGEM, 70, Quadro::caber(num, fonteTitulo, L - 2 * MARGEM), fonteTitulo, cor::TEXTO);
  q.linhaH(MARGEM, 80, L - 2 * MARGEM, cor::DIVISOR);
  const auto linhas = Quadro::quebrar(_msg.texto, fonteTexto, L - 2 * MARGEM - 8);
  _linhasTotal = (int)linhas.size();
  _linhasVisiveis = _textoLongo(q, linhas, fonteTexto, 88, 268, _rolagem, cor::TEXTO);
  // Dica das teclas: o 5 abre as opções; 2 e 8 rolam, se o texto não coube
  const bool rola = _linhasTotal > _linhasVisiveis;
  q.texto(L / 2, 287, rola ? "2 ▲  8 ▼  ·  5 opções" : "5 opções", fontePequena, cor::DISCRETO, Alinha::Centro);
  const auto ops = _opcoes();
  const std::string verde = ops[0] == Opcao::Responder      ? "Responder"
                            : ops[0] == Opcao::TentarDeNovo ? "Reenviar"
                            : ops[0] == Opcao::Continuar    ? "Continuar"
                                                            : "Opções";
  _dicas(q, verde, "Voltar");
}

static const char* nomeDaOpcao(int o) {
  static const char* const nomes[] = {"Responder",       "Ligar",  "Encaminhar",         "Tentar de novo",
                                      "Editar e enviar", "Apagar", "Continuar escrevendo"};
  return nomes[o];
}

void Interface::_desenharOpcoes(Quadro& q) const {
  _barra(q, true);
  q.texto(MARGEM, 50, "Opções", fonteTitulo, cor::TEXTO);
  q.texto(L - MARGEM, 50, Quadro::caber(formatarNumero(_msg.numero), fontePequena, 130), fontePequena, cor::SUAVE,
          Alinha::Direita);
  const auto ops = _opcoes();
  const int alt = 42, y0 = 62;
  for (int i = 0; i < (int)ops.size(); i++) {
    const int y = y0 + i * alt;
    if (i == _opcaoSel) q.retanguloRedondo(8, y, L - 16, alt - 4, 9, cor::CARTAO);
    q.texto(24, y + 25, nomeDaOpcao((int)ops[i]), fonteTitulo, ops[i] == Opcao::Apagar ? cor::VERMELHO : cor::TEXTO);
  }
  _dicas(q, "Escolher", "Voltar", "2 ▲  8 ▼");
}

void Interface::_desenharApagar(Quadro& q) const {
  _barra(q, true);
  q.anel(L / 2.f, 100.f, 26.f, 3.f, cor::VERMELHO);
  q.texto(L / 2, 100 + fonteGrande.maiuscula / 2, "✗", fonteGrande, cor::VERMELHO, Alinha::Centro);
  const bool rascunho = _msg.estado == Caixa::Rascunho;
  q.texto(L / 2, 162, rascunho ? "Apagar este rascunho?" : "Apagar esta mensagem?", fonteTitulo, cor::TEXTO,
          Alinha::Centro);
  const bool recebida = _msg.estado == Caixa::Nova || _msg.estado == Caixa::Lida;
  const std::string quem = (recebida ? "De " : "Para ") +
                           (!_msg.numero.empty() ? formatarNumero(_msg.numero)
                            : rascunho          ? std::string("(sem destinatário)")
                                                : std::string("número privado"));
  q.texto(L / 2, 188, Quadro::caber(quem, fonteTexto, L - 32), fonteTexto, cor::SUAVE, Alinha::Centro);
  q.texto(L / 2, 212, Quadro::caber("“" + comecoDoTexto(_msg.texto) + "”", fontePequena, L - 40), fontePequena,
          cor::DISCRETO, Alinha::Centro);
  _dicas(q, "Apagar", "Cancelar");
}

void Interface::_desenharUssdEspera(Quadro& q, uint32_t agora) const {
  _barra(q, true);
  q.texto(L / 2, 118, "Consultando a operadora", fonteTitulo, cor::SUAVE, Alinha::Centro);
  q.texto(L / 2, 160, Quadro::caber(_ussdCodigo, fonteGrande, L - 24), fonteGrande, cor::TEXTO, Alinha::Centro);
  _pontinhos(q, 196, agora);
  _dicas(q, "", "Cancelar");
}

void Interface::_desenharUssd(Quadro& q) {
  _barra(q, true);
  q.texto(MARGEM, 46, "Operadora", fontePequena, cor::SUAVE);
  q.texto(L - MARGEM, 46, _ussdCodigo, fontePequena, cor::SUAVE, Alinha::Direita);
  const int y0 = 56, y1 = _ussdMenu ? 238 : 286;
  const Fonte* f = &fonteTexto;
  auto linhas = Quadro::quebrar(_ussdTexto, *f, L - 2 * MARGEM - 8);
  if ((int)linhas.size() * f->alturaLinha() > y1 - y0) {     // não cabe: letra menor
    f = &fontePequena;
    linhas = Quadro::quebrar(_ussdTexto, *f, L - 2 * MARGEM - 8);
  }
  _linhasTotal = (int)linhas.size();
  _linhasVisiveis = _textoLongo(q, linhas, *f, y0, y1, _rolagem, cor::TEXTO);
  if (_ussdMenu) {
    q.retanguloRedondo(MARGEM - 4, 244, L - 2 * MARGEM + 8, 40, 10, cor::CARTAO);
    const int x = MARGEM + 6;
    const int w1 = q.texto(x, 269, "Resposta:", fontePequena, cor::SUAVE);
    const int w2 = q.texto(x + w1 + 8, 270, _ussdResposta, fonteTitulo, cor::TEXTO);
    q.retangulo(x + w1 + 8 + w2 + 2, 254, 2, 20, cor::AZUL);     // cursor
    _dicas(q, "Enviar", _ussdResposta.empty() ? "Sair" : "Apagar");
  } else {
    _dicas(q, "OK", "Fechar", _linhasTotal > _linhasVisiveis ? "2 ▲  8 ▼" : "");
  }
}

void Interface::_desenharImei(Quadro& q) const {
  _barra(q, true);
  q.texto(L / 2, 110, "IMEI", fonteTitulo, cor::SUAVE, Alinha::Centro);
  const std::string v = _imei.empty() ? "não disponível" : _imei;
  const Fonte& f = Quadro::largura(v, fonteGrande) <= L - 24 ? fonteGrande : fonteTitulo;
  q.texto(L / 2, 148, v, f, cor::TEXTO, Alinha::Centro);
  q.texto(L / 2, 190, "TijoloOS, Fase 3", fontePequena, cor::SUAVE, Alinha::Centro);
  _dicas(q, "OK", "");
}

void Interface::_desenharDesligar(Quadro& q) const {
  _barra(q, true);
  // símbolo de liga/desliga: anel aberto em cima + traço
  q.anel(L / 2.f, 112.f, 24.f, 4.f, cor::VERMELHO);
  q.retangulo(L / 2 - 9, 82, 18, 18, cor::FUNDO);
  q.retanguloRedondo(L / 2 - 2, 80, 4, 30, 2, cor::VERMELHO);
  q.texto(L / 2, 176, "Desligar o telefone?", fonteTitulo, cor::TEXTO, Alinha::Centro);
  const auto linhas = Quadro::quebrar(_comoLigar, fontePequena, L - 48);
  int b = 202;
  for (const auto& l : linhas) { q.texto(L / 2, b, l, fontePequena, cor::SUAVE, Alinha::Centro); b += 18; }
  _dicas(q, "Desligar", "Cancelar");
}

void Interface::_desenharTeclado(Quadro& q) const {
  _barra(q, true);
  q.texto(L / 2, 52, "Configurar o teclado", fonteTitulo, cor::TEXTO, Alinha::Centro);
  if (!_tecladoFim) {
    q.texto(L / 2, 84, "Aperte a tecla", fonteTexto, cor::SUAVE, Alinha::Centro);
    q.texto(L / 2, 172, std::string(1, Teclado::teclaDoIndice(_passo)), fonteRelogio, cor::TEXTO, Alinha::Centro);
    for (int i = 0; i < Teclado::TECLAS; i++) {   // progresso: 12 bolinhas
      const float x = L / 2.f - 88.f + i * 16.f, y = 204.f;
      if (i < _passo) q.circulo(x, y, 4.f, cor::VERDE);
      else if (i == _passo) q.anel(x, y, 4.5f, 1.5f, cor::TEXTO);
      else q.circulo(x, y, 3.f, cor::APAGADO);
    }
    if (!_msgTeclado.empty()) {
      const auto linhas = Quadro::quebrar(_msgTeclado, fontePequena, L - 40);
      int b = 240;
      for (const auto& l : linhas) { q.texto(L / 2, b, l, fontePequena, cor::AMBAR, Alinha::Centro); b += 18; }
    }
    _dicas(q, "Recomeçar", "Pular");
  } else if (_tecladoOk) {
    q.texto(L / 2, 140, "Teclado pronto!", fonteGrande, cor::VERDE, Alinha::Centro);
    if (_nomesFios) {
      std::string lin = "Linhas:", col = "Colunas:";
      for (int i = 0; i < 4; i++) lin += std::string(" ") + _nomesFios[_linhasFio[i]];
      for (int i = 0; i < 3; i++) col += std::string(" ") + _nomesFios[_colunasFio[i]];
      q.texto(L / 2, 178, lin, fontePequena, cor::SUAVE, Alinha::Centro);
      q.texto(L / 2, 198, col, fontePequena, cor::SUAVE, Alinha::Centro);
    }
  } else {
    q.texto(L / 2, 112, "Algo não bateu", fonteGrande, cor::AMBAR, Alinha::Centro);
    const auto linhas = Quadro::quebrar(
        "As teclas não formam a grade de um teclado de telefone. Confira os fios e tente de novo.",
        fonteTexto, L - 32);
    int b = 150;
    for (const auto& l : linhas) { q.texto(L / 2, b, l, fonteTexto, cor::SUAVE, Alinha::Centro); b += fonteTexto.alturaLinha(); }
    _dicas(q, "Refazer", "Usar assim");
  }
}

void Interface::_desenharAviso(Quadro& q) const {
  auto linhas = Quadro::quebrar(_aviso, fontePequena, L - 48);
  if (linhas.size() > 3) linhas.resize(3);
  const int alt = (int)linhas.size() * 18 + 16;
  const int y = 286 - alt;
  q.retanguloRedondo(12, y, L - 24, alt, 10, cor::AVISO);
  int b = y + 8 + fontePequena.maiuscula + 2;
  for (const auto& l : linhas) { q.texto(L / 2, b, l, fontePequena, cor::TEXTO, Alinha::Centro); b += 18; }
}

void Interface::_desenharVolume(Quadro& q) const {
  const int y = 188, h = 46;
  q.retanguloRedondo(20, y, L - 40, h, 12, cor::AVISO);
  q.texto(36, y + 28, "Volume", fontePequena, cor::TEXTO);
  for (int i = 0; i < 7; i++)
    q.retanguloRedondo(98 + i * 15, y + 15, 11, 16, 3, i < _volume ? cor::TEXTO : cor::APAGADO);
}

void Interface::desenharLigando(Quadro& q, const std::string& texto) {
  q.semRecorte();
  q.limpar(cor::FUNDO);
  const int x = 84, y = 92, w = 72, h = 44;        // um tijolinho
  q.retanguloRedondo(x, y, w, h, 6, cor::TIJOLO);
  q.retangulo(x, y + h / 2 - 1, w, 2, cor::FUNDO);
  q.retangulo(x + w / 2 - 1, y, 2, h / 2 - 1, cor::FUNDO);
  q.retangulo(x + w / 4 - 1, y + h / 2 + 1, 2, h / 2 - 1, cor::FUNDO);
  q.retangulo(x + 3 * w / 4 - 1, y + h / 2 + 1, 2, h / 2 - 1, cor::FUNDO);
  q.texto(L / 2, 178, "Tijolo", fonteGrande, cor::TEXTO, Alinha::Centro);
  const auto linhas = Quadro::quebrar(texto, fontePequena, L - 40);
  int b = 206;
  for (const auto& l : linhas) { q.texto(L / 2, b, l, fontePequena, cor::SUAVE, Alinha::Centro); b += 18; }
}

void Interface::desenharDesligado(Quadro& q) {
  q.semRecorte();
  q.limpar(cor::FUNDO);
}
