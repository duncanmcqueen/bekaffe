package bekaffe.applet;

import java.lang.String;
import java.applet.AudioClip;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.net.URL;

public class AudioPlayer
  implements AudioClip
{
	boolean stop;
	URL     url;

public AudioPlayer( URL url) {
	this.url = url;
}

public void loop() {
	// AudioClip.loop() must not block; there is no audio output yet.
	stop = false;
}

public void play() {
	// No audio output in the Haiku AWT peer yet.
}

public void stop() {
	stop = true;
}
}
