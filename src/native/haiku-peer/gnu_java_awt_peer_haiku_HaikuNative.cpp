/* gnu_java_awt_peer_haiku_HaikuNative.cpp -- native side of the Haiku AWT peer
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

/*
 * Design notes.
 *
 * All rendering happens in Java (gnu.java.awt.java2d.AbstractGraphics2D
 * over an int[] backing image).  The native side only:
 *   - runs one BApplication on its own native thread,
 *   - creates one BWindow + BView per AWT heavyweight window,
 *   - copies dirty rectangles of the Java backing image into a BBitmap
 *     that the BView draws in Draw(),
 *   - queues input/window events.
 *
 * Native threads never call into the JVM.  Java polls the event queue
 * with nextEvent(), which blocks for a short time and returns a packed
 * int record.  This keeps Haiku looper threads unknown to the VM's
 * thread system and garbage collector.
 */

#include <jni.h>

#include <Application.h>
#include <Beep.h>
#include <Bitmap.h>
#include <Entry.h>
#include <FilePanel.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Message.h>
#include <Messenger.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <Screen.h>
#include <View.h>
#include <Window.h>
#include <OS.h>

#include <pthread.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <deque>
#include <map>
#include <string>

/* Event types.  Keep in sync with HaikuNative.java. */
enum {
  EV_CLOSE = 1,       /* window close box */
  EV_RESIZED = 2,     /* a = client width, b = client height */
  EV_MOVED = 3,       /* a = client x, b = client y (screen) */
  EV_MOUSE_DOWN = 4,  /* a = x, b = y, c = buttons, d = modifiers, e = clicks */
  EV_MOUSE_UP = 5,    /* a = x, b = y, c = buttons, d = modifiers */
  EV_MOUSE_MOVED = 6, /* a = x, b = y, c = buttons, d = modifiers */
  EV_MOUSE_ENTER = 7, /* a = x, b = y, c = buttons, d = modifiers */
  EV_MOUSE_EXIT = 8,  /* a = x, b = y, c = buttons, d = modifiers */
  EV_KEY_DOWN = 9,    /* a = raw char, b = unicode char, c = modifiers, d = key, e = repeat */
  EV_KEY_UP = 10,     /* a = raw char, b = unicode char, c = modifiers, d = key */
  EV_ACTIVATED = 11,  /* a = 1 active, 0 inactive */
  EV_WHEEL = 12,      /* a = x, b = y, c = delta y (wheel clicks), d = modifiers */
  EV_APP_QUIT = 13,   /* application quit requested (e.g. from Deskbar) */
  EV_MENU = 14,       /* a = menu item id, b = modifiers */
  EV_CHOICE = 15,     /* window id = choice id, a = selected index */
  EV_FILE_PANEL = 16  /* window id = dialog id, a = 1 if a file was chosen */
};

/* Message codes used by menus and file panels. */
enum {
  MSG_MENU = 'jmnu',
  MSG_CHOICE = 'jcho',
  MSG_FILE = 'jfil'
};

#define EV_INTS 8

struct PeerEvent
{
  int32 v[EV_INTS]; /* type, window id, a, b, c, d, e, f */
};

static pthread_mutex_t gQueueLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gQueueCond = PTHREAD_COND_INITIALIZER;
static std::deque<PeerEvent> gQueue;

static void
postEvent(int32 type, int32 win, int32 a = 0, int32 b = 0, int32 c = 0,
          int32 d = 0, int32 e = 0, int32 f = 0)
{
  PeerEvent ev;
  ev.v[0] = type; ev.v[1] = win; ev.v[2] = a; ev.v[3] = b;
  ev.v[4] = c; ev.v[5] = d; ev.v[6] = e; ev.v[7] = f;
  pthread_mutex_lock(&gQueueLock);
  /* Coalesce consecutive motion and resize events for the same window. */
  if (!gQueue.empty())
    {
      PeerEvent &last = gQueue.back();
      if (last.v[0] == type && last.v[1] == win
          && (type == EV_MOUSE_MOVED || type == EV_RESIZED
              || type == EV_MOVED))
        {
          last = ev;
          pthread_mutex_unlock(&gQueueLock);
          return;
        }
    }
  gQueue.push_back(ev);
  pthread_cond_signal(&gQueueCond);
  pthread_mutex_unlock(&gQueueLock);
}

/* ------------------------------------------------------------------ */

/* File panel results, keyed by dialog id.  Guarded by gQueueLock. */
static std::map<int32, std::string> gFileResults;

static void
setFileResult(int32 id, const char *path)
{
  pthread_mutex_lock(&gQueueLock);
  gFileResults[id] = path;
  pthread_mutex_unlock(&gQueueLock);
}

