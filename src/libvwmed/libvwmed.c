#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <errno.h>

#include <libv/libvwm.h>
#include <libv/libvwmed.h>
#include <libv/libvci.h>

//#define self(__f__, ...) this->self.__f__ (this, ##__VA_ARGS__)
#define $my(__p__) this->prop->__p__

#define Vwm    ((vwm_t *) $my(objects)[VWM_OBJECT])->self
#define Vwin   ((vwm_t *) $my(objects)[VWM_OBJECT])->win
#define Vframe ((vwm_t *) $my(objects)[VWM_OBJECT])->frame
#define Vterm  ((vwm_t *) $my(objects)[VWM_OBJECT])->term

struct vwmed_prop {
  this_T *__This__;
  E_T    *__E__;

  ed_t   *ed;
  win_t  *win;
  buf_t  *buf;

  EdToplineMethod orig_topline;
  string_t *topline;
  video_t  *video;

  void *objects[NUM_OBJECTS];
  int state;
};

#define VWMED_VFRAME_CLEAR_VIDEO_MEM    (1 << 0)
#define VWMED_VFRAME_CLEAR_LOG          (1 << 1)
#define VWMED_CLEAR_CURRENT_FRAME       (1 << 2)
#define VWMED_BUF_IS_PAGER              (1 << 3)
#define VWMED_BUF_HASNOT_EMPTYLINE      (1 << 4)
#define VWMED_BUF_DONOT_SHOW_STATUSLINE (1 << 5)
#define VWMED_BUF_DONOT_SHOW_TOPLINE    (1 << 6)

private void ed_set_topline_vwmed (ed_t *ed, buf_t *buf) {
  (void) ed; (void) buf;
  video_t *video = Ed.get.video (ed);
  string_t *topline = Ed.get.topline (ed);
  String.replace_with (topline, "[Command Mode] VirtualWindowManager");
  Video.set.row_with (video, 0, topline->bytes);
}

private void ed_set_topline_void (ed_t *ed, buf_t *buf) {
  (void) ed; (void) buf;
  video_t *video = Ed.get.video (ed);
  string_t *topline = Ed.get.topline (ed);
  String.replace_with (topline, "");
  Video.set.row_with (video, 0, topline->bytes);
}

private int ed_exit (ed_t *ed, int retval) {
  Ed.set.state_bit (ed, ED_PAUSE);
  return retval;
}

private string_t *filter_ed_rline (string_t *);
private string_t *filter_ed_rline (string_t *line) {
  char *sp = Cstring.bytes_in_str (line->bytes, "--fname=");
  if (NULL is sp) return line;
  String.delete_numbytes_at (line, 8, sp - line->bytes);
  return filter_ed_rline (line);
}

private int vwmed_edit_file_cb (vwm_t *, char *, void *);

private void vwmed_get_info (vwmed_t *this, vwm_t *vwm) {
  tmpfname_t *tmpn = File.tmpfname.new (Vwm.get.tmpdir (vwm), "vwmed_info");
  if (NULL is tmpn or -1 is tmpn->fd) return;

  FILE *fp = fdopen (tmpn->fd, "w+");

  vwm_info *vinfo = Vwm.get.info (vwm);

  fprintf (fp, "==- Vwm Info -==\n");
  fprintf (fp, "Master Pid         : %d\n", vinfo->pid);
  fprintf (fp, "Sequences fname    : %s\n", vinfo->sequences_fname);
  fprintf (fp, "Unimplemented fname: %s\n", vinfo->unimplemented_fname);
  fprintf (fp, "Num windows        : %d\n", vinfo->num_win);

  for (int widx = 0; widx < vinfo->num_win; widx++) {
    vwin_info *w_info = vinfo->wins[widx];
    fprintf (fp, "\n--= window [%d]--=\n", widx + 1);
    fprintf (fp, "Window name        : %s\n", w_info->name);
    fprintf (fp, "Num rows           : %d\n", w_info->num_rows);
    fprintf (fp, "Num frames         : %d\n", w_info->num_frames);
    fprintf (fp, "Visible frames     : %d\n", w_info->num_visible_frames);

    for (int fidx = 0; fidx < w_info->num_frames; fidx++) {
      vframe_info *f_info = w_info->frames[fidx];
      fprintf (fp, "\n--= %s frame [%d] =--\n", w_info->name, fidx);
      fprintf (fp, "At frame           : %d\n", f_info->at_frame);
      fprintf (fp, "Frame pid          : %d\n", f_info->pid);
      fprintf (fp, "Frame first row    : %d\n", f_info->first_row);
      fprintf (fp, "Frame last row     : %d\n", f_info->last_row);
      fprintf (fp, "Frame is visible   : %d\n", f_info->is_visible);
      fprintf (fp, "Frame logfile      : %s\n", f_info->logfile);
      fprintf (fp, "Frame argv         :");

      int arg = 0;
      while (f_info->argv[arg])
        fprintf (fp, " %s", f_info->argv[arg++]);
      fprintf (fp, "\n");
    }
  }

  fflush (fp);

  $my(state) |= (VWMED_BUF_IS_PAGER|VWMED_BUF_HASNOT_EMPTYLINE|
                 VWMED_BUF_DONOT_SHOW_STATUSLINE|VWMED_BUF_DONOT_SHOW_TOPLINE);

  vwmed_edit_file_cb (vwm, tmpn->fname->bytes, this);

  Vwm.release_info (vwm, &vinfo);
  File.tmpfname.free (tmpn);
}

