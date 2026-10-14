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

#include "gnu_java_awt_peer_haiku_HaikuNative.h"
#include "gnu_javax_sound_sampled_haiku_HaikuAudio.h"

#include <Application.h>
#include <Beep.h>
#include <Bitmap.h>
#include <Clipboard.h>
#include <Cursor.h>
#include <Entry.h>
#include <FilePanel.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Message.h>
#include <Messenger.h>
#include <Node.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <Screen.h>
#include <SoundPlayer.h>
#include <View.h>
#include <Window.h>
#include <OS.h>

#include <pthread.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

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
  EV_FILE_PANEL = 16, /* window id = dialog id, a = 1 if a file was chosen */
  EV_STATE = 17,      /* a = 1 if minimized, b = 1 if zoomed */
  EV_EDIT = 18,       /* a = EDIT_CUT, EDIT_COPY, EDIT_PASTE or EDIT_SELECT_ALL */
  EV_FILTER = 19      /* window id = dialog id; a file panel waits for filterAnswer() */
};

/* Commands of EV_EDIT: the Command-X/C/V/A shortcuts of a BWindow. */
enum {
  EDIT_CUT = 1,
  EDIT_COPY = 2,
  EDIT_PASTE = 3,
  EDIT_SELECT_ALL = 4
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

/* An absolute time timeoutMs milliseconds from now, for
   pthread_cond_timedwait. */
static void
deadlineIn(struct timespec *until, int32 timeoutMs)
{
  struct timeval now;
  gettimeofday(&now, NULL);
  long long ns = (long long) now.tv_usec * 1000
                 + (long long) timeoutMs * 1000000;
  until->tv_sec = now.tv_sec + ns / 1000000000;
  until->tv_nsec = ns % 1000000000;
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

/* ------------------------------------------------------------------ */
/* FilenameFilter support.
 *
 * A BRefFilter runs in the file panel's thread and cannot call Java.  For
 * each directory it asks Java once: it queues the directory, posts
 * EV_FILTER and waits.  Java runs the FilenameFilter on every name in the
 * directory and sends the answers with filterAnswer().
 */

typedef std::map<std::string, bool> FilterNames;
typedef std::map<std::string, FilterNames> FilterDirs;

static pthread_mutex_t gFilterLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gFilterCond = PTHREAD_COND_INITIALIZER;
static std::map<int32, FilterDirs> gFilterCache;
static std::map<int32, std::deque<std::string> > gFilterRequests;
static std::map<int32, std::set<std::string> > gFilterPending;

/* Forgets the answers of a dialog and releases a waiting panel. */
static void
filterForget(int32 id)
{
  pthread_mutex_lock(&gFilterLock);
  gFilterCache.erase(id);
  gFilterRequests.erase(id);
  gFilterPending.erase(id);
  pthread_cond_broadcast(&gFilterCond);
  pthread_mutex_unlock(&gFilterLock);
}

static bool
filterLookup(int32 id, const std::string &dir, const std::string &name)
{
  struct timespec until;
  deadlineIn(&until, 3000);
  bool requested = false;
  pthread_mutex_lock(&gFilterLock);
  while (true)
    {
      FilterDirs &dirs = gFilterCache[id];
      FilterDirs::iterator d = dirs.find(dir);
      if (d != dirs.end())
        {
          FilterNames::iterator n = d->second.find(name);
          if (n != d->second.end())
            {
              bool accept = n->second;
              pthread_mutex_unlock(&gFilterLock);
              return accept;
            }
          if (requested)
            break;
          /* The file is newer than the answer: ask again. */
          dirs.erase(d);
        }
      if (!requested)
        {
          if (gFilterPending[id].insert(dir).second)
            {
              gFilterRequests[id].push_back(dir);
              postEvent(EV_FILTER, id);
            }
          requested = true;
        }
      if (pthread_cond_timedwait(&gFilterCond, &gFilterLock, &until)
          == ETIMEDOUT)
        break;
    }
  pthread_mutex_unlock(&gFilterLock);
  return true;
}

class JRefFilter : public BRefFilter
{
public:
  JRefFilter(int32 id) : fId(id) {}

  virtual bool Filter(const entry_ref *ref, BNode *, struct stat_beos *,
                      const char *mimeType)
  {
    /* Always show folders, volumes and links, so the user can move
       around. */
    BEntry entry(ref);
    if (entry.IsDirectory() || entry.IsSymLink())
      return true;
    if (mimeType != NULL
        && (strcmp(mimeType, "application/x-vnd.Be-directory") == 0
            || strcmp(mimeType, "application/x-vnd.Be-volume") == 0
            || strcmp(mimeType, "application/x-vnd.Be-symlink") == 0))
      return true;
    BPath path(ref);
    BPath parent;
    if (path.InitCheck() != B_OK || path.GetParent(&parent) != B_OK)
      return true;
    return filterLookup(fId, parent.Path(), path.Leaf());
  }

private:
  int32 fId;
};

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
  filterForget(id);
  postEvent(EV_FILE_PANEL, id, chosen ? 1 : 0);
  if (known)
    {
      BRefFilter *filter = panel->RefFilter();
      delete panel;
      delete filter;
    }
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
    switch (msg->what)
      {
      case B_CUT:
        postEvent(EV_EDIT, fId, EDIT_CUT);
        return;
      case B_COPY:
        postEvent(EV_EDIT, fId, EDIT_COPY);
        return;
      case B_PASTE:
        postEvent(EV_EDIT, fId, EDIT_PASTE);
        return;
      case B_SELECT_ALL:
        postEvent(EV_EDIT, fId, EDIT_SELECT_ALL);
        return;
      }
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
      fMenuBar(NULL), fMenuHeight(0), fZoomed(false)
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
    /* A zoomed window that the user resizes is no longer zoomed. */
    if (fZoomed && (fabsf(w - fZoomedFrame.Width()) > 1
                    || fabsf(h - fZoomedFrame.Height()) > 1))
      {
        fZoomed = false;
        PostState();
      }
  }

