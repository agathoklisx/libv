#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <pty.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <libv/libvwm.h>
#include <libv/libvtach.h>
#include <libv/libvci.h>

#define self(__f__, ...) this->self.__f__ (this, ##__VA_ARGS__)
#define $my(__p__) this->prop->__p__

#define Vwm    $my(vwm)->self
#define Vwin   $my(vwm)->win
#define Vframe $my(vwm)->frame
#define Vterm  $my(vwm)->term

enum
{
  MSG_PUSH  = 0,
  MSG_ATTACH  = 1,
  MSG_DETACH  = 2,
  MSG_WINCH  = 3,
  MSG_REDRAW  = 4,
};

enum
{
  REDRAW_UNSPEC  = 0,
  REDRAW_NONE  = 1,
  REDRAW_CTRL_L  = 2,
  REDRAW_WINCH  = 3,
};

struct packet {
  unsigned char type;
  unsigned char len;
  union  {
    unsigned char buf[sizeof (struct winsize)];
    struct winsize ws;
  } u;
};

struct pty {
  int fd;
  pid_t pid;
  struct termios term;
  struct winsize ws;
};

struct client {
  struct client *next;
  struct client **pprev;
  int fd;
  int attached;
};

struct vtach_prop {
  vwm_t *vwm;
  vwm_term *term;

  char mode_key;
  char *sockname;

  int
    detach_char,
    no_suspend,
    redraw_method;

  int
    waitattach,
    dont_have_tty;

  struct client *clients;
  struct pty pty;
  PtyOnExecChild_cb exec_child_cb;
};

#define BUFSIZE 4096
#define EOS "\033[999H"

static int win_changed;

private int fd_set_nonblocking (int fd) {
  int flags = fcntl (fd, F_GETFL);
  if (flags < 0 or fcntl (fd, F_SETFL, flags | O_NONBLOCK) < 0)
    return NOTOK;

  return OK;
}

private int create_socket (char *sockname) {
  int s;
  struct sockaddr_un sockun;

  if (bytelen (sockname) > sizeof (sockun.sun_path) - 1) {
    errno = ENAMETOOLONG;
    return -1;
  }

  s = socket (PF_UNIX, SOCK_STREAM, 0);
  if (s < 0)
    return -1;

  sockun.sun_family = AF_UNIX;
  strcpy (sockun.sun_path, sockname);
  if (bind (s, (struct sockaddr*)&sockun, sizeof (sockun)) < 0) {
    close (s);
    return -1;
  }

  if (listen (s, 128) < 0) {
    close (s);
    return -1;
  }

  if (fd_set_nonblocking (s) < 0) {
    close (s);
    return -1;
  }

  if (chmod (sockname, 0600) < 0) {
    close (s);
    return -1;
  }

  return s;
}

private int connect_socket (char *sockname) {
  struct sockaddr_un sockun;
  int s = socket (PF_UNIX, SOCK_STREAM, 0);

  if (s < 0)
    return -1;

  sockun.sun_family = AF_UNIX;
  strcpy (sockun.sun_path, sockname);

  if (connect (s, (struct sockaddr*)&sockun, sizeof (sockun)) < 0) {
    close (s);

    /* ECONNREFUSED is also returned for regular files, so make
    ** sure we are trying to connect to a socket. */
    if (errno is ECONNREFUSED) {
      struct stat st;

      if (stat (sockname, &st) < 0)
        return -1;
      else if (!S_ISSOCK(st.st_mode) or S_ISREG(st.st_mode))
        errno = ENOTSOCK;
    }

    return -1;
  }

  return s;
}

private void update_socket_modes (char *sockname, int exec) {
  struct stat st;
  mode_t newmode;

  if (stat (sockname, &st) < 0)
    return;

  if (exec)
    newmode = st.st_mode | S_IXUSR;
  else
    newmode = st.st_mode & ~S_IXUSR;

  if (st.st_mode != newmode)
    chmod (sockname, newmode);
}

