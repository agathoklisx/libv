
#include <libvwm.h>
#include <libvwmed.h>
#include <libvtach.h>


#ifndef STR_FMT
#define STR_FMT(fmt_, ...)                                            \
({                                                                    \
  char buf_[MAXLEN_LINE];                                             \
  snprintf (buf_, MAXLEN_LINE, fmt_, __VA_ARGS__);                    \
  buf_;                                                               \
})
#endif

#ifndef debug_append
#define debug_append(fmt, ...)                            \
({                                                        \
  char *file_ = STR_FMT ("/tmp/%s.debug", __func__);      \
  FILE *fp_ = fopen (file_, "a+");                        \
  if (fp_ isnot NULL) {                                   \
    fprintf (fp_, (fmt), ## __VA_ARGS__);                 \
    fclose (fp_);                                         \
  }                                                       \
})
#endif

typedef struct v_t v_t;
typedef struct v_prop v_prop;

/*
typedef struct vwm_ex_set_self {
  void (*exec_child_cb) (vwm_ex *, PtyOnExecChild_cb);
    vwm_t *(*vwm) (vwm_ex *);
} vwm_ex_set_self;

typedef struct vwm_ex_get_self {
  vwm_t *(*vwm) (vwm_ex *);
} vwm_ex_get_self;

typedef struct vwm_ex_init_self {
  int
    (*ved) (vwm_ex *),
    (*pty) (vwm_ex *, char *);

  vwm_term
    *(*term) (vwm_ex *, int *, int *);

} vwm_ex_init_self;

typedef struct vwm_ex_pty_self {
  int (*main) (vwm_ex *this, int, char **);
} vwm_ex_pty_self;

typedef struct vwm_ex_tty_self {
  int (*main) (vwm_ex *this);
} vwm_ex_tty_self;

typedef struct vwm_ex_self {
  vwm_ex_set_self  set;
  vwm_ex_get_self  get;
  vwm_ex_pty_self  pty;
  vwm_ex_tty_self  tty;
  vwm_ex_init_self init;
} vwm_ex_self;
*/

typedef struct v_self {
  int (*main) (v_t *, int, char **);
} v_self;

typedef struct v_t {
  v_self  self;
  v_prop *prop;
} v_t;

public v_t *__init_v__ (vwm_t *);
public void __deinit_v__ (v_t **);
