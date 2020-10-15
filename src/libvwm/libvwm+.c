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

#ifdef self
#undef self
#endif

#ifdef $my
#undef $my
#endif

//#define self(__f__, ...) this->self.__f__ (this, ##__VA_ARGS__)
#define $my(__p__) this->prop->__p__

#define Vwm    $my(vwm)->self
#define Vwin   $my(vwm)->win
#define Vframe $my(vwm)->frame
#define Vterm  $my(vwm)->term

struct vwm_ex_prop {
  vwm_t *vwm;

  this_T *__This__;
  E_T    *__E__;

  ed_t   *ed;
  win_t  *win;
  buf_t  *buf;

  EdToplineMethod orig_topline;
  string_t *topline;
  video_t  *video;
};

private int cstring_cmp_n (const char *sa, const char *sb, size_t n) {
  const uchar *spa = (const uchar *) sa;
  const uchar *spb = (const uchar *) sb;
  for (;n--; spa++, spb++) {
    if (*spa != *spb)
      return (*(uchar *) spa - *(uchar *) spb);

    if (*spa == 0) return 0;
  }

  return 0;
}

private int cstring_eq_n  (const char *sa, const char *sb, size_t n) {
  return (0 == cstring_cmp_n (sa, sb, n));
}

private char *strstr (const char *str, const char *substr) {
  while (*str != '\0') {
    if (*str == *substr) {
      const char *spa = str + 1;
      const char *spb = substr + 1;

      while (*spa && *spb){
        if (*spa != *spb)
          break;
        spa++; spb++;
      }

      if (*spb == '\0')
        return (char *) str;
    }

    str++;
  }

  return NULL;
}

private void ed_set_topline_void (ed_t *ed, buf_t *buf) {
  (void) ed; (void) buf;
  video_t *video = Ed.get.video (ed);
  string_t *topline = Ed.get.topline (ed);
  String.replace_with (topline, "[Command Mode] VirtualWindowManager");
  Video.set.row_with (video, 0, topline->bytes);
}

private int ed_exit (ed_t *ed, int retval) {
  Ed.set.state_bit (ed, ED_PAUSE);
  return retval;
}

private string_t *filter_ed_rline (string_t *);
private string_t *filter_ed_rline (string_t *line) {
  char *sp = strstr (line->bytes, "--fname=");
  if (NULL is sp) return line;
  String.delete_numbytes_at (line, 8, sp - line->bytes);
  return filter_ed_rline (line);
}

private int ed_rline_cb (buf_t **bufp, rline_t *rl, utf8 c) {
  (void) bufp; (void) c;
  vwm_ex *this = (vwm_ex *) Rline.get.user_object (rl);

  int retval = RLINE_NO_COMMAND;
  string_t *com = Rline.get.command (rl);

  if (Cstring.eq (com->bytes, "quit")) {
    int state = Vwm.get.state ($my(vwm));
    state |= VWM_QUIT;
    Vwm.set.state ($my(vwm), state);
    retval = VWM_QUIT;
    goto theend;

  } else if (Cstring.eq (com->bytes, "frame_delete")) {
    Vwin.delete_frame (Vwm.get.current_win ($my(vwm)),
        Vwm.get.current_frame ($my(vwm)), DRAW);
    retval = OK;
    goto theend;
  } else if (Cstring.eq (com->bytes, "split_and_fork")) {
    retval = OK;

    vwm_win *win = Vwm.get.current_win ($my(vwm));
    vwm_frame *frame = Vwin.add_frame (win, 0, NULL, DONOT_DRAW);
    if (NULL is frame)  goto theend;

    string_t *command = Rline.get.anytype_arg (rl, "command");
    if (NULL is command) {
      Vframe.set.command (frame, Vwm.get.default_app ($my(vwm)));
    } else {
      command = filter_ed_rline (command);
      Vframe.set.command (frame, command->bytes);
    }

    Vframe.fork (frame);
    goto theend;
  } else if (Cstring.eq (com->bytes, "win_new")) {
    string_t *a_draw = Rline.get.anytype_arg (rl, "draw");
    string_t *a_focus = Rline.get.anytype_arg (rl, "focus");
    string_t *a_num_frames = Rline.get.anytype_arg (rl, "num-frames");
    Vstring_t *a_commands  = Rline.get.anytype_args (rl, "command");

    int draw  = (NULL is a_draw  ? 1 : atoi (a_draw->bytes));
    int focus = (NULL is a_focus ? 1 : atoi (a_focus->bytes));
    int num_frames = (NULL is a_num_frames ? 1 : atoi (a_num_frames->bytes));
    if (num_frames is 0 or num_frames > MAX_FRAMES) num_frames = 1;

    char *commands[num_frames];
    char *command = Vwm.get.default_app ($my(vwm));

    if (NULL is a_commands) {
      for (int i = 0; i < num_frames; i++)
          commands[i] = command;
    } else {
      int num = 0;
      vstring_t *it = a_commands->head;
      while (it and num < num_frames) {
        commands[num++] = it->data->bytes;
        it = it->next;
      }

      while (num < num_frames)
        commands[num++] = command;
    }

    Vwm.new.win ($my(vwm), NULL, WinNewOpts (
        .rows = Vwm.get.lines ($my(vwm)),
        .cols = Vwm.get.columns ($my(vwm)),
        .num_frames = num_frames,
        .max_frames = MAX_FRAMES,
        .draw = draw,
        .focus = focus,
        .commands = commands));

    Vstring.free (a_commands);
    goto theend;
  } else if (Cstring.eq (com->bytes, "ed")) {
    string_t *line = Rline.get.line (rl);
    String.delete_numbytes_at (line, 2, 0);
    String.prepend (line, Vwm.get.editor ($my(vwm)));
    line = filter_ed_rline (line);
    char *commands[] = {line->bytes};
    Vwm.new.win ($my(vwm), NULL, WinNewOpts (
        .rows = Vwm.get.lines ($my(vwm)),
        .cols = Vwm.get.columns ($my(vwm)),
        .num_frames = 1,
        .max_frames = MAX_FRAMES,
        .focus = 1,
        .draw = 1,
        .commands = commands));
    goto theend;
  }

theend:
  String.free (com);

  return ed_exit ($my(ed), retval);
}