  virtual void Minimize(bool minimize)
  {
    BWindow::Minimize(minimize);
    PostState();
  }

  /* The zoom button and Zoom() toggle between the zoomed frame and the
     frame the window had before. */
  virtual void Zoom(BPoint origin, float w, float h)
  {
    if (fZoomed)
      {
        fZoomed = false;
        BWindow::Zoom(fNormalFrame.LeftTop(), fNormalFrame.Width(),
                      fNormalFrame.Height());
      }
    else
      {
        fNormalFrame = Frame();
        fZoomedFrame = BRect(origin.x, origin.y, origin.x + w,
                             origin.y + h);
        fZoomed = true;
        BWindow::Zoom(origin, w, h);
      }
    PostState();
  }

  using BWindow::Zoom;

  bool IsZoomed() { return fZoomed; }

  void PostState()
  {
    postEvent(EV_STATE, fId, IsMinimized() ? 1 : 0, fZoomed ? 1 : 0);
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
  bool fZoomed;
  BRect fNormalFrame;
  BRect fZoomedFrame;
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

/* Starts the BApplication thread once.  AWT and sound share it. */
static pthread_mutex_t gInitLock = PTHREAD_MUTEX_INITIALIZER;

static bool
startApp()
{
  pthread_mutex_lock(&gInitLock);
  bool ok = true;
  if (be_app == NULL)
    {
      gAppReady = create_sem(0, "awt app ready");
      gAppThread = spawn_thread(appThread, "AWT BApplication",
                                B_DISPLAY_PRIORITY, NULL);
      if (gAppThread < 0)
        ok = false;
      else
        {
          resume_thread(gAppThread);
          status_t st;
          do
            st = acquire_sem_etc(gAppReady, 1, B_RELATIVE_TIMEOUT, 10000000);
          while (st == B_INTERRUPTED);
          ok = st == B_OK;
        }
    }
  pthread_mutex_unlock(&gInitLock);
  return ok;
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_init(JNIEnv *, jclass)
{
  return startApp() ? JNI_TRUE : JNI_FALSE;
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
      struct timespec until;
      deadlineIn(&until, timeoutMs);
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
                                                       jstring file,
                                                       jboolean filtered)
{
  pthread_mutex_lock(&gQueueLock);
  gFileResults.erase(id);
  pthread_mutex_unlock(&gQueueLock);
  filterForget(id);

  BMessage msg(MSG_FILE);
  msg.AddInt32("jid", id);
  BMessenger target(be_app);
  BFilePanel *panel = new BFilePanel(save ? B_SAVE_PANEL : B_OPEN_PANEL,
                                     &target, NULL, 0, false, &msg,
                                     filtered ? new JRefFilter(id) : NULL);
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

/* ------------------------------------------------------------------ */
/* Frame state, cursors, clipboard and file panel filters. */

/* UTF-8 to UTF-16, for text that is not in modified UTF-8. */
static jstring
newStringFromUTF8(JNIEnv *env, const char *s, size_t n)
{
  std::vector<jchar> out;
  out.reserve(n);
  const unsigned char *u = (const unsigned char *) s;
  size_t i = 0;
  while (i < n)
    {
      uint32 c = u[i];
      int extra = 0;
      if (c >= 0xF0 && c < 0xF8) { c &= 0x07; extra = 3; }
      else if (c >= 0xE0) { c &= 0x0F; extra = 2; }
      else if (c >= 0xC0) { c &= 0x1F; extra = 1; }
      else if (c >= 0x80) c = 0xFFFD;
      i++;
      for (int k = 0; k < extra; k++, i++)
        {
          if (i >= n || (u[i] & 0xC0) != 0x80)
            {
              c = 0xFFFD;
              break;
            }
          c = (c << 6) | (u[i] & 0x3F);
        }
      if (c >= 0x10000)
        {
          c -= 0x10000;
          out.push_back((jchar) (0xD800 + (c >> 10)));
          out.push_back((jchar) (0xDC00 + (c & 0x3FF)));
        }
      else if (c != 0)
        out.push_back((jchar) c);
    }
  return env->NewString(out.empty() ? NULL : &out[0], (jsize) out.size());
}

/* UTF-16 to UTF-8, with surrogate pairs as one 4-byte sequence. */
static std::string
utf8FromString(JNIEnv *env, jstring js)
{
  std::string out;
  if (js == NULL)
    return out;
  jsize n = env->GetStringLength(js);
  const jchar *c = env->GetStringChars(js, NULL);
  if (c == NULL)
    return out;
  for (jsize i = 0; i < n; i++)
    {
      uint32 v = c[i];
      if (v >= 0xD800 && v < 0xDC00 && i + 1 < n
          && c[i + 1] >= 0xDC00 && c[i + 1] < 0xE000)
        {
          v = 0x10000 + ((v - 0xD800) << 10) + (c[i + 1] - 0xDC00);
          i++;
        }
      if (v < 0x80)
        out += (char) v;
      else if (v < 0x800)
        {
          out += (char) (0xC0 | (v >> 6));
          out += (char) (0x80 | (v & 0x3F));
        }
      else if (v < 0x10000)
        {
          out += (char) (0xE0 | (v >> 12));
          out += (char) (0x80 | ((v >> 6) & 0x3F));
          out += (char) (0x80 | (v & 0x3F));
        }
      else
        {
          out += (char) (0xF0 | (v >> 18));
          out += (char) (0x80 | ((v >> 12) & 0x3F));
          out += (char) (0x80 | ((v >> 6) & 0x3F));
          out += (char) (0x80 | (v & 0x3F));
        }
    }
  env->ReleaseStringChars(js, c);
  return out;
}

/* Java cursor types (java.awt.Cursor) to Haiku system cursors. */
static BCursorID
cursorIdFor(jint type)
{
  switch (type)
    {
    case 1: return B_CURSOR_ID_CROSS_HAIR;
    case 2: return B_CURSOR_ID_I_BEAM;
    case 3: return B_CURSOR_ID_PROGRESS;
    case 4: return B_CURSOR_ID_RESIZE_SOUTH_WEST;
    case 5: return B_CURSOR_ID_RESIZE_SOUTH_EAST;
    case 6: return B_CURSOR_ID_RESIZE_NORTH_WEST;
    case 7: return B_CURSOR_ID_RESIZE_NORTH_EAST;
    case 8: return B_CURSOR_ID_RESIZE_NORTH;
    case 9: return B_CURSOR_ID_RESIZE_SOUTH;
    case 10: return B_CURSOR_ID_RESIZE_WEST;
    case 11: return B_CURSOR_ID_RESIZE_EAST;
    case 12: return B_CURSOR_ID_FOLLOW_LINK;
    case 13: return B_CURSOR_ID_MOVE;
    default: return B_CURSOR_ID_SYSTEM_DEFAULT;
    }
}

static pthread_mutex_t gCursorLock = PTHREAD_MUTEX_INITIALIZER;
static std::map<int32, BCursor *> gSystemCursors;

static const BCursor *
systemCursor(jint type)
{
  BCursorID cid = cursorIdFor(type);
  pthread_mutex_lock(&gCursorLock);
  BCursor *&c = gSystemCursors[(int32) cid];
  if (c == NULL)
    c = new BCursor(cid);
  BCursor *r = c;
  pthread_mutex_unlock(&gCursorLock);
  return r;
}

extern "C" {

/* Minimizes, restores or zooms a window. */
JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_setFrameState(JNIEnv *, jclass,
                                                       jlong h,
                                                       jboolean minimized,
                                                       jboolean zoomed)
{
  PeerWindow *pw = win(h);
  if (pw->Lock())
    {
      if ((bool) zoomed != pw->IsZoomed())
        pw->Zoom();
      if ((bool) minimized != pw->IsMinimized())
        pw->Minimize(minimized);
      pw->Unlock();
    }
}

/* out = minimized, zoomed. */
JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_getFrameState(JNIEnv *env, jclass,
                                                       jlong h,
                                                       jbooleanArray out)
{
  jboolean v[2] = { JNI_FALSE, JNI_FALSE };
  PeerWindow *pw = win(h);
  if (pw->Lock())
    {
      v[0] = pw->IsMinimized();
      v[1] = pw->IsZoomed();
      pw->Unlock();
    }
  env->SetBooleanArrayRegion(out, 0, 2, v);
}

/* The largest size of a zoomed window (outer size, as in AWT). */
JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_setZoomLimits(JNIEnv *, jclass,
                                                       jlong h, jint w,
                                                       jint hh)
{
  PeerWindow *pw = win(h);
  if (pw->Lock())
    {
      pw->SetZoomLimits(w > 0 ? w - 1 : 32768, hh > 0 ? hh - 1 : 32768);
      pw->Unlock();
    }
}

/* Sets the cursor of a window: a java.awt.Cursor type, or a cursor made
   with cursorCreate() when custom is not 0. */
JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_setCursor(JNIEnv *, jclass,
                                                   jlong h, jint type,
                                                   jlong custom)
{
  const BCursor *c = custom != 0 ? (const BCursor *) (intptr_t) custom
                                 : systemCursor(type);
  PeerWindow *pw = win(h);
  if (pw->Lock())
    {
      pw->View()->SetViewCursor(c, true);
      pw->Unlock();
    }
}

/* Makes a cursor from w x h ARGB pixels with the hot spot (hx, hy). */
JNIEXPORT jlong JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_cursorCreate(JNIEnv *env, jclass,
                                                      jintArray pixels,
                                                      jint w, jint h,
                                                      jint hx, jint hy)
{
  if (w < 1 || h < 1)
    return 0;
  BBitmap bmp(BRect(0, 0, w - 1, h - 1), B_RGBA32);
  if (bmp.InitCheck() != B_OK)
    return 0;
  jint *src = env->GetIntArrayElements(pixels, NULL);
  if (src == NULL)
    return 0;
  uint8 *bits = (uint8 *) bmp.Bits();
  int32 bpr = bmp.BytesPerRow();
  /* Java 0xAARRGGBB == B_RGBA32 (B, G, R, A in memory). */
  for (jint row = 0; row < h; row++)
    memcpy(bits + row * bpr, src + row * w, w * 4);
  env->ReleaseIntArrayElements(pixels, src, JNI_ABORT);
  return (jlong) (intptr_t) new BCursor(&bmp, BPoint(hx, hy));
}

/* Puts text on the system clipboard.  Returns the clipboard count. */
JNIEXPORT jlong JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_clipboardSetText(JNIEnv *env,
                                                          jclass,
                                                          jstring text)
{
  std::string t = utf8FromString(env, text);
  jlong count = -1;
  if (be_clipboard->Lock())
    {
      be_clipboard->Clear();
      BMessage *data = be_clipboard->Data();
      if (data != NULL)
        {
          data->AddData("text/plain", B_MIME_TYPE, t.data(), t.size());
          be_clipboard->Commit();
        }
      count = be_clipboard->SystemCount();
      be_clipboard->Unlock();
    }
  return count;
}

/* The text on the system clipboard, or null. */
JNIEXPORT jstring JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_clipboardGetText(JNIEnv *env,
                                                          jclass)
{
  jstring result = NULL;
  if (be_clipboard->Lock())
    {
      BMessage *data = be_clipboard->Data();
      const void *text = NULL;
      ssize_t len = 0;
      if (data != NULL
          && data->FindData("text/plain", B_MIME_TYPE, &text, &len) == B_OK
          && text != NULL)
        result = newStringFromUTF8(env, (const char *) text, len);
      be_clipboard->Unlock();
    }
  return result;
}

/* Changes every time any application writes to the clipboard. */
JNIEXPORT jlong JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_clipboardCount(JNIEnv *, jclass)
{
  return be_clipboard->SystemCount();
}

/* The next directory a file panel wants filtered, or null. */
JNIEXPORT jstring JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_filterNext(JNIEnv *env, jclass,
                                                    jint id)
{
  std::string dir;
  bool have = false;
  pthread_mutex_lock(&gFilterLock);
  std::deque<std::string> &q = gFilterRequests[id];
  if (!q.empty())
    {
      dir = q.front();
      q.pop_front();
      have = true;
    }
  pthread_mutex_unlock(&gFilterLock);
  return have ? newStringFromUTF8(env, dir.data(), dir.size()) : NULL;
}

/* The FilenameFilter answers for every name in a directory. */
JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_haiku_HaikuNative_filterAnswer(JNIEnv *env, jclass,
                                                      jint id, jstring dir,
                                                      jobjectArray names,
                                                      jbooleanArray accepted)
{
  std::string d = utf8FromString(env, dir);
  FilterNames answers;
  jsize n = env->GetArrayLength(names);
  jboolean *acc = env->GetBooleanArrayElements(accepted, NULL);
  for (jsize i = 0; i < n; i++)
    {
      jstring js = (jstring) env->GetObjectArrayElement(names, i);
      answers[utf8FromString(env, js)] = acc != NULL && acc[i];
      if (js != NULL)
        env->DeleteLocalRef(js);
    }
  if (acc != NULL)
    env->ReleaseBooleanArrayElements(accepted, acc, JNI_ABORT);
  pthread_mutex_lock(&gFilterLock);
  gFilterCache[id][d] = answers;
  gFilterPending[id].erase(d);
  pthread_cond_broadcast(&gFilterCond);
  pthread_mutex_unlock(&gFilterLock);
}

} /* extern "C" */

/* ------------------------------------------------------------------ */
/* Sound output for javax.sound.sampled (gnu.javax.sound.sampled.haiku).
 *
 * Each line owns one BSoundPlayer.  Java converts all audio to 16-bit
 * signed PCM.  A clip keeps all its frames here and loops in the player
 * callback; a stream line has a ring buffer that Java writes to.
 */

struct JSound
{
  BSoundPlayer *player;
  pthread_mutex_t lock;
  pthread_cond_t cond;
  int32 channels;
  bool started;
  bool playing;
  float gain;
  /* Clip mode. */
  bool clipMode;
  int16 *clip;
  int64 clipFrames;
  int64 pos;
  int64 loopStart;
  int64 loopEnd;   /* exclusive */
  int32 loops;     /* -1 = forever */
  /* Stream mode. */
  int16 *ring;
  int64 ringFrames;
  int64 head;
  int64 count;
  int64 played;
};

static void
soundFill(void *cookie, void *buffer, size_t size,
          const media_raw_audio_format &fmt)
{
  JSound *s = (JSound *) cookie;
  int32 outCh = fmt.channel_count > 0 ? fmt.channel_count : 1;
  int32 sampleSize = fmt.format & media_raw_audio_format::B_AUDIO_SIZE_MASK;
  if (sampleSize == 0)
    sampleSize = 2;
  size_t frames = size / (outCh * sampleSize);
  pthread_mutex_lock(&s->lock);
  float gain = s->gain;
  for (size_t i = 0; i < frames; i++)
    {
      int32 l = 0, r = 0;
      if (s->playing)
        {
          const int16 *f = NULL;
          if (s->clipMode)
            {
              if (s->loops != 0 && s->pos == s->loopEnd)
                {
                  s->pos = s->loopStart;
                  if (s->loops > 0)
                    s->loops--;
                }
              if (s->pos < s->clipFrames)
                f = s->clip + s->pos++ * s->channels;
              else
                s->playing = false;
            }
          else if (s->count > 0)
            {
              f = s->ring + s->head * s->channels;
              s->head = (s->head + 1) % s->ringFrames;
              s->count--;
            }
          if (f != NULL)
            {
              l = f[0];
              r = s->channels > 1 ? f[1] : l;
              s->played++;
            }
        }
      for (int32 c = 0; c < outCh; c++)
        {
          float v = (outCh == 1 ? (l + r) / 2 : (c == 0 ? l : c == 1 ? r : 0))
                    * gain;
          if (v > 32767) v = 32767;
          if (v < -32768) v = -32768;
          size_t k = i * outCh + c;
          switch (fmt.format)
            {
            case media_raw_audio_format::B_AUDIO_FLOAT:
              ((float *) buffer)[k] = v / 32768.0f;
              break;
            case media_raw_audio_format::B_AUDIO_DOUBLE:
              ((double *) buffer)[k] = v / 32768.0;
              break;
            case media_raw_audio_format::B_AUDIO_INT:
              ((int32 *) buffer)[k] = (int32) v << 16;
              break;
            case media_raw_audio_format::B_AUDIO_UCHAR:
              ((uint8 *) buffer)[k] = (uint8) (((int32) v >> 8) + 128);
              break;
            case media_raw_audio_format::B_AUDIO_CHAR:
              ((int8 *) buffer)[k] = (int8) ((int32) v >> 8);
              break;
            default:
              ((int16 *) buffer)[k] = (int16) v;
              break;
            }
        }
    }
  pthread_cond_broadcast(&s->cond);
  pthread_mutex_unlock(&s->lock);
}

static JSound *
snd(jlong h)
{
  return (JSound *) (intptr_t) h;
}

/* Starts the player the first time something plays.  Called with
   s->lock held. */
static void
soundEnsureStarted(JSound *s)
{
  if (s->started)
    return;
  s->started = true;
  pthread_mutex_unlock(&s->lock);
  s->player->Start();
  s->player->SetHasData(true);
  pthread_mutex_lock(&s->lock);
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_gnu_javax_sound_sampled_haiku_HaikuAudio_init(JNIEnv *, jclass)
{
  return startApp() ? JNI_TRUE : JNI_FALSE;
}

/* Opens an output line.  Returns 0 if the media server refused it. */
JNIEXPORT jlong JNICALL
Java_gnu_javax_sound_sampled_haiku_HaikuAudio_open(JNIEnv *, jclass,
                                                   jfloat rate,
                                                   jint channels)
{
  JSound *s = new JSound;
  memset(s, 0, sizeof(*s));
  pthread_mutex_init(&s->lock, NULL);
  pthread_cond_init(&s->cond, NULL);
  s->channels = channels < 1 ? 1 : channels > 2 ? 2 : channels;
  s->gain = 1.0f;
  media_raw_audio_format f = media_raw_audio_format::wildcard;
  f.frame_rate = rate;
  f.channel_count = s->channels;
  f.format = media_raw_audio_format::B_AUDIO_FLOAT;
  f.byte_order = B_MEDIA_HOST_ENDIAN;
  f.buffer_size = 1024 * s->channels * sizeof(float);
  s->player = new BSoundPlayer(&f, "Java sound", soundFill, NULL, s);
  if (s->player->InitCheck() != B_OK)
    {
      delete s->player;
      pthread_mutex_destroy(&s->lock);
      pthread_cond_destroy(&s->cond);
      delete s;
      return 0;
    }
  return (jlong) (intptr_t) s;
}

JNIEXPORT void JNICALL
Java_gnu_javax_sound_sampled_haiku_HaikuAudio_close(JNIEnv *, jclass,
                                                    jlong h)
{
  JSound *s = snd(h);
  if (s->started)
    s->player->Stop(true, true);
  delete s->player;
  pthread_mutex_destroy(&s->lock);
  pthread_cond_destroy(&s->cond);
  free(s->clip);
  free(s->ring);
  delete s;
}

/* Gives a clip line its frames (interleaved 16-bit samples). */
JNIEXPORT void JNICALL
Java_gnu_javax_sound_sampled_haiku_HaikuAudio_loadClip(JNIEnv *env, jclass,
                                                       jlong h,
                                                       jshortArray data)
{
  JSound *s = snd(h);
  jsize n = env->GetArrayLength(data);
  int16 *buf = (int16 *) malloc((n > 0 ? n : 1) * sizeof(int16));
  env->GetShortArrayRegion(data, 0, n, (jshort *) buf);
  pthread_mutex_lock(&s->lock);
  free(s->clip);
  s->clip = buf;
  s->clipFrames = n / s->channels;
  s->pos = 0;
  s->clipMode = true;
  s->playing = false;
  pthread_mutex_unlock(&s->lock);
}

/* Plays a clip from frame start.  At loopEnd (exclusive) it goes back to
   loopStart, loops times (-1 = forever), then plays to the end. */
JNIEXPORT void JNICALL
Java_gnu_javax_sound_sampled_haiku_HaikuAudio_playClip(JNIEnv *, jclass,
                                                       jlong h, jlong start,
                                                       jlong loopStart,
                                                       jlong loopEnd,
                                                       jint loops)
{
  JSound *s = snd(h);
  pthread_mutex_lock(&s->lock);
  s->clipMode = true;
  s->pos = start < 0 ? 0 : start > s->clipFrames ? s->clipFrames : start;
  s->loopStart = loopStart;
  s->loopEnd = loopEnd;
  s->loops = loopEnd > loopStart ? loops : 0;
  s->playing = s->pos < s->clipFrames || s->loops != 0;
  soundEnsureStarted(s);
  pthread_mutex_unlock(&s->lock);
}

/* Starts or continues a stream line with a ring buffer of the given
   size. */
JNIEXPORT void JNICALL
Java_gnu_javax_sound_sampled_haiku_HaikuAudio_startStream(JNIEnv *, jclass,
                                                          jlong h,
                                                          jint frames)
{
  JSound *s = snd(h);
  pthread_mutex_lock(&s->lock);
  if (s->ring == NULL)
    {
      s->ringFrames = frames < 256 ? 256 : frames;
      s->ring = (int16 *) malloc(s->ringFrames * s->channels
                                 * sizeof(int16));
      s->head = 0;
      s->count = 0;
    }
  s->clipMode = false;
  s->playing = true;
  soundEnsureStarted(s);
  pthread_mutex_unlock(&s->lock);
}

/* Pauses playback.  The position and buffered data stay. */
JNIEXPORT void JNICALL
Java_gnu_javax_sound_sampled_haiku_HaikuAudio_pause(JNIEnv *, jclass,
                                                    jlong h)
{
  JSound *s = snd(h);
  pthread_mutex_lock(&s->lock);
  s->playing = false;
  pthread_cond_broadcast(&s->cond);
  pthread_mutex_unlock(&s->lock);
}

JNIEXPORT jboolean JNICALL
Java_gnu_javax_sound_sampled_haiku_HaikuAudio_isPlaying(JNIEnv *, jclass,
                                                        jlong h)
{
  JSound *s = snd(h);
  pthread_mutex_lock(&s->lock);
  bool p = s->playing && (s->clipMode || s->count > 0);
  pthread_mutex_unlock(&s->lock);
  return p;
}

/* Clip lines: the frame position.  Stream lines: the frames played. */
JNIEXPORT jlong JNICALL
Java_gnu_javax_sound_sampled_haiku_HaikuAudio_position(JNIEnv *, jclass,
                                                       jlong h)
{
  JSound *s = snd(h);
  pthread_mutex_lock(&s->lock);
  jlong p = s->clipMode ? s->pos : s->played;
  pthread_mutex_unlock(&s->lock);
  return p;
}

JNIEXPORT void JNICALL
Java_gnu_javax_sound_sampled_haiku_HaikuAudio_setPosition(JNIEnv *, jclass,
                                                          jlong h,
                                                          jlong frame)
{
  JSound *s = snd(h);
  pthread_mutex_lock(&s->lock);
  if (frame < 0)
    frame = 0;
  if (frame > s->clipFrames)
    frame = s->clipFrames;
  s->pos = frame;
  pthread_mutex_unlock(&s->lock);
}

/* Waits until a clip stops or a stream line has played all its data.
   Returns true if that happened within timeoutMs. */
JNIEXPORT jboolean JNICALL
Java_gnu_javax_sound_sampled_haiku_HaikuAudio_waitIdle(JNIEnv *, jclass,
                                                       jlong h,
                                                       jint timeoutMs)
{
  JSound *s = snd(h);
  struct timespec until;
  deadlineIn(&until, timeoutMs);
  pthread_mutex_lock(&s->lock);
  bool idle;
  while (!(idle = !s->playing || (!s->clipMode && s->count == 0)))
    if (pthread_cond_timedwait(&s->cond, &s->lock, &until) == ETIMEDOUT)
      break;
  pthread_mutex_unlock(&s->lock);
  return idle;
}

/* Copies whole frames from data[off .. off + len) into the ring buffer,
   waiting up to timeoutMs for space.  Returns the samples taken. */
JNIEXPORT jint JNICALL
Java_gnu_javax_sound_sampled_haiku_HaikuAudio_write(JNIEnv *env, jclass,
                                                    jlong h,
                                                    jshortArray data,
                                                    jint off, jint len,
                                                    jint timeoutMs)
{
  JSound *s = snd(h);
  struct timespec until;
  deadlineIn(&until, timeoutMs);
  jint frames = len / s->channels;
  pthread_mutex_lock(&s->lock);
  if (s->ring == NULL)
    {
      pthread_mutex_unlock(&s->lock);
      return 0;
    }
  while (s->count >= s->ringFrames)
    if (pthread_cond_timedwait(&s->cond, &s->lock, &until) == ETIMEDOUT)
      break;
  int64 space = s->ringFrames - s->count;
  if (frames > space)
    frames = (jint) space;
  jshort *src = (jshort *) env->GetPrimitiveArrayCritical(data, NULL);
  if (src != NULL)
    {
      for (jint i = 0; i < frames; i++)
        {
          int64 slot = (s->head + s->count) % s->ringFrames;
          memcpy(s->ring + slot * s->channels,
                 src + off + i * s->channels, s->channels * sizeof(int16));
          s->count++;
        }
      env->ReleasePrimitiveArrayCritical(data, src, JNI_ABORT);
    }
  else
    frames = 0;
  pthread_mutex_unlock(&s->lock);
  return frames * s->channels;
}

/* Free space in the ring buffer, in samples. */
JNIEXPORT jint JNICALL
Java_gnu_javax_sound_sampled_haiku_HaikuAudio_available(JNIEnv *, jclass,
                                                        jlong h)
{
  JSound *s = snd(h);
  pthread_mutex_lock(&s->lock);
  jint a = s->ring == NULL ? 0
                           : (jint) ((s->ringFrames - s->count) * s->channels);
  pthread_mutex_unlock(&s->lock);
  return a;
}

JNIEXPORT void JNICALL
Java_gnu_javax_sound_sampled_haiku_HaikuAudio_flush(JNIEnv *, jclass,
                                                    jlong h)
{
  JSound *s = snd(h);
  pthread_mutex_lock(&s->lock);
  s->count = 0;
  pthread_cond_broadcast(&s->cond);
  pthread_mutex_unlock(&s->lock);
}

JNIEXPORT void JNICALL
Java_gnu_javax_sound_sampled_haiku_HaikuAudio_setGain(JNIEnv *, jclass,
                                                      jlong h, jfloat gain)
{
  JSound *s = snd(h);
  pthread_mutex_lock(&s->lock);
  s->gain = gain;
  pthread_mutex_unlock(&s->lock);
}

} /* extern "C" */
