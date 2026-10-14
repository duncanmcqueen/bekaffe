import java.awt.Graphics;
import java.awt.Font;
import java.awt.Color;

public class BigPalindrome extends java.applet.Applet {

    Font f = new Font("TimesRoman", Font.BOLD, 30);

    public void paint(Graphics g) {
        g.setFont(f);
        g.setColor(Color.red);
        g.drawString("Go hang a salami, I'm a lasagna hog.", 5, 25);
    }
}
