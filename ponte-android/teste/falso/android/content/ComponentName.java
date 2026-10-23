package android.content;

/** Falso (só para os testes no computador). */
public final class ComponentName {
  private final String classe;

  public ComponentName(Context c, Class<?> classe) {
    this.classe = classe.getName();
  }

  public String getClassName() {
    return classe;
  }
}