private void pty_die (int sig) {
  if (sig is SIGCHLD)
    return;

  exit (1);
}

private void tty_die (int sig) {
  if (sig is SIGHUP or sig is SIGINT)
    fprintf (stdout, EOS "\r\n[detached]\r\n");
  else
    fprintf (stdout, EOS "\r\n[got signal %d - dying]\r\n", sig);

  exit (1);
}

private void tty_sigwinch_handler (int);
private void tty_sigwinch_handler (int sig) {
  (void) sig;
  signal (SIGWINCH, tty_sigwinch_handler);
  win_changed = 1;
}

private char *ustring_character (utf8 c, char *buf, int *len) {
  *len = 1;
  if (c < 0x80) {
    buf[0] = (char) c;
  } else if (c < 0x800) {
    buf[0] = (c >> 6) | 0xC0;
    buf[1] = (c & 0x3F) | 0x80;
    (*len)++;
  } else if (c < 0x10000) {
    buf[0] = (c >> 12) | 0xE0;
    buf[1] = ((c >> 6) & 0x3F) | 0x80;
    buf[2] = (c & 0x3F) | 0x80;
    (*len) += 2;
  } else if (c < 0x110000) {
    buf[0] = (c >> 18) | 0xF0;
    buf[1] = ((c >> 12) & 0x3F) | 0x80;
    buf[2] = ((c >> 6) & 0x3F) | 0x80;
    buf[3] = (c & 0x3F) | 0x80;
    (*len) += 3;
  } else
    return 0;

  buf[*len] = '\0';
  return buf;
}

private int tty_process_kbd (vtach_t *this, int s, struct packet *pkt) {
  /* Suspend? */
  if (0 is $my(no_suspend) and (pkt->u.buf[0] is $my(term)->raw_mode.c_cc[VSUSP])) {
    pkt->type = MSG_DETACH;
    write (s, pkt, sizeof (struct packet));

    tcsetattr (0, TCSADRAIN, &$my(term)->orig_mode);
    fprintf (stdout, EOS "\r\n");
    kill (getpid(), SIGTSTP);
    tcsetattr (0, TCSADRAIN, &$my(term)->raw_mode);

    /* Tell the master that we are returning. */
    pkt->type = MSG_ATTACH;
    write (s, pkt, sizeof (struct packet));

    /* We would like a redraw, too. */
    pkt->type = MSG_REDRAW;
    pkt->len = $my(redraw_method);
    ioctl (0, TIOCGWINSZ, &pkt->u.ws);
    write (s, pkt, sizeof (struct packet));
    return 0;
  } else if (pkt->u.buf[0] is $my(mode_key)) {
    utf8 c = Vwm.getkey ($my(vwm), 0);

    if (c is $my(detach_char)) {
      printf (EOS "\r\n[detached]\r\n");
      return 1;
    }

    pkt->len = 1;
    write (s, pkt, sizeof (struct packet));

    int len;
    char buf[8];
    ustring_character (c, buf, &len);
    pkt->len = len;
    for (int i = 0; i < len; i++) pkt->u.buf[i] = buf[i];
  }
  /* Just in case something pukes out. */
  else if (pkt->u.buf[0] is '\f')
    win_changed = 1;

  /* Push it out */
  write (s, pkt, sizeof (struct packet));
  return 0;
}

