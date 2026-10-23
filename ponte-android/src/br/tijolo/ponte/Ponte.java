package br.tijolo.ponte;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.SystemClock;
import android.telephony.SmsManager;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Locale;

/** O que as partes do app usam em comum: as opções, a caixa de saída das SMS, o registro. */
public final class Ponte {
  private Ponte() {}

  public static final String[] PACOTES_WHATSAPP = {"com.whatsapp", "com.whatsapp.w4b"};
  public static final String[] NOMES_DO_APP = {"WhatsApp", "WhatsApp Business"};

  /**
   * Texto máximo de uma SMS repassada (o resto vira "..."). Com acento, o SMS
   * vai em partes de 67 letras: 330 + "WA#123xx " + o título cabem em 6
   * partes (cada parte é cobrada como uma SMS).
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

  /** O relógio do app (os testes no computador adiantam com `adiantar`) */
  static long desvio = 0;

  public static long agora() {
    return System.currentTimeMillis() + desvio;
  }

  static final String ACAO_SAIDA = "br.tijolo.ponte.SAIDA";
  static final String ACAO_VIGIA = "br.tijolo.ponte.VIGIA";
  static final long VIGIA_MS = 30 * 60 * 1000;

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
  private static Saida saida;

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

  /** A caixa de saída (gravada na hora: uma SMS na fila não se perde) */
  public static synchronized Saida saida(Context c) {
    if (saida == null) {
      final Context app = c.getApplicationContext();
      saida = new Saida(new Saida.Guarda() {
        @Override
        public String ler() {
          return app.getSharedPreferences("saida", Context.MODE_PRIVATE).getString("fila", "");
        }

        @Override
        public void gravar(String conteudo) {
          app.getSharedPreferences("saida", Context.MODE_PRIVATE).edit().putString("fila", conteudo).commit();
        }
      });
    }
    return saida;
  }

  public static boolean ehWhatsApp(String pacote) {
    for (String p : PACOTES_WHATSAPP)
      if (p.equals(pacote)) return true;
    return false;
  }