private int ed_rline_cb (buf_t **bufp, rline_t *rl, utf8 c) {
  (void) bufp; (void) c;
  vwmed_t *this = (vwmed_t *) Rline.get.user_object (rl);
  vwm_t *vwm = $my(objects)[VWM_OBJECT];

  int retval = RLINE_NO_COMMAND;
  string_t *com = Rline.get.command (rl);

  if (Cstring.eq (com->bytes, "quit")) {
    int state = Vwm.get.state (vwm);
    state |= VWM_QUIT;
    Vwm.set.state (vwm, state);
    retval = VWM_QUIT;
    goto theend;

  } else if (Cstring.eq (com->bytes, "frame_delete")) {
    Vwin.delete_frame (Vwm.get.current_win (vwm),
        Vwm.get.current_frame (vwm), DRAW);
    retval = OK;
    goto theend;

  } else if (Cstring.eq (com->bytes, "frame_clear")) {
    $my(state) |= (VWMED_CLEAR_CURRENT_FRAME|VWMED_VFRAME_CLEAR_VIDEO_MEM|VWMED_VFRAME_CLEAR_LOG);
    string_t *clear_log = Rline.get.anytype_arg (rl, "clear-log");
    string_t *clear_mem = Rline.get.anytype_arg (rl, "clear-video-mem");

    ifnot (NULL is clear_log)
      if (atoi (clear_log->bytes) is 0)
        $my(state) &= ~VWMED_VFRAME_CLEAR_LOG;

    ifnot (NULL is clear_mem)
      if (atoi (clear_log->bytes) is 0)
        $my(state) &= ~VWMED_VFRAME_CLEAR_VIDEO_MEM;

    retval = OK;
    goto theend;

  } else if (Cstring.eq (com->bytes, "split_and_fork")) {
    vwm_win *win = Vwm.get.current_win (vwm);
    vwm_frame *frame = Vwin.add_frame (win, 0, NULL, DONOT_DRAW);
    if (NULL is frame)  goto theend;

    string_t *command = Rline.get.anytype_arg (rl, "command");
    if (NULL is command) {
      Vframe.set.command (frame, Vwm.get.default_app (vwm));
    } else {
      command = filter_ed_rline (command);
      Vframe.set.command (frame, command->bytes);
    }

    Vframe.fork (frame);

    retval = OK;
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

    char *command = Vwm.get.default_app (vwm);

    win_opts w_opts = WinOpts (
        .rows = Vwm.get.lines (vwm),
        .cols = Vwm.get.columns (vwm),
        .num_frames = num_frames,
        .max_frames = MAX_FRAMES,
        .draw = draw,
        .focus = focus);

    if (NULL is a_commands) {
      for (int i = 0; i < num_frames; i++)
        w_opts.frame_opts[i].command = command;
    } else {
      int num = 0;
      vstring_t *it = a_commands->head;
      while (it and num < num_frames) {
        w_opts.frame_opts[num++].command = filter_ed_rline (it->data)->bytes;
        it = it->next;
      }

      while (num < num_frames)
        w_opts.frame_opts[num++].command = command;
    }

    Vwm.new.win (vwm, NULL, w_opts);

    Vstring.free (a_commands);

    retval = OK;
    goto theend;

  } else if (Cstring.eq (com->bytes, "ed")) {
    string_t *line = Rline.get.line (rl);
    String.delete_numbytes_at (line, 2, 0);
    String.prepend (line, Vwm.get.editor (vwm));
    line = filter_ed_rline (line);

    win_opts w_opts = WinOpts (
        .rows = Vwm.get.lines (vwm),
        .cols = Vwm.get.columns (vwm),
        .num_frames = 1,
        .max_frames = MAX_FRAMES,
        .focus = 1,
        .draw = 1);

    w_opts.frame_opts[0].command = line->bytes;

    Vwm.new.win (vwm, NULL, w_opts);

    retval = OK;
    goto theend;

  } else if (Cstring.eq (com->bytes, "set")) {
    string_t *log_file = Rline.get.anytype_arg (rl, "log-file");
    if (NULL is log_file)
      goto theend;
    int set_log = atoi (log_file->bytes);
    if (set_log)
      Vframe.set.log (Vwm.get.current_frame (vwm), NULL, 1);
    else
      Vframe.release_log (Vwm.get.current_frame (vwm));

    retval = OK;
    goto theend;
  } else if (Cstring.eq (com->bytes, "info")) {
    vwmed_get_info (this, vwm);
    retval = OK;
    goto theend;
  }

theend:
  String.free (com);

  return ed_exit ($my(ed), retval);
}

