package br.tijolo.ponte;

import java.util.List;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * O formato das SMS entre a ponte e o Tijolo. Só texto, nada de Android:
 * os testes rodam no computador (teste/).
 *
 * Ponte → Tijolo:
 *   "WA#7xk Família\nJoão: bom dia"   mensagem (num grupo, cada linha com o autor)
 *   "WA? #3ab Maria\n#7xk Família"    resposta ao "WA?": as conversas recentes
 *   "WA! texto"                       aviso da ponte (erro, confirmação, teste)
 *
 * Tijolo → Ponte:
 *   "WA#7xk texto"                    responder na conversa 7
 *   "WA?"                             pedir a lista de conversas
 *   "WA-" / "WA+"                     pausar / voltar a repassar tudo
 *   "WA-7xk" / "WA+7xk"               silenciar / reativar só a conversa 7
 *
 * O número da conversa (1 a 999) é dado pela ponte e fica guardado nela;
 * as duas letras depois dele são um código da conversa (tirado do id dela
 * no WhatsApp). Se o número passar a ser de outra conversa (a ponte
 * reinstalada, ou mais de 999 conversas), o código não bate e a resposta é
 * recusada, em vez de ir para a pessoa errada.
 */
public final class Protocolo {
  private Protocolo() {}

  public static final int MAX_CONVERSA = 999;

  /** Duas letras (a..z) que identificam a conversa: o mesmo id dá sempre as mesmas. */
  public static String codigo(String chave) {
    int h = 0x811C9DC5;
    final String s = chave == null ? "" : chave;
    for (int i = 0; i < s.length(); i++) {
      h ^= s.charAt(i);
      h *= 0x01000193;
    }
    final int n = (h & 0x7FFFFFFF) % (26 * 26);
    return "" + (char) ('a' + n / 26) + (char) ('a' + n % 26);
  }

  public static String mensagem(int n, String codigo, String titulo, String corpo) {
    return "WA#" + n + codigo + " " + umaLinha(titulo) + "\n" + corpo;
  }

  public static String aviso(String texto) {
    return "WA! " + texto;
  }

  /** conversas: trios {número, código, título}, a mais recente primeiro */
  public static String lista(List<String[]> conversas) {
    final StringBuilder b = new StringBuilder("WA?");
    if (conversas.isEmpty()) return b.append(" nenhuma conversa ainda").toString();
    for (int i = 0; i < conversas.size(); i++) {
      final String[] c = conversas.get(i);
      b.append(i == 0 ? " " : "\n").append('#').append(c[0]).append(c[1]).append(' ').append(umaLinha(c[2]));
    }
    return b.toString();
  }

  /** O que o Tijolo pediu. */
  public static final class Pedido {
    public enum Tipo { RESPOSTA, LISTA, PAUSAR, RETOMAR, SILENCIAR, REATIVAR, DESCONHECIDO }
    public final Tipo tipo;
    public final int conversa;     // RESPOSTA, SILENCIAR, REATIVAR
    public final String codigo;    // as duas letras (null se não vieram)
    public final String texto;     // RESPOSTA

    Pedido(Tipo tipo, int conversa, String codigo, String texto) {
      this.tipo = tipo;
      this.conversa = conversa;
      this.codigo = codigo;
      this.texto = texto;
    }
  }

  private static final Pattern RESPOSTA = Pattern.compile(
      "^\\s*WA\\s*#\\s*(\\d{1,3})([A-Za-z]{2})?(?![0-9A-Za-z])[ \\t]?(.*)$", Pattern.DOTALL);
  private static final Pattern SILENCIO = Pattern.compile(
      "^\\s*WA\\s*([-+])\\s*#?\\s*(\\d{1,3})([A-Za-z]{2})?(?![0-9A-Za-z])\\s*$");

  private static final Pedido NADA = new Pedido(Pedido.Tipo.DESCONHECIDO, 0, null, "");

  private static String minusculas(String s) {
    return s == null ? null : s.toLowerCase(Locale.ROOT);
  }

  public static Pedido ler(String sms) {
    if (sms == null) return NADA;
    final String s = sms.trim();
    if (s.length() < 3 || !s.substring(0, 2).equalsIgnoreCase("WA")) return NADA;
    final String m = s.toUpperCase(Locale.ROOT);
    if (m.equals("WA?")) return new Pedido(Pedido.Tipo.LISTA, 0, null, "");
    if (m.equals("WA-")) return new Pedido(Pedido.Tipo.PAUSAR, 0, null, "");
    if (m.equals("WA+")) return new Pedido(Pedido.Tipo.RETOMAR, 0, null, "");
    final String semWa = "WA" + s.substring(2);           // "wa#7" também vale
    final Matcher q = SILENCIO.matcher(semWa);
    if (q.matches()) {
      final int n = Integer.parseInt(q.group(2));
      if (n < 1 || n > MAX_CONVERSA) return NADA;
      return new Pedido(q.group(1).equals("-") ? Pedido.Tipo.SILENCIAR : Pedido.Tipo.REATIVAR, n,
          minusculas(q.group(3)), "");
    }
    final Matcher r = RESPOSTA.matcher(semWa);
    if (r.matches()) {
      final int n = Integer.parseInt(r.group(1));
      final String texto = r.group(3).trim();
      if (n >= 1 && n <= MAX_CONVERSA && !texto.isEmpty())
        return new Pedido(Pedido.Tipo.RESPOSTA, n, minusculas(r.group(2)), texto);
    }
    return NADA;
  }

  /** Título numa linha só, sem espaços sobrando. */
  public static String umaLinha(String s) {
    if (s == null) return "";
    return s.replace('\r', ' ').replace('\n', ' ').replaceAll("\\s+", " ").trim();
  }
}
