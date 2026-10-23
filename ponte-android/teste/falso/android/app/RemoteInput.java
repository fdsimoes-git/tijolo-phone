package android.app;

import android.content.Intent;
import android.os.Bundle;

/** Falso: guarda a resposta escrita no Intent, como o Android. */
public final class RemoteInput {
  private final String chave;
  private final boolean livre;

  public RemoteInput(String chave, boolean livre) { this.chave = chave; this.livre = livre; }
  public String getResultKey() { return chave; }
  public boolean getAllowFreeFormInput() { return livre; }

  public static void addResultsToIntent(RemoteInput[] r, Intent i, Bundle resultados) {
    i.putExtra("android.remoteinput.results", resultados);
  }
}
