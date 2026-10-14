import java.awt.*;
import java.awt.event.*;
import java.awt.image.BufferedImage;
import javax.swing.*;

/** Milestone 3: a small Swing UI plus off-screen rendering. */
public class SwingTest
{
  public static void main(String[] args) throws Exception
  {
    // Off-screen rendering through the Haiku GraphicsEnvironment.
    BufferedImage img = new BufferedImage(40, 20, BufferedImage.TYPE_INT_ARGB);
    Graphics2D g = img.createGraphics();
    g.setColor(Color.GREEN);
    g.fillRect(0, 0, 40, 20);
    g.dispose();
    System.out.println("offscreen pixel=" + Integer.toHexString(img.getRGB(5, 5))
                       + " graphics=" + g.getClass().getName());

    SwingUtilities.invokeAndWait(new Runnable()
      {
        public void run()
        {
          final JFrame f = new JFrame("Swing on Kaffe/Haiku");
          f.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
          JPanel p = new JPanel(new GridLayout(0, 1, 4, 4));
          p.setBorder(BorderFactory.createEmptyBorder(8, 8, 8, 8));
          final JLabel label = new JLabel("Type and press the button");
          final JTextField field = new JTextField("hello");
          final JCheckBox check = new JCheckBox("Shout");
          JButton button = new JButton("Apply");
          button.addActionListener(new ActionListener()
            {
              public void actionPerformed(ActionEvent e)
              {
                String t = field.getText();
                if (check.isSelected())
                  t = t.toUpperCase();
                label.setText("You typed: " + t);
                System.out.println("apply: " + t);
              }
            });
          p.add(label);
          p.add(field);
          p.add(check);
          p.add(button);
          f.getContentPane().add(p);
          f.pack();
          f.setLocation(200, 150);
          f.setVisible(true);
          System.out.println("swing frame visible, size=" + f.getSize());
        }
      });
    int secs = args.length > 0 ? Integer.parseInt(args[0]) : 600;
    long end = System.currentTimeMillis() + secs * 1000L;
    while (System.currentTimeMillis() < end)
      Thread.sleep(1000);
    System.exit(0);
  }
}
