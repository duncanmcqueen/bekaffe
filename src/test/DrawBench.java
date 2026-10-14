import java.awt.*;
import java.awt.geom.*;
import java.awt.image.BufferedImage;

/**
 * Times drawing operations on an off-screen image (HaikuGraphics2D, the
 * same code that draws windows). Prints the time per operation and a
 * checksum of the pixels, so that faster code can be compared with the
 * old results. Argument: a repeat factor (default 1).
 */
public class DrawBench
{
  interface Op
  {
    void run(Graphics2D g, int i);
  }

  static BufferedImage target = new BufferedImage(400, 300,
                                                  BufferedImage.TYPE_INT_RGB);

  /** Where to write a PNG of each result, or null. */
  static String dumpDir;

  static long now()
  {
    return System.currentTimeMillis();
  }

  static void bench(String name, int n, Op op)
  {
    Graphics2D g = target.createGraphics();
    g.setColor(Color.white);
    g.fillRect(0, 0, 400, 300);
    g.setColor(new Color(0x2050a0));
    long t0 = now();
    for (int i = 0; i < n; i++)
      op.run(g, i);
    long t = now() - t0;
    g.dispose();
    if (dumpDir != null)
      {
        // A binary PPM; GNU Classpath's ImageIO cannot write PNG.
        try
          {
            java.io.OutputStream out = new java.io.BufferedOutputStream(
              new java.io.FileOutputStream(new java.io.File(dumpDir,
                name.replaceAll("[^A-Za-z0-9]+", "_") + ".ppm")));
            out.write("P6\n400 300\n255\n".getBytes("US-ASCII"));
            int[] rgb = target.getRGB(0, 0, 400, 300, null, 0, 400);
            byte[] b = new byte[rgb.length * 3];
            for (int i = 0; i < rgb.length; i++)
              {
                b[3 * i] = (byte) (rgb[i] >> 16);
                b[3 * i + 1] = (byte) (rgb[i] >> 8);
                b[3 * i + 2] = (byte) rgb[i];
              }
            out.write(b);
            out.close();
          }
        catch (Exception e)
          {
            System.out.println("BENCH dump failed: " + e);
          }
      }
    int[] px = target.getRGB(0, 0, 400, 300, null, 0, 400);
    long sum = 0;
    for (int i = 0; i < px.length; i++)
      sum = sum * 31 + px[i];
    System.out.println("BENCH " + pad(name, 26) + pad(String.valueOf(n), 6)
                       + pad(String.format("%d", t * 1000 / n), 9)
                       + " us/op  sum=" + Long.toHexString(sum));
  }

  static String pad(String s, int n)
  {
    StringBuilder b = new StringBuilder(s);
    while (b.length() < n)
      b.append(' ');
    return b.toString();
  }