private int vwm_new_rline (vwm_t *vwm, vwm_win *win, vwm_frame *frame, void *object, int should_return) {
  (void) frame;
  vwm_ex *this = (vwm_ex *) object;

  $my(win) = Ed.get.current_win ($my(ed));
  $my(buf) = Ed.get.current_buf ($my(ed));
  Win.draw ($my(win));

  rline_t *rl = Ed.rline.new_with ($my(ed), "\t");
  Rline.set.user_object (rl, (void *) this);
  Rline.set.state_bit (rl, RL_PROCESS_CHAR);

  if (should_return)
    Rline.set.opts_bit (rl, RL_OPT_RETURN_AFTER_TAB_COMPLETION);

  int retval = Buf.rline (&$my(buf), rl);

  win = Vwm.get.current_win (vwm);

  ifnot (Vwin.get.num_frames (win)) {
    Vwm.change_win (vwm, win, PREV_POS, DONOT_DRAW);

    Vwm.release_win (vwm, win);

    win = Vwm.get.current_win (vwm);
  }

  ifnot (NULL is win)
    Vwin.draw (win);

  return retval;
}

private int vwm_tab_callback (vwm_t *vwm, vwm_win *win, vwm_frame *frame, void *object) {
  return vwm_new_rline (vwm, win, frame, object, 1);
}

private int vwm_rline_callback (vwm_t *vwm, vwm_win *win, vwm_frame *frame, void *object) {
  return vwm_new_rline (vwm, win, frame, object, 0);
}

private int vwm_edit_file_callback (vwm_t *vwm, char *file, void *object) {
  vwm_ex *this = (vwm_ex *) object;

  ed_t *ed = E.new ($my(__E__), QUAL(ED_INIT,
      .num_win = 1, .init_cb = __init_ext__,
      .term_flags = (TERM_DONOT_CLEAR_SCREEN|TERM_DONOT_RESTORE_SCREEN)));

  E.set.state_bit (THIS_E, E_DONOT_CHANGE_FOCUS|E_PAUSE);

  win_t *win = Ed.get.current_win (ed);
  buf_t *buf = Win.buf.new (win, QUAL(BUF_INIT, .fname = file));
  Win.append_buf (win, buf);
  Win.set.current_buf (win, 0, DONOT_DRAW);

  Ed.set.topline = $my(orig_topline);

  int retval = E.main ($my(__E__), buf);

  E.delete ($my(__E__), E.get.idx ($my(__E__), ed), 0);

  Ed.set.topline = ed_set_topline_void;

  Vterm.raw_mode (Vwm.get.term (vwm));

  return retval;
}

private vwm_t *vex_get_vwm (vwm_ex *this) {
  return $my(vwm);
}

