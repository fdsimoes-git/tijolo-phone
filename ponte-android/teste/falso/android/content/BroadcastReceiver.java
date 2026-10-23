package android.content;

/** Falso (só para os testes no computador). */
public abstract class BroadcastReceiver {
  public static int terminados = 0;
  private int codigo = -1;

  public abstract void onReceive(Context c, Intent i);

  public final int getResultCode() { return codigo; }
  public final void setResultCode(int c) { codigo = c; }

  public final PendingResult goAsync() { return new PendingResult(); }

  public static final class PendingResult {
    public void finish() { terminados++; }
  }
}
