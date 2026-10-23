#ifdef ARDUINO
#include "Medidor.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace {
SemaphoreHandle_t adcLivre = nullptr;            // o ADC1 é de uma tarefa por vez
portMUX_TYPE trava = portMUX_INITIALIZER_UNLOCKED; // janelas abertas: tarefa x loop
}

void Medidor::travarAdc() {
  if (adcLivre) xSemaphoreTake(adcLivre, portMAX_DELAY);
}

void Medidor::soltarAdc() {
  if (adcLivre) xSemaphoreGive(adcLivre);
}

bool Medidor::begin(int pino3v3, int pinoBateria) {
  _p3 = pino3v3;
  _pBat = pinoBateria;
  if (!adcLivre) adcLivre = xSemaphoreCreateMutex();
  if (_p3 < 0 || _ativo) return _ativo;
  uint32_t soma = 0;
  travarAdc();
  for (int i = 0; i < 32; i++) soma += analogReadMilliVolts(_p3);
  soltarAdc();
  _inicial = (int)(soma / 32);
  // Só com o resistor da placa (10 kΩ até o 3V3), o pino fica no máximo do
  // ADC (~3,1 V). Com o segundo resistor até o GND, fica na metade do 3V3.
  _ativo = _inicial > 1200 && _inicial < 2100;
  if (_ativo) {
    _ultimaColheita = millis();
    // Núcleo 0, prioridade baixa: o loop do Tijolo (núcleo 1) não sente.
    _ativo = xTaskCreatePinnedToCore(tarefa, "medidor", 4096, this, 1, nullptr, 0) == pdPASS;
  }
  return _ativo;
}

void Medidor::tarefa(void* arg) {
  Medidor* m = static_cast<Medidor*>(arg);
  for (;;) {
    travarAdc();
    for (int i = 0; i < 32; i++) {
      // 0 = erro do driver do ADC (o pino nunca mede 0 V de verdade aqui): pula
      const uint32_t a = analogReadMilliVolts(m->_p3);
      const uint32_t b = analogReadMilliVolts(m->_pBat);
      portENTER_CRITICAL(&trava);
      if (a) m->_aberta3.somar((uint16_t)(a * 2));
      if (b) m->_abertaBat.somar((uint16_t)(b * 2));
      portEXIT_CRITICAL(&trava);
    }
    soltarAdc();
    vTaskDelay(1);                 // deixa o resto do núcleo 0 trabalhar
  }
}

void Medidor::colher() {
  if (!_ativo) return;
  Janela a, b;
  portENTER_CRITICAL(&trava);
  a = _aberta3;
  b = _abertaBat;
  _aberta3 = Janela{};
  _abertaBat = Janela{};
  portEXIT_CRITICAL(&trava);
  _h3.colocar(a);
  _hBat.colocar(b);
  const uint32_t agora = millis();
  if (agora > _ultimaColheita) _porSegundo = a.n * 1000 / (agora - _ultimaColheita);
  _ultimaColheita = agora;
}
#endif
