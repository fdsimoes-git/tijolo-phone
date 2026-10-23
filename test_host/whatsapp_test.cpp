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
  CONFERE(ehConversa("WA#7 Maria") && ehConversa("WA#7") && ehConversa("WA#123 Família Silva"));
  CONFERE(!ehConversa("WA#0 X") && !ehConversa("WA#1234 X") && !ehConversa("WA#7x") && !ehConversa("WA#"));
  CONFERE(!ehConversa("+5511999998888") && !ehConversa("WA") && !ehConversa("wa#7 Maria"));
  CONFERE(ehEndereco("WA") && ehEndereco("WA#7 Maria") && !ehEndereco("WAX") && !ehEndereco("TIM"));
  CONFERE(conversa("WA#7 Maria") == 7 && conversa("WA#123") == 123 && conversa("WA") == 0 && conversa("190") == 0);
  CONFERE(nome("WA#7 Maria") == "Maria" && nome("WA#7") == "Conversa 7" && nome("WA") == "WhatsApp");
  CONFERE(nome("WA#12 Família Silva") == "Família Silva");
  CONFERE(endereco(7, "Maria") == "WA#7 Maria" && endereco(7, "") == "WA#7" && endereco(7, " A\nB ") == "WA#7 A B");

  printf("2. o que vai para a ponte\n");
  CONFERE(textoParaPonte("WA#7 Maria", "Chego às 8") == "WA#7 Chego às 8");
  CONFERE(textoParaPonte("WA", "WA-") == "WA-");
  CONFERE(textoParaPonte("+5511999998888", "oi") == "oi");
  CONFERE(silenciar(7) == "WA-7" && reativar(12) == "WA+12");

  printf("3. o que vem da ponte\n");
  std::string n, t;
  CONFERE(daPonte("WA#7 Maria\nChego às 8", n, t) && n == "WA#7 Maria" && t == "Chego às 8");
  CONFERE(daPonte("WA#2 Família\nJoão: bom dia\nAna: oi", n, t) && n == "WA#2 Família" && t == "João: bom dia\nAna: oi");
  CONFERE(daPonte("WA#3 \n[foto]", n, t) && n == "WA#3" && t == "[foto]");
  CONFERE(daPonte("WA#4\noi", n, t) && n == "WA#4" && t == "oi");
  CONFERE(daPonte("WA! Teste da ponte", n, t) && n == "WA" && t == "Teste da ponte");
  CONFERE(daPonte("WA? #3 Maria\n#7 Família", n, t) && n == "WA" && t == "Conversas no WhatsApp:\n#3 Maria\n#7 Família");
  CONFERE(!daPonte("Oi, tudo bem?", n, t) && !daPonte("WA#0 X\ny", n, t) && !daPonte("", n, t));
  CONFERE(!daPonte("[…]ia\nChego às 8", n, t));    // a primeira parte se perdeu: SMS comum

  printf("4. o número da ponte\n");
  CONFERE(mesmoNumero("+5511977776666", "11977776666") && mesmoNumero("+55 (11) 97777-6666", "977776666"));
  CONFERE(mesmoNumero("+551133334444", "33334444") && !mesmoNumero("+5511977776666", "+5511977776667"));
  CONFERE(!mesmoNumero("190", "190") && !mesmoNumero("", "11977776666") && !mesmoNumero("TIM", "TIM"));

  printf("\n%d verificações, %d falha(s)\n", verificacoes, falhas);
  return falhas ? 1 : 0;
}
