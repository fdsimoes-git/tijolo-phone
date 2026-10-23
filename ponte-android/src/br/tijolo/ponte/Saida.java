package br.tijolo.ponte;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.List;

/**
 * A caixa de saída das SMS para o Tijolo.
 *
 * Cada SMS fica guardada até o Android confirmar que todas as partes
 * saíram, e elas saem na ordem em que chegaram (os avisos da ponte passam
 * na frente). Se uma falhar (sem crédito, sem sinal), a fila inteira espera
 * 1, 2, 5, 10 e 30 minutos e depois de hora em hora, e recomeça pela mais
 * antiga; uma SMS que sai prova que o sinal voltou, e a fila anda de novo
 * na hora. Depois de 2 dias falhando, as SMS mais velhas que isso saem da
 * fila (e a ponte avisa quantas).
 *
 * E não manda mais que LIMITE_POR_MINUTO partes por minuto: acima de 30 o
 * Android segura as SMS do app e pergunta se pode, numa tela que ninguém
 * vê no Android da ponte.
 *
 * Só Java comum: os testes rodam no computador.
 */
public final class Saida {
  public interface Guarda {
    String ler();
    void gravar(String conteudo);   // de uma vez (commit), antes de voltar
  }

  public static final int LIMITE_POR_MINUTO = 20;
  static final long MINUTO = 60_000;
  static final long[] ESPERAS = {MINUTO, 2 * MINUTO, 5 * MINUTO, 10 * MINUTO, 30 * MINUTO, 60 * MINUTO};
  /** Sem o resultado de todas as partes em 15 min: conta como falha (o app morreu no meio, ou o Android sumiu com ela) */
  static final long SEM_RESPOSTA_MS = 15 * MINUTO;
  /** Desiste das SMS com mais de 2 dias, mas só com a fila falhando há um tempo (um relógio que pulou não basta) */
  static final long DESISTIR_MS = 48 * 60 * MINUTO;
  static final int FALHAS_PARA_DESISTIR = 8;
  /** Quantos textos mandados a ponte lembra (para não obedecer à própria SMS, se o número do Tijolo for o dela) */
  static final int ECOS = 50;

  public static final class Item {
    public final long id;
    public final String texto;
    public final long criada;
    public int tentativas;
    long enviadaEm;      // 0 = não está saindo agora
    int partes, partesOk;

    Item(long id, String texto, long criada) {
      this.id = id;
      this.texto = texto;
      this.criada = criada;
    }

    public boolean saindo() {
      return enviadaEm > 0;
    }
  }

  /** O que aconteceu com a SMS depois do resultado de uma parte */
  public enum Resultado { ESPERANDO, SAIU, VAI_TENTAR_DE_NOVO, DESCONHECIDA }

  private final Guarda guarda;
  private final List<Item> itens = new ArrayList<>();
  private long proximoId = 1;
  private long pausaAte = 0;              // a fila espera até aqui (depois de uma falha)
  private int falhasSeguidas = 0;
  private int descartadas = 0;            // tiradas por velhice, ainda não avisadas
  private final ArrayDeque<Long> partesRecentes = new ArrayDeque<>();
  private final LinkedHashSet<String> ecos = new LinkedHashSet<>();

  public Saida(Guarda guarda) {
    this.guarda = guarda;
    carregar();
  }

  /** No fim da fila (as mensagens do WhatsApp) */
  public synchronized long adicionar(String texto, long agora) {
    final Item i = new Item(proximoId++, texto, agora);
    itens.add(i);
    gravar();
    return i.id;
  }

  /** Na frente da fila (os avisos e respostas da ponte: "WA!", "WA?") */
  public synchronized long adicionarNaFrente(String texto, long agora) {
    final Item i = new Item(proximoId++, texto, agora);
    int p = 0;
    while (p < itens.size() && itens.get(p).saindo()) p++;
    itens.add(p, i);
    gravar();
    return i.id;
  }

  /** A próxima SMS para mandar agora (a primeira da fila que não está saindo), ou null. */
  public synchronized Item proxima(long agora) {
    boolean mudou = arrumarRelogio(agora);
    for (Item i : itens) {
      if (i.saindo() && agora - i.enviadaEm > SEM_RESPOSTA_MS) {   // ninguém respondeu: falhou
        falhou(i, agora);
        mudou = true;
      }
    }
    if (desistir(agora)) mudou = true;
    if (mudou) gravar();
    if (agora < pausaAte) return null;
    for (Item i : itens)
      if (!i.saindo()) return i;
    return null;
  }

