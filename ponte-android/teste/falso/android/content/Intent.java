package android.content;

import java.util.HashMap;
import java.util.Map;

/** Falso (só para os testes no computador). */
public class Intent {
  private String acao;
  private final Map<String, Object> extras = new HashMap<>();

  public Intent() {}
  public Intent(String acao) { this.acao = acao; }
  public Intent(Context c, Class<?> classe) {}

  public String getAction() { return acao; }
  public Intent putExtra(String k, int v) { extras.put(k, v); return this; }
  public Intent putExtra(String k, String v) { extras.put(k, v); return this; }
  public Intent putExtra(String k, Object v) { extras.put(k, v); return this; }
  public int getIntExtra(String k, int padrao) { return extras.containsKey(k) ? (Integer) extras.get(k) : padrao; }
  public String getStringExtra(String k) { return (String) extras.get(k); }
  public Object extra(String k) { return extras.get(k); }
}
