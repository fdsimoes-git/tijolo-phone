// ============================================================
//  caixa_test — a Caixa de mensagens com uma pasta de mentira
//
//  Da raiz do projeto:
//  g++ -std=c++17 -I lib/Caixa test_host/caixa_test.cpp lib/Caixa/Caixa.cpp -o /tmp/caixa && /tmp/caixa
// ============================================================
#include <stdio.h>
#include <map>
#include <string>
#include <vector>
#include "Caixa.h"

static int falhas = 0, verificacoes = 0;
#define CONFERE(cond)                                                       \
  do {                                                                      \
    verificacoes++;                                                         \
    if (!(cond)) { falhas++; printf("  FALHOU (linha %d): %s\n", __LINE__, #cond); } \
  } while (0)

// Pasta na memória, com defeitos sob encomenda
struct PastaFalsa : Pasta {
  std::map<std::string, std::string> arquivos;
  bool falharEscrita = false, falharRenomear = false, falharLista = false, falharApagar = false;
  int leituras = 0;
  bool listar(std::vector<std::string>& nomes) override {
    if (falharLista) return false;
    for (auto& a : arquivos) nomes.push_back(a.first);
    return true;
  }
  bool ler(const std::string& n, std::string& c) override {
    leituras++;
    auto it = arquivos.find(n);
    if (it == arquivos.end()) return false;
    c = it->second;
    return true;
  }
  bool escrever(const std::string& n, const std::string& c) override {
    if (falharEscrita) return false;
    arquivos[n] = c;
    return true;
  }
  bool renomear(const std::string& de, const std::string& para) override {
    if (falharRenomear) return false;
    auto it = arquivos.find(de);
    if (it == arquivos.end()) return false;
    arquivos[para] = it->second;
    arquivos.erase(it);
    return true;
  }
  bool apagar(const std::string& n) override { return !falharApagar && arquivos.erase(n) > 0; }
};

int main() {
  printf("1. nomes dos arquivos\n");
  CONFERE(Caixa::nomeDoArquivo(42, Caixa::Nova) == "0000002a.n");
  uint32_t id;
  Caixa::Estado e;
  CONFERE(Caixa::lerNome("0000002a.n", id, e) && id == 42 && e == Caixa::Nova);
  CONFERE(Caixa::lerNome("ffffffff.e", id, e) && id == 0xffffffffu && e == Caixa::Enviada);
  CONFERE(Caixa::lerNome("00000003.r", id, e) && id == 3 && e == Caixa::Rascunho);
  CONFERE(!Caixa::lerNome("0000002A.n", id, e));        // só minúsculas
  CONFERE(!Caixa::lerNome("0000002a.x", id, e));
  CONFERE(!Caixa::lerNome("0000002a.nn", id, e));
  CONFERE(!Caixa::lerNome("00000000.n", id, e));        // 0 não é número de mensagem
  CONFERE(!Caixa::lerNome("lixo.txt", id, e));

  printf("2. guardar, ler, ordem\n");
  PastaFalsa pasta;
  Caixa cx;
  CONFERE(cx.begin(pasta, 10));
  CONFERE(cx.total() == 0 && cx.novas() == 0 && cx.primeiraNova() == 0);
  const uint32_t a = cx.guardar(Caixa::Nova, "+5511988887777", "26/10/22,18:05:12-12", "Oi! Tudo bem?\nAbraço");
  const uint32_t b = cx.guardar(Caixa::Enviada, "11977776666", "26/10/22,18:06:00", "Tudo ótimo 👍");
  const uint32_t c = cx.guardar(Caixa::Nova, "TIM", "26/10/22,18:07:00-12", "");
  CONFERE(a == 1 && b == 2 && c == 3);
  CONFERE(cx.total() == 3 && cx.novas() == 2);
  CONFERE(cx.itens()[0].id == c && cx.itens()[2].id == a);   // a mais nova primeiro
  CONFERE(cx.primeiraNova() == a);                            // a nova mais antiga
  Caixa::Mensagem m;
  CONFERE(cx.ler(a, m) && m.numero == "+5511988887777" && m.dataHora == "26/10/22,18:05:12-12" &&
          m.texto == "Oi! Tudo bem?\nAbraço" && m.estado == Caixa::Nova);
  CONFERE(cx.ler(c, m) && m.numero == "TIM" && m.texto.empty());
  CONFERE(pasta.arquivos.count("00000001.n") && pasta.arquivos.count("00000002.e"));
  // quebras de linha no número e na data não estragam o arquivo
  const uint32_t d = cx.guardar(Caixa::Lida, "12\n34", "x\ny", "texto");
  CONFERE(cx.ler(d, m) && m.numero == "1234" && m.dataHora == "xy" && m.texto == "texto");
  CONFERE(cx.apagar(d));

  printf("3. mudar de estado e apagar\n");
  CONFERE(cx.mudar(a, Caixa::Lida));
  CONFERE(cx.novas() == 1 && cx.primeiraNova() == c);
  CONFERE(pasta.arquivos.count("00000001.l") && !pasta.arquivos.count("00000001.n"));
  CONFERE(cx.ler(a, m) && m.estado == Caixa::Lida && m.texto == "Oi! Tudo bem?\nAbraço");
  CONFERE(cx.mudar(a, Caixa::Lida));                          // de novo: nada muda
  pasta.falharRenomear = true;
  CONFERE(!cx.mudar(c, Caixa::Lida) && cx.estado(c) == Caixa::Nova);   // falhou: continua nova
  pasta.falharRenomear = false;
  CONFERE(cx.apagar(b) && cx.total() == 2 && cx.posicao(b) < 0);
  CONFERE(!pasta.arquivos.count("00000002.e"));
  CONFERE(!cx.ler(b, m) && !cx.mudar(b, Caixa::Lida) && !cx.apagar(b));

  printf("4. ler de novo a pasta (o Tijolo religou)\n");
  pasta.arquivos["anotacao.txt"] = "não é mensagem";
  Caixa cx2;
  CONFERE(cx2.begin(pasta, 10));
  CONFERE(cx2.total() == 2 && cx2.itens()[0].id == c && cx2.itens()[1].id == a);
  CONFERE(cx2.novas() == 1);
  const uint32_t e2 = cx2.guardar(Caixa::Nova, "1", "", "nova depois de religar");
  CONFERE(e2 == 4);                                           // continua depois da maior que existe
  CONFERE(cx2.ler(a, m) && m.texto == "Oi! Tudo bem?\nAbraço");

  printf("5. cheia: sai a mais antiga já lida\n");
  PastaFalsa p3;
  Caixa cx3;
  cx3.begin(p3, 4);
  const uint32_t n1 = cx3.guardar(Caixa::Nova, "1", "", "n1");
  const uint32_t l2 = cx3.guardar(Caixa::Lida, "2", "", "l2");
  const uint32_t e3 = cx3.guardar(Caixa::Enviada, "3", "", "e3");
  const uint32_t n4 = cx3.guardar(Caixa::Nova, "4", "", "n4");
  const uint32_t n5 = cx3.guardar(Caixa::Nova, "5", "", "n5");    // sai a l2 (a nova n1 fica)
  CONFERE(cx3.total() == 4 && cx3.posicao(l2) < 0 && cx3.posicao(n1) >= 0);
  CONFERE(!p3.arquivos.count(Caixa::nomeDoArquivo(l2, Caixa::Lida)));
  cx3.guardar(Caixa::Nova, "6", "", "n6");                        // sai a e3
  CONFERE(cx3.posicao(e3) < 0 && cx3.total() == 4);
  cx3.guardar(Caixa::Nova, "7", "", "n7");                        // só novas: sai a mais antiga (n1)
  CONFERE(cx3.posicao(n1) < 0 && cx3.posicao(n4) >= 0 && cx3.posicao(n5) >= 0);
  CONFERE(p3.arquivos.size() == 4);
  // os rascunhos saem antes das novas, mas depois das lidas
  PastaFalsa p3b;
  Caixa cx3b;
  cx3b.begin(p3b, 3);
  const uint32_t r1 = cx3b.guardar(Caixa::Rascunho, "", "", "r1");
  const uint32_t nn2 = cx3b.guardar(Caixa::Nova, "2", "", "n2");
  const uint32_t l3 = cx3b.guardar(Caixa::Lida, "3", "", "l3");
  cx3b.guardar(Caixa::Nova, "4", "", "n4");                         // sai a l3, a lida
  CONFERE(cx3b.posicao(l3) < 0 && cx3b.posicao(r1) >= 0);
  cx3b.guardar(Caixa::Nova, "5", "", "n5");                         // sai o rascunho
  CONFERE(cx3b.posicao(r1) < 0 && cx3b.posicao(nn2) >= 0 && cx3b.total() == 3);
  // gravação que falha com a caixa cheia: ninguém sai
  p3b.falharEscrita = true;
  CONFERE(cx3b.guardar(Caixa::Nova, "6", "", "n6") == 0 && cx3b.total() == 3 && cx3b.posicao(nn2) >= 0);
  p3b.falharEscrita = false;
  // apagar que falha: a mensagem fica (passa do máximo), nada se perde da lista
  p3b.falharApagar = true;
  const uint32_t n7 = cx3b.guardar(Caixa::Nova, "7", "", "n7");
  CONFERE(n7 && cx3b.total() == 4 && cx3b.posicao(nn2) >= 0 && p3b.arquivos.size() == 4);
  p3b.falharApagar = false;

  printf("6. falhas da pasta\n");
  PastaFalsa p4;
  Caixa cx4;
  p4.falharLista = true;
  CONFERE(!cx4.begin(p4) && !cx4.pronta());
  CONFERE(cx4.guardar(Caixa::Nova, "1", "", "x") == 0);           // sem pasta, não guarda
  p4.falharLista = false;
  CONFERE(cx4.begin(p4) && cx4.pronta());
  p4.falharEscrita = true;
  CONFERE(cx4.guardar(Caixa::Nova, "1", "", "x") == 0 && cx4.total() == 0);
  p4.falharEscrita = false;
  CONFERE(cx4.guardar(Caixa::Nova, "1", "", "x") == 1);           // o número não foi gasto
  p4.arquivos["00000001.n"] = "só uma linha";                     // arquivo estranho
  Caixa::Mensagem m4;
  CONFERE(cx4.ler(1, m4) && m4.texto == "só uma linha" && m4.numero.empty());

  printf("7. a lista lê do arquivo uma vez só (cache)\n");
  PastaFalsa p5;
  Caixa cx5;
  cx5.begin(p5);
  const uint32_t x = cx5.guardar(Caixa::Nova, "1", "", "x");
  const int antes = p5.leituras;
  for (int i = 0; i < 20; i++) cx5.ler(x, m4);
  CONFERE(p5.leituras == antes + 1);
  cx5.mudar(x, Caixa::Lida);
  CONFERE(cx5.ler(x, m4) && m4.estado == Caixa::Lida && p5.leituras == antes + 1);
  CONFERE(cx5.apagarTodas() == 1 && cx5.total() == 0 && p5.arquivos.empty());
  CONFERE(!cx5.ler(x, m4));                                       // apagada: o cache esqueceu

  printf("\n%d verificações, %d falha(s)\n", verificacoes, falhas);
  return falhas ? 1 : 0;
}
