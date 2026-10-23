package android.os;

/** Falso: entrega as tarefas pelo relógio falso (SystemClock.avancar). */
public class Handler {
  public Handler(Looper l) {}
  public boolean post(Runnable r) { SystemClock.agendar(r, SystemClock.uptimeMillis()); return true; }
  public boolean postDelayed(Runnable r, long ms) { SystemClock.agendar(r, SystemClock.uptimeMillis() + ms); return true; }
  public boolean postAtTime(Runnable r, long quando) { SystemClock.agendar(r, quando); return true; }
  public void removeCallbacks(Runnable r) { SystemClock.tirar(r); }
}
