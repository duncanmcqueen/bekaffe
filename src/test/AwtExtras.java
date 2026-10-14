import java.awt.*;
import java.awt.datatransfer.*;
import java.awt.event.*;
import java.awt.image.BufferedImage;
import java.io.File;
import java.io.FilenameFilter;
import javax.sound.sampled.*;

/**
 * Tests the clipboard, cursors, frame state, FileDialog filters and
 * sound. Every result is printed with the prefix "EVENT".
 * Arguments run one test at start: "clip", "tone", "stream", "filter",
 * "glob", "min", "zoom".
 */
public class AwtExtras extends Frame
{
  Label status = new Label("Ready");
  TextField text = new TextField("Copy me from Java", 30);

  void report(String s)
  {
    System.out.println("EVENT " + s);
    status.setText(s);
  }

  AwtExtras()
  {
    super("AWT extras");
    setLayout(new BorderLayout());

    Panel top = new Panel(new FlowLayout(FlowLayout.LEFT));
    top.add(text);
    Button copy = new Button("Copy");
    copy.addActionListener(new ActionListener()
      {
        public void actionPerformed(ActionEvent e)
        {
          Clipboard cb = getToolkit().getSystemClipboard();
          cb.setContents(new StringSelection(text.getText()), null);
          report("copied " + text.getText());
        }
      });
    top.add(copy);
    Button paste = new Button("Paste");
    paste.addActionListener(new ActionListener()
      {
        public void actionPerformed(ActionEvent e)
        {
          paste();
        }
      });
    top.add(paste);
    add(top, BorderLayout.NORTH);

    // One cell per cursor.
    Panel cursors = new Panel(new GridLayout(2, 4, 4, 4));
    String[] names = { "Default", "Crosshair", "Text", "Wait", "Hand",
                       "Move", "E resize", "Custom" };
    int[] types = { Cursor.DEFAULT_CURSOR, Cursor.CROSSHAIR_CURSOR,
                    Cursor.TEXT_CURSOR, Cursor.WAIT_CURSOR,
                    Cursor.HAND_CURSOR, Cursor.MOVE_CURSOR,
                    Cursor.E_RESIZE_CURSOR, -1 };
    for (int i = 0; i < names.length; i++)
      {
        Label l = new Label(names[i], Label.CENTER);
        l.setBackground(new Color(0xFFFFFF - i * 0x101008));
        if (types[i] >= 0)
          l.setCursor(Cursor.getPredefinedCursor(types[i]));
        else
          l.setCursor(customCursor());
        cursors.add(l);
      }
    add(cursors, BorderLayout.CENTER);

    Panel buttons = new Panel(new GridLayout(2, 4, 4, 4));
    addButton(buttons, "Minimize", new Runnable()
      {
        public void run()
        {
          setExtendedState(ICONIFIED);
        }
      });
    addButton(buttons, "Zoom", new Runnable()
      {
        public void run()
        {
          setExtendedState(MAXIMIZED_BOTH);
        }
      });
    addButton(buttons, "Normal", new Runnable()
      {
        public void run()
        {
          setExtendedState(NORMAL);
        }
      });
    addButton(buttons, "Open *.txt", new Runnable()
      {
        public void run()
        {
          openFiltered(false);
        }
      });
    addButton(buttons, "Clip", new Runnable()
      {
        public void run()
        {
          playClip();
        }
      });
    addButton(buttons, "Loop 2x", new Runnable()
      {
        public void run()
        {
          loopClip();
        }
      });
    addButton(buttons, "Stream", new Runnable()
      {
        public void run()
        {
          stream();
        }
      });
    addButton(buttons, "Glob *.java", new Runnable()
      {
        public void run()
        {
          openFiltered(true);
        }
      });
    Panel south = new Panel(new BorderLayout());
    south.add(buttons, BorderLayout.CENTER);
    south.add(status, BorderLayout.SOUTH);
    add(south, BorderLayout.SOUTH);

    addWindowListener(new WindowAdapter()
      {
        public void windowClosing(WindowEvent e)
        {
          System.exit(0);
        }

        public void windowIconified(WindowEvent e)
        {
          report("iconified");
        }

        public void windowDeiconified(WindowEvent e)
        {
          report("deiconified");
        }
      });
    addWindowStateListener(new WindowStateListener()
      {
        public void windowStateChanged(WindowEvent e)
        {
          report("state " + e.getOldState() + " -> " + e.getNewState()
                 + " (getExtendedState " + getExtendedState() + ")");
        }
      });
    setBounds(80, 80, 600, 360);
  }

