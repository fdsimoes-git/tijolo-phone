package br.tijolo.ponte;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * A caixa de saída das SMS para o Tijolo.
 *
 * Cada SMS fica guardada até o Android confirmar que todas as partes
 * saíram. Se falhar (sem crédito, sem sinal), tenta de novo mais tarde:
 * 1, 2, 5, 10 e 30 minutos, depois de hora em hora, por até 2 dias.
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
  /** Sem o resultado de todas as partes em 3 min: conta como falha */
  static final long SEM_RESPOSTA_MS = 3 * MINUTO;
  static final long DESISTIR_MS = 48 * 60 * MINUTO;

  public static final class Item {
    public final long id;
    public final String texto;
    public final long criada;
    public int tentativas;
    long proxima;        // quando pode tentar
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
  public enum Resultado { ESPERANDO, SAIU, VAI_TENTAR_DE_NOVO, DESISTIU, DESCONHECIDA }

  private final Guarda guarda;
  private final List<Item> itens = new ArrayList<>();
  private long proximoId = 1;
  private final ArrayDeque<Long> partesRecentes = new ArrayDeque<>();

  public Saida(Guarda guarda) {
    this.guarda = guarda;
    carregar();
  }

  public synchronized long adicionar(String texto, long agora) {
    final Item i = new Item(proximoId++, texto, agora);
    i.proxima = agora;
    itens.add(i);
    gravar();
    return i.id;
  }

  /** A próxima SMS para mandar agora (a mais antiga que pode), ou null. */
  public synchronized Item proxima(long agora) {
    boolean mudou = false;
    for (Iterator<Item> it = itens.iterator(); it.hasNext();) {
      final Item i = it.next();
      if (i.saindo() && agora - i.enviadaEm > SEM_RESPOSTA_MS) {   // ninguém respondeu: falhou
        falhou(i, agora);
        mudou = true;
      }
    }
    if (desistir(agora)) mudou = true;
    if (mudou) gravar();
    for (Item i : itens)
      if (!i.saindo() && i.proxima <= agora) return i;
    return null;
  }

  /** Cabem mais `partes` neste minuto? (uma SMS sozinha sempre cabe num minuto vazio) */
  public synchronized boolean cabe(int partes, long agora) {
    while (!partesRecentes.isEmpty() && agora - partesRecentes.peekFirst() >= MINUTO) partesRecentes.pollFirst();
    return partesRecentes.isEmpty() || partesRecentes.size() + partes <= LIMITE_POR_MINUTO;
  }

  /** Quando o limite por minuto libera espaço */
  public synchronized long quandoCabe(long agora) {
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

  /** O Android disse como foi uma parte. */
  public synchronized Resultado resultado(long id, boolean ok, long agora) {
    final Item i = porId(id);
    if (i == null || !i.saindo()) return Resultado.DESCONHECIDA;
    if (!ok) {
      falhou(i, agora);
      final boolean desistiu = desistir(agora);
      gravar();
      return desistiu && porId(id) == null ? Resultado.DESISTIU : Resultado.VAI_TENTAR_DE_NOVO;
    }
    if (++i.partesOk < i.partes) {
      gravar();
      return Resultado.ESPERANDO;
    }
    itens.remove(i);
    gravar();
    return Resultado.SAIU;
  }

  /** Quando há algo a fazer (mandar ou conferir um envio sem resposta); 0 = nada pendente */
  public synchronized long proximaHora(long agora) {
    long r = 0;
    for (Item i : itens) {
      final long t = i.saindo() ? i.enviadaEm + SEM_RESPOSTA_MS + 1 : i.proxima;
      if (r == 0 || t < r) r = t;
    }
    return r;
  }

  public synchronized int pendentes() {
    return itens.size();
  }

  public synchronized Item porId(long id) {
    for (Item i : itens)
      if (i.id == id) return i;
    return null;
  }

  public synchronized long esperaAtual(long id) {
    final Item i = porId(id);
    return i == null ? 0 : i.proxima;
  }

  private void falhou(Item i, long agora) {
    i.enviadaEm = 0;
    i.partesOk = 0;
    i.proxima = agora + ESPERAS[Math.min(Math.max(i.tentativas, 1), ESPERAS.length) - 1];
  }

  /** Tira as que estão há mais de 2 dias tentando. true se tirou alguma. */
  private boolean desistir(long agora) {
    boolean tirou = false;
    for (Iterator<Item> it = itens.iterator(); it.hasNext();) {
      final Item i = it.next();
      if (!i.saindo() && agora - i.criada > DESISTIR_MS) {
        it.remove();
        tirou = true;
      }
    }
    return tirou;
  }

  // ---------- guardar: uma SMS por linha ----------
  // id, tentativas, próxima, criada, enviadaEm, partes, partesOk, texto (com \n e \t escapados)
  private void gravar() {
    final StringBuilder b = new StringBuilder();
    b.append("#\t").append(proximoId).append('\n');
    for (Item i : itens) {
      b.append(i.id).append('\t').append(i.tentativas).append('\t').append(i.proxima).append('\t')
          .append(i.criada).append('\t').append(i.enviadaEm).append('\t').append(i.partes).append('\t')
          .append(i.partesOk).append('\t').append(escapar(i.texto)).append('\n');
    }
    guarda.gravar(b.toString());
  }

  private void carregar() {
    final String s = guarda.ler();
    if (s == null) return;
    for (String linha : s.split("\n")) {
      final String[] c = linha.split("\t", -1);
      try {
        if (c.length == 2 && c[0].equals("#")) {
          proximoId = Math.max(proximoId, Long.parseLong(c[1]));
          continue;
        }
        if (c.length != 8) continue;
        final Item i = new Item(Long.parseLong(c[0]), desescapar(c[7]), Long.parseLong(c[3]));
        i.tentativas = Integer.parseInt(c[1]);
        i.proxima = Long.parseLong(c[2]);
        i.enviadaEm = Long.parseLong(c[4]);
        i.partes = Integer.parseInt(c[5]);
        i.partesOk = Integer.parseInt(c[6]);
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
