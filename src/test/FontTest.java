import java.awt.*;
import java.awt.event.*;
import java.awt.geom.AffineTransform;

/**
 * Shows text in the logical fonts, styles and sizes, CJK text, rotated
 * text and gradient text. A red line under each sample is as long as
 * FontMetrics.stringWidth() says, so it shows whether measuring and
 * drawing agree.
 */
public class FontTest extends Frame
{
  FontTest()
  {
    super("Font test");
    add(new Canvas()
      {
        public void paint(Graphics g)
        {
          Graphics2D g2 = (Graphics2D) g;
          String[] names = { "Dialog", "Serif", "Monospaced", "SansSerif" };
          int[] styles = { Font.PLAIN, Font.BOLD, Font.ITALIC };
          int y = 24;
          for (int i = 0; i < names.length; i++)
            for (int s = 0; s < styles.length; s++)
              {
                Font f = new Font(names[i], styles[s], 13 + 2 * s);
                sample(g, f, names[i] + " " + style(styles[s])
                       + " - The quick brown fox", 10, y);
                y += 22;
              }
          sample(g, new Font("Dialog", Font.PLAIN, 20),
                 "Japanese: \u65e5\u672c\u8a9e  Greek: \u03b1\u03b2\u03b3", 10, y + 8);
          y += 40;
          sample(g, new Font("Serif", Font.BOLD, 36), "Big Serif 36", 10, y + 20);

          // Rotated text (outline path).
          AffineTransform old = g2.getTransform();
          g2.translate(470, 140);
          g2.rotate(-Math.PI / 6);
          g2.setColor(Color.blue);
          g2.setFont(new Font("SansSerif", Font.BOLD, 18));
          g2.drawString("Rotated 30\u00b0", 0, 0);
          g2.setTransform(old);

          // Gradient text (outline path).
          g2.setPaint(new GradientPaint(440, 0, Color.red, 620, 0, Color.blue));
          g2.setFont(new Font("Serif", Font.BOLD, 28));
          g2.drawString("Gradient", 440, 230);
          g2.setPaint(Color.black);
        }

        void sample(Graphics g, Font f, String s, int x, int y)
        {
          g.setFont(f);
          g.setColor(Color.black);
          g.drawString(s, x, y);
          int w = g.getFontMetrics().stringWidth(s);
          g.setColor(Color.red);
          g.drawLine(x, y + 3, x + w, y + 3);
        }

        String style(int s)
        {
          return s == Font.BOLD ? "bold" : s == Font.ITALIC ? "italic"
                                                            : "plain";
        }
      });
    addWindowListener(new WindowAdapter()
      {
        public void windowClosing(WindowEvent e)
        {
          System.exit(0);
        }
      });
    setBackground(Color.white);
    setBounds(40, 40, 660, 420);
  }

  public static void main(String[] args)
  {
    String[] fams = GraphicsEnvironment.getLocalGraphicsEnvironment()
                    .getAvailableFontFamilyNames();
    System.out.println("EVENT " + fams.length + " families, first "
                       + fams[0] + ", " + fams[5]);
    new FontTest().setVisible(true);
  }
}
