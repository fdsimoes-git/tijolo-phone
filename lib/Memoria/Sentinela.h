// ============================================================
//  Sentinela — avisa se a PSRAM perdeu os dados
//
//  A PSRAM é a memória extra da ESP32-S3 (2 MB, num chip à parte dentro do
//  módulo). Se ela se perder — uma queda rápida na alimentação pode fazer
//  isso sem derrubar a ESP32 —, tudo o que está nela vira lixo (em geral,
//  tudo 1). Se a imagem da tela morar lá, a tela fica branca com o
//  controlador dela dizendo que está tudo certo.
//
//  A sentinela guarda 4 KB com um padrão conhecido na PSRAM e confere de
//  tempos em tempos, lendo direto do chip (passando por cima do cache da
//  ESP32, que esconderia o problema).
//
//  valorSentinela() é só lógica (testada no computador); a classe, só na placa.
// ============================================================
#pragma once
#include <cstdint>

inline uint32_t valorSentinela(uint32_t i) { return 0xA5C3F00Fu ^ (i * 2654435761u); }

#ifdef ARDUINO
#include <Arduino.h>

class SentinelaPsram {
public:
  static constexpr uint32_t PALAVRAS = 1024;   // 4 KB
  static constexpr uint32_t A_CADA_MS = 500;

  bool begin();                      // false: placa sem PSRAM
  bool ativa() const { return _p != nullptr; }
  // Confere, se já deu a hora. Devolve true quando o estado muda
  // (estava boa e perdeu os dados, ou voltou a guardar).
  bool conferir(uint32_t agora);
  bool ok() const { return _ok; }
  uint32_t perdas() const { return _perdas; }          // quantas vezes perdeu os dados
  uint32_t conferencias() const { return _conferencias; }
  uint32_t erradas() const { return _erradas; }        // na última conferência ruim
  uint32_t lido() const { return _lido; }
  uint32_t esperado() const { return _esperado; }
  uint32_t primeiraPerda() const { return _primeira; } // millis()

private:
  void gravar();
  uint32_t* _p = nullptr;
  bool _ok = true;
  uint32_t _proxima = 0, _perdas = 0, _conferencias = 0, _erradas = 0;
  uint32_t _lido = 0, _esperado = 0, _primeira = 0;
};
#endif
