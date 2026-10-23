// ============================================================
//  VigiaTela — confere a tela pelo fio MISO e conserta quando ela se perde
//
//  A cada `conferirMs` lê o "boletim" da tela (RDDST, veja EstadoTela.h).
//  Se parece errado:
//   1. confirma: lê mais duas vezes, 1,54 ms e 3,08 ms depois da primeira
//      (um terço e dois terços do quadro do 2G), e as três têm de dar
//      EXATAMENTE o mesmo valor. No 2G o rádio transmite em janelas de
//      0,577 ms a cada 4,615 ms (até 4 janelas seguidas, 2,3 ms, nos dados):
//      três leituras espalhadas assim pelo quadro nunca caem todas dentro
//      da transmissão. Uma tela perdida de verdade fica parada no mesmo
//      estado. (No 4G transmitindo sem parar, nada separa; aí o resultado
//      aparece como "leituras com ruído" e consertos que dão certo de cara.)
//   2. conserta: reenvia a configuração (ou RESET direto, se o conversor
//      interno parou ou se o painel parou de varrer, porque o reenvio não
//      os religa);
//   3. 150 ms depois confere de novo (de novo com leituras confirmadas). Se
//      não voltou, RESET completo; se nem assim, avisa uma vez e tenta de
//      novo cada vez mais espaçado (2 s, 4 s, 8 s... até 1 min), para não
//      travar o telefone com consertos que não adiantam.
//  Uma falha só é contada uma vez, mesmo que leve várias tentativas, e só
//  é dada como resolvida com duas leituras boas.
//  Leituras todas 0 ou todas 1 (fio MISO solto, ou a tela sem energia
//  nenhuma) não são "consertadas": depois de 3 seguidas a vigia desiste e
//  avisa, e de 15 em 15 s confere se o fio voltou a responder.
//  Rede de segurança: depois de uma leitura boa, se faz 1 minuto que nada
//  foi reenviado (ou se pediram, ao acender a tela), reenvia tudo, para o
//  que o boletim não mostra (registradores de tensão, pixels corrompidos).
//
//  Só lógica: quem lê e conserta a tela de verdade é o TelaIli9341, pela
//  interface AcoesTela. Os testes no computador usam uma tela de mentira.
// ============================================================
#pragma once
#include <cstdint>
#include "EstadoTela.h"

// Uma vez em que a tela se perdeu (ou em que o fio MISO parou/voltou)
struct OcorrenciaTela {
  uint32_t quando = 0;          // millis() em que o Tijolo percebeu
  uint32_t estado = 0;          // o que a tela disse (RDDST), confirmado por 3 leituras
  estado_tela::Falha falha = estado_tela::Falha::Nenhuma;
  uint8_t conserto = 0;         // 0 = voltou sem conserto na hora; 1 = reenvio; 2 = RESET completo
  bool consertou = false;       // SemResposta: true = o fio voltou a responder
  uint16_t tentativas = 0;      // quantas vezes o Tijolo tentou consertar
  uint32_t depois = 0;          // o que a tela disse depois da última tentativa
};

struct ContagemTela {
  uint32_t falhas[estado_tela::N_FALHAS] = {};   // uma por falha, não por tentativa
  uint32_t leiturasRuins = 0;   // leitura errada que não se confirmou: ruído na leitura
  uint32_t consertosLeves = 0;  // o reenvio da configuração bastou
  uint32_t consertosCompletos = 0;
  uint32_t semConserto = 0;     // rodadas (reenvio + RESET) que não adiantaram
  uint32_t reforcosDeSeguranca = 0;
  uint32_t reenviosAsCegas = 0; // sem o fio MISO
};

// O que a vigia pode fazer com a tela
struct AcoesTela {
  virtual uint32_t ler() = 0;              // RDDST
  virtual void reforcar() = 0;             // reenvia a configuração e a imagem inteira
  virtual void reiniciar() = 0;            // RESET completo (com a luz apagada) e a imagem inteira
  virtual void esperarUs(uint32_t us) = 0; // espera curta, sem sair da vez
  virtual uint32_t agora() = 0;            // millis(): os consertos levam tempo
  virtual uint32_t ultimoReforco() = 0;    // millis() do último reenvio ou RESET, de quem for
  virtual ~AcoesTela() = default;
};

