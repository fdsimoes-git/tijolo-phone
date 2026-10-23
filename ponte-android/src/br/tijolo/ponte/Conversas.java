package br.tijolo.ponte;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Random;

/**
 * Cada conversa do WhatsApp ganha um número (1 a 999), que vai nas SMS
 * ("WA#7xk"). O número fica guardado: a mesma conversa tem sempre o mesmo,
 * mesmo depois de reiniciar o Android.
 *
 * Os números novos andam para a frente (7, 8, 9...) e só voltam a ser
 * usados depois de dar a volta nos 999; a tabela nova (app instalado de
 * novo) começa num número sorteado. Assim, um número velho guardado no
 * Tijolo quase nunca aponta para outra conversa; e, se apontar, o código
 * de duas letras não bate (Protocolo.codigo) e a resposta é recusada.
 * Cheia, a conversa usada há mais tempo cede o número dela.
 *
 * A "chave" identifica a conversa no WhatsApp (o id do contato ou do grupo,
 * quando o Android informa; senão, o título).
 *
 * Só Java comum: os testes rodam no computador.
 */
public final class Conversas {
  /** Onde a tabela fica guardada (no Android, as preferências do app). */
  public interface Guarda {
    String ler();
    void gravar(String conteudo);
  }

  public static final class Entrada {
    public final int numero;
    public final String chave;
    public String titulo;
    public long uso;          // a última vez que passou mensagem (ms)
    long usoGravado;

    Entrada(int numero, String chave, String titulo, long uso) {
      this.numero = numero;
      this.chave = chave;
      this.titulo = titulo;
      this.uso = uso;
      this.usoGravado = uso;
    }
  }

  /** O uso só é regravado quando anda mais que isto (a ordem das antigas não precisa de mais) */
  static final long REGRAVAR_USO_MS = 60 * 60 * 1000;

  private final Guarda guarda;
  private final int maximo;
  private final List<Entrada> entradas = new ArrayList<>();
  private int proximo;

  public Conversas(Guarda guarda) {
    this(guarda, Protocolo.MAX_CONVERSA, 1 + new Random().nextInt(Protocolo.MAX_CONVERSA));
  }

  /** inicio: onde a numeração começa se a tabela estiver vazia */
  public Conversas(Guarda guarda, int maximo, int inicio) {
    this.guarda = guarda;
    this.maximo = maximo;
    this.proximo = Math.max(1, Math.min(maximo, inicio));
    carregar();
  }

  /** O número da conversa (dá um novo se ela ainda não tem). titulo vazio ou null: não mexe no título. */
  public synchronized int numero(String chave, String titulo, long agora) {
    if (chave == null || chave.isEmpty()) chave = titulo == null || titulo.isEmpty() ? "?" : "titulo:" + titulo;
    chave = limpo(chave);
    titulo = titulo == null ? null : limpo(titulo);
    Entrada e = porChave(chave);
    boolean mudou = false;
    if (e == null) {
      int n = livre();
      if (n == 0) {                         // cheia: cede a usada há mais tempo
        Entrada velha = entradas.get(0);
        for (Entrada x : entradas)
          if (x.uso < velha.uso) velha = x;
        entradas.remove(velha);
        n = velha.numero;
      }
      proximo = n % maximo + 1;
      e = new Entrada(n, chave, titulo == null ? "" : titulo, agora);
      entradas.add(e);
      mudou = true;
    } else {
      if (titulo != null && !titulo.isEmpty() && !titulo.equals(e.titulo)) {
        e.titulo = titulo;
        mudou = true;
      }
      if (agora > e.uso) e.uso = agora;
      if (e.uso - e.usoGravado > REGRAVAR_USO_MS) mudou = true;
    }
    if (mudou) gravar();
    return e.numero;
  }

  public synchronized String chave(int numero) {
    final Entrada e = porNumero(numero);
    return e == null ? null : e.chave;
  }

  public synchronized String titulo(int numero) {
    final Entrada e = porNumero(numero);
    return e == null ? null : e.titulo;
  }

  /** As duas letras da conversa (null se o número não existe) */
  public synchronized String codigo(int numero) {
    final Entrada e = porNumero(numero);
    return e == null ? null : Protocolo.codigo(e.chave);
  }

  /** As conversas usadas mais recentemente primeiro. */
  public synchronized List<Entrada> recentes(int quantas) {
    final List<Entrada> v = new ArrayList<>(entradas);
    Collections.sort(v, new Comparator<Entrada>() {
      @Override
      public int compare(Entrada a, Entrada b) {
        return a.uso == b.uso ? a.numero - b.numero : (a.uso > b.uso ? -1 : 1);
      }
    });
    return v.size() > quantas ? new ArrayList<>(v.subList(0, quantas)) : v;
  }

  public synchronized int total() {
    return entradas.size();
  }

  /** A última vez que a conversa teve mensagem (0 se não existe) */
  public synchronized long usoDe(String chave) {
    final Entrada e = porChave(limpo(chave));
    return e == null ? 0 : e.uso;
  }

  public synchronized boolean conhece(String chave) {
    return porChave(limpo(chave)) != null;
  }

  /** A chave da conversa (a usada mais recentemente) com este título, ou null. */
  public synchronized String chavePorTitulo(String titulo) {
    if (titulo == null || titulo.isEmpty()) return null;
    final String t = limpo(titulo);
    Entrada achada = null;
    for (Entrada e : entradas)
      if (e.titulo.equals(t) && (achada == null || e.uso > achada.uso)) achada = e;
    return achada == null ? null : achada.chave;
  }

  private Entrada porChave(String chave) {
    for (Entrada e : entradas)
      if (e.chave.equals(chave)) return e;
    return null;
  }

  private Entrada porNumero(int numero) {
    for (Entrada e : entradas)
      if (e.numero == numero) return e;
    return null;
  }

  // O primeiro livre a partir de `proximo`, dando a volta
  private int livre() {
    if (entradas.size() >= maximo) return 0;
    final boolean[] usado = new boolean[maximo + 1];
    for (Entrada e : entradas) usado[e.numero] = true;
    for (int i = 0; i < maximo; i++) {
      final int n = (proximo - 1 + i) % maximo + 1;
      if (!usado[n]) return n;
    }
    return 0;
  }

  // Primeira linha: "#\t<próximo>"; depois uma conversa por linha: número,
  // chave, título e uso, separados por tab
  private void carregar() {
    entradas.clear();
    final String s = guarda.ler();
    if (s == null) return;
    for (String linha : s.split("\n")) {
      final String[] c = linha.split("\t", -1);
      try {
        if (c.length == 2 && c[0].equals("#")) {
          final int p = Integer.parseInt(c[1]);
          if (p >= 1 && p <= maximo) proximo = p;
          continue;
        }
        if (c.length != 4) continue;
        final int n = Integer.parseInt(c[0]);
        if (n < 1 || n > maximo || porNumero(n) != null || porChave(c[1]) != null) continue;
        entradas.add(new Entrada(n, c[1], c[2], Long.parseLong(c[3])));
      } catch (NumberFormatException ignorada) {
        // linha estragada: fica de fora
      }
    }
  }

  private void gravar() {
    final StringBuilder b = new StringBuilder();
    b.append("#\t").append(proximo).append('\n');
    for (Entrada e : entradas) {
      b.append(e.numero).append('\t').append(limpo(e.chave)).append('\t').append(limpo(e.titulo)).append('\t')
          .append(e.uso).append('\n');
      e.usoGravado = e.uso;
    }
    guarda.gravar(b.toString());
  }

  private static String limpo(String s) {
    return s == null ? "" : s.replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
  }
}
