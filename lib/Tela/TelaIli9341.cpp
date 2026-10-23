#ifdef ARDUINO
#include "TelaIli9341.h"
#include <LovyanGFX.hpp>
#include <driver/gpio.h>

using estado_tela::Falha;

namespace {

// O ILI9341 com o que a LovyanGFX não tem pronto: refazer a configuração
// (com ou sem RESET) e ler o "boletim" da tela (RDDST) pelo fio MISO.
class PainelIli : public lgfx::Panel_ILI9341 {
public:
  // Pulso no RESET e a mesma lista de comandos do começo.
  void refazer() {
    rst_control(false);
    delay(10);
    rst_control(true);
    delay(120);
    startWrite(true);
    for (uint8_t i = 0; auto comandos = getInitCommands(i); i++) command_list(comandos);
    aplicarDetector();
    endWrite();
  }

  // A mesma lista de início, sem o RESET e sem as esperas longas.
  // Mandar de novo o valor que o registrador já tem não muda nada na
  // imagem; numa tela que voltou ao estado de fábrica, devolve tudo.
  void reforcar() {
    startWrite(true);
    for (uint8_t i = 0; auto lista = getInitCommands(i); i++) {
      for (const uint8_t* p = lista;;) {
        const uint8_t cmd = *p++;
        const uint8_t num = *p++;
        if (cmd == 0xFF && num == 0xFF) break;
        const uint8_t n = num & ~CMD_INIT_DELAY;
        if (cmd != CMD_SLPOUT && cmd != CMD_DISPON) {   // esses dois vão no fim, na ordem certa
          comando(cmd);
          for (uint8_t k = 0; k < n; k++) dado(p[k]);
        }
        p += n;
        if (num & CMD_INIT_DELAY) p++;                   // pula o tempo de espera
      }
    }
    aplicarDetector();
    comando(CMD_SLPOUT);   // sair do repouso: sem efeito se ela já estiver acordada
    delay(5);              // o ILI9341 pede 5 ms antes do próximo comando
    comando(CMD_NORON);    // modo normal (sai de rolagem/parcial, se um ruído ligou)
    comando(CMD_DISPON);   // imagem ligada
    endWrite();
  }

  // Um comando com um parâmetro (usado no teste da leitura).
  void comandoComDado(uint8_t c, uint8_t d) {
    startWrite(true);
    comando(c);
    dado(d);
    endWrite();
  }

  // RDDST (09h): 32 bits depois de `espera` ciclos de clock "vazios" (o
  // datasheet pede 1). Lê 40 bits seguidos e descarta o atraso aqui, sem
  // depender do truque que a LovyanGFX usa no S3 para gerar um clock só.
  uint32_t lerEstado() {
    startWrite();
    write_command(0x09);
    _bus->beginRead(0);
    uint64_t v = 0;
    for (int i = 0; i < 5; i++) v = (v << 8) | (_bus->readData(8) & 0xFF);
    cs_control(true);
    _bus->endRead();
    endWrite();
    if (_in_transaction) cs_control(false);
    return (uint32_t)(v >> (8 - espera));
  }

  // Get_Scanline (45h): a linha que o painel está varrendo agora (10 bits,
  // depois de um byte "vazio"). Para a vigia só importa se ela anda: devolve
  // os 32 bits lidos, sem decifrar.
  uint32_t lerVarredura() {
    startWrite();
    write_command(0x45);
    _bus->beginRead(0);
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) v = (v << 8) | (_bus->readData(8) & 0xFF);
    cs_control(true);
    _bus->endRead();
    endWrite();
    if (_in_transaction) cs_control(false);
    return v;
  }

  // RAMRD (2Eh) de um ponto: 48 bits seguidos depois do comando (o atraso
  // e as cores, quem separa é estado_tela::atrasoComCores).
  uint64_t lerPonto(uint16_t x, uint16_t y) {
    startWrite(true);
    comando(0x2A); dado(x >> 8); dado(x & 0xFF); dado(x >> 8); dado(x & 0xFF);
    comando(0x2B); dado(y >> 8); dado(y & 0xFF); dado(y >> 8); dado(y & 0xFF);
    endWrite();
    startWrite();
    write_command(0x2E);
    _bus->beginRead(0);
    uint64_t v = 0;
    for (int i = 0; i < 6; i++) v = (v << 8) | (_bus->readData(8) & 0xFF);
    cs_control(true);
    _bus->endRead();
    endWrite();
    if (_in_transaction) cs_control(false);
    return v;
  }

