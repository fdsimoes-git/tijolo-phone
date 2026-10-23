package br.tijolo.ponte;

import android.Manifest;
import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.PowerManager;
import android.provider.Settings;
import android.text.InputType;
import android.util.TypedValue;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

/** A única tela do app: o número do Tijolo, as permissões, as opções e o registro. */
public class Principal extends Activity {
  private TextView estado, restrito, lista, registro, fila;
  private EditText numero;
  private CheckBox ligada, grupos;
  private LinearLayout coluna;

  @Override
  protected void onCreate(Bundle salvo) {
    super.onCreate(salvo);
    final ScrollView rolagem = new ScrollView(this);
    coluna = new LinearLayout(this);
    coluna.setOrientation(LinearLayout.VERTICAL);
    final int m = dp(16);
    coluna.setPadding(m, m, m, m);
    rolagem.addView(coluna);

    final TextView titulo = texto("Ponte do Tijolo", 24);
    titulo.setTypeface(Typeface.DEFAULT_BOLD);
    texto("As mensagens do WhatsApp deste Android vão para o Tijolo por SMS, e as respostas do Tijolo voltam " +
        "para o WhatsApp. Deixe este Android ligado, no carregador e no Wi-Fi.", 15);

    secao("1. O número do Tijolo");
    numero = new EditText(this);
    numero.setInputType(InputType.TYPE_CLASS_PHONE);
    numero.setHint("ex.: 11 98888-7777");
    numero.setText(Ponte.numeroTijolo(this));
    coluna.addView(numero);
    botao("Guardar o número", new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        final String d = Ponte.arrumarNumero(numero.getText().toString());
        if (d == null) {
          Toast.makeText(Principal.this, "Escreva o número com DDD.", Toast.LENGTH_LONG).show();
          return;
        }
        Ponte.opcoes(Principal.this).edit().putString("tijolo", d).apply();
        numero.setText(d);
        Ponte.registrar(Principal.this, "Número do Tijolo: " + d);
        atualizar();
      }
    });

    secao("2. Permissões");
    estado = texto("", 15);
    restrito = texto("Android 13 ou mais novo: se aparecer \"Configuração restrita\", toque em \"Informações do " +
        "app\" abaixo, no menu ⋮ (canto de cima) escolha \"Permitir configurações restritas\" e volte aqui.", 14);
    botao("Permitir SMS", new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        if (Build.VERSION.SDK_INT >= 23)
          requestPermissions(new String[] {Manifest.permission.SEND_SMS, Manifest.permission.RECEIVE_SMS}, 1);
      }
    });
    botao("Acesso às notificações", new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        abrir(new Intent("android.settings.ACTION_NOTIFICATION_LISTENER_SETTINGS"));
      }
    });
    botao("Não economizar bateria", new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        if (!abrir(new Intent("android.settings.REQUEST_IGNORE_BATTERY_OPTIMIZATIONS",
            Uri.parse("package:" + getPackageName()))))
          abrir(new Intent("android.settings.IGNORE_BATTERY_OPTIMIZATION_SETTINGS"));
      }
    });
    botao("Informações do app", new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        abrir(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:" + getPackageName())));
      }
    });

    secao("3. Opções");
    ligada = caixa("Ponte ligada (pelo Tijolo: WA- pausa, WA+ liga)", "ligada", true);
    grupos = caixa("Repassar também os grupos (juntando até 1 minuto de mensagens por SMS)", "grupos", false);
    botao("Mandar uma SMS de teste ao Tijolo", new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        if (Ponte.numeroTijolo(Principal.this).isEmpty()) {
          Toast.makeText(Principal.this, "Falta o número do Tijolo.", Toast.LENGTH_LONG).show();
          return;
        }
        Ponte.enviar(Principal.this, Protocolo.aviso("Teste da ponte: se chegou, o caminho Android -> Tijolo funciona."));
        Toast.makeText(Principal.this, "Mandando...", Toast.LENGTH_SHORT).show();
        atualizar();
      }
    });
    fila = texto("", 14);
    botao("Tentar mandar a fila agora", new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        Ponte.esvaziarSaida(Principal.this);
        atualizar();
      }
    });
    botao("Diagnóstico: anotar as próximas 10 notificações do WhatsApp", new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        Ponte.opcoes(Principal.this).edit().putInt("diagnostico", 10).apply();
        Toast.makeText(Principal.this, "Elas aparecem no registro, embaixo.", Toast.LENGTH_LONG).show();
      }
    });

    secao("Conversas (o número que vai na SMS)");
    lista = texto("", 14);

    secao("Últimos acontecimentos");
    registro = texto("", 12);
    registro.setTypeface(Typeface.MONOSPACE);

    setContentView(rolagem);
  }

  @Override
  protected void onResume() {
    super.onResume();
    Ponte.agendarVigia(this);
    atualizar();
  }

  @Override
  public void onRequestPermissionsResult(int codigo, String[] permissoes, int[] resultados) {
    atualizar();
  }

  private void atualizar() {
    final StringBuilder b = new StringBuilder();
    final String t = Ponte.numeroTijolo(this);
    linha(b, !t.isEmpty(), t.isEmpty() ? "Falta o número do Tijolo" : "Tijolo: " + t);
    final boolean wa = whatsappInstalado();
    linha(b, wa, wa ? "WhatsApp instalado" : "WhatsApp não encontrado neste Android");
    final boolean sms = permitido(Manifest.permission.SEND_SMS) && permitido(Manifest.permission.RECEIVE_SMS);
    linha(b, sms, sms ? "SMS liberado" : "Falta liberar o SMS (botão abaixo)");
    final boolean acesso = acessoNotificacoes();
    linha(b, acesso, acesso ? "Acesso às notificações liberado" : "Falta o acesso às notificações (botão abaixo)");
    if (acesso) {
      final boolean ouvindo = Ouvinte.ouvindo();
      if (!ouvindo) Ouvinte.religar(this);
      linha(b, ouvindo, ouvindo ? "Ouvindo o WhatsApp agora"
          : "Ainda não está ouvindo. Se continuar assim, desligue e ligue de novo o acesso às notificações.");
    }
    final boolean bateria = semEconomia();
    linha(b, bateria, bateria ? "Sem economia de bateria para a ponte"
        : "O Android pode fechar a ponte para economizar bateria (botão abaixo)");
    if (!mantemPermissoes())
      linha(b, false, "O Android pode tirar as permissões se o app ficar sem uso: em \"Informações do app\", " +
          "desligue \"Remover permissões se o app não for usado\" / \"Pausar atividade no app\"");
    estado.setText(b.toString().trim());
    final int n = Ponte.saida(this).pendentes();
    fila.setText(n == 0 ? "Fila de SMS para o Tijolo: vazia."
        : "Fila de SMS para o Tijolo: " + n + " esperando (sem sinal, sem crédito ou no limite por minuto).");
    restrito.setVisibility(Build.VERSION.SDK_INT >= 33 && !(sms && acesso) ? View.VISIBLE : View.GONE);
    ligada.setChecked(Ponte.ligada(this));
    grupos.setChecked(Ponte.grupos(this));

    final StringBuilder l = new StringBuilder();
    for (Conversas.Entrada e : Ponte.conversas(this).recentes(30)) {
      l.append('#').append(e.numero).append(Protocolo.codigo(e.chave)).append("  ").append(e.titulo);
      if (Ponte.silenciada(this, e.chave)) l.append("  (silenciada: WA+").append(e.numero).append(" reativa)");
      l.append('\n');
    }
    lista.setText(l.length() == 0 ? "Nenhuma ainda: aparecem quando chega mensagem no WhatsApp." : l.toString().trim());
    registro.setText(Ponte.registro(this));
  }

  private static void linha(StringBuilder b, boolean ok, String texto) {
    b.append(ok ? "✓ " : "✗ ").append(texto).append('\n');
  }

  // Antes do Android 6 as permissões vêm todas na instalação
  private boolean permitido(String permissao) {
    return Build.VERSION.SDK_INT < 23 || checkSelfPermission(permissao) == PackageManager.PERMISSION_GRANTED;
  }

  private boolean semEconomia() {
    if (Build.VERSION.SDK_INT < 23) return true;     // sem "Soneca" (Doze) antes do Android 6
    final PowerManager pm = (PowerManager) getSystemService(POWER_SERVICE);
    return pm != null && pm.isIgnoringBatteryOptimizations(getPackageName());
  }

  // Android 11+: "remover permissões se o app não for usado" (por reflexão: compilado para a API 23)
  private boolean mantemPermissoes() {
    if (Build.VERSION.SDK_INT < 30) return true;
    try {
      return (Boolean) PackageManager.class.getMethod("isAutoRevokeWhitelisted").invoke(getPackageManager());
    } catch (Exception e) {
      return true;
    }
  }

  private boolean whatsappInstalado() {
    for (String p : Ponte.PACOTES_WHATSAPP) {
      try {
        getPackageManager().getPackageInfo(p, 0);
        return true;
      } catch (PackageManager.NameNotFoundException ignorada) {
        // tenta o próximo
      }
    }
    return false;
  }

  private boolean acessoNotificacoes() {
    final String lista = Settings.Secure.getString(getContentResolver(), "enabled_notification_listeners");
    final String eu = new ComponentName(this, Ouvinte.class).flattenToString();
    return lista != null && lista.contains(eu);
  }

  private boolean abrir(Intent i) {
    try {
      startActivity(i);
      return true;
    } catch (Exception e) {
      return false;
    }
  }

  // ---------- montar a tela ----------
  private int dp(int v) {
    return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics());
  }

  private TextView texto(String s, int sp) {
    final TextView t = new TextView(this);
    t.setText(s);
    t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
    t.setPadding(0, dp(4), 0, dp(4));
    coluna.addView(t);
    return t;
  }

  private void secao(String s) {
    final TextView t = texto(s, 18);
    t.setTypeface(Typeface.DEFAULT_BOLD);
    t.setPadding(0, dp(20), 0, dp(4));
  }

  private void botao(String s, View.OnClickListener acao) {
    final Button b = new Button(this);
    b.setText(s);
    b.setAllCaps(false);
    b.setOnClickListener(acao);
    coluna.addView(b);
  }

  private CheckBox caixa(String s, final String opcao, boolean padrao) {
    final CheckBox c = new CheckBox(this);
    c.setText(s);
    c.setChecked(Ponte.opcoes(this).getBoolean(opcao, padrao));
    c.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
      @Override
      public void onCheckedChanged(CompoundButton v, boolean marcado) {
        Ponte.opcoes(Principal.this).edit().putBoolean(opcao, marcado).apply();
      }
    });
    coluna.addView(c);
    return c;
  }
}
