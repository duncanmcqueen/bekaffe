> Working notes from the development of this port. Paths such as `kaffe-awt/` refer to the
> author's work directory, which is the root of this repository.

# Handoff: write a minimal Haiku AWT toolkit peer for Kaffe 1.1.10 on Haiku

## UPDATE (2026-10-13, evening): clipboard, cursors, sound, filters, frame state, Classpath build

- **Clipboard:** `HaikuClipboard` (Toolkit.getSystemClipboard) writes text to `be_clipboard`
  as `text/plain` UTF-8 and takes text that other apps put there (it compares
  `be_clipboard->SystemCount()`; the old owner gets `lostOwnership`). Window shortcuts
  Command-X/C/V/A arrive as `B_CUT`/`B_COPY`/`B_PASTE`/`B_SELECT_ALL`, not as keys; the native
  view posts `EV_EDIT` and the pump posts Ctrl+X/C/V/A, which Swing text components use.
  Tested both ways with the Haiku `clipboard` CLI (non-BMP characters too).
- **Cursors:** the pump finds the component under the mouse (`Window.findComponentAt`) on each
  move and calls `HaikuWindowPeer.showCursor` (native `SetViewCursor` with a cached
  `BCursor(BCursorID)`). AWT text components get the I-beam. `setCursor` on any peer re-checks
  at the last mouse position. `createCustomCursor` makes a `BCursor` from a `B_RGBA32` bitmap
  (`HaikuCursor`). Note: Haiku's *default* cursor is the pointing hand.
- **Frame state:** `PeerWindow` overrides `Minimize` and `Zoom` (own toggle; a user resize
  ends the zoomed state) and posts `EV_STATE`; `HaikuFramePeer.nativeStateChanged` posts
  `WINDOW_ICONIFIED`/`DEICONIFIED`/`WINDOW_STATE_CHANGED`. `getState()` asks the native
  window. `setMaximizedBounds` -> `SetZoomLimits`. Classpath's `Frame.setState()` had the
  states swapped (fixed in the Classpath patch; `setExtendedState` was fine).
- **FileDialog filter:** `JRefFilter` (a `BRefFilter`) runs in the panel thread. For each new
  directory it queues a request, posts `EV_FILTER` and waits (3 s timeout = show all).
  `HaikuFileDialogPeer.filterRequested` runs the `FilenameFilter` on every name and answers
  with `filterAnswer`. Folders, volumes and links always show. A file name with `*` (e.g.
  `*.txt;*.java`) becomes a glob filter.
- **Sound:** `gnu.javax.sound.sampled.haiku` (`HaikuMixerProvider`, listed in
  `META-INF/services`): `HaikuClip` and `HaikuSourceDataLine` over one `BSoundPlayer` each
  (`-lmedia`). Java converts to 16-bit PCM (`Pcm`: 8/16-bit signed/unsigned, both byte
  orders, mu-law, a-law). The native callback does clip loops and the stream ring buffer.
  Classpath's `AudioSystem.getClip()` takes a Clip from `Mixer.getSourceLines()`, so the
  mixer returns new lines there. The applet viewer's `AudioPlayer` now uses
  `AudioSystem.getClip()`. Waits use native condition variables (Kaffe's `Thread.sleep`
  returns early). Verified: clip 1000 ms, loop of 2 extra half-seconds = 1.93 s, stream of
  22050 frames in 1008 ms, TicTacToe `yahoo1.au` (8 kHz mu-law) plays to the last frame.
  **Not heard:** QEMU's audio stream from the VM carries only zeros, also for a native
  `BSoundPlayer` tone (`tools/tonetest.cpp`), so the VM's AC'97 output is silent (mixer
  muted or driver issue). Not investigated, to leave the shared VM's settings alone.
- **Classpath build:** `cp-work/classpath-0.98` is a git repo (pristine 0.98 + one commit).
  Patch: `github/bekaffe/patches/classpath/0001-haiku-peer-and-fixes.patch`. It adds the
  sources, `native/jni/haiku-peer/Makefile.am`, `configure --enable-haiku-peer` (default on
  `haiku*`; also sets the default toolkit and `@HAIKU_MIXER_PROVIDER@`),
  `AC_SEARCH_LIBS(inet_pton, nsl network)`, `fonts-haiku.properties` (`OpenTypeFontPeer`
  tries `fonts-<os.name>.properties` first), and a `cpnet.c` fallback when
  `gethostbyname_r` is in a library but not declared (Haiku). Run `autoreconf -fi` on the
  host (autoconf 2.73, automake 1.19, libtool 2.6); the VM has no libtool. With the new
  `config.sub`, configure runs natively (`x86_64-unknown-haiku`), no `--build=linux` trick.
  VM build: `/boot/home/Projects/bekaffe/cp-haiku/{src,build,install}`, script
  `cp-haiku/build-cp.sh` (status file `cp-haiku/status`). The VM clock is ~5 h behind the
  host, so the script touches inputs, then generated files, before configure.
