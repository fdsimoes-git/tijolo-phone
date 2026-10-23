package br.tijolo.ponte;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.RemoteInput;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Parcelable;
import android.os.SystemClock;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Pattern;

/**
 * Ouve as notificações do Android. Quando chega mensagem do WhatsApp, manda
 * para o Tijolo por SMS ("WA#7 Maria\nChego às 8"); e guarda o botão
 * "Responder" de cada conversa, que é por onde a resposta do Tijolo volta.
 *
 * O Android mantém este serviço ligado enquanto o app tiver o "acesso às
 * notificações" (Configurações): é ele que deixa a ponte sempre de pé.
 *
 * Tudo aqui roda na linha principal do app, uma coisa de cada vez (o
 * Android 5 e 6 chamam o ouvinte de outra linha: naMao() traz para ela).
 */
public class Ouvinte extends NotificationListenerService {
  private static volatile Ouvinte instancia;

  /** O botão "Responder" mais recente de uma conversa (vale mesmo depois que a notificação some). */
  private static final class Botao {
    final String chave;
    final Notification.Action acao;

    Botao(String chave, Notification.Action acao) {
      this.chave = chave;
      this.acao = acao;
    }
  }

  private static final Map<Integer, Botao> respostas = new HashMap<>();

  /** Mensagens de uma conversa juntando para irem numa SMS só. */
  private static final class Pacote {
    final int numero;
    final long inicio;
    String titulo;
    final List<String> linhas = new ArrayList<>();
    Runnable envio;

    Pacote(int numero, long inicio) {
      this.numero = numero;
      this.inicio = inicio;
    }
  }

  private final Handler mao = new Handler(Looper.getMainLooper());
  private final Map<Integer, Pacote> fila = new HashMap<>();
  private Conversas conversas;

  public static boolean ouvindo() {
    return instancia != null;
  }

  private void naMao(Runnable r) {
    if (Looper.myLooper() == Looper.getMainLooper()) r.run();
    else mao.post(r);
  }

  @Override
  public void onCreate() {
    super.onCreate();
    conversas = Ponte.conversas(this);
  }

  @Override
  public void onListenerConnected() {
    naMao(new Runnable() {
      @Override
      public void run() {
        instancia = Ouvinte.this;
        Ponte.registrar(Ouvinte.this, "Ouvindo as notificações.");
        esquecerVistasVelhas();
        // As notificações que já estavam lá: guarda os botões de responder e
        // repassa o que chegou com a ponte desligada
        try {
          final StatusBarNotification[] ativas = getActiveNotifications();
          if (ativas != null)
            for (StatusBarNotification s : ativas) processar(s, true);
        } catch (Exception e) {
          Ponte.registrar(Ouvinte.this, "Erro ao ler as notificações abertas: " + e);
        }
      }
    });
  }

  // (API 24; nas versões antigas não é chamado: lá vale o onDestroy)
  public void onListenerDisconnected() {
    naMao(new Runnable() {
      @Override
      public void run() {
        if (instancia == Ouvinte.this) instancia = null;
        Ponte.registrar(Ouvinte.this, "Parou de ouvir as notificações (o Android desligou a ponte?).");
      }
    });
  }

  @Override
  public void onDestroy() {
    if (instancia == this) instancia = null;
    // O que estava juntando vai agora, em vez de se perder
    for (Pacote p : new ArrayList<>(fila.values())) {
      mao.removeCallbacks(p.envio);
      mandar(p);
    }
    fila.clear();
    super.onDestroy();
  }

  @Override
  public void onNotificationPosted(final StatusBarNotification sbn) {
    naMao(new Runnable() {
      @Override
      public void run() {
        try {
          processar(sbn, false);
        } catch (Exception e) {
          Ponte.registrar(Ouvinte.this, "Erro ao ler uma notificação: " + e);
        }
      }
    });
  }

  @Override
  public void onNotificationRemoved(StatusBarNotification sbn) {
    // nada: o botão de responder guardado continua valendo
  }

  // ----------------------------------------------------------------------
  // Do WhatsApp para o Tijolo
  // ----------------------------------------------------------------------

