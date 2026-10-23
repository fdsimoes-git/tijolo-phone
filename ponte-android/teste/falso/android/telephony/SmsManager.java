package android.telephony;

import android.app.PendingIntent;

import java.util.ArrayList;
import java.util.List;

/** Falso: anota as SMS mandadas (para quem, o texto inteiro, quantas partes). */
public final class SmsManager {
  private static final SmsManager UNICO = new SmsManager();
  public static final List<String[]> enviadas = new ArrayList<>();

  public static SmsManager getDefault() { return UNICO; }

  public ArrayList<String> divideMessage(String texto) {
    final ArrayList<String> v = new ArrayList<>();
    if (texto.length() <= 70) { v.add(texto); return v; }
    for (int i = 0; i < texto.length(); i += 67) v.add(texto.substring(i, Math.min(texto.length(), i + 67)));
    return v;
  }

  public void sendMultipartTextMessage(String para, String centro, ArrayList<String> partes,
                                       ArrayList<PendingIntent> enviadas, ArrayList<PendingIntent> entregues) {
    final StringBuilder b = new StringBuilder();
    for (String p : partes) b.append(p);
    SmsManager.enviadas.add(new String[] {para, b.toString(), String.valueOf(partes.size())});
  }
}