class PeerApp : public BApplication
{
public:
  PeerApp(sem_id ready)
    : BApplication("application/x-vnd.gnu-classpath-awt"), fReady(ready)
  {
  }

  virtual void ReadyToRun()
  {
    release_sem(fReady);
  }

  virtual bool QuitRequested()
  {
    /* Let Java decide (it posts WINDOW_CLOSING to every window). */
    postEvent(EV_APP_QUIT, 0);
    return false;
  }

  virtual void MessageReceived(BMessage *msg)
  {
    int32 id = 0, index = 0;
    switch (msg->what)
      {
      case MSG_MENU:
        if (msg->FindInt32("jid", &id) == B_OK)
          postEvent(EV_MENU, 0, id, modifiers());
        return;
      case MSG_CHOICE:
        if (msg->FindInt32("jid", &id) == B_OK
            && msg->FindInt32("jindex", &index) == B_OK)
          postEvent(EV_CHOICE, id, index);
        return;
      case MSG_FILE:
        {
          /* Open panel: "refs"; save panel: "directory" + "name". */
          msg->FindInt32("jid", &id);
          entry_ref ref;
          const char *name = NULL;
          if (msg->FindRef("refs", &ref) == B_OK)
            {
              BPath path(&ref);
              if (path.InitCheck() == B_OK)
                setFileResult(id, path.Path());
            }
          else if (msg->FindRef("directory", &ref) == B_OK
                   && msg->FindString("name", &name) == B_OK)
            {
              BPath path(&ref);
              if (path.InitCheck() == B_OK && path.Append(name) == B_OK)
                setFileResult(id, path.Path());
            }
          return;
        }
      case B_CANCEL:
        {
          /* Sent when a file panel closes, after a selection or not. */
          int32 oldWhat = 0;
          if (msg->FindInt32("old_what", &oldWhat) == B_OK
              && oldWhat == MSG_FILE)
            {
              void *source = NULL;
              msg->FindPointer("source", &source);
              if (msg->FindInt32("jid", &id) != B_OK)
                id = -1;
              PanelClosed((BFilePanel *) source, id);
            }
          return;
        }
      }
    BApplication::MessageReceived(msg);
  }

  void PanelClosed(BFilePanel *panel, int32 id);

private:
  sem_id fReady;
};

/* Open file panels and the dialog id each belongs to. */
static pthread_mutex_t gPanelLock = PTHREAD_MUTEX_INITIALIZER;
static std::map<BFilePanel *, int32> gPanels;

void
PeerApp::PanelClosed(BFilePanel *panel, int32 id)
{
  bool known = false;
  pthread_mutex_lock(&gPanelLock);
  std::map<BFilePanel *, int32>::iterator it = gPanels.find(panel);
  if (it != gPanels.end())
    {
      if (id < 0)
        id = it->second;
      gPanels.erase(it);
      known = true;
    }
  pthread_mutex_unlock(&gPanelLock);
  if (id < 0)
    return;
  bool chosen;
  pthread_mutex_lock(&gQueueLock);
  chosen = gFileResults.find(id) != gFileResults.end();
  pthread_mutex_unlock(&gQueueLock);
  postEvent(EV_FILE_PANEL, id, chosen ? 1 : 0);
  if (known)
    delete panel;
}

static void
currentMouseState(BView *view, int32 *buttons, int32 *clicks)
{
  *buttons = 0;
  *clicks = 1;
  BMessage *msg = view->Window()->CurrentMessage();
  if (msg != NULL)
    {
      msg->FindInt32("buttons", buttons);
      msg->FindInt32("clicks", clicks);
    }
}

class PeerView : public BView
{
public:
  PeerView(BRect frame, int32 id)
    : BView(frame, "awt", B_FOLLOW_ALL_SIDES, B_WILL_DRAW | B_FRAME_EVENTS),
      fId(id), fBitmap(NULL), fButtons(0)
  {
    /* We repaint every pixel from the bitmap; no app_server erase. */
    SetViewColor(B_TRANSPARENT_COLOR);
    SetLowColor(220, 220, 220);
  }

  virtual ~PeerView()
  {
    delete fBitmap;
  }

  /* Called with the window locked. */
  void EnsureBitmap(int32 w, int32 h)
  {
    if (fBitmap != NULL)
      {
        BRect b = fBitmap->Bounds();
        if (b.IntegerWidth() + 1 == w && b.IntegerHeight() + 1 == h)
          return;
        delete fBitmap;
        fBitmap = NULL;
      }
    if (w > 0 && h > 0)
      fBitmap = new BBitmap(BRect(0, 0, w - 1, h - 1), B_RGB32);
  }

  BBitmap *Bitmap() { return fBitmap; }

