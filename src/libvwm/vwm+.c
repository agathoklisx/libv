/* This is like vwn.c, but it uses the extended library.

  As an extension, MODKEY-tab should offer a tab completion, that
  offers a menu with the declared commands. This menu can be navigated
  with the arrow keys and can be narrowed by pressing matching letters.
  A command can be accepted with the space key or if there is one left that|and matches
  the typed letters.
  An escape key cansels or if there is no match.
  Similarly MODKEY-: brings you to the same readline interface, that can
  offer argument|command|file completion.

  This also allows editing log files without the need to fork.

  It offers also an abstraction level to the applications.
 */

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libvwm.h>
#include <libvwm+.h>

#define Vwm    this->self
#define Vframe this->frame
#define Vwin   this->win
#define Vterm  this->term
#define Vex    vex->self

int main (int argc, char **argv) {
  vwm_ex *vex = __init_vwm_ex__ (NULL);
  vwm_t  *this = Vex.get.vwm (vex);

  int rows, cols;
  vwm_term *term = Vex.init.term (vex, &rows, &cols);

  Vex.init.ved (vex);

  vwm_win *win = Vwm.new.win (this, "v", WinNewOpts (
    .rows = rows,
    .cols = cols,
    .num_frames = 2,
    .max_frames = 3));

  vwm_frame *frame = Vwin.get.frame_at (win, 0);

  if (argc > 1)
    Vframe.set.argv (frame, argc-1, argv + 1);

  Vframe.set.argv (frame, argc-1, argv + 1);
  Vframe.set.log (frame, NULL, 1);

  char *largv[] = {"bash", NULL};
  frame = Vwin.get.frame_at (win, 1);
  Vframe.set.argv (frame, 1, largv);
  Vframe.set.log (frame, NULL, 1);

  win = Vwm.new.win (this, NULL, WinNewOpts (
    .rows =rows,
    .cols = cols,
    .num_frames = 1,
    .max_frames = 3));

  char *llargv[] = {"zsh", NULL};
  frame = Vwin.get.frame_at (win, 0);
  Vframe.set.argv (frame, 1, llargv);
  Vframe.set.log (frame, NULL, 1);
  Vframe.fork (frame);

  Vterm.screen.save (term);
  Vterm.screen.clear (term);

  int retval = Vwm.main (this);

  Vterm.screen.restore (term);

  __deinit_vwm_ex__ (&vex);
  __deinit_vwm__ (&this);

  return retval;
}
