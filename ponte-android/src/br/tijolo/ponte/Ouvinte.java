package br.tijolo.ponte;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.RemoteInput;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Parcelable;
import android.os.PowerManager;
import android.os.SystemClock;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Pattern;

/**
 * Ouve as notificações do Android. Quando chega mensagem do WhatsApp, manda
 * para o Tijolo por SMS ("WA#7xk Maria\nChego às 8"); e guarda o botão
 * "Responder" de cada conversa, que é por onde a resposta do Tijolo volta.
 *
 * O Android mantém este serviço ligado enquanto o app tiver o "acesso às
 * notificações" (Configurações): é ele que deixa a ponte sempre de pé.
 *
 * Tudo aqui roda na linha principal do app, uma coisa de cada vez (o
 * Android 5 e 6 chamam o ouvinte de outra linha: naMao() traz para ela).
 *
 * Uma mensagem só é dada como repassada quando a SMS dela entra na caixa
 * de saída (Saida, gravada na hora). Se o app morrer antes, a notificação
 * continua aberta e a mensagem vai quando a ponte voltar.
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
    final String chave;
    final boolean grupo;
    final long inicio;
    String titulo = "";
    final List<String> linhas = new ArrayList<>();
    final List<String> marcas = new ArrayList<>();
    Runnable envio;

    Pacote(int numero, String chave, boolean grupo, long inicio) {
      this.numero = numero;
      this.chave = chave;
      this.grupo = grupo;
      this.inicio = inicio;
    }
  }

  private final Handler mao = new Handler(Looper.getMainLooper());
  private final Map<Integer, Pacote> fila = new HashMap<>();
  /** As marcas das mensagens que estão na fila (ainda não gravadas), por conversa */
  private final Map<String, Set<String>> naFila = new HashMap<>();
  private LinkedHashSet<String> geral;
  private Conversas conversas;
  private PowerManager.WakeLock trava;

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
        Ponte.agendarVigia(Ouvinte.this);
        esquecerVistasVelhas();
        // As notificações que já estavam lá: guarda os botões de responder e
        // repassa o que chegou com a ponte desligada (ou que ficou na fila)
        try {
          final StatusBarNotification[] ativas = getActiveNotifications();
          if (ativas != null)
            for (StatusBarNotification s : ativas) processar(s, true);
        } catch (Exception e) {
          Ponte.registrar(Ouvinte.this, "Erro ao ler as notificações abertas: " + e);
        }
        Ponte.esvaziarSaida(Ouvinte.this);
      }
    });
  }

  // (API 24; nas versões antigas não é chamado: lá vale o onDestroy)
  public void onListenerDisconnected() {
    naMao(new Runnable() {
      @Override
      public void run() {
        if (instancia == Ouvinte.this) instancia = null;
        Ponte.registrar(Ouvinte.this, "Parou de ouvir as notificações; pedi ao Android para religar.");
        religar(Ouvinte.this);
      }
    });
  }

  @Override
  public void onDestroy() {
    if (instancia == this) instancia = null;
    // O que estava juntando vai agora, em vez de esperar
    for (Pacote p : new ArrayList<>(fila.values())) {
      mao.removeCallbacks(p.envio);
      mandar(p);
    }
    fila.clear();
    soltar();
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

  /** Android 7+: pede ao sistema para ligar o ouvinte de novo */
  static void religar(Context c) {
    try {
      NotificationListenerService.class.getMethod("requestRebind", ComponentName.class)
          .invoke(null, new ComponentName(c, Ouvinte.class));
    } catch (Exception ignorada) {
      // Android antigo: só desligando e ligando o acesso às notificações
    }
  }

  // ---------- trava: o Android não dorme com mensagem esperando na fila ----------
  private void segurar() {
    try {
      if (trava == null) {
        final PowerManager pm = (PowerManager) getSystemService(POWER_SERVICE);
        if (pm == null) return;
        trava = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "tijolo:fila");
        trava.setReferenceCounted(false);
      }
      trava.acquire(Ponte.ESPERA_GRUPO_MAX_MS + 15_000);
    } catch (Exception e) {
      Ponte.registrar(this, "Sem a trava de energia: " + e);
    }
  }

  private void soltar() {
    if (trava != null && trava.isHeld()) trava.release();
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
    diagnosticar(sbn, n, x);
    final Notification.Action responder = acaoDeResponder(n);
    final boolean chamada = responder == null && ehChamadaPerdida(n, x);
    if (responder == null && !chamada) return;      // "WhatsApp Web está ativo" e outras do sistema

    final String eu = eu(x);
    String chave = chaveDa(sbn, n);
    final boolean grupo = ehGrupo(x) || (chave != null && chave.endsWith("@g.us"));
    String tituloCru = tituloDa(x, grupo);
    final List<Novidades.Msg> todas = mensagensDa(n, x, tituloCru, chamada);
    // Numa conversa a dois, o título é o nome de quem escreveu por último:
    // depois da sua resposta vira o seu ("Você"). E o Android 15 troca por
    // "WhatsApp" quando esconde uma mensagem com código. Nos dois casos, o
    // nome certo é o da última mensagem que não é sua.
    if (!grupo && (mesmoNome(tituloCru, eu) || Ponte.ehNomeDoApp(tituloCru))) {
      final String outro = Novidades.outroMaisRecente(todas, eu);
      tituloCru = outro != null && !Ponte.ehNomeDoApp(outro) ? outro : "";
    }
    if (Ponte.ehNomeDoApp(tituloCru)) tituloCru = "";
    // Foto de grupo sem os sinais de grupo ("João @ Família"): o título não é o do grupo
    if (grupo && chave != null && x.getCharSequence("android.conversationTitle") == null) tituloCru = "";
    if (chave == null) {
      if (tituloCru.isEmpty()) return;               // não dá para saber de que conversa é
      chave = "titulo:" + tituloCru;
      if (chamada) {                                 // a chamada perdida vai para a conversa da pessoa
        final String daConversa = conversas.chavePorTitulo(Texto.limpar(tituloCru, Ponte.MAXIMO_TITULO));
        if (daConversa != null) chave = daConversa;
      }
    }
    final String titulo = tituloCru.isEmpty() ? "" : Texto.limpar(tituloCru, Ponte.MAXIMO_TITULO);
    final long agora = Ponte.agora();
    final int numero = conversas.numero(chave, titulo, agora);
    if (responder != null) respostas.put(numero, new Botao(chave, responder));
    if (todas.isEmpty()) return;

    // As mensagens novas desta conversa
    final SharedPreferences pv = Ponte.vistas(this);
    final Set<String> gravadas = Novidades.lerMarcas(pv.getString(chave, null));
    Set<String> vistas = gravadas;
    final Set<String> esperando = naFila.get(chave);
    if (esperando != null) {
      vistas = gravadas == null ? new LinkedHashSet<String>() : new LinkedHashSet<>(gravadas);
      vistas.addAll(esperando);
    }
    final long desde = aoLigar ? agora - Ponte.JANELA_AO_LIGAR_MS : 0;
    final List<Novidades.Msg> novas = Novidades.novas(todas, vistas, geral(), eu, desde);
    // Pausada ("WA-"), grupo com os grupos desligados, conversa silenciada:
    // só anota que já viu
    final boolean repassar = Ponte.ligada(this) && !(grupo && !Ponte.grupos(this)) && !Ponte.silenciada(this, chave);
    final List<String> vistasAgora = new ArrayList<>();
    for (Novidades.Msg m : todas)
      if (!repassar || !novas.contains(m)) vistasAgora.add(Novidades.marca(m));
    if (!vistasAgora.isEmpty()) gravarMarcas(chave, vistasAgora);
    if (!repassar) return;
    for (Novidades.Msg m : novas) {
      String linha = m.texto;
      if (grupo && m.autor != null && !m.autor.trim().isEmpty())
        linha = Protocolo.umaLinha(Texto.limpar(m.autor, Ponte.MAXIMO_TITULO)) + ": " + linha;
      enfileirar(numero, chave, titulo, grupo, Texto.limpar(linha, Ponte.MAXIMO_TEXTO), Novidades.marca(m));
    }
  }

  private static boolean mesmoNome(String a, String eu) {
    return a != null && eu != null && !eu.trim().isEmpty() && a.trim().equalsIgnoreCase(eu.trim());
  }

  // Mensagens seguidas da mesma conversa saem juntas, numa SMS só
  private void enfileirar(final int numero, String chave, String titulo, boolean grupo, String linha, String marca) {
    final long agora = SystemClock.uptimeMillis();
    Pacote p = fila.get(numero);
    if (p != null && tamanho(p) + linha.length() + 1 > Ponte.MAXIMO_TEXTO) {   // não cabe: vai o que já tem
      mao.removeCallbacks(p.envio);
      fila.remove(numero);
      mandar(p);
      p = null;
    }
    if (p == null) {
      final Pacote novo = new Pacote(numero, chave, grupo, agora);
      novo.envio = new Runnable() {
        @Override
        public void run() {
          if (fila.get(numero) == novo) fila.remove(numero);
          mandar(novo);
          if (fila.isEmpty()) soltar();
        }
      };
      fila.put(numero, novo);
      p = novo;
    }
    if (!titulo.isEmpty()) p.titulo = titulo;
    p.linhas.add(linha);
    p.marcas.add(marca);
    Set<String> esperando = naFila.get(chave);
    if (esperando == null) {
      esperando = new LinkedHashSet<>();
      naFila.put(chave, esperando);
    }
    esperando.add(marca);
    segurar();
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
    // Pausada ou silenciada enquanto juntava: não vai, mas fica como vista
    final boolean repassar = Ponte.ligada(this) && !Ponte.silenciada(this, p.chave) && !(p.grupo && !Ponte.grupos(this));
    if (repassar) {
      final StringBuilder corpo = new StringBuilder();
      for (String l : p.linhas) {
        if (corpo.length() > 0) corpo.append('\n');
        corpo.append(l);
      }
      String titulo = p.titulo;
      if (titulo.isEmpty()) titulo = conversas.titulo(p.numero);
      if (titulo == null || titulo.isEmpty()) titulo = "Conversa " + p.numero;
      Ponte.enviar(this, Protocolo.mensagem(p.numero, Protocolo.codigo(p.chave), titulo,
          Texto.cortar(corpo.toString(), Ponte.MAXIMO_TEXTO)));
    }
    // Primeiro a SMS na caixa de saída, depois as marcas: se o app morrer no
    // meio, no máximo a mensagem vai duas vezes, nunca nenhuma
    gravarMarcas(p.chave, p.marcas);
    final Set<String> esperando = naFila.get(p.chave);
    if (esperando != null) {
      esperando.removeAll(p.marcas);
      if (esperando.isEmpty()) naFila.remove(p.chave);
    }
  }

  // ---------- as marcas das mensagens já tratadas ----------
  private LinkedHashSet<String> geral() {
    if (geral == null) {
      final Set<String> g = Novidades.lerMarcas(Ponte.vistas(this).getString("*geral", ""));
      geral = g == null ? new LinkedHashSet<String>() : new LinkedHashSet<>(g);
    }
    return geral;
  }

  private void gravarMarcas(String chave, Collection<String> marcas) {
    final SharedPreferences pv = Ponte.vistas(this);
    final Set<String> antes = Novidades.lerMarcas(pv.getString(chave, null));
    final LinkedHashSet<String> r = antes == null ? new LinkedHashSet<String>() : new LinkedHashSet<>(antes);
    final LinkedHashSet<String> g = geral();
    for (String k : marcas) {
      r.remove(k);
      r.add(k);
      g.remove(k);
      g.add(k);
    }
    final String gs = Novidades.juntarMarcas(g, Novidades.MARCAS_GERAL);
    geral = new LinkedHashSet<>(Novidades.lerMarcas(gs));
    pv.edit().putString(chave, Novidades.juntarMarcas(r, Novidades.MARCAS)).putString("*geral", gs).apply();
  }

  // Esquece as marcas das conversas que saíram da tabela ou estão paradas há 2 semanas
  private void esquecerVistasVelhas() {
    final SharedPreferences pv = Ponte.vistas(this);
    final SharedPreferences.Editor e = pv.edit();
    final long limite = Ponte.agora() - 14L * 24 * 3600 * 1000;
    boolean mudou = false;
    for (String chave : pv.getAll().keySet()) {
      if (chave.equals("*geral")) continue;
      if (!conversas.conhece(chave) || conversas.usoDe(chave) < limite) {
        e.remove(chave);
        mudou = true;
      }
    }
    if (mudou) e.apply();
  }

  // ---------- diagnóstico: o que o WhatsApp põe nas notificações deste Android ----------
  private void diagnosticar(StatusBarNotification sbn, Notification n, Bundle x) {
    final SharedPreferences op = Ponte.opcoes(this);
    final int resta = op.getInt("diagnostico", 0);
    if (resta <= 0) return;
    op.edit().putInt("diagnostico", resta - 1).apply();
    String atalho = null;
    try {
      final Object a = Notification.class.getMethod("getShortcutId").invoke(n);
      atalho = a == null ? null : a.toString();
    } catch (Exception ignorada) {
      // Android antigo
    }
    final Parcelable[] ms = x.getParcelableArray("android.messages");
    Ponte.registrar(this, "diag: tag=" + sbn.getTag() + " id=" + sbn.getId() + " atalho=" + atalho +
        " título=" + x.getCharSequence("android.title") + " conversa=" + x.getCharSequence("android.conversationTitle") +
        " grupo=" + x.get("android.isGroupConversation") + " eu=" + x.getCharSequence("android.selfDisplayName") +
        " msgs=" + (ms == null ? "-" : ms.length) + " cat=" + n.category +
        " ações=" + (n.actions == null ? 0 : n.actions.length) + " flags=" + Integer.toHexString(n.flags));
  }

  // ----------------------------------------------------------------------
  // Do Tijolo para o WhatsApp
  // ----------------------------------------------------------------------

  /** Responde na conversa `numero`. Devolve null se deu certo, ou o motivo (para o Tijolo). */
  public static String responder(Context c, int numero, String codigo, String texto) {
    final Ouvinte o = instancia;
    if (o == null) return "a ponte não está ouvindo as notificações. Abra o app Ponte do Tijolo no Android.";
    return o.responderAqui(numero, codigo, texto);
  }

  private String responderAqui(int numero, String codigo, String texto) {
    final String chave = conversas.chave(numero);
    if (chave == null) return "não conheço a conversa " + numero + ". Mande WA? para ver a lista.";
    final String titulo = conversas.titulo(numero);
    final String quem = titulo != null && !titulo.isEmpty() ? titulo : "a conversa " + numero;
    if (codigo == null) return "a resposta veio sem o código da conversa: responda pela própria mensagem, no Tijolo.";
    if (!codigo.equals(Protocolo.codigo(chave)))
      return "a conversa " + numero + " agora é outra (" + quem + "), então a resposta não foi, para não ir à " +
          "pessoa errada. Responda numa mensagem mais nova.";
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
        conversas.numero(chave, null, Ponte.agora());
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
        String k = chaveDa(s, n);
        if (k == null) {
          final String t = tituloDa(n.extras, ehGrupo(n.extras));
          if (t.isEmpty()) continue;
          k = "titulo:" + t;
        }
        if (!k.equals(chave)) continue;
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

  // " (3 mensagens)" no fim do título (e não "(2019 formandos)", que é parte do nome)
  private static final Pattern CONTAGEM = Pattern.compile(
      "\\s*\\(\\d+ (mensagens?|novas mensagens|messages?|new messages|mensajes?|nuevos mensajes)\\)\\s*$",
      Pattern.CASE_INSENSITIVE);
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
  // grupo no WhatsApp); senão a etiqueta da notificação, se for um id do
  // WhatsApp ("...@s.whatsapp.net", "...@g.us"); senão null (aí vale o título)
  private static String chaveDa(StatusBarNotification s, Notification n) {
    try {
      final Object id = Notification.class.getMethod("getShortcutId").invoke(n);
      if (id != null && id.toString().length() > 0) return id.toString();
    } catch (Exception ignorada) {
      // Android antigo
    }
    final String tag = s.getTag();
    if (tag != null && tag.contains("@")) return tag;
    return null;
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

  // android.app.Person (API 28): o nome por reflexão (o app é compilado para a API 23).
  // Sem nome ("", o Android 15 escondendo uma mensagem): "" (alguém), não null (você)
  private static CharSequence nomeDaPessoa(Object p) {
    if (p == null) return null;
    try {
      final Object nome = p.getClass().getMethod("getName").invoke(p);
      return nome instanceof CharSequence ? (CharSequence) nome : "";
    } catch (Exception e) {
      return "";
    }
  }
}
