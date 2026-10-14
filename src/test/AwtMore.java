import java.awt.*;
import java.awt.datatransfer.*;
import java.awt.dnd.*;
import java.awt.event.*;
import java.awt.font.TextHitInfo;
import java.awt.im.InputMethodRequests;
import java.awt.image.BufferedImage;
import java.awt.print.*;
import java.io.File;
import java.io.InputStream;
import java.text.AttributedCharacterIterator;
import java.util.List;

/**
 * Tests modal dialogs, always-on-top windows, drag and drop, printing,
 * input methods and pictures on the clipboard. Every result is printed
 * with the prefix "EVENT". Arguments run one test at start: "modal",
 * "ontop", "cbimage", "print1", "print2", "print3".
 */
public class AwtMore extends Frame
{
  Label status = new Label("Ready");
  TextField text = new TextField("type here", 20);

  static void report(String s)
  {
    System.out.println("EVENT " + s);
  }

  void say(String s)
  {
    report(s);
    status.setText(s);
  }

  AwtMore()
  {
    super("AWT more");
    setLayout(new BorderLayout());

    Panel top = new Panel(new FlowLayout(FlowLayout.LEFT));
    top.add(text);
    addButton(top, "Modal", new Runnable()
      {
        public void run()
        {
          modal();
        }
      });
    addButton(top, "On top", new Runnable()
      {
        public void run()
        {
          onTop();
        }
      });
    addButton(top, "Copy picture", new Runnable()
      {
        public void run()
        {
          copyPicture();
        }
      });
    addButton(top, "Paste", new Runnable()
      {
        public void run()
        {
          paste();
        }
      });
    addButton(top, "Print", new Runnable()
      {
        public void run()
        {
          printAwt();
        }
      });
    add(top, BorderLayout.NORTH);

    Panel middle = new Panel(new GridLayout(1, 3, 6, 6));
    // Drag source: text and a file.
    final Label source = new Label("Drag me", Label.CENTER);
    source.setBackground(new Color(0xC0D8FF));
    DragSource ds = DragSource.getDefaultDragSource();
    ds.createDefaultDragGestureRecognizer(source,
      DnDConstants.ACTION_COPY_OR_MOVE, new DragGestureListener()
      {
        public void dragGestureRecognized(DragGestureEvent e)
        {
          report("drag gesture at " + e.getDragOrigin());
          e.startDrag(null, new StringSelection("Text dragged from Java"),
                      new DragSourceAdapter()
                      {
                        public void dragDropEnd(DragSourceDropEvent d)
                        {
                          report("drag end success=" + d.getDropSuccess()
                                 + " action=" + d.getDropAction());
                        }
                      });
        }
      });
    middle.add(source);

    // Drop target: reports what it gets.
    final Label target = new Label("Drop here", Label.CENTER);
    target.setBackground(new Color(0xD8FFC0));
    new DropTarget(target, new DropTargetAdapter()
      {
        public void dragEnter(DropTargetDragEvent e)
        {
          DataFlavor[] fl = e.getCurrentDataFlavors();
          report("drag enter action=" + e.getDropAction() + " flavors="
                 + (fl == null ? "null" : String.valueOf(fl.length)));
          e.acceptDrag(e.getDropAction());
        }

        public void dragExit(DropTargetEvent e)
        {
          report("drag exit");
        }

        public void drop(DropTargetDropEvent e)
        {
          e.acceptDrop(e.getDropAction());
          String what = describe(e.getTransferable());
          report("drop local=" + e.isLocalTransfer() + " action="
                 + e.getDropAction() + " " + what);
          target.setText("Dropped: " + what);
          e.dropComplete(true);
        }
      });
    middle.add(target);

    // An input method client with inline composition.
    middle.add(new ImCanvas());
    add(middle, BorderLayout.CENTER);
    add(status, BorderLayout.SOUTH);

    text.addKeyListener(new KeyAdapter()
      {
        public void keyTyped(KeyEvent e)
        {
          report("typed U+" + Integer.toHexString(e.getKeyChar()));
        }
      });
    addMouseListener(new MouseAdapter()
      {
        public void mousePressed(MouseEvent e)
        {
          report("main window click " + e.getX() + "," + e.getY());
        }
      });
    addWindowListener(new WindowAdapter()
      {
        public void windowClosing(WindowEvent e)
        {
          System.exit(0);
        }
      });
    setBounds(60, 60, 640, 300);
  }

