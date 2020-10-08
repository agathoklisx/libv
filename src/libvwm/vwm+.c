/* This is like vwn.c, but it uses the extended library.

  As an extension, MODKEY-tab should offer a tab completion, that
  offers a menu with the declared commands. This menu can be navigated
  with the arrow keys and can be narrowed by pressing matching letters.
  A command can be accepted with the space key or if there is one left that|and matches
  the typed letters.
  An escape key cansels or if there is no match.
 */

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libvwm.h>
#include <libvwm+.h>

static vwm_t *VWM;
#define Vwm VWM->self

int main (int argc, char **argv) {
  vwm_t *this = __init_vwm__ ();
  VWM = this;

  vwm_ex_t *ex = __init_vwm_ex__ (this);

  vwm_term *term =  Vwm.get.term (this);

  Vwm.term.raw_mode (term);

  int rows, cols;
  Vwm.term.init_size (term, &rows, &cols);

  Vwm.set.size (this, rows, cols, 1);

  vwm_win *win = Vwm.win.new (this, "v", WinNewOpts (
    .rows = rows,
    .cols = cols,
    .num_frames = 2,
    .max_frames = 3));

  vwm_frame *frame = Vwm.win.get.frame_at (win, 0);
  Vwm.frame.set.argv (frame, argc-1, argv + 1);
  Vwm.frame.set.log (frame, NULL, 1);

  char *largv[] = {"bash", NULL};
  frame = Vwm.win.get.frame_at (win, 1);
  Vwm.frame.set.argv (frame, 1, largv);
  Vwm.frame.set.log (frame, NULL, 1);

  win = Vwm.win.new (this, NULL, WinNewOpts (
    .rows =rows,
    .cols = cols,
    .num_frames = 1,
    .max_frames = 3));

  char *llargv[] = {"zsh", NULL};
  frame = Vwm.win.get.frame_at (win, 0);
  Vwm.frame.set.argv (frame, 1, llargv);
  Vwm.frame.set.log (frame, NULL, 1);
  Vwm.frame.fork (this, frame);

  Vwm.term.screen.save (term);
  Vwm.term.screen.clear (term);

  int retval = Vwm.main (this);

  Vwm.term.screen.restore (term);

  __deinit_vwm_ex__ (&ex);
  __deinit_vwm__ (&this);

  return retval;
}
