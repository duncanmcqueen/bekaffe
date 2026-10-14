/* HaikuClipboard.java -- the Haiku system clipboard
   Copyright (C) 2026 Free Software Foundation, Inc.

This file is part of GNU Classpath.

GNU Classpath is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Classpath is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Classpath; see the file COPYING.  If not, write to the
Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301 USA.

Linking this library statically or dynamically with other modules is
making a combined work based on this library.  Thus, the terms and
conditions of the GNU General Public License cover the whole
combination.

As a special exception, the copyright holders of this library give you
permission to link this library with independent modules to produce an
executable, regardless of the license terms of these independent
modules, and to copy and distribute the resulting executable under
terms of your choice, provided that you also meet, for each linked
independent module, the terms and conditions of the license of that
module.  An independent module is a module which is not derived from
or based on this library.  If you modify this library, you may extend
this exception to your version of the library, but you are not
obligated to do so.  If you do not wish to do so, delete this
exception statement from your version. */

package gnu.java.awt.peer.haiku;

import java.awt.datatransfer.Clipboard;
import java.awt.datatransfer.ClipboardOwner;
import java.awt.datatransfer.DataFlavor;
import java.awt.datatransfer.StringSelection;
import java.awt.datatransfer.Transferable;
import java.awt.datatransfer.UnsupportedFlavorException;
import java.io.IOException;

/**
 * The system clipboard, connected to Haiku's be_clipboard. Text is
 * exchanged with other applications as "text/plain" (UTF-8). Other
 * contents stay inside this Java application.
 */
final class HaikuClipboard
  extends Clipboard
{

  /** The be_clipboard count when this clipboard last matched it. */
  private long count = -1;

  HaikuClipboard()
  {
    super("System");
  }

  /**
   * Takes the text of the native clipboard if another application
   * changed it.
   */
  private void sync()
  {
    ClipboardOwner oldOwner = null;
    Transferable oldContents = null;
    synchronized (this)
      {
        long now = HaikuNative.clipboardCount();
        if (now == count)
          return;
        count = now;
        String text = HaikuNative.clipboardGetText();
        oldOwner = owner;
        oldContents = contents;
        owner = null;
        contents = text != null ? new StringSelection(text) : null;
      }
    if (oldOwner != null)
      oldOwner.lostOwnership(this, oldContents);
  }

  public synchronized void setContents(Transferable t, ClipboardOwner o)
  {
    super.setContents(t, o);
    String text = null;
    if (t != null && t.isDataFlavorSupported(DataFlavor.stringFlavor))
      {
        try
          {
            text = (String) t.getTransferData(DataFlavor.stringFlavor);
          }
        catch (UnsupportedFlavorException e)
          {
            // No text.
          }
        catch (IOException e)
          {
            // No text.
          }
      }
    // Other applications see the text, or an empty clipboard.
    count = HaikuNative.clipboardSetText(text != null ? text : "");
  }

  /** Also used by getAvailableDataFlavors() and getData(). */
  public Transferable getContents(Object requestor)
  {
    sync();
    return super.getContents(requestor);
  }
}