  static String describe(Transferable t)
  {
    StringBuilder b = new StringBuilder();
    DataFlavor[] fs = t.getTransferDataFlavors();
    for (int i = 0; i < fs.length; i++)
      {
        try
          {
            Object o = t.getTransferData(fs[i]);
            if (fs[i].equals(DataFlavor.stringFlavor))
              b.append("[text '").append(o).append("']");
            else if (fs[i].equals(DataFlavor.javaFileListFlavor))
              b.append("[files ").append(o).append("]");
            else if (fs[i].equals(DataFlavor.imageFlavor))
              {
                Image img = (Image) o;
                b.append("[image ").append(img.getWidth(null)).append('x')
                 .append(img.getHeight(null));
                if (img instanceof BufferedImage)
                  b.append(" corner=#").append(Integer.toHexString(
                    ((BufferedImage) img).getRGB(0, 0)));
                b.append("]");
              }
            else if (o instanceof InputStream)
              {
                int n = 0;
                while (((InputStream) o).read() >= 0)
                  n++;
                b.append("[").append(fs[i].getMimeType().split(";")[0])
                 .append(" ").append(n).append(" bytes]");
              }
          }
        catch (Exception e)
          {
            b.append("[").append(fs[i].getHumanPresentableName())
             .append(" failed: ").append(e).append("]");
          }
      }
    return b.toString();
  }

  void addButton(Panel p, String label, final Runnable r)
  {
    Button b = new Button(label);
    b.addActionListener(new ActionListener()
      {
        public void actionPerformed(ActionEvent e)
        {
          report("button " + e.getActionCommand());
          r.run();
        }
      });
    p.add(b);
  }

  // Modal dialogs and always-on-top windows.

  void modal()
  {
    final Dialog d = new Dialog(this, "Modal dialog", true);
    d.setLayout(new FlowLayout());
    d.add(new Label("The main window is blocked."));
    Button close = new Button("Close");
    close.addActionListener(new ActionListener()
      {
        public void actionPerformed(ActionEvent e)
        {
          report("dialog close");
          d.setVisible(false);
          d.dispose();
        }
      });
    d.add(close);
    d.setBounds(160, 200, 300, 110);
    report("modal show");
    d.setVisible(true);
    report("modal returned");
  }

  Frame onTopFrame;

  void onTop()
  {
    if (onTopFrame == null)
      {
        onTopFrame = new Frame("Always on top");
        onTopFrame.add(new Label("I float above all windows",
                                 Label.CENTER));
        onTopFrame.setBounds(500, 140, 260, 90);
        onTopFrame.setAlwaysOnTop(true);
        onTopFrame.setVisible(true);
      }
    else
      onTopFrame.setAlwaysOnTop(! onTopFrame.isAlwaysOnTop());
    report("on top " + onTopFrame.isAlwaysOnTop());
  }

  // Pictures on the clipboard.

  static BufferedImage picture()
  {
    BufferedImage img = new BufferedImage(120, 80,
                                          BufferedImage.TYPE_INT_ARGB);
    for (int y = 0; y < 80; y++)
      for (int x = 0; x < 120; x++)
        img.setRGB(x, y, 0xFF000000 | (x * 2 << 16) | (y * 3 << 8) | 0x40);
    Graphics g = img.getGraphics();
    g.setColor(Color.white);
    g.drawString("Java", 40, 45);
    g.dispose();
    return img;
  }

  static class PictureSelection implements Transferable
  {
    final Image image;
    static DataFlavor html;

