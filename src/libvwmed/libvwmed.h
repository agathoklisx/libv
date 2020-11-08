#include <libved.h>
#include <libved+.h>

typedef void (*EdToplineMethod) (ed_t *, buf_t *);

typedef struct vwmed_t vwmed_t;
typedef struct vwmed_prop vwmed_prop;

typedef struct vwmed_get_self {
  E_T  *(*e) (vwmed_t *);
  void *(*object) (vwmed_t *, int);
} vwmed_get_self;

typedef struct vwmed_init_self {
  int
    (*ved) (vwmed_t *);

  vwm_term
    *(*term) (vwmed_t *, int *, int *);

} vwmed_init_self;

typedef struct vwmed_self {
  vwmed_get_self   get;
  vwmed_init_self  init;
} vwmed_self;

typedef struct vwmed_t {
  vwmed_self self;
  vwmed_prop *prop;
} vwmed_t;

public vwmed_t *__init_vwmed__ (vwm_t *);
public void __deinit_vwmed__ (vwmed_t **);
