package br.tijolo.ponte;

import java.text.Normalizer;

/**
 * Deixa o texto do WhatsApp do jeito que o Tijolo mostra e o SMS leva bem:
 * a tela do Tijolo só tem as letras do português (Latin-1) e alguns sinais;
 * emoji viraria um quadradinho e ainda faria o SMS caber menos.
 *
 *  - os marcadores de mídia do WhatsApp viram palavras: "📷 Foto" → "[foto] Foto"
 *    (sem acento: um texto só com letras sem acento vai mais barato no SMS)
 *  - alguns emoji comuns viram sinais: 😀 → ":)", ❤️ → "<3", 👍 → "(ok)"
 *  - os outros emoji somem (se não sobrar nada, fica "[emoji]")
 *  - letras "enfeitadas" (𝓜𝓪𝓻𝓲𝓪, ｍａｒｉａ) viram as comuns; caracteres de
 *    controle somem; quebras de linha em excesso somem
 *  - o texto é cortado em `maximo` unidades de SMS (UTF-16: uma letra comum
 *    é 1, uma letra rara de fora do BMP é 2)
 *
 * Só Java comum: os testes rodam no computador.
 */
public final class Texto {
  private Texto() {}

  // O que a tela do Tijolo sabe desenhar além do Latin-1 (lib/Tela/Fontes)
  private static final String EXTRAS = "…–—‘’“”•€▲▼◀▶●○←→↑↓✓✗□";

  private static final int[][] CODIGOS = {
      // mídia
      {0x1F4F7, 0}, {0x1F4F8, 0}, {0x1F5BC, 0},      // câmera, foto
      {0x1F3A5, 1}, {0x1F4F9, 1}, {0x1F39E, 1},      // vídeo
      {0x1F3A4, 2}, {0x1F399, 2}, {0x1F3B5, 2},      // áudio
      {0x1F4C4, 3}, {0x1F4CE, 3}, {0x1F4D1, 3},      // documento
      {0x1F4CD, 4}, {0x1F4CC, 4},                    // localização
      {0x1F464, 5}, {0x1F465, 5},                    // contato
      {0x1F4CA, 6},                                  // enquete
      {0x1F4DE, 7}, {0x260E, 7},                     // chamada
      // sinais
      {0x1F600, 10}, {0x1F603, 10}, {0x1F604, 10}, {0x1F601, 10}, {0x1F642, 10}, {0x1F60A, 10}, {0x263A, 10},
      {0x1F606, 11}, {0x1F602, 11}, {0x1F923, 11}, {0x1F605, 11},
      {0x1F609, 12},
      {0x1F622, 13}, {0x1F62D, 13}, {0x2639, 13}, {0x1F641, 13}, {0x1F61E, 13},
      {0x2764, 14}, {0x1F495, 14}, {0x1F496, 14}, {0x1F497, 14}, {0x1F499, 14}, {0x1F49A, 14}, {0x1F49B, 14},
      {0x1F49C, 14}, {0x1F5A4, 14}, {0x1F90D, 14}, {0x2665, 14}, {0x1F60D, 14}, {0x1F618, 14}, {0x1F970, 14},
      {0x1F44D, 15}, {0x1F44C, 15}, {0x2705, 15}, {0x2714, 15},
      {0x1F44E, 16},
      {0x1F64F, 17},
      {0x1F44F, 18},
      {0x1F914, 19},
      {0x1F60E, 20},
      {0x1F62E, 21}, {0x1F631, 21}, {0x1F633, 21},
      {0x1F621, 22}, {0x1F620, 22},
      {0x1F61C, 23}, {0x1F61B, 23}, {0x1F61D, 23},
  };
  private static final String[] PALAVRAS = {
      "[foto]", "[video]", "[audio]", "[documento]", "[local]", "[contato]", "[enquete]", "[chamada]", "", "",
      ":)", ":D", ";)", ":(", "<3", "(ok)", "(nao)", "(obrigado)", "(palmas)", "(hmm)", "B)", ":O", ">:(", ":P",
  };

  private static String palavra(int cp) {
    for (int[] c : CODIGOS)
      if (c[0] == cp) return PALAVRAS[c[1]];
    return null;
  }

  // Emoji e símbolos "de enfeite" (que somem): as faixas principais
  private static boolean ehEmoji(int cp) {
    return (cp >= 0x1F000 && cp <= 0x1FAFF) || (cp >= 0x2600 && cp <= 0x27BF) || (cp >= 0x2B00 && cp <= 0x2BFF) ||
        (cp >= 0x1F1E6 && cp <= 0x1F1FF) || cp == 0x3030 || cp == 0x303D || cp == 0x3297 || cp == 0x3299 ||
        (cp >= 0x2190 && cp <= 0x21FF && EXTRAS.indexOf(cp) < 0);
  }

