// ============================================================
//  Medidor — vigia a tensão de 3,3 V da placa (a que alimenta a tela)
//  e a da bateria, milhares de vezes por segundo
//
//  Serve para o diagnóstico da tela branca: um multímetro mostra a
//  média e não pega quedas de milissegundos (o modem puxa picos de até
//  2 A quando transmite). Aqui uma tarefa no outro núcleo da ESP32 lê o
//  ADC sem parar e guarda mínimo, máximo e média em janelas de 100 ms.
//
//  O 3V3 é medido pelo IO2 (SCL da barra J2): a placa já tem um resistor
//  de 10 kΩ do IO2 para o 3V3; com OUTRO de 10 kΩ do IO2 para o GND
//  (pinos 5 e 4 da J2), o IO2 fica com metade do 3V3. Sem esse resistor
//  o IO2 lê o máximo do ADC e o Medidor fica parado.
//
//  Historico (as janelas) é só lógica e roda nos testes do computador;
//  Medidor (a tarefa e o ADC) só na placa.
// ============================================================
#pragma once
#include <cstdint>

// Mínimo, máximo e soma das leituras (mV) de um pedaço de tempo
struct Janela {
  uint16_t minimo = 0xFFFF;
  uint16_t maximo = 0;
  uint64_t soma = 0;
  uint32_t n = 0;
  void somar(uint16_t mv) {
    if (mv < minimo) minimo = mv;
    if (mv > maximo) maximo = mv;
    soma += mv;
    n++;
  }
  void juntar(const Janela& j) {
    if (j.n == 0) return;
    if (j.minimo < minimo) minimo = j.minimo;
    if (j.maximo > maximo) maximo = j.maximo;
    soma += j.soma;
    n += j.n;
  }
  int media() const { return n ? (int)(soma / n) : 0; }
};

// As últimas 100 janelas de 100 ms (10 s)
class Historico {
public:
  static constexpr int JANELAS = 100;
  void colocar(const Janela& j) {
    _j[_pos] = j;
    _pos = (_pos + 1) % JANELAS;
    if (_cheias < JANELAS) _cheias++;
  }
  // Junta as últimas `quantas` janelas (as mais novas)
  Janela ultimas(int quantas) const {
    Janela r;
    if (quantas > _cheias) quantas = _cheias;
    for (int i = 1; i <= quantas; i++) r.juntar(_j[(_pos - i + JANELAS) % JANELAS]);
    return r;
  }

private:
  Janela _j[JANELAS];
  int _pos = 0, _cheias = 0;
};

#ifdef ARDUINO
#include <Arduino.h>

class Medidor {
public:
  // pino3v3: o IO com o resistor (metade do 3V3); pinoBateria: divisor 1:2
  // da placa. Devolve true se o resistor está lá e a medição começou.
  bool begin(int pino3v3, int pinoBateria);
  bool ativo() const { return _ativo; }
  int leituraInicial() const { return _inicial; }   // mV no pino, no begin()

  // Chame a cada ~100 ms: fecha a janela que estava aberta.
  void colher();
  // Os últimos `ms` milissegundos (até 10 s), já em mV reais (×2)
  Janela tensao3v3(uint32_t ms) const { return _h3.ultimas(ms / 100); }
  Janela bateria(uint32_t ms) const { return _hBat.ultimas(ms / 100); }
  uint32_t leiturasPorSegundo() const { return _porSegundo; }

  // O ADC não pode ser lido por duas tarefas ao mesmo tempo: quem mais lê
  // (a bateria, no loop) passa por aqui.
  static void travarAdc();
  static void soltarAdc();

private:
  static void tarefa(void* arg);
  int _p3 = -1, _pBat = -1;
  bool _ativo = false;
  int _inicial = 0;
  Janela _aberta3, _abertaBat;     // escritas pela tarefa
  Historico _h3, _hBat;
  uint32_t _porSegundo = 0, _ultimaColheita = 0;
};
#endif
