#include <libved.h>
#include <libved+.h>

typedef struct vwmed_t vwmed_t;
typedef struct vwmed_prop vwmed_prop;

typedef void (*EdToplineMethod) (ed_t *, buf_t *);
typedef void (*VwmedRlineCommand_cb) (ed_t *);
typedef int  (*VwmedRline_cb) (vwmed_t *, rline_t *, vwm_t *, vwm_win *, vwm_frame *);
typedef int  (*VwmedInfo_cb) (vwmed_t *, vwm_t *, FILE *);

typedef struct vwmed_set_self {
  void
    (*object) (vwmed_t *, void *, int),
    (*info_cb) (vwmed_t *, VwmedInfo_cb),
    (*rline_cb) (vwmed_t *, VwmedRline_cb),
    (*rline_command_cb) (vwmed_t *, VwmedRlineCommand_cb);
} vwmed_set_self;

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
  vwmed_set_self   set;
  vwmed_init_self  init;
} vwmed_self;

typedef struct vwmed_t {
  vwmed_self self;
  vwmed_prop *prop;
} vwmed_t;

public vwmed_t *__init_vwmed__ (vwm_t *);
public void __deinit_vwmed__ (vwmed_t **);
