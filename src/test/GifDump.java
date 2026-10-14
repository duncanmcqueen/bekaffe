import java.awt.image.BufferedImage;
import java.io.File;
import javax.imageio.ImageIO;

/** Prints a GIF (or any image) as ASCII art: '#' opaque, '.' clear. */
public class GifDump
{
  public static void main(String[] args) throws Exception
  {
    BufferedImage img = ImageIO.read(new File(args[0]));
    System.out.println(img.getWidth() + "x" + img.getHeight() + " type="
                       + img.getType() + " cm=" + img.getColorModel());
    for (int y = 0; y < img.getHeight(); y++)
      {
        StringBuilder b = new StringBuilder();
        for (int x = 0; x < img.getWidth(); x++)
          {
            int p = img.getRGB(x, y);
            b.append((p >>> 24) < 128 ? '.' : ((p & 0xFFFFFF) == 0 ? 'k' : '#'));
          }
        System.out.println(b);
      }
  }
}
