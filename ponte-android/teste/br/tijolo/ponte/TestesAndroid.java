package br.tijolo.ponte;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.RemoteInput;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Parcelable;
import android.os.SystemClock;
import android.provider.Telephony;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;
import android.telephony.SmsManager;
import android.telephony.SmsMessage;

import java.lang.reflect.Field;
import java.util.Map;

/**
 * O caminho inteiro da ponte, com um Android de mentira (teste/falso):
 * notificação do WhatsApp → SMS para o Tijolo, e SMS do Tijolo → resposta
 * no WhatsApp. O relógio só anda quando o teste manda.
 *   sh testar.sh
 */
public class TestesAndroid {
  private static int verificacoes = 0, falhas = 0;
  private static final String TIJOLO = "11988887777";
  private static Context app;
  private static Ouvinte ouvinte;
  private static final long T0 = System.currentTimeMillis();
  private static int vistas = 0;      // quantas SMS do teste já foram conferidas

  private static void confere(boolean ok, String oque) {
    verificacoes++;
    if (!ok) {
      falhas++;
      System.out.println("  FALHOU: " + oque);
    }
  }

  // ---------- o mundo de mentira ----------
  private static void novoMundo(StatusBarNotification... abertas) throws Exception {
    Context.zerarPreferencias();
    SystemClock.zerar();
    SmsManager.enviadas.clear();
    vistas = 0;
    estatico(Ponte.class, "conversas", null);
    estatico(Ouvinte.class, "instancia", null);
    ((Map<?, ?>) estatico(Ouvinte.class, "respostas")).clear();
    app = new Context();
    Ponte.opcoes(app).edit().putString("tijolo", TIJOLO).apply();
    NotificationListenerService.abertas = abertas;
    ouvinte = new Ouvinte();
    ouvinte.onCreate();
    ouvinte.onListenerConnected();
    SystemClock.avancar(0);
  }

  private static Object estatico(Class<?> c, String nome) throws Exception {
    final Field f = c.getDeclaredField(nome);
    f.setAccessible(true);
    return f.get(null);
  }

  private static void estatico(Class<?> c, String nome, Object valor) throws Exception {
    final Field f = c.getDeclaredField(nome);
    f.setAccessible(true);
    f.set(null, valor);
  }

  /** Notificação de conversa; msgs: autor (null = você), texto, segundos atrás */
  private static Notification conversa(String titulo, String atalho, PendingIntent responder, Object... msgs) {
    final Notification n = new Notification();
    n.extras.putCharSequence("android.title", titulo);
    final Parcelable[] ms = new Parcelable[msgs.length / 3];
    for (int i = 0; i < ms.length; i++) {
      final Bundle b = new Bundle();
      if (msgs[3 * i] != null) b.putCharSequence("sender", (String) msgs[3 * i]);
      b.putCharSequence("text", (String) msgs[3 * i + 1]);
      b.putLong("time", T0 - 1000L * (Integer) msgs[3 * i + 2]);
      ms[i] = b;
    }
    n.extras.putParcelableArray("android.messages", ms);
    n.extras.putCharSequence("android.selfDisplayName", "Você");
    if (responder != null) n.actions = new Notification.Action[] {new Notification.Action(responder, new RemoteInput("resposta", true))};
    n.atalho = atalho;
    return n;
  }

  private static Notification grupo(String nome, String atalho, PendingIntent responder, Object... msgs) {
    final Notification n = conversa(nome + ": " + msgs[0], atalho, responder, msgs);
    n.extras.putBoolean("android.isGroupConversation", true);
    n.extras.putCharSequence("android.conversationTitle", nome + " (3 mensagens)");
    return n;
  }

  private static StatusBarNotification sbn(Notification n) {
    return new StatusBarNotification("com.whatsapp", null, n);
  }

  private static void postar(Notification n) {
    ouvinte.onNotificationPosted(sbn(n));
  }

  private static void doTijolo(String texto) {
    doNumero("+55 (11) 98888-7777", texto);
  }

  private static void doNumero(String numero, String texto) {
    final Intent i = new Intent(Telephony.Sms.Intents.SMS_RECEIVED_ACTION);
    i.putExtra("pdus", new SmsMessage[] {new SmsMessage(numero, texto)});
    new SmsRecebido().onReceive(app, i);
    SystemClock.avancar(0);
  }

