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
//#define Vframe this->frame
//#define Vwin   this->win
#define Vterm  this->term
#define Vwmed  vwmed->self

int main (int argc, char **argv) {
  vwmed_t *vwmed = __init_vwmed__ (NULL);
  vwm_t *this = Vwmed.get.object (vwmed, VWM_OBJECT);

  int rows, cols;
  vwm_term *term = Vwmed.init.term (vwmed, &rows, &cols);

  Vwmed.init.ved (vwmed);

  win_opts w_opts = WinOpts (
      .num_rows = rows,
      .num_cols = cols,
      .num_frames = 1,
      .max_frames = 3);

  if (argc > 1) {
    w_opts.frame_opts[0].argv = argv + 1;
    w_opts.frame_opts[0].argc = argc - 1;
  }

  w_opts.frame_opts[0].enable_log = 1;

  Vwm.new.win (this, "main", w_opts);

  Vterm.screen.save (term);
  Vterm.screen.clear (term);

  int retval = Vwm.main (this);

  Vterm.screen.restore (term);

  __deinit_vwmed__ (&vwmed);
  __deinit_vwm__ (&this);

  return retval;
}