class VigiaTela {
public:
  static constexpr uint32_t CONFIRMA1_US = 1540;      // 2ª leitura: 1/3 do quadro do 2G depois da 1ª
  static constexpr uint32_t CONFIRMA2_US = 1540;      // 3ª: 2/3 do quadro depois da 1ª
  static constexpr uint32_t VERIFICAR_MS = 150;       // depois de um conserto
  static constexpr uint32_t ESPERA_INICIAL_MS = 2000;
  static constexpr uint32_t ESPERA_MAXIMA_MS = 60000;
  static constexpr uint32_t REFORCO_MS = 60000;       // rede de segurança
  static constexpr uint32_t TENTAR_VOLTAR_MS = 15000; // depois de desistir do fio MISO
  static constexpr int SEM_RESPOSTA_PARA_DESISTIR = 3;
  static constexpr int RUIDOS_NA_VERIFICACAO = 10;

  void comecar(uint32_t referencia, uint32_t conferirMs, uint32_t agora) {
    *this = VigiaTela{};
    _referencia = referencia;
    _conferirMs = conferirMs;
    _prazo = agora;
    _ativa = true;
    _comecou = true;
  }
  bool ativa() const { return _ativa; }               // false: desistiu (fio MISO mudo)
  uint32_t referencia() const { return _referencia; }
  bool consertando() const { return _ativa && _etapa != Etapa::Vigiando; }
  // A próxima conferência é já (se não estiver no meio de um conserto)
  void conferirJa(uint32_t agora) { if (_ativa && _etapa == Etapa::Vigiando) _prazo = agora; }
  // Depois da próxima leitura boa, reenvia tudo (ao acender a tela)
  void pedirReforco() { _reforcoPedido = true; }
  const ContagemTela& contagem() const { return _cont; }
  ContagemTela& contagem() { return _cont; }

  bool proximaOcorrencia(OcorrenciaTela& o) {
    if (_tamanho == 0) return false;
    o = _fila[_inicio];
    _inicio = (_inicio + 1) % FILA;
    _tamanho--;
    return true;
  }

  // Chame sempre (no loop) enquanto ativa(). Não faz nada até chegar a hora.
  void passo(AcoesTela& a, uint32_t agora) {
    using namespace estado_tela;
    if (!_ativa || (int32_t)(agora - _prazo) < 0) return;
    switch (_etapa) {
      case Etapa::Vigiando: {
        _prazo = agora + _conferirMs;
        const uint32_t e = a.ler();
        const Falha f = classificar(_referencia, e);
        if (f == Falha::Nenhuma) {
          _semResposta = 0;
          if (_falhaAberta) {                          // voltou sem conserto na hora?
            if (!confirmaBoa(a)) { _cont.leiturasRuins++; return; }
            fechar(e, 0);
          }
          if (_reforcoPedido || a.agora() - a.ultimoReforco() >= REFORCO_MS) {
            _reforcoPedido = false;
            _cont.reforcosDeSeguranca++;
            a.reforcar();
          }
          return;
        }
        if (!confirma(a, e)) { _cont.leiturasRuins++; return; }
        if (f == Falha::SemResposta) { semResposta(a.agora(), e); return; }
        _semResposta = 0;
        if (!_falhaAberta) {                           // falha nova: conta uma vez só
          _falhaAberta = true;
          _avisouSemConserto = false;
          _espera = ESPERA_INICIAL_MS;
          _cont.falhas[(int)f]++;
          _atual = OcorrenciaTela{};
          _atual.quando = agora;
          _atual.estado = e;
          _atual.falha = f;
        }
        consertar(a, f == Falha::ConversorParou || f == Falha::PainelParado ? 2 : 1);
        return;
      }
      case Etapa::Verificando: {
        const uint32_t d = a.ler();
        const Falha f = classificar(_referencia, d);
        if (f == Falha::Nenhuma) {
          if (confirmaBoa(a)) { fechar(d, _atual.conserto); return; }
          ruido(agora);
          return;
        }
        if (!confirma(a, d)) { ruido(agora); return; }
        _ruidos = 0;
        if (f == Falha::SemResposta) { semResposta(a.agora(), d); return; }
        _atual.depois = d;
        if (_atual.conserto == 1) {                    // o reenvio não bastou: RESET completo
          consertar(a, 2);
          return;
        }
        // Nem o RESET resolveu: avisa uma vez e tenta de novo, cada vez mais espaçado.
        _cont.semConserto++;
        if (!_avisouSemConserto) {
          _avisouSemConserto = true;
          OcorrenciaTela o = _atual;
          o.consertou = false;
          guardar(o);
        }
        esperar(a.agora());
        return;
      }
      case Etapa::Esperando:
        _etapa = Etapa::Vigiando;
        _prazo = agora;
        return;
    }
  }

