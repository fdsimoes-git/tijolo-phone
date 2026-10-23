package android.telephony;

import android.app.PendingIntent;

import java.util.ArrayList;
import java.util.List;

/** Falso: anota as SMS mandadas (para quem, o texto inteiro, quantas partes) e guarda os avisos de envio. */
public final class SmsManager {
  private static final SmsManager UNICO = new SmsManager();
  public static final List<String[]> enviadas = new ArrayList<>();
  public static final List<PendingIntent> avisos = new ArrayList<>();

  public static SmsManager getDefault() { return UNICO; }

  /** Como o Android: sem acento, 160 letras (153 por parte); com acento, 70 (67 por parte) */
  public ArrayList<String> divideMessage(String texto) {
    boolean simples = true;
    for (int i = 0; i < texto.length(); i++) simples = simples && texto.charAt(i) < 128;
    final int uma = simples ? 160 : 70, parte = simples ? 153 : 67;
    final ArrayList<String> v = new ArrayList<>();
    if (texto.length() <= uma) { v.add(texto); return v; }
    for (int i = 0; i < texto.length(); i += parte) v.add(texto.substring(i, Math.min(texto.length(), i + parte)));
    return v;
  }

  public void sendMultipartTextMessage(String para, String centro, ArrayList<String> partes,
                                       ArrayList<PendingIntent> enviadas, ArrayList<PendingIntent> entregues) {
    final StringBuilder b = new StringBuilder();
    for (String p : partes) b.append(p);
    SmsManager.enviadas.add(new String[] {para, b.toString(), String.valueOf(partes.size())});
    if (enviadas != null) avisos.addAll(enviadas);
  }

  /** O "rádio" responde todas as partes pendentes com este resultado (-1 = deu certo) */
  public static void responder(int resultado) {
    final List<PendingIntent> v = new ArrayList<>(avisos);
    avisos.clear();
    for (PendingIntent pi : v) pi.disparar(resultado);
  }
}
