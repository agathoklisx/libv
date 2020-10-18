typedef struct vtach_t vtach_t;
typedef struct vtach_prop vtach_prop;

typedef int (*PtyOnExecChild_cb) (vtach_t *, int, char **);

typedef struct vtach_set_self {
  void (*exec_child_cb) (vtach_t *, PtyOnExecChild_cb);
  vwm_t *(*vwm) (vtach_t *);
} vtach_set_self;

typedef struct vtach_get_self {
  vwm_t *(*vwm) (vtach_t *);
} vtach_get_self;

typedef struct vtach_init_self {
  int
    (*pty) (vtach_t *, char *);

  vwm_term
    *(*term) (vtach_t *, int *, int *);

} vtach_init_self;

typedef struct vtach_pty_self {
  int (*main) (vtach_t *this, int, char **);
} vtach_pty_self;

typedef struct vtach_tty_self {
  int (*main) (vtach_t *this);
} vtach_tty_self;

typedef struct vtach_self {
  vtach_set_self  set;
  vtach_get_self  get;
  vtach_pty_self  pty;
  vtach_tty_self  tty;
  vtach_init_self init;
} vtach_self;

typedef struct vtach_t {
  vtach_self self;
  vtach_prop *prop;
} vtach_t;

public vtach_t *__init_vtach__ (vwm_t *);
public void __deinit_vtach__ (vtach_t **);
