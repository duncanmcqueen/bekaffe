package bekaffe.applet;

import java.applet.AudioClip;
import java.net.URL;
import javax.sound.sampled.AudioInputStream;
import javax.sound.sampled.AudioSystem;
import javax.sound.sampled.Clip;

/**
 * An applet audio clip played with javax.sound.sampled. The sound is
 * loaded the first time it plays. If it cannot be loaded or there is no
 * sound output, the clip is silent.
 */
public class AudioPlayer
  implements AudioClip
{
	URL     url;
	Clip    clip;
	boolean failed;

public AudioPlayer( URL url) {
	this.url = url;
}

synchronized Clip getClip() {
	if ( clip == null && !failed ) {
		try {
			AudioInputStream in = AudioSystem.getAudioInputStream( url);
			Clip c = AudioSystem.getClip();
			c.open( in);
			in.close();
			clip = c;
		}
		catch ( Exception x ) {
			failed = true;
		}
	}
	return clip;
}

public void loop() {
	Clip c = getClip();
	if ( c != null ) {
		c.stop();
		c.setFramePosition( 0);
		c.loop( Clip.LOOP_CONTINUOUSLY);
	}
}

public void play() {
	Clip c = getClip();
	if ( c != null ) {
		c.stop();
		c.setFramePosition( 0);
		c.start();
	}
}

public void stop() {
	Clip c;
	synchronized ( this ) {
		c = clip;
	}
	if ( c != null )
		c.stop();
}
}