  virtual void Draw(BRect update)
  {
    if (fBitmap != NULL)
      {
        BRect b = fBitmap->Bounds();
        BRect r = update & b;
        if (r.IsValid())
          DrawBitmap(fBitmap, r, r);
        /* Fill area not covered by the bitmap (window grew before Java
           repainted). */
        if (update.right > b.right)
          FillRect(BRect(b.right + 1, update.top, update.right,
                         update.bottom), B_SOLID_LOW);
        if (update.bottom > b.bottom)
          FillRect(BRect(update.left, b.bottom + 1,
                         update.right < b.right ? update.right : b.right,
                         update.bottom), B_SOLID_LOW);
      }
    else
      FillRect(update, B_SOLID_LOW);
  }

  virtual void MouseDown(BPoint where)
  {
    int32 buttons, clicks;
    currentMouseState(this, &buttons, &clicks);
    fButtons = buttons;
    MakeFocus(true);
    SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
    postEvent(EV_MOUSE_DOWN, fId, (int32) where.x, (int32) where.y,
              buttons, modifiers(), clicks);
  }

  virtual void MouseUp(BPoint where)
  {
    int32 buttons, clicks;
    currentMouseState(this, &buttons, &clicks);
    /* Report the buttons that were released. */
    int32 released = fButtons & ~buttons;
    fButtons = buttons;
    postEvent(EV_MOUSE_UP, fId, (int32) where.x, (int32) where.y,
              released, modifiers());
  }

  virtual void MouseMoved(BPoint where, uint32 transit, const BMessage *)
  {
    int32 buttons, clicks;
    currentMouseState(this, &buttons, &clicks);
    int32 type = EV_MOUSE_MOVED;
    if (transit == B_ENTERED_VIEW)
      type = EV_MOUSE_ENTER;
    else if (transit == B_EXITED_VIEW)
      type = EV_MOUSE_EXIT;
    postEvent(type, fId, (int32) where.x, (int32) where.y, buttons,
              modifiers());
    if (type != EV_MOUSE_MOVED)
      postEvent(EV_MOUSE_MOVED, fId, (int32) where.x, (int32) where.y,
                buttons, modifiers());
  }

  virtual void KeyDown(const char *bytes, int32 numBytes)
  {
    PostKey(EV_KEY_DOWN, bytes, numBytes);
  }

  virtual void KeyUp(const char *bytes, int32 numBytes)
  {
    PostKey(EV_KEY_UP, bytes, numBytes);
  }

  virtual void MessageReceived(BMessage *msg)
  {
    if (msg->what == B_MOUSE_WHEEL_CHANGED)
      {
        float dy = 0;
        msg->FindFloat("be:wheel_delta_y", &dy);
        BPoint where;
        uint32 b;
        GetMouse(&where, &b, false);
        postEvent(EV_WHEEL, fId, (int32) where.x, (int32) where.y,
                  (int32) dy, modifiers());
        return;
      }
    BView::MessageReceived(msg);
  }

private:
  void PostKey(int32 type, const char *bytes, int32 numBytes)
  {
    BMessage *msg = Window()->CurrentMessage();
    int32 key = 0, mods = 0, rawChar = 0, repeat = 0;
    if (msg != NULL)
      {
        msg->FindInt32("key", &key);
        msg->FindInt32("modifiers", &mods);
        msg->FindInt32("raw_char", &rawChar);
        msg->FindInt32("be:key_repeat", &repeat);
      }
    postEvent(type, fId, rawChar, decodeUTF8(bytes, numBytes), mods, key,
              repeat);
  }

  static int32 decodeUTF8(const char *s, int32 n)
  {
    const unsigned char *u = (const unsigned char *) s;
    if (n <= 0)
      return 0;
    if (u[0] < 0x80)
      return u[0];
    if ((u[0] & 0xE0) == 0xC0 && n >= 2)
      return ((u[0] & 0x1F) << 6) | (u[1] & 0x3F);
    if ((u[0] & 0xF0) == 0xE0 && n >= 3)
      return ((u[0] & 0x0F) << 12) | ((u[1] & 0x3F) << 6) | (u[2] & 0x3F);
    if ((u[0] & 0xF8) == 0xF0 && n >= 4)
      return ((u[0] & 0x07) << 18) | ((u[1] & 0x3F) << 12)
             | ((u[2] & 0x3F) << 6) | (u[3] & 0x3F);
    return 0;
  }

  int32 fId;
  BBitmap *fBitmap;
  int32 fButtons;
};

class PeerWindow : public BWindow
{
public:
  PeerWindow(BRect frame, const char *title, window_look look,
             window_feel feel, uint32 flags, int32 id)
    : BWindow(frame, title, look, feel, flags), fShown(false), fId(id),
      fMenuBar(NULL), fMenuHeight(0)
  {
    fView = new PeerView(Bounds(), id);
    AddChild(fView);
    fView->MakeFocus(true);
  }