  /** @param aoLigar a notificação já estava aberta quando a ponte ligou */
  private void processar(StatusBarNotification sbn, boolean aoLigar) {
    if (!Ponte.ehWhatsApp(sbn.getPackageName())) return;
    final Notification n = sbn.getNotification();
    if (n == null || n.extras == null) return;
    if ((n.flags & Notification.FLAG_GROUP_SUMMARY) != 0) return;   // o resumo "3 conversas"
    if ((n.flags & Notification.FLAG_ONGOING_EVENT) != 0) return;   // chamada em andamento, "procurando mensagens"
    final Bundle x = n.extras;
    final boolean grupo = ehGrupo(x);
    final String tituloCru = tituloDa(x, grupo);
    if (tituloCru.isEmpty()) return;
    final Notification.Action responder = acaoDeResponder(n);
    final boolean chamada = responder == null && ehChamadaPerdida(n, x);
    if (responder == null && !chamada) return;      // "WhatsApp Web está ativo" e outras do sistema

    final String titulo = Texto.limpar(tituloCru, Ponte.MAXIMO_TITULO);
    String chave = chaveDa(sbn, n, tituloCru);
    if (chamada && chave.startsWith("titulo:")) {   // a chamada perdida vai para a conversa da pessoa
      final String daConversa = conversas.chavePorTitulo(titulo);
      if (daConversa != null) chave = daConversa;
    }
    final int numero = conversas.numero(chave, titulo, System.currentTimeMillis());
    if (responder != null) respostas.put(numero, new Botao(chave, responder));

    // As mensagens novas desta conversa
    final List<Novidades.Msg> todas = mensagensDa(n, x, tituloCru, chamada);
    if (todas.isEmpty()) return;
    final SharedPreferences pv = Ponte.vistas(this);
    final Set<String> vistas = Novidades.lerMarcas(pv.getString(chave, null));
    final long desde = aoLigar ? System.currentTimeMillis() - Ponte.JANELA_AO_LIGAR_MS : 0;
    final List<Novidades.Msg> novas = Novidades.novas(todas, vistas, eu(x), desde);
    pv.edit().putString(chave, Novidades.juntar(vistas, todas)).apply();
    if (novas.isEmpty()) return;
    // Pausada ("WA-"), grupo com os grupos desligados, conversa silenciada:
    // só anota que já viu
    if (!Ponte.ligada(this)) return;
    if (grupo && !Ponte.grupos(this)) return;
    if (Ponte.silenciada(this, chave)) return;
    for (Novidades.Msg m : novas) {
      String linha = Texto.limpar(m.texto, Ponte.MAXIMO_TEXTO);
      if (grupo && m.autor != null) linha = Protocolo.umaLinha(Texto.limpar(m.autor, Ponte.MAXIMO_TITULO)) + ": " + linha;
      enfileirar(numero, titulo, grupo, linha);
    }
  }

  // Mensagens seguidas da mesma conversa saem juntas, numa SMS só
  private void enfileirar(final int numero, String titulo, boolean grupo, String linha) {
    final long agora = SystemClock.uptimeMillis();
    Pacote p = fila.get(numero);
    if (p != null && tamanho(p) + linha.length() + 1 > Ponte.MAXIMO_TEXTO) {   // não cabe: vai o que já tem
      mao.removeCallbacks(p.envio);
      fila.remove(numero);
      mandar(p);
      p = null;
    }
    if (p == null) {
      final Pacote novo = new Pacote(numero, agora);
      novo.envio = new Runnable() {
        @Override
        public void run() {
          if (fila.get(numero) == novo) fila.remove(numero);
          mandar(novo);
        }
      };
      fila.put(numero, novo);
      p = novo;
    }
    p.titulo = titulo;
    p.linhas.add(linha);
    mao.removeCallbacks(p.envio);
    final long espera = grupo ? Ponte.ESPERA_GRUPO_MS : Ponte.ESPERA_MS;
    final long maximo = grupo ? Ponte.ESPERA_GRUPO_MAX_MS : Ponte.ESPERA_MAX_MS;
    mao.postAtTime(p.envio, Math.min(agora + espera, p.inicio + maximo));
  }

  private static int tamanho(Pacote p) {
    int t = 0;
    for (String l : p.linhas) t += l.length() + 1;
    return t;
  }