- **Classpath build result:** clean build from scratch OK (`cp-haiku/status` = DONE), and
  `run-classpath.sh AwtExtras clip cbtest` works with no overlay and no `-Dawt.toolkit`.
  Fixes found on the way (all in the patch): Haiku declares no `gethostbyname_r` but
  `libnetwork` has it (`cpnet.c` fallback); `configure.ac` had `statvfs \` + two spaces, so
  autoconf 2.73 checked for " mmap" and fonts failed ("memory-mapped files not
  implemented"); out-of-tree builds copied the stale empty `MixerProvider` service file from
  the source tree (`lib/Makefile.am` now copies configure's output); Classpath's JNI check
  needs `include/<class>.h` for every class with natives (made with OpenJDK 8 `javah`). The
  new build also detects all socket functions, which the old `--build=linux` build missed.
- **Shared VM:** other agents (opencode) use the same VM; keep GUI input to your own windows
  and prefer argument-driven tests (`AwtExtras cbtest|states|clip|loop|stream|filter|glob|au:FILE`).
- Still not done: modal/always-on-top window feel, drag and drop, printing, input methods.

## UPDATE (2026-10-13): all AWT widgets work; bekaffe.sourceforge.net screenshots recreated

- **Every peer is implemented.** Native Haiku parts: MenuBar/Menu/MenuItem/CheckboxMenuItem
  (a `BMenuBar` at the top of the window, counted in the insets; shortcuts use Command),
  PopupMenu (async `BPopUpMenu`), the Choice list (a temporary `BPopUpMenu`), and FileDialog
  (`BFilePanel`, open and save; `Dialog.show()` blocks until the panel closes). Swing-drawn
  parts: Scrollbar, List, ScrollPane, Choice (closed look). `HaikuSwingWrapper` sends mouse
  events to the deepest Swing child, keeps a grab while a button is held, and forwards keys.
  Selections come back as `EV_MENU`/`EV_CHOICE`/`EV_FILE_PANEL` in the normal event queue.
- **AWT 1.0 event model works.** Classpath's Swing peers call listeners directly and never
  post events, and Classpath does not translate item or adjustment events to the old
  `handleEvent()`/`action()` events. `HaikuButtonPeer`, `HaikuCheckboxPeer`,
  `HaikuTextFieldPeer`, `HaikuListPeer`, `HaikuScrollbarPeer` and `HaikuChoicePeer` post the
  new event and, when the component has no listener of that kind, the 1.0 event too.
  CheckboxGroup is supported (radio look, `setSelectedCheckbox`).
- **Other fixes:** XOR mode (`setXORMode`/`setPaintMode` threw in `AbstractGraphics2D`);
  windows get default colours (black on 0xDDDDDD), so inherited colours are never null;
  panels clear their background and give `paint()` the foreground colour and the right
  graphics background; empty Labels keep one line of height; decoded images are converted to
  INT_RGB/ARGB (indexed GIFs drew smeared through the generic ImagePaint path); scaled and
  background-colour `drawImage` have fast paths (the generic path took seconds per frame);
  click counts reach `MOUSE_CLICKED`; JViewport uses simple scroll mode.
- **Screenshots:** `kaffe-awt/screenshots/` has `bekaffe2-haiku.png`, `bekaffe3-haiku.png`,
  `bekaffe-haiku.png` and `*-compare.png` (1999 original beside the 2026 shot). The demos are
  in `src/demos/` (JDK 1.0/1.1 demo applets from users.ece.utexas.edu/~garg/dist/demo/, the
  Curve applet by Michael Heinrichs, and BigPalindrome typed from the 1999 screenshot).
  `runav.sh <Demo/page.html> [-x X -y Y]` starts the applet viewer on the VM.
- **Applet viewer:** `src/applet/bekaffe/applet/` is Kaffe 1.0's `kaffe.applet.AppletViewer`
  (the one in the 1999 shots), renamed because Kaffe refuses `kaffe.*` classes from the class
  path. Changes: `-x`/`-y` options, title "AppletViewer: <file>", no "(dummy)" menu labels,
  Java 1.4 `AppletContext` stream methods, applet sized before `init()`, status Label does not
  widen the window, `documentBase` self-assignment bug fixed, silent non-blocking AudioPlayer.
- **Kaffe/Classpath quirks found:** Kaffe's AppletTag parser cuts unquoted `param` values at
  `/` (quote them); `Thread.getAllStackTraces()` is unsupported (no thread dumps).
- **Screenshot helpers:** `tools/mousemode.cpp` (get/set the focus mode; "click to focus" lets
  a background window open its menu without raising) and `tools/palin.sh`. The screen was set
  to 800x600 / 640x480 for the shots and set back to `1280 800 24 75`; mouse mode back to 0.
- Still not done: system clipboard, cursors, audio, `FilenameFilter` in FileDialog, frame
  state (minimize/zoom), modal window feel, Classpath build integration.

## STATUS (2026-10-12): Milestones 1–3 work

Verified on the VM with screenshots and QMP input:

- **M1**: `new Frame(...)` opens a native titled BWindow at the requested position. The
  decorator insets come from the app_server (`top=29, left=5, bottom=5, right=5`). The close
  box sends `WINDOW_CLOSING`, and `System.exit` works.
- **M2**: `Canvas.paint` draws correctly (fillOval, drawLine, drawRect, drawString). `Label`
  and `Button` are drawn by the Swing-based peers. Mouse press, release and click arrive with
  correct component coordinates. Key events map to correct VK codes and chars, and the
  KeyboardFocusManager routes them to the focus owner. When the user resizes the window, the
  layout runs again and the window repaints. Window focus events are posted.
- **M3**: Swing works: a `JFrame` with JLabel, JTextField, JCheckBox and JButton draws in Metal
  style. Typing, the checkbox and button actions work, and so does `EXIT_ON_CLOSE`.
  `BufferedImage.createGraphics()` renders off-screen through `HaikuGraphics2D`.

### Where things are

- Host (the source of truth): `kaffe-awt/`
  - `src/gnu/java/awt/peer/haiku/*.java`: the peer. It has these classes: HaikuToolkit,
    HaikuGraphicsEnvironment, -Device, -Configuration, HaikuWindowPeer, -FramePeer,
    -DialogPeer, HaikuGraphics2D, HaikuVolatileImage, HaikuEventPump, HaikuNative.
  - `src/native/haiku-peer/gnu_java_awt_peer_haiku_HaikuNative.cpp`: the native lib
    (`libhaikupeer.so`, `-lbe`).
  - `src/resource/gnu/java/awt/font/fonts.properties`: maps the logical fonts to the DejaVu
    TTFs in `/boot/system/data/fonts/ttfonts`.
  - `src/test/`: AwtFrame, AwtCanvas, SwingTest, ReflectTest.
  - `patches/kaffe/0001-haiku-x86_64-port.patch` (this repo) includes a Kaffe VM fix (see below).
  - `sync.sh`: stops old test runs, pushes `src/` to the VM, and runs `build.sh`.
    `runtest.sh <Class> <wait> <png> [args]`: starts a test detached, waits, and takes a
    screenshot.
  - `ref/`: Classpath reference sources pulled from the VM (X peer, Swing peers, java2d).
- VM: `/boot/home/Projects/bekaffe/haiku-peer/`. `build.sh` compiles the classes into
  `classes/` (an overlay that goes first on `BOOTCLASSPATH`) and builds `lib/libhaikupeer.so`.
  `run.sh <Class>` runs a test with `-Dawt.toolkit=gnu.java.awt.peer.haiku.HaikuToolkit`.
  Set `KAFFE_OPTS=-Dgnu.java.awt.peer.haiku.debug=true` to trace native events.
  - Do not put the overlay `classes/` dir on `-classpath`. That stops Kaffe's app loader from
    finding any class. `BOOTCLASSPATH` alone is enough.

### Design

- All rendering is done in Java. Each window has an INT_RGB backing `BufferedImage` the size
  of the client area. `HaikuGraphics2D` (which extends `AbstractGraphics2D`) draws into it.
  Fast paths exist for solid fills, 1-px lines, image blits, coverage-blended scanlines (for
  text and antialiasing), gradient and other `Paint` scanlines, and copyArea. When the
  graphics is disposed, the dirty rectangle is blitted into a `BBitmap`, and the `BView`
  draws that bitmap in `Draw()`.
- Native threads never call into the JVM. The native lib runs `BApplication::Run()` on its
  own `spawn_thread`, and its BWindow/BView hooks put events in a mutex/condvar queue. The
  `HaikuEventPump` Java daemon thread polls `nextEvent(int[], 100ms)` and posts AWT events.
  This keeps Haiku looper threads out of Kaffe's thread system and GC.
- Widgets inside windows use Classpath's `gnu.java.awt.peer.swing` peers (as the X peer does).
  Text uses `gnu.java.awt.font.OpenTypeFontPeer` (a pure-Java TrueType engine).
- AWT window bounds include the decorations. The native frame is the client area only. Window
  graphics are translated by (-left, -top).

### Kaffe VM bug found and fixed (rebuilt `libkaffevm.so`)

`config/x86_64/sysdepCallMethod.h` called every 1-slot non-float native through
`jint (*)()`. The x86-64 ABI defines only `%al` for a `jboolean` return, so garbage upper
bits made `VMClass.isPrimitive()` return **true for every class**. Swing's `UIDefaults.getUI`
then failed in `Method.invoke` ("cannot happen"). The earlier `Method.java` array guard was
working around the same bug. The fix calls through correctly typed pointers for `Z/B/C/S`.
The fix is part of `patches/kaffe/0001-haiku-x86_64-port.patch` in this repo. The original header is kept on the VM as
`sysdepCallMethod.h.orig`.

### Pitfalls found

- `Thread.sleep(n)` returns well before `n` ms on Kaffe/Haiku. (Not investigated.
  Possibly signal interruption.) The tests loop on `System.currentTimeMillis()`.
- `AbstractGraphics2D` bugs that the peer works around: the default `rawDrawLine`,
  `rawDrawRect`, `rawFillRect` and `rawDrawImage` take device coordinates but call
  `draw`/`fill`, which applies the transform again. The default `renderScanline` throws an
  NPE when the paint context has no alpha raster (any opaque `Color`). `clearRect` passes
  user coordinates to `rawClearRect`. `drawImage` silently ignores images that are not
  `RenderedImage` (for example `VolatileImage`).
- Haiku has no `setsid` or `killall`. Use the `ps | grep kaffe-latest/kaffe/kaffe/ | kill -9`
  pattern (see `sync.sh`). An SSH call that starts a detached job can stay open, so wrap it
  in `timeout`. QEMU `screendump` can show a blanked screen, so move the mouse first
  (`mon.sh 'mouse_move 1 1'`).

### Next steps

1. ~~Missing peers~~ Done on 2026-10-13 (see UPDATE above).
2. Clipboard: connect `getSystemClipboard()` to `be_clipboard` (it is a local `Clipboard`
   now). Cursors: `BCursor`. Modifier-only key events: `B_MODIFIERS_CHANGED`. Frame state
   (Minimize/Zoom), always-on-top (`B_FLOATING_ALL_WINDOW_FEEL`), and modal dialogs
   (`B_MODAL_APP_WINDOW_FEEL`).
3. Performance: rendering is pure Java on Kaffe and is slow. It has not been measured.
4. Integrate into the Classpath build. Copy `gnu/java/awt/peer/haiku` into `classpath-0.98`,
   replace `resource/gnu/java/awt/font/fonts.properties`, add `native/jni/haiku-peer/` with a
   `Makefile.am` (`libhaikupeer.la`, `-lbe`) and a configure switch, and maybe make
   HaikuToolkit the default `awt.toolkit` on Haiku.
5. Get the Kaffe `sysdepCallMethod.h` fix upstream or into the port, and check
   whether the `Method.java` array guard is still needed.

---

## Original handoff (before this work)


## Goal

Kaffe 1.1.10-pre on Haiku now runs **command-line** Java (see "Working CLI setup").
AWT/javax.swing **do not** work yet because GNU Classpath 0.98's AWT needs a
native *toolkit peer*, and the only working peers (GTK, Qt, X) are absent on Haiku.

Task: write a **minimal Haiku AWT peer** so at least a `java.awt.Frame` window
appears on Haiku's app_server, plus basic widgets/text. Start tiny
(Frame + Canvas + Label/Button), expand later.

There is an old full BeOS AWT implementation in the Kaffe **1.0.5** tree
(`build/kaffe/libraries/clib/awt/BeOS/`), but it targets Kaffe's own `kaffe.awt`
AWT, **not** Classpath's peer model, so it can only be used as a C API reference
for "how to talk to the BeOS/Haiku GUI", not copied in.

---

## Environment / access

- **Host**: `/home/dwmcqueen/Projects/HaikuOS/` (Ubuntu-ish, fish on host, bash in guest).
- **VM**: QEMU/KVM Haiku **R1~beta6 hrev60228 x86_64**, hostname `shredder`, user `root`,
  home `/boot/home`. 12 GB RAM, 6 vCPU, e1000 NIC.
  - Start (detached, survives agent exit):
    ```
    cd /home/dwmcqueen/Projects/HaikuOS/vm
    systemd-run --user --unit=haiku-vm --collect --working-directory=$PWD \
      -E DISPLAY -E WAYLAND_DISPLAY -E XDG_RUNTIME_DIR ./run-haiku.sh
    ./mon.sh 'hostfwd_add tcp:127.0.0.1:2222-:22'   # REQUIRED after every start
    ```
  - Run commands in guest: `vm/vmssh.sh '<bash command>'`
  - Copy files in: `vm/vmpush.sh <local> <guest-abs-path>`  (no sftp/scp)
  - QEMU monitor: `vm/mon.sh '<cmd>'`  (e.g. `send-key alt+sysgrq+d` for KDL; `info usernet`)
  - VM serial/KDL log: `vm/haiku-serial.log` (look for `PANIC:` / `kdebug>`)
- **Guest `/tmp` is wiped on every boot.** The Kaffe work lives under
  `/boot/home/Projects/bekaffe/` and persists.
- **Never** modify `/boot/system/**` except `/boot/system/non-packaged/**`.
- Long builds: run detached with `nohup ... </dev/null >/dev/null 2>&1 &` and poll a status file.
- Poll SSH rarely (one check per many minutes) — memory pressure can wedge sshd.

---

## Working CLI setup (already working — reuse this)

Build tree: `/boot/home/Projects/bekaffe/checkout/kaffe-latest` (git clone of `github.com/kaffe/kaffe`, 1.1.10-pre).

Classpath: `/boot/home/Projects/bekaffe/cp-098-jni/` — GNU Classpath **0.98** built with
JNI natives (+ local patches). Runtime library: `cp-098-jni/lib/glibj.zip`.

Classpath install layout (needed so `java.security.Security` loads providers locally
instead of hitting the network): `/boot/home/Projects/bekaffe/cp-install/lib/security/classpath.security`.

Run any Java class:

```sh
cd /boot/home/Projects/bekaffe/checkout/kaffe-latest
VMJ=libraries/javalib/vmspecific/kaffe-vminterface.jar
GLIBJ=/boot/home/Projects/bekaffe/cp-098-jni/lib/glibj.zip
N=/boot/home/Projects/bekaffe/cp-098-jni/native/jni
export KAFFEHOME=/boot/home/Projects/bekaffe/cp-install
export KAFFELIBRARYPATH="$N/java-nio/.libs:$N/java-io/.libs:$N/java-lang/.libs:$N/java-util/.libs:$N/java-net/.libs:$N/native-lib/.libs:$PWD/kaffe/kaffevm/.libs"
export BOOTCLASSPATH="$VMJ:$GLIBJ"
./kaffe/kaffe/kaffe-bin -classpath "$VMJ:$GLIBJ:<appdir>" YourMain arg1 arg2
```

Verified: `Hello` (args), and a test using HashMap/enhanced-for/autoboxing/exceptions/String ops.
`-Djava.awt.headless=true` alone is NOT enough (see below) — it still needs the peer.

Details of the patches already applied (keep them):
1. `libraries/javalib/vmspecific/java/lang/reflect/Method.java`: guard primitive branch
   against array types (`pt.isPrimitive() && pt.getName().charAt(0) != 0x5b`).
2. `libraries/javalib/vmspecific/java/lang/Thread.java`: `locals` non-final + lazy init.
3. Classpath `java/net/ResolverCache.java`: declare `cache`/`killqueue` before the
   `POSITIVE_TTL = getTTL(...)` initializer.
4. Classpath JNI natives relinked with `-lnetwork` (Haiku's `__h_errno` is a function in
   `libnetwork.so`, not libroot); `IFF_RUNNING` defined in `java_net_VMNetworkInterface.c`.
5. `KAFFEHOME` points at the Classpath install layout above.

---

## Why AWT currently fails (exact evidence)

```
$ kaffe-bin ... AwtTest        # new java.awt.Frame("t")
creating Frame...
java.awt.AWTError: Cannot load AWT toolkit: gnu.java.awt.peer.gtk.GtkToolkit
  at java.awt.Toolkit.getDefaultToolkit (Toolkit.java:607)
  at java.awt.GraphicsEnvironment.getLocalGraphicsEnvironment (GraphicsEnvironment.java:103)
  at java.awt.Window.<init> (Window.java:133)
  at java.awt.Frame.<init> (Frame.java:246)
```

Headless is not a workaround — Classpath's headless graphics env still delegates
drawing to GTK:

```
$ kaffe-bin -Djava.awt.headless=true ... HeadTest   # BufferedImage.getGraphics()
headless=true
java.lang.UnsatisfiedLinkError: Native library `gtkpeer' not found
  (as file `libgtkpeer.so') in gnu.classpath.boot.library.path and java.library.path [...]
  at gnu.java.awt.peer.headless.HeadlessGraphicsEnvironment.createGraphics (HeadlessGraphicsEnvironment.java:77)
  at java.awt.image.BufferedImage.createGraphics (BufferedImage.java:407)
```

Facts:
- The 1.1.10 tree has **no AWT native peer** (`libraries/clib/` = `Makefile*`, `native`, `zip`; no `awt/`).
- `libgtk` is not present on the VM, and the Classpath build has no gtk-peer.
- Classpath 0.98 ships peers: `gnu/java/awt/peer/{gtk,qt,x,headless,swing,ClasspathFontPeer,...}`.
  No Haiku/BeOS peer.

---

## How Classpath AWT picks a toolkit (what to hook into)

`java/awt/Toolkit.java`:
- `Toolkit.getDefaultToolkit()` (line ~565) reads system property **`awt.toolkit`**,
  default `"gnu.java.awt.peer.gtk.GtkToolkit"`:
  ```java
  String toolkit_name = SystemProperties.getProperty("awt.toolkit",
                                                     "gnu.java.awt.peer.gtk.GtkToolkit");
  ```
- It loads that class via `Class.forName`, checks it's a `Toolkit`, and on failure
  either falls back to `HeadlessToolkit` (if `java.awt.headless=true`) or throws the
  `AWTError` above.

**=> A Haiku peer does NOT require editing `Toolkit.java`.** Put the class in glibj and run
with `-Dawt.toolkit=gnu.java.awt.peer.haiku.HaikuToolkit` (or change the default string).
`GraphicsEnvironment.getLocalGraphicsEnvironment()` (line ~77) just calls
`Toolkit.getDefaultToolkit().getLocalGraphicsEnvironment()`.

A toolkit peer must extend `java.awt.Toolkit` and (strongly recommended) implement
`gnu.java.awt.ClasspathToolkit`. Abstract methods to implement:

`java.awt.Toolkit` abstract methods (`java/awt/Toolkit.java`):
```
getScreenSize()                 (499)
getScreenResolution()           (508)
getColorModel()                 (527)
getFontList()                   (536)
getFontMetrics(Font)            (547)
sync()                          (553)
getImage(String)                (620)
getImage(URL)                   (628)
createImage(String)             (630)
createImage(URL)                (632)
prepareImage(Image,w,h,ImageObserver)  (648)
checkImage(Image,w,h,ImageObserver)    (665)
createImage(ImageProducer)      (675)
createImage(byte[],int,int)     (701)
getPrintJob(Frame,String,Properties)   (719)
beep()                          (758)
getSystemClipboard()            (767)
createDragSourceContextPeer(DragGestureEvent)  (993)
getDesktopProperty(String)      (1287)
isModalExclusionTypeSupported(Dialog.ModalExclusionType)  (1290)
isModalityTypeSupported(Dialog.ModalityType)              (1293)
```
plus the per-widget `protected XXXPeer createXXX(...)` factories (see `HeadlessToolkit`
for the full list; those that can't work yet should `throw new HeadlessException()`).

`gnu.java.awt.ClasspathToolkit` abstract methods (`gnu/java/awt/ClasspathToolkit.java`):
```
Lucide  getLocalGraphicsEnvironment()      (97)   -> return your HaikuGraphicsEnvironment
        getClasspathFontPeer(String,Map)   (112)
        createFont(int, InputStream)       (181)
        createRobot(GraphicsDevice)        (186)   -> may throw AWTException for now
        createEmbeddedWindow(EmbeddedWindow)(195)  -> may throw for now
```

`java.awt.GraphicsEnvironment` abstract methods (`java/awt/GraphicsEnvironment.java`):
```
getScreenDevices()               (150)
getDefaultScreenDevice()         (158)
createGraphics(BufferedImage)    (166)   -> return a Graphics2D over the image
getAllFonts()                    (180)
getAvailableFontFamilyNames()    (192)
getAvailableFontFamilyNames(Locale) (206)
```

Peer interfaces live in `java/awt/peer/`:
`ComponentPeer, ContainerPeer, WindowPeer, FramePeer, DialogPeer, CanvasPeer, PanelPeer,
LabelPeer, ButtonPeer, TextFieldPeer, TextComponentPeer, TextAreaPeer, ScrollbarPeer,
ScrollPanePeer, CheckboxPeer, CheckboxMenuItemPeer, ChoicePeer, ListPeer, MenuPeer,
MenuBarPeer, MenuComponentPeer, MenuItemPeer, PopupMenuPeer, FontPeer, Clipboard-related,
RobotPeer, MouseInfoPeer, DesktopPeer, FileDialogPeer, LightweightPeer`.

---

## Recommended implementation strategy

### Best templates to read first
- **`gnu/java/awt/peer/headless/HeadlessToolkit.java` (385 lines)** and
  `HeadlessGraphicsEnvironment.java` — every method, minimal stubs. Copy this shape and
  replace the `HeadlessException` methods with Haiku-backed ones. It is the smallest
  complete `Toolkit`+`GraphicsEnvironment` example.
- **`gnu/java/awt/peer/gtk/`** — the full reference peer (`GtkToolkit`, `GtkFramePeer`,
  `GtkComponentPeer`, image/font/clipboard peers) plus its JNI in
  `native/jni/gtk-peer/gnu_java_awt_peer_gtk_*.c`. Use it to see how a real peer drives a
  native toolkit and how the native lib is wired (`nativeexeclib_LTLIBRARIES = libgtkpeer.la`).
- **`gnu/java/awt/peer/x/`** — a pure-Java toolkit (`XToolkit`, `XFramePeer`,
  `XGraphics2D`, `XGraphicsDevice`, `XGraphicsEnvironment`, `XFontPeer`) if you prefer a
  mostly-Java peer with a thin native shim.
- **1.0.5 BeOS C API reference**: `/boot/home/Projects/bekaffe/build/kaffe/libraries/clib/awt/BeOS/`
  (files `wnd.c evt.c gra.c fnt.c clr.c img.c japp.c tlk.c graphics.h keysyms.h toolkit.h`;
  `Makefile.am` builds `libawt.la` from these). This is Kaffe-1.0.x `kaffe.awt`, unrelated
  to Classpath peers, but shows how the old code opened a BeOS window and pumped events.

### Suggested class layout (put under Classpath `gnu/java/awt/peer/haiku/`)
```
HaikuToolkit.java            extends java.awt.Toolkit implements gnu.java.awt.ClasspathToolkit
HaikuGraphicsEnvironment.java  extends java.awt.GraphicsEnvironment
HaikuGraphicsDevice.java       extends java.awt.GraphicsDevice
HaikuGraphicsConfiguration.java extends java.awt.GraphicsConfiguration
HaikuFramePeer.java            implements java.awt.peer.FramePeer   (extends a HaikuWindowPeer)
HaikuWindowPeer.java           implements java.awt.peer.WindowPeer
HaikuCanvasPeer.java           implements java.awt.peer.CanvasPeer
HaikuLabelPeer.java            implements java.awt.peer.LabelPeer
HaikuButtonPeer.java           implements java.awt.peer.ButtonPeer
```
Native side: `native/jni/haiku-peer/gnu_java_awt_peer_haiku_*.c`, building `libhaikupeer.la`,
linked against Haiku's `libbe.so` (`-lbe`).

### The bidirectional problem (the hard part)
AWT is push-based: the Java peer calls native to create/adjust a widget, and native events
must call back into Java. Classpath's GTK peer solves this with a Gtk main loop.
On Haiku:
- Java→native: use `BWindow`/`BView` created from native code, or (cleaner) create your own
  `BWindow` subclass in C++ and a `JNIEnv*` capture to call back into
  `ComponentPeer.handleEvent(AWTEvent)` / `EventQueue`.
- native→Java: implement `BWindow::MessageReceived`/`BView::MessageReceived` (or a
  `BHandler`/`BMessageRunner`) and marshal into Java AWT events on the right thread.
  AWT requires posting onto the **EventQueue**; see how Classpath peers schedule events.
- **Threading**: Haiku requires one `BApplication` per process and the app's main thread
  runs the `BApplication::Run()` message loop. AWT's `EventQueue` runs on its own Java
  thread. You will likely need to start `BApplication`+`Run()` on a dedicated native thread
  and forward messages to the Java `EventQueue`. This is the main design risk.

### Milestone 1 (do this first — small)
Just get a window:
1. `HaikuToolkit implements ClasspathToolkit` with stubs; `getLocalGraphicsEnvironment()`
   returns a `HaikuGraphicsEnvironment`; `getScreenSize()`/`getScreenResolution()` return
   fixed values (e.g. from Haiku's `BScreen`).
2. `HaikuFramePeer` creates a `BWindow` with a `BView` (title, size, quit-on-close) and
   calls `Show()`.
3. Run a test that only does `new Frame("hi"); f.setSize(200,100); f.setVisible(true);`
   and verify a window appears on the VM's screen (use `vm/shot.sh` or view the VM GUI).
4. `Toolkit.createImage`/`getImage` can return null at first; the window is the goal.

### Milestone 2
Canvas + custom `paint(Graphics)` drawing, Label/Button, window resizing + events
(WM_DELETE → `WindowEvent.WINDOW_CLOSING`), so `f.addWindowListener`+`System.exit(0)` works.

### Milestone 3
Fonts (`getFontMetrics`, `getAllFonts` via Haiku `BFont`), `ColorModel`, off-screen images
(`BufferedImage.getGraphics`), then Swing (Swing needs a working `Graphics2D` — see
Classpath's `java2d` package and `XGraphics2D` as a model).

---

## Build integration

### Rebuild glibj.zip after editing Classpath Java sources
```sh
cd /boot/home/Projects/bekaffe/cp-098-jni
export PATH=/boot/system/lib/openjdk8/bin:$PATH
make JAVAC="/boot/system/lib/openjdk8/bin/javac -source 1.6 -target 1.6 -Xlint:none" -j2
# output: /boot/home/Projects/bekaffe/cp-098-jni/lib/glibj.zip
```
Kaffe accepts class file version ≤ 50, so **always compile with `-source 1.6 -target 1.6`**
(it warns "bootstrap class path not set" — harmless). openjdk8 javac is at
`/boot/system/lib/openjdk8/bin/`.

### Build the native peer lib
Model on `native/jni/gtk-peer/Makefile.am`. Add a `native/jni/haiku-peer/` with
`nativeexeclib_LTLIBRARIES = libhaikupeer.la`, sources `gnu_java_awt_peer_haiku_*.c`,
`libhaikupeer_la_LIBADD = -lbe` (Haiku's Application Kit). Build with the same libtool
setup already working in this tree (ELF shared libs were transplanted; `-module`).
Place the resulting `.so` where Kaffe's `java.library.path` looks — for Kaffe that is
**`KAFFELIBRARYPATH`** (confirmed: the UnsatisfiedLinkError above printed the
`KAFFELIBRARYPATH` dirs as `java.library.path`). Easiest: add the new `.libs` dir to
`KAFFELIBRARYPATH`, or call `System.load("<abs path>")` explicitly from the toolkit's
static initializer for a first smoke test.
Classpath native loading also consults `gnu.classpath.boot.library.path`.

### Rebuild the Kaffe launcher/VM (only if you change VM C)
```sh
cd /boot/home/Projects/bekaffe/checkout/kaffe-latest
export PATH=/boot/home/Projects/bekaffe/checkout/kaffe-latest/replace:$PATH
make -C kaffe -j2          # -> kaffe/kaffe/kaffe-bin, kaffe/kaffevm/.libs/libkaffevm.so
```

---

## Gotchas / notes

- **`gnu.classpath.home.url`** is `"file://" + java.home + "/lib"` (Kaffe's
  `vmspecific/gnu/classpath/VMSystemProperties.java:222`). `java.home` = env **`KAFFEHOME`**
  (`gnu_classpath_VMSystemProperties.c:211`, default `"."`). Keep `KAFFEHOME` pointing at
  `cp-install` or Security init will try to load provider config from a bad path and the
  bootstrap will die in `VMInetAddress.getHostByName`.
- Filesystem has shown **corruption** of generated files before (zeroed/ELF-clobbered
  `config.h`, `libtool`, Makefiles). If a generated file looks wrong, regenerate with
  `./config.status` rather than hand-editing.
- `-lnetwork` is needed for anything touching the socket/native-lib code.
- Native debug: Kaffe's `KAFFELIBRARYPATH` search and `NATIVELOAD_FAIL` style prints were
  used before; Kaffe's `external.c` dlopen path is where "Native library X not found"
  originates.
- Debug the VM from host: `vm/vmssh.sh`, `vm/shot.sh` (GUI screenshot), `vm/mon.sh`
  (`send-key alt+sysgrq+d` → KDL to `vm/haiku-serial.log`).
- The 1.1.10 tree builds with the system `zziplib`/`glib2`; keep `/boot/system/lib` on
  `LIBRARY_PATH` for ICU-style tools if you touch those areas.

---

## Key paths (guest `/boot/home/Projects/bekaffe/`)

| Path | What |
|---|---|
| `checkout/kaffe-latest/` | Kaffe 1.1.10-pre source + build (`kaffe/kaffe/kaffe-bin`) |
| `checkout/kaffe-latest/kaffe/kaffevm/.libs/` | `libkaffevm.so` (in KAFFELIBRARYPATH) |
| `checkout/kaffe-latest/libraries/javalib/vmspecific/kaffe-vminterface.jar` | VM interface classes |
| `cp-098-jni/lib/glibj.zip` | GNU Classpath 0.98 runtime (editable via rebuild) |
| `cp-098-jni/native/jni/*/.libs/` | Classpath JNI natives (in KAFFELIBRARYPATH) |
| `cp-098-jni/native/jni/gtk-peer/` | GTK peer reference (Java in `classpath-0.98/gnu/java/awt/peer/gtk/`) |
| `cp-install/lib/security/` | Classpath install layout (KAFFEHOME target) |
| `classpath-0.98/` | GNU Classpath 0.98 source (java/awt, gnu/java/awt/peer/*) |
| `build/kaffe/libraries/clib/awt/BeOS/` | Kaffe **1.0.5** BeOS AWT peer (C reference only) |

Host helpers: `/home/dwmcqueen/Projects/HaikuOS/vm/{vmssh.sh,vmpush.sh,vmpull.sh,mon.sh,shot.sh,run-haiku.sh}`.