private int vtach_tty_main (vtach_t *this) {
  int s = connect_socket ($my(sockname));

  if (s < 0) {
    fprintf (stderr, "%s: %s\n", $my(sockname), strerror (errno));
    return 1;
  }

  signal (SIGPIPE, SIG_IGN);
  signal (SIGXFSZ, SIG_IGN);
  signal (SIGHUP,   tty_die);
  signal (SIGTERM,  tty_die);
  signal (SIGINT,   tty_die);
  signal (SIGQUIT,  tty_die);
  signal (SIGWINCH, tty_sigwinch_handler);

  Vterm.raw_mode ($my(term));
  Vterm.screen.save ($my(term));
  Vterm.screen.clear ($my(term));

  struct packet pkt;

  memset (&pkt, 0, sizeof (struct packet));
  pkt.type = MSG_ATTACH;
  write (s, &pkt, sizeof (struct packet));

  pkt.type = MSG_REDRAW;
  pkt.len = $my(redraw_method);
  ioctl (0, TIOCGWINSZ, &pkt.u.ws);
  write (s, &pkt, sizeof (struct packet));

  int retval = 0;

  unsigned char buf[BUFSIZE];
  fd_set readfds;

  while (1) {

    FD_ZERO(&readfds);
    FD_SET(0, &readfds);
    FD_SET(s, &readfds);

    int n = select (s + 1, &readfds, NULL, NULL, NULL);

    if (n < 0 and errno isnot EINTR and errno isnot EAGAIN) {
      fprintf (stderr, EOS "\r\n[select failed]\r\n");
      retval = -1;
      break;
    }

    if (n > 0 and FD_ISSET(s, &readfds)) {
      ssize_t len = read (s, buf, sizeof (buf));

      if (len is 0) {
        fprintf (stderr, EOS "\r\n[EOF - dtach terminating]\r\n");
        break;
      } else if (len < 0) {
        fprintf (stderr, EOS "\r\n[read returned an error]\r\n");
        retval = -1;
        break;
      }

      write (1, buf, len);
      n--;
    }

    if (n > 0 and FD_ISSET(0, &readfds)) {
      ssize_t len;

      pkt.type = MSG_PUSH;
      memset (pkt.u.buf, 0, sizeof (pkt.u.buf));
      len = read (0, pkt.u.buf, sizeof (pkt.u.buf));

      if (len <= 0) {
        retval = -1;
        break;
      }

      pkt.len = len;
      if (1 is (retval = tty_process_kbd (this, s, &pkt)))
        break;

      n--;
    }

    if (win_changed) {
      win_changed = 0;

      pkt.type = MSG_WINCH;
      ioctl (0, TIOCGWINSZ, &pkt.u.ws);
      write (s, &pkt, sizeof (pkt));
    }
  }

  Vterm.orig_mode ($my(term));
  Vterm.screen.restore ($my(term));

  if (1 isnot retval)
    unlink ($my(sockname));

  return (retval is -1 ? 1 : 0);
}

private int pty_child (vtach_t *this, int argc, char **argv) {
  $my(pty).term = $my(term)->orig_mode;
  memset (&$my(pty).ws, 0, sizeof (struct winsize));

  char name[1024];
  $my(pty).pid = forkpty (&$my(pty).fd, name, &$my(pty).term, NULL);

  if ($my(pty).pid < 0)
    return -1;

  if ($my(pty).pid is 0) {
    setsid ();

    int fd = open (name, O_RDWR|O_CLOEXEC|O_NOCTTY);
    close ($my(pty).fd);

    close (0);
    close (1);
    close (2);

    dup (fd);
    dup (fd);
    dup (fd);

    ioctl (0, TIOCSCTTY, 1);

    int rows = Vwm.get.lines ($my(vwm));
    int cols = Vwm.get.columns ($my(vwm));

    struct winsize wsiz;
    wsiz.ws_row = rows;
    wsiz.ws_col = cols;
    wsiz.ws_xpixel = 0;
    wsiz.ws_ypixel = 0;
    ioctl (fd, TIOCSWINSZ, &wsiz);

    close (fd);

    int retval = $my(exec_child_cb) (this, argc, argv);
    __deinit_vwm__ (&$my(vwm));
    __deinit_vtach__ (&this);

    _exit (retval);
  }

  return 0;
}