  /** As SMS que saíram desde a última conferência, juntas com "|" entre elas */
  private static String novasSms() {
    final StringBuilder b = new StringBuilder();
    for (; vistas < SmsManager.enviadas.size(); vistas++) {
      final String[] s = SmsManager.enviadas.get(vistas);
      if (!s[0].equals(TIJOLO)) b.append("[para ").append(s[0]).append("] ");
      if (b.length() > 0 && !b.toString().endsWith("] ")) b.append('|');
      b.append(s[1]);
    }
    return b.toString();
  }

  private static void sms(String esperado, String oque) {
    final String obtido = novasSms();
    confere(esperado.equals(obtido), oque + "\n    esperado [" + esperado + "]\n    obtido   [" + obtido + "]");
  }

  private static String respondido(PendingIntent pi) {
    if (pi.enviados.isEmpty()) return null;
    final Intent i = pi.enviados.get(pi.enviados.size() - 1);
    final Bundle r = (Bundle) i.extra("android.remoteinput.results");
    return r == null ? null : String.valueOf(r.getCharSequence("resposta"));
  }

  public static void main(String[] args) throws Exception {
    System.out.println("1. mensagem de uma pessoa");
    novoMundo();
    final PendingIntent maria = new PendingIntent(null);
    final String jidMaria = "5511911112222@s.whatsapp.net";
    postar(conversa("Maria", jidMaria, maria, "Maria", "Oi, tudo bem?", 1));
    SystemClock.avancar(7_000);
    sms("", "espera 8 s por mais mensagens");
    SystemClock.avancar(1_100);
    sms("WA#1 Maria\nOi, tudo bem?", "vai depois de 8 s");
    postar(conversa("Maria (2 mensagens)", jidMaria, maria, "Maria", "Oi, tudo bem?", 1, "Maria", "Chego às 8 😀", 0));
    SystemClock.avancar(9_000);
    sms("WA#1 Maria\nChego às 8 :)", "só a nova; contagem fora do título; emoji vira sinal");

    System.out.println("2. mensagens seguidas vão juntas");
    postar(conversa("Maria", jidMaria, maria, "Maria", "A", -1));
    SystemClock.avancar(5_000);
    postar(conversa("Maria", jidMaria, maria, "Maria", "A", -1, "Maria", "B", -5));
    SystemClock.avancar(8_100);
    sms("WA#1 Maria\nA\nB", "juntas numa SMS");
    for (int i = 0; i < 8; i++) {                 // uma a cada 5 s: no máximo 30 s de espera
      final Object[] ms = new Object[3 * (i + 1)];
      for (int k = 0; k <= i; k++) {
        ms[3 * k] = "Maria";
        ms[3 * k + 1] = "m" + k;
        ms[3 * k + 2] = -20 - k;
      }
      postar(conversa("Maria", jidMaria, maria, ms));
      SystemClock.avancar(5_000);
    }
    sms("WA#1 Maria\nm0\nm1\nm2\nm3\nm4\nm5", "sai em até 30 s mesmo com mensagem chegando");
    SystemClock.avancar(5_000);
    sms("WA#1 Maria\nm6\nm7", "e o resto 8 s depois da última");
    SystemClock.avancar(10_000);
    sms("", "nada sobrando");

    System.out.println("3. resposta do Tijolo");
    doTijolo("WA#1 Chego às 8!");
    confere("Chego às 8!".equals(respondido(maria)), "a resposta foi pelo botão Responder da Maria");
    sms("", "deu certo: sem aviso");
    postar(conversa("Maria", jidMaria, maria, "Maria", "m7", -27, null, "Chego às 8!", -60));
    SystemClock.avancar(10_000);
    sms("", "a sua resposta não volta para o Tijolo");
    doTijolo("wa#1 linha 1\nlinha 2");
    confere("linha 1\nlinha 2".equals(respondido(maria)), "várias linhas");

    System.out.println("4. pedidos do Tijolo");
    doTijolo("WA#5 oi");
    sms("WA! Não conheço a conversa 5. Mande WA? para ver a lista.", "conversa desconhecida");
    doTijolo("WA?");
    sms("WA? #1 Maria", "a lista");
    doNumero("+55 21 97777-6666", "WA?");
    sms("", "pedido de outro número: nada");
    doTijolo("Oi, tudo bem?");
    sms("", "SMS comum do Tijolo: nada");

    System.out.println("5. grupos");
    final PendingIntent familia = new PendingIntent(null);
    postar(grupo("Família 🏠", "120363001@g.us", familia, "João", "bom dia", 0));
    SystemClock.avancar(70_000);
    sms("", "os grupos começam desligados");
    Ponte.opcoes(app).edit().putBoolean("grupos", true).apply();
    postar(grupo("Família 🏠", "120363001@g.us", familia, "João", "bom dia", 0, "Ana 🌸", "oi gente", -1));
    SystemClock.avancar(15_000);
    sms("", "grupo espera 20 s");
    SystemClock.avancar(6_000);
    sms("WA#2 Família\nAna: oi gente", "com o autor, sem o emoji e sem a mensagem de antes");
    doTijolo("WA#2 Bom dia!");
    confere("Bom dia!".equals(respondido(familia)), "resposta no grupo");

    System.out.println("6. silenciar uma conversa / pausar tudo");
    doTijolo("WA-1");
    sms("WA! Maria silenciada: as mensagens dela não vêm mais (WA+1 para voltar).", "silenciar");
    postar(conversa("Maria", jidMaria, maria, "Maria", "psiu", -100));
    SystemClock.avancar(40_000);
    sms("", "silenciada: nada");
    doTijolo("WA?");
    final String lista = novasSms();
    confere(lista.startsWith("WA? #") && lista.contains("#1 Maria (silenciada)") && lista.contains("#2 Família"),
        "a lista mostra a silenciada: " + lista);
    doTijolo("WA+1");
    sms("WA! Maria volta a vir para o Tijolo.", "reativar");
    postar(conversa("Maria", jidMaria, maria, "Maria", "psiu", -100, "Maria", "voltou?", -101));
    SystemClock.avancar(9_000);
    sms("WA#1 Maria\nvoltou?", "reativada: vem (a de quando estava silenciada não)");
    doTijolo("WA-");
    sms("WA! Ponte pausada: o WhatsApp não vem para o Tijolo até você mandar WA+", "pausar");
    postar(conversa("Maria", jidMaria, maria, "Maria", "pausada", -102));
    SystemClock.avancar(40_000);
    sms("", "pausada: nada");
    doTijolo("WA+");
    sms("WA! Ponte ligada de novo.", "retomar");

    System.out.println("7. chamada perdida e notificações que não são mensagem");
    final Notification chamada = new Notification();
    chamada.extras.putCharSequence("android.title", "Maria");
    chamada.extras.putCharSequence("android.text", "Chamada de voz perdida");
    chamada.when = T0 + 200_000;
    postar(chamada);
    SystemClock.avancar(9_000);
    sms("WA#1 Maria\n[Chamada de voz perdida]", "chamada perdida vai para a conversa da Maria");
    final Notification web = new Notification();
    web.extras.putCharSequence("android.title", "WhatsApp Web");
    web.extras.putCharSequence("android.text", "WhatsApp Web está ativo");
    postar(web);
    final Notification resumo = conversa("Maria", jidMaria, maria, "Maria", "resumo", -300);
    resumo.flags |= Notification.FLAG_GROUP_SUMMARY;
    postar(resumo);
    final Notification andamento = conversa("Maria", jidMaria, maria, "Maria", "ligando", -301);
    andamento.flags |= Notification.FLAG_ONGOING_EVENT;
    postar(andamento);
    ouvinte.onNotificationPosted(new StatusBarNotification("com.outro.app", null,
        conversa("Banco", null, maria, "Banco", "Seu código é 1234", 0)));
    SystemClock.avancar(70_000);
    sms("", "ignora: WhatsApp Web, resumo do grupo, em andamento, outro app");
    confere(Ponte.conversas(app).total() == 2, "nenhuma conversa nova por elas");

    System.out.println("8. textos longos");
    final StringBuilder longo = new StringBuilder();
    for (int i = 0; i < 60; i++) longo.append("palavra").append(i).append(' ');
    postar(conversa("Maria", jidMaria, maria, "Maria", longo.toString(), -400));
    SystemClock.avancar(100);
    String s = novasSms();
    confere(s.isEmpty(), "um só, cortado, ainda espera");
    SystemClock.avancar(9_000);
    s = novasSms();
    confere(s.startsWith("WA#1 Maria\npalavra0 ") && s.endsWith("...") && s.length() <= "WA#1 Maria\n".length() + 330,
        "cortado em 330 com reticências: " + s.length());
    final String metade = longo.substring(0, 200);
    postar(conversa("Maria", jidMaria, maria, "Maria", metade + "x", -401));
    postar(conversa("Maria", jidMaria, maria, "Maria", metade + "x", -401, "Maria", metade + "y", -402));
    s = novasSms();
    confere(s.equals("WA#1 Maria\n" + metade + "x"), "a segunda não cabe: a primeira vai já");
    SystemClock.avancar(9_000);
    confere(novasSms().equals("WA#1 Maria\n" + metade + "y"), "e a segunda depois");

    System.out.println("9. quando a resposta não dá");
    maria.cancelado = true;
    doTijolo("WA#1 alô");
    sms("WA! Não consigo responder a Maria agora: a ponte não tem a notificação dessa conversa. " +
        "Quando chegar outra mensagem dela, a resposta funciona.", "botão cancelado e nenhuma notificação aberta");
    final PendingIntent maria2 = new PendingIntent(null);
    NotificationListenerService.abertas = new StatusBarNotification[] {sbn(conversa("Maria", jidMaria, maria2, "Maria", "x", -500))};
    doTijolo("WA#1 de novo");
    confere("de novo".equals(respondido(maria2)), "achou o botão na notificação aberta");
    sms("", "deu certo");
    estatico(Ouvinte.class, "instancia", null);   // o Android desligou o ouvinte
    doTijolo("WA#1 alô?");
    sms("", "espera o ouvinte voltar...");
    SystemClock.avancar(8_500);
    sms("WA! A ponte não está ouvindo as notificações. Abra o app Ponte do Tijolo no Android.", "...até 8 s");

    System.out.println("10. ao ligar a ponte");
    final PendingIntent pedro = new PendingIntent(null);
    novoMundo(sbn(conversa("Pedro", "5511933334444@s.whatsapp.net", pedro, "Pedro", "de ontem", 86_400, "Pedro", "de agora há pouco", 300)));
    SystemClock.avancar(9_000);
    sms("WA#1 Pedro\nde agora há pouco", "das abertas, só a última meia hora");
    doTijolo("WA#1 oi Pedro");
    confere("oi Pedro".equals(respondido(pedro)), "o botão das abertas fica guardado");
    // o Android reinicia a ponte: a mesma notificação aberta não vai de novo
    estatico(Ouvinte.class, "instancia", null);
    ouvinte = new Ouvinte();
    ouvinte.onCreate();
    ouvinte.onListenerConnected();
    SystemClock.avancar(9_000);
    sms("", "religada: o que já foi não vai de novo");

    System.out.println("11. o ouvinte fechando com mensagem juntando");
    postar(conversa("Pedro", "5511933334444@s.whatsapp.net", pedro, "Pedro", "de ontem", 86_400,
        "Pedro", "de agora há pouco", 300, "Pedro", "última", -10));
    ouvinte.onDestroy();
    sms("WA#1 Pedro\núltima", "vai na hora, não se perde");
    SystemClock.avancar(20_000);
    sms("", "e não vai duas vezes");

    System.out.println("12. o resultado do envio no registro");
    final SmsEnviada r = new SmsEnviada();
    final Intent ok = new Intent();
    ok.putExtra("resumo", "WA#1 Pedro última");
    ok.putExtra("parte", 2);
    ok.putExtra("partes", 2);
    r.setResultCode(android.app.Activity.RESULT_OK);
    r.onReceive(app, ok);
    confere(Ponte.registro(app).contains("→ Tijolo: WA#1 Pedro última (2 partes)"), "enviada");
    final SmsEnviada falhou = new SmsEnviada();
    falhou.setResultCode(4);
    falhou.onReceive(app, ok);
    confere(Ponte.registro(app).contains("FALHOU a SMS (sem sinal)"), "falhou, com o motivo");

    System.out.println("\n" + verificacoes + " verificações, " + falhas + " falha(s)");
    System.exit(falhas == 0 ? 0 : 1);
  }
}