  static Cursor customCursor()
  {
    Dimension d = Toolkit.getDefaultToolkit().getBestCursorSize(16, 16);
    BufferedImage img = new BufferedImage(d.width, d.height,
                                          BufferedImage.TYPE_INT_ARGB);
    Graphics g = img.getGraphics();
    g.setColor(Color.red);
    g.fillOval(0, 0, d.width - 1, d.height - 1);
    g.setColor(Color.black);
    g.drawOval(0, 0, d.width - 1, d.height - 1);
    g.dispose();
    return Toolkit.getDefaultToolkit().createCustomCursor(
             img, new Point(d.width / 2, d.height / 2), "dot");
  }

  void addButton(Panel p, String label, final Runnable r)
  {
    Button b = new Button(label);
    b.addActionListener(new ActionListener()
      {
        public void actionPerformed(ActionEvent e)
        {
          r.run();
        }
      });
    p.add(b);
  }

  void paste()
  {
    try
      {
        Clipboard cb = getToolkit().getSystemClipboard();
        Transferable t = cb.getContents(this);
        if (t != null && t.isDataFlavorSupported(DataFlavor.stringFlavor))
          {
            String s = (String) t.getTransferData(DataFlavor.stringFlavor);
            text.setText(s);
            report("pasted " + s);
          }
        else
          report("pasted nothing");
      }
    catch (Exception e)
      {
        report("paste failed " + e);
      }
  }

  void openFiltered(boolean glob)
  {
    FileDialog fd = new FileDialog(this, "Filtered", FileDialog.LOAD);
    fd.setDirectory("/boot/home/Projects/bekaffe/haiku-peer/filtertest");
    if (glob)
      fd.setFile("*.java");
    else
      fd.setFilenameFilter(new FilenameFilter()
        {
          public boolean accept(File dir, String name)
          {
            boolean ok = name.endsWith(".txt");
            System.out.println("FILTER " + name + " " + ok);
            return ok;
          }
        });
    fd.setVisible(true);
    report("file " + fd.getDirectory() + " | " + fd.getFile());
  }

  // Sound: a one-second tone, A4 then E5, 8-bit mu-law like old applets.

  static byte[] tone(float rate, boolean ulaw)
  {
    int n = (int) rate;
    byte[] b = new byte[ulaw ? n : n * 2];
    for (int i = 0; i < n; i++)
      {
        double f = i < n / 2 ? 440 : 659.25;
        double env = Math.min(1.0, Math.min(i, n - i) / (rate * 0.01));
        int v = (int) (Math.sin(2 * Math.PI * f * i / rate) * 12000 * env);
        if (ulaw)
          b[i] = linearToUlaw(v);
        else
          {
            b[2 * i] = (byte) v;
            b[2 * i + 1] = (byte) (v >> 8);
          }
      }
    return b;
  }

  static byte linearToUlaw(int sample)
  {
    int sign = (sample >> 8) & 0x80;
    if (sign != 0)
      sample = -sample;
    if (sample > 32635)
      sample = 32635;
    sample += 0x84;
    int exponent = 7;
    for (int mask = 0x4000; (sample & mask) == 0 && exponent > 0;
         mask >>= 1)
      exponent--;
    int mantissa = (sample >> (exponent + 3)) & 0x0F;
    return (byte) ~(sign | (exponent << 4) | mantissa);
  }

  Clip clip;

  Clip getClip()
    throws Exception
  {
    if (clip == null)
      {
        AudioFormat f = new AudioFormat(AudioFormat.Encoding.ULAW, 8000, 8,
                                        1, 1, 8000, false);
        byte[] data = tone(8000, true);
        clip = AudioSystem.getClip();
        clip.addLineListener(new LineListener()
          {
            public void update(LineEvent e)
            {
              System.out.println("EVENT line " + e.getType() + " at "
                                 + e.getFramePosition() + " t="
                                 + (System.currentTimeMillis() % 100000));
            }
          });
        clip.open(f, data, 0, data.length);
        report("clip " + clip.getFrameLength() + " frames, "
               + clip.getMicrosecondLength() / 1000 + " ms");
      }
    return clip;
  }

  void playClip()
  {
    try
      {
        Clip c = getClip();
        c.setFramePosition(0);
        c.start();
      }
    catch (Exception e)
      {
        report("clip failed " + e);
        e.printStackTrace();
      }
  }

  /** Plays an audio file (AU or WAV) the way the applet viewer does. */
  void playFile(String path)
  {
    try
      {
        AudioInputStream in = AudioSystem.getAudioInputStream(new File(path));
        report("file format " + in.getFormat() + ", "
               + in.getFrameLength() + " frames");
        Clip c = AudioSystem.getClip();
        c.addLineListener(new LineListener()
          {
            public void update(LineEvent e)
            {
              System.out.println("EVENT file " + e.getType() + " at "
                                 + e.getFramePosition() + " t="
                                 + (System.currentTimeMillis() % 100000));
            }
          });
        c.open(in);
        c.start();
      }
    catch (Exception e)
      {
        report("file failed " + e);
        e.printStackTrace();
      }
  }

