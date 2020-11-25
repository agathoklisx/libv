/* An application that utilises the library, that might be useful
 * as a tiny window manager, though it is meant for demonstration.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/termios.h>

#include <libv/libvwm.h>

#define Vwm    this->self
#define Vframe this->frame
#define Vwin   this->win
#define Vterm  this->term

int main (int argc, char **argv) {
  vwm_t *this = __init_vwm__ ();

  vwm_term *term =  Vwm.get.term (this);

  Vterm.raw_mode (term);

  int rows, cols;
  Vterm.init_size (term, &rows, &cols);

  Vwm.set.size (this, rows, cols, 1);

  vwm_win *win = Vwm.new.win (this, "v", WinOpts (
    .num_rows = rows,
    .num_cols = cols,
    .num_frames = 2,
    .max_frames = 3));

  vwm_frame *frame = Vwin.get.frame_at (win, 0);
  if (argc > 1)
     Vframe.set.argv (frame, argc-1, argv + 1);

  Vframe.set.log (frame, NULL, 1);

  char *largv[] = {"bash", NULL};
  frame = Vwin.get.frame_at (win, 1);
  Vframe.set.argv (frame, 1, largv);
  Vframe.set.log (frame, NULL, 1);

  win = Vwm.new.win (this, NULL, WinOpts (
    .num_rows =rows,
    .num_cols = cols,
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

  __deinit_vwm__ (&this);

  return retval;
}