    PictureSelection(Image image)
    {
      this.image = image;
      try
        {
          html = new DataFlavor("text/html; class=java.lang.String");
        }
      catch (Exception e)
        {
          throw new RuntimeException(e);
        }
    }

    public DataFlavor[] getTransferDataFlavors()
    {
      return new DataFlavor[] { DataFlavor.imageFlavor, html,
                                DataFlavor.stringFlavor };
    }

    public boolean isDataFlavorSupported(DataFlavor f)
    {
      return f.equals(DataFlavor.imageFlavor) || f.equals(html)
             || f.equals(DataFlavor.stringFlavor);
    }

    public Object getTransferData(DataFlavor f)
      throws UnsupportedFlavorException
    {
      if (f.equals(DataFlavor.imageFlavor))
        return image;
      if (f.equals(html))
        return "<b>Picture</b> from Java";
      if (f.equals(DataFlavor.stringFlavor))
        return "Picture from Java";
      throw new UnsupportedFlavorException(f);
    }
  }

  void copyPicture()
  {
    getToolkit().getSystemClipboard().setContents(
      new PictureSelection(picture()), null);
    say("copied picture");
  }

  void paste()
  {
    Transferable t = getToolkit().getSystemClipboard().getContents(this);
    say("paste " + (t == null ? "nothing" : describe(t)));
  }

  /** Copies a picture, then reports clipboard changes for 40 s. */
  void clipboardImageTest()
  {
    copyPicture();
    Clipboard cb = getToolkit().getSystemClipboard();
    String last = null;
    long end = System.currentTimeMillis() + 40000;
    while (System.currentTimeMillis() < end)
      {
        pause(500);
        Transferable t = cb.getContents(this);
        String d = t == null ? "nothing" : describe(t);
        if (! d.equals(last))
          {
            report("clipboard now " + d);
            last = d;
          }
      }
  }

  // Printing.

  static void drawPage(Graphics g, int page, int w, int h)
  {
    g.setColor(Color.black);
    g.drawRect(0, 0, w - 1, h - 1);
    g.setFont(new Font("Serif", Font.BOLD, 28));
    g.drawString("BeKaffe printing, page " + page, 40, 70);
    g.setFont(new Font("SansSerif", Font.PLAIN, 12));
    g.drawString("Printed from Java on Haiku with BPrintJob.", 40, 100);
    g.setColor(Color.red);
    g.fillOval(40, 130, 120, 120);
    g.setColor(Color.blue);
    g.fillRect(200, 130, 160, 120);
    g.setColor(Color.black);
    for (int i = 0; i < 10; i++)
      g.drawLine(40, 280 + i * 12, 40 + i * 40, 400);
  }

  void printAwt()
  {
    PrintJob job = getToolkit().getPrintJob(this, "AWT print test", null);
    if (job == null)
      {
        say("print cancelled");
        return;
      }
    Dimension d = job.getPageDimension();
    report("print page " + d.width + "x" + d.height + " at "
           + job.getPageResolution() + " dpi");
    for (int p = 1; p <= 2; p++)
      {
        Graphics g = job.getGraphics();
        drawPage(g, p, d.width, d.height);
        g.dispose();
      }
    job.end();
    say("printed 2 pages");
  }

  void printJava2(boolean dialog)
  {
    PrinterJob job = PrinterJob.getPrinterJob();
    report("printer job " + job.getClass().getName());
    if (job.getClass().getName().indexOf("haiku") < 0)
      {
        // A GNU Classpath without the Haiku patch ignores
        // java.awt.printerjob; make the Haiku job directly.
        try
          {
            job = (PrinterJob) Class.forName(
              "gnu.java.awt.peer.haiku.HaikuPrinterJob").newInstance();
          }
        catch (Exception e)
          {
            say("no Haiku printer job: " + e);
            return;
          }
      }
    job.setJobName("Java 2 print test");
    job.setPrintable(new Printable()
      {
        public int print(Graphics g, PageFormat pf, int index)
        {
          if (index > 1)
            return NO_SUCH_PAGE;
          g.translate((int) pf.getImageableX(), (int) pf.getImageableY());
          drawPage(g, index + 1, (int) pf.getImageableWidth(),
                   (int) pf.getImageableHeight());
          return PAGE_EXISTS;
        }
      });
    try
      {
        if (dialog && ! job.printDialog())
          {
            say("print cancelled");
            return;
          }
        job.print();
        say("printed with PrinterJob");
      }
    catch (PrinterException e)
      {
        say("print failed " + e);
      }
  }