  // Invisíveis que acompanham os emoji (variação, junção, tom de pele, marcas)
  private static boolean ehInvisivel(int cp) {
    return cp == 0xFE0F || cp == 0xFE0E || cp == 0x200D || cp == 0x200B || cp == 0x200C || cp == 0x2060 ||
        cp == 0xFEFF || (cp >= 0x1F3FB && cp <= 0x1F3FF) || (cp >= 0xE0020 && cp <= 0xE007F) || cp == 0x20E3 ||
        (cp >= 0x202A && cp <= 0x202E) || (cp >= 0x2066 && cp <= 0x2069) || cp == 0x200E || cp == 0x200F;
  }

  // Controle (C0, DEL, C1), menos a quebra de linha
  private static boolean ehControle(int cp) {
    return (cp < 0x20 && cp != '\n') || (cp >= 0x7F && cp < 0xA0);
  }

  private static boolean tijoloMostra(int cp) {
    return (cp >= 0x20 && cp < 0x7F) || (cp >= 0xA0 && cp <= 0xFF) || cp == '\n' || EXTRAS.indexOf(cp) >= 0;
  }

  /** As letras "enfeitadas" e as largas viram as comuns (NFKC); o resto fica igual. */
  private static String desenfeitar(String s) {
    final StringBuilder b = new StringBuilder(s.length());
    for (int i = 0; i < s.length();) {
      final int cp = s.codePointAt(i);
      final int n = Character.charCount(cp);
      if (tijoloMostra(cp) || palavra(cp) != null || ehEmoji(cp) || ehInvisivel(cp)) {
        b.append(s, i, i + n);
      } else {
        final String k = Normalizer.normalize(s.substring(i, i + n), Normalizer.Form.NFKC);
        b.append(k);
      }
      i += n;
    }
    return b.toString();
  }

  /** O texto pronto para o Tijolo, com no máximo `maximo` unidades de SMS. */
  public static String limpar(CharSequence entrada, int maximo) {
    if (entrada == null) return "";
    // NFC junta os acentos soltos ("e" + "´" → "é") antes de tudo
    final String s = desenfeitar(Normalizer.normalize(entrada.toString(), Normalizer.Form.NFC)
        .replace("\r\n", "\n").replace('\r', '\n').replace('\t', ' '));
    final StringBuilder b = new StringBuilder(s.length());
    boolean tinhaAlgo = false;
    for (int i = 0; i < s.length();) {
      final int cp = s.codePointAt(i);
      i += Character.charCount(cp);
      if (ehInvisivel(cp) || ehControle(cp)) continue;
      final String p = palavra(cp);
      if (p != null) {
        tinhaAlgo = true;
        if (p.isEmpty()) continue;
        // "[foto]" no lugar de "📷", com espaço antes e depois se precisar
        if (b.length() > 0 && b.charAt(b.length() - 1) != ' ' && b.charAt(b.length() - 1) != '\n') b.append(' ');
        b.append(p);
        if (i < s.length() && s.charAt(i) != ' ' && s.charAt(i) != '\n') b.append(' ');
        continue;
      }
      if (tijoloMostra(cp)) {
        b.appendCodePoint(cp);
        continue;
      }
      if (ehEmoji(cp)) {
        tinhaAlgo = true;
        continue;                 // enfeite que o Tijolo não mostra
      }
      // Outras letras (grego, cirílico, árabe...): ficam (viram □ na tela,
      // mas não somem — pode ser um nome)
      b.appendCodePoint(cp);
    }
    String r = b.toString();
    r = r.replaceAll("[ ]{2,}", " ");
    r = r.replaceAll(" *\n *", "\n");
    r = r.replaceAll("\n{3,}", "\n\n");
    r = r.trim();
    if (r.isEmpty() && tinhaAlgo) r = "[emoji]";
    return cortar(r, maximo);
  }

  /**
   * Corta em `maximo` unidades UTF-16 (o que o SMS conta), sem partir um
   * caractere, com "..." no fim.
   */
  public static String cortar(String s, int maximo) {
    if (s.length() <= maximo) return s;
    int fim = Math.max(0, maximo - 3);
    if (fim > 0 && Character.isHighSurrogate(s.charAt(fim - 1))) fim--;
    return s.substring(0, fim).trim() + "...";
  }
}
