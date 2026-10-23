package android.os;

import java.util.HashMap;
import java.util.Map;

/** Falso (só para os testes no computador): um mapa. */
public class Bundle implements Parcelable {
  private final Map<String, Object> m = new HashMap<>();

  public Object get(String k) { return m.get(k); }
  public boolean containsKey(String k) { return m.containsKey(k); }
  public CharSequence getCharSequence(String k) { return (CharSequence) m.get(k); }
  public String getString(String k) { return (String) m.get(k); }
  public boolean getBoolean(String k) { return getBoolean(k, false); }
  public boolean getBoolean(String k, boolean padrao) { return m.containsKey(k) ? (Boolean) m.get(k) : padrao; }
  public long getLong(String k) { return m.containsKey(k) ? (Long) m.get(k) : 0L; }
  public Parcelable[] getParcelableArray(String k) { return (Parcelable[]) m.get(k); }
  public void putCharSequence(String k, CharSequence v) { m.put(k, v); }
  public void putString(String k, String v) { m.put(k, v); }
  public void putBoolean(String k, boolean v) { m.put(k, v); }
  public void putLong(String k, long v) { m.put(k, v); }
  public void putParcelableArray(String k, Parcelable[] v) { m.put(k, v); }
  public void putObjeto(String k, Object v) { m.put(k, v); }
}
