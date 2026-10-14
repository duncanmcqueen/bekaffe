import java.io.*;
import java.math.BigDecimal;
import java.net.InetAddress;
import java.util.*;
import java.util.regex.*;

/**
 * Command line checks for a Kaffe + GNU Classpath build. Each line is
 * "OK name result" or "FAIL name exception".
 */
public class CliTest
{
  interface Check
  {
    Object run() throws Exception;
  }

  static int failures;

  static void check(String name, Check c)
  {
    try
      {
        System.out.println("OK   " + name + " " + c.run());
      }
    catch (Throwable t)
      {
        failures++;
        System.out.println("FAIL " + name + " " + t);
      }
  }

  public static void main(String[] args) throws Exception
  {
    check("version", new Check() { public Object run() {
      return System.getProperty("java.vm.name") + " / "
             + System.getProperty("java.version") + " / "
             + System.getProperty("os.name"); } });
    check("collections", new Check() { public Object run() {
      Map<String,Integer> m = new TreeMap<String,Integer>();
      for (String s : "the quick brown fox jumps over the lazy dog".split(" "))
        m.put(s, m.containsKey(s) ? m.get(s) + 1 : 1);
      return m; } });
    check("format", new Check() { public Object run() {
      return String.format("%05d|%-6s|%.3f|%x", 42, "ab", Math.PI, 255); } });
    check("bigdecimal", new Check() { public Object run() {
      return new BigDecimal("1234.5678").setScale(2, BigDecimal.ROUND_HALF_UP)
             + " " + new BigDecimal("0.000").stripTrailingZeros(); } });
    check("objects (0.99 API)", new Check() { public Object run() throws Exception {
      Class<?> c = Class.forName("java.util.Objects");
      return c.getMethod("hash", Object[].class)
              .invoke(null, (Object) new Object[] { "a", 1 }); } });
    check("console (0.99 API)", new Check() { public Object run() throws Exception {
      return Class.forName("java.io.Console").getName(); } });
    check("regex", new Check() { public Object run() {
      Matcher m = Pattern.compile("(\\w+)@(\\w+)\\.org").matcher("mail me@haiku.org now");
      return m.find() ? m.group(1) + " at " + m.group(2) : "no match"; } });
    check("file io", new Check() { public Object run() throws Exception {
      File f = File.createTempFile("clitest", ".txt");
      PrintWriter w = new PrintWriter(new FileWriter(f));
      w.println("line one");
      w.close();
      BufferedReader r = new BufferedReader(new FileReader(f));
      String s = r.readLine();
      r.close();
      f.delete();
      return s; } });
    check("threads", new Check() { public Object run() throws Exception {
      final int[] sum = new int[1];
      Thread[] ts = new Thread[4];
      for (int i = 0; i < ts.length; i++)
        {
          final int n = i;
          ts[i] = new Thread() { public void run() {
            synchronized (sum) { sum[0] += n; } } };
          ts[i].start();
        }
      for (int i = 0; i < ts.length; i++)
        ts[i].join();
      return sum[0]; } });
    check("thread local", new Check() { public Object run() {
      ThreadLocal<String> t = new ThreadLocal<String>();
      t.set("kept");
      return t.get(); } });
    check("process", new Check() { public Object run() throws Exception {
      Process p = new ProcessBuilder("uname", "-sm").redirectErrorStream(true).start();
      BufferedReader r = new BufferedReader(new InputStreamReader(p.getInputStream()));
      String s = r.readLine();
      return s + " exit=" + p.waitFor(); } });
    check("process exit code", new Check() { public Object run() throws Exception {
      return new ProcessBuilder("sh", "-c", "exit 200").start().waitFor(); } });
    check("process missing", new Check() { public Object run() throws Exception {
      try
        {
          new ProcessBuilder("/no/such/program").start();
          return "started?!";
        }
      catch (IOException e)
        {
          return "IOException as expected";
        }
      } });
    check("network", new Check() { public Object run() throws Exception {
      return InetAddress.getByName("localhost").getHostAddress(); } });
    check("system properties", new Check() { public Object run() {
      return System.getProperty("user.home") + " " + System.getProperty("file.encoding"); } });
    System.out.println(failures == 0 ? "ALL PASSED" : failures + " FAILED");
  }
}
