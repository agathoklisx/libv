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
#define Vwmed  vwmed->self
#define Vtach  vtach->self

#define V_NUM_OBJECTS NUM_OBJECTS + 1
#define E_OBJECT V_NUM_OBJECTS - 1

struct v_prop {
  int input_fd;
  v_opts *opts;
  string_t *as_sockname;
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

private int v_exec_child (vtach_t *vtach, int argc, char **argv) {
  (void) argc; (void) argv;
  vwm_t *vwm = Vtach.get.object (vtach, VWM_OBJECT);
  vwmed_t *vwmed = Vwm.get.object (vwm, VWMED_OBJECT);

  int retval = Vwm.main (vwm);

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

private int v_main (v_t *this) {
  vtach_t *vtach = $my(objects)[VTACH_OBJECT];

  v_opts *opts = $my(opts);

  int argc = opts->argc;
  int force = opts->force;
  int attach = opts->attach;
  int exit_this = opts->exit;
  int send_data = opts->send_data;
  int remove_socket = opts->remove_socket;
  int exit_on_no_command = opts->exit_on_no_command;
  char *sockname = opts->sockname;
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
      OPT_BOOLEAN('a', "attach", &attach, "attach to the specified socket", NULL, 0, 0),
      OPT_BOOLEAN(0, "force", &force, "connect to socket, even when socket exists", NULL, 0, 0),
      OPT_BOOLEAN(0, "send", &send_data, "send data to the specified socket", NULL, 0, 0),
      OPT_BOOLEAN(0, "exit", &exit_this, "create the socket, fork and then exit", NULL, 0, 0),
      OPT_BOOLEAN(0, "remove-socket", &remove_socket, "remove socket if exists and can not be connected", NULL, 0, 0),
      OPT_END()
    };

    argparse_t argparser;
    Argparse.init (&argparser, options, arg_parse_usage, ARGPARSE_DONOT_EXIT_ON_UNKNOWN);
    argc = Argparse.exec (&argparser, argc, (const char **) argv);
  }

  if (argc is -1) return 0;

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

  if (exit_on_no_command) {
    if (argc is 0 or argv is NULL) {
      if ((0 is attach and 0 is send_data)) {
        fprintf (stderr, "command hasn't been set\n");
        fprintf (stderr, "%s", usage);
        return 1;
      }
    }
  }

  if (File.exists (sockname)) {
    if (0 is attach and 0 is send_data) {
      ifnot (force) {
        ifnot (remove_socket) {
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
    if (0 is attach and 0 is send_data)
      if (remove_socket)
        unlink (sockname);

    if (NOTOK is fd) {
      if (attach or send_data) {
        if (remove_socket)
          unlink (sockname);
        fprintf (stderr, "can not connect/attach to the socket\n");
        return 1;
      }
    } else
      close (fd);
  }

  if (0 is send_data or (send_data and data isnot NULL)) {
    if (0 is isatty (fileno (stdin))) {
      fprintf (stderr, "Not a controlled terminal\n");
      return 1;
    }
  }

  if (send_data)
    return self(send, sockname, data);

  if (NOTOK is Vtach.init.pty (vtach, sockname))
    return 1;

  ifnot (attach) {
    vwmed_t *vwmed = $my(objects)[VWMED_OBJECT];
    Vwmed.init.ved (vwmed);

    Vtach.set.object (vtach, vwmed, VWMED_OBJECT);
    Vtach.set.exec_child_cb (vtach, vexec_child);
    Vtach.set.pty_main_cb (vtach, vpty_main);

    Vtach.pty.main (vtach, argc, argv);
  }

  if (exit_this)
    return 0;

  return Vtach.tty.main (vtach);
}

public v_t *__init_v__ (vwm_t *vwm, v_opts *opts) {
  v_t *this = Alloc (sizeof (v_t));
  this->prop = Alloc (sizeof (v_prop));

  this->self = (v_self) {
    .main = v_main,
    .send = v_send,
    .get = (v_get_self) {
      .sockname = v_get_sockname,
      .object = v_get_object
    },
    .set = (v_set_self) {
      .object = v_set_object
    }
  };

  struct termios orig_mode;
  if (-1 isnot tcgetattr (0, &orig_mode)) { // dont exit yes, as we might be at the end of a pipe
    $my(orig_mode) = orig_mode;
    $my(input_fd) = 0;
  }

  $my(opts) = opts;
  $my(as_sockname) = NULL;

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

  Vwm.set.object (vwm, this, V_OBJECT);
  Vtach.set.object (vtach, this, V_OBJECT);
  Vtach.set.object (vtach, vwmed, VWMED_OBJECT);

  return this;
}

public void __deinit_v__ (v_t **thisp) {
  if (NULL is *thisp) return;

  v_t *this = *thisp;

  String.free ($my(as_sockname));

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