  public static boolean ehNomeDoApp(String s) {
    if (s == null) return false;
    for (String n : NOMES_DO_APP)
      if (n.equalsIgnoreCase(s.trim())) return true;
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
   * O número do Tijolo como digitado, arrumado: "+55 11 98888-7777" →
   * "+5511988887777"; "11 98888-7777" → "11988887777"; "011 98888-7777" →
   * "11988887777". null se não der para usar (veja motivoNumero).
   */
  public static String arrumarNumero(String digitado) {
    if (digitado == null) return null;
    String d = digitos(digitado);
    boolean mais = digitado.trim().startsWith("+");
    // "+11 98888-7777": o + sobrou (um número dos EUA não tem 9 no 3º dígito)
    if (mais && !d.startsWith("55") && d.length() == 11 && d.charAt(2) == '9') mais = false;
    if (!mais && d.startsWith("0") && (d.length() == 11 || d.length() == 12)) d = d.substring(1);   // 0 + DDD
    if (!mais && d.startsWith("55") && (d.length() == 12 || d.length() == 13)) mais = true;
    if (mais) {
      if (d.startsWith("55") && d.length() == 12 && celularSemNove(d.substring(2))) return null;
      return d.length() >= 10 ? "+" + d : null;
    }
    if (d.length() == 11) return d;
    if (d.length() == 10 && !celularSemNove(d)) return d;     // fixo: 2 a 5 depois do DDD
    return null;
  }

  /** Por que o número não serve (para a tela), ou null se serve */
  public static String motivoNumero(String digitado) {
    if (arrumarNumero(digitado) != null) return null;
    final String d = digitos(digitado);
    final String n = d.startsWith("55") && d.length() >= 12 ? d.substring(2) : d.startsWith("0") ? d.substring(1) : d;
    if (n.length() == 10 && celularSemNove(n)) return "Celular tem 11 dígitos com o DDD: falta o 9 depois do DDD?";
    return "Escreva o número com DDD (11 dígitos, como 11 98888-7777).";
  }

  // DDD + 8 dígitos começando com 6 a 9: é celular sem o 9 da frente
  private static boolean celularSemNove(String d) {
    return d.length() == 10 && d.charAt(2) >= '6';
  }

  /**
   * O mesmo telefone? "+5511988887777", "011988887777" e "11988887777" são
   * o mesmo: compara os últimos 11 dígitos (DDD + número; 10 se um deles
   * for mais curto). Sem o DDD não vale: outro DDD é outro telefone.
   */
  public static boolean mesmoNumero(String a, String b) {
    final String x = digitos(a), y = digitos(b);
    final int n = Math.min(11, Math.min(x.length(), y.length()));
    if (n < 10) return false;
    return x.substring(x.length() - n).equals(y.substring(y.length() - n));
  }

  // ----------------------------------------------------------------------
  // SMS para o Tijolo: tudo passa pela caixa de saída
  // ----------------------------------------------------------------------

  /** Põe no fim da fila (as mensagens do WhatsApp) e tenta mandar já. */
  public static void enviar(Context c, String texto) {
    saida(c).adicionar(texto, agora());
    esvaziarSaida(c);
  }

  /** Um aviso ou resposta da ponte ("WA!", "WA?"): passa na frente das mensagens */
  public static void avisar(Context c, String texto) {
    saida(c).adicionarNaFrente(texto, agora());
    esvaziarSaida(c);
  }

  /** Manda o que der (o limite por minuto, as esperas depois de falha); agenda o resto. */
  public static void esvaziarSaida(Context c) {
    final Saida s = saida(c);
    while (true) {
      final long agora = agora();
      final Saida.Item i = s.proxima(agora);
      final int velhas = s.tirarDescartadas();
      if (velhas > 0) {
        registrar(c, "Desisti de " + velhas + " SMS: 2 dias sem conseguir mandar.");
        s.adicionarNaFrente(Protocolo.aviso(velhas + (velhas == 1 ? " mensagem do WhatsApp não chegou" :
            " mensagens do WhatsApp não chegaram") + " ao Tijolo: a ponte passou 2 dias sem conseguir mandar SMS."), agora);
        continue;
      }
      if (i == null) {
        final long quando = s.proximaHora(agora);
        if (quando > 0) agendar(c, ACAO_SAIDA, quando);
        return;
      }
      final String numero = numeroTijolo(c);
      if (digitos(numero).length() < 10) {
        // uma vez só (até o número ser guardado): as SMS esperam na fila
        if (!opcoes(c).getBoolean("avisou_sem_numero", false)) {
          opcoes(c).edit().putBoolean("avisou_sem_numero", true).apply();
          registrar(c, "Sem o número do Tijolo: as SMS esperam na fila até ele ser guardado.");
        }
        agendar(c, ACAO_SAIDA, agora + 10 * 60 * 1000);
        return;
      }
      final SmsManager sms = SmsManager.getDefault();
      final ArrayList<String> partes = sms.divideMessage(i.texto);
      if (!s.cabe(partes.size(), agora)) {
        agendar(c, ACAO_SAIDA, s.quandoCabe(agora) + 500);
        return;
      }
      try {
        final ArrayList<PendingIntent> avisos = new ArrayList<>();
        final int imutavel = Build.VERSION.SDK_INT >= 23 ? 0x04000000 /* FLAG_IMMUTABLE */ : 0;
        s.enviando(i, partes.size(), agora);
        for (int p = 0; p < partes.size(); p++) {
          final Intent it = new Intent(c, SmsEnviada.class);
          it.putExtra("id", i.id);
          it.putExtra("tentativa", i.tentativas);
          it.putExtra("resumo", resumo(i.texto));
          it.putExtra("parte", p + 1);
          it.putExtra("partes", partes.size());
          avisos.add(PendingIntent.getBroadcast(c, proximoCodigo(), it, PendingIntent.FLAG_UPDATE_CURRENT | imutavel));
        }
        sms.sendMultipartTextMessage(numero, null, partes, avisos, null);
      } catch (Exception e) {
        registrar(c, "Erro ao mandar SMS: " + e.getMessage());
        s.resultado(i.id, i.tentativas, false, agora);
      }
    }
  }

  private static int codigo = (int) (System.currentTimeMillis() & 0xFFFFFF);

  private static synchronized int proximoCodigo() {
    return ++codigo & 0x7FFFFFFF;
  }

  /** Acorda o app em `quando` (hora do relógio), mesmo com o Android dormindo */
  static void agendar(Context c, String acao, long quando) {
    final AlarmManager am = (AlarmManager) c.getSystemService(Context.ALARM_SERVICE);
    if (am == null) return;
    final long daqui = Math.max(1000, quando - agora());
    final PendingIntent pi = despertador(c, acao);
    final long em = SystemClock.elapsedRealtime() + daqui;
    try {
      if (Build.VERSION.SDK_INT >= 23) am.setExactAndAllowWhileIdle(AlarmManager.ELAPSED_REALTIME_WAKEUP, em, pi);
      else am.setExact(AlarmManager.ELAPSED_REALTIME_WAKEUP, em, pi);
    } catch (Exception e) {
      am.set(AlarmManager.ELAPSED_REALTIME_WAKEUP, em, pi);
    }
  }

  /** A vigia: a cada 30 min confere se a ponte ainda ouve o WhatsApp e se sobrou SMS na fila */
  static void agendarVigia(Context c) {
    final AlarmManager am = (AlarmManager) c.getSystemService(Context.ALARM_SERVICE);
    if (am == null) return;
    am.setInexactRepeating(AlarmManager.ELAPSED_REALTIME_WAKEUP, SystemClock.elapsedRealtime() + VIGIA_MS,
        VIGIA_MS, despertador(c, ACAO_VIGIA));
  }

  private static PendingIntent despertador(Context c, String acao) {
    final Intent it = new Intent(c, Despertador.class);
    it.setAction(acao);
    final int imutavel = Build.VERSION.SDK_INT >= 23 ? 0x04000000 : 0;
    return PendingIntent.getBroadcast(c, acao.hashCode(), it, PendingIntent.FLAG_UPDATE_CURRENT | imutavel);
  }

  static String resumo(String s) {
    final String u = Protocolo.umaLinha(s);
    return u.length() > 40 ? u.substring(0, 40) + "..." : u;
  }

  static String hora(long quando) {
    return new SimpleDateFormat("HH:mm", Locale.ROOT).format(new Date(quando));
  }

  // ---------- registro: as últimas 60 linhas, para a tela do app ----------
  public static synchronized void registrar(Context c, String linha) {
    final SharedPreferences p = opcoes(c);
    final String hora = new SimpleDateFormat("dd/MM HH:mm", Locale.ROOT).format(new Date());
    String r = hora + "  " + linha + "\n" + p.getString("registro", "");
    final String[] linhas = r.split("\n");
    if (linhas.length > 60) {
      final StringBuilder b = new StringBuilder();
      for (int i = 0; i < 60; i++) b.append(linhas[i]).append('\n');
      r = b.toString();
    }
    p.edit().putString("registro", r).apply();
  }

  public static String registro(Context c) {
    return opcoes(c).getString("registro", "");
  }
}
