#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>

#include <libv.h>

#define V      v->self

int main (int argc, char **argv) {
  v_t *v = __init_v__ (NULL);
  int retval = V.main (v, argc, argv);
  __deinit_v__ (&v);
  exit (retval);
}
