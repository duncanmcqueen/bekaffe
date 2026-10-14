import java.awt.*;
import java.awt.geom.*;
import java.awt.image.BufferedImage;

/**
 * Draws shapes and images through an elliptical clip, so HaikuGraphics2D
 * must use its Java fallbacks, and writes the result as a PPM file.
 * Argument: the output file.
 */
public class ClipTest
{
  public static void main(String[] args) throws Exception
  {
    BufferedImage img = new BufferedImage(60, 40, BufferedImage.TYPE_INT_ARGB);
    for (int y = 0; y < 40; y++)
      for (int x = 0; x < 60; x++)
        img.setRGB(x, y, 0xFF000000 | (x * 4 << 16) | (y * 6 << 8) | 0x80);
    BufferedImage t = new BufferedImage(400, 300, BufferedImage.TYPE_INT_RGB);
    Graphics2D g = t.createGraphics();
    g.setColor(Color.white);
    g.fillRect(0, 0, 400, 300);
    g.setClip(new Ellipse2D.Float(20, 20, 360, 260));
    g.setColor(new Color(0xDDEEFF));
    g.fillRect(0, 0, 400, 300);
    g.setColor(Color.red);
    g.fillOval(10, 10, 120, 120);
    g.drawImage(img, 150, 10, null);
    g.drawImage(img, 220, 10, 120, 80, null);
    g.drawImage(img, 20, 150, 140, 260, 10, 5, 50, 35, Color.yellow, null);
    g.drawImage(img, AffineTransform.getRotateInstance(0.5, 260, 200), null);
    g.drawImage(img, 300, 200, Color.green, null);
    g.dispose();
    java.io.OutputStream out = new java.io.FileOutputStream(args[0]);
    out.write("P6\n400 300\n255\n".getBytes("US-ASCII"));
    int[] rgb = t.getRGB(0, 0, 400, 300, null, 0, 400);
    byte[] b = new byte[rgb.length * 3];
    for (int i = 0; i < rgb.length; i++)
      {
        b[3 * i] = (byte) (rgb[i] >> 16);
        b[3 * i + 1] = (byte) (rgb[i] >> 8);
        b[3 * i + 2] = (byte) rgb[i];
      }
    out.write(b);
    out.close();
    System.out.println("CLIPTEST done");
  }
}