  private void mandar(Pacote p) {
    final StringBuilder corpo = new StringBuilder();
    for (String l : p.linhas) {
      if (corpo.length() > 0) corpo.append('\n');
      corpo.append(l);
    }
    final String sms = Protocolo.mensagem(p.numero, p.titulo, Texto.cortar(corpo.toString(), Ponte.MAXIMO_TEXTO));
    Ponte.enviarSms(this, Ponte.numeroTijolo(this), sms);
  }

  // Esquece as marcas das conversas que saíram da tabela
  private void esquecerVistasVelhas() {
    final SharedPreferences pv = Ponte.vistas(this);
    final SharedPreferences.Editor e = pv.edit();
    boolean mudou = false;
    for (String chave : pv.getAll().keySet()) {
      if (!conversas.conhece(chave)) {
        e.remove(chave);
        mudou = true;
      }
    }
    if (mudou) e.apply();
  }

  // ----------------------------------------------------------------------
  // Do Tijolo para o WhatsApp
  // ----------------------------------------------------------------------

  /** Responde na conversa `numero`. Devolve null se deu certo, ou o motivo (para o Tijolo). */
  public static String responder(Context c, int numero, String texto) {
    final Ouvinte o = instancia;
    if (o == null) return "a ponte não está ouvindo as notificações. Abra o app Ponte do Tijolo no Android.";
    return o.responderAqui(numero, texto);
  }

  private String responderAqui(int numero, String texto) {
    final String chave = conversas.chave(numero);
    if (chave == null) return "não conheço a conversa " + numero + ". Mande WA? para ver a lista.";
    final String titulo = conversas.titulo(numero);
    final String quem = titulo != null && !titulo.isEmpty() ? titulo : "a conversa " + numero;
    final Botao b = respostas.get(numero);
    Notification.Action a = b != null && b.chave.equals(chave) ? b.acao : null;
    if (a == null) a = procurarResposta(chave);
    for (int tentativa = 0; a != null && tentativa < 2; tentativa++) {
      try {
        final RemoteInput[] entradas = a.getRemoteInputs();
        final Intent i = new Intent();
        final Bundle r = new Bundle();
        for (RemoteInput e : entradas) r.putCharSequence(e.getResultKey(), texto);
        RemoteInput.addResultsToIntent(entradas, i, r);
        a.actionIntent.send(this, 0, i);
        respostas.put(numero, new Botao(chave, a));
        conversas.numero(chave, titulo, System.currentTimeMillis());
        Ponte.registrar(this, "← Tijolo para " + quem + ": " + Ponte.resumo(texto));
        return null;
      } catch (PendingIntent.CanceledException e) {
        // O WhatsApp cancelou este botão: tenta o da notificação aberta, se houver outro
        respostas.remove(numero);
        final Notification.Action outra = procurarResposta(chave);
        a = outra != a ? outra : null;
      } catch (Exception e) {
        return "erro ao responder a " + quem + ": " + e.getMessage();
      }
    }
    return "não consigo responder a " + quem + " agora: a ponte não tem a notificação dessa conversa. " +
        "Quando chegar outra mensagem dela, a resposta funciona.";
  }

  // O botão de responder nas notificações que estão abertas agora
  private Notification.Action procurarResposta(String chave) {
    try {
      final StatusBarNotification[] ativas = getActiveNotifications();
      if (ativas == null) return null;
      for (StatusBarNotification s : ativas) {
        if (!Ponte.ehWhatsApp(s.getPackageName())) continue;
        final Notification n = s.getNotification();
        if (n == null || n.extras == null) continue;
        final String t = tituloDa(n.extras, ehGrupo(n.extras));
        if (!chaveDa(s, n, t).equals(chave)) continue;
        final Notification.Action a = acaoDeResponder(n);
        if (a != null) return a;
      }
    } catch (Exception e) {
      Ponte.registrar(this, "Erro ao procurar a notificação: " + e);
    }
    return null;
  }

  // ----------------------------------------------------------------------
  // Lendo a notificação
  // ----------------------------------------------------------------------

  private static final Pattern CONTAGEM = Pattern.compile("\\s*\\(\\d+ [^)]*\\)\\s*$");   // " (3 mensagens)"
  private static final Pattern CHAMADA_PERDIDA =
      Pattern.compile("(chamada|liga[cç][aã]o).*(perdida|n[aã]o atendida)|missed.*call", Pattern.CASE_INSENSITIVE);

