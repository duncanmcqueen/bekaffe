import java.awt.*;
import java.awt.event.*;

/**
 * Shows every AWT widget. Each event is printed and shown in the status
 * line. Run with an argument "open" or "save" to show a FileDialog at
 * start.
 */
public class AwtWidgets extends Frame
{
  Label status = new Label("Ready");

  void report(String s)
  {
    System.out.println("EVENT " + s);
    status.setText(s);
  }

  AwtWidgets()
  {
    super("AWT widgets");
    MenuBar mb = new MenuBar();
    Menu file = new Menu("File");
    MenuItem open = new MenuItem("Open" + "...", new MenuShortcut('O'));
    MenuItem save = new MenuItem("Save as" + "...");
    MenuItem quit = new MenuItem("Quit", new MenuShortcut('Q'));
    file.add(open);
    file.add(save);
    file.addSeparator();
    Menu recent = new Menu("Recent");
    recent.add(new MenuItem("one.txt"));
    recent.add(new MenuItem("two.txt"));
    file.add(recent);
    file.addSeparator();
    file.add(quit);
    Menu view = new Menu("View");
    final CheckboxMenuItem wrap = new CheckboxMenuItem("Word wrap", true);
    MenuItem disabled = new MenuItem("Disabled item");
    disabled.setEnabled(false);
    view.add(wrap);
    view.add(disabled);
    mb.add(file);
    mb.add(view);
    setMenuBar(mb);

    ActionListener menuAction = new ActionListener()
      {
        public void actionPerformed(ActionEvent e)
        {
          report("menu " + e.getActionCommand());
          if (e.getActionCommand().equals("Quit"))
            System.exit(0);
          if (e.getActionCommand().startsWith("Open")
              || e.getActionCommand().startsWith("Save"))
            showFileDialog(e.getActionCommand().startsWith("Open")
                           ? FileDialog.LOAD : FileDialog.SAVE);
        }
      };
    file.addActionListener(menuAction);
    recent.addActionListener(menuAction);
    wrap.addItemListener(new ItemListener()
      {
        public void itemStateChanged(ItemEvent e)
        {
          report("wrap " + wrap.getState());
        }
      });

    setLayout(new BorderLayout());
    Panel top = new Panel(new FlowLayout(FlowLayout.LEFT));
    final Choice choice = new Choice();
    choice.add("Red");
    choice.add("Green");
    choice.add("Blue");
    choice.addItemListener(new ItemListener()
      {
        public void itemStateChanged(ItemEvent e)
        {
          report("choice " + choice.getSelectedItem());
        }
      });
    top.add(choice);
    CheckboxGroup g = new CheckboxGroup();
    ItemListener radio = new ItemListener()
      {
        public void itemStateChanged(ItemEvent e)
        {
          report("radio " + e.getItem() + " " + e.getStateChange());
        }
      };
    for (String s : new String[] { "Small", "Medium", "Large" })
      {
        Checkbox cb = new Checkbox(s, g, s.equals("Medium"));
        cb.addItemListener(radio);
        top.add(cb);
      }
    final TextField tf = new TextField("type and press Enter", 18);
    tf.addActionListener(new ActionListener()
      {
        public void actionPerformed(ActionEvent e)
        {
          report("text " + tf.getText());
        }
      });
    top.add(tf);
    add(top, BorderLayout.NORTH);

    final List list = new List(6, false);
    for (int i = 1; i <= 20; i++)
      list.add("List item " + i);
    list.addItemListener(new ItemListener()
      {
        public void itemStateChanged(ItemEvent e)
        {
          report("list select " + list.getSelectedItem());
        }
      });
    list.addActionListener(new ActionListener()
      {
        public void actionPerformed(ActionEvent e)
        {
          report("list action " + e.getActionCommand());
        }
      });
    add(list, BorderLayout.WEST);

    ScrollPane sp = new ScrollPane();
    final Canvas big = new Canvas()
      {
        public void paint(Graphics gr)
        {
          for (int y = 0; y < 600; y += 50)
            for (int x = 0; x < 600; x += 50)
              {
                gr.setColor(((x + y) / 50) % 2 == 0 ? Color.white
                                                     : new Color(0x6080c0));
                gr.fillRect(x, y, 50, 50);
                gr.setColor(Color.black);
                gr.drawString(x / 50 + "," + y / 50, x + 12, y + 30);
              }
        }

        public Dimension getPreferredSize()
        {
          return new Dimension(600, 600);
        }
      };
    final PopupMenu popup = new PopupMenu("Canvas");
    popup.add(new MenuItem("Cut"));
    popup.add(new MenuItem("Copy"));
    popup.add(new MenuItem("Paste"));
    popup.addActionListener(new ActionListener()
      {
        public void actionPerformed(ActionEvent e)
        {
          report("popup " + e.getActionCommand());
        }
      });
    big.add(popup);
    big.addMouseListener(new MouseAdapter()
      {
        public void mousePressed(MouseEvent e)
        {
          if (e.isPopupTrigger())
            popup.show(big, e.getX(), e.getY());
          else
            report("canvas " + e.getX() + "," + e.getY());
        }
      });
    sp.add(big);
    add(sp, BorderLayout.CENTER);

    final Scrollbar sb = new Scrollbar(Scrollbar.VERTICAL, 50, 10, 0, 110);
    sb.addAdjustmentListener(new AdjustmentListener()
      {
        public void adjustmentValueChanged(AdjustmentEvent e)
        {
          report("scrollbar " + e.getValue());
        }
      });
    add(sb, BorderLayout.EAST);
    add(status, BorderLayout.SOUTH);

    addWindowListener(new WindowAdapter()
      {
        public void windowClosing(WindowEvent e)
        {
          System.exit(0);
        }
      });
    setBounds(60, 60, 640, 420);
  }

  void showFileDialog(int mode)
  {
    FileDialog fd = new FileDialog(this, mode == FileDialog.LOAD
                                         ? "Open a file" : "Save a file",
                                   mode);
    fd.setDirectory("/boot/home");
    fd.setFile("untitled.txt");
    fd.setVisible(true);
    report("file " + fd.getDirectory() + " | " + fd.getFile());
  }

  public static void main(final String[] args)
  {
    final AwtWidgets w = new AwtWidgets();
    w.setVisible(true);
    if (args.length > 0)
      EventQueue.invokeLater(new Runnable()
        {
          public void run()
          {
            w.showFileDialog(args[0].equals("save") ? FileDialog.SAVE
                                                    : FileDialog.LOAD);
          }
        });
  }
}