  virtual bool QuitRequested()
  {
    postEvent(EV_CLOSE, fId);
    return false;
  }

  /* Events and bounds use the content area: the window frame minus the
     menu bar at its top. */
  virtual void FrameResized(float w, float h)
  {
    postEvent(EV_RESIZED, fId, (int32) w + 1,
              (int32) h + 1 - fMenuHeight);
  }

  virtual void FrameMoved(BPoint p)
  {
    postEvent(EV_MOVED, fId, (int32) p.x, (int32) p.y + fMenuHeight);
  }

  /* Called with the window locked.  The window frame stays the same; the
     content view shrinks or grows. */
  void SetJMenuBar(BMenuBar *bar)
  {
    if (fMenuBar == bar)
      return;
    if (fMenuBar != NULL)
      RemoveChild(fMenuBar);
    fMenuBar = bar;
    fMenuHeight = 0;
    if (bar != NULL)
      {
        AddChild(bar);
        float pw, ph;
        bar->GetPreferredSize(&pw, &ph);
        bar->MoveTo(0, 0);
        bar->ResizeTo(Bounds().Width(), ph);
        fMenuHeight = (int32) ph + 1;
      }
    BRect b = Bounds();
    fView->MoveTo(0, fMenuHeight);
    fView->ResizeTo(b.Width(), b.Height() - fMenuHeight);
  }

  int32 MenuHeight() { return fMenuHeight; }
  BMenuBar *JMenuBar() { return fMenuBar; }

  virtual void WindowActivated(bool active)
  {
    postEvent(EV_ACTIVATED, fId, active ? 1 : 0);
  }

  PeerView *View() { return fView; }
  bool fShown;

private:
  int32 fId;
  PeerView *fView;
  BMenuBar *fMenuBar;
  int32 fMenuHeight;
};

static void releaseMenuBar(BMenuBar *bar);

/* ------------------------------------------------------------------ */

static sem_id gAppReady = -1;
static thread_id gAppThread = -1;

static int32
appThread(void *)
{
  PeerApp *app = new PeerApp(gAppReady);
  app->Run();
  delete app;
  return 0;
}

static PeerWindow *
win(jlong handle)
{
  return (PeerWindow *) (intptr_t) handle;
}

