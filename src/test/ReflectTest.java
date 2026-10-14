import java.lang.reflect.Method;

public class ReflectTest
{
  public static void main(String[] args) throws Exception
  {
    Class c = Class.forName("javax.swing.JComponent");
    System.out.println("JComponent isPrimitive=" + c.isPrimitive());
    Class ui = Class.forName("javax.swing.plaf.basic.BasicButtonUI");
    Method m = ui.getMethod("createUI", new Class[] { c });
    Class[] pts = m.getParameterTypes();
    for (int i = 0; i < pts.length; i++)
      System.out.println("pt[" + i + "]=" + pts[i].getName() + " prim="
                         + pts[i].isPrimitive() + " same=" + (pts[i] == c)
                         + " array=" + pts[i].isArray());
    System.out.println("after getParameterTypes: JComponent isPrimitive="
                       + c.isPrimitive());
    Class r = Class.forName("java.lang.Runnable");
    System.out.println("Runnable isPrimitive=" + r.isPrimitive());
    System.out.println("Object isPrimitive=" + Object.class.isPrimitive()
                       + " int=" + int.class.isPrimitive());
  }
}