private int vwm_new_rline (vwm_t *vwm, vwm_win *win, vwm_frame *frame, void *object, int should_return, int has_init_completion) {
  vwmed_t *this = (vwmed_t *) object;

  $my(win) = Ed.get.current_win ($my(ed));
  $my(buf) = Ed.get.current_buf ($my(ed));
  Win.draw ($my(win));

  rline_t *rl = NULL;

  if (has_init_completion)
    rl = Ed.rline.new_with ($my(ed), "\t");
  else
    rl = Ed.rline.new ($my(ed));

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

  ifnot (NULL is win) {
    Vwin.draw (win);
    if ($my(state) & VWMED_CLEAR_CURRENT_FRAME) {
      $my(state) &= ~VWMED_CLEAR_CURRENT_FRAME;
      Vframe.clear (frame, $my(state));
    }
  }

  return retval;
}

private int vwmed_tab_cb (vwm_t *vwm, vwm_win *win, vwm_frame *frame, void *object) {
  return vwm_new_rline (vwm, win, frame, object, 1, 1);
}

private int vwmed_rline_cb (vwm_t *vwm, vwm_win *win, vwm_frame *frame, void *object) {
  return vwm_new_rline (vwm, win, frame, object, 0, 0);
}

private int vwmed_edit_file_cb (vwm_t *vwm, char *file, void *object) {
  vwmed_t *this = (vwmed_t *) object;
/*
      .frow = 3,
      .fcol = 8,
      .lines = 16,
      .columns = 20,
*/
  ed_t *ed = E.new ($my(__E__), EdOpts(
      .num_win = 1,
      .init_cb = __init_ext__,
      .term_flags = (TERM_DONOT_CLEAR_SCREEN|TERM_DONOT_RESTORE_SCREEN)));

  E.set.state_bit (THIS_E, E_DONOT_CHANGE_FOCUS|E_PAUSE);

  win_t *win = Ed.get.current_win (ed);

  int is_pager = $my(state) & VWMED_BUF_IS_PAGER;
  buf_t *buf = Win.buf.new (win, BufOpts(
      .fname = file,
      .flags = (is_pager ? BUF_IS_PAGER : 0)));

  $my(state) &= ~VWMED_BUF_IS_PAGER;

  int hasnot_emptyline = $my(state) & VWMED_BUF_HASNOT_EMPTYLINE;
  if (hasnot_emptyline) {
    $my(state) &= ~VWMED_BUF_HASNOT_EMPTYLINE;
    Buf.set.on_emptyline (buf, " ");
  }

  int donot_show_statusline = $my(state) & VWMED_BUF_DONOT_SHOW_STATUSLINE;
  if (donot_show_statusline) {
    $my(state) &= ~VWMED_BUF_DONOT_SHOW_STATUSLINE;
    Buf.set.show_statusline (buf, 0);
  }

  Win.append_buf (win, buf);
  Win.set.current_buf (win, 0, DONOT_DRAW);

  int donot_show_topline = $my(state) & VWMED_BUF_DONOT_SHOW_TOPLINE;
  if (donot_show_topline) {
    Ed.set.topline = ed_set_topline_void;
    $my(state) &= ~VWMED_BUF_DONOT_SHOW_TOPLINE;
  } else
    Ed.set.topline = $my(orig_topline);

  int retval = E.main ($my(__E__), buf);

  E.delete ($my(__E__), E.get.idx ($my(__E__), ed), 0);

  Ed.set.topline = ed_set_topline_vwmed;

  Vterm.raw_mode (Vwm.get.term (vwm));

  return retval;
}

private E_T *vwmed_get_e (vwmed_t *this) {
  return $my(__E__);
}

private void *vwmed_get_object (vwmed_t *this, int idx) {
  if (idx >= NUM_OBJECTS or idx < 0) return NULL;
  return $my(objects)[idx];
}

