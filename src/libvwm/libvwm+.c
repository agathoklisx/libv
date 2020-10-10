/* This is an extended library, that depends on libved at:
   https://github.com/agathoklisx/libved

   This at the begining can be used for a tab completion mechanism,
   but the big idea is, that we get also for free a ui and a tiny interpeter,
   but also if desirable (while building libved+), a more rich but also very small
   and simple programming language, a C compiler, libcurl, a json parser, a math expression
   library and all the functionality from this library.
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <libvwm.h>
#include <libvwm+.h>

static vwm_ex_t *ThisVwm;

#define Vwm ThisVwm->vwm->self

private void __ed_set_topline_void (ed_t *ed, buf_t *this) {
  (void) ed; (void) this; // There is no way to change
}

private int exit_ed (ed_t *ed, int retval) {
  int state = Ed.get.state (ed);
  state |= ED_PAUSE;
  Ed.set.state (ed, state);
  return retval;
}

private int __rline_cb__ (buf_t **bufp, rline_t *rl, utf8 c) {
  (void) bufp; (void) c;
  vwm_ex_t *ex = (vwm_ex_t *) Rline.get.user_object (rl);
  int retval = RLINE_NO_COMMAND;
  string_t *com = Rline.get.command (rl);

  if (Cstring.eq (com->bytes, "quit")) {
    int state = Vwm.get.state (ex->vwm);
    state |= VWM_QUIT;
    Vwm.set.state (ex->vwm, state);
    retval = VWM_QUIT;
    goto theend;

  } else if (Cstring.eq (com->bytes, "framedelete")) {
    Vwm.win.delete_frame (ex->vwm,
        Vwm.get.current_win (ex->vwm), Vwm.get.current_frame (ex->vwm), DRAW);
    retval = OK;
    goto theend;
  } else if (Cstring.eq (com->bytes, "split")) {
    Vwm.win.add_frame (ex->vwm, Vwm.get.current_win (ex->vwm), 0, NULL, DRAW);
    retval = OK;
    goto theend;
  } else if (Cstring.eq (com->bytes, "fork")) {
    retval = OK;
    vwm_frame *frame = Vwm.get.current_frame (ex->vwm);
    pid_t pid = Vwm.frame.get.pid (frame);
    if (pid > 0) goto theend;

    string_t *command = Rline.get.anytype_arg (rl, "command");
    if (NULL is command) {
      char *argv[] = {Vwm.get.shell (ex->vwm)->bytes, NULL};
      Vwm.frame.set.argv (frame, 1, argv);
    } else
      Vwm.frame.set.command (frame, command->bytes);

    Vwm.frame.fork (ex->vwm, frame);
    goto theend;
  }

theend:
  String.free (com);

  return exit_ed (ex->ed, retval);
}

private int tab_callback (vwm_t *this, vwm_win *win, vwm_frame *frame, void *object) {
  (void) frame;
  vwm_ex_t *ex = (vwm_ex_t *) object;
  ex->win = Ed.get.current_win (ex->ed);
  ex->buf = Ed.get.current_buf (ex->ed);
  Win.draw (ex->win);
  rline_t *rl = Ed.rline.new_with (ex->ed, "\t");
  Rline.set.user_object (rl, (void *) ex);

  int state = Rline.get.state (rl);
  state |= RL_PROCESS_CHAR;
  Rline.set.state (rl, state);
  int opts = Rline.get.opts (rl);
  opts |= RL_OPT_RETURN_AFTER_TAB_COMPLETION;
  Rline.set.opts (rl, opts);

  int retval = Buf.rline (&ex->buf, rl);
  win = Vwm.get.current_win (this);
  Vwm.win.draw (win);
  return retval;
}

private int rline_callback (vwm_t *this, vwm_win *win, vwm_frame *frame, void *object) {
  (void) frame;
  vwm_ex_t *ex = (vwm_ex_t *) object;
  ex->win = Ed.get.current_win (ex->ed);
  ex->buf = Ed.get.current_buf (ex->ed);
  Win.draw (ex->win);
  rline_t *rl = Ed.rline.new_with (ex->ed, "\t");
  Rline.set.user_object (rl, (void *) ex);

  int state = Rline.get.state (rl);
  state |= RL_PROCESS_CHAR;
  Rline.set.state (rl, state);

  int retval = Buf.rline (&ex->buf, rl);
  win = Vwm.get.current_win (this);
  Vwm.win.draw (win);
  return retval;
}

private int __init_editor__ (vwm_ex_t *ex) {
  if (NULL is __init_this__ ())
    return NOTOK;

  ex->__This__ = __This__;
  ex->__E__    = ex->__This__->__E__;

  E.set.at_init_cb (ex->__E__, __init_ext__);
  E.set.at_exit_cb (ex->__E__, __deinit_ext__);

  ex->ed = E.new (ex->__E__, QUAL(ED_INIT,
      .num_win = 1,
      .init_cb = __init_ext__));

  Ed.deinit_commands (ex->ed);

  Ed.append.rline_command (ex->ed, "quit", 0, 0);
  Ed.append.rline_command (ex->ed, "framedelete", 0, 0);
  Ed.append.rline_command (ex->ed, "split", 0, 0);
  Ed.append.rline_command (ex->ed, "fork", 0, 0);

  Ed.append.command_arg (ex->ed, "framedelete", "--idx=", 6);
  Ed.append.command_arg (ex->ed, "fork",        "--command=", 10);

  Ed.set.rline_cb (ex->ed, __rline_cb__);

  ex->win = Ed.get.current_win (ex->ed);

  ex->buf = Win.buf.new (ex->win, QUAL(BUF_INIT,
      .fname = STR_FMT
         ("%s/vwm_unamed", E.get.env (ex->__E__, "data_dir")->bytes)));

  Buf.set.show_statusline (ex->buf, 0);

  string_t *on_emptyline = String.new (2);
  Buf.set.on_emptyline (ex->buf, on_emptyline);

  //Buf.set.normal.at_beg_cb (buf, __buf_on_normal_beg);

  Win.append_buf (ex->win, ex->buf);
  Win.set.current_buf (ex->win, 0, DONOT_DRAW);

  ex->video = Ed.get.video (ex->ed);
  ex->topline = Ed.get.topline (ex->ed);
  String.clear (ex->topline);
  String.append (ex->topline, "[Command Mode] VirtualWindowManager");
  Video.set.row_with (ex->video, 0, ex->topline->bytes);
  Ed.set.topline = __ed_set_topline_void;

  return OK;
}

public vwm_ex_t *__init_vwm_ex__ (vwm_t *vwm) {
  vwm_ex_t *ex = AllocType  (vwm_ex);

  __init_editor__ (ex);

  ex->vwm = vwm;
  ThisVwm = ex;

  Vwm.set.user_object (vwm, (void *) ex);
  Vwm.set.rline_callback (vwm, rline_callback);
  Vwm.set.on_tab_callback (vwm, tab_callback);

  return ex;
}

public void __deinit_vwm_ex__ (vwm_ex_t **exp) {
  if (NULL is *exp) return;

  vwm_ex_t *ex = *exp;

  __deinit_this__ (&ex->__This__);

  free (ex);
  *exp = NULL;
}
