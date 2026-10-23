package br.tijolo.ponte;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.telephony.SmsManager;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Locale;

/** O que as partes do app usam em comum: as opções, o registro e mandar SMS. */
public final class Ponte {
  private Ponte() {}

  public static final String[] PACOTES_WHATSAPP = {"com.whatsapp", "com.whatsapp.w4b"};

  /**
   * Texto máximo de uma SMS repassada (o resto vira "..."). Com acento, o SMS
   * vai em partes de 67 letras: 330 + "WA#123 " + o título cabem em 6 partes
   * (cada parte é cobrada como uma SMS).
   */
  public static final int MAXIMO_TEXTO = 330;
  /** Título máximo de uma conversa */
  public static final int MAXIMO_TITULO = 30;

  /**
   * Mensagens seguidas da mesma conversa vão juntas numa SMS só: a SMS sai
   * quando a conversa fica ESPERA sem mensagem nova, ou ESPERA_MAX depois da
   * primeira. Nos grupos a espera é maior (menos SMS).
   */
  public static final long ESPERA_MS = 8_000, ESPERA_MAX_MS = 30_000;
  public static final long ESPERA_GRUPO_MS = 20_000, ESPERA_GRUPO_MAX_MS = 60_000;

  /** Ao ligar a ponte, das conversas que ela não conhecia só vão as mensagens dos últimos 30 min */
  public static final long JANELA_AO_LIGAR_MS = 30 * 60 * 1000;

  public static SharedPreferences opcoes(Context c) {
    return c.getSharedPreferences("ponte", Context.MODE_PRIVATE);
  }

  /** As marcas das mensagens já vistas, por conversa (arquivo à parte: muda a cada mensagem) */
  public static SharedPreferences vistas(Context c) {
    return c.getSharedPreferences("vistas", Context.MODE_PRIVATE);
  }

  public static String numeroTijolo(Context c) {
    return opcoes(c).getString("tijolo", "");
  }

  public static boolean ligada(Context c) {
    return opcoes(c).getBoolean("ligada", true);
  }

  /** Grupos costumam ter muita mensagem: começam desligados */
  public static boolean grupos(Context c) {
    return opcoes(c).getBoolean("grupos", false);
  }

  public static boolean silenciada(Context c, String chave) {
    return c.getSharedPreferences("silencio", Context.MODE_PRIVATE).getBoolean(chave, false);
  }

  public static void silenciar(Context c, String chave, boolean sim) {
    final SharedPreferences.Editor e = c.getSharedPreferences("silencio", Context.MODE_PRIVATE).edit();
    if (sim) e.putBoolean(chave, true);
    else e.remove(chave);
    e.apply();
  }

  private static Conversas conversas;

  /** A tabela de conversas (uma só para o app todo) */
  public static synchronized Conversas conversas(Context c) {
    if (conversas == null) {
      final Context app = c.getApplicationContext();
      conversas = new Conversas(new Conversas.Guarda() {
        @Override
        public String ler() {
          return opcoes(app).getString("conversas", "");
        }

        @Override
        public void gravar(String conteudo) {
          opcoes(app).edit().putString("conversas", conteudo).apply();
        }
      });
    }
    return conversas;
  }

  public static boolean ehWhatsApp(String pacote) {
    for (String p : PACOTES_WHATSAPP)
      if (p.equals(pacote)) return true;
    return false;
  }

  /** Só os dígitos */
  public static String digitos(String s) {
    final StringBuilder b = new StringBuilder();
    if (s != null)
      for (int i = 0; i < s.length(); i++)
        if (s.charAt(i) >= '0' && s.charAt(i) <= '9') b.append(s.charAt(i));
    return b.toString();
  }

  /**
   * O mesmo telefone? "+5511988887777", "011988887777" e "988887777" são o
   * mesmo: compara os últimos 9 dígitos (8, se um deles for mais curto).
   */
  public static boolean mesmoNumero(String a, String b) {
    final String x = digitos(a), y = digitos(b);
    if (x.length() < 8 || y.length() < 8) return false;
    final int n = Math.min(9, Math.min(x.length(), y.length()));
    return x.substring(x.length() - n).equals(y.substring(y.length() - n));
  }

  private static int codigo = (int) (System.currentTimeMillis() & 0xFFFFFF);

  private static synchronized int proximoCodigo() {
    return ++codigo & 0x7FFFFFFF;
  }

  /** Manda uma SMS (em partes, se for longa) e anota no registro. */
  public static boolean enviarSms(Context c, String numero, String texto) {
    if (numero == null || digitos(numero).length() < 8) {
      registrar(c, "Sem o número do Tijolo: não mandei \"" + resumo(texto) + "\"");
      return false;
    }
    try {
      final SmsManager sms = SmsManager.getDefault();
      final ArrayList<String> partes = sms.divideMessage(texto);
      final ArrayList<PendingIntent> avisos = new ArrayList<>();
      final int imutavel = Build.VERSION.SDK_INT >= 23 ? 0x04000000 /* FLAG_IMMUTABLE */ : 0;
      for (int i = 0; i < partes.size(); i++) {
        final Intent it = new Intent(c, SmsEnviada.class);
        it.putExtra("resumo", resumo(texto));
        it.putExtra("parte", i + 1);
        it.putExtra("partes", partes.size());
        avisos.add(PendingIntent.getBroadcast(c, proximoCodigo(), it, PendingIntent.FLAG_UPDATE_CURRENT | imutavel));
      }
      sms.sendMultipartTextMessage(numero, null, partes, avisos, null);
      return true;
    } catch (Exception e) {
      registrar(c, "Erro ao mandar SMS: " + e.getMessage());
      return false;
    }
  }

  static String resumo(String s) {
    final String u = Protocolo.umaLinha(s);
    return u.length() > 40 ? u.substring(0, 40) + "..." : u;
  }

  // ---------- registro: as últimas 40 linhas, para a tela do app ----------
  public static synchronized void registrar(Context c, String linha) {
    final SharedPreferences p = opcoes(c);
    final String hora = new SimpleDateFormat("dd/MM HH:mm", Locale.ROOT).format(new Date());
    String r = hora + "  " + linha + "\n" + p.getString("registro", "");
    final String[] linhas = r.split("\n");
    if (linhas.length > 40) {
      final StringBuilder b = new StringBuilder();
      for (int i = 0; i < 40; i++) b.append(linhas[i]).append('\n');
      r = b.toString();
    }
    p.edit().putString("registro", r).apply();
  }

  public static String registro(Context c) {
    return opcoes(c).getString("registro", "");
  }
}
