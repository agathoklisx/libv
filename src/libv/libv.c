/* This is a library, that links against libvwm, libvwmed and libvtach,
 * and abstracts the details.
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <termios.h>
#include <errno.h>

#include <libv/libv.h>
#include <libv/libvci.h>

#define self(__f__, ...) this->self.__f__ (this, ##__VA_ARGS__)
#define $my(__p__) this->prop->__p__

#define Vwm    vwm->self
#define Vwin   vwm->win
#define Vframe vwm->frame
#define Vterm  vwm->term
#define Vwmed  vwmed->self
#define Vtach  vtach->self

#define V_NUM_OBJECTS NUM_OBJECTS + 2
#define E_OBJECT V_NUM_OBJECTS - 2
#define I_OBJECT V_NUM_OBJECTS - 1

struct v_prop {
  char
    *image_name,
    *image_file;

  int
    input_fd,
    save_image,
    always_connect;

  string_t
    *data_dir,
    *as_sockname;

  v_opts *opts;
  struct termios orig_mode;
  void *objects[V_NUM_OBJECTS];
};

static const char *const arg_parse_usage[] = {
  "v -s,--sockname= [options] [command] [command arguments]",
  NULL,
};

static const char usage[] =
  "v -s,--sockname= [options] [command] [command arguments]\n"
  "\n"
  "Options:\n"
  "    -s, --sockname=     set the socket name [required if --as= missing]\n"
  "        --as=           create the socket name in an inner environment [required if -s is missing]\n"
  "    -a, --attach        attach to the specified socket\n"
  "    -f, --force         connect to socket, even when socket exists\n"
  "        --send          send data to the specified socket from standard input\n"
  "        --exit          create the socket, fork and then exit\n"
  "        --remove-socket remove socket if exists and can not be connected\n"
  "        --loadfile=     load file for evaluation\n"
  "\n";

private char *v_get_sockname (v_t *this) {
  ifnot (NULL is $my(opts)->sockname)
    return $my(opts)->sockname;
  ifnot (NULL is $my(as_sockname))
    return $my(as_sockname)->bytes;
  return NULL;
}

private void *v_get_object (v_t *this, int idx) {
  if (idx >= V_NUM_OBJECTS or idx < 0) return NULL;
  return $my(objects)[idx];
}

private void v_set_object (v_t *this, void *object, int idx) {
  if (idx >= V_NUM_OBJECTS or idx < 0) return;
  $my(objects)[idx] = object;
}

private int v_set_data_dir (v_t *this, char *dir) {
  if (NULL is $my(data_dir))
    $my(data_dir) = String.new (32);

  String.clear ($my(data_dir));

  if (NULL is dir) {
    E_T *__E = $my(objects)[E_OBJECT];
    String.append ($my(data_dir), E.get.env (__E, "data_dir")->bytes);
    String.append ($my(data_dir), "/v");
  } else
    String.append ($my(data_dir), dir);

  ifnot (File.exists ($my(data_dir)->bytes)) {
    if (-1 is mkdir ($my(data_dir)->bytes, S_IRWXU))
      goto theerror;
  } else {
    ifnot (Dir.is_directory ($my(data_dir)->bytes))
      goto theerror;

    ifnot (File.is_rwx ($my(data_dir)->bytes))
      goto theerror;
  }

  return OK;

theerror:
  self(unset.data_dir);
  return NOTOK;
}

private int v_set_i_dir (v_t *this, char *dir) {
  if (NULL is dir)
    if (NULL is $my(data_dir) or $my(data_dir)->num_bytes is 0)
      return NOTOK;

  char scripts[] = "scripts";

  size_t len = 1 + bytelen (scripts);

  if (NULL is dir) {
    dir = $my(data_dir)->bytes;
    len += $my(data_dir)->num_bytes;
  } else
    len += bytelen (dir);

  ifnot (Dir.is_directory (dir)) return NOTOK;

  char script_dir[len + 1];
  Cstring.cp_fmt (script_dir, len + 1, "%s/scripts", dir);

  ifnot (File.exists (script_dir)) {
    if (-1 is mkdir (script_dir, S_IRWXU))
      return NOTOK;
  } else {
    ifnot (Dir.is_directory (script_dir))
      return NOTOK;

    ifnot (File.is_rwx (script_dir))
      return NOTOK;
  }

  E.set.i_dir ($my(objects)[E_OBJECT], dir);
  return OK;
}

private void v_set_image_file (v_t *this, char *name) {
  if (NULL is name) return;

  ifnot (NULL is $my(image_file))
    free ($my(image_file));

  char *cwd = NULL;

  size_t len = bytelen (name);

  char *extname = Path.extname (name);
  size_t exlen = bytelen (extname);

  int hasnot_ext = (0 is exlen or (exlen and 0 is Cstring.eq (extname, ".i")));

  if (hasnot_ext) len += 2;

  ifnot (Path.is_absolute (name)) {
    cwd = Dir.current ();
    if (NULL is cwd) return;
    len += bytelen (cwd) + 1;
  }

  $my(image_file) = Alloc (len + 1);

  ifnot (Path.is_absolute (name))
    Cstring.cp_fmt ($my(image_file), len + 1, "%s/%s", cwd, name);
  else
    Cstring.cp ($my(image_file), len + 1, name, len - (hasnot_ext ? 2 : 0));

  if (hasnot_ext)
    Cstring.cat ($my(image_file), len + 1, ".i");
}

private void v_set_image_name (v_t *this, char *name) {
  if (NULL is name) return;

  ifnot (NULL is $my(image_name))
    free ($my(image_name));

  $my(image_name) = Cstring.dup (name, bytelen (name));
}

private void v_set_save_image (v_t *this, int val) {
  $my(save_image) = val;
}

private void v_unset_data_dir (v_t *this) {
  String.free ($my(data_dir));
  $my(data_dir) = NULL;
}

private int v_save_image (v_t *this, char *fname) {
  if (NULL is fname and NULL is $my(image_file) and NULL is $my(image_name))
    return NOTOK;

  string_t *file = NULL;
  if (NULL is fname) {
    if (NULL isnot $my(image_name)) {
      E_T *__E = $my(objects)[E_OBJECT];
      file = String.new_with (E.get.env (__E, "i_dir")->bytes);
      String.append (file, "/scripts/");
      String.append (file, $my(image_name));
      if (file->bytes[file->num_bytes - 1] isnot 'i' and
          file->bytes[file->num_bytes - 2] isnot '.')
        String.append (file, ".i");

      fname = file->bytes;
    } else
      fname = $my(image_file);
  }

  vwm_t *vwm = $my(objects)[VWM_OBJECT];

  FILE *fp = fopen (fname, "w");
  if (NULL is fp) return NOTOK;

  int cur_win_idx = Vwm.get.current_win_idx (vwm);

  fprintf (fp,
    "# v image script\n\n"
    "# variable initialization\n"
    "var NULL = 0\n"
    "var v = v_get ()\n"
    "var num_frames = 0\n"
    "var max_frames = 0\n"
    "var force = 0\n"
    "var log = 0\n"
    "var remove_log = 1\n"
    "var win = NULL\n"
    "var frame = NULL\n"
    "var cur_win_idx = 0\n"
    "var cur_frame_idx = 0\n"
    "var visibility = 0\n"
    "var num_visible_frames = 0\n"
    "\n"
    "var save_image = %d\n\n"
    "var force = %d\n"
    "if (force) {\n"
    "  v_set_opt_force (v, 1)\n"
    "}\n"
    "\n"
    "v_set_sockname (v, \"%s\")\n"
    "v_set_raw_mode (v)\n"
    "v_set_size (v)\n"
    "\n"
    "var rows = v_get_rows (v)\n"
    "var cols = v_get_cols (v)\n"
    "var num_win = %d\n"
    "\n"
    "cur_win_idx = %d\n",
    $my(always_connect),
    $my(save_image),
    self(get.sockname),
    Vwm.get.num_wins (vwm),
    cur_win_idx);

  int num_wins = Vwm.get.num_wins (vwm);

  for (int i = 0; i < num_wins; i++) {
    vwm_win *win = Vwm.get.win_at (vwm, i);
    int num_frames = Vwin.get.num_frames (win);
    int max_frames = Vwin.get.max_frames (win);
    int num_visible_frames = Vwin.get.num_visible_frames (win);
    int cur_frame_idx = Vwin.get.current_frame_idx (win);
    char *name = Vwin.get.name (win);

    fprintf (fp,
      "\n"
      "# %s\n"
      "num_frames = %d\n"
      "max_frames = %d\n"
      "num_visible_frames = %d\n"
      "cur_frame_idx = %d\n"
      "win = v_new_win (v, num_frames, max_frames)\n",
      name,
      num_frames,
      max_frames,
      num_visible_frames,
      cur_frame_idx);

    for (int f = 0; f < num_frames; f++) {
      vwm_frame *frame = Vwin.get.frame_at (win, f);
      char *logfile =  Vframe.get.logfile (frame);
      char **argv = Vframe.get.argv (frame);
      int argc = Vframe.get.argc (frame);
      int visibility = Vframe.get.visibility (frame);

      fprintf (fp,
          "frame = v_win_get_frame_at (v, win, %d)\n"
          "v_set_frame_visibility (v, frame, %d)\n",
          f, visibility);

      fprintf (fp, "v_set_frame_command (v, frame, \"");

      for (int j = 0; j < argc; j++)
        fprintf (fp, "%s%s", argv[j], (j is argc - 1 ? "" : " "));

      fprintf (fp, "\")\n");

      ifnot (NULL is logfile) {
        int remove_log = Vframe.get.remove_log (frame);
        fprintf (fp, "v_set_frame_log (v, frame, \"%s\", %d)\n",
           logfile, remove_log);
      }

      fprintf (fp, "\n");
    }

    fprintf (fp, "v_win_set_current_at (v, win, %d)\n", cur_frame_idx);
  }

  ifnot (NULL is $my(image_name))
    fprintf (fp, "v_set_image_name (v, \"%s\")\n", $my(image_name));

  ifnot (NULL is $my(image_file))
    fprintf (fp, "v_set_image_file (v, \"%s\")\n", $my(image_file));

  fprintf (fp, "v_set_save_image (v, %d)\n", $my(save_image));

  fprintf (fp, "\n");

  fprintf (fp, "v_set_current_at (v, %d)\n", cur_win_idx);
  fprintf (fp, "v_main (v)\n");

  fclose (fp);

  String.free (file);

  return OK;
}

private int v_rline_cb (vwmed_t *vwmed, rline_t *rl, vwm_t *vwm, vwm_win *win, vwm_frame *frame) {
  (void) vwm; (void) win;

  v_t *this = Vwmed.get.object (vwmed, V_OBJECT);

  int retval = RLINE_NO_COMMAND;
  string_t *com = Rline.get.command (rl);

  if (Cstring.eq (com->bytes, "set")) {
    string_t *arg = Rline.get.anytype_arg (rl, "log-file");
    ifnot (NULL is arg) {
      int set_log = atoi (arg->bytes);
      if (set_log)
        Vframe.set.log (frame, NULL, 1);
      else
        Vframe.release_log (frame);
    }

    arg = Rline.get.anytype_arg (rl, "save-image");
    ifnot (NULL is arg)
      self(set.save_image, atoi (arg->bytes));

    arg = Rline.get.anytype_arg (rl, "image-file");
    ifnot (NULL is arg)
      self(set.image_file, arg->bytes);

    arg = Rline.get.anytype_arg (rl, "image-name");
    ifnot (NULL is arg)
      self(set.image_name, arg->bytes);

    arg = Rline.get.anytype_arg (rl, "always-connect");
    ifnot (NULL is arg)
      $my(always_connect) = atoi (arg->bytes);

    retval = OK;
    goto theend;
  } else if (Cstring.eq (com->bytes, "@save_image")) {
    char *fname = NULL;
    string_t *fn_arg = Rline.get.anytype_arg (rl, "as");

    ifnot (NULL is fn_arg) fname = fn_arg->bytes;

    retval = self(save_image, fname);

    goto theend;
  }

theend:
  String.free (com);
  return retval;
}

private void v_rline_command_cbs (ed_t *ed) {
  Ed.append.command_arg (ed, "set", "--save-image=", 13);
  Ed.append.command_arg (ed, "set", "--image-name=", 13);
  Ed.append.command_arg (ed, "set", "--image-file=", 13);
  Ed.append.command_arg (ed, "set", "--always-connect=", 17);

  Ed.append.rline_command (ed, "@save_image", 0, 0);
  Ed.append.command_arg   (ed, "@save_image", "--as=", 5);
}

private int v_info_cb (vwmed_t *vwmed, vwm_t *vwm, FILE *fp) {
  (void) vwm;

  v_t *this = Vwmed.get.object (vwmed, V_OBJECT);

  fprintf (fp,
      "\n"
      "--= V info =--\n"
      "Save image         : %d\n"
      "Image name         : %s\n"
      "Image file         : %s\n"
      "Socket Name        : %s\n",
      $my(save_image),
      $my(image_name),
      $my(image_file),
      self(get.sockname));

  fflush (fp);
  return OK;
}

private int v_exec_child (vtach_t *vtach, int argc, char **argv) {
  (void) argc; (void) argv;
  v_t *this = Vtach.get.object (vtach, V_OBJECT);

  vwm_t *vwm = Vtach.get.object (vtach, VWM_OBJECT);
  vwmed_t *vwmed = Vwm.get.object (vwm, VWMED_OBJECT);

  int retval = Vwm.main (vwm);

  if ($my(save_image)) self(save_image, NULL);

  __deinit_vwmed__ (&vwmed);
  return retval;
}

private int v_pty_main (vtach_t *vtach, int argc, char **argv) {
  vwm_t *vwm = Vtach.get.object (vtach, VWM_OBJECT);

  int rows = Vwm.get.lines (vwm);
  int cols = Vwm.get.columns (vwm);

  win_opts w_opts = WinOpts (
      .num_rows = rows,
      .num_cols = cols,
      .num_frames = 1,
      .max_frames = 2);

  vwm_win *win = Vwm.new.win (vwm, NULL, w_opts);
  vwm_frame *frame = Vwin.get.frame_at (win, 0);
  Vframe.set.argv (frame, argc, argv);
  Vframe.set.log (frame, NULL, 1);
  Vframe.create_fd (frame);

  return OK;
}

private string_t *v_make_sockname (v_t *this, char *sockdir, char *as) {
  if (NULL is as) return NULL;

  ifnot (NULL is Cstring.byte.in_str (as, '\\')) {
    fprintf (stderr, "`as' argument includes a slash\n");
    return NULL;
  }

  size_t aslen = bytelen (as);

  string_t *sockname = String.new (aslen + 16);

  if (NULL is sockdir) {
    string_t *tmp = E.get.env ($my(objects)[E_OBJECT], "tmp_dir");
    String.append_with_len (sockname, tmp->bytes, tmp->num_bytes);

    tmp = E.get.env ($my(objects)[E_OBJECT], "user_name");
    String.append_fmt (sockname, "/%s_vsockets", tmp->bytes);

    if (File.exists (sockname->bytes)) {
      ifnot (Dir.is_directory (sockname->bytes)) {
        fprintf (stderr, "%s: not a directory\n", sockname->bytes);
        goto theerror;
      }

      ifnot (File.is_rwx (sockname->bytes)) {
        fprintf (stderr, "%s: insufficient permissions\n", sockname->bytes);
        goto theerror;
      }

    } else {
      if (-1 is mkdir (sockname->bytes, S_IRWXU)) {
        fprintf (stderr, "%s: can not make directory\n", sockname->bytes);
        fprintf (stderr, "%s\n", strerror (errno));
        goto theerror;
      }
    }

    String.append_fmt (sockname, "/%s", as);
  } else {
    size_t dirlen = bytelen (sockdir);
    while (sockdir[dirlen - 1] is '/') {
      dirlen--;
      sockdir[dirlen] = '\0';
    }

    String.append_fmt (sockname, "%s/%s", sockdir, as);
  }

  struct sockaddr_un sockun;
  if (sockname->num_bytes > sizeof (sockun.sun_path) - 1) {
    fprintf (stderr, "socket name `%s' exceeds %zd limit\n", sockname->bytes, sizeof (sockun.sun_path));
    goto theerror;
  }

  return sockname;

theerror:
  String.free (sockname);
  return NULL;
}

private int v_send (v_t *this, char *sockname, char *data) {
  vtach_t *vtach = $my(objects)[VTACH_OBJECT];
  int s = Vtach.sock.connect (vtach, sockname);
  if (s is NOTOK) return 1;

  size_t max_size = Vtach.get.sock_max_data_size (vtach);
  char buf[max_size];
  int retval = 0;

  if (data isnot NULL) {
    size_t len = bytelen (data);
    int num = len;
    char *sp = data;

    while (num > 0) {
      size_t n = 0;
      for (;n < max_size and n < len; n++)
        buf[n] = *sp++;

      if (NOTOK is Vtach.sock.send_data (vtach, s, buf, n, MSG_PUSH)) {
        retval = NOTOK;
        goto theend;
      }

      len -= n;
      num -= n;
    }

    goto theend;
  }

  if ($my(input_fd) is 0) { // read from the pipe
    retval = 1;
    goto theend;
  }

  for (;;) {
    ssize_t len = read ($my(input_fd), buf, max_size);
    if (0 is len) goto theend;
    if (len < 0) {
      retval = 1;
      fprintf (stderr, "error while reading from stdin\n");
      goto theend;
    }

    retval = Vtach.sock.send_data (vtach, s, buf, len, MSG_PUSH);
  }

theend:
  close (s);
  return retval;
}

private ival_t i_v_get (i_t *__i) {
  return (ival_t) I.get.object (__i);
}

private ival_t i_v_get_vwm (i_t *__i, v_t *this) {
  (void) __i;
  return (ival_t) $my(objects)[VWM_OBJECT];
}

private ival_t i_v_get_term (i_t *__i, v_t *this) {
  (void) __i;
  vwm_t *vwm = $my(objects)[VWM_OBJECT];
  return (ival_t) Vwm.get.term (vwm);
}

private ival_t i_v_get_rows (i_t *__i, v_t *this) {
  (void) __i;
  vwm_t *vwm = $my(objects)[VWM_OBJECT];
  return Vwm.get.lines (vwm);
}

private ival_t i_v_get_cols (i_t *__i, v_t *this) {
  (void) __i;
  vwm_t *vwm = $my(objects)[VWM_OBJECT];
  return Vwm.get.columns (vwm);
}

private ival_t i_v_set_opt_force (i_t *__i, v_t *this, int val) {
  (void) __i;
  $my(opts)->force = val;
  return I_OK;
}

private ival_t i_v_win_get_frame_at (i_t *__i, v_t *this, vwm_win *win, int idx) {
  (void) __i;
  vwm_t *vwm = $my(objects)[VWM_OBJECT];
  return (ival_t)  Vwin.get.frame_at (win, idx);
}

private ival_t i_v_win_set_current_at (i_t *__i, v_t *this, vwm_win *win, int idx) {
  (void) __i;
  vwm_t *vwm = $my(objects)[VWM_OBJECT];
  return (ival_t)  Vwin.set.current_at (win, idx);
}

private ival_t i_v_set_raw_mode (i_t *__i, v_t *this) {
  (void) __i;
  vwm_t *vwm = $my(objects)[VWM_OBJECT];
  Vterm.raw_mode (Vwm.get.term (vwm));
  return I_OK;
}

private ival_t i_v_set_size (i_t *__i, v_t *this) {
  (void) __i;
  vwm_t *vwm = $my(objects)[VWM_OBJECT];
  int rows, cols;
  Vterm.init_size (Vwm.get.term (vwm), &rows, &cols);
  Vwm.set.size (vwm, rows, cols, 1);
  return I_OK;
}

private ival_t i_v_set_sockname (i_t *__i, v_t *this, char *sockname) {
  (void) __i;
  $my(as_sockname) = String.new_with (sockname);
  free (sockname);
  return I_OK;
}

private ival_t i_v_set_frame_command (i_t *__i, v_t *this, vwm_frame *frame, char *command) {
  (void) __i;
  vwm_t *vwm = $my(objects)[VWM_OBJECT];
  Vframe.set.command (frame, command);
  free (command);
  return I_OK;
}

private ival_t i_v_set_frame_visibility (i_t *__i, v_t *this, vwm_frame *frame, int visibility) {
  (void) __i;
  vwm_t *vwm = $my(objects)[VWM_OBJECT];
  Vframe.set.visibility (frame, visibility);
  return I_OK;
}

private ival_t i_v_set_frame_log (i_t *__i, v_t *this, vwm_frame *frame, char *fname, int val) {
  (void) __i;
  vwm_t *vwm = $my(objects)[VWM_OBJECT];
  Vframe.set.log (frame, fname, val);
  free (fname);
  return I_OK;
}

private ival_t i_v_set_image_file (i_t *__i, v_t *this, char *fn) {
  (void) __i;
  self(set.image_file, fn);
  free (fn);
  return I_OK;
}

private ival_t i_v_set_image_name (i_t *__i, v_t *this, char *name) {
  (void) __i;
  self(set.image_name, name);
  free (name);
  return I_OK;
}

private ival_t i_v_set_save_image (i_t *__i, v_t *this, int val) {
  (void) __i;
  self(set.save_image, val);
  return I_OK;
}

private ival_t i_v_set_current_at (i_t *__i, v_t *this, int idx) {
  (void) __i;
  vwm_t *vwm = $my(objects)[VWM_OBJECT];
  Vwm.set.current_at (vwm, idx);
  return I_OK;
}

private ival_t i_v_new_win (i_t *__i, v_t *this, int num_frames, int max_frames) {
  (void) __i;
  vwm_t *vwm = $my(objects)[VWM_OBJECT];
  int rows = Vwm.get.lines (vwm);
  int cols = Vwm.get.columns (vwm);

  vwm_win *win = Vwm.new.win (vwm, NULL, WinOpts (
    .num_rows = rows,
    .num_cols = cols,
    .num_frames = num_frames,
    .max_frames = max_frames));

  return (ival_t) win;
}

private int i_v_pty_main (vtach_t *vtach, int argc, char **argv) {
  (void) vtach; (void) argc; (void) argv;
  return OK;
}

private ival_t i_v_main (i_t *__i, v_t *this) {
  (void) __i;
  if (NULL is $my(as_sockname))
    return I_NOTOK;

  char *sockname = $my(as_sockname)->bytes;

  if (NULL is sockname) return I_NOTOK;

  vtach_t *vtach = $my(objects)[VTACH_OBJECT];

  v_opts *opts = $my(opts);
  (void) opts;

  int attach = 0;
  if (File.exists (sockname)) {
    ifnot (File.is_sock (sockname)) {
      fprintf (stderr, "%s: is not a socket\n", sockname);
      return I_NOTOK;
    }

    int fd = Vtach.sock.connect (vtach, sockname);
    if (NOTOK is fd) {
      if (opts->force) {
        if (-1 is unlink (sockname)) {
          fprintf (stderr,
              "socket %s exists, and cannot be removed %s\n",
               sockname, strerror (errno));
          return I_NOTOK;
        }
      } else {
        fprintf (stderr,
            "socket %s exists but can not connect/attach\n", sockname);
        return I_NOTOK;
      }
    } else
      close (fd);

    attach = 1;
  }

  if (NOTOK is Vtach.init.pty (vtach, sockname))
    return I_NOTOK;

  ifnot (attach) {
    vwmed_t *vwmed = $my(objects)[VWMED_OBJECT];
    Vwmed.init.ved (vwmed);

    Vtach.set.object (vtach, vwmed, VWMED_OBJECT);
    Vtach.set.exec_child_cb (vtach, v_exec_child);
    Vtach.set.pty_main_cb (vtach, i_v_pty_main);

    Vtach.pty.main (vtach, 0, NULL);
  }

  int retval = Vtach.tty.main (vtach);
  if (retval isnot OK)
    return I_NOTOK;

  return I_OK;
}

struct vfun_t {
  const char *name;
  ival_t val;
  int nargs;
} vfuns[] = {
  { "v_get",                  (ival_t) i_v_get, 0},
  { "v_get_vwm",              (ival_t) i_v_get_vwm, 1},
  { "v_get_term",             (ival_t) i_v_get_term, 1},
  { "v_get_rows",             (ival_t) i_v_get_rows, 1},
  { "v_get_cols",             (ival_t) i_v_get_cols, 1},
  { "v_set_size",             (ival_t) i_v_set_size, 1},
  { "v_set_sockname",         (ival_t) i_v_set_sockname, 2},
  { "v_set_raw_mode",         (ival_t) i_v_set_raw_mode, 1},
  { "v_set_frame_log",        (ival_t) i_v_set_frame_log, 4},
  { "v_set_save_image",       (ival_t) i_v_set_save_image, 2},
  { "v_set_image_name",       (ival_t) i_v_set_image_name, 2},
  { "v_set_image_file",       (ival_t) i_v_set_image_file, 2},
  { "v_set_current_at",       (ival_t) i_v_set_current_at, 2},
  { "v_set_opt_force",        (ival_t) i_v_set_opt_force, 2},
  { "v_set_frame_command",    (ival_t) i_v_set_frame_command, 3},
  { "v_set_frame_visibility", (ival_t) i_v_set_frame_visibility, 3},
  { "v_win_get_frame_at",     (ival_t) i_v_win_get_frame_at, 3},
  { "v_win_set_current_at",   (ival_t) i_v_win_set_current_at, 3},
  { "v_new_win",              (ival_t) i_v_new_win, 3},
  { "v_main",                 (ival_t) i_v_main, 1},
  { NULL, 0, 0}
};

private int v_i_define_funs_cb (i_t *this) {
  int err;
  for (int i = 0; vfuns[i].name; i++) {
    if (I_OK isnot (err = I.def (this, vfuns[i].name, I_CFUNC (vfuns[i].nargs), vfuns[i].val)))
      return err;
  }

  return I_OK;
}

private i_t *v_init_i (v_t *this) {
  ifnot (NULL is $my(objects)[I_OBJECT])
    return $my(objects)[I_OBJECT];

  E_T *__E = $my(objects)[E_OBJECT];
  i_T *__I = E.get.iclass (__E);
  i_t *__i = I.init_instance (__I, IOpts(
    .define_funs_cb = v_i_define_funs_cb,
    .object = this));

  $my(objects)[I_OBJECT] = __i;
  return __i;
}

private int v_loadfile (v_t *this, char *fn) {
  E_T *__E = $my(objects)[E_OBJECT];
  i_T *__I = E.get.iclass (__E);
  i_t *__i = v_init_i (this);
  int retval = I.load_file (__I, __i, fn);
  return retval;
}

private int v_main (v_t *this) {
  vtach_t *vtach = $my(objects)[VTACH_OBJECT];

  v_opts *opts = $my(opts);

  int argc = opts->argc;
  char *sockname = opts->sockname;
  char *loadfile = opts->loadfile;
  char **argv = opts->argv;
  char *as = opts->as;
  char *data = opts->data;
  VExecChild vexec_child = (opts->at_exec_child is NULL ? v_exec_child : opts->at_exec_child);
  VPtyMain vpty_main = (opts->at_pty_main is NULL ? v_pty_main : opts->at_pty_main);

  if ($my(opts)->parse_argv) {
    argparse_option_t options[] = {
      OPT_HELP (),
      OPT_GROUP("Options:"),
      OPT_STRING(0, "as", &as, "create the socket name in an inner environment [required if -s is missing]", NULL, 0, 0),
      OPT_STRING('s', "sockname", &sockname, "set the socket name [required if --as= missing]", NULL, 0, 0),
      OPT_STRING(0, "loadfile", &loadfile, "load file for evaluation", NULL, 0, 0), 
      OPT_BOOLEAN('a', "attach", &opts->attach, "attach to the specified socket", NULL, 0, 0),
      OPT_BOOLEAN(0, "force", &opts->force, "connect to socket, even when socket exists", NULL, 0, 0),
      OPT_BOOLEAN(0, "send", &opts->send_data, "send data to the specified socket", NULL, 0, 0),
      OPT_BOOLEAN(0, "exit", &opts->exit, "create the socket, fork and then exit", NULL, 0, 0),
      OPT_BOOLEAN(0, "remove-socket", &opts->remove_socket, "remove socket if exists and can not be connected", NULL, 0, 0),
      OPT_END()
    };

    argparse_t argparser;
    Argparse.init (&argparser, options, arg_parse_usage, ARGPARSE_DONOT_EXIT_ON_UNKNOWN);
    argc = Argparse.exec (&argparser, argc, (const char **) argv);
  }

  if (argc is -1) return 0;

  ifnot (NULL is loadfile)
    return v_loadfile (this, loadfile);

  if (NULL is sockname) {
    if (NULL is as) {
      fprintf (stderr, "required socket name hasn't been specified\n");
      return 1;
    }

    $my(as_sockname) = v_make_sockname (this, NULL, as);
    if (NULL is $my(as_sockname))
      return 1;

    sockname = $my(as_sockname)->bytes;
  }

  if (opts->exit_on_no_command) {
    if (argc is 0 or argv is NULL) {
      if ((0 is opts->attach and 0 is opts->send_data)) {
        fprintf (stderr, "command hasn't been set\n");
        fprintf (stderr, "%s", usage);
        return 1;
      }
    }
  }

  if (File.exists (sockname)) {
    if (0 is opts->attach and 0 is opts->send_data) {
      ifnot (opts->force) {
        ifnot (opts->remove_socket) {
          fprintf (stderr, "%s: exists in the filesystem\n", sockname);
          return 1;
        }
      }
    }

    ifnot (File.is_sock (sockname)) {
      fprintf (stderr, "%s: is not a socket\n", sockname);
      return 1;
    }

    int fd = Vtach.sock.connect (vtach, sockname);
    if (0 is opts->attach and 0 is opts->send_data)
      if (opts->remove_socket)
        unlink (sockname);

    if (NOTOK is fd) {
      if (opts->attach or opts->send_data) {
        if (opts->remove_socket)
          unlink (sockname);
        fprintf (stderr, "can not connect/attach to the socket\n");
        return 1;
      }
    } else
      close (fd);
  }

  if (0 is opts->send_data or (opts->send_data and data isnot NULL)) {
    if (0 is isatty (fileno (stdin))) {
      fprintf (stderr, "Not a controlled terminal\n");
      return 1;
    }
  }

  if (opts->send_data)
    return self(send, sockname, data);

  if (NOTOK is Vtach.init.pty (vtach, sockname))
    return 1;

  ifnot (opts->attach) {
    vwmed_t *vwmed = $my(objects)[VWMED_OBJECT];
    Vwmed.init.ved (vwmed);

    Vtach.set.object (vtach, vwmed, VWMED_OBJECT);
    Vtach.set.exec_child_cb (vtach, vexec_child);
    Vtach.set.pty_main_cb (vtach, vpty_main);

    Vtach.pty.main (vtach, argc, argv);
  }

  if (opts->exit)
    return 0;

  return Vtach.tty.main (vtach);
}

public v_t *__init_v__ (vwm_t *vwm, v_opts *opts) {
  v_t *this = Alloc (sizeof (v_t));
  this->prop = Alloc (sizeof (v_prop));

  this->self = (v_self) {
    .main = v_main,
    .send = v_send,
    .save_image = v_save_image,
    .get = (v_get_self) {
      .sockname = v_get_sockname,
      .object = v_get_object
    },
    .set = (v_set_self) {
      .i_dir = v_set_i_dir,
      .object = v_set_object,
      .data_dir = v_set_data_dir,
      .save_image = v_set_save_image,
      .image_file = v_set_image_file,
      .image_name = v_set_image_name
    },
    .unset = (v_unset_self) {
      .data_dir = v_unset_data_dir
    }
  };

  struct termios orig_mode;
  if (-1 isnot tcgetattr (0, &orig_mode)) { // dont exit yes, as we might be at the end of a pipe
    $my(orig_mode) = orig_mode;
    $my(input_fd) = 0;
  }

  $my(opts) = opts;
  $my(image_file) = NULL;
  $my(image_name) = NULL;
  $my(as_sockname) = NULL;
  $my(data_dir) = NULL;
  $my(save_image) = 0;
  $my(always_connect) = 0;

  if (NULL is vwm)
    if (NULL is (vwm = __init_vwm__ ())) {
      __deinit_v__ (&this);
      return NULL;
    }

  vtach_t *vtach;
  if (NULL is (vtach = __init_vtach__ (vwm))) {
    __deinit_v__ (&this);
    return NULL;
  }

  vwmed_t *vwmed;
  if (NULL is (vwmed = __init_vwmed__ (vwm))) {
    ifnot (NULL is $my(opts)->argv) {
      for (int i = 0; i < $my(opts)->argc; i++) {
        if (0 is strcmp ($my(opts)->argv[i], "--send")) {
          ifnot (isatty (fileno (stdin))) {
            $my(input_fd) = dup (fileno (stdin));
            freopen ("/dev/tty", "r", stdin);
            break;
          }
        }
      }
    }

    vwmed = __init_vwmed__ (vwm);
    if (NULL is vwmed) {
      __deinit_v__ (&this);
      return NULL;
    }

    if (-1 isnot tcgetattr ($my(input_fd), &orig_mode))
      $my(orig_mode) = orig_mode;
  }

  $my(objects)[VTACH_OBJECT] = vtach;
  $my(objects)[VWMED_OBJECT] = vwmed;
  $my(objects)[VWM_OBJECT] = vwm;
  $my(objects)[E_OBJECT] = Vwmed.get.e (vwmed);
  $my(objects)[I_OBJECT] = NULL;

  Vwm.set.object (vwm, this, V_OBJECT);

  Vtach.set.object (vtach, this, V_OBJECT);
  Vtach.set.object (vtach, vwmed, VWMED_OBJECT);

  Vwmed.set.object (vwmed, this, V_OBJECT);
  Vwmed.set.rline_cb (vwmed, v_rline_cb);
  Vwmed.set.rline_command_cb (vwmed, v_rline_command_cbs);
  Vwmed.set.info_cb (vwmed, v_info_cb);

  self(set.data_dir, NULL);
  self(set.i_dir, NULL);

  return this;
}

public void __deinit_v__ (v_t **thisp) {
  if (NULL is *thisp) return;

  v_t *this = *thisp;

  String.free ($my(as_sockname));
  self(unset.data_dir);

  ifnot (NULL is $my(image_file)) free ($my(image_file));
  ifnot (NULL is $my(image_name)) free ($my(image_name));

  vtach_t *vtach = $my(objects)[VTACH_OBJECT];
  vwmed_t *vwmed = $my(objects)[VWMED_OBJECT];
  vwm_t   *vwm   = $my(objects)[VWM_OBJECT];

  __deinit_vtach__ (&vtach);
  __deinit_vwmed__ (&vwmed);
  __deinit_vwm__   (&vwm);

  tcsetattr ($my(input_fd), TCSAFLUSH, &$my(orig_mode));

  free (this->prop);
  free (this);
  *thisp = NULL;
}