  /** Cabem mais `partes` neste minuto? (uma SMS sozinha sempre cabe num minuto vazio) */
  public synchronized boolean cabe(int partes, long agora) {
    limparMinuto(agora);
    return partesRecentes.isEmpty() || partesRecentes.size() + partes <= LIMITE_POR_MINUTO;
  }

  /** Quando o limite por minuto libera espaço */
  public synchronized long quandoCabe(long agora) {
    limparMinuto(agora);
    return partesRecentes.isEmpty() ? agora : partesRecentes.peekFirst() + MINUTO;
  }

  public synchronized void enviando(Item i, int partes, long agora) {
    i.enviadaEm = agora;
    i.partes = partes;
    i.partesOk = 0;
    i.tentativas++;
    for (int p = 0; p < partes; p++) partesRecentes.addLast(agora);
    gravar();
  }

  /**
   * O Android disse como foi uma parte da tentativa `tentativa` da SMS `id`.
   * Resultado de uma tentativa velha (que já tinha contado como falha) não conta.
   */
  public synchronized Resultado resultado(long id, int tentativa, boolean ok, long agora) {
    final Item i = porId(id);
    if (i == null || !i.saindo() || i.tentativas != tentativa) return Resultado.DESCONHECIDA;
    if (!ok) {
      falhou(i, agora);
      gravar();
      return Resultado.VAI_TENTAR_DE_NOVO;
    }
    if (++i.partesOk < i.partes) {
      gravar();
      return Resultado.ESPERANDO;
    }
    itens.remove(i);
    lembrar(i.texto);
    // saiu: o sinal e o crédito estão bem, a fila não espera mais
    falhasSeguidas = 0;
    pausaAte = 0;
    gravar();
    return Resultado.SAIU;
  }

  /** "Tentar mandar a fila agora": esquece a espera */
  public synchronized void tentarAgora() {
    if (pausaAte != 0) {
      pausaAte = 0;
      gravar();
    }
  }

  /** Tira da fila as SMS (que não estão saindo) que começam com `prefixo`; devolve quantas */
  public synchronized int descartar(String prefixo) {
    int n = 0;
    for (Iterator<Item> it = itens.iterator(); it.hasNext();) {
      final Item i = it.next();
      if (!i.saindo() && i.texto.startsWith(prefixo)) {
        it.remove();
        n++;
      }
    }
    if (n > 0) gravar();
    return n;
  }

  /** Quantas saíram da fila por velhice desde a última vez (e zera) */
  public synchronized int tirarDescartadas() {
    final int n = descartadas;
    if (n > 0) {
      descartadas = 0;
      gravar();
    }
    return n;
  }

  /** A ponte mandou este texto há pouco? (uma SMS "do Tijolo" igual é a ponte falando com ela mesma) */
  public synchronized boolean mandouHaPouco(String texto) {
    return texto != null && ecos.contains(Novidades.hash(texto));
  }

  /** Quando há algo a fazer (mandar, ou conferir um envio sem resposta); 0 = nada pendente */
  public synchronized long proximaHora(long agora) {
    long r = 0;
    boolean esperando = false;
    for (Item i : itens) {
      if (i.saindo()) {
        final long t = i.enviadaEm + SEM_RESPOSTA_MS + 1;
        if (r == 0 || t < r) r = t;
      } else {
        esperando = true;
      }
    }
    if (esperando) {
      final long t = Math.max(agora, pausaAte);
      if (r == 0 || t < r) r = t;
    }
    return r;
  }

  /** Até quando a fila espera (0 = não espera) */
  public synchronized long esperaAte() {
    return pausaAte;
  }

  public synchronized int pendentes() {
    return itens.size();
  }

  public synchronized Item porId(long id) {
    for (Item i : itens)
      if (i.id == id) return i;
    return null;
  }

  private void falhou(Item i, long agora) {
    i.enviadaEm = 0;
    i.partesOk = 0;
    falhasSeguidas++;
    pausaAte = Math.max(pausaAte, agora + ESPERAS[Math.min(falhasSeguidas, ESPERAS.length) - 1]);
  }

  /** Depois de muitas falhas seguidas, tira as que estão há mais de 2 dias na fila. true se tirou alguma. */
  private boolean desistir(long agora) {
    if (falhasSeguidas < FALHAS_PARA_DESISTIR) return false;
    boolean tirou = false;
    for (Iterator<Item> it = itens.iterator(); it.hasNext();) {
      final Item i = it.next();
      if (!i.saindo() && agora - i.criada > DESISTIR_MS) {
        it.remove();
        descartadas++;
        tirou = true;
      }
    }
    return tirou;
  }

