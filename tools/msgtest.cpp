// Test helper for the Haiku AWT peer. It sends messages straight to the
// windows of a Java program, so tests do not move the real mouse.
//
//   msgtest list                       window titles of the Java program
//   msgtest click "<title>" X Y        click at view point (X, Y)
//   msgtest im "<title>"               a Japanese input method session
//   msgtest clip-image                 put a picture on the clipboard
//   msgtest clip-dump                  show what is on the clipboard
#include <Application.h>
#include <Bitmap.h>
#include <Clipboard.h>
#include <Input.h>
#include <InterfaceDefs.h>
#include <View.h>
#include <Messenger.h>
#include <OS.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *kJavaApp = "application/x-vnd.gnu-classpath-awt";

static BMessenger
javaApp()
{
  return BMessenger(kJavaApp);
}

static bool
windowTitle(int32 index, BString *title)
{
  BMessage get(B_GET_PROPERTY), reply;
  get.AddSpecifier("Title");
  get.AddSpecifier("Window", index);
  if (javaApp().SendMessage(&get, &reply, 2000000, 2000000) != B_OK)
    return false;
  return reply.FindString("result", title) == B_OK;
}

// The messenger of the AWT view ("awt") of the window with this title.
static bool
viewMessenger(const char *title, BMessenger *out)
{
  BString t;
  for (int32 i = 0; i < 50 && windowTitle(i, &t); i++)
    {
      if (t != title)
        continue;
      BMessage get(B_GET_PROPERTY), reply;
      get.AddSpecifier("Messenger");
      get.AddSpecifier("View", "awt");
      get.AddSpecifier("Window", i);
      if (javaApp().SendMessage(&get, &reply, 2000000, 2000000) == B_OK
          && reply.FindMessenger("result", out) == B_OK)
        return true;
    }
  fprintf(stderr, "no window \"%s\"\n", title);
  return false;
}

static void
mouse(BMessenger &view, uint32 what, BPoint p, int32 buttons)
{
  BMessage m(what);
  m.AddInt64("when", system_time());
  m.AddPoint("where", p);
  m.AddPoint("be:view_where", p);
  m.AddInt32("modifiers", 0);
  m.AddInt32("buttons", buttons);
  m.AddInt32("clicks", 1);
  view.SendMessage(&m);
}

class App : public BApplication
{
public:
  App(int argc, char **argv)
    : BApplication("application/x-vnd.awt-msgtest"), fArgc(argc),
      fArgv(argv), fStatus(1)
  {
  }

  virtual void ReadyToRun()
  {
    thread_id t = spawn_thread(TestThread, "test", B_NORMAL_PRIORITY, this);
    resume_thread(t);
  }

  // Location replies of the input method session.
  virtual void MessageReceived(BMessage *msg)
  {
    if (msg->what == B_INPUT_METHOD_EVENT)
      {
        BPoint p;
        float h = 0;
        msg->FindFloat("be:height_reply", &h);
        printf("location reply: height %.0f, points", h);
        for (int32 i = 0; msg->FindPoint("be:location_reply", i, &p) == B_OK;
             i++)
          printf(" (%.0f,%.0f)", p.x, p.y);
        printf("\n");
        return;
      }
    BApplication::MessageReceived(msg);
  }

  int Status() { return fStatus; }

private:
  static int32 TestThread(void *data)
  {
    App *app = (App *) data;
    app->fStatus = app->Test();
    fflush(stdout);
    app->PostMessage(B_QUIT_REQUESTED);
    return 0;
  }

  void Changed(BMessenger &view, const char *text, bool confirmed,
               int32 selStart, int32 selEnd)
  {
    BMessage m(B_INPUT_METHOD_EVENT);
    m.AddInt32("be:opcode", B_INPUT_METHOD_CHANGED);
    m.AddString("be:string", text);
    m.AddBool("be:confirmed", confirmed);
    m.AddInt32("be:selection", selStart);
    m.AddInt32("be:selection", selEnd);
    view.SendMessage(&m);
  }

