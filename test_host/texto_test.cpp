// Testes no computador do lib/Texto e do lib/Toque (sem placa).
//   g++ -std=c++17 -I lib/Texto -I lib/Toque test_host/texto_test.cpp -o /tmp/t && /tmp/t
// Gera também test_host/toque.wav para ouvir o trrrim no computador.
#include <Texto.h>
#include <Toque.h>
#include <cassert>
#include <cstdio>

using namespace texto;

static int falhas = 0;
#define CONFERE(cond) do { if (!(cond)) { printf("FALHOU linha %d: %s\n", __LINE__, #cond); falhas++; } } while (0)

int main() {
  // UTF-8 → UCS2 → UTF-8
  CONFERE(utf8ParaUcs2Hex("Olá") == "004F006C00E1");
  CONFERE(utf8ParaUcs2Hex("+5511") == "002B0035003500310031");
  CONFERE(ucs2HexParaUtf8("004F006C00E1") == "Olá");
  CONFERE(ucs2HexParaUtf8("00E700E3006F") == "ção");
  // emoji (fora do plano básico): par substituto
  CONFERE(utf8ParaUcs2Hex("😀") == "D83DDE00");
  CONFERE(ucs2HexParaUtf8("D83DDE00") == "😀");
  CONFERE(ucs2HexParaUtf8(utf8ParaUcs2Hex("Pão de açúcar 👍 ok")) == "Pão de açúcar 👍 ok");
  // par quebrado vira U+FFFD
  CONFERE(ucs2HexParaUtf8("D83D0041") == "\xEF\xBF\xBD" "A");
  // UTF-8 inválido
  CONFERE(utf8ParaUcs2Hex(std::string("A\xC3", 2)) == "0041FFFD");

  // Reconhecer campos em UCS2
  CONFERE(pareceUcs2Hex("002B00350035", true));
  CONFERE(!pareceUcs2Hex("11987654321", true));     // número comum
  CONFERE(!pareceUcs2Hex("119876543210", true));    // 12 dígitos: múltiplo de 4, mas não "00.."
  CONFERE(!pareceUcs2Hex("REC UNREAD", true));
  CONFERE(decodificarCampo("002B0035003500310031", true) == "+5511");
  CONFERE(decodificarCampo("+5511999998888", true) == "+5511999998888");

  // Codificação do SMS
  CONFERE(ehGsmSeguro("Oi, tudo bem? Chego as 8h."));
  CONFERE(!ehGsmSeguro("Oi, tudo bem? Chego às 8h."));   // à
  CONFERE(!ehGsmSeguro("preço"));
  CONFERE(tamanhoSms("açaí", false) == 4);
  CONFERE(tamanhoSms("a😀", false) == 3);
  CONFERE(tamanhoSms("abc", true) == 3);

  // Divisão sem quebrar caracteres
  {
    std::string t;
    for (int i = 0; i < 100; i++) t += "ã";           // 100 unidades UCS2
    auto p = dividir(t, 67, false);
    CONFERE(p.size() == 2);
    CONFERE(tamanhoSms(p[0], false) == 67);
    CONFERE(tamanhoSms(p[1], false) == 33);
    std::string emojis;
    for (int i = 0; i < 40; i++) emojis += "😀";       // 80 unidades
    auto q = dividir(emojis, 67, false);
    CONFERE(q.size() == 2);
    CONFERE(tamanhoSms(q[0], false) == 66);           // 33 emoji; o 34º não cabe
    CONFERE(q[0] + q[1] == emojis);
    auto r = dividir(std::string(200, 'a'), 153, true);
    CONFERE(r.size() == 2 && r[0].size() == 153 && r[1].size() == 47);
  }

  // Campos com aspas e vírgulas
  {
    auto f = campos("\"REC UNREAD\",\"+5511999998888\",\"\",\"26/10/21,18:05:12-12\"");
    CONFERE(f.size() == 4);
    CONFERE(f[0] == "REC UNREAD" && f[1] == "+5511999998888" && f[2].empty() && f[3] == "26/10/21,18:05:12-12");
    auto g = campos("1,1,4,0,0,\"+5511999998888\",145");
    CONFERE(g.size() == 7 && g[2] == "4" && g[5] == "+5511999998888");
    auto h = campos(" 0,\"Saldo: R$ 10,00\",15");
    CONFERE(h.size() == 3 && h[1] == "Saldo: R$ 10,00");
  }

  // Números
  CONFERE(numeroValido("+5511999998888", false));
  CONFERE(numeroValido("190", false));
  CONFERE(!numeroValido("*222#", false));
  CONFERE(numeroValido("*222#", true));
  CONFERE(!numeroValido("11 9999", true));
  CONFERE(!numeroValido("55+11", true));

  // Toque: cabeçalho WAV e tamanho
  {
    auto w = toque::gerarWav(0.5f);
    CONFERE(w.size() == 44 + (size_t)((0.45 + 0.20 + 0.45 + 1.30) * 8000) * 2);
    CONFERE(w[0] == 'R' && w[8] == 'W' && w[36] == 'd');
    FILE* fp = fopen("test_host/toque.wav", "wb");
    if (fp) { fwrite(w.data(), 1, w.size(), fp); fclose(fp); }
    printf("toque.wav: %zu bytes\n", w.size());
  }

  printf(falhas ? "%d falha(s)\n" : "Todos os testes passaram.\n", falhas);
  return falhas ? 1 : 0;
}
