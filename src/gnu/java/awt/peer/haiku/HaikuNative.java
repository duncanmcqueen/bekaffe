/* HaikuNative.java -- part of the Haiku AWT peer
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

/**
 * Native entry points of the Haiku AWT peer (libhaikupeer).
 *
 * The native side runs a BApplication on its own native thread. Native
 * threads never call into Java; Java polls events with
 * {@link #nextEvent(int[], int)} from the {@link HaikuEventPump} thread.
 */
final class HaikuNative
{
  // Event types. Keep in sync with gnu_java_awt_peer_haiku_HaikuNative.cpp.
  static final int EV_CLOSE = 1;
  static final int EV_RESIZED = 2;
  static final int EV_MOVED = 3;
  static final int EV_MOUSE_DOWN = 4;
  static final int EV_MOUSE_UP = 5;
  static final int EV_MOUSE_MOVED = 6;
  static final int EV_MOUSE_ENTER = 7;
  static final int EV_MOUSE_EXIT = 8;
  static final int EV_KEY_DOWN = 9;
  static final int EV_KEY_UP = 10;
  static final int EV_ACTIVATED = 11;
  static final int EV_WHEEL = 12;
  static final int EV_APP_QUIT = 13;
  static final int EV_MENU = 14;
  static final int EV_CHOICE = 15;
  static final int EV_FILE_PANEL = 16;

  // Menu kinds for menuCreate().
  static final int MENU_BAR = 0;
  static final int MENU = 1;
  static final int MENU_POPUP = 2;

  // Haiku modifier bits (InterfaceDefs.h).
  static final int B_SHIFT_KEY = 0x1;
  static final int B_COMMAND_KEY = 0x2;

  /** Number of ints in one event record. */
  static final int EV_INTS = 8;

  // Window kinds for createWindow().
  static final int KIND_FRAME = 0;
  static final int KIND_DIALOG = 1;
  static final int KIND_WINDOW = 2;

  private static boolean initialized;

  private HaikuNative()
  {
  }

  /**
   * Loads libhaikupeer and starts the BApplication thread.
   */
  static synchronized void ensureInitialized()
  {
    if (initialized)
      return;
    String explicit = System.getProperty("gnu.java.awt.peer.haiku.library");
    if (explicit != null)
      System.load(explicit);
    else
      System.loadLibrary("haikupeer");
    if (! init())
      throw new java.awt.AWTError("Cannot start the Haiku BApplication");
    initialized = true;
  }

  private static native boolean init();

  /** out = width, height, dpi. */
  static native void screenInfo(int[] out);

  static native long createWindow(int id, int kind, String title,
                                  int x, int y, int w, int h);

  /** out = left, top, right, bottom. */
  static native void getInsets(long window, int[] out);

  static native void setTitle(long window, String title);

  static native void setVisible(long window, boolean visible);

  /** Bounds of the client area in screen coordinates. */
  static native void setBounds(long window, int x, int y, int w, int h);

  static native void setResizable(long window, boolean resizable);

  static native void toFront(long window);

  static native void toBack(long window);

  static native void dispose(long window);

  static native void blit(long window, int[] pixels, int imgW, int imgH,
                          int x, int y, int w, int h);

  static native int nextEvent(int[] out, int timeoutMs);

  static native void beep();

  // Menus. Handles are native BMenu / BMenuItem pointers.

  static native long menuCreate(int kind, String label);

  static native long menuItemCreate(int id, String label, int shortcut,
                                    int modifiers, boolean separator);

  static native void menuAddItem(long menu, long item, int index);

  static native void menuAddSubmenu(long menu, long submenu, int index);

  static native void menuAddSeparator(long menu);

  static native void menuRemoveItem(long menu, int index);

  static native void menuItemSetLabel(long item, String label);

  static native void menuItemSetEnabled(long item, boolean enabled);

  static native void menuItemSetMarked(long item, boolean marked);

  static native void menuSetLabel(long menu, String label);

  static native void menuSetEnabled(long menu, boolean enabled);

  static native void menuItemDispose(long item);

  static native void menuDispose(long menu);

  /** Attaches a menu bar (0 detaches) and returns its height. */
  static native int setMenuBar(long window, long menuBar);

  static native void popupShow(long menu, int x, int y);

  static native void choicePopup(int id, String[] items, int selected,
                                 int x, int y);

  // File panels.

  static native void filePanelShow(int id, boolean save, String title,
                                   String dir, String file);

  static native String fileResult(int id);
}
