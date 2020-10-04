#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <libvwm.h>

int main (int argc, char **argv) {

  vwm_t *this = __init_vwm__ ();

  vwm_term *term =  my.get.term (this);

  my.term.raw_mode (term);

  int rows, cols;
  my.term.init_size (term, &rows, &cols);

  my.set.size (this, rows, cols, 1);

  vwm_win *win = my.win.new (this, "v", WinNewOpts (
    .rows = rows,
    .cols = cols,
    .num_frames = 2,
    .max_frames = 2));

  vwm_frame *frame = my.win.get.frame_at (win, 0);

  my.frame.set.argv (frame, argc-1, argv + 1);
  my.frame.set.log (frame, NULL, 1);
  char *largv[] = {"sh", NULL};
  frame = my.win.get.frame_at (win, 1);
  my.frame.set.argv (frame, 1, largv);

  win = my.win.new (this, NULL, WinNewOpts (
    .rows =rows,
    .cols = cols,
    .num_frames = 3,
    .max_frames = 3));

  frame = my.win.get.frame_at (win, 0);

  char *llargv[] = {"bash", NULL};
  my.frame.set.argv (frame, argc-1, argv + 1);
  frame = my.win.get.frame_at (win, 0);
  my.frame.set.argv (frame, 1, llargv);

  my.frame.fork (this, frame);
  frame = my.win.get.frame_at (win, 1);
  my.frame.set.argv (frame, 1, llargv);
  my.frame.fork (this, frame);

  frame = my.win.get.frame_at (win, 2);
  my.frame.set.argv (frame, 1, llargv);
  my.frame.fork (this, frame);

  my.term.screen.save (term);
  my.term.screen.clear (term);

  int retval = my.main (this);

  my.term.screen.restore (term);

  __deinit_vwm__ (&this);

  return retval;
}
