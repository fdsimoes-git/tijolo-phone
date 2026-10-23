// ============================================================
//  TelaIli9341 — leva o Quadro para a tela ILI9341 de verdade
//
//  Usa a biblioteca LovyanGFX (a mesma do robô de mesa) só para
//  conversar com a tela pelo SPI. O desenho é todo feito no Quadro.
//  Para economizar tempo, manda só as linhas da imagem que mudaram
//  desde a última vez (o relógio mudando não redesenha a tela toda).
//
//  Tela que se perde: uma queda de tensão ou ruído nos fios faz a tela
//  esquecer a configuração e ficar branca, e ela não avisa. Dois jeitos
//  de vigiar:
//   - com o fio MISO (SDO da tela → IO13): o Tijolo lê o "boletim" da
//     tela a cada conferirMs, e também se o painel continua varrendo as
//     linhas (o painel pode travar branco com o boletim certo), conserta
//     na hora e anota o que aconteceu (VigiaTela.h, que também reenvia
//     tudo de minuto em minuto, para o que o boletim não mostra);
//   - sem ele (ou se ele parar de responder): reenvia a configuração e a
//     imagem a cada vigiaMs, às cegas.
//
//  Só existe na placa (precisa do Arduino e da LovyanGFX).
// ============================================================
#pragma once
#ifdef ARDUINO
#include <Arduino.h>
#include "EstadoTela.h"
#include "Quadro.h"
#include "VigiaTela.h"

struct ConfigTela {
  int sck, mosi, cs, dc, rst;
  int luz;              // pino do LED da tela (-1 = ligado direto no 3V3)
  uint32_t spiHz;       // 40 MHz; se aparecer chuvisco, 27 MHz
  uint8_t rotacao;      // 0 = em pé, com o conector em cima; 2 = de cabeça para baixo
  bool inverterCores;   // fundo branco onde devia ser preto? true
  bool trocarRGB;       // vermelho e azul trocados? true
  uint32_t luzHz;       // frequência do PWM do brilho
  uint32_t vigiaMs;     // sem o fio MISO: reconfirma às cegas, com a luz acesa (0 = nunca)
  int miso;             // SDO(MISO) da tela; -1 = não ligado
  uint32_t conferirMs;  // com o fio MISO: lê o estado da tela a cada conferirMs
};

class TelaIli9341 {
public:
  bool begin(const ConfigTela& c);
  void enviar(const Quadro& q);      // só as linhas que mudaram
  void enviarTudo(const Quadro& q);

  // Refaz tudo: pulso no RESET + comandos de início. Leva ~0,4 s e a
  // imagem some enquanto isso: use com a luz apagada.
  void reiniciar();
  // RESET completo com a luz apagada; o loop reacende depois de redesenhar.
  // É o que destrava o painel quando o modem troca de 4G para 2G (o
  // controlador diz que está tudo certo, mas o painel fica branco).
  void refazerNoEscuro() { if (_ok && !_dormindo) { brilho(0); reiniciar(); } }
  // Reenvia a configuração sem RESET e sem apagar nada (~5 ms). Se a tela
  // estava bem, nada muda; se tinha se perdido, volta. O próximo envio
  // manda a imagem inteira.
  void reforcar();
  // Chame sempre no loop. Com o fio MISO: confere o estado e conserta.
  // Sem ele: com a luz acesa, a cada vigiaMs, reforcar() + imagem inteira.
  void vigiar(const Quadro& q, uint32_t agora);
  // A luz acabou de apagar / vai acender (o loop chama). Sem o fio MISO:
  // RESET no escuro / reenvio antes de acender (se a vigia às cegas estiver
  // ligada). Com o fio: confere na hora e reenvia depois de uma leitura boa.
  void aoApagar();
  void aoAcender();
  bool consertando() const { return temLeitura() && _vigia.consertando(); }
  void conferirJa() { if (temLeitura()) _vigia.conferirJa(millis()); }   // a próxima vigia lê na hora
  void definirVigia(uint32_t ms) { _c.vigiaMs = ms; }
  uint32_t vigiaMs() const { return _c.vigiaMs; }

  void brilho(uint8_t b);            // 0 a 255
  void dormir();                     // tela em repouso (gasta quase nada) e luz apagada
  void acordar();

  // ----- Diagnóstico -----
  bool temLeitura() const { return _temFio && _vigia.ativa(); }  // o fio MISO responde
  bool temFio() const { return _temFio; }                      // respondeu no começo
  uint32_t referencia() const { return _vigia.referencia(); } // estado logo depois de configurar
  bool lerEstado(uint32_t& estado);                           // RDDST agora; false sem o fio
  // Get_Scanline (45h): a linha que o painel varre, lida duas vezes com
  // 0,25 ms entre elas (valores crus). Iguais = o painel parou de varrer.
  bool lerVarredura(uint32_t& primeira, uint32_t& segunda);
  bool temVarredura() const { return _varreduraOk; }          // o módulo responde ao 45h
  bool proximaOcorrencia(OcorrenciaTela& o) { return _vigia.proximaOcorrencia(o); }
  const ContagemTela& contagem() const { return _vigia.contagem(); }
  uint32_t spiHz() const { return _c.spiHz; }
  // Diagnóstico: pinta três faixas (vermelha, verde, azul) direto da RAM
  // interna, sem passar pelo Quadro, e lê de volta um ponto de cada uma
  // pela MISO (veja estado_tela::atrasoComCores). false: sem o fio MISO
  // (as faixas são pintadas assim mesmo).
  static constexpr int FAIXA_Y = 90;
  bool faixasDeTeste(uint64_t lidos[3]);
  // Experimento: liga/desliga o detector de tensão baixa da própria tela
  // (vale também para os consertos seguintes).
  void detectorDeTensao(bool ligado);
  bool detectorLigado() const;

private:
  bool testarLeitura(uint32_t& referencia);
  bool testarVarredura();
  bool varreduraParada();
  void reiniciarNoEscuro(const Quadro& q);

  // O que a VigiaTela faz com esta tela
  struct Acoes : AcoesTela {
    TelaIli9341* tela = nullptr;
    const Quadro* quadro = nullptr;
    uint32_t ler() override;
    void reforcar() override;
    void reiniciar() override;
    void esperarUs(uint32_t us) override { delayMicroseconds(us); }
    uint32_t agora() override { return millis(); }
    uint32_t ultimoReforco() override { return tela->_ultimaVigia; }
  };

  ConfigTela _c{};
  bool _ok = false;
  bool _tudo = true;
  bool _dormindo = false;
  uint8_t _brilho = 255;
  uint32_t _ultimaVigia = 0;              // último reenvio (às cegas ou de segurança)
  uint32_t _marca[Quadro::ALTURA] = {};   // "impressão digital" de cada linha já enviada
  bool _temFio = false;                   // o fio MISO passou no teste do começo
  bool _varreduraOk = false;              // e o comando 45h mostra a varredura andando
  VigiaTela _vigia;
  Acoes _acoes;
};
#endif
