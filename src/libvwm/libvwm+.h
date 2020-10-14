#include <libved.h>
#include <libved+.h>

typedef void (*EdToplineMethod) (ed_t *, buf_t *);

typedef struct vwm_ex vwm_ex;
typedef struct vwm_ex_prop vwm_ex_prop;

typedef struct vwm_ex_get_self {
  vwm_t *(*vwm) (vwm_ex *);
} vwm_ex_get_self;

typedef struct vwm_ex_init_self {
  int
    (*ved) (vwm_ex *);

  vwm_term
    *(*term) (vwm_ex *, int *, int *);

} vwm_ex_init_self;

typedef struct vwm_ex_self {
  vwm_ex_get_self   get;
  vwm_ex_init_self  init;
} vwm_ex_self;

typedef struct vwm_ex {
  vwm_ex_self self;
  vwm_ex_prop *prop;
} vwm_ex;

public vwm_ex *__init_vwm_ex__ (vwm_t *);
public void __deinit_vwm_ex__ (vwm_ex **);
