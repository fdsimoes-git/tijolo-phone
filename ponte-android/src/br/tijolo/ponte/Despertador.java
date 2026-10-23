package br.tijolo.ponte;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/**
 * Os alarmes do app (não é exportado: só o próprio app agenda):
 *  - SAIDA: hora de tentar de novo uma SMS da fila;
 *  - VIGIA: a cada 30 min, confere se a ponte ainda ouve o WhatsApp. Se
 *    não estiver ouvindo em duas vigias seguidas, avisa o Tijolo uma vez.
 */
public class Despertador extends BroadcastReceiver {
  @Override
  public void onReceive(Context c, Intent i) {
    if (Ponte.ACAO_VIGIA.equals(i.getAction())) vigiar(c);
    Ponte.esvaziarSaida(c);
  }

  static void vigiar(Context c) {
    if (Ouvinte.ouvindo()) {
      if (Ponte.opcoes(c).getInt("vigia_falhas", 0) != 0 || Ponte.opcoes(c).getBoolean("vigia_avisou", false)) {
        if (Ponte.opcoes(c).getBoolean("vigia_avisou", false))
          Ponte.enviar(c, Protocolo.aviso("A ponte voltou a ouvir o WhatsApp."));
        Ponte.opcoes(c).edit().putInt("vigia_falhas", 0).putBoolean("vigia_avisou", false).apply();
      }
      return;
    }
    Ouvinte.religar(c);
    final int falhas = Ponte.opcoes(c).getInt("vigia_falhas", 0) + 1;
    Ponte.opcoes(c).edit().putInt("vigia_falhas", falhas).apply();
    Ponte.registrar(c, "Vigia: a ponte não está ouvindo o WhatsApp (pedi para religar).");
    if (falhas >= 2 && !Ponte.opcoes(c).getBoolean("vigia_avisou", false)) {
      Ponte.opcoes(c).edit().putBoolean("vigia_avisou", true).apply();
      Ponte.enviar(c, Protocolo.aviso("A ponte parou de ouvir o WhatsApp. Abra o app Ponte do Tijolo no Android."));
    }
  }
}