  // Detector de tensão baixa do ILI9341 (registrador B7h, bit GAS; vem
  // ligado). O datasheet não diz o que ele faz ao disparar: desligá-lo é
  // um experimento do diagnóstico (comando "tela detector"). Só escreve
  // quando o experimento está ativo; o RESET devolve o valor de fábrica.
  void aplicarDetector() {
    if (!semDetector) return;
    comando(0xB7);
    dado(0x07);                        // 06h (padrão) com GAS = 1: detector desligado
  }

  uint8_t espera = 1;                  // 0 a 8
  bool semDetector = false;

private:
  void comando(uint8_t c) { writeCommand(c, 1); _bus->flush(); }
  void dado(uint8_t d) { writeData(d, 1); _bus->flush(); }
};

// Descrição do hardware para a LovyanGFX (como no robô de mesa).
class Painel : public lgfx::LGFX_Device {
  PainelIli _painel;
  lgfx::Bus_SPI _spi;

public:
  explicit Painel(const ConfigTela& c) {
    auto b = _spi.config();
    b.spi_host = SPI2_HOST;
    b.spi_mode = 0;
    b.freq_write = c.spiHz;
    b.freq_read = 6000000;    // leitura do ILI9341: ciclo mínimo de 150 ns
    b.spi_3wire = false;
    b.use_lock = true;
    b.dma_channel = SPI_DMA_CH_AUTO;
    b.pin_sclk = c.sck;
    b.pin_mosi = c.mosi;
    b.pin_miso = c.miso;      // -1: a tela só recebe
    b.pin_dc = c.dc;
    _spi.config(b);
    _painel.setBus(&_spi);

    auto p = _painel.config();
    p.pin_cs = c.cs;
    p.pin_rst = c.rst;
    p.pin_busy = -1;
    p.panel_width = Quadro::LARGURA;
    p.panel_height = Quadro::ALTURA;
    p.readable = c.miso >= 0;
    p.invert = c.inverterCores;
    p.rgb_order = c.trocarRGB;
    p.bus_shared = false;
    _painel.config(p);
    setPanel(&_painel);
  }

  PainelIli& ili() { return _painel; }

  void refazer() {
    _painel.refazer();
    configurarImagem();
  }

  void reforcar() {
    _painel.reforcar();
    configurarImagem();
  }

  // Inversão de cores, 16 bits por ponto e rotação (MADCTL). setRotation
  // também faz a biblioteca esquecer a última janela de escrita, para
  // mandá-la de novo (a tela pode ter perdido a dela).
  void configurarImagem() {
    startWrite();
    invertDisplay(_painel.getInvert());
    setColorDepth(_painel.getWriteDepth());
    setRotation(_painel.getRotation());
    endWrite();
  }
};

Painel* painel = nullptr;

// Impressão digital de uma linha (240 pontos): se não mudou, não manda.
uint32_t marcaDaLinha(const uint16_t* p) {
  uint32_t h = 2166136261u;
  for (int i = 0; i < Quadro::LARGURA; i++) {
    h ^= p[i];
    h *= 16777619u;
  }
  return h;
}

}  // namespace

bool TelaIli9341::begin(const ConfigTela& c) {
  _c = c;
  if (_c.luz >= 0) {
    ledcAttach(_c.luz, _c.luzHz, 8);
    ledcWrite(_c.luz, 0);               // apagada até ter imagem
  }
  _brilho = 0;
  if (!painel) painel = new Painel(c);
  if (!painel->init()) return false;
  painel->setRotation(c.rotacao);
  painel->fillScreen(0);
  _ok = true;
  _tudo = true;
  _dormindo = false;
  _ultimaVigia = millis();
  // Fio MISO solto lê sempre tudo 1, nunca lixo que pareça um estado
  // (a saída da tela vence o pull-up interno com folga).
  if (_c.miso >= 0) gpio_pullup_en((gpio_num_t)_c.miso);
  uint32_t referencia = 0;
  _temFio = _c.miso >= 0 && testarLeitura(referencia);
  _varreduraOk = _temFio && testarVarredura();
  if (_temFio) _vigia.comecar(referencia, _c.conferirMs, millis());
  _acoes.tela = this;
  return true;
}

