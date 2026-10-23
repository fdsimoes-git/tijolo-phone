package android.app;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import java.util.ArrayList;
import java.util.List;

/** Falso: anota o que foi mandado por ele; os de broadcast entregam ao receptor do app. */
public final class PendingIntent {
  public static final int FLAG_UPDATE_CURRENT = 0x08000000;
  public final List<Intent> enviados = new ArrayList<>();
  public boolean cancelado = false;
  public final Intent base;
  public final Context ctx;
  public final int codigo;

  public PendingIntent(Intent base) { this(null, 0, base); }
  PendingIntent(Context c, int codigo, Intent base) { this.ctx = c; this.codigo = codigo; this.base = base; }

  public static PendingIntent getBroadcast(Context c, int codigo, Intent i, int flags) { return new PendingIntent(c, codigo, i); }

  public void send(Context c, int codigo, Intent i) throws CanceledException {
    if (cancelado) throw new CanceledException();
    enviados.add(i);
  }

  /** Entrega o broadcast ao receptor (a classe do Intent), com este resultado */
  public void disparar(int resultado) {
    try {
      final BroadcastReceiver r = (BroadcastReceiver) base.classe.getDeclaredConstructor().newInstance();
      r.setResultCode(resultado);
      r.onReceive(ctx, base);
    } catch (RuntimeException e) {
      throw e;
    } catch (Exception e) {
      throw new RuntimeException(e);
    }
  }

  /** O "mesmo" PendingIntent para o AlarmManager: a ação e o código */
  public String chave() { return base.getAction() + "#" + codigo; }

  public static class CanceledException extends Exception {}
}