static char *
utf8(JNIEnv *env, jstring s)
{
  if (s == NULL)
    return strdup("");
  const char *c = env->GetStringUTFChars(s, NULL);
  char *r = strdup(c != NULL ? c : "");
  if (c != NULL)
    env->ReleaseStringUTFChars(s, c);
  return r;
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_init(JNIEnv *, jclass)
{
  if (be_app != NULL)
    return JNI_TRUE;
  gAppReady = create_sem(0, "awt app ready");
  gAppThread = spawn_thread(appThread, "AWT BApplication",
                            B_DISPLAY_PRIORITY, NULL);
  if (gAppThread < 0)
    return JNI_FALSE;
  resume_thread(gAppThread);
  status_t st;
  do
    st = acquire_sem_etc(gAppReady, 1, B_RELATIVE_TIMEOUT, 10000000);
  while (st == B_INTERRUPTED);
  return st == B_OK ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_screenInfo(JNIEnv *env, jclass,
                                                    jintArray out)
{
  BScreen screen(B_MAIN_SCREEN_ID);
  BRect f = screen.Frame();
  jint v[3];
  v[0] = f.IntegerWidth() + 1;
  v[1] = f.IntegerHeight() + 1;
  v[2] = 72;
  monitor_info info;
  if (screen.GetMonitorInfo(&info) == B_OK && info.width > 0)
    v[2] = (jint) ((f.Width() + 1) / (info.width / 2.54));
  if (v[2] < 72)
    v[2] = 72;
  env->SetIntArrayRegion(out, 0, 3, v);
}

/* kind: 0 = frame, 1 = dialog, 2 = undecorated window */
JNIEXPORT jlong JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_createWindow(JNIEnv *env, jclass,
                                                      jint id, jint kind,
                                                      jstring title,
                                                      jint x, jint y,
                                                      jint w, jint h)
{
  char *t = utf8(env, title);
  window_look look = B_TITLED_WINDOW_LOOK;
  window_feel feel = B_NORMAL_WINDOW_FEEL;
  uint32 flags = B_ASYNCHRONOUS_CONTROLS;
  if (kind == 1)
    look = B_TITLED_WINDOW_LOOK;
  else if (kind == 2)
    {
      look = B_NO_BORDER_WINDOW_LOOK;
      flags |= B_AVOID_FOCUS;
    }
  if (w < 1) w = 1;
  if (h < 1) h = 1;
  PeerWindow *pw = new PeerWindow(BRect(x, y, x + w - 1, y + h - 1), t,
                                  look, feel, flags, id);
  free(t);
  return (jlong) (intptr_t) pw;
}

/* out = left, top, right, bottom decorator insets. */
JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_getInsets(JNIEnv *env, jclass,
                                                   jlong h, jintArray out)
{
  jint v[4] = { 0, 0, 0, 0 };
  PeerWindow *pw = win(h);
  if (pw->Lock())
    {
      BRect f = pw->Frame();
      BRect d = pw->DecoratorFrame();
      if (d.IsValid() && d != f)
        {
          v[0] = (jint) (f.left - d.left);
          v[1] = (jint) (f.top - d.top);
          v[2] = (jint) (d.right - f.right);
          v[3] = (jint) (d.bottom - f.bottom);
        }
      v[1] += pw->MenuHeight();
      pw->Unlock();
    }
  env->SetIntArrayRegion(out, 0, 4, v);
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_setTitle(JNIEnv *env, jclass,
                                                  jlong h, jstring title)
{
  char *t = utf8(env, title);
  PeerWindow *pw = win(h);
  if (pw->Lock())
    {
      pw->SetTitle(t);
      pw->Unlock();
    }
  free(t);
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_setVisible(JNIEnv *, jclass,
                                                    jlong h, jboolean v)
{
  PeerWindow *pw = win(h);
  if (pw->Lock())
    {
      if (v && !pw->fShown)
        {
          pw->Show();
          pw->fShown = true;
        }
      else if (!v && pw->fShown)
        {
          pw->Hide();
          pw->fShown = false;
        }
      pw->Unlock();
    }
}

/* x, y, w, h describe the client area in screen coordinates. */
JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_setBounds(JNIEnv *, jclass,
                                                   jlong h, jint x, jint y,
                                                   jint w, jint hh)
{
  PeerWindow *pw = win(h);
  if (w < 1) w = 1;
  if (hh < 1) hh = 1;
  if (pw->Lock())
    {
      y -= pw->MenuHeight();
      hh += pw->MenuHeight();
      BRect f = pw->Frame();
      if ((jint) f.left != x || (jint) f.top != y)
        pw->MoveTo(x, y);
      if (f.IntegerWidth() + 1 != w || f.IntegerHeight() + 1 != hh)
        pw->ResizeTo(w - 1, hh - 1);
      pw->Unlock();
    }
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_setResizable(JNIEnv *, jclass,
                                                      jlong h, jboolean r)
{
  PeerWindow *pw = win(h);
  if (pw->Lock())
    {
      uint32 flags = pw->Flags();
      if (r)
        flags &= ~(B_NOT_RESIZABLE | B_NOT_ZOOMABLE);
      else
        flags |= B_NOT_RESIZABLE | B_NOT_ZOOMABLE;
      pw->SetFlags(flags);
      pw->Unlock();
    }
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_toFront(JNIEnv *, jclass, jlong h)
{
  PeerWindow *pw = win(h);
  if (pw->Lock())
    {
      pw->Activate(true);
      pw->Unlock();
    }
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_toBack(JNIEnv *, jclass, jlong h)
{
  PeerWindow *pw = win(h);
  if (pw->Lock())
    {
      pw->SendBehind(NULL);
      pw->Unlock();
    }
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_dispose(JNIEnv *, jclass, jlong h)
{
  PeerWindow *pw = win(h);
  if (pw->Lock())
    {
      /* Menus belong to their Java peers; keep them out of the window's
         destructor. */
      BMenuBar *bar = pw->JMenuBar();
      pw->SetJMenuBar(NULL);
      pw->Quit();
      if (bar != NULL)
        releaseMenuBar(bar);
    }
}

/* Copy the rectangle (x, y, w, h) of the Java image pixels (imgW x imgH,
   0x00RRGGBB, scanline imgW) into the window bitmap and redraw it. */
JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_blit(JNIEnv *env, jclass, jlong h,
                                              jintArray pixels, jint imgW,
                                              jint imgH, jint x, jint y,
                                              jint w, jint hh)
{
  PeerWindow *pw = win(h);
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { hh += y; y = 0; }
  if (x + w > imgW) w = imgW - x;
  if (y + hh > imgH) hh = imgH - y;
  if (w <= 0 || hh <= 0)
    return;
  if (!pw->Lock())
    return;
  PeerView *view = pw->View();
  BBitmap *old = view->Bitmap();
  view->EnsureBitmap(imgW, imgH);
  BBitmap *bmp = view->Bitmap();
  if (bmp != old)
    {
      /* New bitmap: copy everything, not just the dirty area. */
      x = 0; y = 0; w = imgW; hh = imgH;
    }
  if (bmp != NULL)
    {
      jint *src = (jint *) env->GetPrimitiveArrayCritical(pixels, NULL);
      if (src != NULL)
        {
          uint8 *bits = (uint8 *) bmp->Bits();
          int32 bpr = bmp->BytesPerRow();
          for (jint row = 0; row < hh; row++)
            {
              uint32 *dst = (uint32 *) (bits + (y + row) * bpr) + x;
              const jint *s = src + (y + row) * imgW + x;
              /* Java 0x00RRGGBB == B_RGB32 (B, G, R, A in memory).  */
              for (jint col = 0; col < w; col++)
                dst[col] = (uint32) s[col] | 0xFF000000;
            }
          env->ReleasePrimitiveArrayCritical(pixels, src, JNI_ABORT);
        }
      view->Invalidate(BRect(x, y, x + w - 1, y + hh - 1));
    }
  pw->Unlock();
}

/* Wait up to timeoutMs for an event; returns the number of ints written to
   out (0 on timeout). */
JNIEXPORT jint JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_nextEvent(JNIEnv *env, jclass,
                                                   jintArray out,
                                                   jint timeoutMs)
{
  PeerEvent ev;
  bool have = false;
  pthread_mutex_lock(&gQueueLock);
  if (gQueue.empty() && timeoutMs > 0)
    {
      struct timeval now;
      gettimeofday(&now, NULL);
      struct timespec until;
      long long ns = (long long) now.tv_usec * 1000
                     + (long long) timeoutMs * 1000000;
      until.tv_sec = now.tv_sec + ns / 1000000000;
      until.tv_nsec = ns % 1000000000;
      while (gQueue.empty())
        {
          int rc = pthread_cond_timedwait(&gQueueCond, &gQueueLock, &until);
          if (rc == ETIMEDOUT)
            break;
        }
    }
  if (!gQueue.empty())
    {
      ev = gQueue.front();
      gQueue.pop_front();
      have = true;
    }
  pthread_mutex_unlock(&gQueueLock);
  if (!have)
    return 0;
  env->SetIntArrayRegion(out, 0, EV_INTS, (jint *) ev.v);
  return EV_INTS;
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_beep(JNIEnv *, jclass)
{
  beep();
}

} /* extern "C" */

/* ------------------------------------------------------------------ */
/* Menus.
 *
 * Every AWT menu component peer owns one native object: a BMenuBar, a
 * BMenu, a BPopUpMenu or a BMenuItem.  Selections are sent to be_app as
 * MSG_MENU with the Java item id and come back to Java as EV_MENU.
 *
 * Menu objects are owned by Java.  When a peer is disposed while its
 * object is still in a parent menu, it is only marked; it is deleted when
 * it is removed from the parent.  A submenu cannot be separated from the
 * item that holds it, so that small item is not deleted while the Java
 * submenu is alive.
 */

static pthread_mutex_t gMenuLock = PTHREAD_MUTEX_INITIALIZER;
static std::map<void *, bool> gDisposed; /* Java objects disposed while attached */

/* Locks the window (or open menu window) that shows a menu tree. */
class MenuLocker
{
public:
  MenuLocker(BMenu *m) : fRoot(NULL)
  {
    if (m == NULL)
      return;
    BMenu *root = m;
    while (root->Supermenu() != NULL)
      root = root->Supermenu();
    if (root->LockLooper())
      fRoot = root;
  }
  ~MenuLocker()
  {
    if (fRoot != NULL)
      fRoot->UnlockLooper();
  }
private:
  BMenu *fRoot;
};

static bool
takeDisposed(void *p)
{
  pthread_mutex_lock(&gMenuLock);
  std::map<void *, bool>::iterator it = gDisposed.find(p);
  bool was = it != gDisposed.end();
  if (was)
    gDisposed.erase(it);
  pthread_mutex_unlock(&gMenuLock);
  return was;
}

static void
markDisposed(void *p)
{
  pthread_mutex_lock(&gMenuLock);
  gDisposed[p] = true;
  pthread_mutex_unlock(&gMenuLock);
}

static void detachAll(BMenu *m);

/* An item was taken out of its menu: delete what Java no longer owns. */
static void
releaseItem(BMenuItem *item)
{
  BMenu *sub = item->Submenu();
  if (sub != NULL)
    {
      /* The holder item deletes its submenu, so only delete both when the
         Java submenu was disposed. */
      if (takeDisposed(sub))
        {
          detachAll(sub);
          delete item;
        }
      return;
    }
  /* Separators made by menuAddSeparator have no Java owner.  Those made
     for an AWT "-" item are JSeparatorItems, which carry a message. */
  if (dynamic_cast<BSeparatorItem *>(item) != NULL && item->Message() == NULL)
    {
      delete item;
      return;
    }
  if (takeDisposed(item))
    delete item;
}

/* Removes every item from m and releases it. */
static void
detachAll(BMenu *m)
{
  BMenuItem *item;
  while ((item = m->RemoveItem((int32) 0)) != NULL)
    releaseItem(item);
}

/* A menu bar was taken out of its window: delete it if Java disposed it. */
static void
releaseMenuBar(BMenuBar *bar)
{
  if (takeDisposed(bar))
    {
      detachAll(bar);
      delete bar;
    }
}

static BMenu *menuOf(jlong h) { return (BMenu *) (intptr_t) h; }
static BMenuItem *itemOf(jlong h) { return (BMenuItem *) (intptr_t) h; }

/* Java-made separators: a BSeparatorItem with a marker message. */
class JSeparatorItem : public BSeparatorItem
{
public:
  JSeparatorItem() { SetMessage(new BMessage('jsep')); }
};

extern "C" {

/* kind: 0 = menu bar, 1 = menu, 2 = popup menu */
JNIEXPORT jlong JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_menuCreate(JNIEnv *env, jclass,
                                                    jint kind, jstring label)
{
  char *l = utf8(env, label);
  BMenu *m;
  if (kind == 0)
    m = new BMenuBar(BRect(0, 0, 100, 20), "menubar");
  else if (kind == 2)
    m = new BPopUpMenu(l, false, false);
  else
    m = new BMenu(l);
  free(l);
  return (jlong) (intptr_t) m;
}

JNIEXPORT jlong JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_menuItemCreate(JNIEnv *env, jclass,
                                                        jint id, jstring label,
                                                        jint shortcut,
                                                        jint mods,
                                                        jboolean separator)
{
  if (separator)
    return (jlong) (intptr_t) new JSeparatorItem();
  char *l = utf8(env, label);
  BMessage *msg = new BMessage(MSG_MENU);
  msg->AddInt32("jid", id);
  BMenuItem *item = new BMenuItem(l, msg, (char) shortcut, (uint32) mods);
  item->SetTarget(be_app);
  free(l);
  return (jlong) (intptr_t) item;
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_menuAddItem(JNIEnv *, jclass,
                                                     jlong menu, jlong item,
                                                     jint index)
{
  BMenu *m = menuOf(menu);
  BMenuItem *i = itemOf(item);
  MenuLocker lock(m);
  if (i->Menu() != NULL)
    return;
  if (index < 0 || index > m->CountItems())
    m->AddItem(i);
  else
    m->AddItem(i, index);
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_menuAddSubmenu(JNIEnv *, jclass,
                                                        jlong menu, jlong sub,
                                                        jint index)
{
  BMenu *m = menuOf(menu);
  BMenu *s = menuOf(sub);
  MenuLocker lock(m);
  if (s->Superitem() != NULL)
    return;
  if (index < 0 || index > m->CountItems())
    m->AddItem(s);
  else
    m->AddItem(s, index);
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_menuAddSeparator(JNIEnv *, jclass,
                                                          jlong menu)
{
  BMenu *m = menuOf(menu);
  MenuLocker lock(m);
  m->AddSeparatorItem();
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_menuRemoveItem(JNIEnv *, jclass,
                                                        jlong menu,
                                                        jint index)
{
  BMenu *m = menuOf(menu);
  MenuLocker lock(m);
  BMenuItem *item = m->RemoveItem(index);
  if (item != NULL)
    releaseItem(item);
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_menuItemSetLabel(JNIEnv *env, jclass,
                                                          jlong item,
                                                          jstring label)
{
  BMenuItem *i = itemOf(item);
  char *l = utf8(env, label);
  MenuLocker lock(i->Menu());
  i->SetLabel(l);
  free(l);
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_menuItemSetEnabled(JNIEnv *, jclass,
                                                            jlong item,
                                                            jboolean e)
{
  BMenuItem *i = itemOf(item);
  MenuLocker lock(i->Menu());
  i->SetEnabled(e);
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_menuItemSetMarked(JNIEnv *, jclass,
                                                           jlong item,
                                                           jboolean mark)
{
  BMenuItem *i = itemOf(item);
  MenuLocker lock(i->Menu());
  i->SetMarked(mark);
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_menuSetLabel(JNIEnv *env, jclass,
                                                      jlong menu,
                                                      jstring label)
{
  BMenu *m = menuOf(menu);
  char *l = utf8(env, label);
  MenuLocker lock(m);
  if (m->Superitem() != NULL)
    m->Superitem()->SetLabel(l);
  else
    m->SetName(l);
  free(l);
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_menuSetEnabled(JNIEnv *, jclass,
                                                        jlong menu,
                                                        jboolean e)
{
  BMenu *m = menuOf(menu);
  MenuLocker lock(m);
  m->SetEnabled(e);
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_menuItemDispose(JNIEnv *, jclass,
                                                         jlong item)
{
  BMenuItem *i = itemOf(item);
  BMenu *parent = i->Menu();
  if (parent != NULL)
    {
      MenuLocker lock(parent);
      if (i->Menu() != NULL)
        {
          markDisposed(i);
          return;
        }
    }
  delete i;
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_menuDispose(JNIEnv *, jclass,
                                                     jlong menu)
{
  BMenu *m = menuOf(menu);
  {
    MenuLocker lock(m);
    if (m->Superitem() != NULL || m->Window() != NULL)
      {
        markDisposed(m);
        return;
      }
  }
  detachAll(m);
  delete m;
}

/* Attaches a menu bar to a window (0 detaches).  Returns its height. */
JNIEXPORT jint JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_setMenuBar(JNIEnv *, jclass,
                                                    jlong h, jlong bar)
{
  PeerWindow *pw = win(h);
  jint height = 0;
  BMenuBar *old = NULL;
  if (pw->Lock())
    {
      old = pw->JMenuBar();
      pw->SetJMenuBar((BMenuBar *) (intptr_t) bar);
      height = pw->MenuHeight();
      pw->Unlock();
    }
  if (old != NULL && old != (BMenuBar *) (intptr_t) bar)
    releaseMenuBar(old);
  return height;
}

/* Shows a popup menu at screen point (x, y) without blocking. */
JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_popupShow(JNIEnv *, jclass,
                                                   jlong menu, jint x, jint y)
{
  BPopUpMenu *pm = dynamic_cast<BPopUpMenu *>(menuOf(menu));
  if (pm == NULL || pm->Window() != NULL)
    return;
  pm->Go(BPoint(x, y), true, false, false, true);
}

/* Shows the items of a Choice in a temporary popup at screen point (x, y).
   The selection comes back as EV_CHOICE. */
JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_choicePopup(JNIEnv *env, jclass,
                                                     jint id,
                                                     jobjectArray items,
                                                     jint selected,
                                                     jint x, jint y)
{
  BPopUpMenu *pm = new BPopUpMenu("choice", false, false);
  jsize n = env->GetArrayLength(items);
  for (jsize i = 0; i < n; i++)
    {
      jstring js = (jstring) env->GetObjectArrayElement(items, i);
      char *l = utf8(env, js);
      if (js != NULL)
        env->DeleteLocalRef(js);
      BMessage *msg = new BMessage(MSG_CHOICE);
      msg->AddInt32("jid", id);
      msg->AddInt32("jindex", i);
      BMenuItem *item = new BMenuItem(l, msg);
      item->SetTarget(be_app);
      if (i == selected)
        item->SetMarked(true);
      pm->AddItem(item);
      free(l);
    }
  pm->Go(BPoint(x, y), true, false, true, true);
}

/* ------------------------------------------------------------------ */
/* File panels. */

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_filePanelShow(JNIEnv *env, jclass,
                                                       jint id,
                                                       jboolean save,
                                                       jstring title,
                                                       jstring dir,
                                                       jstring file)
{
  pthread_mutex_lock(&gQueueLock);
  gFileResults.erase(id);
  pthread_mutex_unlock(&gQueueLock);

  BMessage msg(MSG_FILE);
  msg.AddInt32("jid", id);
  BMessenger target(be_app);
  BFilePanel *panel = new BFilePanel(save ? B_SAVE_PANEL : B_OPEN_PANEL,
                                     &target, NULL, 0, false, &msg);
  char *d = utf8(env, dir);
  char *f = utf8(env, file);
  char *t = utf8(env, title);
  if (*d != '\0')
    panel->SetPanelDirectory(d);
  if (save && *f != '\0')
    panel->SetSaveText(f);
  if (*t != '\0' && panel->Window()->Lock())
    {
      panel->Window()->SetTitle(t);
      panel->Window()->Unlock();
    }
  free(d);
  free(f);
  free(t);
  pthread_mutex_lock(&gPanelLock);
  gPanels[panel] = id;
  pthread_mutex_unlock(&gPanelLock);
  panel->Show();
}

/* The path chosen in the last file panel of a dialog, or null. */
JNIEXPORT jstring JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_fileResult(JNIEnv *env, jclass,
                                                    jint id)
{
  std::string path;
  bool have = false;
  pthread_mutex_lock(&gQueueLock);
  std::map<int32, std::string>::iterator it = gFileResults.find(id);
  if (it != gFileResults.end())
    {
      path = it->second;
      have = true;
      gFileResults.erase(it);
    }
  pthread_mutex_unlock(&gQueueLock);
  return have ? env->NewStringUTF(path.c_str()) : NULL;
}

} /* extern "C" */
