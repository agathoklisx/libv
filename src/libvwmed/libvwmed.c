#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <termios.h>
#include <signal.h>
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

  int state;

  EdToplineMethod orig_topline;
  string_t *topline;
  video_t  *video;

  int num_rline_cbs;
  VwmedRline_cb *rline_cbs;

  int num_rline_command_cbs;
  VwmedRlineCommand_cb *rline_command_cbs;

  int num_info_cbs;
  VwmedInfo_cb *info_cbs;

  VwmEditFile_cb edit_file_cb;

  void *objects[NUM_OBJECTS];
};

#define VWMED_SHM_FILE  "vwmed_shm"
#define VWMED_SHM_ID    65

#define VWMED_VFRAME_CLEAR_VIDEO_MEM    (1 << 0)
#define VWMED_VFRAME_CLEAR_LOG          (1 << 1)
#define VWMED_CLEAR_CURRENT_FRAME       (1 << 2)
#define VWMED_BUF_IS_PAGER              (1 << 3)
#define VWMED_BUF_HASNOT_EMPTYLINE      (1 << 4)
#define VWMED_BUF_DONOT_SHOW_STATUSLINE (1 << 5)
#define VWMED_BUF_DONOT_SHOW_TOPLINE    (1 << 6)
#define VWMED_RLINE_HAS_INIT_COMPLETION (1 << 7)
#define VWMED_RLINE_SHOULD_RETURN       (1 << 8)
#define VWMED_IPC                       (1 << 9)

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

private int vwmed_process_frame (vwmed_t *this, vwm_win *win, vwm_frame *frame) {
  int frame_fd = Vframe.get.fd (frame);

  if (frame_fd is -1) return NOTOK;

  fd_set read_mask;
  struct timeval *tv = NULL;

  char
    input_buf[MAX_CHAR_LEN],
    output_buf[BUFSIZE];

  Vwin.set.frame (win, frame);

  int
    maxfd = frame_fd,
    numready,
    output_len;

  for (;;) {
    FD_ZERO (&read_mask);
    FD_SET (STDIN_FILENO, &read_mask);

    if (0 is Vframe.check_pid (frame))
      goto theend;

    FD_SET (frame_fd, &read_mask);

    if (0 >= (numready = select (maxfd + 1, &read_mask, NULL, NULL, tv))) {
      switch (errno) {
        case EIO:
        case EINTR:
        default:
          break;
      }

      continue;
    }

    for (int i = 0; i < MAX_CHAR_LEN; i++) input_buf[i] = '\0';

    if (FD_ISSET (STDIN_FILENO, &read_mask)) {
      if (0 < read (STDIN_FILENO, input_buf, 1))
        write (frame_fd, input_buf, 1);
    }

    if (FD_ISSET (frame_fd, &read_mask)) {
      output_buf[0] = '\0';
      if (0 > (output_len = read (frame_fd, output_buf, BUFSIZE))) {
        switch (errno) {
          case EIO:
          default:
            Vframe.check_pid (frame);
            goto theend;
        }
      }
      output_buf[output_len] = '\0';

      Vframe.process_output (frame, output_buf, output_len);
    }
  }

theend:
  return OK;
}

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
  fprintf (fp, "Current window idx : %d\n", vinfo->cur_win_idx);

  for (int widx = 0; widx < vinfo->num_win; widx++) {
    vwin_info *w_info = vinfo->wins[widx];
    fprintf (fp, "\n--= window [%d]--=\n", widx + 1);
    fprintf (fp, "Window name        : %s\n", w_info->name);
    fprintf (fp, "It is current      : %s\n", (w_info->is_current ? "Yes" : "No"));
    fprintf (fp, "Num rows           : %d\n", w_info->num_rows);
    fprintf (fp, "Num frames         : %d\n", w_info->num_frames);
    fprintf (fp, "Visible frames     : %d\n", w_info->num_visible_frames);
    fprintf (fp, "Current frame idx  : %d\n", w_info->cur_frame_idx);

    for (int fidx = 0; fidx < w_info->num_frames; fidx++) {
      vframe_info *f_info = w_info->frames[fidx];
      fprintf (fp, "\n--= %s frame [%d] =--\n", w_info->name, fidx);
      fprintf (fp, "At frame           : %d\n", f_info->at_frame);
      fprintf (fp, "Frame pid          : %d\n", f_info->pid);
      fprintf (fp, "Frame num rows     : %d\n", f_info->num_rows);
      fprintf (fp, "Frame first row    : %d\n", f_info->first_row);
      fprintf (fp, "Frame last row     : %d\n", f_info->last_row);
      fprintf (fp, "It is current      : %s\n", (f_info->is_current ? "Yes" : "No"));
      fprintf (fp, "Frame is visible   : %s\n", (f_info->is_visible ? "Yes" : "No"));
      fprintf (fp, "Frame logfile      : %s\n", (f_info->logfile[0] isnot 0 ? f_info->logfile : "Hasn't been set"));
      fprintf (fp, "Frame argv         :");

      int arg = 0;
      while (f_info->argv[arg])
        fprintf (fp, " %s", f_info->argv[arg++]);
      fprintf (fp, "\n");
    }
  }

  fflush (fp);

  for (int i = 0; i < $my(num_info_cbs); i++)
    $my(info_cbs)[i] (this, vwm, fp);

  $my(state) |= (VWMED_BUF_IS_PAGER|VWMED_BUF_HASNOT_EMPTYLINE|
                 VWMED_BUF_DONOT_SHOW_STATUSLINE|VWMED_BUF_DONOT_SHOW_TOPLINE);

  $my(edit_file_cb) (vwm, Vwm.get.current_frame (vwm), tmpn->fname->bytes, this);

  Vwm.release_info (vwm, &vinfo);
  //File.tmpfname.free (tmpn);
}

