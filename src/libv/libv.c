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
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <termios.h>
#include <errno.h>

#include <libv/libv.h>
#include <libv/libvci.h>

#define $my(__p__) this->prop->__p__

#define Vwm    vwm->self
#define Vwmed  vwmed->self
#define Vtach  vtach->self
#define Vframe vwm->frame
#define Vwin   vwm->win

struct v_prop {
  vwm_t   *vwm;
  vtach_t *vtach;
  vwmed_t *vwmed;
};

static const char usage[] =
  "v [options] [command] [command arguments]\n"
  "\n"
  "Options:\n"
  "    -s, --sockname=     set the socket name [required]\n"
  "    -a, --attach        attach to the specified socket\n";

private char **set_argv (int *argc, char **argv, char **sockname, int *attach) {
  argv++; *argc -= 1;

  char **largv = argv;

  int n = 0;
  int skip = 0;
  for (int i = 0; i < *argc; i++) {
    if (skip) {
      skip = 0;
      continue;
    }

    if (0 == strcmp (argv[i], "-h") or
        0 == strcmp (argv[i], "--help")) {
      fprintf (stderr, "%s", usage);
      *argc = -1;
      return NULL;
    }

    if (0 == strncmp (argv[i], "--sockname=", 11)) {
      char *sp = strchr (argv[i], '=') + 1;
      ifnot (*sp)
        continue;

      *sockname = sp;
      largv++;
      continue;
    }

    if (0 == strcmp (argv[i], "-s")) {
      if (i + 1 == *argc)
        continue;

      *sockname = argv[i+1];
      skip = 1;
      largv += 2;
      continue;
    }

    if (0 == strcmp (argv[i], "-a") or
        0 == strcmp (argv[i], "--attach")) {
      *attach = 1;
      largv++;
      continue;
    }

    n++;
  }

  *argc = n;
  return largv;
}

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

  vwmed_t *vwmed = (vwmed_t *) Vwm.get.user_object (vwm);
  __deinit_vwmed__ (&vwmed);
  return retval;
}

private int v_main (v_t *this, int argc, char **argv) {
  int attach = 0;
  char *sockname = NULL;

  argv = set_argv (&argc, argv, &sockname, &attach);
  if (argc < 0) return 1;

  if (NULL is sockname) {
    fprintf (stderr, "required socket name hasn't been specified\n");
    fprintf (stderr, "%s", usage);
    return 1;
  }

  vtach_t *vtach = $my(vtach);

  if (NOTOK is Vtach.init.pty ($my(vtach), sockname))
    return 1;

  vwm_t *vwm = $my(vwm);

  vwmed_t *vwmed =  __init_vwmed__ ($my(vwm));
  $my(vwmed) = vwmed;

  ifnot (attach) {
    Vwmed.init.ved (vwmed);
    Vtach.set.exec_child_cb ($my(vtach), v_exec_child_default);
    Vtach.pty.main ($my(vtach), argc, argv);
  }

  return Vtach.tty.main ($my(vtach));
}

public v_t *__init_v__ (vwm_t *vwm) {
  v_t *this = Alloc (sizeof (v_t));

  this->prop = Alloc (sizeof (v_prop));
  this->self = (v_self) {
    .main = v_main
  };

  if (NULL is vwm)
    $my(vwm) = __init_vwm__ ();
  else
    $my(vwm) = vwm;

  $my(vtach) = __init_vtach__ ($my(vwm));

  return this;
}

public void __deinit_v__ (v_t **thisp) {
  if (NULL is *thisp) return;

  v_t *this = *thisp;

  __deinit_vtach__ (&$my(vtach));
  __deinit_vwmed__ (&$my(vwmed));
  __deinit_vwm__ (&$my(vwm));

  free (this->prop);
  free (this);
  *thisp = NULL;
}
