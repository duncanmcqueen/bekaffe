import java.awt.*;
import java.awt.event.*;

/** Milestone 1: a plain Frame. */
public class AwtFrame
{
  public static void main(String[] args) throws Exception
  {
    System.out.println("creating Frame...");
    Frame f = new Frame("Kaffe AWT on Haiku");
    f.setBounds(100, 100, 320, 200);
    f.setBackground(new Color(0x3366CC));
    f.addWindowListener(new WindowAdapter()
      {
        public void windowClosing(WindowEvent e)
        {
          System.out.println("windowClosing -> exit");
          System.exit(0);
        }
      });
    f.setVisible(true);
    System.out.println("visible, insets=" + f.getInsets());
    int secs = args.length > 0 ? Integer.parseInt(args[0]) : 30;
    long end = System.currentTimeMillis() + secs * 1000L;
    long now;
    while ((now = System.currentTimeMillis()) < end)
      {
        long t0 = now;
        Thread.sleep(Math.min(1000L, end - now));
        long slept = System.currentTimeMillis() - t0;
        if (slept < 900 && end - now >= 1000)
          System.out.println("short sleep: " + slept + " ms");
      }
    System.out.println("timeout -> exit");
    System.exit(0);
  }
}
