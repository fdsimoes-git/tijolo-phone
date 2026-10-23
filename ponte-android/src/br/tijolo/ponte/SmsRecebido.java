package br.tijolo.ponte;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.provider.Telephony;
import android.telephony.SmsMessage;

import java.util.ArrayList;
import java.util.List;

/**
 * SMS chegando no Android. As que vêm do Tijolo (o número com DDD) são
 * pedidos:
 *   "WA#7xk texto"     responder na conversa 7 (o código xk tem que bater)
 *   "WA?"              a lista das conversas recentes
 *   "WA-" / "WA+"      pausar / voltar a repassar tudo
 *   "WA-7xk" / "WA+7xk"  silenciar / reativar a conversa 7
 * As outras SMS ficam com o app de mensagens do Android, como sempre.
 */
public class SmsRecebido extends BroadcastReceiver {
  @Override
  public void onReceive(final Context c, Intent i) {
    if (!Telephony.Sms.Intents.SMS_RECEIVED_ACTION.equals(i.getAction())) return;
    final SmsMessage[] ms = Telephony.Sms.Intents.getMessagesFromIntent(i);
    if (ms == null || ms.length == 0 || ms[0] == null) return;
    final String de = ms[0].getOriginatingAddress();
    if (!Ponte.mesmoNumero(de, Ponte.numeroTijolo(c))) return;
    final StringBuilder b = new StringBuilder();
    for (SmsMessage m : ms)
      if (m != null && m.getMessageBody() != null) b.append(m.getMessageBody());
    final Protocolo.Pedido p = Protocolo.ler(b.toString());
    if (p.tipo == Protocolo.Pedido.Tipo.DESCONHECIDO) return;

    // A ponte pode estar acabando de ligar (o Android reabriu o app por
    // causa desta SMS): espera o ouvinte das notificações por até 8 s
    final PendingResult fim = goAsync();
    final Handler mao = new Handler(Looper.getMainLooper());
    mao.post(new Runnable() {
      private int tentativas = 0;

      @Override
      public void run() {
        if (p.tipo == Protocolo.Pedido.Tipo.RESPOSTA && !Ouvinte.ouvindo() && tentativas++ < 16) {
          mao.postDelayed(this, 500);
          return;
        }
        try {
          tratar(c, p);
        } catch (Exception e) {
          Ponte.registrar(c, "Erro com o pedido do Tijolo: " + e);
        } finally {
          fim.finish();
        }
      }
    });
  }

  private static void tratar(Context c, Protocolo.Pedido p) {
    final Conversas conversas = Ponte.conversas(c);
    switch (p.tipo) {
      case RESPOSTA: {
        final String erro = Ouvinte.responder(c, p.conversa, p.codigo, p.texto);
        if (erro != null) {
          Ponte.registrar(c, "Não respondi: " + erro);
          Ponte.enviar(c, Protocolo.aviso(primeiraMaiuscula(erro)));
        }
        break;
      }
      case LISTA: {
        final List<String[]> v = new ArrayList<>();
        for (Conversas.Entrada e : conversas.recentes(10)) {
          final String t = e.titulo + (Ponte.silenciada(c, e.chave) ? " (silenciada)" : "");
          v.add(new String[] {String.valueOf(e.numero), Protocolo.codigo(e.chave), t});
        }
        Ponte.enviar(c, Protocolo.lista(v));
        Ponte.registrar(c, "O Tijolo pediu a lista das conversas.");
        break;
      }
      case PAUSAR:
        Ponte.opcoes(c).edit().putBoolean("ligada", false).apply();
        Ponte.registrar(c, "Pausada pelo Tijolo (WA-).");
        Ponte.enviar(c, Protocolo.aviso("Ponte pausada: o WhatsApp não vem para o Tijolo até você mandar WA+"));
        break;
      case RETOMAR:
        Ponte.opcoes(c).edit().putBoolean("ligada", true).apply();
        Ponte.registrar(c, "Ligada de novo pelo Tijolo (WA+).");
        Ponte.enviar(c, Protocolo.aviso("Ponte ligada de novo."));
        break;
      case SILENCIAR:
      case REATIVAR: {
        final String chave = conversas.chave(p.conversa);
        if (chave == null) {
          Ponte.enviar(c, Protocolo.aviso("Não conheço a conversa " + p.conversa + ". Mande WA? para ver a lista."));
          break;
        }
        final String t = conversas.titulo(p.conversa);
        if (p.codigo != null && !p.codigo.equals(Protocolo.codigo(chave))) {
          Ponte.enviar(c, Protocolo.aviso("A conversa " + p.conversa + " agora é outra (" + t + "): não mexi nela."));
          break;
        }
        final boolean silenciar = p.tipo == Protocolo.Pedido.Tipo.SILENCIAR;
        Ponte.silenciar(c, chave, silenciar);
        Ponte.registrar(c, (silenciar ? "Silenciada pelo Tijolo: " : "Reativada pelo Tijolo: ") + t);
        Ponte.enviar(c, Protocolo.aviso(silenciar
            ? t + " silenciada: as mensagens dela não vêm mais (Reativar a conversa, no Tijolo, volta)."
            : t + " volta a vir para o Tijolo."));
        break;
      }
      default:
        break;
    }
  }

  private static String primeiraMaiuscula(String s) {
    return s.isEmpty() ? s : Character.toUpperCase(s.charAt(0)) + s.substring(1);
  }
}
