#ifdef ARDUINO
#include "Sentinela.h"
#include <esp_cache.h>
#include <esp_heap_caps.h>

bool SentinelaPsram::begin() {
  if (_p) return true;
  // Alinhada à linha do cache (64 bytes), para dar para invalidar só ela.
  _p = (uint32_t*)heap_caps_aligned_alloc(64, PALAVRAS * 4, MALLOC_CAP_SPIRAM);
  if (!_p) return false;
  gravar();
  return true;
}

void SentinelaPsram::gravar() {
  for (uint32_t i = 0; i < PALAVRAS; i++) _p[i] = valorSentinela(i);
  esp_cache_msync(_p, PALAVRAS * 4, ESP_CACHE_MSYNC_FLAG_DIR_C2M);   // do cache para o chip
}

bool SentinelaPsram::conferir(uint32_t agora) {
  if (!_p || (int32_t)(agora - _proxima) < 0) return false;
  _proxima = agora + A_CADA_MS;
  _conferencias++;
  // Joga fora o que o cache tem dessa área: a leitura vem do chip.
  esp_cache_msync(_p, PALAVRAS * 4, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
  uint32_t erradas = 0;
  for (uint32_t i = 0; i < PALAVRAS; i++) {
    const uint32_t v = _p[i];
    if (v != valorSentinela(i)) {
      if (!erradas) { _lido = v; _esperado = valorSentinela(i); }
      erradas++;
    }
  }
  if (erradas) {
    _erradas = erradas;
    gravar();                        // grava de novo: dá para ver se ela volta a guardar
    if (!_ok) return false;
    _ok = false;
    if (!_perdas++) _primeira = agora;
    return true;
  }
  if (_ok) return false;
  _ok = true;
  return true;
}
#endif
