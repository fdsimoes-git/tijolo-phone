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

  private static void igual(Object esperado, Object obtido, String oque) {
    confere(String.valueOf(esperado).equals(String.valueOf(obtido)),
        oque + "\n    esperado [" + esperado + "]\n    obtido   [" + obtido + "]");
  }

  static final class Memoria implements Conversas.Guarda, Saida.Guarda {
    String conteudo = "";
    int gravacoes = 0;

    @Override
    public String ler() {
      return conteudo;
    }

    @Override
    public void gravar(String c) {
      conteudo = c;
      gravacoes++;
    }
  }

  public static void main(String[] args) {
    System.out.println("1. protocolo: ponte -> Tijolo");
    igual("WA#7xk Família\nJoão: bom dia", Protocolo.mensagem(7, "xk", "Família", "João: bom dia"), "mensagem");
    igual("WA#3ab Maria Silva\noi", Protocolo.mensagem(3, "ab", " Maria\n Silva ", "oi"), "título numa linha");
    igual("WA! Teste", Protocolo.aviso("Teste"), "aviso");
    final List<String[]> l = new ArrayList<>();
    igual("WA? nenhuma conversa ainda", Protocolo.lista(l), "lista vazia");
    l.add(new String[] {"3", "ab", "Maria"});
    l.add(new String[] {"7", "xk", "Família"});
    igual("WA? #3ab Maria\n#7xk Família", Protocolo.lista(l), "lista");
    final String k1 = Protocolo.codigo("5511911112222@s.whatsapp.net");
    confere(k1.matches("[a-z]{2}") && k1.equals(Protocolo.codigo("5511911112222@s.whatsapp.net")), "código: 2 letras, sempre o mesmo");
    int iguais = 0;
    for (int i = 0; i < 200; i++)
      if (Protocolo.codigo("55119" + i + "@s.whatsapp.net").equals(Protocolo.codigo("55119" + (i + 1) + "@s.whatsapp.net"))) iguais++;
    confere(iguais < 5, "chaves diferentes, códigos (quase sempre) diferentes: " + iguais);

    System.out.println("2. protocolo: Tijolo -> ponte");
    Protocolo.Pedido p = Protocolo.ler("WA#7xk Chego às 8!");
    confere(p.tipo == Protocolo.Pedido.Tipo.RESPOSTA && p.conversa == 7 && "xk".equals(p.codigo) && p.texto.equals("Chego às 8!"), "resposta");
    p = Protocolo.ler("wa#12AB linha 1\nlinha 2");
    confere(p.tipo == Protocolo.Pedido.Tipo.RESPOSTA && p.conversa == 12 && "ab".equals(p.codigo) && p.texto.equals("linha 1\nlinha 2"),
        "minúsculas e várias linhas");
    p = Protocolo.ler(" WA # 5  oi ");
    confere(p.tipo == Protocolo.Pedido.Tipo.RESPOSTA && p.conversa == 5 && p.codigo == null && p.texto.equals("oi"), "sem código");
    confere(Protocolo.ler("WA#5xk").tipo == Protocolo.Pedido.Tipo.DESCONHECIDO, "resposta sem texto");
    confere(Protocolo.ler("WA#7xkoi").tipo == Protocolo.Pedido.Tipo.DESCONHECIDO, "código grudado no texto");
    confere(Protocolo.ler("WA#0xk oi").tipo == Protocolo.Pedido.Tipo.DESCONHECIDO, "conversa 0 não existe");
    p = Protocolo.ler("WA#100ab oi");
    confere(p.tipo == Protocolo.Pedido.Tipo.RESPOSTA && p.conversa == 100, "conversa 100 existe");
    confere(Protocolo.ler("WA#1000 oi").tipo == Protocolo.Pedido.Tipo.DESCONHECIDO, "conversa 1000 não existe");
    p = Protocolo.ler("WA-7xk");
    confere(p.tipo == Protocolo.Pedido.Tipo.SILENCIAR && p.conversa == 7 && "xk".equals(p.codigo), "silenciar");
    p = Protocolo.ler("wa + #12 ");
    confere(p.tipo == Protocolo.Pedido.Tipo.REATIVAR && p.conversa == 12 && p.codigo == null, "reativar sem código");
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
    igual("[audio] 0:12", Texto.limpar("🎤 0:12", 400), "áudio, sem acento (SMS mais barato)");
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
    igual("abc", Texto.limpar("a\u0000b\u001Bc\u0085", 400), "caracteres de controle somem");
    igual("Maria", Texto.limpar("𝓜𝓪𝓻𝓲𝓪", 400), "letras enfeitadas viram comuns");
    igual("maria", Texto.limpar("ｍａｒｉａ", 400), "letras largas viram comuns");
    igual("café", Texto.limpar("café", 400), "acento solto se junta");
    igual("abcdefg...", Texto.limpar("abcdefghijklmnopqrstuvwxyz", 10), "corte");
    igual("", Texto.limpar(null, 10), "nulo");
    igual("ab...", Texto.cortar("ab😀cdefgh", 6), "corte sem partir um caractere de 2 unidades");
    confere(Texto.cortar("x".repeat(400), 330).length() == 330, "corte em 330 unidades");

    System.out.println("4. números das conversas");
    Memoria g = new Memoria();
    Conversas c = new Conversas(g, 3, 1);
    igual(1, c.numero("5511988887777@s.whatsapp.net", "Maria", 100), "primeira = 1");
    igual(2, c.numero("12036301@g.us", "Família", 200), "segunda = 2");
    igual(1, c.numero("5511988887777@s.whatsapp.net", "Maria Silva", 300), "a mesma conversa, o mesmo número");
    igual("Maria Silva", c.titulo(1), "o título acompanha o nome novo");
    c.numero("5511988887777@s.whatsapp.net", "", 310);
    igual("Maria Silva", c.titulo(1), "título vazio não apaga o nome");
    c = new Conversas(g, 3, 1);                  // o Android reiniciou
    igual(2, c.numero("12036301@g.us", "Família", 400), "guardado");
    igual("5511988887777@s.whatsapp.net", c.chave(1), "chave guardada");
    igual(3, c.numero("x@s.whatsapp.net", "X", 500), "terceira");
    // cheia (3): a usada há mais tempo (Maria, 300) cede o número
    igual(1, c.numero("y@s.whatsapp.net", "Y", 600), "cheia: reaproveita o 1");
    confere(c.chave(1).equals("y@s.whatsapp.net") && c.total() == 3, "a Maria saiu");
    igual(Protocolo.codigo("y@s.whatsapp.net"), c.codigo(1), "o código vem da chave");
    confere(c.codigo(99) == null, "sem código para conversa que não existe");
    final List<Conversas.Entrada> r = c.recentes(2);
    confere(r.size() == 2 && r.get(0).numero == 1 && r.get(1).numero == 3, "recentes, a mais nova primeiro");
    igual(2, c.numero(null, "Sem chave", 700), "sem chave: pelo título (e a Família cede o 2)");
    igual("titulo:Sem chave", c.chave(2), "a chave do título");
    g.conteudo = "lixo\n1\tk\tT\tabc\n2\tk2\tT2\t5\n";
    c = new Conversas(g, 3, 1);
    confere(c.total() == 1 && "T2".equals(c.titulo(2)), "linhas estragadas ficam de fora");
    c.numero("tab\tchave", "tí\ttulo", 10);
    c = new Conversas(g, 3, 1);
    confere(c.total() == 2, "tab na chave não estraga a tabela");
    confere(c.conhece("tab\tchave") && c.conhece("k2") && !c.conhece("k"), "conhece");
    c.numero("k3", "T2", 20);
    igual("k3", c.chavePorTitulo("T2"), "pelo título: o usado mais recentemente");
    confere(c.chavePorTitulo("Ninguém") == null, "título desconhecido");
    igual(20, c.usoDe("k3"), "quando foi usada");
    // os números andam para a frente, também depois de reiniciar
    final Memoria g2 = new Memoria();
    Conversas c2 = new Conversas(g2, 999, 50);
    igual(50, c2.numero("a", "A", 1), "começa onde mandaram");
    igual(51, c2.numero("b", "B", 2), "e anda");
    c2 = new Conversas(g2, 999, 1);
    igual(52, c2.numero("c", "C", 3), "depois de reiniciar, continua de onde parou");
    final Memoria g3 = new Memoria();
    final Conversas c3 = new Conversas(g3, 3, 3);
    confere(c3.numero("a", "A", 1) == 3 && c3.numero("b", "B", 2) == 1 && c3.numero("c", "C", 3) == 2, "dá a volta");
    final int n0 = new Conversas(new Memoria()).numero("a", "A", 1);
    confere(n0 >= 1 && n0 <= Protocolo.MAX_CONVERSA, "a tabela nova começa num número sorteado: " + n0);
    // só grava quando muda
    final Memoria g4 = new Memoria();
    final Conversas c4 = new Conversas(g4, 999, 1);
    c4.numero("a", "A", 1000);
    final int antes = g4.gravacoes;
    c4.numero("a", "A", 2000);
    igual(antes, g4.gravacoes, "a mesma conversa, logo depois: não regrava");
    c4.numero("a", "A", 2000 + Conversas.REGRAVAR_USO_MS + 1);
    igual(antes + 1, g4.gravacoes, "uma hora depois: regrava");

    System.out.println("5. mensagens novas de uma notificação");
    final long agora = 10_000_000;
    final Novidades.Msg antiga = new Novidades.Msg(agora - 3_600_000, "Maria", "antiga");
    final Novidades.Msg recente = new Novidades.Msg(agora - 60_000, "Maria", "recente");
    final Novidades.Msg minha = new Novidades.Msg(agora - 30_000, null, "minha resposta");
    final Novidades.Msg minha2 = new Novidades.Msg(agora - 20_000, "Você", "minha também");
    final Novidades.Msg nova = new Novidades.Msg(agora - 10_000, "Maria", "nova");
    final List<Novidades.Msg> todas = Arrays.asList(antiga, recente, minha, minha2, nova);
    List<Novidades.Msg> n = Novidades.novas(todas, null, null, "Você", agora - 1_800_000);
    confere(n.size() == 2 && n.get(0) == recente && n.get(1) == nova, "ao ligar: só as da última meia hora, sem as minhas");
    n = Novidades.novas(todas, null, null, "Você", 0);
    confere(n.size() == 3 && n.get(0) == antiga, "conversa nova: todas as dos outros");
    String vistas = Novidades.juntar(null, todas, Novidades.MARCAS);
    final Novidades.Msg outra = new Novidades.Msg(agora - 5_000, "Maria", "outra");
    n = Novidades.novas(Arrays.asList(recente, minha, minha2, nova, outra), Novidades.lerMarcas(vistas), null, "Você", 0);
    confere(n.size() == 1 && n.get(0) == outra, "só a que não foi");
    final Novidades.Msg atrasada = new Novidades.Msg(agora - 50_000, "Maria", "atrasada");
    n = Novidades.novas(Arrays.asList(recente, atrasada, nova), Novidades.lerMarcas(vistas), null, "Você", 0);
    confere(n.size() == 1 && n.get(0) == atrasada, "atrasada não se perde");
    n = Novidades.novas(Arrays.asList(new Novidades.Msg(agora - 10_000, "Maria", "nova!")),
        Novidades.lerMarcas(vistas), null, "Você", 0);
    confere(n.size() == 1, "texto diferente, mensagem diferente");
    confere(Novidades.novas(todas, Novidades.lerMarcas(vistas), null, "Você", 0).isEmpty(), "nada novo");
    n = Novidades.novas(Arrays.asList(new Novidades.Msg(agora - 10_000, "Maria Silva", "nova")),
        Novidades.lerMarcas(vistas), null, "Você", 0);
    confere(n.isEmpty(), "o contato mudou de nome: a mesma mensagem não volta");
    n = Novidades.novas(Arrays.asList(nova, outra), null, Novidades.lerMarcas(vistas), "Você", 0);
    confere(n.size() == 1 && n.get(0) == outra, "conversa com chave nova: as já repassadas (em outra chave) não voltam");
    final List<Novidades.Msg> muitas = new ArrayList<>();
    for (int i = 0; i < Novidades.MARCAS + 10; i++) muitas.add(new Novidades.Msg(i, "Maria", "m" + i));
    vistas = Novidades.juntar(Novidades.lerMarcas(vistas), muitas, Novidades.MARCAS);
    confere(Novidades.lerMarcas(vistas).size() == Novidades.MARCAS, "limite de marcas");
    n = Novidades.novas(muitas, Novidades.lerMarcas(vistas), null, null, 0);
    confere(n.size() == 10 && n.get(0).texto.equals("m0"), "as mais antigas saem primeiro");
    confere(Novidades.lerMarcas(null) == null && Novidades.lerMarcas("").isEmpty(), "nunca vista x vista vazia");
    n = Novidades.novas(Arrays.asList(new Novidades.Msg(agora, "Maria", "  ")), null, null, null, 0);
    confere(n.isEmpty(), "texto vazio não vai");
    confere(Novidades.minha(new Novidades.Msg(1, null, "x"), "Você") && Novidades.minha(new Novidades.Msg(1, "você", "x"), "Você"),
        "sem autor, ou com o seu nome: é sua");
    confere(!Novidades.minha(new Novidades.Msg(1, "", "x"), "Você"), "autor sem nome (o Android 15 escondeu): não é sua");
    igual("Maria", Novidades.outroMaisRecente(todas, "Você"), "o outro: o autor da mais recente que não é sua");
    confere(Novidades.outroMaisRecente(Arrays.asList(minha, minha2), "Você") == null, "só as suas: ninguém");

    System.out.println("6. a caixa de saída");
    final Memoria gs = new Memoria();
    Saida s = new Saida(gs);
    final long t = 1_000_000;
    final long id = s.adicionar("WA#7xk Maria\nlinha 1\nlinha\t2 \\ fim", t);
    confere(s.pendentes() == 1, "na fila");
    Saida.Item it = s.proxima(t);
    confere(it != null && it.id == id, "pronta para sair");
    s.enviando(it, 2, t);
    confere(s.proxima(t) == null, "saindo: não sai de novo");
    igual(Saida.Resultado.ESPERANDO, s.resultado(id, true, t + 1000), "uma parte foi");
    igual(Saida.Resultado.SAIU, s.resultado(id, true, t + 2000), "as duas: saiu");
    confere(s.pendentes() == 0, "e saiu da fila");
    igual(Saida.Resultado.DESCONHECIDA, s.resultado(id, true, t + 3000), "resultado repetido: nada");
    // falha: tenta de novo em 1, 2, 5 min...
    final long id2 = s.adicionar("WA! teste", t);
    s.enviando(s.proxima(t), 1, t);
    igual(Saida.Resultado.VAI_TENTAR_DE_NOVO, s.resultado(id2, false, t + 1000), "falhou");
    confere(s.proxima(t + 30_000) == null, "espera 1 minuto");
    it = s.proxima(t + 61_001);
    confere(it != null && it.id == id2, "e tenta de novo");
    s.enviando(it, 1, t + 61_001);
    s.resultado(id2, false, t + 62_000);
    confere(s.proxima(t + 62_000 + 61_000) == null && s.proxima(t + 62_000 + 121_000) != null, "depois, 2 minutos");
    // sem resposta do Android em 3 min: conta como falha
    s = new Saida(new Memoria());
    final long id3 = s.adicionar("x", t);
    s.enviando(s.proxima(t), 1, t);
    confere(s.proxima(t + Saida.SEM_RESPOSTA_MS) == null, "esperando o resultado");
    confere(s.proxima(t + Saida.SEM_RESPOSTA_MS + 1) == null, "sem resposta: falhou, espera 1 min");
    confere(s.proxima(t + Saida.SEM_RESPOSTA_MS + 1 + 60_000) != null, "e tenta de novo");
    igual(Saida.Resultado.DESCONHECIDA, s.resultado(id3, true, t + 1), "resultado atrasado de uma tentativa velha: ignorado");
    // guardada: sobrevive a reiniciar, com o texto inteiro
    final Saida s2 = new Saida(gs);
    final Saida.Item i2 = s2.porId(id2);
    confere(i2 != null && i2.tentativas == 2, "guardada, com as tentativas");
    final long id4 = s2.adicionar("a\nb\tc\\d", t);
    igual("a\nb\tc\\d", new Saida(gs).porId(id4).texto, "texto com quebras, tabs e barras");
    confere(new Saida(gs).adicionar("y", t) > id4, "os números continuam");
    // limite por minuto
    s = new Saida(new Memoria());
    confere(s.cabe(6, t), "minuto vazio");
    for (int k = 0; k < 3; k++) s.enviando(new Saida.Item(100 + k, "x", t), 6, t);
    confere(!s.cabe(6, t + 10_000) && s.cabe(2, t + 10_000), "18 partes: cabem só mais 2");
    igual(t + 60_000, s.quandoCabe(t + 10_000), "libera um minuto depois");
    confere(s.cabe(6, t + 60_000), "um minuto depois, cabe");
    // desiste depois de 2 dias
    s = new Saida(new Memoria());
    s.adicionar("velha", t);
    confere(s.proxima(t + Saida.DESISTIR_MS + 1) == null && s.pendentes() == 0, "2 dias tentando: sai da fila");
    // a próxima hora a fazer algo
    s = new Saida(new Memoria());
    confere(s.proximaHora(t) == 0, "fila vazia: nada a fazer");
    s.adicionar("z", t + 5000);
    igual(t + 5000, s.proximaHora(t), "a próxima");

    System.out.println("\n" + verificacoes + " verificações, " + falhas + " falha(s)");
    System.exit(falhas == 0 ? 0 : 1);
  }
}