private int vex_init_ved (vwm_ex *this) {
  if (NULL is __init_this__ ())
    return NOTOK;

  $my(__This__) = __This__;
  $my(__E__)    = $my(__This__)->__E__;

  E.set.at_init_cb ($my(__E__), __init_ext__);
  E.set.at_exit_cb ($my(__E__), __deinit_ext__);
  E.set.state_bit  ($my(__E__), E_DONOT_RESTORE_TERM_STATE);

  $my(ed) = E.new ($my(__E__), QUAL(ED_INIT,
      .num_win = 1,
      .init_cb = __init_ext__,
      .term_flags = TERM_DONOT_CLEAR_SCREEN|TERM_DONOT_RESTORE_SCREEN|TERM_DONOT_SAVE_SCREEN
        ));

  Ed.deinit_commands ($my(ed));

  Ed.append.rline_command ($my(ed), "quit", 0, 0);

  Ed.append.rline_command ($my(ed), "frame_delete", 0, 0);
  Ed.append.command_arg   ($my(ed), "frame_delete", "--idx=", 6);

  Ed.append.rline_command ($my(ed), "split_and_fork", 0, 0);
  Ed.append.command_arg ($my(ed),   "split_and_fork", "--command={", 11);

  Ed.append.rline_command ($my(ed), "win_new", 0, 0);
  Ed.append.command_arg ($my(ed), "win_new", "--draw=", 7);
  Ed.append.command_arg ($my(ed), "win_new", "--focus=", 8);
  Ed.append.command_arg ($my(ed), "win_new", "--command={", 11);
  Ed.append.command_arg ($my(ed), "win_new", "--num-frames=", 13);

  Ed.append.rline_command ($my(ed), "ed", 0, 0);
  if (cstring_eq_n ("veda", Vwm.get.editor ($my(vwm)), 4)) {
    Ed.append.command_arg ($my(ed), "ed", "--exit", 6);
    Ed.append.command_arg ($my(ed), "ed", "--pager", 7);
    Ed.append.command_arg ($my(ed), "ed", "--ftype=", 8);
    Ed.append.command_arg ($my(ed), "ed", "--column=", 9);
    Ed.append.command_arg ($my(ed), "ed", "--line-nr=", 10);
    Ed.append.command_arg ($my(ed), "ed", "--num-win=", 10);
    Ed.append.command_arg ($my(ed), "ed", "--exec-com=", 11);
    Ed.append.command_arg ($my(ed), "ed", "--autosave=", 12);
    Ed.append.command_arg ($my(ed), "ed", "--exit-quick", 13);
    Ed.append.command_arg ($my(ed), "ed", "--load-file=", 13);
    Ed.append.command_arg ($my(ed), "ed", "--backupfile", 13);
    Ed.append.command_arg ($my(ed), "ed", "--backup-suffix=", 17);
 }

  Ed.set.rline_cb ($my(ed), ed_rline_cb);

  $my(win) = Ed.get.current_win ($my(ed));
  $my(buf) = Win.buf.new ($my(win), QUAL(BUF_INIT,
      .fname = STR_FMT
         ("%s/vwm_unamed", E.get.env ($my(__E__), "data_dir")->bytes)));

  Buf.set.on_emptyline ($my(buf), "");
  Buf.set.show_statusline ($my(buf), 0);

  Win.append_buf ($my(win), $my(buf));
  Win.set.current_buf ($my(win), 0, DONOT_DRAW);

  $my(video) = Ed.get.video ($my(ed));
  $my(topline) = Ed.get.topline ($my(ed));

  String.clear ($my(topline));
  String.append ($my(topline), "[Command Mode] VirtualWindowManager");
  Video.set.row_with ($my(video), 0, $my(topline)->bytes);

  $my(orig_topline) = Ed.set.topline;
  Ed.set.topline = ed_set_topline_void;

  Vwm.set.rline_callback ($my(vwm), vwm_rline_callback);
  Vwm.set.on_tab_callback ($my(vwm), vwm_tab_callback);
  Vwm.set.edit_file_callback ($my(vwm), vwm_edit_file_callback);

  return OK;
}

private vwm_term *vex_init_term (vwm_ex *this, int *rows, int *cols) {
  vwm_t *vwm = $my(vwm);

  vwm_term *term =  Vwm.get.term (vwm);

  Vterm.raw_mode (term);

  Vterm.init_size (term, rows, cols);

  Vwm.set.size (vwm, *rows, *cols, 1);

  return term;
}

public vwm_ex *__init_vwm_ex__ (vwm_t *vwm) {
  vwm_ex *this = Alloc (sizeof (vwm_ex));

  this->prop = Alloc (sizeof (vwm_ex_prop));

  this->self = (vwm_ex_self) {
    .get = (vwm_ex_get_self) {
      .vwm = vex_get_vwm
    },
    .init = (vwm_ex_init_self) {
      .ved = vex_init_ved,
      .term = vex_init_term
    }
  };

  if (NULL is vwm)
    $my(vwm) = __init_vwm__ ();
  else
    $my(vwm) = vwm;

  Vwm.set.user_object ($my(vwm), (void *) this);

  return this;
}

public void __deinit_vwm_ex__ (vwm_ex **thisp) {
  if (NULL is *thisp) return;

  vwm_ex *this = *thisp;

  __deinit_this__ (&$my(__This__));

  free (this->prop);
  free (this);
  *thisp = NULL;
}
