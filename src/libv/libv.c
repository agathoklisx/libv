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

struct v_prop {
  vwm_t   *vwm;
  vtach_t *vtach;
  vwmed_t *vwmed;

  v_init_opts *opts;
  string_t *as_sockname;
  int input_fd;
  struct termios orig_mode;
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
  "\n";

private int v_exec_child_default (vtach_t *vtach, int argc, char **argv) {
  vwm_t *vwm = Vtach.get.vwm (vtach);

  int rows = Vwm.get.lines (vwm);
  int cols = Vwm.get.columns (vwm);

  vwm_win *win = Vwm.new.win (vwm, NULL, WinNewOpts (
      .rows = rows,
      .cols = cols,
      .num_frames = 1,
      .max_frames = 2));
  vwm_frame *frame = Vwin.get.frame_at (win, 0);
  Vframe.set.argv (frame, argc, argv);
  Vframe.set.log  (frame, NULL, 1);
  Vframe.fork (frame);

  int retval = Vwm.main (vwm);

  vwmed_t *vwmed = (vwmed_t *) Vwm.get.user_object_at (vwm, VED_OBJECT);
  __deinit_vwmed__ (&vwmed);
  return retval;
}

private string_t *v_make_sockname (v_t *this, char *sockdir, char *as) {
  if (NULL is as) return NULL;

  ifnot (NULL is Cstring.byte.in_str (as, '\\')) {
    fprintf (stderr, "`as' argument includes a slash\n");
    return NULL;
  }

  vwmed_t *vwmed = $my(vwmed);

  size_t aslen = bytelen (as);
  size_t dirlen;

  char *sockd = sockdir;
  if (NULL is sockd) {
    string_t *tmp = E.get.env (Vwmed.get.e (vwmed), "tmp_dir");
    sockd = tmp->bytes;
    dirlen = tmp->num_bytes;
  } else {
    dirlen = bytelen (sockd);
    while (sockd[dirlen - 1] is '/') {
      dirlen--;
      sockd[dirlen] = '\0';
    }
  }

  size_t tlen = aslen + dirlen + 1;

  struct sockaddr_un sockun;
  if (tlen  > sizeof (sockun.sun_path) - 1) {
    fprintf (stderr, "socket name succeeds %zd limit\n", sizeof (sockun.sun_path));
    return NULL;
  }

  string_t *sockname = String.new_with_fmt ("%s/%s", sockd, as);
  return sockname;
}

private int v_send (v_t *this, char *sockname, char *data) {
  vtach_t *vtach = $my(vtach);
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
  v_init_opts *opts = $my(opts);

  int attach = opts->attach;
  int argc = opts->argc;
  int force = opts->force;
  int send_data = opts->send_data;
  char *sockname = opts->sockname;
  char **argv = opts->argv;
  char *as = opts->as;
  char *data = opts->data;

  if ($my(opts)->parse_argv) {
    argparse_option_t options[] = {
      OPT_HELP (),
      OPT_GROUP("Options:"),
      OPT_STRING(0, "as", &as, "create the socket name in an inner environment [required if -s is missing]", NULL, 0, 0),
      OPT_STRING('s', "sockname", &sockname, "set the socket name [required if --as= missing]", NULL, 0, 0),
      OPT_BOOLEAN('a', "attach", &attach, "attach to the specified socket", NULL, 0, 0),
      OPT_BOOLEAN(0, "force", &force, "connect to socket, even when socket exists", NULL, 0, 0),
      OPT_BOOLEAN(0, "send", &send_data, "send data to the specified socket", NULL, 0, 0),
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

  if (argc is 0 or argv is NULL) {
    if (0 is attach and 0 is send_data) {
      fprintf (stderr, "command hasn't been set\n");
      fprintf (stderr, "%s", usage);
      return 1;
    }
  }

  if (File.exists (sockname)) {
    if (0 is attach and 0 is send_data) {
      ifnot (force) {
        fprintf (stderr, "%s: exists in the filesystem\n", sockname);
        return 1;
      }
    }

    ifnot (File.is_sock (sockname)) {
      fprintf (stderr, "%s: is not a socket\n", sockname);
      return 1;
    }
  }

  if (0 is send_data or (send_data and data isnot NULL)) {
    if (0 is isatty (fileno (stdin))) {
      fprintf (stderr, "Not a controlled terminal\n");
      return 1;
    }
  }

  if (send_data)
    return self(send, sockname, data);

  vtach_t *vtach = $my(vtach);

  if (NOTOK is Vtach.init.pty ($my(vtach), sockname))
    return 1;

  ifnot (attach) {
    vwmed_t *vwmed = $my(vwmed);

    Vwmed.init.ved (vwmed);

    Vtach.set.exec_child_cb ($my(vtach), v_exec_child_default);

    Vtach.pty.main ($my(vtach), argc, argv);
  }

  return Vtach.tty.main ($my(vtach));
}

public v_t *__init_v__ (vwm_t *vwm, v_init_opts *opts) {
  v_t *this = Alloc (sizeof (v_t));

  this->prop = Alloc (sizeof (v_prop));

  this->self = (v_self) {
    .main = v_main,
    .send = v_send
  };

  struct termios orig_mode;
  if (-1 isnot tcgetattr (0, &orig_mode)) { // dont exit yes, as we might be at the end of a pipe
    $my(orig_mode) = orig_mode;
    $my(input_fd) = 0;
  }

  $my(opts) = opts;
  $my(as_sockname) = NULL;

  if (NULL is vwm) {
    $my(vwm) = __init_vwm__ ();
    vwm = $my(vwm);
  } else
    $my(vwm) = vwm;

  $my(vtach) = __init_vtach__ ($my(vwm));
  $my(vwmed) = __init_vwmed__ ($my(vwm));

  if (NULL is $my(vwmed)) {
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

    $my(vwmed) = __init_vwmed__ ($my(vwm));
    if (NULL is $my(vwmed)) {
      __deinit_v__ (&this);
      return NULL;
    }

    if (-1 isnot tcgetattr ($my(input_fd), &orig_mode)) // dont exit yes, as we might be at the end of a pipe
      $my(orig_mode) = orig_mode;

  }

  Vwm.set.user_object_at (vwm, this, V_OBJECT);

  return this;
}

public void __deinit_v__ (v_t **thisp) {
  if (NULL is *thisp) return;

  v_t *this = *thisp;

  String.free ($my(as_sockname));

  __deinit_vtach__ (&$my(vtach));
  __deinit_vwmed__ (&$my(vwmed));
  __deinit_vwm__   (&$my(vwm));

  tcsetattr ($my(input_fd), TCSAFLUSH, &$my(orig_mode));

  free (this->prop);
  free (this);
  *thisp = NULL;
}
