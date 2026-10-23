package br.tijolo.ponte;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/**
 * O Android acabou de ligar (com tela bloqueada, só depois de destravar a
 * primeira vez). Avisa o Tijolo, liga a vigia e manda o que ficou na fila.
 */
public class AoLigar extends BroadcastReceiver {
  @Override
  public void onReceive(Context c, Intent i) {
    if (!Intent.ACTION_BOOT_COMPLETED.equals(i.getAction())) return;
    Ponte.registrar(c, "O Android ligou.");
    Ponte.agendarVigia(c);
    if (!Ponte.numeroTijolo(c).isEmpty())
      Ponte.enviar(c, Protocolo.aviso("A ponte ligou de novo (o Android reiniciou)."));
    else Ponte.esvaziarSaida(c);
  }
}
