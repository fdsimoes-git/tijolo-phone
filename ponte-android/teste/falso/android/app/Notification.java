package android.app;

import android.os.Bundle;

import java.util.ArrayList;
import java.util.List;

/** Falso (só para os testes no computador). */
public class Notification {
  public static final int FLAG_ONGOING_EVENT = 0x00000002;
  public static final int FLAG_GROUP_SUMMARY = 0x00000200;

  public Bundle extras = new Bundle();
  public int flags;
  public Action[] actions;
  public String category;
  public long when;
  public String atalho;               // o getShortcutId() do Android 8+

  public String getShortcutId() { return atalho; }

  public static class Action {
    public PendingIntent actionIntent;
    private final RemoteInput[] entradas;

    public Action(PendingIntent pi, RemoteInput... entradas) { this.actionIntent = pi; this.entradas = entradas; }
    public RemoteInput[] getRemoteInputs() { return entradas; }
  }

  public static final class WearableExtender {
    public WearableExtender(Notification n) {}
    public List<Action> getActions() { return new ArrayList<>(); }
  }
}
