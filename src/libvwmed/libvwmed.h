#include <libved.h>
#include <libved+.h>

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
typedef void (*EdToplineMethod) (ed_t *, buf_t *);

typedef struct vwmed_t vwmed_t;
typedef struct vwmed_prop vwmed_prop;

typedef struct vwmed_get_self {
  vwm_t *(*vwm) (vwmed_t *);
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
