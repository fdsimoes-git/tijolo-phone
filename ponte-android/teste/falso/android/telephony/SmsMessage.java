package android.telephony;

/** Falso (só para os testes no computador). */
public class SmsMessage {
  private final String de, texto;

  public SmsMessage(String de, String texto) { this.de = de; this.texto = texto; }
  public String getOriginatingAddress() { return de; }
  public String getMessageBody() { return texto; }
}