  public static void main(String[] args)
  {
    int k = args.length > 0 ? Integer.parseInt(args[0]) : 1;
    if (args.length > 1)
      dumpDir = args[1];
    final BufferedImage img = new BufferedImage(100, 100,
                                                BufferedImage.TYPE_INT_ARGB);
    for (int y = 0; y < 100; y++)
      for (int x = 0; x < 100; x++)
        img.setRGB(x, y, ((x + y) * 2 << 24) | (x * 2 << 16) | (y * 2 << 8) | 128);
    final BufferedImage opaque = new BufferedImage(100, 100,
                                                   BufferedImage.TYPE_INT_RGB);
    opaque.createGraphics().drawImage(img, 0, 0, null);
    final Polygon star = new Polygon();
    for (int i = 0; i < 10; i++)
      {
        double a = Math.PI * i / 5;
        double r = i % 2 == 0 ? 60 : 25;
        star.addPoint((int) (200 + r * Math.sin(a)), (int) (150 - r * Math.cos(a)));
      }

    bench("fillRect 100x100", 200 * k, new Op() { public void run(Graphics2D g, int i) {
      g.fillRect(i % 300, i % 200, 100, 100); } });
    bench("drawLine", 2000 * k, new Op() { public void run(Graphics2D g, int i) {
      g.drawLine(i % 400, 0, 399 - i % 400, 299); } });
    bench("drawRect 100x100", 200 * k, new Op() { public void run(Graphics2D g, int i) {
      g.drawRect(i % 300, i % 200, 100, 100); } });
    bench("fillOval 100x100", 100 * k, new Op() { public void run(Graphics2D g, int i) {
      g.fillOval(i % 300, i % 200, 100, 100); } });
    bench("drawOval 100x100", 100 * k, new Op() { public void run(Graphics2D g, int i) {
      g.drawOval(i % 300, i % 200, 100, 100); } });
    bench("fillPolygon star", 100 * k, new Op() { public void run(Graphics2D g, int i) {
      g.fillPolygon(star); } });
    bench("fillArc 100x100 270deg", 100 * k, new Op() { public void run(Graphics2D g, int i) {
      g.fillArc(i % 300, i % 200, 100, 100, 0, 270); } });
    bench("fillRoundRect", 100 * k, new Op() { public void run(Graphics2D g, int i) {
      g.fillRoundRect(i % 300, i % 200, 100, 80, 20, 20); } });
    bench("thick line (width 4)", 100 * k, new Op() { public void run(Graphics2D g, int i) {
      g.setStroke(new BasicStroke(4));
      g.drawLine(i % 400, 0, 399 - i % 400, 299); } });
    bench("AA fillOval 100x100", 50 * k, new Op() { public void run(Graphics2D g, int i) {
      g.setRenderingHint(RenderingHints.KEY_ANTIALIASING,
                         RenderingHints.VALUE_ANTIALIAS_ON);
      g.fillOval(i % 300, i % 200, 100, 100); } });
    bench("AA thick line", 50 * k, new Op() { public void run(Graphics2D g, int i) {
      g.setRenderingHint(RenderingHints.KEY_ANTIALIASING,
                         RenderingHints.VALUE_ANTIALIAS_ON);
      g.setStroke(new BasicStroke(3));
      g.drawLine(i % 400, 0, 399 - i % 400, 299); } });
    bench("translucent fillRect", 100 * k, new Op() { public void run(Graphics2D g, int i) {
      g.setColor(new Color(255, 0, 0, 128));
      g.fillRect(i % 300, i % 200, 100, 100); } });
    bench("gradient fillRect", 20 * k, new Op() { public void run(Graphics2D g, int i) {
      g.setPaint(new GradientPaint(0, 0, Color.red, 400, 0, Color.blue));
      g.fillRect(i % 300, i % 200, 100, 100); } });
    bench("drawImage 100x100 ARGB", 100 * k, new Op() { public void run(Graphics2D g, int i) {
      g.drawImage(img, i % 300, i % 200, null); } });
    bench("drawImage 100x100 RGB", 100 * k, new Op() { public void run(Graphics2D g, int i) {
      g.drawImage(opaque, i % 300, i % 200, null); } });
    bench("drawImage scaled to 200", 50 * k, new Op() { public void run(Graphics2D g, int i) {
      g.drawImage(img, i % 200, i % 100, 200, 200, null); } });
    bench("drawImage rotated", 20 * k, new Op() { public void run(Graphics2D g, int i) {
      g.drawImage(opaque, AffineTransform.getRotateInstance(0.3, 200, 150), null); } });
    bench("drawString", 200 * k, new Op() { public void run(Graphics2D g, int i) {
      g.drawString("The quick brown fox " + (i % 10), i % 300, 20 + i % 270); } });
    bench("copyArea 200x100", 50 * k, new Op() { public void run(Graphics2D g, int i) {
      g.copyArea(0, 0, 200, 100, 100, 100); } });
    System.out.println("BENCH done");
  }
}
