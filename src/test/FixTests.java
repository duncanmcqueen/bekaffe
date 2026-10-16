import java.io.*;

/** Regression tests for the Kaffe fixes. Each test prints PASS or FAIL. */
public class FixTests
{
  static int fails = 0;
  static void check(String name, boolean ok) {
    System.out.println((ok ? "PASS " : "FAIL ") + name);
    if (!ok) fails++;
  }

  // The JNI name of this method is longer than 1024 bytes.
  static native void n_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_();

  public static void main(String[] args) throws Exception
  {
    String na = "éèê中文üöä";
    String big = "";
    for (int i = 0; i < 100; i++) big += na;

    // A thread name with many non-ASCII characters (thread.c copies it
    // into a fixed buffer).
    Thread t = new Thread(new Runnable() { public void run() {} }, big);
    t.start(); t.join();
    check("non-ASCII thread name", t.getName().equals(big));

    // loadLibrary with a long non-ASCII name (Runtime.c copies the path
    // into a fixed buffer).
    try { System.loadLibrary(big); check("non-ASCII loadLibrary", false); }
    catch (UnsatisfiedLinkError e) { check("non-ASCII loadLibrary", true); }

    // File names with non-ASCII characters.
    File f = new File(System.getProperty("java.io.tmpdir"), "kfix-" + na + ".txt");
    FileOutputStream o = new FileOutputStream(f); o.write(42); o.close();
    boolean ok = f.exists() && f.length() == 1;
    File[] l = f.getParentFile().listFiles();
    boolean listed = false;
    for (int i = 0; l != null && i < l.length; i++)
      if (l[i].getName().equals(f.getName())) listed = true;
    ok = ok && listed && f.delete() && !f.exists();
    check("non-ASCII file name", ok);

    // A native method with a very long JNI name.
    try { n_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_a_b_(); check("long JNI name", false); }
    catch (UnsatisfiedLinkError e) { check("long JNI name", true); }

    // A multi-dimensional array whose array count does not fit in an int.
    try { int[][][] a = new int[65536][65536][1]; check("huge multi-array", a == null); }
    catch (OutOfMemoryError e) { check("huge multi-array", true); }

    // Normal multi-dimensional arrays still work.
    int[][][] m = new int[3][4][5];
    m[2][3][4] = 7;
    check("normal multi-array", m.length == 3 && m[0].length == 4 && m[2][3][4] == 7);

    // Static constants (ConstantValue attributes), read by reflection so
    // that the VM's values are used, not copies that javac inlined.
    Class c = C.class;
    check("static constants",
          c.getDeclaredField("I").getInt(null) == 42
          && c.getDeclaredField("J").getLong(null) == 1L << 40
          && c.getDeclaredField("F").getFloat(null) == 1.5f
          && c.getDeclaredField("D").getDouble(null) == 2.25
          && "sé".equals(c.getDeclaredField("S").get(null))
          && c.getDeclaredField("B").getByte(null) == 7
          && c.getDeclaredField("Z").getBoolean(null)
          && c.getDeclaredField("CH").getChar(null) == 'é'
          && c.getDeclaredField("SH").getShort(null) == -3);

    System.out.println(fails == 0 ? "ALL PASS" : fails + " FAILED");
    System.exit(fails == 0 ? 0 : 1);
  }
}

class C {
  // Compile-time constants: javac writes ConstantValue attributes.
  static final int I = 42;
  static final long J = 1099511627776L;
  static final float F = 1.5f;
  static final double D = 2.25;
  static final String S = "sé";
  static final byte B = 7;
  static final boolean Z = true;
  static final char CH = 'é';
  static final short SH = -3;
}