private int vwmed_process_rline (vwmed_t *this, rline_t *rl, vwm_t *vwm, vwm_win *win, vwm_frame *frame) {
  int retval;
  string_t *com = NULL;

  for (int i = 0; i < $my(num_rline_cbs); i++) {
    retval = $my(rline_cbs)[i] (this, rl, vwm, win, frame);
    if (RLINE_NO_COMMAND isnot retval)
      goto theend;
  }

  retval = RLINE_NO_COMMAND;
  com = Rline.get.command (rl);

  if (Cstring.eq (com->bytes, "quit")) {
    int state = Vwm.get.state (vwm);
    state |= VWM_QUIT;
    Vwm.set.state (vwm, state);
    retval = VWM_QUIT;
    goto theend;

  } else if (Cstring.eq (com->bytes, "frame_delete")) {
    Vwin.delete_frame (win, frame, DRAW);
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
    vwm_frame *n_frame = Vwin.add_frame (win, 0, NULL, DONOT_DRAW);
    if (NULL is n_frame)  goto theend;

    string_t *command = Rline.get.anytype_arg (rl, "command");
    if (NULL is command) {
      Vframe.set.command (n_frame, Vwm.get.default_app (vwm));
    } else {
      command = filter_ed_rline (command);
      Vframe.set.command (n_frame, command->bytes);
    }

    Vframe.fork (n_frame);

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
        .num_rows = Vwm.get.lines (vwm),
        .num_cols = Vwm.get.columns (vwm),
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
        .num_rows = Vwm.get.lines (vwm),
        .num_cols = Vwm.get.columns (vwm),
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
      Vframe.set.log (frame, NULL, 1);
    else
      Vframe.release_log (frame);

    retval = OK;
    goto theend;
  } else if (Cstring.eq (com->bytes, "info")) {
    vwmed_get_info (this, vwm);
    retval = OK;
    goto theend;
  }

theend:
  String.free (com);
  return retval;
}

private int ed_rline_ipc_cb (vwmed_t *this, vwm_t *vwm, rline_t *rl) {
  key_t key = ftok (STR_FMT ("%s/" VWMED_SHM_FILE, Vwm.get.tmpdir (vwm)), VWMED_SHM_ID);
  int shmid = shmget (key, 1024, 0666|IPC_CREAT);
  char *rline = (char *) shmat (shmid, (void *)0, 0);

  string_t *sline = Rline.get.line (rl);
  for (size_t i = 0; i < sline->num_bytes; i++)
    rline[i] = sline->bytes[i];
  rline[sline->num_bytes] = '\0';

  shmdt (rline);
  shmctl (shmid, IPC_RMID, NULL);
  return OK;
}