  void loopClip()
  {
    try
      {
        Clip c = getClip();
        c.setFramePosition(0);
        c.setLoopPoints(0, 3999);
        c.loop(2);
      }
    catch (Exception e)
      {
        report("loop failed " + e);
      }
  }

  void stream()
  {
    new Thread()
      {
        public void run()
        {
          try
            {
              AudioFormat f = new AudioFormat(22050, 16, 1, true, false);
              SourceDataLine line = AudioSystem.getSourceDataLine(f);
              line.open(f);
              line.start();
              byte[] data = tone(22050, false);
              long t0 = System.currentTimeMillis();
              int n = line.write(data, 0, data.length);
              line.drain();
              long t1 = System.currentTimeMillis();
              report("stream wrote " + n + " bytes, played "
                     + line.getLongFramePosition() + " frames in "
                     + (t1 - t0) + " ms");
              line.close();
            }
          catch (Exception e)
            {
              report("stream failed " + e);
              e.printStackTrace();
            }
        }
      }.start();
  }

  /** Sleeps ms milliseconds; Thread.sleep() can return early on Kaffe. */
  static void pause(long ms)
  {
    long end = System.currentTimeMillis() + ms;
    long now;
    while ((now = System.currentTimeMillis()) < end)
      {
        try
          {
            Thread.sleep(Math.min(100, end - now));
          }
        catch (InterruptedException e)
          {
            return;
          }
      }
  }

  /** Puts text on the clipboard, then reports changes made elsewhere. */
  void clipboardTest()
  {
    Clipboard cb = getToolkit().getSystemClipboard();
    String mine = "H\u00e4llo from Java \u2713 \ud83d\ude00";
    cb.setContents(new StringSelection(mine), new ClipboardOwner()
      {
        public void lostOwnership(Clipboard c, Transferable t)
          {
            System.out.println("EVENT lost ownership");
          }
      });
    report("clipboard set");
    String last = mine;
    long end = System.currentTimeMillis() + 40000;
    while (System.currentTimeMillis() < end)
      {
        pause(300);
        try
          {
            Transferable t = cb.getContents(this);
            String s = t == null ? null
              : (String) t.getTransferData(DataFlavor.stringFlavor);
            if (s != null && ! s.equals(last))
              {
                StringBuilder hex = new StringBuilder();
                for (int i = 0; i < s.length(); i++)
                  if (s.charAt(i) > 127)
                    hex.append(" U+").append(Integer.toHexString(s.charAt(i)));
                report("clipboard now: " + s + " [" + hex + " ]");
                last = s;
              }
          }
        catch (Exception e)
          {
            report("clipboard read failed " + e);
          }
      }
  }

  /** Zooms, restores, minimizes and restores the window. */
  void stateTest()
  {
    int[] states = { MAXIMIZED_BOTH, NORMAL, ICONIFIED, NORMAL };
    for (int i = 0; i < states.length; i++)
      {
        final int st = states[i];
        System.out.println("EVENT set " + st);
        try
          {
            EventQueue.invokeAndWait(new Runnable()
              {
                public void run()
                {
                  setExtendedState(st);
                }
              });
          }
        catch (Exception e)
          {
            report("state failed " + e);
          }
        pause(4000);
        System.out.println("EVENT bounds " + getBounds() + " state "
                           + getExtendedState());
      }
  }

  public static void main(String[] args)
  {
    final AwtExtras w = new AwtExtras();
    w.setVisible(true);
    Mixer.Info[] mixers = AudioSystem.getMixerInfo();
    for (int i = 0; i < mixers.length; i++)
      System.out.println("EVENT mixer " + mixers[i].getName());
    for (int i = 0; i < args.length; i++)
      {
        final String a = args[i];
        EventQueue.invokeLater(new Runnable()
          {
            public void run()
            {
              if (a.equals("clip")) w.playClip();
              else if (a.equals("loop")) w.loopClip();
              else if (a.equals("stream")) w.stream();
              else if (a.startsWith("au:")) w.playFile(a.substring(3));
              else if (a.equals("filter")) w.openFiltered(false);
              else if (a.equals("glob")) w.openFiltered(true);
              else if (a.equals("min")) w.setExtendedState(ICONIFIED);
              else if (a.equals("zoom")) w.setExtendedState(MAXIMIZED_BOTH);
              else if (a.equals("cbtest") || a.equals("states"))
                new Thread()
                  {
                    public void run()
                    {
                      if (a.equals("cbtest"))
                        w.clipboardTest();
                      else
                        w.stateTest();
                    }
                  }.start();
            }
          });
      }
  }
}
