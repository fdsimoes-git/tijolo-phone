package android.service.notification;

/** Falso: o teste chama os métodos e diz quais notificações estão abertas. */
public abstract class NotificationListenerService extends android.content.Context {
  public static StatusBarNotification[] abertas = new StatusBarNotification[0];
  public static int religacoes = 0;

  public static void requestRebind(android.content.ComponentName c) { religacoes++; }

  public void onCreate() {}
  public void onDestroy() {}
  public void onListenerConnected() {}
  public void onNotificationPosted(StatusBarNotification sbn) {}
  public void onNotificationRemoved(StatusBarNotification sbn) {}
  public StatusBarNotification[] getActiveNotifications() { return abertas; }
}
