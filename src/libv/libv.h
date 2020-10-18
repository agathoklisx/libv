#include <libv/libvwm.h>
#include <libv/libvwmed.h>
#include <libv/libvtach.h>

typedef struct v_t v_t;
typedef struct v_prop v_prop;

typedef struct v_self {
  int (*main) (v_t *, int, char **);
} v_self;

typedef struct v_t {
  v_self  self;
  v_prop *prop;
} v_t;

public v_t *__init_v__ (vwm_t *);
public void __deinit_v__ (v_t **);
