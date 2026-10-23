#include "Caixa.h"
#include <algorithm>
#include <stdio.h>

std::string Caixa::nomeDoArquivo(uint32_t id, Estado estado) {
  char b[16];
  snprintf(b, sizeof b, "%08lx.%c", (unsigned long)id, (char)estado);
  return b;
}

bool Caixa::lerNome(const std::string& nome, uint32_t& id, Estado& estado) {
  if (nome.size() != 10 || nome[8] != '.') return false;
  uint32_t v = 0;
  for (int i = 0; i < 8; i++) {
    const char c = nome[i];
    int d;
    if (c >= '0' && c <= '9') d = c - '0';
    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else return false;
    v = (v << 4) | (uint32_t)d;
  }
  const char e = nome[9];
  if (e != Nova && e != Lida && e != Enviada && e != NaoEnviada && e != Rascunho) return false;
  if (v == 0) return false;
  id = v;
  estado = (Estado)e;
  return true;
}

bool Caixa::begin(Pasta& pasta, size_t maximo) {
  _pasta = nullptr;
  _itens.clear();
  _proximo = 1;
  _maximo = maximo < 1 ? 1 : maximo;
  for (auto& c : _cache) c = Mensagem();
  std::vector<std::string> nomes;
  if (!pasta.listar(nomes)) return false;
  for (const auto& n : nomes) {
    uint32_t id;
    Estado e;
    if (!lerNome(n, id, e)) continue;          // outros arquivos: não são daqui
    _itens.push_back({id, e});
  }
  std::sort(_itens.begin(), _itens.end(), [](const Item& a, const Item& b) { return a.id > b.id; });
  // O mesmo número duas vezes (não devia acontecer): fica o primeiro
  _itens.erase(std::unique(_itens.begin(), _itens.end(), [](const Item& a, const Item& b) { return a.id == b.id; }),
               _itens.end());
  if (!_itens.empty()) _proximo = _itens.front().id + 1;
  _pasta = &pasta;
  return true;
}

size_t Caixa::novas() const {
  size_t n = 0;
  for (const auto& i : _itens) n += i.estado == Nova;
  return n;
}

int Caixa::posicao(uint32_t id) const {
  for (size_t i = 0; i < _itens.size(); i++)
    if (_itens[i].id == id) return (int)i;
  return -1;
}

Caixa::Estado Caixa::estado(uint32_t id) const {
  const int p = posicao(id);
  return p < 0 ? Lida : _itens[p].estado;
}

uint32_t Caixa::primeiraNova() const {
  for (size_t i = _itens.size(); i-- > 0;)
    if (_itens[i].estado == Nova) return _itens[i].id;
  return 0;
}

uint32_t Caixa::guardar(Estado estado, const std::string& numero, const std::string& dataHora,
                        const std::string& texto) {
  if (!_pasta) return 0;
  const uint32_t id = _proximo;
  std::string conteudo;
  conteudo.reserve(numero.size() + dataHora.size() + texto.size() + 2);
  for (char c : numero) if (c != '\n' && c != '\r') conteudo += c;
  conteudo += '\n';
  for (char c : dataHora) if (c != '\n' && c != '\r') conteudo += c;
  conteudo += '\n';
  conteudo += texto;
  if (!_pasta->escrever(nomeDoArquivo(id, estado), conteudo)) return 0;   // nada saiu
  _proximo++;
  _itens.insert(_itens.begin(), {id, estado});
  _abrirEspaco();
  return id;
}

// Passou do máximo: sai a mais antiga já lida ou enviada; depois os
// rascunhos; só se todas forem novas, a nova mais antiga. Nunca a que
// acabou de entrar. Se a memória não deixar apagar, fica (e tenta na próxima).
void Caixa::_abrirEspaco() {
  auto ordem = [](Estado e) { return e == Nova ? 2 : e == Rascunho ? 1 : 0; };
  while (_itens.size() > _maximo) {
    int sai = -1;
    for (int nivel = 0; nivel <= 2 && sai < 0; nivel++)
      for (int i = (int)_itens.size() - 1; i >= 1; i--)
        if (ordem(_itens[i].estado) == nivel) { sai = i; break; }
    if (sai < 1) return;
    const Item it = _itens[sai];
    if (!_pasta->apagar(nomeDoArquivo(it.id, it.estado))) return;
    _esquecer(it.id);
    _itens.erase(_itens.begin() + sai);
  }
}

bool Caixa::ler(uint32_t id, Mensagem& m) {
  const int p = posicao(id);
  if (p < 0 || !_pasta) return false;
  for (const auto& c : _cache)
    if (c.id == id) {
      m = c;
      m.estado = _itens[p].estado;
      return true;
    }
  std::string conteudo;
  if (!_pasta->ler(nomeDoArquivo(id, _itens[p].estado), conteudo)) return false;
  m = Mensagem();
  m.id = id;
  m.estado = _itens[p].estado;
  const size_t a = conteudo.find('\n');
  const size_t b = a == std::string::npos ? std::string::npos : conteudo.find('\n', a + 1);
  if (b == std::string::npos) {           // arquivo estranho: o que tiver vira o texto
    m.texto = conteudo;
  } else {
    m.numero = conteudo.substr(0, a);
    m.dataHora = conteudo.substr(a + 1, b - a - 1);
    m.texto = conteudo.substr(b + 1);
  }
  _cache[_proximoCache] = m;
  _proximoCache = (_proximoCache + 1) % CACHE;
  return true;
}

bool Caixa::mudar(uint32_t id, Estado estado) {
  const int p = posicao(id);
  if (p < 0 || !_pasta) return false;
  if (_itens[p].estado == estado) return true;
  if (!_pasta->renomear(nomeDoArquivo(id, _itens[p].estado), nomeDoArquivo(id, estado))) return false;
  _itens[p].estado = estado;
  for (auto& c : _cache) if (c.id == id) c.estado = estado;
  return true;
}

bool Caixa::apagar(uint32_t id) {
  const int p = posicao(id);
  if (p < 0 || !_pasta) return false;
  if (!_pasta->apagar(nomeDoArquivo(id, _itens[p].estado))) return false;
  _esquecer(id);
  _itens.erase(_itens.begin() + p);
  return true;
}

size_t Caixa::apagarTodas() {
  size_t n = 0;
  while (!_itens.empty()) {
    if (!apagar(_itens.back().id)) break;
    n++;
  }
  return n;
}

void Caixa::_esquecer(uint32_t id) {
  for (auto& c : _cache)
    if (c.id == id) c = Mensagem();
}
