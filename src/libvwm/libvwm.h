#ifndef VTM_H
#define VTM_H

#ifndef public
#define public __attribute__((visibility ("default")))
#endif

#ifndef private
#define private __attribute__((visibility ("hidden")))
#endif

#ifndef mutable
#define mutable __attribute__((__weak__))
#endif

#ifndef OK
#define OK 0
#endif

#ifndef NOTOK
#define NOTOK -1
#endif

#ifndef ifnot
#define ifnot(__expr__) if (0 == (__expr__))
#endif

#ifndef ifnull
#define ifnull(__expr__) if (NULL == (__expr__))
#endif

#ifndef is
#define is ==
#endif

#ifndef isnot
#define isnot !=
#endif

#ifndef or
#define or ||
#endif

#ifndef and
#define and &&
#endif

#ifndef $my
#define $my(__p__) this->prop->__p__
#endif

#ifndef my
#define my this->self
#endif

#ifndef $myprop
#define $myprop this->prop
#endif

#ifndef bytelen
#define bytelen strlen
#endif

typedef unsigned int uint;
typedef unsigned char uchar;
typedef signed int utf8;

typedef struct vt_string {
  size_t
    mem_size,
    num_bytes;

  char *bytes;
} vt_string;

typedef struct VtString_T {
  void (*release) (vt_string *);
  vt_string
    *(*new) (size_t),
    *(*clear) (vt_string *),
    *(*append) (vt_string *, char *),
    *(*append_byte) (vt_string *, char),
    *(*append_with_len) (vt_string *, char *, size_t);
} VtString_T;

public VtString_T __init_vt_string__ (void);

typedef struct vwm_prop vwm_prop;
typedef struct vwm_term vwm_term;
typedef struct vwm_win vwm_win;
typedef struct vwm_frame vwm_frame;
typedef struct vwm_t vwm_t;

typedef vt_string *(*ProcessChar) (vwm_frame *, vt_string *, int);
typedef void (*Unimplemented) (vwm_frame *, const char *, int, int);

typedef struct win_opts {
  int
    rows,
    cols,
    first_row,
    first_col,
    num_frames,
    max_frames;
} win_opts;

#define WinNewOpts(...) (win_opts) {  \
  .rows = 24,                         \
  .cols = 78,                         \
  .first_row = 1,                     \
  .first_col = 1,                     \
  .num_frames = 1,                    \
  .max_frames = 3,                    \
  __VA_ARGS__ }

typedef struct vwm_term_screen_self {
  void
    (*clear) (vwm_term *),
    (*save) (vwm_term *),
    (*restore) (vwm_term *);
} vwm_term_screen_self;

typedef struct vwm_tern_self {
  vwm_term_screen_self screen;

  vwm_term *(*new) (void);

  void
    (*init_size) (vwm_term *, int *, int *),
    (*release) (vwm_term **);

  int
    (*orig_mode) (vwm_term *),
    (*raw_mode) (vwm_term *),
    (*sane_mode) (vwm_term *);

} vwm_term_self;

typedef struct vwm_frame_get_self {
  int (*fd) (vwm_frame *);
} vwm_frame_get_self;

typedef struct vwm_frame_set_self {
  void
    (*fd) (vwm_frame *, int),
    (*argv) (vwm_frame *, int, char **),
    (*unimplemented) (vwm_frame *, Unimplemented);

  int (*log) (vwm_frame *, char *,  int);
} vwm_frame_set_self;

typedef struct vwm_frame_self {
  vwm_frame_get_self get;
  vwm_frame_set_self set;

  void (*release) (vwm_win *, int);

  pid_t (*fork) (vwm_t *, vwm_frame *);

  vwm_frame *(*new) (vwm_win *, int, int);
} vwm_frame_self;


typedef struct vwm_win_set_self {
  void
    (*frame) (vwm_win *, vwm_frame *),
    (*frame_by_idx) (vwm_win *, int);
} vwm_win_set_self;

typedef struct vwm_win_get_self {
  vwm_frame *(*frame_at) (vwm_win *, int);
} vwm_win_get_self;

typedef struct vwm_win_frame_self {
  void
    (*change)        (vwm_win *, vwm_frame *, int, int),
    (*increase_size) (vwm_win *, vwm_frame *, int, int),
    (*decrease_size) (vwm_win *, vwm_frame *, int, int);
} vwm_win_frame_self;

typedef struct vwm_win_self {
  vwm_win_set_self set;
  vwm_win_get_self get;
  vwm_win_frame_self frame;

  void
    (*release) (vwm_t *, vwm_win *);

  int
    (*frame_rows) (vwm_win *, int, int *);

  vwm_win *(*new) (vwm_t *, char *, win_opts);

} vwm_win_self;

typedef struct vwm_get_self {
  int
    (*lines) (vwm_t *),
    (*columns) (vwm_t *);

  vwm_term *(*term) (vwm_t *);

} vwm_get_self;

typedef struct vwm_set_self {
  void
    (*size) (vwm_t *, int, int, int),
    (*tmpdir) (vwm_t *, char *, size_t);

} vwm_set_self;

typedef struct vwm_self {
   vwm_term_self term;
   vwm_win_self win;
   vwm_frame_self frame;
   vwm_get_self get;
   vwm_set_self set;

  int
    (*main) (vwm_t *),
    (*process_input) (vwm_t *, vwm_win *, vwm_frame *, char *);

  utf8 (*getkey) (int);

  void
    (*process_output) (vwm_frame *, char *, int);
} vwm_self;

struct vwm_t {
  vwm_self self;
  vwm_prop *prop;
};

public vwm_t *__init_vwm__ (void);
public void __deinit_vwm__ (vwm_t **);

#endif
