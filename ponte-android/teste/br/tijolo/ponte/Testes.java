package br.tijolo.ponte;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Testes da lógica da ponte no computador (sem Android):
 *   sh testar.sh
 */
public class Testes {
  private static int verificacoes = 0, falhas = 0;

  private static void confere(boolean ok, String oque) {
    verificacoes++;
    if (!ok) {
      falhas++;
      System.out.println("  FALHOU: " + oque);
    }
  }

  private static void igual(String esperado, String obtido, String oque) {
    confere(esperado.equals(obtido), oque + "\n    esperado [" + esperado + "]\n    obtido   [" + obtido + "]");
  }

  public static void main(String[] args) {
    System.out.println("1. protocolo: ponte -> Tijolo");
    igual("WA#7 Família\nJoão: bom dia", Protocolo.mensagem(7, "Família", "João: bom dia"), "mensagem");
    igual("WA#3 Maria Silva\noi", Protocolo.mensagem(3, " Maria\n Silva ", "oi"), "título numa linha");
    igual("WA! Teste", Protocolo.aviso("Teste"), "aviso");
    final List<String[]> l = new ArrayList<>();
    igual("WA? nenhuma conversa ainda", Protocolo.lista(l), "lista vazia");
    l.add(new String[] {"3", "Maria"});
    l.add(new String[] {"7", "Família"});
    igual("WA? #3 Maria\n#7 Família", Protocolo.lista(l), "lista");

    System.out.println("2. protocolo: Tijolo -> ponte");
    Protocolo.Pedido p = Protocolo.ler("WA#7 Chego às 8!");
    confere(p.tipo == Protocolo.Pedido.Tipo.RESPOSTA && p.conversa == 7 && p.texto.equals("Chego às 8!"), "resposta");
    p = Protocolo.ler("wa#12 linha 1\nlinha 2");
    confere(p.tipo == Protocolo.Pedido.Tipo.RESPOSTA && p.conversa == 12 && p.texto.equals("linha 1\nlinha 2"),
        "minúsculas e várias linhas");
    p = Protocolo.ler(" WA # 5  oi ");
    confere(p.tipo == Protocolo.Pedido.Tipo.RESPOSTA && p.conversa == 5 && p.texto.equals("oi"), "com espaços");
    confere(Protocolo.ler("WA#5").tipo == Protocolo.Pedido.Tipo.DESCONHECIDO, "resposta sem texto");
    confere(Protocolo.ler("WA#0 oi").tipo == Protocolo.Pedido.Tipo.DESCONHECIDO, "conversa 0 não existe");
    p = Protocolo.ler("WA#100 oi");
    confere(p.tipo == Protocolo.Pedido.Tipo.RESPOSTA && p.conversa == 100, "conversa 100 existe");
    confere(Protocolo.ler("WA#1000 oi").tipo == Protocolo.Pedido.Tipo.DESCONHECIDO, "conversa 1000 não existe");
    p = Protocolo.ler("WA-7");
    confere(p.tipo == Protocolo.Pedido.Tipo.SILENCIAR && p.conversa == 7, "silenciar");
    p = Protocolo.ler("wa + #12 ");
    confere(p.tipo == Protocolo.Pedido.Tipo.REATIVAR && p.conversa == 12, "reativar com espaços");
    confere(Protocolo.ler("WA-0").tipo == Protocolo.Pedido.Tipo.DESCONHECIDO, "silenciar a conversa 0");
    confere(Protocolo.ler("WA-7 oi").tipo == Protocolo.Pedido.Tipo.DESCONHECIDO, "silenciar com texto");
    confere(Protocolo.ler("WA?").tipo == Protocolo.Pedido.Tipo.LISTA, "lista");
    confere(Protocolo.ler("wa?").tipo == Protocolo.Pedido.Tipo.LISTA, "lista minúscula");
    confere(Protocolo.ler("WA-").tipo == Protocolo.Pedido.Tipo.PAUSAR, "pausar");
    confere(Protocolo.ler("WA+").tipo == Protocolo.Pedido.Tipo.RETOMAR, "retomar");
    confere(Protocolo.ler("Oi, tudo bem?").tipo == Protocolo.Pedido.Tipo.DESCONHECIDO, "SMS comum");
    confere(Protocolo.ler(null).tipo == Protocolo.Pedido.Tipo.DESCONHECIDO, "nada");

    System.out.println("3. texto para o Tijolo");
    igual("[foto] Foto", Texto.limpar("📷 Foto", 400), "foto");
    igual("[áudio] 0:12", Texto.limpar("🎤 0:12", 400), "áudio");
    igual("kkk :D :D", Texto.limpar("kkk 😂😂", 400), "risada");
    igual("Família", Texto.limpar("Família 🏠", 400), "emoji desconhecido no fim some");
    igual("Bom dia :)", Texto.limpar("Bom dia😀", 400), "sinal grudado ganha espaço");
    igual("te amo <3", Texto.limpar("te amo ❤️", 400), "coração com seletor de variação");
    igual("(ok)", Texto.limpar("👍🏽", 400), "joinha com tom de pele");
    igual("[emoji]", Texto.limpar("🦄", 400), "só um emoji desconhecido");
    igual("Ação, ç, ã, é, ü", Texto.limpar("Ação, ç, ã, é, ü", 400), "acentos ficam");
    igual("linha 1\nlinha 2\n\nlinha 3", Texto.limpar("linha 1\r\nlinha 2\n\n\n\nlinha 3  ", 400), "linhas");
    igual("a b", Texto.limpar("a​   b", 400), "invisíveis e espaços");
    igual("Пётр", Texto.limpar("Пётр", 400), "outras letras ficam");
    final String longo = Texto.limpar("abcdefghijklmnopqrstuvwxyz", 10);
    igual("abcdefg...", longo, "corte");
    igual("", Texto.limpar(null, 10), "nulo");
    confere(Texto.cortar("😀😀😀abc", 5).length() > 0, "corte sem partir caractere");

    System.out.println("4. números das conversas");
    final String[] guardado = {""};
    final Conversas.Guarda g = new Conversas.Guarda() {
      @Override
      public String ler() {
        return guardado[0];
      }

      @Override
      public void gravar(String c) {
        guardado[0] = c;
      }
    };
    Conversas c = new Conversas(g, 3);
    confere(c.numero("5511988887777@s.whatsapp.net", "Maria", 100) == 1, "primeira = 1");
    confere(c.numero("12036301@g.us", "Família", 200) == 2, "segunda = 2");
    confere(c.numero("5511988887777@s.whatsapp.net", "Maria Silva", 300) == 1, "a mesma conversa, o mesmo número");
    igual("Maria Silva", c.titulo(1), "o título acompanha o nome novo");
    c = new Conversas(g, 3);                     // o Android reiniciou
    confere(c.numero("12036301@g.us", "Família", 400) == 2, "guardado");
    igual("5511988887777@s.whatsapp.net", c.chave(1), "chave guardada");
    confere(c.numero("x@s.whatsapp.net", "X", 500) == 3, "terceira");
    // cheia (3): a usada há mais tempo (Maria, 300) cede o número
    confere(c.numero("y@s.whatsapp.net", "Y", 600) == 1, "cheia: reaproveita o 1");
    confere(c.chave(1).equals("y@s.whatsapp.net") && c.total() == 3, "a Maria saiu");
    final List<Conversas.Entrada> r = c.recentes(2);
    confere(r.size() == 2 && r.get(0).numero == 1 && r.get(1).numero == 3, "recentes, a mais nova primeiro");
    confere(c.numero(null, "Sem chave", 700) > 0 && c.chave(2).equals("Sem chave"), "sem chave: o título");
    guardado[0] = "lixo\n1\tk\tT\tabc\n2\tk2\tT2\t5\n";
    c = new Conversas(g, 3);
    confere(c.total() == 1 && "T2".equals(c.titulo(2)), "linhas estragadas ficam de fora");
    c.numero("tab\tchave", "tí\ttulo", 10);
    c = new Conversas(g, 3);
    confere(c.total() == 2, "tab na chave não estraga a tabela");
    confere(c.conhece("tab\tchave") && c.conhece("k2") && !c.conhece("k"), "conhece");
    c.numero("k3", "T2", 20);                    // outro com o mesmo título, usado depois
    igual("k3", c.chavePorTitulo("T2"), "pelo título: o usado mais recentemente");
    confere(c.chavePorTitulo("Ninguém") == null, "título desconhecido");

    System.out.println("5. mensagens novas de uma notificação");
    final long agora = 10_000_000;
    final Novidades.Msg antiga = new Novidades.Msg(agora - 3_600_000, "Maria", "antiga");
    final Novidades.Msg recente = new Novidades.Msg(agora - 60_000, "Maria", "recente");
    final Novidades.Msg minha = new Novidades.Msg(agora - 30_000, null, "minha resposta");
    final Novidades.Msg minha2 = new Novidades.Msg(agora - 20_000, "Você", "minha também");
    final Novidades.Msg nova = new Novidades.Msg(agora - 10_000, "Maria", "nova");
    final List<Novidades.Msg> todas = Arrays.asList(antiga, recente, minha, minha2, nova);
    // a ponte acabou de ligar: das conversas que não conhecia, só a última meia hora
    List<Novidades.Msg> n = Novidades.novas(todas, null, "Você", agora - 1_800_000);
    confere(n.size() == 2 && n.get(0) == recente && n.get(1) == nova,
        "ao ligar: só as da última meia hora, sem as minhas");
    // conversa nova chegando com a ponte ligada: todas (não lidas), menos as minhas
    n = Novidades.novas(todas, null, "Você", 0);
    confere(n.size() == 3 && n.get(0) == antiga, "conversa nova: todas as dos outros");
    // a notificação é refeita com mais uma: só ela vai
    String vistas = Novidades.juntar(null, todas);
    final Novidades.Msg outra = new Novidades.Msg(agora - 5_000, "Maria", "outra");
    n = Novidades.novas(Arrays.asList(recente, minha, minha2, nova, outra), Novidades.lerMarcas(vistas), "Você", 0);
    confere(n.size() == 1 && n.get(0) == outra, "só a que não foi");
    // chegou atrasada (hora mais antiga que as já vistas): vai também
    final Novidades.Msg atrasada = new Novidades.Msg(agora - 50_000, "Maria", "atrasada");
    n = Novidades.novas(Arrays.asList(recente, atrasada, nova), Novidades.lerMarcas(vistas), "Você", 0);
    confere(n.size() == 1 && n.get(0) == atrasada, "atrasada não se perde");
    // mesma hora e autor, texto diferente: são duas
    n = Novidades.novas(Arrays.asList(new Novidades.Msg(agora - 10_000, "Maria", "nova!")),
        Novidades.lerMarcas(vistas), "Você", 0);
    confere(n.size() == 1, "texto diferente, mensagem diferente");
    // a mesma notificação de novo (o WhatsApp reabriu): nada
    confere(Novidades.novas(todas, Novidades.lerMarcas(vistas), "Você", 0).isEmpty(), "nada novo");
    // as marcas têm limite: ficam as mais novas
    final List<Novidades.Msg> muitas = new ArrayList<>();
    for (int i = 0; i < Novidades.MARCAS + 10; i++) muitas.add(new Novidades.Msg(i, "Maria", "m" + i));
    vistas = Novidades.juntar(Novidades.lerMarcas(vistas), muitas);
    confere(Novidades.lerMarcas(vistas).size() == Novidades.MARCAS, "limite de marcas");
    n = Novidades.novas(muitas, Novidades.lerMarcas(vistas), null, 0);
    confere(n.size() == 10 && n.get(0).texto.equals("m0"), "as mais antigas saem primeiro");
    confere(Novidades.lerMarcas(null) == null && Novidades.lerMarcas("").isEmpty(), "nunca vista x vista vazia");
    n = Novidades.novas(Arrays.asList(new Novidades.Msg(agora, "Maria", "  ")), null, null, 0);
    confere(n.isEmpty(), "texto vazio não vai");

    System.out.println("\n" + verificacoes + " verificações, " + falhas + " falha(s)");
    System.exit(falhas == 0 ? 0 : 1);
  }
}
