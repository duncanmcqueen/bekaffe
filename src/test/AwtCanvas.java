import java.awt.*;
import java.awt.event.*;

/** Milestone 2: Canvas painting, Label, Button, events. */
public class AwtCanvas
{
  public static void main(String[] args) throws Exception
  {
    final Frame f = new Frame("Canvas + widgets");
    f.setLayout(new BorderLayout());
    final Label status = new Label("Click the canvas or the button");
    Canvas c = new Canvas()
      {
        public void paint(Graphics g)
        {
          int w = getWidth(), h = getHeight();
          g.setColor(Color.WHITE);
          g.fillRect(0, 0, w, h);
          g.setColor(Color.RED);
          g.fillOval(10, 10, w / 2, h / 2);
          g.setColor(Color.BLUE);
          g.drawLine(0, 0, w, h);
          g.drawLine(0, h, w, 0);
          g.setColor(Color.BLACK);
          g.drawRect(5, 5, w - 11, h - 11);
          g.drawString("Hello from Kaffe on Haiku", 20, h - 20);
        }
      };
    c.addMouseListener(new MouseAdapter()
      {
        public void mousePressed(MouseEvent e)
        {
          System.out.println("canvas press " + e.getX() + "," + e.getY()
                             + " button " + e.getButton());
          status.setText("Canvas pressed at " + e.getX() + "," + e.getY());
        }
      });
    Button b = new Button("Quit");
    b.addActionListener(new ActionListener()
      {
        public void actionPerformed(ActionEvent e)
        {
          System.out.println("Quit button -> exit");
          System.exit(0);
        }
      });
    f.add(status, BorderLayout.NORTH);
    f.add(c, BorderLayout.CENTER);
    f.add(b, BorderLayout.SOUTH);
    f.addWindowListener(new WindowAdapter()
      {
        public void windowClosing(WindowEvent e)
        {
          System.out.println("windowClosing -> exit");
          System.exit(0);
        }
      });
    f.addKeyListener(new KeyAdapter()
      {
        public void keyTyped(KeyEvent e)
        {
          System.out.println("key typed '" + e.getKeyChar() + "'");
        }
      });
    Toolkit.getDefaultToolkit().addAWTEventListener(new AWTEventListener()
      {
        public void eventDispatched(AWTEvent e)
        {
          KeyEvent k = (KeyEvent) e;
          System.out.println("key event id=" + k.getID() + " code="
                             + k.getKeyCode() + " char=" + (int) k.getKeyChar()
                             + " src=" + k.getSource().getClass().getName());
        }
      }, AWTEvent.KEY_EVENT_MASK);
    Toolkit.getDefaultToolkit().addAWTEventListener(new AWTEventListener()
      {
        public void eventDispatched(AWTEvent e)
        {
          System.out.println("focus event " + e.paramString() + " src="
                             + e.getSource().getClass().getName());
        }
      }, AWTEvent.FOCUS_EVENT_MASK | AWTEvent.WINDOW_FOCUS_EVENT_MASK);
    f.setBounds(150, 120, 400, 300);
    f.setVisible(true);
    System.out.println("visible");
    int secs = args.length > 0 ? Integer.parseInt(args[0]) : 60;
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
