package br.tijolo.ponte;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/** O resultado de cada parte de SMS mandada ao Tijolo: tira da fila, ou tenta de novo mais tarde. */
public class SmsEnviada extends BroadcastReceiver {
  @Override
  public void onReceive(Context c, Intent i) {
    final long id = i.getLongExtra("id", 0);
    final int tentativa = i.getIntExtra("tentativa", 0);
    final int partes = i.getIntExtra("partes", 1);
    final String resumo = i.getStringExtra("resumo");
    final boolean ok = getResultCode() == Activity.RESULT_OK;
    final long agora = Ponte.agora();
    final Saida s = Ponte.saida(c);
    final long esperaAntes = s.esperaAte();
    switch (s.resultado(id, tentativa, ok, agora)) {
      case SAIU:
        Ponte.registrar(c, "→ Tijolo: " + resumo + (partes > 1 ? " (" + partes + " partes)" : ""));
        break;
      case VAI_TENTAR_DE_NOVO: {
        final long quando = s.esperaAte();
        Ponte.registrar(c, "FALHOU a SMS (" + motivo(getResultCode()) + "): " + resumo +
            (quando > esperaAntes ? "; a fila tenta de novo às " + Ponte.hora(quando) : ""));
        break;
      }
      default:
        break;
    }
    Ponte.esvaziarSaida(c);
  }

  private static String motivo(int r) {
    switch (r) {
      case 1: return "falha genérica: crédito ou chip?";            // RESULT_ERROR_GENERIC_FAILURE
      case 2: return "rádio desligado (modo avião?)";               // RESULT_ERROR_RADIO_OFF
      case 3: return "erro no envio";                               // RESULT_ERROR_NULL_PDU
      case 4: return "sem sinal";                                   // RESULT_ERROR_NO_SERVICE
      case 5: return "limite de SMS do Android";                    // RESULT_ERROR_LIMIT_EXCEEDED
      default: return "código " + r;
    }
  }
}