  // An input method client with inline composition.

  static class ImCanvas extends Canvas implements InputMethodRequests
  {
    String committed = "";
    String composed = "";

    ImCanvas()
    {
      setBackground(Color.white);
      enableEvents(AWTEvent.INPUT_METHOD_EVENT_MASK
                   | AWTEvent.MOUSE_EVENT_MASK);
      enableInputMethods(true);
    }

    public InputMethodRequests getInputMethodRequests()
    {
      return this;
    }

    public boolean isFocusable()
    {
      return true;
    }

    protected void processMouseEvent(MouseEvent e)
    {
      if (e.getID() == MouseEvent.MOUSE_PRESSED)
        requestFocus();
      super.processMouseEvent(e);
    }

    protected void processInputMethodEvent(InputMethodEvent e)
    {
      StringBuilder all = new StringBuilder();
      AttributedCharacterIterator it = e.getText();
      if (it != null)
        for (char c = it.first(); c != AttributedCharacterIterator.DONE;
             c = it.next())
          all.append(c);
      int n = e.getCommittedCharacterCount();
      committed += all.substring(0, n);
      composed = all.substring(n);
      StringBuilder hex = new StringBuilder();
      for (int i = 0; i < all.length(); i++)
        hex.append(" U+").append(Integer.toHexString(all.charAt(i)));
      report("im event committed=" + n + " text=[" + hex + " ]");
      repaint();
      e.consume();
    }

    public void paint(Graphics g)
    {
      g.setColor(Color.black);
      g.drawString("IM canvas: " + committed, 6, 20);
      g.setColor(Color.blue);
      g.drawString(composed, 6, 40);
      g.drawLine(6, 42, 6 + g.getFontMetrics().stringWidth(composed), 42);
    }

    public Rectangle getTextLocation(TextHitInfo offset)
    {
      Point p = getLocationOnScreen();
      return new Rectangle(p.x + 6, p.y + 26, 10, 16);
    }

    public TextHitInfo getLocationOffset(int x, int y)
    {
      return null;
    }

    public int getInsertPositionOffset()
    {
      return committed.length();
    }

    public AttributedCharacterIterator getCommittedText(int begin, int end,
      AttributedCharacterIterator.Attribute[] attributes)
    {
      return new java.text.AttributedString(committed.substring(begin, end))
             .getIterator();
    }

    public int getCommittedTextLength()
    {
      return committed.length();
    }

    public AttributedCharacterIterator cancelLatestCommittedText(
      AttributedCharacterIterator.Attribute[] attributes)
    {
      return null;
    }

    public AttributedCharacterIterator getSelectedText(
      AttributedCharacterIterator.Attribute[] attributes)
    {
      return null;
    }
  }

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

  public static void main(String[] args)
  {
    final AwtMore w = new AwtMore();
    w.setVisible(true);
    for (int i = 0; i < args.length; i++)
      {
        final String a = args[i];
        Runnable r = new Runnable()
          {
            public void run()
            {
              if (a.equals("modal")) w.modal();
              else if (a.equals("ontop")) w.onTop();
              else if (a.equals("cbimage")) w.clipboardImageTest();
              else if (a.equals("print1")) w.printAwt();
              else if (a.equals("print2")) w.printJava2(true);
              else if (a.equals("print3")) w.printJava2(false);
            }
          };
        if (a.equals("cbimage") || a.startsWith("print"))
          new Thread(r).start();
        else
          EventQueue.invokeLater(r);
      }
  }
}
