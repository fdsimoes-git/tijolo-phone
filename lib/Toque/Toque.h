// ============================================================
//  Toque — gera o "trrrim" de telefone antigo como um arquivo WAV
//
//  Em vez de guardar um MP3 no firmware, o som é calculado na hora:
//  duas notas de sino somadas (1150 Hz e 1430 Hz), ligadas e desligadas
//  25 vezes por segundo, que é o "tremido" do martelinho batendo no
//  sino. Dois toques curtos e um silêncio; o modem repete enquanto
//  a chamada toca.
//
//  Formato: WAV PCM, 8000 amostras/s, 16 bits, mono (o mesmo que o
//  modem usa para gravar). Uns 38 KB.
//  Não depende do Arduino: dá para gerar e ouvir no computador.
// ============================================================
#pragma once
#include <math.h>
#include <stdint.h>
#include <vector>

namespace toque {

constexpr uint32_t TAXA = 8000;   // amostras por segundo

inline void escrever16(std::vector<uint8_t>& v, uint16_t x) { v.push_back(x & 0xFF); v.push_back(x >> 8); }
inline void escrever32(std::vector<uint8_t>& v, uint32_t x) { for (int i = 0; i < 4; i++) v.push_back((x >> (8 * i)) & 0xFF); }

// volume: 0.0 a 1.0 (0.5 já é alto no alto-falante de 2 W)
inline std::vector<uint8_t> gerarWav(float volume = 0.5f) {
  // Ritmo: toque 0,45 s · pausa 0,2 s · toque 0,45 s · silêncio 1,3 s
  const float trechos[][2] = { {0.45f, 1}, {0.20f, 0}, {0.45f, 1}, {1.30f, 0} };
  uint32_t total = 0;
  for (auto& t : trechos) total += (uint32_t)(t[0] * TAXA);

  std::vector<uint8_t> wav;
  wav.reserve(44 + total * 2);
  // Cabeçalho RIFF/WAVE
  wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
  escrever32(wav, 36 + total * 2);
  wav.insert(wav.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
  escrever32(wav, 16);          // tamanho do bloco fmt
  escrever16(wav, 1);           // PCM
  escrever16(wav, 1);           // mono
  escrever32(wav, TAXA);
  escrever32(wav, TAXA * 2);    // bytes por segundo
  escrever16(wav, 2);           // bytes por amostra
  escrever16(wav, 16);          // bits
  wav.insert(wav.end(), {'d', 'a', 't', 'a'});
  escrever32(wav, total * 2);

  const float PI2 = 6.28318530718f;
  for (auto& t : trechos) {
    const uint32_t n = (uint32_t)(t[0] * TAXA);
    for (uint32_t i = 0; i < n; i++) {
      float s = 0;
      if (t[1] > 0) {
        const float seg = (float)i / TAXA;
        // martelinho: 25 batidas/s, cada uma decaindo rápido
        const float fase = fmodf(seg * 25.0f, 1.0f);
        const float batida = expf(-fase * 6.0f);
        // entra e sai suave nas pontas do toque (sem estalo)
        float borda = 1.0f;
        const float ate = (float)(n - i) / TAXA;
        if (seg < 0.01f) borda = seg / 0.01f;
        if (ate < 0.01f) borda = ate / 0.01f;
        s = (0.6f * sinf(PI2 * 1150.0f * seg) + 0.4f * sinf(PI2 * 1430.0f * seg)) * batida * borda;
      }
      int v = (int)(s * volume * 32767.0f);
      if (v > 32767) v = 32767;
      if (v < -32768) v = -32768;
      escrever16(wav, (uint16_t)(int16_t)v);
    }
  }
  return wav;
}

}  // namespace toque
