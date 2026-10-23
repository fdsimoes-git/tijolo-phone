package android.os;

/** Falso: uma linha só (a "principal"). */
public final class Looper {
  private static final Looper PRINCIPAL = new Looper();
  public static Looper getMainLooper() { return PRINCIPAL; }
  public static Looper myLooper() { return PRINCIPAL; }
}
