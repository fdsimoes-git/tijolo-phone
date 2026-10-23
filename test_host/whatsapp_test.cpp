// ============================================================
//  whatsapp_test — o formato das SMS da Ponte do WhatsApp
//
//  Da raiz do projeto:
//  g++ -std=c++17 -I lib/WhatsApp test_host/whatsapp_test.cpp lib/WhatsApp/WhatsApp.cpp -o /tmp/wa && /tmp/wa
// ============================================================
#include <stdio.h>
#include "WhatsApp.h"

static int falhas = 0, verificacoes = 0;
#define CONFERE(cond)                                                       \
  do {                                                                      \
    verificacoes++;                                                         \
    if (!(cond)) { falhas++; printf("  FALHOU (linha %d): %s\n", __LINE__, #cond); } \
  } while (0)

using namespace whatsapp;

int main() {
  printf("1. endereços\n");
  CONFERE(ehConversa("WA#7xk Maria") && ehConversa("WA#7xk") && ehConversa("WA#7") && ehConversa("WA#123ab Família Silva"));
  CONFERE(!ehConversa("WA#0xk X") && !ehConversa("WA#1234 X") && !ehConversa("WA#7x") && !ehConversa("WA#7xkz") &&
          !ehConversa("WA#7XK") && !ehConversa("WA#"));
  CONFERE(!ehConversa("+5511999998888") && !ehConversa("WA") && !ehConversa("wa#7xk Maria"));
  CONFERE(ehEndereco("WA") && ehEndereco("WA#7xk Maria") && !ehEndereco("WAX") && !ehEndereco("TIM"));
  CONFERE(conversa("WA#7xk Maria") == 7 && conversa("WA#123") == 123 && conversa("WA") == 0 && conversa("190") == 0);
  CONFERE(codigo("WA#7xk Maria") == "7xk" && codigo("WA#7 Maria") == "7" && codigo("190") == "");
  CONFERE(nome("WA#7xk Maria") == "Maria" && nome("WA#7xk") == "Conversa 7" && nome("WA") == "WhatsApp");
  CONFERE(nome("WA#12ab Família Silva") == "Família Silva");
  CONFERE(endereco("7xk", "Maria") == "WA#7xk Maria" && endereco("7xk", "") == "WA#7xk" &&
          endereco("7xk", " A\nB ") == "WA#7xk A B");

  printf("2. o que vai para a ponte\n");
  CONFERE(textoParaPonte("WA#7xk Maria", "Chego às 8") == "WA#7xk Chego às 8");
  CONFERE(textoParaPonte("WA", "WA-") == "WA-");
  CONFERE(textoParaPonte("+5511999998888", "oi") == "oi");
  CONFERE(silenciar("WA#7xk Maria") == "WA-7xk" && reativar("WA#12ab Família") == "WA+12ab");

  printf("3. o que vem da ponte\n");
  std::string n, t;
  CONFERE(daPonte("WA#7xk Maria\nChego às 8", n, t) && n == "WA#7xk Maria" && t == "Chego às 8");
  CONFERE(daPonte("WA#2ab Família\nJoão: bom dia\nAna: oi", n, t) && n == "WA#2ab Família" && t == "João: bom dia\nAna: oi");
  CONFERE(daPonte("WA#3cd \n[foto]", n, t) && n == "WA#3cd" && t == "[foto]");
  CONFERE(daPonte("WA#4ef\noi", n, t) && n == "WA#4ef" && t == "oi");
  CONFERE(daPonte("WA! Teste da ponte", n, t) && n == "WA" && t == "Teste da ponte");
  CONFERE(daPonte("WA? #3ab Maria\n#7xk Família", n, t) && n == "WA" && t == "Conversas no WhatsApp:\n#3ab Maria\n#7xk Família");
  CONFERE(!daPonte("Oi, tudo bem?", n, t) && !daPonte("WA#0xk X\ny", n, t) && !daPonte("", n, t));
  CONFERE(!daPonte("[…]ia\nChego às 8", n, t));    // a primeira parte se perdeu: SMS comum

  printf("4. o número da ponte\n");
  CONFERE(mesmoNumero("+5511977776666", "11977776666") && mesmoNumero("+55 (11) 97777-6666", "011977776666"));
  CONFERE(mesmoNumero("+551133334444", "1133334444") && !mesmoNumero("+5511977776666", "+5511977776667"));
  CONFERE(!mesmoNumero("+5521977776666", "11977776666"));   // outro DDD
  CONFERE(!mesmoNumero("977776666", "977776666"));          // sem DDD não vale
  CONFERE(!mesmoNumero("190", "190") && !mesmoNumero("", "11977776666") && !mesmoNumero("TIM", "TIM"));

  printf("\n%d verificações, %d falha(s)\n", verificacoes, falhas);
  return falhas ? 1 : 0;
}