private int ed_rline_cb (buf_t **bufp, rline_t *rl, utf8 c) {
  (void) bufp; (void) c;

  vwmed_t *this = (vwmed_t *) Rline.get.user_object (rl);
  vwm_t *vwm = $my(objects)[VWM_OBJECT];

  if ($my(state) & VWMED_IPC) {
    $my(state) &= ~VWMED_IPC;
    return ed_rline_ipc_cb (this, vwm, rl);
  }

  vwm_win *win = Vwm.get.current_win (vwm);
  vwm_frame *frame = Vwin.get.current_frame (win);

  int retval = vwmed_process_rline (this, rl, vwm, win, frame);
  return ed_exit ($my(ed), retval);
}

private int vwm_new_rline_fallback (vwm_t *vwm, vwm_win *win, vwm_frame *frame, void *object, int should_return, int has_init_completion) {
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

private int vwmed_rline_at_fork_cb (vwm_frame *frame, vwm_t *vwm, vwm_win *vwin) {
  vwmed_t *this = vwm->self.get.object (vwm, VWMED_OBJECT);
  (void) vwin; (void) frame;

  $my(win) = Ed.get.current_win ($my(ed));
  $my(buf) = Ed.get.current_buf ($my(ed));

  signal (SIGWINCH, sigwinch_handler);
  kill (getpid(), SIGWINCH);

  Buf.set.on_emptyline ($my(buf), " ");
  Buf.set.show_statusline ($my(buf), 0);
  Ed.set.topline = ed_set_topline_void;

  rline_t *rl = NULL;

  if ($my(state) & VWMED_RLINE_HAS_INIT_COMPLETION) {
    $my(state) &= ~VWMED_RLINE_HAS_INIT_COMPLETION;
    rl = Ed.rline.new_with ($my(ed), "\t");
  } else
    rl = Ed.rline.new ($my(ed));

  Rline.set.user_object (rl, (void *) this);
  Rline.set.state_bit (rl, RL_PROCESS_CHAR);

  if ($my(state) & VWMED_RLINE_SHOULD_RETURN) {
    $my(state) &= ~VWMED_RLINE_SHOULD_RETURN;
    Rline.set.opts_bit (rl, RL_OPT_RETURN_AFTER_TAB_COMPLETION);
  }

  $my(state) |= VWMED_IPC;

  Buf.rline (&$my(buf), rl);

  exit (ed_exit ($my(ed), 0));
}

private int vwm_new_rline (vwm_t *vwm, vwm_win *win, vwm_frame *cur_frame, void *object, int should_return, int has_init_completion) {
  vwmed_t *this = (vwmed_t *) object;

  int retval;
  vframe_info *finfo = Vframe.get.info (cur_frame);

  if (finfo->num_rows < Ed.get.min_rows ($my(ed))) {
    retval = vwm_new_rline_fallback (vwm, win, cur_frame, object, should_return, has_init_completion);
    Vframe.release_info (finfo);
    return retval;
  }

  vwm_frame *frame = Vwin.new_frame (win, FrameOpts(
      .first_row = finfo->first_row,
      .num_rows = finfo->num_rows,
      .at_frame = finfo->at_frame,
      .fork = 0,
      .is_visible = 0,
      .create_fd = 1,
      .at_fork_cb = vwmed_rline_at_fork_cb));

  Vframe.release_info (finfo);

  $my(win) = Ed.get.current_win ($my(ed));
  $my(buf) = Ed.get.current_buf ($my(ed));

  $my(state) &= ~(VWMED_RLINE_HAS_INIT_COMPLETION|VWMED_RLINE_SHOULD_RETURN);
  if (has_init_completion) $my(state) |= VWMED_RLINE_HAS_INIT_COMPLETION;
  if (should_return)       $my(state) |= VWMED_RLINE_SHOULD_RETURN;

  Vframe.set.visibility (cur_frame, 0);
  Vframe.set.visibility (frame, 1);
  Vwin.set.frame_as_current (win, frame);

  Vframe.fork (frame);

  key_t key = ftok (STR_FMT ("%s/" VWMED_SHM_FILE, Vwm.get.tmpdir (vwm)), VWMED_SHM_ID);
  int shmid = shmget (key, 1024, 0666|IPC_CREAT);
  char *rline = (char *) shmat (shmid, (void *)0, 0);

  vwmed_process_frame (this, win, frame);

  Vframe.set.visibility (cur_frame, 1);
  Vwin.set.frame_as_current (win, cur_frame);
  Vframe.set.visibility (frame, 0);
  Vwin.delete_frame (win, frame, DRAW);

  rline_t *rl = Ed.rline.new_with ($my(ed), rline);

  shmdt (rline);

  rl = Rline.parse (rl, $my(buf));
  retval = vwmed_process_rline (this, rl, vwm, win, cur_frame);

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
      Vframe.clear (cur_frame, $my(state));
    }
  }

  Ed.set.topline = ed_set_topline_void;

  Vterm.raw_mode (Vwm.get.term (vwm));

  return retval;
}

