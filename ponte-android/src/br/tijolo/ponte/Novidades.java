package br.tijolo.ponte;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/**
 * Das mensagens que estão numa notificação do WhatsApp, quais ainda não
 * foram para o Tijolo.
 *
 * A notificação de uma conversa traz as mensagens não lidas e é refeita a
 * cada mensagem nova, repetindo as anteriores. Por isso a ponte guarda as
 * "marcas" (hora + texto) das mensagens que já repassou: as de cada
 * conversa e, à parte, as mais recentes de todas. Só vai o que tem marca
 * nova. Pela hora apenas não dá: uma mensagem que chega atrasada tem hora
 * antiga e se perderia. E a marca não leva o autor: se o contato muda de
 * nome (ou a conversa muda de chave), as mensagens dela não voltam.
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
  /** Quantas marcas guarda o conjunto de todas as conversas */
  public static final int MARCAS_GERAL = 400;

  public static final class Msg {
    public final long hora;
    public final String autor;     // null = você mesmo; "" = alguém sem nome (o Android escondeu)
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
   * @param geral  as marcas recentes de todas as conversas (pode ser null)
   * @param eu     o nome com que o WhatsApp mostra você ("Você"), ou null
   * @param desde  para conversa nunca vista: só as mensagens depois desta hora
   *               (ao ligar a ponte, as antigas não viram uma enxurrada de
   *               SMS); 0 = todas
   */
  public static List<Msg> novas(List<Msg> todas, Set<String> vistas, Set<String> geral, String eu, long desde) {
    final List<Msg> v = new ArrayList<>();
    for (Msg m : todas) {
      if (m.texto == null || m.texto.trim().isEmpty()) continue;
      if (minha(m, eu)) continue;
      final String k = marca(m);
      if (geral != null && geral.contains(k)) continue;
      if (vistas != null ? vistas.contains(k) : m.hora <= desde) continue;
      v.add(m);
    }
    return v;
  }

  /** As marcas de antes + as destas mensagens (as mais novas no fim), no máximo `limite`. */
  public static String juntar(Set<String> antes, Collection<Msg> msgs, int limite) {
    final LinkedHashSet<String> r = antes == null ? new LinkedHashSet<String>() : new LinkedHashSet<>(antes);
    for (Msg m : msgs) {
      final String k = marca(m);
      r.remove(k);                 // a mais nova vai para o fim
      r.add(k);
    }
    return juntarMarcas(r, limite);
  }

  /** O mesmo, com marcas já calculadas */
  public static String juntarMarcas(Collection<String> marcas, int limite) {
    final LinkedHashSet<String> r = new LinkedHashSet<>(marcas);
    final Iterator<String> it = r.iterator();
    for (int sobra = r.size() - limite; sobra > 0; sobra--) {
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

  /**
   * A marca de uma mensagem: dois hashes de 32 bits de hora + texto. Sem o
   * nome do autor ("" = alguém sem nome: o Android 15 escondendo uma
   * mensagem com código), só o texto: o Android põe a hora de agora a cada
   * vez que mostra a notificação escondida, e a mesma mensagem não deve ir
   * de novo.
   */
  public static String marca(Msg m) {
    final String texto = m.texto == null ? "" : m.texto;
    return hash(m.autor != null && m.autor.isEmpty() ? "\u0002" + texto : m.hora + "\u0001" + texto);
  }

  /** Dois hashes de 32 bits do texto */
  public static String hash(String s) {
    int fnv = 0x811C9DC5;
    for (int i = 0; i < s.length(); i++) {
      fnv ^= s.charAt(i);
      fnv *= 0x01000193;
    }
    return Integer.toHexString(s.hashCode()) + Integer.toHexString(fnv);
  }

  /** Escrita por você? (sem autor = você; o nome igual ao seu também) */
  public static boolean minha(Msg m, String eu) {
    if (m.autor == null) return true;
    return eu != null && !eu.trim().isEmpty() && m.autor.trim().equalsIgnoreCase(eu.trim());
  }

  /** O autor da mensagem mais recente que não é você (para o título de uma conversa a dois), ou null */
  public static String outroMaisRecente(List<Msg> todas, String eu) {
    Msg r = null;
    for (Msg m : todas)
      if (!minha(m, eu) && !m.autor.trim().isEmpty() && (r == null || m.hora >= r.hora)) r = m;
    return r == null ? null : r.autor.trim();
  }
}
