/* A testing application, that uses libvtach to add attaching|detaching capabilities to
 * the vwm application.
 * 
 * The only addition is MODKEY-CTRL(D) which simply detachs the application.
 */

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>

#include <libv/libvwm.h>
#include <libv/libvtach.h>
#include <libv/libvci.h>

#define Vtach  vtach->self

static const char usage[] =
  "vtach [options] [command] [command arguments]\n"
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

int main (int argc, char **argv) {
  vtach_t *vtach = __init_vtach__ (NULL);
  vwm_t *this = Vtach.get.vwm (vtach);

  int
    retval = 1,
    attach = 0;
  char *sockname = NULL;

  argv = set_argv (&argc, argv, &sockname, &attach);
  if (argc < 0) goto theend;

  if (NULL is sockname) {
    fprintf (stderr, "required socket name hasn't been specified\n");
    fprintf (stderr, "%s", usage);
    goto theend;
  }

  if (NOTOK is Vtach.init.pty (vtach, sockname))
    goto theend;

  ifnot (attach)
    Vtach.pty.main (vtach, argc, argv);

  retval = Vtach.tty.main (vtach);

theend:
  __deinit_vtach__ (&vtach);
  __deinit_vwm__ (&this);

  return retval;
}