  private static boolean ehGrupo(Bundle x) {
    if (x.containsKey("android.isGroupConversation")) return x.getBoolean("android.isGroupConversation", false);
    return x.getCharSequence("android.conversationTitle") != null;
  }

  private static String tituloDa(Bundle x, boolean grupo) {
    CharSequence t = grupo ? x.getCharSequence("android.conversationTitle") : null;
    if (t == null) t = x.getCharSequence("android.title");
    if (t == null) return "";
    return CONTAGEM.matcher(Protocolo.umaLinha(t.toString())).replaceAll("").trim();
  }

  private static String eu(Bundle x) {
    final CharSequence e = x.getCharSequence("android.selfDisplayName");
    return e == null ? null : e.toString();
  }

  // O que identifica a conversa: o atalho dela (API 26+: o id do contato ou
  // grupo no WhatsApp); senão a etiqueta da notificação; senão o título
  private static String chaveDa(StatusBarNotification s, Notification n, String titulo) {
    try {
      final Object id = Notification.class.getMethod("getShortcutId").invoke(n);
      if (id != null && id.toString().length() > 0) return id.toString();
    } catch (Exception ignorada) {
      // Android antigo
    }
    final String tag = s.getTag();
    if (tag != null && tag.contains("@")) return tag;
    return "titulo:" + titulo;
  }

  private static boolean temEntradaLivre(Notification.Action a) {
    final RemoteInput[] r = a == null ? null : a.getRemoteInputs();
    if (r == null || r.length == 0 || a.actionIntent == null) return false;
    for (RemoteInput x : r)
      if (x.getAllowFreeFormInput()) return true;
    return false;
  }

  private static Notification.Action acaoDeResponder(Notification n) {
    if (n.actions != null)
      for (Notification.Action a : n.actions)
        if (temEntradaLivre(a)) return a;
    // Android 5 e 6: o "Responder" fica só na parte da notificação para relógios
    try {
      final Notification.WearableExtender w = new Notification.WearableExtender(n);
      for (Notification.Action a : w.getActions())
        if (temEntradaLivre(a)) return a;
    } catch (Exception ignorada) {
      // sem extensão para relógio
    }
    return null;
  }

  private static boolean ehChamadaPerdida(Notification n, Bundle x) {
    if ("missed_call".equals(n.category)) return true;
    final CharSequence t = x.getCharSequence("android.text");
    return t != null && CHAMADA_PERDIDA.matcher(t).find();
  }

  private static List<Novidades.Msg> mensagensDa(Notification n, Bundle x, String titulo, boolean chamada) {
    final List<Novidades.Msg> v = new ArrayList<>();
    final Parcelable[] ms = chamada ? null : x.getParcelableArray("android.messages");
    if (ms != null && ms.length > 0) {
      for (Parcelable p : ms) {
        if (!(p instanceof Bundle)) continue;
        final Bundle b = (Bundle) p;
        final CharSequence t = b.getCharSequence("text");
        CharSequence autor = b.getCharSequence("sender");
        if (autor == null) autor = nomeDaPessoa(b.get("sender_person"));
        v.add(new Novidades.Msg(b.getLong("time"), autor == null ? null : autor.toString(),
            t == null ? null : t.toString()));
      }
      return v;
    }
    // Sem a lista de mensagens (Android antigo, chamada perdida): o texto da notificação
    CharSequence t = x.getCharSequence("android.bigText");
    if (t == null) t = x.getCharSequence("android.text");
    if (t == null) return v;
    final String texto = chamada ? "[" + Protocolo.umaLinha(t.toString()) + "]" : t.toString();
    v.add(new Novidades.Msg(n.when, titulo, texto));
    return v;
  }

  // android.app.Person (API 28): o nome por reflexão (o app é compilado para a API 23)
  private static CharSequence nomeDaPessoa(Object p) {
    if (p == null) return null;
    try {
      final Object nome = p.getClass().getMethod("getName").invoke(p);
      return nome instanceof CharSequence ? (CharSequence) nome : null;
    } catch (Exception e) {
      return null;
    }
  }
}