// O fio MISO funciona? Com a luz ainda apagada: lê o estado, vira a ordem
// das linhas (bit MY do MADCTL) e vê se a tela "conta" a mudança no bit
// certo (D30) e só nele; depois desvira. Um fio solto lê tudo 1 (ou lixo)
// e não passa. O datasheet pede 1 ciclo de clock "vazio" antes dos dados;
// as outras quantidades ficam de reserva, para módulos diferentes.
bool TelaIli9341::testarLeitura(uint32_t& referencia) {
  using namespace estado_tela;
  PainelIli& ili = painel->ili();
  for (uint8_t espera : {1, 0, 2, 3}) {
    ili.espera = espera;
    const uint32_t antes = ili.lerEstado();
    if (!funcionando(antes)) continue;
    ili.comandoComDado(0x36, madctl(antes) ^ 0x80);   // MADCTL com o MY trocado
    const uint32_t virada = ili.lerEstado();
    painel->configurarImagem();                       // volta o MADCTL configurado
    const uint32_t depois = ili.lerEstado();
    if (((antes ^ virada) & CONFERIR) == (1u << 30) && ((antes ^ depois) & CONFERIR) == 0) {
      referencia = depois;
      return true;
    }
  }
  ili.espera = 1;
  painel->configurarImagem();
  return false;
}

// A linha que o painel varre anda? Num quadro de ~14 ms o ILI9341 varre
// ~330 linhas (uma a cada ~43 µs): em 250 µs ela anda umas 6.
static constexpr uint32_t INTERVALO_VARREDURA_US = 250;

bool TelaIli9341::testarVarredura() {
  PainelIli& ili = painel->ili();
  uint32_t antes = ili.lerVarredura();
  int mudou = 0;
  for (int i = 0; i < 6; i++) {
    delayMicroseconds(INTERVALO_VARREDURA_US);
    const uint32_t agora = ili.lerVarredura();
    if (agora != antes) mudou++;
    antes = agora;
  }
  return mudou >= 5;           // um módulo que não responde ao 45h fica de fora
}

bool TelaIli9341::varreduraParada() {
  PainelIli& ili = painel->ili();
  const uint32_t a = ili.lerVarredura();
  delayMicroseconds(INTERVALO_VARREDURA_US);
  return ili.lerVarredura() == a;
}

bool TelaIli9341::lerVarredura(uint32_t& a, uint32_t& b) {
  if (!_ok || !_temFio) return false;
  a = painel->ili().lerVarredura();
  delayMicroseconds(INTERVALO_VARREDURA_US);
  b = painel->ili().lerVarredura();
  return true;
}

// O boletim (RDDST) e, se ele diz que está tudo ligado, se o painel varre
// as linhas: o painel pode parar com o boletim certo (a tela fica branca).
uint32_t TelaIli9341::Acoes::ler() {
  uint32_t e = painel->ili().lerEstado();
  if (tela->_varreduraOk && estado_tela::funcionando(e) && tela->varreduraParada()) e |= estado_tela::PARADA;
  return e;
}

void TelaIli9341::Acoes::reforcar() {
  tela->reforcar();
  tela->enviar(*quadro);              // a imagem inteira
}

void TelaIli9341::Acoes::reiniciar() { tela->reiniciarNoEscuro(*quadro); }

// RESET com a luz apagada: a tela fica escura ~0,4 s em vez de piscar branco.
void TelaIli9341::reiniciarNoEscuro(const Quadro& q) {
  const uint8_t b = _brilho;
  brilho(0);
  reiniciar();
  enviar(q);
  brilho(b);
}

void TelaIli9341::detectorDeTensao(bool ligado) {
  if (!_ok) return;
  PainelIli& ili = painel->ili();
  ili.semDetector = !ligado;
  if (ligado) {
    brilho(0);                 // no escuro; o loop reacende depois de redesenhar
    reiniciar();               // o RESET volta o registrador ao valor de fábrica
    return;
  }
  ili.startWrite(true);
  ili.aplicarDetector();
  ili.endWrite();
}

bool TelaIli9341::detectorLigado() const { return !painel || !painel->ili().semDetector; }

bool TelaIli9341::faixasDeTeste(uint64_t lidos[3]) {
  if (!_ok) return false;
  static uint16_t linha[Quadro::LARGURA];          // na RAM interna, não na PSRAM
  const uint16_t cores[3] = {0xF800, 0x07E0, 0x001F};
  painel->startWrite();
  for (int k = 0; k < 3; k++) {
    for (int i = 0; i < Quadro::LARGURA; i++) linha[i] = cores[k];
    for (int y = 0; y < 40; y++)
      painel->pushImage(0, FAIXA_Y + k * 50 + y, Quadro::LARGURA, 1, reinterpret_cast<const lgfx::rgb565_t*>(linha));
  }
  painel->endWrite();
  _tudo = true;                                    // a próxima imagem cobre as faixas
  if (!_temFio) return false;
  for (int k = 0; k < 3; k++) lidos[k] = painel->ili().lerPonto(Quadro::LARGURA / 2, FAIXA_Y + k * 50 + 20);
  painel->configurarImagem();                      // a janela de escrita mudou
  return true;
}