private void killpty (struct pty *pty, int sig) {
  pid_t pgrp = -1;

#ifdef TIOCSIGNAL
  if (ioctl (pty->fd, TIOCSIGNAL, sig) >= 0)
    return;
#endif

#ifdef TIOCSIG
  if (ioctl (pty->fd, TIOCSIG, sig) >= 0)
    return;
#endif

#ifdef TIOCGPGRP
  if (ioctl (pty->fd, TIOCGPGRP, &pgrp) >= 0 and pgrp isnot -1 and
    kill (-pgrp, sig) >= 0)
    return;
#endif

  /* Fallback using the child's pid. */
  kill (-pty->pid, sig);
}

private void pty_activity (vtach_t *this, int s) {
  unsigned char buf[BUFSIZE];
  ssize_t len;
  struct client *p;
  fd_set readfds, writefds;
  int max_fd, nclients;

  len = read ($my(pty).fd, buf, sizeof (buf));

  if (len <= 0)
    exit (1);

  if (tcgetattr ($my(pty).fd, &$my(pty).term) < 0)
    exit (1);

top:
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_SET(s, &readfds);
  max_fd = s;

  for (p = $my(clients), nclients = 0; p; p = p->next) {
    if (!p->attached)
      continue;

    FD_SET(p->fd, &writefds);
    if (p->fd > max_fd)
      max_fd = p->fd;

    nclients++;
  }

  ifnot (nclients) return;

  if (select (max_fd + 1, &readfds, &writefds, NULL, NULL) < 0)
    return;

  for (p = $my(clients), nclients = 0; p; p = p->next) {
    ssize_t written;

    if (!FD_ISSET(p->fd, &writefds))
      continue;

    written = 0;
    while (written < len) {
      ssize_t n = write (p->fd, buf + written, len - written);

      if (n > 0) {
        written += n;
        continue;
      } else if (n < 0 and errno is EINTR)
        continue;
      else if (n < 0 and errno isnot EAGAIN)
        nclients = -1;
      break;
    }

    if (nclients isnot -1 and written is len)
      nclients++;
  }

  /* Try again if nothing happened. */
  if (!FD_ISSET(s, &readfds) and nclients is 0)
    goto top;
}

private void pty_socket_activity (vtach_t *this, int s) {
  int fd = accept (s, NULL, NULL);
  if (fd < 0)
    return;

  if (fd_set_nonblocking (fd) < 0) {
    close (fd);
    return;
  }

  struct client *p = Alloc (sizeof (struct client));

  p->fd = fd;
  p->attached = 0;
  p->pprev = &$my(clients);
  p->next = *(p->pprev);
  if (p->next)
    p->next->pprev = &p->next;
  *(p->pprev) = p;
}

private void pty_client_activity (vtach_t *this, struct client *p) {
  struct packet pkt;

  ssize_t len = read (p->fd, &pkt, sizeof (struct packet));
  if (len < 0 and (errno is EAGAIN or errno is EINTR))
    return;

  if (len <= 0) {
    close (p->fd);

    if (p->next)
      p->next->pprev = p->pprev;
    *(p->pprev) = p->next;
    free(p);
    return;
  }

  /* Push out data to the program. */
  if (pkt.type is MSG_PUSH) {
    if (pkt.len <= sizeof (pkt.u.buf))
      write ($my(pty).fd, pkt.u.buf, pkt.len);
  } else if (pkt.type is MSG_ATTACH)
    p->attached = 1;
  else if (pkt.type is MSG_DETACH)
    p->attached = 0;
  else if (pkt.type is MSG_WINCH) {
    $my(pty).ws = pkt.u.ws;
    ioctl ($my(pty).fd, TIOCSWINSZ, &$my(pty).ws);
  } else if (pkt.type == MSG_REDRAW) {
    int method = pkt.len;

    /* If the client didn't specify a particular method, use
    ** whatever we had on startup. */
    if (method is REDRAW_UNSPEC)
      method = $my(redraw_method);
    if (method is REDRAW_NONE)
      return;

    $my(pty).ws = pkt.u.ws;
    ioctl ($my(pty).fd, TIOCSWINSZ, &$my(pty).ws);

    /* Send a ^L character if the terminal is in no-echo and
    ** character-at-a-time mode. */
    if (method is REDRAW_CTRL_L) {
      char c = '\f';

      if ((($my(pty).term.c_lflag & (ECHO|ICANON)) is 0) and
           ($my(pty).term.c_cc[VMIN] is 0))
           //($my(pty).term.c_cc[VMIN] is 1)) {
        write ($my(pty).fd, &c, 1);
    } else if (method is REDRAW_WINCH)
      killpty (&$my(pty), SIGWINCH);
  }
}

