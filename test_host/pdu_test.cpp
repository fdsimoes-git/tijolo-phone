// Testes do lib/Pdu no computador.
//   g++ -std=c++17 -I lib/Texto -I lib/Pdu test_host/pdu_test.cpp -o /tmp/pdu && /tmp/pdu [vetores.tsv saida.tsv]
//
// Sem argumentos: testes fixos. Com argumentos (usados no desenvolvimento):
// confere a leitura contra PDUs gerados pela biblioteca Python independente
// python-gsmmodem-new e grava os nossos PDUs para ela conferir de volta.
#include <Pdu.h>
#include <cstdio>
#include <fstream>
#include <map>
#include <random>
#include <sstream>

using namespace pdu;
static int falhas = 0, verificacoes = 0;
#define CONFERE(c) do { verificacoes++; if (!(c)) { printf("  FALHOU (linha %d): %s\n", __LINE__, #c); falhas++; } } while (0)

static std::string deHex(const std::string& h) { std::vector<uint8_t> b; hexParaBytes(h, b); return std::string(b.begin(), b.end()); }

int main(int argc, char** argv) {
  // ----- exemplo clássico (SMS-DELIVER "hellohello") -----
  {
    Mensagem m;
    CONFERE(decodificar("07917283010010F5040BC87238880900F10000993092516195800AE8329BFD4697D9EC37", m));
    CONFERE(m.tipo == 0);
    CONFERE(m.numero == "27838890001");
    CONFERE(m.texto == "hellohello");
    CONFERE(m.dataHora == "99/03/29,15:16:59+08");
    CONFERE(m.total == 1);
  }
  // ----- nosso PDU de envio do exemplo clássico -----
  {
    auto v = codificar("+46708251358", "hellohello", 1, 0xAA);
    CONFERE(v.size() == 1);
    CONFERE(v[0].hex == "0011000B916407281553F80000AA0AE8329BFD4697D9EC37");
    CONFERE(v[0].tamanho == 23);
  }
  // ----- ida e volta: acentos, emoji, extensão GSM, longo -----
  {
    const char* textos[] = {"Olá 👍 ação", "[x] {y} €5 ~^|\\", "Tudo certo, até já", ""};
    for (auto t : textos) {
      auto v = codificar("+5511999998888", t, 9);
      std::string junto;
      for (auto& e : v) { Mensagem m; CONFERE(decodificar(e.hex, m)); CONFERE(m.tipo == 1); junto += m.texto; CONFERE(m.numero == "+5511999998888"); }
      CONFERE(junto == t);
    }
    std::string longo;
    for (int i = 0; i < 30; i++) longo += "Mensagem longa com acentuação 😀 ";
    auto v = codificar("11999998888", longo, 200);
    CONFERE(v.size() > 1);
    Montador mo;
    Montador::Pronta pr;
    bool completou = false;
    for (int i = (int)v.size() - 1; i >= 0; i--) {   // chegam fora de ordem
      Mensagem m;
      CONFERE(decodificar(v[i].hex, m));
      CONFERE(m.ref == 200 && m.total == (int)v.size() && m.parte == i + 1);
      m.tipo = 0;
      completou = mo.adicionar(m, 10 + i, 1000, pr);
      CONFERE(completou == (i == 0));
    }
    CONFERE(completou && pr.msg.texto == longo && !pr.incompleta && pr.indices.size() == v.size());
    // parte perdida: entrega o que chegou depois do prazo
    Montador mo2;
    Mensagem a; decodificar(v[0].hex, a);
    CONFERE(!mo2.adicionar(a, 1, 0, pr));
    CONFERE(!mo2.expirada(1000, 180000, pr));
    CONFERE(mo2.expirada(200000, 180000, pr));
    CONFERE(pr.incompleta && pr.msg.texto.find("[…]") != std::string::npos);
  }
  // ----- limites -----
  {
    CONFERE(codificar("+55 11", "oi", 1).empty());
    std::string e;
    CONFERE(codificar("abc", "oi", 1, 0xA7, &e).empty() && e == "número inválido");
    CONFERE(codificar("123", std::string(160, 'a'), 1).size() == 1);
    CONFERE(codificar("123", std::string(161, 'a'), 1).size() == 2);
    std::string u70, u71;
    for (int i = 0; i < 70; i++) u70 += "ã";
    u71 = u70 + "ã";
    CONFERE(codificar("123", u70, 1).size() == 1);
    CONFERE(codificar("123", u71, 1).size() == 2);
  }
  // ----- lixo não pode travar -----
  {
    std::mt19937 rng(1);
    for (int i = 0; i < 100000; i++) {
      std::string h;
      int n = rng() % 80;
      for (int k = 0; k < n; k++) h += "0123456789ABCDEF"[rng() % 16];
      Mensagem m;
      decodificar(h, m);
    }
  }

  // ----- vetores da biblioteca independente -----
  if (argc >= 3) {
    std::ifstream f(argv[1]);
    std::string linha;
    std::map<std::string, std::string> juntos;     // grupo → texto acumulado
    std::map<std::string, std::string> esperado;
    int n = 0;
    while (std::getline(f, linha)) {
      std::stringstream ss(linha);
      std::string tipo, hex, txt, num, sref, stotal, sparte;
      std::getline(ss, tipo, '\t'); std::getline(ss, hex, '\t'); std::getline(ss, txt, '\t');
      std::getline(ss, num, '\t'); std::getline(ss, sref, '\t'); std::getline(ss, stotal, '\t'); std::getline(ss, sparte, '\t');
      Mensagem m;
      bool ok = decodificar(hex, m);
      CONFERE(ok);
      if (!ok) continue;
      CONFERE(m.tipo == (tipo == "SUBMIT" ? 1 : 0));
      if (m.numero != num) printf("  numero: %s != %s\n", m.numero.c_str(), num.c_str());
      CONFERE(m.numero == num);
      CONFERE(m.total == atoi(stotal.c_str()) && m.parte == atoi(sparte.c_str()));
      if (atoi(stotal.c_str()) > 1) CONFERE(m.ref == atoi(sref.c_str()));
      std::string chave = tipo + num + deHex(txt);
      juntos[chave] += m.texto;
      esperado[chave] = deHex(txt);
      if (tipo == "DELIVER") CONFERE(m.dataHora == "26/10/12,18:05:05+04");
      n++;
    }
    for (auto& kv : juntos) {
      if (kv.second != esperado[kv.first]) printf("  texto: [%s] != [%s]\n", kv.second.c_str(), esperado[kv.first].c_str());
      CONFERE(kv.second == esperado[kv.first]);
    }
    printf("vetores lidos: %d PDUs\n", n);

    // os nossos, para o Python conferir
    std::ofstream o(argv[2]);
    const char* textos[] = {"hellohello", "Já cheguei, até já!", "[x] {y} €5 ~^|\\", "Acento: ação",
                            "", "Fim de linha\ncom quebra"};
    std::vector<std::string> extras = {std::string(160, 'a'), std::string(161, 'b'), std::string(459, 'x')};
    std::string ac; for (int i = 0; i < 12; i++) ac += "Longo com acento ç "; extras.push_back(ac);
    std::string eu; for (int i = 0; i < 81; i++) eu += "€"; extras.push_back(eu);
    std::vector<std::string> todos(textos, textos + 6);
    todos.insert(todos.end(), extras.begin(), extras.end());
    uint8_t ref = 50;
    for (auto& num : {std::string("+5511999998888"), std::string("11999998888")})
      for (auto& t : todos) {
        auto v = codificar(num, t, ref++);
        for (auto& e : v) o << e.hex << '\t' << e.tamanho << '\t' << bytesParaHex(std::vector<uint8_t>(t.begin(), t.end()))
                           << '\t' << num << '\t' << v.size() << '\n';
      }
  }

  printf("%d verificações, %d falha(s)\n", verificacoes, falhas);
  return falhas ? 1 : 0;
}
