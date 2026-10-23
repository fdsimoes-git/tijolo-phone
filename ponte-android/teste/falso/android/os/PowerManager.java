package android.os;

/** Falso: a trava de energia só anota se está presa. */
public class PowerManager {
  public static final int PARTIAL_WAKE_LOCK = 1;
  public static int presas = 0;

  public WakeLock newWakeLock(int tipo, String nome) { return new WakeLock(); }

  public static final class WakeLock {
    private boolean presa;
    public void setReferenceCounted(boolean s) {}
    public void acquire(long ms) { if (!presa) presas++; presa = true; }
    public void release() { if (presa) presas--; presa = false; }
    public boolean isHeld() { return presa; }
  }
}
