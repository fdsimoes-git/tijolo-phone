package br.tijolo.ponte;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/**
 * Das mensagens que estão numa notificação do WhatsApp, quais ainda não
 * foram para o Tijolo.
 *
 * A notificação de uma conversa traz as mensagens não lidas e é refeita a
 * cada mensagem nova, repetindo as anteriores. Por isso cada conversa
 * guarda as "marcas" (hora + autor + texto) das mensagens que já viu, e só
 * vai o que tem marca nova. (Pela hora apenas não dá: uma mensagem que
 * chega atrasada tem hora antiga e se perderia.)
 *
 * As mensagens escritas por você (a resposta que veio do Tijolo aparece na
 * notificação) não voltam.
 *
 * Só Java comum: os testes rodam no computador.
 */
public final class Novidades {
  private Novidades() {}

  /** Quantas marcas cada conversa guarda (a notificação mostra no máximo ~25 mensagens) */
  public static final int MARCAS = 40;

  public static final class Msg {
    public final long hora;
    public final String autor;     // null = você mesmo
    public final String texto;

    public Msg(long hora, String autor, String texto) {
      this.hora = hora;
      this.autor = autor;
      this.texto = texto;
    }
  }

  /**
   * As mensagens que ainda não foram.
   *
   * @param vistas as marcas já vistas nesta conversa; null = conversa nunca vista
   * @param eu     o nome com que o WhatsApp mostra você ("Você"), ou null
   * @param desde  para conversa nunca vista: só as mensagens depois desta hora
   *               (ao ligar a ponte, as antigas não viram uma enxurrada de
   *               SMS); 0 = todas
   */
  public static List<Msg> novas(List<Msg> todas, Set<String> vistas, String eu, long desde) {
    final List<Msg> v = new ArrayList<>();
    for (Msg m : todas) {
      if (m.texto == null || m.texto.trim().isEmpty()) continue;
      if (minha(m, eu)) continue;
      if (vistas != null ? vistas.contains(marca(m)) : m.hora <= desde) continue;
      v.add(m);
    }
    return v;
  }

  /** As marcas vistas depois desta notificação (as antigas + as dela, no máximo MARCAS). */
  public static String juntar(Set<String> vistas, List<Msg> todas) {
    final LinkedHashSet<String> r = vistas == null ? new LinkedHashSet<String>() : new LinkedHashSet<>(vistas);
    for (Msg m : todas) {
      final String k = marca(m);
      r.remove(k);                 // a mais nova vai para o fim
      r.add(k);
    }
    final Iterator<String> it = r.iterator();
    for (int sobra = r.size() - MARCAS; sobra > 0; sobra--) {
      it.next();
      it.remove();
    }
    final StringBuilder b = new StringBuilder();
    for (String k : r) {
      if (b.length() > 0) b.append(',');
      b.append(k);
    }
    return b.toString();
  }

  /** null (conversa nunca vista) continua null */
  public static Set<String> lerMarcas(String s) {
    if (s == null) return null;
    final LinkedHashSet<String> r = new LinkedHashSet<>();
    for (String k : s.split(","))
      if (!k.isEmpty()) r.add(k);
    return r;
  }

  /** A marca de uma mensagem: dois hashes de 32 bits de hora + autor + texto. */
  static String marca(Msg m) {
    final String s = m.hora + "\u0001" + (m.autor == null ? "" : m.autor) + "\u0001" + (m.texto == null ? "" : m.texto);
    int fnv = 0x811C9DC5;
    for (int i = 0; i < s.length(); i++) {
      fnv ^= s.charAt(i);
      fnv *= 0x01000193;
    }
    return Integer.toHexString(s.hashCode()) + Integer.toHexString(fnv);
  }

  static boolean minha(Msg m, String eu) {
    if (m.autor == null || m.autor.trim().isEmpty()) return true;
    return eu != null && !eu.trim().isEmpty() && m.autor.trim().equalsIgnoreCase(eu.trim());
  }
}
