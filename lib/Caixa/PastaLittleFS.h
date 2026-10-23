// ============================================================
//  PastaLittleFS — a Pasta da Caixa na memória flash da placa
//
//  Usa a partição "spiffs" da tabela default_16MB.csv (3,4 MB), com o
//  sistema de arquivos LittleFS: ele espalha as gravações pela flash
//  (não gasta sempre o mesmo lugar) e não deixa arquivo pela metade se a
//  energia cair.
//
//  Cuidados:
//   - Gravar: primeiro num arquivo ".tmp", que é conferido (o tamanho) e só
//     então renomeado para o nome de verdade. Se a memória encher ou a
//     energia cair no meio, a mensagem não aparece pela metade (nem vazia):
//     a gravação dá erro e o SMS continua no chip.
//   - Montar: só formata a partição se ela estiver vazia (a primeira vez).
//     Se ela tem dados e não monta (uma leitura ruim numa queda de tensão,
//     outra versão do LittleFS), nada é apagado: o Tijolo usa a RAM e avisa;
//     "caixa formatar" no monitor começa do zero, se for o caso.
//
//  Só existe na placa.
// ============================================================
#pragma once
#ifdef ARDUINO
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_partition.h>
#include "Caixa.h"

class PastaLittleFS : public Pasta {
public:
  enum class Montagem : uint8_t { Ok, Formatou, NaoMontou };

  // dir: "/caixa". aoFormatar: chamada antes de formatar a partição vazia
  // (só na primeira vez; pode levar alguns segundos).
  Montagem begin(const char* dir, void (*aoFormatar)() = nullptr) {
    _dir = dir;
    bool ok = LittleFS.begin(false);
    for (int i = 0; !ok && i < 2; i++) {         // uma leitura ruim não é motivo para apagar nada
      delay(150);
      ok = LittleFS.begin(false);
    }
    Montagem m = Montagem::Ok;
    if (!ok) {
      if (!particaoVazia()) return Montagem::NaoMontou;
      if (aoFormatar) aoFormatar();
      if (!LittleFS.begin(true)) return Montagem::NaoMontou;   // true: formata e monta
      m = Montagem::Formatou;
    }
    if (!LittleFS.exists(_dir.c_str()) && !LittleFS.mkdir(_dir.c_str())) return Montagem::NaoMontou;
    limparTemporarios();
    return m;
  }

  bool listar(std::vector<std::string>& nomes) override {
    File d = LittleFS.open(_dir.c_str());
    if (!d || !d.isDirectory()) return false;
    // Só os nomes (sem abrir cada arquivo: com centenas, abrir um por um é lento)
    for (;;) {
      bool pasta = false;
      const String n = d.getNextFileName(&pasta);
      if (n.length() == 0) break;
      if (!pasta) nomes.push_back(nomeSemPasta(n.c_str()));
    }
    d.close();
    return true;
  }

  bool ler(const std::string& nome, std::string& conteudo) override {
    File f = LittleFS.open(caminho(nome).c_str(), "r");
    if (!f) return false;
    const size_t n = f.size();
    conteudo.assign(n, '\0');
    const size_t lidos = n ? f.read((uint8_t*)&conteudo[0], n) : 0;
    f.close();
    return lidos == n;
  }

  bool escrever(const std::string& nome, const std::string& conteudo) override {
    const String final = caminho(nome);
    const String tmp = final + ".tmp";
    File f = LittleFS.open(tmp.c_str(), "w");
    if (!f) return false;
    const size_t escritos = f.write((const uint8_t*)conteudo.data(), conteudo.size());
    f.close();                                     // a LittleFS grava de fato aqui
    // O close() não diz se deu certo: confere pelo tamanho do que ficou
    bool ok = escritos == conteudo.size();
    if (ok) {
      File c = LittleFS.open(tmp.c_str(), "r");
      ok = c && c.size() == conteudo.size();
      if (c) c.close();
    }
    if (ok && LittleFS.exists(final.c_str())) LittleFS.remove(final.c_str());
    if (ok) ok = LittleFS.rename(tmp.c_str(), final.c_str());
    if (!ok) LittleFS.remove(tmp.c_str());
    return ok;
  }

  bool renomear(const std::string& de, const std::string& para) override {
    return LittleFS.rename(caminho(de).c_str(), caminho(para).c_str());
  }

  bool apagar(const std::string& nome) override { return LittleFS.remove(caminho(nome).c_str()); }

  // Espaço da partição (para o "status")
  static size_t usados() { return LittleFS.usedBytes(); }
  static size_t total() { return LittleFS.totalBytes(); }

  // "caixa formatar": apaga tudo e começa do zero
  bool formatar() {
    LittleFS.end();
    if (!LittleFS.format()) return false;
    return begin(_dir.c_str()) != Montagem::NaoMontou;
  }

private:
  // A partição nunca foi usada: os dois primeiros blocos (onde a LittleFS
  // guarda o começo dela) ainda estão apagados (só 0xFF)
  static bool particaoVazia() {
    const esp_partition_t* p =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
    if (!p) return false;
    uint8_t b[64];
    for (uint32_t bloco = 0; bloco < 2; bloco++) {
      if (esp_partition_read(p, bloco * 4096, b, sizeof b) != ESP_OK) return false;
      for (uint8_t x : b)
        if (x != 0xFF) return false;
    }
    return true;
  }

  // Sobras de uma gravação interrompida (a energia caiu no meio)
  void limparTemporarios() {
    std::vector<std::string> nomes;
    if (!listar(nomes)) return;
    for (const auto& n : nomes)
      if (n.size() > 4 && n.compare(n.size() - 4, 4, ".tmp") == 0) LittleFS.remove(caminho(n).c_str());
  }

  String caminho(const std::string& nome) const { return String(_dir.c_str()) + "/" + nome.c_str(); }
  static std::string nomeSemPasta(const char* n) {
    const char* barra = strrchr(n, '/');
    return barra ? std::string(barra + 1) : std::string(n);
  }
  std::string _dir;
};
#endif
