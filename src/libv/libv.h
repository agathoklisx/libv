#include <libv/libvwm.h>
#include <libv/libvwmed.h>
#include <libv/libvtach.h>

typedef struct v_t v_t;
typedef struct v_prop v_prop;

typedef struct v_init_opts {
  char
    *as,
    *data,
    *sockname,
    **argv;

  int
    argc,
    exit,
    force,
    attach,
    send_data,
    parse_argv;
} v_init_opts;

#define V_INIT_OPTS(...) \
  (v_init_opts) {        \
  .as = NULL,            \
  .data = NULL,          \
  .sockname = NULL,      \
  .argv = NULL,          \
  .argc = 0,             \
  .exit = 0,             \
  .force = 0,            \
  .attach = 0,           \
  .send_data = 0,        \
  .parse_argv = 1,       \
  __VA_ARGS__            \
}

typedef struct v_self {
  int
    (*main) (v_t *),
    (*send) (v_t *, char *, char *);
} v_self;

typedef struct v_t {
  v_self  self;
  v_prop *prop;
} v_t;

public v_t *__init_v__ (vwm_t *, v_init_opts *);
public void __deinit_v__ (v_t **);
