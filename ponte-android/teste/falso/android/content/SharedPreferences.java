package android.content;

import java.util.Map;

/** Falso (só para os testes no computador). */
public interface SharedPreferences {
  String getString(String k, String padrao);
  boolean getBoolean(String k, boolean padrao);
  long getLong(String k, long padrao);
  Map<String, ?> getAll();
  Editor edit();

  interface Editor {
    Editor putString(String k, String v);
    Editor putBoolean(String k, boolean v);
    Editor putLong(String k, long v);
    Editor remove(String k);
    void apply();
    boolean commit();
  }
}
