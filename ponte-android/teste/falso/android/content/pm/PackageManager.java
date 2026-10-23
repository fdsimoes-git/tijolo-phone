package android.content.pm;

import android.content.ComponentName;

import java.util.ArrayList;
import java.util.List;

/** Falso: anota quem foi ligado e desligado (o religar do ouvinte). */
public class PackageManager {
  public static final int COMPONENT_ENABLED_STATE_DEFAULT = 0;
  public static final int COMPONENT_ENABLED_STATE_ENABLED = 1;
  public static final int COMPONENT_ENABLED_STATE_DISABLED = 2;
  public static final int DONT_KILL_APP = 1;
  public static final int PERMISSION_GRANTED = 0;

  public static final List<String> mudancas = new ArrayList<>();

  public void setComponentEnabledSetting(ComponentName c, int estado, int flags) {
    mudancas.add(c.getClassName() + "=" + estado);
  }
}