private void pty_process (vtach_t *this, int s, int argc, char **argv, int statusfd) {
  setsid ();

  signal (SIGCHLD, pty_die);

  if (pty_child (this, argc, argv) < 0) {
    if (statusfd isnot -1)
      dup2 (statusfd, 1);

    if (errno is ENOENT)
      printf ("Could not find a pty.\n");
    else
      printf ("init_pty: %s\n", strerror (errno));

    unlink ($my(sockname));
    exit (1);
  }

  signal (SIGPIPE, SIG_IGN);
  signal (SIGXFSZ, SIG_IGN);
  signal (SIGHUP, SIG_IGN);
  signal (SIGTTIN, SIG_IGN);
  signal (SIGTTOU, SIG_IGN);
  signal (SIGINT, pty_die);
  signal (SIGTERM, pty_die);

  /* Close statusfd, since we don't need it anymore. */
  if (statusfd isnot -1) close (statusfd);

  /* Make sure stdin/stdout/stderr point to /dev/null. We are now a
  ** daemon. */
  int nullfd = open ("/dev/null", O_RDWR);
  dup2 (nullfd, 0);
  dup2 (nullfd, 1);
  dup2 (nullfd, 2);

  if (nullfd > 2)
    close (nullfd);

  struct client *p, *next;
  fd_set readfds;
  int
    max_fd,
    has_attached_client = 0;

  while (1) {
    int new_has_attached_client = 0;

    FD_ZERO(&readfds);
    FD_SET(s, &readfds);
    max_fd = s + 1;

    /*
    ** When waitattach is set, wait until the client attaches
    ** before trying to read from the pty.
    */
    if ($my(waitattach)) {
      if ($my(clients) and $my(clients)->attached)
        $my(waitattach) = 0;
    } else {
      FD_SET($my(pty).fd, &readfds);
      if ($my(pty).fd > max_fd)
        max_fd = $my(pty).fd;
    }

    for (p = $my(clients); p; p = p->next) {
      FD_SET(p->fd, &readfds);
      if (p->fd > max_fd)
        max_fd = p->fd;

      if (p->attached)
        new_has_attached_client = 1;
    }

    /* chmod the socket if necessary. */
    if (has_attached_client isnot new_has_attached_client) {
      update_socket_modes ($my(sockname), new_has_attached_client);
      has_attached_client = new_has_attached_client;
    }

    if (select (max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
      if (errno is EINTR or errno isnot EAGAIN)
        continue;

      unlink ($my(sockname));
      exit (1);
    }

    /* New client? */
    if (FD_ISSET(s, &readfds))
      pty_socket_activity (this, s);

    for (p = $my(clients); p; p = next) {
      next = p->next;
      if (FD_ISSET(p->fd, &readfds))
        pty_client_activity (this, p);
    }

    if (FD_ISSET($my(pty).fd, &readfds))
      pty_activity (this, s);
  }
}

private int vtach_pty_main (vtach_t *this, int argc, char **argv) {
  int s = create_socket ($my(sockname));

  if (s < 0) {
    printf ("%s: %s\n", $my(sockname), strerror (errno));
    return -1;
  }

  fcntl (s, F_SETFD, FD_CLOEXEC);

  int fd[2] = {-1, -1};

  /* If FD_CLOEXEC works, create a pipe and use it to report any errors
  ** that occur while trying to execute the program. */
  if (pipe (fd) >= 0) {
    if (fcntl (fd[0], F_SETFD, FD_CLOEXEC) < 0 or
        fcntl (fd[1], F_SETFD, FD_CLOEXEC) < 0) {
      close (fd[0]);
      close (fd[1]);
      fd[0] = fd[1] = -1;
    }
  }

  int rows; int cols;
  self(init.term, &rows, &cols);

  pid_t pid = fork ();

  if (pid < 0) {
    printf ("fork: %s\n", strerror (errno));
    unlink ($my(sockname));
    return -1;
  } else if (pid is 0) {
    /* Child - this becomes the master */
    if (fd[0] != -1)
      close (fd[0]);

    pty_process (this, s, argc, argv, fd[1]);

    return 0;
  }

  close (s);

  return 0;
}

private vwm_term *vtach_init_term (vtach_t *this, int *rows, int *cols) {
  vwm_t *vwm = $my(vwm);

  vwm_term *term =  Vwm.get.term (vwm);

  Vterm.raw_mode (term);

  Vterm.init_size (term, rows, cols);

  Vwm.set.size (vwm, *rows, *cols, 1);

  return term;
}

private int vtach_exec_child_default (vtach_t *this, int argc, char **argv) {
  vwm_t *vwm = $my(vwm);

  int rows = Vwm.get.lines (vwm);
  int cols = Vwm.get.columns (vwm);

  vwm_win *win = Vwm.new.win (vwm, NULL, WinNewOpts (
      .rows = rows,
      .cols = cols,
      .num_frames = 1,
      .max_frames = 2));
  vwm_frame *frame = Vwin.get.frame_at (win, 0);
  Vframe.set.argv (frame, argc, argv);
  Vframe.fork (frame);

  return Vwm.main (vwm);
}

private int vtach_init_pty (vtach_t *this, char *sockname) {
  struct sockaddr_un sockun;
  if (bytelen (sockname) > sizeof (sockun.sun_path) - 1) {
    fprintf (stderr, "socket name succeeds %zd limit\n", sizeof (sockun.sun_path));
    return NOTOK;
  }

  if (tcgetattr (0, &$my(term)->orig_mode) < 0) {
    memset (&$my(term)->orig_mode, 0, sizeof (struct termios));
    $my(dont_have_tty) = 1;
  }

  $my(sockname) = sockname;
  $my(redraw_method) = REDRAW_WINCH;
  $my(no_suspend) = 0;
  $my(detach_char) = 04;
  $my(waitattach) = 1;

  $my(exec_child_cb) = vtach_exec_child_default;

  return OK;
}

private void vtach_set_exec_child_cb (vtach_t *this, PtyOnExecChild_cb cb) {
  $my(exec_child_cb) = cb;
}

private vwm_t *vtach_get_vwm (vtach_t *this) {
  return $my(vwm);
}

public vtach_t *__init_vtach__ (vwm_t *vwm) {
  vtach_t *this = Alloc (sizeof (vtach_t));

  this->prop = Alloc (sizeof (vtach_prop));

  this->self = (vtach_self) {
    .set = (vtach_set_self) {
      .exec_child_cb = vtach_set_exec_child_cb
    },
    .get = (vtach_get_self) {
      .vwm = vtach_get_vwm
    },
    .init = (vtach_init_self) {
      .term = vtach_init_term,
      .pty = vtach_init_pty
    },
    .pty = (vtach_pty_self) {
      .main = vtach_pty_main
    },
    .tty = (vtach_tty_self) {
      .main = vtach_tty_main
    }
  };

  if (NULL is vwm)
    $my(vwm) = __init_vwm__ ();
  else
    $my(vwm) = vwm;

  $my(term) = Vwm.get.term ($my(vwm));
  $my(mode_key) = Vwm.get.mode_key ($my(vwm));

  Vwm.set.user_object ($my(vwm), (void *) this);

  return this;
}

public void __deinit_vtach__ (vtach_t **thisp) {
  if (NULL is *thisp) return;

  vtach_t *this = *thisp;

  free (this->prop);
  free (this);
  *thisp = NULL;
}
