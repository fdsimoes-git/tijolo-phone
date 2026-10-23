package br.tijolo.ponte;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/** O resultado de cada parte de SMS mandada ao Tijolo, para o registro. */
public class SmsEnviada extends BroadcastReceiver {
  @Override
  public void onReceive(Context c, Intent i) {
    final int parte = i.getIntExtra("parte", 1), partes = i.getIntExtra("partes", 1);
    if (parte != partes && getResultCode() == Activity.RESULT_OK) return;   // só a última, se deu certo
    final String resumo = i.getStringExtra("resumo");
    if (getResultCode() == Activity.RESULT_OK) {
      Ponte.registrar(c, "→ Tijolo: " + resumo + (partes > 1 ? " (" + partes + " partes)" : ""));
    } else {
      Ponte.registrar(c, "FALHOU a SMS (" + motivo(getResultCode()) + "): " + resumo);
    }
  }

  private static String motivo(int r) {
    switch (r) {
      case 1: return "falha genérica: crédito ou chip?";            // RESULT_ERROR_GENERIC_FAILURE
      case 2: return "rádio desligado (modo avião?)";               // RESULT_ERROR_RADIO_OFF
      case 3: return "erro no envio";                               // RESULT_ERROR_NULL_PDU
      case 4: return "sem sinal";                                   // RESULT_ERROR_NO_SERVICE
      default: return "código " + r;
    }
  }
}