private int vwmed_init_ved (vwmed_t *this) {
  vwm_t *vwm = $my(objects)[VWM_OBJECT];

  E.set.at_init_cb ($my(__E__), __init_ext__);
  E.set.at_exit_cb ($my(__E__), __deinit_ext__);
  E.set.state_bit  ($my(__E__), E_DONOT_RESTORE_TERM_STATE);

  string_t *hrl_file = String.new_with (E.get.env ($my(__E__), "data_dir")->bytes);
  String.append_fmt (hrl_file, "/.%s_libv_hist_rline", E.get.env ($my(__E__), "user_name")->bytes);

  $my(ed) = E.new ($my(__E__), EdOpts(
      .flags = ED_INIT_OPT_LOAD_HISTORY,
      .num_win = 1,
      .hrl_file = hrl_file->bytes,
      .init_cb = __init_ext__,
      .term_flags = TERM_DONOT_CLEAR_SCREEN|TERM_DONOT_RESTORE_SCREEN|TERM_DONOT_SAVE_SCREEN
     ));

  String.free (hrl_file);

  Ed.deinit_commands ($my(ed));

  Ed.append.rline_command ($my(ed), "quit", 0, 0);

  Ed.append.rline_command ($my(ed), "frame_delete", 0, 0);
  Ed.append.command_arg   ($my(ed), "frame_delete", "--idx=", 6);

  Ed.append.rline_command ($my(ed), "frame_clear", 0, 0);
  Ed.append.command_arg   ($my(ed), "frame_clear", "--clear-log=", 12);
  Ed.append.command_arg   ($my(ed), "frame_clear", "--clear-video-mem=", 18);

  Ed.append.rline_command ($my(ed), "split_and_fork", 0, 0);
  Ed.append.command_arg   ($my(ed), "split_and_fork", "--command={", 11);

  Ed.append.rline_command ($my(ed), "win_new", 0, 0);
  Ed.append.command_arg   ($my(ed), "win_new", "--draw=", 7);
  Ed.append.command_arg   ($my(ed), "win_new", "--focus=", 8);
  Ed.append.command_arg   ($my(ed), "win_new", "--command={", 11);
  Ed.append.command_arg   ($my(ed), "win_new", "--num-frames=", 13);

  Ed.append.rline_command ($my(ed), "set", 0, 0);
  Ed.append.command_arg   ($my(ed), "set", "--log-file=", 11);

  Ed.append.rline_command ($my(ed), "ed", 0, 0);
  if (Cstring.eq_n ("veda", Vwm.get.editor (vwm), 4)) {
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

  Ed.append.rline_command ($my(ed), "info", 0, 0);

  Ed.set.rline_cb ($my(ed), ed_rline_cb);

  $my(win) = Ed.get.current_win ($my(ed));
  $my(buf) = Win.buf.new ($my(win), BufOpts(
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
  Ed.set.topline = ed_set_topline_vwmed;

  Vwm.set.rline_cb (vwm, vwmed_rline_cb);
  Vwm.set.on_tab_cb (vwm, vwmed_tab_cb);
  Vwm.set.edit_file_cb (vwm, vwmed_edit_file_cb);

  return OK;
}

private vwm_term *vwmed_init_term (vwmed_t *this, int *rows, int *cols) {
  vwm_t *vwm = $my(objects)[VWM_OBJECT];

  vwm_term *term =  Vwm.get.term (vwm);

  Vterm.raw_mode (term);

  Vterm.init_size (term, rows, cols);

  Vwm.set.size (vwm, *rows, *cols, 1);

  return term;
}

public vwmed_t *__init_vwmed__ (vwm_t *vwm) {
  if (NULL is __init_this__ ())
    return NULL;

  vwmed_t *this = Alloc (sizeof (vwmed_t));

  this->prop = Alloc (sizeof (vwmed_prop));

  this->self = (vwmed_self) {
    .get = (vwmed_get_self) {
      .e = vwmed_get_e,
      .object = vwmed_get_object
    },
    .init = (vwmed_init_self) {
      .ved = vwmed_init_ved,
      .term = vwmed_init_term
    }
  };

  $my(__This__) = __This__;
  $my(__E__)    = $my(__This__)->__E__;

  $my(state) = 0;

  if (NULL is vwm)
    vwm = __init_vwm__ ();

  $my(objects)[VWM_OBJECT] = vwm;

  Vwm.set.object (vwm, this, VWMED_OBJECT);

  return this;
}

public void __deinit_vwmed__ (vwmed_t **thisp) {
  if (NULL is *thisp) return;

  vwmed_t *this = *thisp;

  __deinit_this__ (&$my(__This__));

  free (this->prop);
  free (this);
  *thisp = NULL;
}