  // Depois de desistir: de tempos em tempos vê se o fio MISO voltou.
  void tentarVoltar(AcoesTela& a, uint32_t agora) {
    if (_ativa || !_comecou || (int32_t)(agora - _prazo) < 0) return;
    _prazo = agora + TENTAR_VOLTAR_MS;
    const uint32_t e = a.ler();
    if (!estado_tela::respondeu(e) || !confirma(a, e)) return;
    _ativa = true;
    _semResposta = 0;
    _etapa = Etapa::Vigiando;
    _prazo = agora;                                    // confere (e conserta) já
    OcorrenciaTela o;
    o.quando = agora;
    o.estado = e;
    o.falha = estado_tela::Falha::SemResposta;
    o.consertou = true;
    guardar(o);
  }

private:
  enum class Etapa : uint8_t { Vigiando, Verificando, Esperando };

  // Mais duas leituras iguais à primeira, fora da janela de rádio dela
  bool confirma(AcoesTela& a, uint32_t primeira) {
    a.esperarUs(CONFIRMA1_US);
    if (a.ler() != primeira) return false;
    a.esperarUs(CONFIRMA2_US);
    return a.ler() == primeira;
  }

  // Mais uma leitura boa, fora da janela de rádio da primeira
  bool confirmaBoa(AcoesTela& a) {
    a.esperarUs(CONFIRMA1_US);
    return estado_tela::classificar(_referencia, a.ler()) == estado_tela::Falha::Nenhuma;
  }

  void consertar(AcoesTela& a, uint8_t conserto) {
    _atual.conserto = conserto;
    if (_atual.tentativas < 0xFFFF) _atual.tentativas++;
    if (conserto == 2) a.reiniciar();
    else a.reforcar();
    _ruidos = 0;
    _etapa = Etapa::Verificando;
    _prazo = a.agora() + VERIFICAR_MS;
  }

  // Leituras que não se confirmam durante a verificação: confere de novo
  // daqui a pouco; se o ruído não passa, espera (sem travar o telefone).
  void ruido(uint32_t agora) {
    _cont.leiturasRuins++;
    if (++_ruidos >= RUIDOS_NA_VERIFICACAO) { _ruidos = 0; esperar(agora); return; }
    _prazo = agora + 50;
  }

  void esperar(uint32_t agora) {
    _etapa = Etapa::Esperando;
    _prazo = agora + _espera;
    _espera = _espera * 2 > ESPERA_MAXIMA_MS ? ESPERA_MAXIMA_MS : _espera * 2;
  }

  void semResposta(uint32_t agora, uint32_t e) {
    _etapa = Etapa::Vigiando;
    if (++_semResposta < SEM_RESPOSTA_PARA_DESISTIR) { _prazo = agora + 1000; return; }
    // Desiste: o fio solto (ou a tela sem energia) não se conserta reenviando nada.
    _ativa = false;
    _falhaAberta = false;
    _prazo = agora + TENTAR_VOLTAR_MS;
    _cont.falhas[(int)estado_tela::Falha::SemResposta]++;
    OcorrenciaTela o;
    o.quando = agora;
    o.estado = e;
    o.falha = estado_tela::Falha::SemResposta;
    guardar(o);
  }

  void fechar(uint32_t depois, uint8_t conserto) {
    _falhaAberta = false;
    _etapa = Etapa::Vigiando;
    if (conserto == 1) _cont.consertosLeves++;
    else if (conserto == 2) _cont.consertosCompletos++;
    OcorrenciaTela o = _atual;
    o.consertou = true;
    o.conserto = conserto;
    o.depois = depois;
    guardar(o);
  }

  void guardar(const OcorrenciaTela& o) {
    if (_tamanho == FILA) { _inicio = (_inicio + 1) % FILA; _tamanho--; }   // perde a mais velha
    _fila[(_inicio + _tamanho) % FILA] = o;
    _tamanho++;
  }

  bool _ativa = false, _comecou = false;
  uint32_t _referencia = 0, _conferirMs = 100, _prazo = 0;
  Etapa _etapa = Etapa::Vigiando;
  bool _falhaAberta = false, _avisouSemConserto = false, _reforcoPedido = false;
  uint32_t _espera = ESPERA_INICIAL_MS;
  int _semResposta = 0, _ruidos = 0;
  OcorrenciaTela _atual;
  ContagemTela _cont;
  static constexpr int FILA = 16;
  OcorrenciaTela _fila[FILA];
  int _inicio = 0, _tamanho = 0;
};