bool TelaIli9341::lerEstado(uint32_t& estado) {
  if (!_ok || !_temFio) return false;
  estado = painel->ili().lerEstado();
  return true;
}

void TelaIli9341::enviar(const Quadro& q) {
  if (!_ok) return;
  const uint16_t* pontos = q.pontos();
  const int L = Quadro::LARGURA, A = Quadro::ALTURA;
  painel->startWrite();
  int y = 0;
  while (y < A) {
    uint32_t m = marcaDaLinha(pontos + y * L);
    if (!_tudo && m == _marca[y]) { y++; continue; }
    // Achou uma linha que mudou: junta as próximas que também mudaram
    // (tolerando até 8 iguais no meio, para mandar menos pedaços).
    const int inicio = y;
    int fim = y, iguais = 0;
    _marca[y++] = m;
    while (y < A) {
      m = marcaDaLinha(pontos + y * L);
      if (_tudo || m != _marca[y]) { _marca[y] = m; fim = y; iguais = 0; }
      else if (++iguais > 8) break;
      y++;
    }
    painel->pushImage(0, inicio, L, fim - inicio + 1,
                      reinterpret_cast<const lgfx::rgb565_t*>(pontos + inicio * L));
  }
  painel->endWrite();
  _tudo = false;
}

void TelaIli9341::reiniciar() {
  if (!_ok) return;
  painel->refazer();
  _tudo = true;               // a memória da tela se perdeu: o próximo envio manda tudo
  _ultimaVigia = millis();
}

void TelaIli9341::reforcar() {
  if (!_ok) return;
  painel->reforcar();
  _tudo = true;               // se ela tinha se perdido, a memória dela é lixo: manda tudo
  _ultimaVigia = millis();
}

void TelaIli9341::vigiar(const Quadro& q, uint32_t agora) {
  if (!_ok || _dormindo) return;
  _acoes.tela = this;
  _acoes.quadro = &q;

  if (_temFio) {
    if (_vigia.ativa()) {
      _vigia.passo(_acoes, agora);
      if (_vigia.ativa()) return;
      _ultimaVigia = agora - _c.vigiaMs;   // acabou de desistir: às cegas já
    } else {
      _vigia.tentarVoltar(_acoes, agora);   // de 15 em 15 s: o fio voltou?
      if (_vigia.ativa()) return;
    }
  }

  // Às cegas (sem o fio, ou com ele mudo)
  if (_c.vigiaMs == 0 || _brilho == 0) return;
  if (agora - _ultimaVigia < _c.vigiaMs) return;
  reforcar();
  enviar(q);                  // a imagem inteira (_tudo); mesma imagem = nada pisca
  _vigia.contagem().reenviosAsCegas++;
}

void TelaIli9341::aoApagar() {
  // Ninguém vê a tela sumir. Também com o fio MISO: o painel pode travar
  // branco sem o controlador perceber (veja refazerNoEscuro).
  if (temLeitura() || _c.vigiaMs) reiniciar();
}

void TelaIli9341::aoAcender() {
  if (temLeitura()) {
    _vigia.conferirJa(millis());
    _vigia.pedirReforco();
  } else if (_c.vigiaMs) {
    reforcar();
  }
}

void TelaIli9341::enviarTudo(const Quadro& q) {
  _tudo = true;
  enviar(q);
}

void TelaIli9341::brilho(uint8_t b) {
  if (b == _brilho) return;
  _brilho = b;
  if (_c.luz >= 0) ledcWrite(_c.luz, b);
}

void TelaIli9341::dormir() {
  if (_c.luz >= 0) ledcWrite(_c.luz, 0);
  _brilho = 0;
  _dormindo = true;           // a vigia não "conserta" o repouso de propósito
  if (_ok) painel->sleep();
}

void TelaIli9341::acordar() {
  if (_ok) painel->wakeup();
  _dormindo = false;
  _tudo = true;
  if (temLeitura()) _vigia.conferirJa(millis());
}
#endif
