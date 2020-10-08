#include <libved.h>
#include <libved+.h>

typedef struct vwm_ex_t {
  vwm_t *vwm;

  this_T *__This__;
  E_T    *__E__;

  ed_t   *ed;
  win_t  *win;
  buf_t  *buf;

  string_t *topline;
  video_t  *video;
} vwm_ex_t;

public vwm_ex_t *__init_vwm_ex__ (vwm_t *);
public void __deinit_vwm_ex__ (vwm_ex_t **);
