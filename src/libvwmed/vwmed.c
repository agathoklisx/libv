#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>

#include <libv/libvwm.h>
#include <libv/libvwmed.h>
#include <libv/libvci.h>

#define Vwm    this->self
#define Vframe this->frame
#define Vwin   this->win
#define Vterm  this->term
#define Vwmed  vwmed->self

int main (int argc, char **argv) {
  vwmed_t *vwmed = __init_vwmed__ (NULL);
  vwm_t *this = Vwmed.get.vwm (vwmed);

  int rows, cols;
  vwm_term *term = Vwmed.init.term (vwmed, &rows, &cols);

  Vwmed.init.ved (vwmed);

  vwm_win *win = Vwm.new.win (this, "main", WinNewOpts (
    .rows = rows,
    .cols = cols,
    .num_frames = 1,
    .max_frames = 3));

  vwm_frame *frame = Vwin.get.frame_at (win, 0);

  if (argc > 1)
    Vframe.set.argv (frame, argc-1, argv + 1);

  Vframe.set.log (frame, NULL, 1);

  Vterm.screen.save (term);
  Vterm.screen.clear (term);

  int retval = Vwm.main (this);

  Vterm.screen.restore (term);

  __deinit_vwmed__ (&vwmed);
  __deinit_vwm__ (&this);

  return retval;
}
