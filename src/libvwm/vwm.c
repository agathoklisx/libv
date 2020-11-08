/* A testing application.
 * 
 * Interface and Usage: (MODKEY is CTRL-\ by default)
 * 
 * This creates two windows (cycle with MODKEY-[right|left] arrow).
 * 
 * The first window creates two frames (cycle with MODKEY-[up|down] arrow).
 * 
 * The first frame is reserved for the first argument in the command line.
 * If no argument the frame is stays inactive.
 *   (use MODKEY-c to fork the default application (by default this is the zsh shell)
 *    or  MODKEY-! (open a shell (again the default is the zsh shell)
 *   to make it functional).
 * 
 * Use MODKEY-s to split the window and create a new frame (if the length is less than
 * max_frames window initialization option).
 * Likewise, use MODKEY-S-[!ce], but also fork either a shell or the default application,
 * or an editor (by default vim).
 * 
 * Use MODKEY-d to delete the current frame.
 * Use MODKEY-[+-] to [in|de]crease the size of the current frame.
 * Use MODKEY-[param]= to set the size of the current frame.
 *
 * Use MODKEY-[param]n to create a new window (max-frames equals to param).
 *
 * Use MODKEY-[E|PAGE_UP] to edit the scrollback buffer (if it has been set at
 * the frame initialization) with the editor (by default vim). The last
 * lines will be the ones on the display.
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
    .rows = rows,
    .cols = cols,
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

  __deinit_vwm__ (&this);

  return retval;
}
