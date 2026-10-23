// ============================================================
//  Caixa — as mensagens guardadas na memória do Tijolo
//
//  Cada mensagem é um arquivo pequeno numa pasta da memória flash
//  (LittleFS): "0000002a.n" = mensagem 42, nova. A letra depois do ponto
//  é o estado, e mudar de estado é só renomear (a LittleFS faz isso de uma
//  vez: se a energia cair no meio, fica o nome velho ou o novo, nunca meio
//  arquivo). Dentro do arquivo:
//      número
//      data e hora ("26/10/21,18:05:12-12", como o modem manda)
//      texto (UTF-8, pode ter várias linhas)
//
//  O número da mensagem só cresce, então a ordem dos números é a ordem de
//  chegada. A lista fica na memória (uns 8 bytes por mensagem); o texto é
//  lido do arquivo quando a tela precisa.
//
//  Cheia (`maximo` mensagens), a mais antiga já lida ou enviada sai para
//  dar lugar à nova (os rascunhos e as novas, não lidas, só por último).
//  A nova é gravada antes: se a gravação falhar, nada sai.
//
//  Não depende do Arduino: quem mexe nos arquivos é uma Pasta (na placa,
//  PastaLittleFS.h; nos testes, uma pasta de mentira na memória).
// ============================================================
#pragma once
#include <stdint.h>
#include <string>
#include <vector>

// Uma pasta de arquivos pequenos
struct Pasta {
  virtual ~Pasta() = default;
  virtual bool listar(std::vector<std::string>& nomes) = 0;   // só os nomes
  virtual bool ler(const std::string& nome, std::string& conteudo) = 0;
  virtual bool escrever(const std::string& nome, const std::string& conteudo) = 0;   // cria ou substitui
  virtual bool renomear(const std::string& de, const std::string& para) = 0;
  virtual bool apagar(const std::string& nome) = 0;
};

class Caixa {
public:
  enum Estado : char {
    Nova = 'n',         // recebida, ainda não lida
    Lida = 'l',         // recebida e lida
    Enviada = 'e',
    NaoEnviada = 'f',   // o envio falhou (fica para tentar de novo)
    Rascunho = 'r',     // escrita e ainda não enviada
  };
  struct Item {
    uint32_t id;
    Estado estado;
  };
  struct Mensagem {
    uint32_t id = 0;
    Estado estado = Lida;
    std::string numero, dataHora, texto;
  };
  static constexpr size_t MAXIMO_PADRAO = 500;

  // Lê a lista da pasta. false se a pasta não responde.
  bool begin(Pasta& pasta, size_t maximo = MAXIMO_PADRAO);
  bool pronta() const { return _pasta != nullptr; }

  // Guarda e devolve o número dela (0 = não conseguiu gravar).
  uint32_t guardar(Estado estado, const std::string& numero, const std::string& dataHora,
                   const std::string& texto);
  bool ler(uint32_t id, Mensagem& m);
  bool mudar(uint32_t id, Estado estado);
  bool apagar(uint32_t id);
  size_t apagarTodas();

  const std::vector<Item>& itens() const { return _itens; }   // a mais nova primeiro
  size_t total() const { return _itens.size(); }
  size_t novas() const;
  int posicao(uint32_t id) const;                              // em itens(); -1 se não tem
  Estado estado(uint32_t id) const;                            // (Lida se não tem)
  // A mais antiga das novas (a próxima a mostrar); 0 se não há
  uint32_t primeiraNova() const;

  static std::string nomeDoArquivo(uint32_t id, Estado estado);
  // "0000002a.n" → id 42, Nova. false se o nome não é de mensagem.
  static bool lerNome(const std::string& nome, uint32_t& id, Estado& estado);

private:
  void _abrirEspaco();
  Pasta* _pasta = nullptr;
  size_t _maximo = MAXIMO_PADRAO;
  uint32_t _proximo = 1;
  std::vector<Item> _itens;

  // As últimas mensagens lidas (a lista da tela lê as mesmas várias vezes)
  static constexpr int CACHE = 8;
  Mensagem _cache[CACHE];
  int _proximoCache = 0;
  void _esquecer(uint32_t id);
};
