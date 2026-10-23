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
 *   "WA#7 Família\nJoão: bom dia"     mensagem (num grupo, cada linha com o autor)
 *   "WA? #3 Maria\n#7 Família"        resposta ao "WA?": as conversas recentes
 *   "WA! texto"                       aviso da ponte (erro, confirmação, teste)
 *
 * Tijolo → Ponte:
 *   "WA#7 texto"                      responder na conversa 7
 *   "WA?"                             pedir a lista de conversas
 *   "WA-" / "WA+"                     pausar / voltar a repassar tudo
 *   "WA-7" / "WA+7"                   silenciar / reativar só a conversa 7
 *
 * O número da conversa (1 a 999) é dado pela ponte e fica guardado nela:
 * o Tijolo só repete o que recebeu.
 */
public final class Protocolo {
  private Protocolo() {}

  public static final int MAX_CONVERSA = 999;

  public static String mensagem(int n, String titulo, String corpo) {
    return "WA#" + n + " " + umaLinha(titulo) + "\n" + corpo;
  }

  public static String aviso(String texto) {
    return "WA! " + texto;
  }

  /** conversas: pares {número, título}, a mais recente primeiro */
  public static String lista(List<String[]> conversas) {
    final StringBuilder b = new StringBuilder("WA?");
    if (conversas.isEmpty()) return b.append(" nenhuma conversa ainda").toString();
    for (int i = 0; i < conversas.size(); i++) {
      b.append(i == 0 ? " " : "\n").append('#').append(conversas.get(i)[0]).append(' ')
          .append(umaLinha(conversas.get(i)[1]));
    }
    return b.toString();
  }

  /** O que o Tijolo pediu. */
  public static final class Pedido {
    public enum Tipo { RESPOSTA, LISTA, PAUSAR, RETOMAR, SILENCIAR, REATIVAR, DESCONHECIDO }
    public final Tipo tipo;
    public final int conversa;     // RESPOSTA, SILENCIAR, REATIVAR
    public final String texto;     // RESPOSTA

    Pedido(Tipo tipo, int conversa, String texto) {
      this.tipo = tipo;
      this.conversa = conversa;
      this.texto = texto;
    }
  }

  private static final Pattern RESPOSTA =
      Pattern.compile("^\\s*WA\\s*#\\s*(\\d{1,3})(?![0-9])[ \\t]?(.*)$", Pattern.DOTALL | Pattern.CASE_INSENSITIVE);
  private static final Pattern SILENCIO =
      Pattern.compile("^\\s*WA\\s*([-+])\\s*#?\\s*(\\d{1,3})(?![0-9])\\s*$", Pattern.CASE_INSENSITIVE);

  private static final Pedido NADA = new Pedido(Pedido.Tipo.DESCONHECIDO, 0, "");

  public static Pedido ler(String sms) {
    if (sms == null) return NADA;
    final String s = sms.trim();
    final String m = s.toUpperCase(Locale.ROOT);
    if (m.equals("WA?")) return new Pedido(Pedido.Tipo.LISTA, 0, "");
    if (m.equals("WA-")) return new Pedido(Pedido.Tipo.PAUSAR, 0, "");
    if (m.equals("WA+")) return new Pedido(Pedido.Tipo.RETOMAR, 0, "");
    final Matcher q = SILENCIO.matcher(s);
    if (q.matches()) {
      final int n = Integer.parseInt(q.group(2));
      if (n < 1 || n > MAX_CONVERSA) return NADA;
      return new Pedido(q.group(1).equals("-") ? Pedido.Tipo.SILENCIAR : Pedido.Tipo.REATIVAR, n, "");
    }
    final Matcher r = RESPOSTA.matcher(s);
    if (r.matches()) {
      final int n = Integer.parseInt(r.group(1));
      final String texto = r.group(2).trim();
      if (n >= 1 && n <= MAX_CONVERSA && !texto.isEmpty()) return new Pedido(Pedido.Tipo.RESPOSTA, n, texto);
    }
    return NADA;
  }

  /** Título numa linha só, sem espaços sobrando. */
  public static String umaLinha(String s) {
    if (s == null) return "";
    return s.replace('\r', ' ').replace('\n', ' ').replaceAll("\\s+", " ").trim();
  }
}
