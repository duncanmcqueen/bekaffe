# BeKaffe: AWT for Kaffe on Haiku

Project page: <https://duncanmcqueen.github.io/bekaffe/>

This repository has an AWT toolkit peer for the Kaffe Java VM on Haiku. With the peer,
AWT and Swing programs and JDK 1.0/1.1 applets run in native Haiku windows.

- VM: Kaffe 1.1.10-pre ([github.com/kaffe/kaffe](https://github.com/kaffe/kaffe), which is
  archived), interpreter engine, on Haiku R1/beta6 x86_64.
- Class library: GNU Classpath 0.98.
- The peer is `gnu.java.awt.peer.haiku`. Select it with
  `-Dawt.toolkit=gnu.java.awt.peer.haiku.HaikuToolkit`.

The name comes from the [BeKaffe project](http://bekaffe.sourceforge.net/) (1999), which
ported Kaffe 1.0 with its own AWT to BeOS. This peer is new code for the Classpath peer model.
It does not use the BeKaffe code.

## Screenshots

The screenshots from bekaffe.sourceforge.net (1999, BeOS, left) and the same applets on
Haiku in 2026 (right). The applet viewer is Kaffe 1.0's AppletViewer, the same viewer that
the 1999 screenshots show.

![Curve, TicTacToe, Blink and Animator](screenshots/bekaffe2-compare.png)

![ImageMap with its menu open, WireFrame and SimpleGraph](screenshots/bekaffe3-compare.png)

![BigPalindrome](screenshots/bekaffe-compare.png)

## What works

- Frame, Dialog and Window are native `BWindow`s. Mouse, keyboard, focus, resize and close
  events go to AWT.
- MenuBar, Menu, MenuItem, CheckboxMenuItem and menu shortcuts use a native `BMenuBar`.
  PopupMenu and the Choice list use `BPopUpMenu`. FileDialog uses `BFilePanel`.
- Canvas, Panel, Label, Button, Checkbox (also with CheckboxGroup), TextField, Scrollbar,
  List, ScrollPane and Choice. Swing components draw the widgets.
- Swing (Metal look and feel).
- The AWT 1.0 event model (`action()`, `handleEvent()`, `mouseUp()` and others). Old applets
  need it.
- Drawing is done in Java (`HaikuGraphics2D`) into a back buffer that is copied to a
  `BBitmap`. XOR mode is supported.

Not done yet: system clipboard, mouse cursors, audio, file name filters in FileDialog,
minimize and zoom, modal window feel, and integration into the Classpath build.

## Contents

| Path | Contents |
|------|----------|
| `src/gnu/java/awt/peer/haiku/` | The peer (Java). |
| `src/native/haiku-peer/` | The native library `libhaikupeer.so` (C++, Haiku API). |
| `src/resource/gnu/java/awt/font/fonts.properties` | Maps the Java logical fonts to the DejaVu fonts on Haiku. |
| `src/applet/bekaffe/applet/` | Kaffe 1.0's AppletViewer, changed to work with Classpath (see below). |
| `src/demos/` | Demo applets for the screenshots. |
| `src/test/` | Test programs. `AwtWidgets` shows all AWT widgets. |
| `src/build.sh`, `src/run.sh` | Build and run scripts for the Haiku side. |
| `sync.sh`, `runtest.sh`, `runav.sh` | Host scripts for a Haiku VM under QEMU (they use `../vm/vmssh.sh`, `mon.sh` and `shot.sh`). |
| `patches/kaffe/` | The Kaffe port to Haiku x86_64 (see below). |
| `patches/classpath/` | Fixes to GNU Classpath 0.98 that Kaffe on Haiku needs. |
| `tools/` | Screenshot helpers. `mousemode.cpp` sets the Haiku focus mode. |
| `screenshots/` | The 2026 screenshots, the comparisons, and the 1999 originals (`1999/`). |
| `docs/development-notes.md` | Detailed working notes: design, Kaffe and Classpath bugs, and how the screenshots were made. |

## Build

You need Haiku x86_64, the OpenJDK 8 package (only for `javac`), and GCC.

1. Kaffe: clone `https://github.com/kaffe/kaffe`, apply `patches/kaffe/0001-haiku-x86_64-port.patch`
   and `0002-regenerated-configure.patch` with `git apply`, then configure and build with the
   interpreter engine.
2. GNU Classpath 0.98: apply `patches/classpath/0001-kaffe-haiku-fixes.patch`. Configure
   with JNI natives and without the GTK and Qt peers. Link the JNI libraries with
   `-lnetwork`.
3. The peer: copy `src/` to the Haiku machine, set the paths at the top of `src/build.sh`
   and `src/run.sh` (they use `/boot/home/Projects/bekaffe`), and run `./build.sh`. The
   script compiles the peer into a `classes/` overlay that goes first on `BOOTCLASSPATH`,
   builds `lib/libhaikupeer.so` (`-lbe -ltracker`), and compiles the tests and demos.
4. Run: `./run.sh AwtWidgets`, or `./run.sh bekaffe.applet.AppletViewer demos/TicTacToe/example1.html`.

`docs/development-notes.md` has the full commands and the known problems.

## The Kaffe and Classpath patches

These patches are working-tree snapshots, not clean upstream patches.

- `0001-haiku-x86_64-port.patch`: Haiku host detection (`config.guess`, `config.alias`), the
  `config/x86_64/haiku/` platform files, `SIGIO`, header fixes, and a fix in
  `config/x86_64/sysdepCallMethod.h`: native methods that return `boolean`, `byte`, `char`
  or `short` are called through correctly typed function pointers. Without this fix the
  upper bits of `%eax` are garbage, and `Class.isPrimitive()` returns true for every class.
  The patch also has local workarounds: the version string is set to "Interpreter", and the
  shared library suffix is set to `.so`.
- `0002-regenerated-configure.patch`: the `configure` script after the Haiku changes.
- `patches/classpath/0001-kaffe-haiku-fixes.patch`: lazy `Thread.locals`, the static field
  order in `ResolverCache`, and `IFF_RUNNING` for Haiku.

## The applet viewer

`src/applet/bekaffe/applet/` is `kaffe.applet.AppletViewer` from Kaffe 1.0. Kaffe does not
load `kaffe.*` classes from the class path, so the package is now `bekaffe.applet`. Changes:
`-x` and `-y` window position options, the Java 1.4 `AppletContext` stream methods, the
applet gets its size before `init()`, a fix for the `documentBase` field, and a silent
`AudioPlayer` that does not block.

## Licenses

- The peer, the native library and `fonts.properties` are part of GNU Classpath style code:
  GPL version 2 or later, with the Classpath linking exception (see `COPYING` and the file
  headers).
- The applet viewer and the Kaffe patches: the Kaffe license (GPL version 2), see the file
  headers.
- The demo applets keep their own licenses:
  - Animator, Blink, ImageMap, SimpleGraph, TicTacToe and WireFrame: Sun Microsystems JDK
    1.0/1.1 demos, from [users.ece.utexas.edu/~garg/dist/demo/](https://users.ece.utexas.edu/~garg/dist/demo/).
    See the Sun notice in each source file.
  - Curve: by Michael Heinrichs (mike_heinrichs@hotmail.com). Free for non-commercial use
    (see the header of `Curve.java`).
  - BigPalindrome: the example applet from the 1999 BeKaffe screenshot, typed from the
    screenshot.
- The 1999 screenshots in `screenshots/1999/` are from
  [bekaffe.sourceforge.net/screen.html](http://bekaffe.sourceforge.net/screen.html).

## AI assistance

Claude Code (an AI model by Anthropic) wrote most of the code and text in this repository,
under the direction of the repository owner. All tests ran on a Haiku R1/beta6 x86_64 virtual machine.
