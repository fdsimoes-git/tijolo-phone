package br.tijolo.ponte;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.RemoteInput;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Parcelable;
import android.os.PowerManager;
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
  private static final String MARIA = "5511911112222@s.whatsapp.net";
  private static final String PEDRO = "5511933334444@s.whatsapp.net";
  private static final String FAMILIA = "120363001@g.us";

  private static void confere(boolean ok, String oque) {
    verificacoes++;
    if (!ok) {
      falhas++;
      System.out.println("  FALHOU: " + oque);
    }
  }

  private static String cod(String chave) {
    return Protocolo.codigo(chave);
  }

  // ---------- o mundo de mentira ----------
  private static void novoMundo(StatusBarNotification... abertas) throws Exception {
    Context.zerarPreferencias();
    SystemClock.zerar();
    Context.ALARMES.zerar();
    SmsManager.enviadas.clear();
    SmsManager.avisos.clear();
    PowerManager.presas = 0;
    vistas = 0;
    estatico(Ponte.class, "conversas", null);
    estatico(Ponte.class, "saida", null);
    estatico(Ponte.class, "desvio", 0L);
    estatico(Ouvinte.class, "instancia", null);
    ((Map<?, ?>) estatico(Ouvinte.class, "respostas")).clear();
    app = new Context();
    Ponte.opcoes(app).edit().putString("tijolo", TIJOLO).putString("conversas", "#\t1\n").apply();
    NotificationListenerService.abertas = abertas;
    ouvinte = new Ouvinte();
    ouvinte.onCreate();
    ouvinte.onListenerConnected();
    avancar(0);
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

  /** Anda os dois relógios juntos (o do Android e o do app), de meio em meio segundo */
  private static void avancar(long ms) {
    long falta = ms;
    do {
      final long passo = Math.min(500, falta);
      Ponte.desvio += passo;
      SystemClock.avancar(passo);
      falta -= passo;
    } while (falta > 0);
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
    avancar(0);
  }

  /** O rádio responde "foi" a tudo que estava saindo, até a fila parar */
  private static void radioOk() {
    for (int i = 0; i < 50 && !SmsManager.avisos.isEmpty(); i++) SmsManager.responder(-1);
  }

  /** As SMS que saíram desde a última conferência, juntas com "|" entre elas */
  private static String novasSms() {
    radioOk();
    final StringBuilder b = new StringBuilder();
    for (; vistas < SmsManager.enviadas.size(); vistas++) {
      final String[] s = SmsManager.enviadas.get(vistas);
      if (b.length() > 0) b.append('|');
      if (!s[0].equals(TIJOLO)) b.append("[para ").append(s[0]).append("] ");
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
    final String m = cod(MARIA), pe = cod(PEDRO), fa = cod(FAMILIA);

    System.out.println("1. mensagem de uma pessoa");
    novoMundo();
    final PendingIntent maria = new PendingIntent(null);
    postar(conversa("Maria", MARIA, maria, "Maria", "Oi, tudo bem?", 1));
    confere(PowerManager.presas == 1, "a trava de energia segura o Android acordado enquanto junta");
    avancar(7_000);
    sms("", "espera 8 s por mais mensagens");
    avancar(1_100);
    sms("WA#1" + m + " Maria\nOi, tudo bem?", "vai depois de 8 s, com o código da conversa");
    confere(PowerManager.presas == 0, "fila vazia: solta a trava");
    postar(conversa("Maria (2 mensagens)", MARIA, maria, "Maria", "Oi, tudo bem?", 1, "Maria", "Chego às 8 😀", 0));
    avancar(9_000);
    sms("WA#1" + m + " Maria\nChego às 8 :)", "só a nova; contagem fora do título; emoji vira sinal");

    System.out.println("2. mensagens seguidas vão juntas");
    postar(conversa("Maria", MARIA, maria, "Maria", "A", -1));
    avancar(5_000);
    postar(conversa("Maria", MARIA, maria, "Maria", "A", -1, "Maria", "B", -5));
    avancar(8_100);
    sms("WA#1" + m + " Maria\nA\nB", "juntas numa SMS");
    for (int i = 0; i < 8; i++) {                 // uma a cada 5 s: no máximo 30 s de espera
      final Object[] ms = new Object[3 * (i + 1)];
      for (int k = 0; k <= i; k++) {
        ms[3 * k] = "Maria";
        ms[3 * k + 1] = "m" + k;
        ms[3 * k + 2] = -20 - k;
      }
      postar(conversa("Maria", MARIA, maria, ms));
      avancar(5_000);
    }
    sms("WA#1" + m + " Maria\nm0\nm1\nm2\nm3\nm4\nm5", "sai em até 30 s mesmo com mensagem chegando");
    avancar(5_000);
    sms("WA#1" + m + " Maria\nm6\nm7", "e o resto 8 s depois da última");

    System.out.println("3. resposta do Tijolo");
    doTijolo("WA#1" + m + " Chego às 8!");
    confere("Chego às 8!".equals(respondido(maria)), "a resposta foi pelo botão Responder da Maria");
    sms("", "deu certo: sem aviso");
    // o WhatsApp refaz a notificação com a sua resposta por último: o título vira "Você"
    postar(conversa("Você", MARIA, maria, "Maria", "m7", -27, null, "Chego às 8!", -60));
    avancar(10_000);
    sms("", "a sua resposta não volta para o Tijolo");
    doTijolo("WA?");
    sms("WA? #1" + m + " Maria", "e a conversa continua com o nome da Maria (não \"Você\")");
    doTijolo("wa#1" + m.toUpperCase() + " linha 1\nlinha 2");
    confere("linha 1\nlinha 2".equals(respondido(maria)), "várias linhas, código em maiúsculas");

    System.out.println("4. pedidos do Tijolo");
    doTijolo("WA#5ab oi");
    sms("WA! Não conheço a conversa 5. Mande WA? para ver a lista.", "conversa desconhecida");
    doTijolo("WA#1 oi");
    sms("WA! A resposta veio sem o código da conversa: responda pela própria mensagem, no Tijolo.", "sem o código");
    final String errado = m.equals("zz") ? "yy" : "zz";
    doTijolo("WA#1" + errado + " oi");
    confere(novasSms().startsWith("WA! A conversa 1 agora é outra (Maria)"), "código errado: não vai");
    confere(!"oi".equals(respondido(maria)), "e não foi mesmo");
    doNumero("+55 21 98888-7777", "WA?");
    sms("", "outro DDD, mesmo número: não é o Tijolo");
    doNumero("+55 21 97777-6666", "WA?");
    sms("", "pedido de outro número: nada");
    doTijolo("Oi, tudo bem?");
    sms("", "SMS comum do Tijolo: nada");

    System.out.println("5. grupos");
    final PendingIntent familia = new PendingIntent(null);
    postar(grupo("Família 🏠", FAMILIA, familia, "João", "bom dia", 0));
    avancar(70_000);
    sms("", "os grupos começam desligados");
    Ponte.opcoes(app).edit().putBoolean("grupos", true).apply();
    postar(grupo("Família 🏠", FAMILIA, familia, "João", "bom dia", 0, "Ana 🌸", "oi gente", -1));
    avancar(15_000);
    sms("", "grupo espera 20 s");
    avancar(6_000);
    sms("WA#2" + fa + " Família\nAna: oi gente", "com o autor, sem o emoji e sem a mensagem de antes");
    doTijolo("WA#2" + fa + " Bom dia!");
    confere("Bom dia!".equals(respondido(familia)), "resposta no grupo");
    Ponte.opcoes(app).edit().putBoolean("grupos", false).apply();
    // foto de grupo sem os sinais de grupo: o id do WhatsApp (…@g.us) entrega
    final Notification foto = conversa("João @ Família", FAMILIA, familia, "João @ Família", "📷 Foto", -2);
    postar(foto);
    avancar(70_000);
    sms("", "foto de grupo com os grupos desligados: não vem");

    System.out.println("6. silenciar uma conversa / pausar tudo");
    doTijolo("WA-1" + m);
    sms("WA! Maria silenciada: as mensagens dela não vêm mais (Reativar a conversa, no Tijolo, volta).", "silenciar");
    postar(conversa("Maria", MARIA, maria, "Maria", "psiu", -100));
    avancar(40_000);
    sms("", "silenciada: nada");
    doTijolo("WA?");
    final String lista = novasSms();
    confere(lista.startsWith("WA? #") && lista.contains("#1" + m + " Maria (silenciada)") && lista.contains("#2" + fa + " Família"),
        "a lista mostra a silenciada: " + lista);
    doTijolo("WA+1" + m);
    sms("WA! Maria volta a vir para o Tijolo.", "reativar");
    postar(conversa("Maria", MARIA, maria, "Maria", "psiu", -100, "Maria", "voltou?", -101));
    avancar(9_000);
    sms("WA#1" + m + " Maria\nvoltou?", "reativada: vem (a de quando estava silenciada não)");
    postar(conversa("Maria", MARIA, maria, "Maria", "na fila", -102));
    doTijolo("WA-");
    sms("WA! Ponte pausada: o WhatsApp não vem para o Tijolo até você mandar WA+", "pausar");
    avancar(10_000);
    sms("", "a que estava juntando também não vai");
    postar(conversa("Maria", MARIA, maria, "Maria", "pausada", -103));
    avancar(40_000);
    sms("", "pausada: nada");
    doTijolo("WA+");
    sms("WA! Ponte ligada de novo.", "retomar");

    System.out.println("7. chamada perdida e notificações que não são mensagem");
    final Notification chamada = new Notification();
    chamada.extras.putCharSequence("android.title", "Maria");
    chamada.extras.putCharSequence("android.text", "Chamada de voz perdida");
    chamada.when = T0 + 200_000;
    postar(chamada);
    avancar(9_000);
    sms("WA#1" + m + " Maria\n[Chamada de voz perdida]", "chamada perdida vai para a conversa da Maria");
    final Notification web = new Notification();
    web.extras.putCharSequence("android.title", "WhatsApp Web");
    web.extras.putCharSequence("android.text", "WhatsApp Web está ativo");
    postar(web);
    final Notification resumo = conversa("Maria", MARIA, maria, "Maria", "resumo", -300);
    resumo.flags |= Notification.FLAG_GROUP_SUMMARY;
    postar(resumo);
    final Notification andamento = conversa("Maria", MARIA, maria, "Maria", "ligando", -301);
    andamento.flags |= Notification.FLAG_ONGOING_EVENT;
    postar(andamento);
    ouvinte.onNotificationPosted(new StatusBarNotification("com.outro.app", null,
        conversa("Banco", null, maria, "Banco", "Seu código é 1234", 0)));
    avancar(70_000);
    sms("", "ignora: WhatsApp Web, resumo do grupo, em andamento, outro app");
    confere(Ponte.conversas(app).total() == 2, "nenhuma conversa nova por elas");

    System.out.println("8. textos longos");
    final StringBuilder longo = new StringBuilder();
    for (int i = 0; i < 60; i++) longo.append("palavra").append(i).append(' ');
    postar(conversa("Maria", MARIA, maria, "Maria", longo.toString(), -400));
    avancar(9_100);
    String s = novasSms();
    final String cab = "WA#1" + m + " Maria\n";
    confere(s.startsWith(cab + "palavra0 ") && s.endsWith("...") && s.length() <= cab.length() + 330,
        "cortado em 330 com reticências: " + s.length());
    final String metade = longo.substring(0, 200);
    postar(conversa("Maria", MARIA, maria, "Maria", metade + "x", -401));
    postar(conversa("Maria", MARIA, maria, "Maria", metade + "x", -401, "Maria", metade + "y", -402));
    s = novasSms();
    confere(s.equals(cab + metade + "x"), "a segunda não cabe: a primeira vai já");
    avancar(9_000);
    confere(novasSms().equals(cab + metade + "y"), "e a segunda depois");
    Ponte.opcoes(app).edit().putBoolean("grupos", true).apply();
    postar(grupo("Família 🏠", FAMILIA, familia, "João", "bom dia", 0, "Ana 🌸", "oi gente", -1, "João", longo.toString(), -2));
    avancar(21_000);
    s = novasSms();
    confere(s.startsWith("WA#2" + fa + " Família\nJoão: palavra0") && s.length() <= ("WA#2" + fa + " Família\n").length() + 330,
        "num grupo, o nome entra antes do corte");
    Ponte.opcoes(app).edit().putBoolean("grupos", false).apply();

    System.out.println("9. quando a resposta não dá");
    avancar(61_000);                              // (o minuto das SMS acima passa)
    maria.cancelado = true;
    doTijolo("WA#1" + m + " alô");
    sms("WA! Não consigo responder a Maria agora: a ponte não tem a notificação dessa conversa. " +
        "Quando chegar outra mensagem dela, a resposta funciona.", "botão cancelado e nenhuma notificação aberta");
    final PendingIntent maria2 = new PendingIntent(null);
    NotificationListenerService.abertas = new StatusBarNotification[] {sbn(conversa("Maria", MARIA, maria2, "Maria", "x", -500))};
    doTijolo("WA#1" + m + " de novo");
    confere("de novo".equals(respondido(maria2)), "achou o botão na notificação aberta");
    sms("", "deu certo");
    estatico(Ouvinte.class, "instancia", null);   // o Android desligou o ouvinte
    doTijolo("WA#1" + m + " alô?");
    sms("", "espera o ouvinte voltar...");
    avancar(8_500);
    sms("WA! A ponte não está ouvindo as notificações. Abra o app Ponte do Tijolo no Android.", "...até 8 s");

    System.out.println("10. ao ligar a ponte");
    final PendingIntent pedro = new PendingIntent(null);
    novoMundo(sbn(conversa("Pedro", PEDRO, pedro, "Pedro", "de ontem", 86_400, "Pedro", "de agora há pouco", 300)));
    avancar(9_000);
    sms("WA#1" + pe + " Pedro\nde agora há pouco", "das abertas, só a última meia hora");
    doTijolo("WA#1" + pe + " oi Pedro");
    confere("oi Pedro".equals(respondido(pedro)), "o botão das abertas fica guardado");
    estatico(Ouvinte.class, "instancia", null);   // o Android reinicia a ponte
    ouvinte = new Ouvinte();
    ouvinte.onCreate();
    ouvinte.onListenerConnected();
    avancar(9_000);
    sms("", "religada: o que já foi não vai de novo");

    System.out.println("11. o app morre com mensagem juntando");
    final StatusBarNotification aberta = sbn(conversa("Pedro", PEDRO, pedro, "Pedro", "de ontem", 86_400,
        "Pedro", "de agora há pouco", 300, "Pedro", "última", -10));
    NotificationListenerService.abertas = new StatusBarNotification[] {aberta};
    ouvinte.onNotificationPosted(aberta);
    avancar(2_000);                               // ainda juntando...
    SystemClock.zerar();                          // ...e o Android mata o app (sem onDestroy)
    estatico(Ouvinte.class, "instancia", null);
    estatico(Ponte.class, "conversas", null);
    estatico(Ponte.class, "saida", null);
    ouvinte = new Ouvinte();
    ouvinte.onCreate();
    ouvinte.onListenerConnected();                // a notificação continua aberta
    avancar(9_000);
    sms("WA#1" + pe + " Pedro\núltima", "a mensagem não se perde: vai quando a ponte volta");
    ouvinte.onNotificationPosted(aberta);
    avancar(20_000);
    sms("", "e não vai duas vezes");
    postar(conversa("Pedro", PEDRO, pedro, "Pedro", "tchau", -20));
    ouvinte.onDestroy();
    sms("WA#1" + pe + " Pedro\ntchau", "fechando direito, o que estava juntando vai na hora");

    System.out.println("12. sem crédito ou sem sinal: a SMS espera e tenta de novo");
    novoMundo();
    postar(conversa("Maria", MARIA, maria, "Maria", "importante", -1));
    avancar(9_000);
    confere(SmsManager.enviadas.size() == 1, "tentou mandar");
    SmsManager.responder(1);                      // falha genérica (sem crédito?)
    confere(Ponte.registro(app).contains("FALHOU a SMS (falha genérica: crédito ou chip?)"), "anotado no registro");
    confere(Ponte.saida(app).pendentes() == 1, "continua na fila");
    avancar(30_000);
    confere(SmsManager.enviadas.size() == 1, "espera 1 minuto");
    avancar(31_000);
    confere(SmsManager.enviadas.size() == 2 && SmsManager.enviadas.get(1)[1].endsWith("importante"), "e tenta de novo");
    SmsManager.responder(-1);
    confere(Ponte.saida(app).pendentes() == 0 && Ponte.registro(app).contains("→ Tijolo:"), "foi");
    vistas = SmsManager.enviadas.size();

    System.out.println("13. muitas conversas de uma vez: no máximo 20 partes por minuto");
    novoMundo();
    for (int i = 0; i < 25; i++)
      postar(conversa("Pessoa " + i, "55119000" + (1000 + i) + "@s.whatsapp.net", new PendingIntent(null), "Pessoa " + i, "oi " + i, -1));
    avancar(9_000);
    radioOk();
    confere(SmsManager.enviadas.size() - vistas == 20, "20 no primeiro minuto: " + (SmsManager.enviadas.size() - vistas));
    avancar(61_000);
    radioOk();
    confere(SmsManager.enviadas.size() - vistas == 25, "o resto depois");
    vistas = SmsManager.enviadas.size();

    System.out.println("14. o nome errado no título");
    novoMundo();
    // sem o atalho do WhatsApp (Android antigo): a conversa é reconhecida pelo título
    final PendingIntent ana = new PendingIntent(null);
    postar(conversa("Ana", null, ana, "Ana", "oi", -1));
    avancar(9_000);
    sms("WA#1" + cod("titulo:Ana") + " Ana\noi", "pelo título");
    doTijolo("WA#1" + cod("titulo:Ana") + " tudo bem?");
    postar(conversa("Você", null, ana, "Ana", "oi", -1, null, "tudo bem?", -2));
    avancar(9_000);
    sms("", "com o título \"Você\", não vira outra conversa nem repete a mensagem");
    confere(Ponte.conversas(app).total() == 1, "uma conversa só");
    // Android 15 escondendo uma mensagem com código: título "WhatsApp", autor sem nome
    final Notification oculta = new Notification();
    oculta.extras.putCharSequence("android.title", "WhatsApp");
    final Bundle b = new Bundle();
    b.putCharSequence("sender", "");
    b.putCharSequence("text", "Conteúdo sensível oculto");
    b.putLong("time", T0 + 3000);
    oculta.extras.putParcelableArray("android.messages", new Parcelable[] {b});
    oculta.actions = new Notification.Action[] {new Notification.Action(new PendingIntent(null), new RemoteInput("resposta", true))};
    oculta.atalho = MARIA;
    postar(oculta);
    avancar(9_000);
    s = novasSms();
    confere(s.equals("WA#2" + m + " Conversa 2\nConteúdo sensível oculto"), "mensagem escondida vem (e o nome não vira \"WhatsApp\"): " + s);
    // o contato muda de nome: as mesmas mensagens não voltam com outro título
    postar(conversa("Aninha", null, ana, "Aninha", "oi", -1, "Aninha", "novo nome", -5));
    avancar(9_000);
    sms("WA#3" + cod("titulo:Aninha") + " Aninha\nnovo nome", "outra chave: só a mensagem nova");
    // "(2019 formandos)" é parte do nome; "(3 mensagens)" não
    Ponte.opcoes(app).edit().putBoolean("grupos", true).apply();
    postar(grupo("Turma (2019 formandos)", "999@g.us", new PendingIntent(null), "Rui", "festa!", -6));
    avancar(21_000);
    s = novasSms();
    confere(s.startsWith("WA#4" + cod("999@g.us") + " Turma (2019 formandos)\n"), "parênteses do nome ficam: " + s);

    System.out.println("15. a vigia e o Android ligando");
    novoMundo();
    estatico(Ouvinte.class, "instancia", null);   // parou de ouvir
    final int religa = NotificationListenerService.religacoes;
    Despertador.vigiar(app);
    confere(NotificationListenerService.religacoes == religa + 1, "pede para religar");
    sms("", "uma vigia só não avisa (pode estar religando)");
    Despertador.vigiar(app);
    sms("WA! A ponte parou de ouvir o WhatsApp. Abra o app Ponte do Tijolo no Android.", "duas seguidas: avisa");
    Despertador.vigiar(app);
    sms("", "avisa uma vez só");
    ouvinte.onListenerConnected();
    Despertador.vigiar(app);
    sms("WA! A ponte voltou a ouvir o WhatsApp.", "voltou: avisa também");
    final Intent ligou = new Intent(Intent.ACTION_BOOT_COMPLETED);
    new AoLigar().onReceive(app, ligou);
    sms("WA! A ponte ligou de novo (o Android reiniciou).", "o Android ligou");
    confere(Context.ALARMES.marcados.containsKey(Ponte.ACAO_VIGIA + "#" + Ponte.ACAO_VIGIA.hashCode()), "a vigia ficou marcada");

    System.out.println("16. o resultado do envio no registro");
    novoMundo();
    Ponte.enviar(app, "WA#1ab Pedro\núltima linha bem comprida para ir em duas partes, passando de setenta letras ao todo, com acento");
    SmsManager.responder(-1);
    final String ultima = Ponte.registro(app).split("\n")[0];
    confere(ultima.contains("→ Tijolo: WA#1ab Pedro última linha") && ultima.endsWith("(2 partes)"), "enviada: " + ultima);

    System.out.println("\n" + verificacoes + " verificações, " + falhas + " falha(s)");
    System.exit(falhas == 0 ? 0 : 1);
  }
}
