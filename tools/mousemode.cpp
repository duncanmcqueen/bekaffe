// Print or set the Haiku mouse focus mode (0 normal, -1 click to focus,
// 1 focus follows mouse).
#include <Application.h>
#include <InterfaceDefs.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv)
{
  BApplication app("application/x-vnd.bekaffe-mousemode");
  if (argc > 1)
    set_mouse_mode((mode_mouse) atoi(argv[1]));
  printf("%d\n", (int) mouse_mode());
  return 0;
}