  int Test()
  {
    const char *cmd = fArgc > 1 ? fArgv[1] : "";
    if (strcmp(cmd, "list") == 0)
      {
        BString t;
        for (int32 i = 0; windowTitle(i, &t); i++)
          printf("%ld: %s\n", (long) i, t.String());
        return 0;
      }
    if (strcmp(cmd, "click") == 0 && fArgc == 5)
      {
        BMessenger view;
        if (!viewMessenger(fArgv[2], &view))
          return 1;
        BPoint p(atoi(fArgv[3]), atoi(fArgv[4]));
        mouse(view, B_MOUSE_DOWN, p, B_PRIMARY_MOUSE_BUTTON);
        snooze(100000);
        mouse(view, B_MOUSE_UP, p, 0);
        return 0;
      }
    if (strcmp(cmd, "im") == 0 && fArgc == 3)
      {
        BMessenger view;
        if (!viewMessenger(fArgv[2], &view))
          return 1;
        BMessage start(B_INPUT_METHOD_EVENT);
        start.AddInt32("be:opcode", B_INPUT_METHOD_STARTED);
        start.AddMessenger("be:reply_to", BMessenger(be_app));
        view.SendMessage(&start);
        // "nihongo" in kana, then converting "nihon" (6 bytes = 2 chars).
        Changed(view, "\xe3\x81\xab\xe3\x81\xbb\xe3\x82\x93\xe3\x81\x94",
                false, 0, 0);
        snooze(1500000);
        Changed(view, "\xe6\x97\xa5\xe6\x9c\xac\xe3\x81\x94", false, 0, 6);
        snooze(1500000);
        BMessage loc(B_INPUT_METHOD_EVENT);
        loc.AddInt32("be:opcode", B_INPUT_METHOD_LOCATION_REQUEST);
        view.SendMessage(&loc);
        snooze(1500000);
        // Confirmed: "nihongo" in kanji.
        Changed(view, "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e", true, 9, 9);
        snooze(500000);
        BMessage stop(B_INPUT_METHOD_EVENT);
        stop.AddInt32("be:opcode", B_INPUT_METHOD_STOPPED);
        view.SendMessage(&stop);
        snooze(1500000);
        return 0;
      }
    if (strcmp(cmd, "clip-image") == 0)
      {
        // 64 x 48, green with a blue corner pixel, as ShowImage copies.
        BBitmap bmp(BRect(0, 0, 63, 47), B_RGBA32);
        uint32 *bits = (uint32 *) bmp.Bits();
        for (int32 i = 0; i < 64 * 48; i++)
          bits[i] = 0xFF20C040;
        bits[0] = 0xFF0000FF;
        BMessage archive;
        bmp.Archive(&archive);
        if (!be_clipboard->Lock())
          return 1;
        be_clipboard->Clear();
        be_clipboard->Data()->AddMessage("image/bitmap", &archive);
        be_clipboard->Commit();
        be_clipboard->Unlock();
        printf("picture on the clipboard\n");
        return 0;
      }
    if (strcmp(cmd, "clip-dump") == 0)
      {
        if (!be_clipboard->Lock())
          return 1;
        BMessage *data = be_clipboard->Data();
        char *name;
        type_code type;
        int32 count;
        for (int32 i = 0;
             data->GetInfo(B_ANY_TYPE, i, &name, &type, &count) == B_OK; i++)
          {
            ssize_t size = 0;
            const void *ptr;
            data->FindData(name, type, &ptr, &size);
            printf("%s type '%c%c%c%c' %ld bytes\n", name,
                   (char) (type >> 24), (char) (type >> 16),
                   (char) (type >> 8), (char) type, (long) size);
          }
        be_clipboard->Unlock();
        return 0;
      }
    fprintf(stderr, "usage: msgtest list | click TITLE X Y | im TITLE"
                    " | clip-image | clip-dump\n");
    return 2;
  }

  int fArgc;
  char **fArgv;
  int fStatus;
};

int
main(int argc, char **argv)
{
  App app(argc, argv);
  app.Run();
  return app.Status();
}
