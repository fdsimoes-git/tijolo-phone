package br.tijolo.ponte;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * Cada conversa do WhatsApp ganha um número pequeno (1 a 99), que vai nas
 * SMS ("WA#7"). O número fica guardado: a mesma conversa tem sempre o
 * mesmo, mesmo depois de reiniciar o Android. Cheia, a conversa usada há
 * mais tempo cede o número dela.
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

    Entrada(int numero, String chave, String titulo, long uso) {
      this.numero = numero;
      this.chave = chave;
      this.titulo = titulo;
      this.uso = uso;
    }
  }

  private final Guarda guarda;
  private final int maximo;
  private final List<Entrada> entradas = new ArrayList<>();

  public Conversas(Guarda guarda) {
    this(guarda, Protocolo.MAX_CONVERSA);
  }

  public Conversas(Guarda guarda, int maximo) {
    this.guarda = guarda;
    this.maximo = maximo;
    carregar();
  }

  /** O número da conversa (dá um novo se ela ainda não tem). */
  public synchronized int numero(String chave, String titulo, long agora) {
    if (chave == null || chave.isEmpty()) chave = titulo == null ? "?" : titulo;
    chave = limpo(chave);
    titulo = titulo == null ? null : limpo(titulo);
    Entrada e = porChave(chave);
    if (e == null) {
      int n = livre();
      if (n == 0) {                         // cheia: cede a usada há mais tempo
        Entrada velha = entradas.get(0);
        for (Entrada x : entradas)
          if (x.uso < velha.uso) velha = x;
        entradas.remove(velha);
        n = velha.numero;
      }
      e = new Entrada(n, chave, titulo == null ? "" : titulo, agora);
      entradas.add(e);
    } else {
      if (titulo != null && !titulo.isEmpty()) e.titulo = titulo;
      e.uso = Math.max(e.uso, agora);
    }
    gravar();
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

  private int livre() {
    for (int n = 1; n <= maximo; n++)
      if (porNumero(n) == null) return n;
    return 0;
  }

  // Uma conversa por linha: número, chave, título e uso, separados por tab
  private void carregar() {
    entradas.clear();
    final String s = guarda.ler();
    if (s == null) return;
    for (String linha : s.split("\n")) {
      final String[] c = linha.split("\t", -1);
      if (c.length != 4) continue;
      try {
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
    for (Entrada e : entradas) {
      b.append(e.numero).append('\t').append(limpo(e.chave)).append('\t').append(limpo(e.titulo)).append('\t')
          .append(e.uso).append('\n');
    }
    guarda.gravar(b.toString());
  }

  private static String limpo(String s) {
    return s == null ? "" : s.replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
  }
}
