package android.app;

import android.os.SystemClock;

import java.util.HashMap;
import java.util.Map;

/** Falso: o alarme vira uma tarefa no relógio falso; agendar de novo o mesmo troca a hora. */
public class AlarmManager {
  public static final int ELAPSED_REALTIME_WAKEUP = 2;
  public final Map<String, Long> marcados = new HashMap<>();
  private final Map<String, Runnable> tarefas = new HashMap<>();

  public void set(int tipo, long quando, PendingIntent pi) { marcar(quando, pi); }
  public void setExact(int tipo, long quando, PendingIntent pi) { marcar(quando, pi); }
  public void setExactAndAllowWhileIdle(int tipo, long quando, PendingIntent pi) { marcar(quando, pi); }
  public void setInexactRepeating(int tipo, long primeiro, long intervalo, PendingIntent pi) {
    marcados.put(pi.chave(), primeiro);   // a vigia: os testes chamam quando querem
  }

  private void marcar(long quando, final PendingIntent pi) {
    final String k = pi.chave();
    final Runnable velha = tarefas.remove(k);
    if (velha != null) SystemClock.tirarTarefa(velha);
    final Runnable r = new Runnable() {
      public void run() { tarefas.remove(k); marcados.remove(k); pi.disparar(-1); }
    };
    tarefas.put(k, r);
    marcados.put(k, quando);
    SystemClock.agendarTarefa(r, quando);
  }

  public void zerar() { for (Runnable r : tarefas.values()) SystemClock.tirarTarefa(r); tarefas.clear(); marcados.clear(); }
}
