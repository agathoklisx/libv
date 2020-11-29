#include <libv/libvwm.h>
#include <libv/libvwmed.h>
#include <libv/libvtach.h>

typedef struct v_t v_t;
typedef struct v_prop v_prop;

typedef PtyOnExecChild_cb VExecChild;
typedef PtyMain_cb VPtyMain;

typedef struct v_init_opts {
  char
    *as,
    *data,
    *loadfile,
    *sockname,
    **argv;

  int
    argc,
    exit,
    force,
    attach,
    send_data,
    parse_argv,
    remove_socket,
    exit_on_no_command;

  VExecChild at_exec_child;
  VPtyMain at_pty_main;
} v_opts;

#define VOpts(...)         \
  (v_opts) {               \
  .as = NULL,              \
  .data = NULL,            \
  .loadfile = NULL,        \
  .sockname = NULL,        \
  .argv = NULL,            \
  .argc = 0,               \
  .exit = 0,               \
  .force = 0,              \
  .attach = 0,             \
  .send_data = 0,          \
  .parse_argv = 1,         \
  .remove_socket = 0,      \
  .at_pty_main = NULL,     \
  .at_exec_child = NULL,   \
  .exit_on_no_command = 1, \
  __VA_ARGS__              \
}

typedef struct v_get_self {
  char *(*sockname) (v_t *);
  void *(*object) (v_t *, int);
} v_get_self;

typedef struct v_set_self {
  void
    (*object) (v_t *, void *, int),
    (*save_image) (v_t *, int),
    (*image_file) (v_t *, char *),
    (*image_name) (v_t *, char *);

  int
    (*i_dir) (v_t *, char *),
    (*current_dir) (v_t *, char *, int),
    (*data_dir) (v_t *, char *);
} v_set_self;

typedef struct v_unset_self {
  void (*data_dir) (v_t *);
} v_unset_self;

typedef struct v_self {
  v_get_self get;
  v_set_self set;
  v_unset_self unset;

  int
    (*main) (v_t *),
    (*send) (v_t *, char *, char *),
    (*save_image) (v_t *, char *);
} v_self;

typedef struct v_t {
  v_self  self;
  v_prop *prop;
} v_t;

public v_t *__init_v__ (vwm_t *, v_opts *);
public void __deinit_v__ (v_t **);
