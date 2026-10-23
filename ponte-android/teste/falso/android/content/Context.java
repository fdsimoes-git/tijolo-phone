package android.content;

import java.util.HashMap;
import java.util.Map;

/** Falso: as preferências ficam na memória, as mesmas para todo o "app". */
public class Context {
  public static final int MODE_PRIVATE = 0;
  private static final Map<String, Map<String, Object>> arquivos = new HashMap<>();

  public static void zerarPreferencias() { arquivos.clear(); }

  public Context getApplicationContext() { return this; }

  public SharedPreferences getSharedPreferences(String nome, int modo) {
    Map<String, Object> a = arquivos.get(nome);
    if (a == null) { a = new HashMap<>(); arquivos.put(nome, a); }
    final Map<String, Object> dados = a;
    return new SharedPreferences() {
      public String getString(String k, String p) { return dados.containsKey(k) ? (String) dados.get(k) : p; }
      public boolean getBoolean(String k, boolean p) { return dados.containsKey(k) ? (Boolean) dados.get(k) : p; }
      public long getLong(String k, long p) { return dados.containsKey(k) ? (Long) dados.get(k) : p; }
      public Map<String, ?> getAll() { return new HashMap<>(dados); }
      public Editor edit() {
        final Map<String, Object> novos = new HashMap<>();
        final java.util.Set<String> tirar = new java.util.HashSet<>();
        return new Editor() {
          public Editor putString(String k, String v) { novos.put(k, v); tirar.remove(k); return this; }
          public Editor putBoolean(String k, boolean v) { novos.put(k, v); tirar.remove(k); return this; }
          public Editor putLong(String k, long v) { novos.put(k, v); tirar.remove(k); return this; }
          public Editor remove(String k) { tirar.add(k); novos.remove(k); return this; }
          public void apply() { for (String k : tirar) dados.remove(k); dados.putAll(novos); }
          public boolean commit() { apply(); return true; }
        };
      }
    };
  }
}
