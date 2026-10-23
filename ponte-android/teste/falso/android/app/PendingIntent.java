package android.app;

import android.content.Context;
import android.content.Intent;

import java.util.ArrayList;
import java.util.List;

/** Falso: anota o que foi mandado por ele (a resposta para o WhatsApp). */
public final class PendingIntent {
  public static final int FLAG_UPDATE_CURRENT = 0x08000000;
  public final List<Intent> enviados = new ArrayList<>();
  public boolean cancelado = false;
  public final Intent base;

  public PendingIntent(Intent base) { this.base = base; }

  public static PendingIntent getBroadcast(Context c, int codigo, Intent i, int flags) { return new PendingIntent(i); }

  public void send(Context c, int codigo, Intent i) throws CanceledException {
    if (cancelado) throw new CanceledException();
    enviados.add(i);
  }

  public static class CanceledException extends Exception {}
}
