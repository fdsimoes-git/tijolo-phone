package android.os;

import java.util.ArrayList;
import java.util.List;

/** Falso: um relógio que só anda quando o teste manda, com a fila dos Handlers. */
public final class SystemClock {
  private static long agora = 1_000_000;
  private static long seq = 0;

  static final class Tarefa {
    final long quando, ordem;
    final Runnable r;
    Tarefa(long quando, long ordem, Runnable r) { this.quando = quando; this.ordem = ordem; this.r = r; }
  }

  static final List<Tarefa> fila = new ArrayList<>();

  public static long uptimeMillis() { return agora; }
  public static long elapsedRealtime() { return agora; }

  static synchronized void agendar(Runnable r, long quando) { fila.add(new Tarefa(quando, seq++, r)); }

  static synchronized void tirar(Runnable r) {
    for (int i = fila.size() - 1; i >= 0; i--) if (fila.get(i).r == r) fila.remove(i);
  }

  /** Anda o relógio, rodando o que vencer pelo caminho, na ordem. */
  public static void avancar(long ms) {
    final long fim = agora + ms;
    while (true) {
      Tarefa proxima = null;
      synchronized (SystemClock.class) {
        for (Tarefa t : fila)
          if (t.quando <= fim && (proxima == null || t.quando < proxima.quando
              || (t.quando == proxima.quando && t.ordem < proxima.ordem))) proxima = t;
        if (proxima == null) break;
        fila.remove(proxima);
        if (proxima.quando > agora) agora = proxima.quando;
      }
      proxima.r.run();
    }
    agora = fim;
  }

  public static int pendentes() { return fila.size(); }

  public static void zerar() { fila.clear(); }
}
