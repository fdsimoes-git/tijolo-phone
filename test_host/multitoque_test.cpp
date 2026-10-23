// ============================================================
//  multitoque_test — escrever com o teclado de telefone
//
//  Da raiz do projeto:
//  g++ -std=c++17 -I lib/Texto -I lib/Escrita test_host/multitoque_test.cpp lib/Escrita/Multitoque.cpp -o /tmp/mt && /tmp/mt
// ============================================================
#include <stdio.h>
#include <string>
#include "Multitoque.h"

static int falhas = 0, verificacoes = 0;
#define CONFERE(cond)                                                       \
  do {                                                                      \
    verificacoes++;                                                         \
    if (!(cond)) { falhas++; printf("  FALHOU (linha %d): %s\n", __LINE__, #cond); } \
  } while (0)

static Multitoque mt;
static uint32_t agora = 1000;

// Toca as teclas com 200 ms entre elas; um espaço na sequência = espera 1,2 s
static void tocar(const char* s) {
  for (; *s; s++) {
    if (*s == ' ') { agora += 1200; mt.tique(agora); continue; }
    mt.tecla(*s, agora);
    agora += 200;
    mt.tique(agora);
  }
}

int main() {
  printf("1. letras: a mesma tecla seguida muda a letra\n");
  mt.begin();
  CONFERE(mt.nomeModo() == std::string("Abc") && mt.proximaMaiuscula());
  tocar("44");                                  // h
  CONFERE(mt.texto() == "H" && mt.pendente());  // começo de frase: maiúscula
  tocar(" 444");                                // i (depois de esperar)
  CONFERE(mt.texto() == "Hi");
  tocar(" ");
  CONFERE(!mt.pendente());

  printf("2. outra tecla confirma a letra na hora\n");
  mt.begin();
  tocar("2223");                                // c, depois d
  CONFERE(mt.texto() == "Cd");
  tocar("3");                                   // a 3 de novo, logo: d → e
  CONFERE(mt.texto() == "Ce");

  printf("3. acentos do português\n");
  mt.begin();
  mt.definirModo(Multitoque::Modo::Minusculas);
  tocar("2222 2222222 22222222 3333 33333 666666 8888 4444");
  CONFERE(mt.texto() == "áãçéêõúí");
  mt.begin();
  mt.definirModo(Multitoque::Modo::Maiusculas);
  tocar("2222222 22222222 33333 666666");
  CONFERE(mt.texto() == "ÃÇÊÕ");
  tocar("222222222");                           // a volta toda: a b c á à â ã ç 2
  CONFERE(mt.texto() == "ÃÇÊÕ2");
  tocar("2");                                   // e recomeça
  CONFERE(mt.texto() == "ÃÇÊÕA");

  printf("4. Abc: maiúscula só no começo de cada frase\n");
  mt.begin();
  tocar("666 7777 0 4 1 0 88 0 7777");          // "Os g. U s"
  CONFERE(mt.texto() == "Os g. U s");
  mt.begin("Fim.");
  tocar("2");                                   // logo depois do ponto, sem espaço: minúscula
  CONFERE(mt.texto() == "Fim.a");
  mt.begin("Linha\n");
  tocar("2");
  CONFERE(mt.texto() == "Linha\nA");

  printf("5. modos; 123 digita os números\n");
  mt.begin();
  mt.proximoModo();
  CONFERE(mt.nomeModo() == std::string("abc") && !mt.proximaMaiuscula());
  mt.proximoModo();
  CONFERE(mt.nomeModo() == std::string("ABC") && mt.proximaMaiuscula());
  mt.proximoModo();
  CONFERE(mt.nomeModo() == std::string("123"));
  tocar("220137");
  CONFERE(mt.texto() == "220137" && !mt.pendente());
  CONFERE(!mt.tecla('*', agora) && !mt.tecla('#', agora) && mt.texto() == "220137");
  mt.proximoModo();
  CONFERE(mt.nomeModo() == std::string("Abc"));
  mt.begin();
  tocar("2");
  mt.proximoModo();                             // trocar o modo confirma a letra pendente
  CONFERE(!mt.pendente());
  tocar("2");
  CONFERE(mt.texto() == "Aa");

  printf("6. espaço, pontuação e símbolos (tudo no 1)\n");
  mt.begin("Oi");
  tocar("0");
  CONFERE(mt.texto() == "Oi " && mt.pendente());
  tocar("1111");                                // . , ? !
  CONFERE(mt.texto() == "Oi !");
  mt.begin("x");
  tocar("00");
  CONFERE(mt.texto() == "x0");
  tocar(" 111111111111");                       // . , ? ! : ; ' " - ( ) @
  CONFERE(mt.texto() == "x0@");
  tocar(" 111111111111111111111111");           // as 24: volta ao 1
  CONFERE(mt.texto() == "x0@1");
  CONFERE(!mt.tecla('*', agora) && mt.texto() == "x0@1");

  printf("7. segurar a tecla dá o número\n");
  mt.begin("ab");
  mt.tecla('5', agora);
  CONFERE(mt.texto() == "abj");
  CONFERE(mt.segurou('5', agora + 800));
  CONFERE(mt.texto() == "ab5" && !mt.pendente());
  mt.tecla('0', agora + 900);
  CONFERE(mt.segurou('0', agora + 1700) && mt.texto() == "ab50");
  mt.tecla('1', agora + 1800);
  CONFERE(mt.segurou('1', agora + 2600) && mt.texto() == "ab501");
  CONFERE(!mt.segurou('7', agora + 2700));      // nada pendente da tecla 7: nada muda
  agora += 3000;
  mt.begin("x");
  mt.tecla('5', agora);                         // j
  mt.tecla('5', agora + 200);                   // e logo segura o 5: o j fica, o 5 vem depois
  CONFERE(mt.texto() == "xk");
  CONFERE(mt.segurou('5', agora + 1000) && mt.texto() == "xj5");
  mt.tecla('5', agora + 1100);
  mt.tecla('5', agora + 1300);
  mt.tecla('5', agora + 1500);                  // j, k, e segura: "k5"
  CONFERE(mt.segurou('5', agora + 2300) && mt.texto() == "xj5k5");
  agora += 3000;

  printf("8. apagar\n");
  mt.begin("Olá");
  CONFERE(mt.apagar() && mt.texto() == "Ol");   // o "á" (2 bytes) sai inteiro
  mt.begin("😀");
  CONFERE(mt.apagar() && mt.texto().empty());   // o emoji (4 bytes) também
  CONFERE(!mt.apagar());
  mt.begin("a");
  tocar("22");                                  // "ab" com o b pendente
  CONFERE(mt.apagar() && mt.texto() == "a" && !mt.pendente());
  tocar("2");                                   // depois de apagar, começa letra nova
  CONFERE(mt.texto() == "aa");
  mt.limpar();
  CONFERE(mt.vazio());
  mt.begin("Bom dia, tudo bem");
  CONFERE(mt.apagarPalavra() && mt.texto() == "Bom dia, tudo ");
  CONFERE(mt.apagarPalavra() && mt.texto() == "Bom dia, ");
  CONFERE(mt.apagarPalavra() && mt.texto() == "Bom ");
  mt.begin("Oi");
  tocar("0 2");                                 // "Oi a", com o a pendente
  CONFERE(mt.apagarPalavra() && mt.texto() == "Oi " && !mt.pendente());
  mt.begin("Linha\num");
  CONFERE(mt.apagarPalavra() && mt.texto() == "Linha\n");
  CONFERE(mt.apagarPalavra() && mt.texto().empty());
  CONFERE(!mt.apagarPalavra());

  printf("9. limite de tamanho\n");
  mt.begin("", 4);
  tocar("2 2 2 2 2");
  CONFERE(mt.texto() == "Aaaa");
  mt.begin("abc", 4);
  tocar("2222");                                // á à â ã ç (2 bytes) não cabem: pula para o 2
  CONFERE(mt.texto() == "abc2");
  tocar("2");                                   // e dá a volta
  CONFERE(mt.texto() == "abca");
  mt.begin("abcd", 4);
  CONFERE(!mt.tecla('2', agora) && mt.texto() == "abcd" && !mt.pendente());

  printf("10. tique confirma depois de 1 s\n");
  mt.begin();
  mt.tecla('2', 100);
  CONFERE(!mt.tique(1000) && mt.pendente());
  CONFERE(mt.tique(1100) && !mt.pendente());
  mt.tecla('2', 1150);
  CONFERE(mt.texto() == "Aa");                  // nova letra, não "B"

  printf("11. limite de quem usa (como o tamanho em SMS: com acento cabe menos)\n");
  auto cabeSms = [](const std::string& t) {
    bool ascii = true;
    for (unsigned char c : t) ascii = ascii && c < 0x80;
    return ascii ? t.size() <= 6 : t.size() <= 4;
  };
  mt.begin("aaaaa");
  mt.limitar(cabeSms);
  agora += 3000;
  tocar("2222");                                // a b c, e o á não cabe: pula para o 2
  CONFERE(mt.texto() == "aaaaa2" && !mt.recusou());
  tocar("2");                                   // dá a volta: a
  CONFERE(mt.texto() == "aaaaaa");
  tocar(" 2");                                  // outra letra não cabe
  CONFERE(mt.texto() == "aaaaaa" && mt.recusou());
  mt.begin("aaaaa");
  mt.tecla('2', agora);                         // a
  mt.tecla('2', agora + 100);                   // b, e segura: "a2" não cabe, volta o a
  CONFERE(mt.texto() == "aaaaab");
  CONFERE(mt.segurou('2', agora + 900) && mt.texto() == "aaaaaa" && mt.recusou() && !mt.pendente());
  agora += 2000;
  // toque recusado e depois segurado: o número vai depois da letra, se couber
  mt.begin("aaaa");
  mt.limitar([](const std::string& t) { return t.find('0') == std::string::npos && t.size() <= 6; });
  mt.tecla('0', agora);                         // espaço
  CONFERE(!mt.tecla('0', agora + 100) && mt.recusou() && mt.texto() == "aaaa ");   // o "0" não pode
  CONFERE(mt.segurou('0', agora + 900) && mt.texto() == "aaaa " && mt.recusou());
  mt.limitar(nullptr);
  mt.begin("x");
  mt.tecla('5', agora + 1000);
  mt.tecla('5', agora + 1100);                  // k
  CONFERE(mt.tique(agora + 2150) && !mt.pendente());               // o loop parou: a letra ficou...
  CONFERE(mt.segurou('5', agora + 2200) && mt.texto() == "xj5");   // ...e o "segurou" atrasado vale igual
  mt.begin("x");
  mt.tecla('5', agora + 3000);
  CONFERE(mt.apagar() && !mt.segurou('5', agora + 3800) && mt.texto() == "x");   // apagou antes: nada
  agora += 5000;

  printf("\n%d verificações, %d falha(s)\n", verificacoes, falhas);
  return falhas ? 1 : 0;
}