  /** O relógio voltou (ou a espera guardada é absurda): não deixa a fila parada no futuro */
  private boolean arrumarRelogio(long agora) {
    boolean mudou = false;
    if (pausaAte - agora > ESPERAS[ESPERAS.length - 1] + MINUTO) {
      pausaAte = agora;
      mudou = true;
    }
    for (Item i : itens) {
      if (i.saindo() && i.enviadaEm - agora > MINUTO) {
        i.enviadaEm = 0;
        i.partesOk = 0;
        mudou = true;
      }
    }
    return mudou;
  }

  private void limparMinuto(long agora) {
    for (Iterator<Long> it = partesRecentes.iterator(); it.hasNext();) {
      final long t = it.next();
      if (agora - t >= MINUTO || t - agora > MINUTO) it.remove();   // velha, ou no futuro (relógio voltou)
    }
  }

  private void lembrar(String texto) {
    final String h = Novidades.hash(texto);
    ecos.remove(h);
    ecos.add(h);
    while (ecos.size() > ECOS) {
      final Iterator<String> it = ecos.iterator();
      it.next();
      it.remove();
    }
  }

  // ---------- guardar ----------
  // "#", próximo id, espera da fila, falhas seguidas, descartadas
  // "m", as horas das partes do último minuto (o limite por minuto vale depois de reiniciar)
  // "e", os textos mandados há pouco (hashes)
  // uma SMS por linha: id, tentativas, criada, enviadaEm, partes, partesOk, texto (com \n e \t escapados)
  private void gravar() {
    final StringBuilder b = new StringBuilder();
    b.append("#\t").append(proximoId).append('\t').append(pausaAte).append('\t').append(falhasSeguidas)
        .append('\t').append(descartadas).append('\n');
    b.append("m\t");
    boolean primeiro = true;
    for (long t : partesRecentes) {
      if (!primeiro) b.append(',');
      b.append(t);
      primeiro = false;
    }
    b.append('\n').append("e\t");
    primeiro = true;
    for (String h : ecos) {
      if (!primeiro) b.append(',');
      b.append(h);
      primeiro = false;
    }
    b.append('\n');
    for (Item i : itens) {
      b.append(i.id).append('\t').append(i.tentativas).append('\t').append(i.criada).append('\t')
          .append(i.enviadaEm).append('\t').append(i.partes).append('\t').append(i.partesOk).append('\t')
          .append(escapar(i.texto)).append('\n');
    }
    guarda.gravar(b.toString());
  }

  private void carregar() {
    final String s = guarda.ler();
    if (s == null) return;
    for (String linha : s.split("\n")) {
      final String[] c = linha.split("\t", -1);
      try {
        if (c[0].equals("#")) {
          if (c.length > 1) proximoId = Math.max(proximoId, Long.parseLong(c[1]));
          if (c.length > 4) {
            pausaAte = Long.parseLong(c[2]);
            falhasSeguidas = Integer.parseInt(c[3]);
            descartadas = Integer.parseInt(c[4]);
          }
          continue;
        }
        if (c[0].equals("m") && c.length == 2) {
          for (String t : c[1].split(","))
            if (!t.isEmpty()) partesRecentes.addLast(Long.parseLong(t));
          continue;
        }
        if (c[0].equals("e") && c.length == 2) {
          for (String h : c[1].split(","))
            if (!h.isEmpty()) ecos.add(h);
          continue;
        }
        if (c.length != 7) continue;
        final Item i = new Item(Long.parseLong(c[0]), desescapar(c[6]), Long.parseLong(c[2]));
        i.tentativas = Integer.parseInt(c[1]);
        i.enviadaEm = Long.parseLong(c[3]);
        i.partes = Integer.parseInt(c[4]);
        i.partesOk = Integer.parseInt(c[5]);
        if (porId(i.id) == null) itens.add(i);
        proximoId = Math.max(proximoId, i.id + 1);
      } catch (NumberFormatException ignorada) {
        // linha estragada: fica de fora
      }
    }
  }

  static String escapar(String s) {
    return s.replace("\\", "\\\\").replace("\n", "\\n").replace("\t", "\\t");
  }

  static String desescapar(String s) {
    final StringBuilder b = new StringBuilder(s.length());
    for (int i = 0; i < s.length(); i++) {
      final char ch = s.charAt(i);
      if (ch == '\\' && i + 1 < s.length()) {
        final char n = s.charAt(++i);
        b.append(n == 'n' ? '\n' : n == 't' ? '\t' : n);
      } else {
        b.append(ch);
      }
    }
    return b.toString();
  }
}