private int vwmed_tab_cb (vwm_t *vwm, vwm_win *win, vwm_frame *frame, void *object) {
  return vwm_new_rline (vwm, win, frame, object, 1, 1);
}

private int vwmed_rline_cb (vwm_t *vwm, vwm_win *win, vwm_frame *frame, void *object) {
  return vwm_new_rline (vwm, win, frame, object, 0, 0);
}

private int vwmed_edit_file (vwmed_t *this, vwm_t *vwm, char *fname) {
  char *cwd = Dir.current ();

  ed_t *ed = E.new ($my(__E__), EdOpts(
      .num_win = 1,
      .init_cb = __init_ext__,
      .term_flags = (TERM_DONOT_CLEAR_SCREEN|TERM_DONOT_RESTORE_SCREEN)));

  E.set.state_bit (THIS_E, E_DONOT_CHANGE_FOCUS|E_PAUSE);

  win_t *win = Ed.get.current_win (ed);

  int is_pager = $my(state) & VWMED_BUF_IS_PAGER;
  buf_t *buf = Win.buf.new (win, BufOpts(
      .fname = fname,
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

  Ed.set.topline = ed_set_topline_void;

  Vterm.raw_mode (Vwm.get.term (vwm));

  chdir (cwd);
  free (cwd);

  return retval;
}

private int vwmed_edit_file_at_fork_cb (vwm_frame *frame, vwm_t *vwm, vwm_win *vwin) {
  (void) vwin;

  vwmed_t *this = vwm->self.get.object (vwm, VWMED_OBJECT);

  term_t *term = E.get.term ($my(__E__));
  Term.set_mode (term, 'o');

  char *fname = Vframe.get.argv (frame)[0];

  vwmed_edit_file  (this, vwm, fname);
  exit (0);
}

private int vwmed_edit_file_cb (vwm_t *vwm, vwm_frame *frame, char *fname, void *object) {
  (void) vwm;
  vwmed_t *this = (vwmed_t *) object;
  vframe_info *finfo = Vframe.get.info (frame);

  int retval;

  if (finfo->num_rows < Ed.get.min_rows ($my(ed))) {
    retval = vwmed_edit_file (this, vwm, fname);
    Vframe.release_info (finfo);
    return retval;
  }

  vwm_win *win = Vframe.get.parent (frame);

  vwm_frame *n_frame = Vwin.new_frame (win, FrameOpts(
      .command = fname,
      .first_row = finfo->first_row,
      .num_rows = finfo->num_rows,
      .at_frame = finfo->at_frame,
      .fork = 0,
      .is_visible = 0,
      .create_fd = 1,
      .at_fork_cb = vwmed_edit_file_at_fork_cb));

  Vframe.release_info (finfo);

  Vframe.set.visibility (frame, 0);
  Vframe.set.visibility (n_frame, 1);
  Vwin.set.frame_as_current (win, n_frame);
  Vframe.clear (frame, 0);

  Vframe.fork (n_frame);

  vwmed_process_frame (this, win, n_frame);

  Vwin.set.frame_as_current (win, frame);
  Vframe.set.visibility (frame, 1);
  Vframe.set.visibility (n_frame, 0);
  Vwin.delete_frame (win, n_frame, DRAW);

  return OK;
}

private E_T *vwmed_get_e (vwmed_t *this) {
  return $my(__E__);
}

private void *vwmed_get_object (vwmed_t *this, int idx) {
  if (idx >= NUM_OBJECTS or idx < 0) return NULL;
  return $my(objects)[idx];
}

private void vwmed_set_object (vwmed_t *this, void *object, int idx) {
  if (idx >= NUM_OBJECTS or idx < 0) return;
  $my(objects)[idx] = object;
}

private void vwmed_set_rline_cb (vwmed_t *this, VwmedRline_cb cb) {
  $my(num_rline_cbs)++;
  ifnot ($my(num_rline_cbs) - 1)
    $my(rline_cbs) = Alloc (sizeof (VwmedRline_cb));
  else
    $my(rline_cbs) = Realloc ($my(rline_cbs), sizeof (VwmedRline_cb) * $my(num_rline_cbs));

  $my(rline_cbs)[$my(num_rline_cbs) - 1] = cb;
}

private void vwmed_set_rline_command_cb (vwmed_t *this, VwmedRlineCommand_cb cb) {
  $my(num_rline_command_cbs)++;
  ifnot ($my(num_rline_command_cbs) - 1)
    $my(rline_command_cbs) = Alloc (sizeof (VwmedRlineCommand_cb));
  else
    $my(rline_command_cbs) = Realloc ($my(rline_command_cbs), sizeof (VwmedRlineCommand_cb) * $my(num_rline_command_cbs));

  $my(rline_command_cbs)[$my(num_rline_command_cbs) - 1] = cb;
}

private void vwmed_set_info_cb (vwmed_t *this, VwmedInfo_cb cb) {
  $my(num_info_cbs)++;
  ifnot ($my(num_info_cbs) - 1)
    $my(info_cbs) = Alloc (sizeof (VwmedInfo_cb));
  else
    $my(info_cbs) = Realloc ($my(info_cbs), sizeof (VwmedInfo_cb) * $my(num_info_cbs));

  $my(info_cbs)[$my(num_info_cbs) - 1] = cb;
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

  Ed.append.rline_command ($my(ed), "info", 0, 0);

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

  for (int i = 0; i < $my(num_rline_command_cbs); i++)
    $my(rline_command_cbs)[i] ($my(ed));

  Ed.set.rline_cb ($my(ed), ed_rline_cb);

  $my(win) = Ed.get.current_win ($my(ed));
  $my(buf) = Win.buf.new ($my(win), BufOpts(
      .fname = STR_FMT
         ("%s/vwm_unamed", E.get.env ($my(__E__), "data_dir")->bytes)));

  Buf.set.autochdir ($my(buf), 0);
  Buf.set.on_emptyline ($my(buf), "");
  Buf.set.show_statusline ($my(buf), 0);

  Win.append_buf ($my(win), $my(buf));
  Win.set.current_buf ($my(win), 0, DONOT_DRAW);

  $my(video) = Ed.get.video ($my(ed));
  $my(topline) = Ed.get.topline ($my(ed));

  String.clear ($my(topline));
  Video.set.row_with ($my(video), 0, $my(topline)->bytes);

  $my(orig_topline) = Ed.set.topline;
  Ed.set.topline = ed_set_topline_void;

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
    .set = (vwmed_set_self) {
      .object = vwmed_set_object,
      .info_cb = vwmed_set_info_cb,
      .rline_cb = vwmed_set_rline_cb,
      .rline_command_cb = vwmed_set_rline_command_cb
    },
    .init = (vwmed_init_self) {
      .ved = vwmed_init_ved,
      .term = vwmed_init_term
    }
  };

  $my(__This__) = __This__;
  $my(__E__)    = $my(__This__)->__E__;

  $my(state) = 0;
  $my(edit_file_cb) = vwmed_edit_file_cb;
  $my(num_rline_cbs) = 0;
  $my(rline_cbs) = NULL;
  $my(num_rline_command_cbs) = 0;
  $my(rline_command_cbs) = NULL;
  $my(num_info_cbs) = 0;
  $my(info_cbs) = NULL;

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

  if ($my(num_rline_cbs))
    free ($my(rline_cbs));

  if ($my(num_rline_command_cbs))
    free ($my(rline_command_cbs));

  if ($my(num_info_cbs))
    free ($my(info_cbs));

  free (this->prop);
  free (this);
  *thisp = NULL;
}
