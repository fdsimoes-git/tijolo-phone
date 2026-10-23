package android.service.notification;

import android.app.Notification;

/** Falso (só para os testes no computador). */
public class StatusBarNotification {
  private final String pacote, etiqueta;
  private final Notification n;

  public StatusBarNotification(String pacote, String etiqueta, Notification n) {
    this.pacote = pacote;
    this.etiqueta = etiqueta;
    this.n = n;
  }

  public String getPackageName() { return pacote; }
  public String getTag() { return etiqueta; }
  public int getId() { return 1; }
  public Notification getNotification() { return n; }
}
