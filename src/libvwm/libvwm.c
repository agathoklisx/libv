#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <pty.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>
#include <dirent.h>
#include <signal.h>

#include <errno.h>

#include <libv/libvwm.h>
#include "__libvwm.h"

static vwm_t *VWM;

#ifndef SHELL
#define SHELL "zsh"
#endif

#ifndef EDITOR
#define EDITOR "vi"
#endif

#ifndef DEFAULT_APP
#define DEFAULT_APP SHELL
#endif

#ifndef TMPDIR
#define TMPDIR "/tmp"
#endif

#ifndef MODE_KEY
#define MODE_KEY  CTRL('\\')
#endif

#ifndef CTRL
#define CTRL(X) (X & 037)
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096  /* bytes in a path name */
#endif

#define MIN_ROWS 2
#define MAX_TTYNAME 1024
#define MAX_PARAMS  12
#define MAX_SEQ_LEN 32

#define TABWIDTH    8

#define ISDIGIT(c_)     ('0' <= (c_) && (c_) <= '9')
#define IS_UTF8(c_)     (((c_) & 0xC0) == 0x80)
#define isnotutf8(c_)   (IS_UTF8 (c_) == 0)
#define isnotatty(fd_)  (0 == isatty ((fd_)))

#define V_STR_FMT_LEN(len_, fmt_, ...)                                      \
({                                                                    \
  char buf_[len_];                                                    \
  snprintf (buf_, len_, fmt_, __VA_ARGS__);                           \
  buf_;                                                               \
})

#define NORMAL          0x00
#define BOLD            0x01
#define UNDERLINE       0x02
#define BLINK           0x04
#define REVERSE         0x08
#define ITALIC          0x10
#define SELECTED        0xF0
#define COLOR_FG_NORM   37
#define COLOR_BG_NORM   49

#define NCHARSETS       2
#define G0              0
#define G1              1
#define UK_CHARSET      0x01
#define US_CHARSET      0x02
#define GRAPHICS        0x04

#define TERM_LAST_RIGHT_CORNER      "\033[999C\033[999B"
#define TERM_LAST_RIGHT_CORNER_LEN  12
#define TERM_GET_PTR_POS            "\033[6n"
#define TERM_GET_PTR_POS_LEN        4
#define TERM_SCREEN_SAVE            "\033[?47h"
#define TERM_SCREEN_SAVE_LEN        6
#define TERM_SCREEN_RESTORE        "\033[?47l"
#define TERM_SCREEN_RESTORE_LEN     6
#define TERM_SCREEN_CLEAR           "\033[2J"
#define TERM_SCREEN_CLEAR_LEN       4
#define TERM_SCROLL_RESET           "\033[r"
#define TERM_SCROLL_RESET_LEN       3
#define TERM_GOTO_PTR_POS_FMT       "\033[%d;%dH"
#define TERM_CURSOR_HIDE            "\033[?25l"
#define TERM_CURSOR_HIDE_LEN        6
#define TERM_CURSOR_SHOW            "\033[?25h"
#define TERM_CURSOR_SHOW_LEN        6
#define TERM_AUTOWRAP_ON            "\033[?7h"
#define TERM_AUTOWRAP_ON_LEN        5
#define TERM_AUTOWRAP_OFF           "\033[?7l"
#define TERM_AUTOWRAP_OFF_LEN       5

#define TERM_SEND_ESC_SEQ(seq) fd_write (this->out_fd, seq, seq ## _LEN)

#define COLOR_FG_NORMAL   39

#define COLOR_RED       "\033[31m"
#define COLOR_GREEN     "\033[32m"

#define COLOR_FOCUS     COLOR_GREEN
#define COLOR_UNFOCUS   COLOR_RED

#define BACKSPACE_KEY   010
#define ESCAPE_KEY      033
#define ARROW_DOWN_KEY  0402
#define ARROW_UP_KEY    0403
#define ARROW_LEFT_KEY  0404
#define ARROW_RIGHT_KEY 0405
#define HOME_KEY        0406
#define FN_KEY(x)       (x + 0410)
#define DELETE_KEY      0512
#define INSERT_KEY      0513
#define PAGE_DOWN_KEY   0522
#define PAGE_UP_KEY     0523
#define END_KEY         0550

enum vt_keystate {
  norm,
  appl
};

typedef struct string_t {
  size_t
    mem_size,
    num_bytes;

  char *bytes;
} string_t;

typedef struct dirlist_t dirlist_t;

struct dirlist_t {
  char **list;
  int
    len,
    retval;

  size_t size;
  char dir[PATH_MAX];
  void (*free) (dirlist_t *dlist);
};

typedef struct tmpname_t tmpname_t;
struct tmpname_t {
  int fd;
  string_t *fname;
 };

typedef string_t *(*FrameProcessChar_cb) (vwm_frame *, string_t *, int);

struct vwm_frame {
  char
    **argv,
    mb_buf[8],
    tty_name[1024];

  uchar
    charset[2],
    textattr,
    saved_textattr;

  int
    fd,
    argc,
    logfd,
    state,
    status,
    mb_len,
    mb_curlen,
    col_pos,
    row_pos,
    new_rows,
    num_rows,
    num_cols,
    last_row,
    first_row,
    first_col,
    scroll_first_row,
    param_idx,
    at_frame,
    is_visible,
    remove_log,
    saved_row_pos,
    saved_col_pos,
    old_attribute,
    **colors,
    **videomem,
    *tabstops,
    *esc_param,
    *cur_param;

  utf8 mb_code;

  enum vt_keystate key_state;

  pid_t pid;

  string_t
    *logfile,
    *render;

  FrameProcessOutput_cb process_output_cb;
  FrameProcessChar_cb   process_char_cb;
  FrameUnimplemented_cb unimplemented_cb;
  FrameAtFork_cb        at_fork_cb;

  vwm_t   *root;
  vwm_win *parent;

  vwm_win_self   win;
  vwm_frame_self self;

  vwm_frame
    *next,
    *prev;
};

struct vwm_win {
  char
    *name;

  string_t
    *render,
    *separators_buf;

  int
    saved_row,
    saved_col,
    cur_row,
    cur_col,
    num_rows,
    num_cols,
    first_row,
    first_col,
    last_row,
    max_frames,
    num_visible_frames,
    draw_separators,
    num_separators,
    is_initialized;

  vwm_frame
    *head,
    *current,
    *tail,
    *last_frame;

  int
    cur_idx,
    length;

  vwm_t *parent;

  vwm_win
    *next,
    *prev;

  vwm_win_self self;
  vwm_frame_self frame;
};

struct vwm_prop {
  vwm_term  *term;

  char mode_key;

  string_t
    *shell,
    *editor,
    *tmpdir,
    *default_app,
    *sequences_fname,
    *unimplemented_fname;

  FILE
    *unimplemented_fp,
    *sequences_fp;

  int
    state,
    name_gen,
    num_rows,
    num_cols,
    need_resize,
    first_column;

  uint modes;

  vwm_win
    *head,
    *current,
    *tail,
    *last_win;

  int
    cur_idx,
    length;

  void *objects[NUM_OBJECTS];

  VwmOnTab_cb on_tab_cb;
  VwmRLine_cb rline_cb;
  VwmEditFile_cb edit_file_cb;

  int num_at_exit_cbs;
  VwmAtExit_cb *at_exit_cbs;
};

static void vwm_sigwinch_handler (int sig);

static const utf8 offsetsFromUTF8[6] = {
  0x00000000UL, 0x00003080UL, 0x000E2080UL,
  0x03C82080UL, 0xFA082080UL, 0x82082080UL
};

static utf8 ustring_to_code (char *buf, int *idx) {
  if (NULL is buf or 0 > *idx or 0 is buf[*idx])
    return 0;

  utf8 code = 0;
  int sz = 0;

  do {
    code <<= 6;
    code += (uchar) buf[(*idx)++];
    sz++;
  } while (buf[*idx] and IS_UTF8 (buf[*idx]));

  code -= offsetsFromUTF8[sz-1];

  return code;
}

static int ustring_charlen (uchar c) {
  if (c < 0x80) return 1;
  if ((c & 0xe0) == 0xc0) return 2;
  return 3 + ((c & 0xf0) != 0xe0);
}

static char *ustring_character (utf8 c, char *buf, int *len) {
  *len = 1;
  if (c < 0x80) {
    buf[0] = (char) c;
  } else if (c < 0x800) {
    buf[0] = (c >> 6) | 0xC0;
    buf[1] = (c & 0x3F) | 0x80;
    (*len)++;
  } else if (c < 0x10000) {
    buf[0] = (c >> 12) | 0xE0;
    buf[1] = ((c >> 6) & 0x3F) | 0x80;
    buf[2] = (c & 0x3F) | 0x80;
    (*len) += 2;
  } else if (c < 0x110000) {
    buf[0] = (c >> 18) | 0xF0;
    buf[1] = ((c >> 12) & 0x3F) | 0x80;
    buf[2] = ((c >> 6) & 0x3F) | 0x80;
    buf[3] = (c & 0x3F) | 0x80;
    (*len) += 3;
  } else
    return 0;

  buf[*len] = '\0';
  return buf;
}

static int dir_is_directory (const char *name) {
  struct stat st;

  if (-1 is stat (name, &st))
    return 0;

  return S_ISDIR(st.st_mode);
}

#define CONTINUE_ON_EXPECTED_ERRNO(fd__)          \
  if (errno == EINTR) continue;                   \
  if (errno == EAGAIN) {                          \
    struct timeval tv;                            \
    fd_set read_fd;                               \
    FD_ZERO(&read_fd);                            \
    FD_SET(fd, &read_fd);                         \
    tv.tv_sec = 0;                                \
    tv.tv_usec = 100000;                          \
    select (fd__ + 1, &read_fd, NULL, NULL, &tv); \
    continue;                                     \
   } do {} while (0)

static int fd_read (int fd, char *buf, size_t len) {
  if (1 > len)
    return NOTOK;

  char *s = buf;
  ssize_t bts;
  int tbts = 0;

  while (1) {
    if (NOTOK == (bts = read (fd, s, len))) {
      CONTINUE_ON_EXPECTED_ERRNO (fd);
      return NOTOK;
    }

    tbts += bts;
    if (tbts == (int) len || bts == 0)
      break;

    s += bts;
  }

  buf[tbts] = '\0';
  return bts;
}

static int fd_write (int fd, char *buf, size_t len) {
  int retval = len;
  int bts;

  while (len > 0) {
    if (NOTOK == (bts = write (fd, buf, len))) {
      CONTINUE_ON_EXPECTED_ERRNO (fd);
      return NOTOK;
    }

    len -= bts;
    buf += bts;
  }

  return retval;
}

static void fd_set_size (int fd, int rows, int cols) {
  struct winsize wsiz;
  wsiz.ws_row = rows;
  wsiz.ws_col = cols;
  wsiz.ws_xpixel = 0;
  wsiz.ws_ypixel = 0;
  ioctl (fd, TIOCSWINSZ, &wsiz);
}

static size_t byte_cp (char *dest, const char *src, size_t nelem) {
  const char *sp = src;
  size_t len = 0;

  while (len < nelem and *sp) { // this differs in memcpy()
    dest[len] = *sp++;
    len++;
  }

  return len;
}

static size_t cstring_cp (char *dest, size_t dest_len, const char *src, size_t nelem) {
  size_t num = (nelem > (dest_len - 1) ? dest_len - 1 : nelem);
  size_t len = (NULL is src ? 0 : byte_cp (dest, src, num));
  dest[len] = '\0';
  return len;
}

static int cstring_eq (const char *sa, const char *sb) {
  const uchar *spa = (const uchar *) sa;
  const uchar *spb = (const uchar *) sb;
  for (; *spa == *spb; spa++, spb++)
    if (*spa == 0) return 1;

  return 0;
}

static int cstring_cmp_n (const char *sa, const char *sb, size_t n) {
  const uchar *spa = (const uchar *) sa;
  const uchar *spb = (const uchar *) sb;
  for (;n--; spa++, spb++) {
    if (*spa != *spb)
      return (*(uchar *) spa - *(uchar *) spb);

    if (*spa == 0) return 0;
  }

  return 0;
}

static int cstring_eq_n  (const char *sa, const char *sb, size_t n) {
  return (0 == cstring_cmp_n (sa, sb, n));
}
static size_t string_align (size_t size) {
  size_t sz = 8 - (size % 8);
  sz = sizeof (char) * (size + (sz < 8 ? sz : 0));
  return sz;
}

/* this is not like realloc(), as size here is the extra size */
static string_t *string_reallocate (string_t *this, size_t size) {
  size_t sz = string_align (this->mem_size + size + 1);
  this->bytes = Realloc (this->bytes, sz);
  this->mem_size = sz;
  return this;
}

static void string_free (string_t *this) {
  if (this is NULL) return;
  if (this->mem_size) free (this->bytes);
  free (this);
}

static string_t *string_new (size_t size) {
  string_t *this = Alloc (sizeof (string_t));
  size_t sz = (size <= 0 ? 8 : string_align (size));
  this->bytes = Alloc (sz);
  this->mem_size = sz;
  this->num_bytes = 0;
  *this->bytes = '\0';
  return this;
}

static string_t *string_new_with_len (const char *bytes, size_t len) {
  string_t *new = Alloc (sizeof (string_t));
  size_t sz = string_align (len + 1);
  char *buf = Alloc (sz);
  byte_cp (buf, bytes, len);
  buf[len] = '\0';
  new->bytes = buf;
  new->num_bytes = len;
  new->mem_size = sz;
  return new;
}

static string_t *string_new_with (const char *bytes) {
  size_t len = (NULL is bytes ? 0 : bytelen (bytes));
  return string_new_with_len (bytes, len); /* this succeeds even if bytes is NULL */
}

static void string_release (string_t *this) {
  free (this->bytes);
  free (this);
}

static string_t *string_clear (string_t *this) {
  this->bytes[0] = '\0';
  this->num_bytes = 0;
  return this;
}

static string_t *string_clear_at (string_t *this, int idx) {
  if (0 > idx) idx += this->num_bytes;
  if (idx < 0) return this;
  if (idx > (int) this->num_bytes) idx = this->num_bytes;
  this->bytes[idx] = '\0';
  this->num_bytes = idx;
  return this;
}

static string_t *string_append_with_len (string_t *this, char *bytes, size_t len) {
  size_t bts = this->num_bytes + len;
  if (bts >= this->mem_size)
    this = string_reallocate (this, bts - this->mem_size + 1);

  byte_cp (this->bytes + this->num_bytes, bytes, len);
  this->num_bytes += len;
  this->bytes[this->num_bytes] = '\0';
  return this;
}

static string_t *string_append (string_t *this, char *bytes) {
  return string_append_with_len (this, bytes, bytelen (bytes));
}

static string_t *string_append_byte (string_t *this, char c) {
  int bts = this->mem_size - (this->num_bytes + 2);
  if (1 > bts) string_reallocate (this, 8);
  this->bytes[this->num_bytes++] = c;
  this->bytes[this->num_bytes] = '\0';
  return this;
}

static void dirlist_free (dirlist_t *dlist) {
  if (NULL is dlist->list)
    return;

  for (int i = 0; i < dlist->len; i++)
    free (dlist->list[i]);

  free (dlist->list);

  dlist->list = NULL;
}

static dirlist_t dir_list (char *dir) {
  dirlist_t dlist = {
    .len = 0,
    .size = 32,
    .retval = -1,
    .free = dirlist_free
    };

  ifnot (dir_is_directory (dir))
    return dlist;

  cstring_cp (dlist.dir, PATH_MAX, dir, PATH_MAX - 1);

  DIR *dh = NULL;
  struct dirent *dp;
  size_t len;

  dlist.list = Alloc (dlist.size * sizeof (char *));

  ifnull (dh = opendir (dir)) {
    dlist.retval = errno;
    return dlist;
  }

  while (1) {
    errno = 0;

    ifnull (dp = readdir (dh))
      break;

    len = bytelen (dp->d_name);

    if (len < 3 and dp->d_name[0] is '.')
      if (len is 1 or dp->d_name[1] is '.')
        continue;

    if ((size_t) dlist.len is dlist.size) {
      dlist.size = dlist.size * 2;
      dlist.list = Realloc (dlist.list, dlist.size * sizeof (char *));
    }

    dlist.list[dlist.len] = Alloc ((size_t) len + 1);
    cstring_cp (dlist.list[dlist.len], len + 1, dp->d_name, len);
    dlist.len++;
  }

  closedir (dh);
  dlist.retval = errno;
  return dlist;
}

/*
static void tmpfname_free (tmpname_t *t, int remove_fname) {
  if (NULL is t) return;

  if (remove_fname)
    ifnot (NULL is t->fname) {
      unlink (t->fname->bytes);

      string_free (t->fname);
      t->fname = NULL;
    }
}
*/

static tmpname_t tmpfname (char *dname, char *prefix) {
  static unsigned int see = 12252;
  tmpname_t t;
  t.fd = -1;
  t.fname = NULL;

  ifnot (dir_is_directory (dname))
    return t;

  char bpid[6];
  pid_t pid = getpid ();
  snprintf (bpid, 6, "%d", pid);

  int len = bytelen (dname) + bytelen (bpid) + bytelen (prefix) + 10;

  char name[len];
  snprintf (name, len, "%s/%s-%s.xxxxxx", dname, prefix, bpid);

  srand ((uint) time (NULL) + (uint) pid + see++);

  dirlist_t dlist = dir_list (dname);
  if (NOTOK is dlist.retval)
    return t;

  int
    found = 0,
    loops = 0,
    max_loops = 1024,
    inner_loops = 0,
    max_inner_loops = 1024;
  char c;

  while (1) {
again:
    found = 0;
    if (++loops is max_loops)
      goto theend;

    for (int i = 0; i < 6; i++) {
      inner_loops = 0;
      while (1) {
        if (++inner_loops is max_inner_loops)
          goto theend;

        c = (char) (rand () % 123);
        if ((c <= 'z' and c >= 'a') or (c >= '0' and c <= '9') or
            (c >= 'A' and c <= 'Z') or c is '_') {
          name[len - i - 2] = c;
          break;
        }
      }
    }

    for (int i = 0; i < dlist.len; i++)
      if (cstring_eq (name, dlist.list[i]))
        goto again;

    found = 1;
    break;
  }

  ifnot (found)
    goto theend;

  t.fd = open (name, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);

  if (-1 isnot t.fd) {
    if (-1 is fchmod (t.fd, 0600)) {
      close (t.fd);
      t.fd = -1;
      goto theend;
    }

    t.fname = string_new_with_len (name, len);
  }

theend:
  dlist.free (&dlist);
  return t;
}

static vwm_term *vwm_new_term (vwm_t *this) {
  ifnot (NULL is $my(term)) return $my(term);

  vwm_term *term = Alloc (sizeof (vwm_term));
  term->lines = 24;
  term->columns = 78;
  term->in_fd = STDIN_FILENO;
  term->out_fd = STDOUT_FILENO;
  term->mode = 'o';

  char *term_name = getenv ("TERM");
  if (NULL is term_name) {
    fprintf (stderr, "TERM environment variable isn't set\n");
    term->name = strdup ("vt100");
  } else {
    if (cstring_eq (term_name, "linux"))
      term->name = strdup ("linux");
    else if (cstring_eq_n (term_name, "xterm", 5))
      term->name = strdup ("xterm");
    else if (cstring_eq_n (term_name, "rxvt-unicode", 12))
      term->name = strdup ("xterm");
    else
      term->name = strdup (term_name);
  }

  $my(term) = term;

  return term;
}

static void term_release (vwm_term **thisp) {
  if (NULL is *thisp) return;
  free ((*thisp)->name);
  free (*thisp);
  *thisp = NULL;
}

static int term_sane_mode (vwm_term *this) {
  if (this->mode == 's') return OK;
  if (isnotatty (this->in_fd)) return NOTOK;

  struct termios mode;
  while (NOTOK  == tcgetattr (this->in_fd, &mode))
    if (errno == EINTR) return NOTOK;

  mode.c_iflag |= (BRKINT|INLCR|ICRNL|IXON|ISTRIP);
  mode.c_iflag &= ~(IGNBRK|INLCR|IGNCR|IXOFF);
  mode.c_oflag |= (OPOST|ONLCR);
  mode.c_lflag |= (ECHO|ECHOE|ECHOK|ECHOCTL|ISIG|ICANON|IEXTEN);
  mode.c_lflag &= ~(ECHONL|NOFLSH|TOSTOP|ECHOPRT);
  mode.c_cc[VEOF] = 'D'^64; // splitvt
  mode.c_cc[VMIN] = 1;   /* 0 */
  mode.c_cc[VTIME] = 0;  /* 1 */

  while (NOTOK == tcsetattr (this->in_fd, TCSAFLUSH, &mode))
    if (errno == EINTR) return NOTOK;

  this->mode = 's';
  return OK;
}

static int term_orig_mode (vwm_term *this) {
  if (this->mode == 'o') return OK;
  if (isnotatty (this->in_fd)) return NOTOK;

  while (NOTOK == tcsetattr (this->in_fd, TCSAFLUSH, &this->orig_mode))
    ifnot (errno == EINTR) return NOTOK;

  this->mode = 'o';

  return OK;
}

static int term_raw_mode (vwm_term *this) {
  if (this->mode == 'r') return OK;
  if (isnotatty (this->in_fd)) return NOTOK;

  while (NOTOK == tcgetattr (this->in_fd, &this->orig_mode))
    if (errno == EINTR) return NOTOK;

  this->raw_mode = this->orig_mode;
  this->raw_mode.c_iflag &= ~(INLCR|ICRNL|IXON|ISTRIP);
  this->raw_mode.c_cflag |= (CS8);
  this->raw_mode.c_oflag &= ~(OPOST);
  this->raw_mode.c_lflag &= ~(ECHO|ISIG|ICANON|IEXTEN);
  this->raw_mode.c_lflag &= NOFLSH;
  this->raw_mode.c_cc[VEOF] = 1;
  this->raw_mode.c_cc[VMIN] = 0;   /* 1 */
  this->raw_mode.c_cc[VTIME] = 1;  /* 0 */

  while (NOTOK == tcsetattr (this->in_fd, TCSAFLUSH, &this->raw_mode))
    ifnot (errno == EINTR) return NOTOK;

  this->mode = 'r';
  return OK;
}

static int term_cursor_get_ptr_pos (vwm_term *this, int *row, int *col) {
  if (NOTOK == TERM_SEND_ESC_SEQ (TERM_GET_PTR_POS))
    return NOTOK;

  char buf[32];
  uint i = 0;
  int bts;
  while (i < sizeof (buf) - 1) {
    if (NOTOK == (bts = fd_read (this->in_fd, buf + i, 1)) ||
         bts == 0)
      return NOTOK;

    if (buf[i] == 'R') break;
    i++;
  }

  buf[i] = '\0';

  if (buf[0] != ESCAPE_KEY || buf[1] != '[' ||
      2 != sscanf (buf + 2, "%d;%d", row, col))
    return NOTOK;

  return OK;
}

static void term_cursor_set_ptr_pos (vwm_term *this, int row, int col) {
  char ptr[32];
  snprintf (ptr, 32, TERM_GOTO_PTR_POS_FMT, row, col);
  fd_write (this->out_fd, ptr, bytelen (ptr));
}

static void term_screen_clear (vwm_term *this) {
  TERM_SEND_ESC_SEQ (TERM_SCREEN_CLEAR);
}

static void term_screen_save (vwm_term *this) {
  TERM_SEND_ESC_SEQ (TERM_SCREEN_SAVE);
}

static void term_screen_restore (vwm_term *this) {
  TERM_SEND_ESC_SEQ (TERM_SCROLL_RESET);
  TERM_SEND_ESC_SEQ (TERM_SCREEN_RESTORE);
}

static void term_init_size (vwm_term *this, int *rows, int *cols) {
  struct winsize wsiz;

  do {
    if (OK == ioctl (this->out_fd, TIOCGWINSZ, &wsiz)) {
      this->lines = (int) wsiz.ws_row;
      this->columns = (int) wsiz.ws_col;
      *rows = this->lines; *cols = this->columns;
      return;
    }
  } while (errno == EINTR);

  int orig_row, orig_col;
  term_cursor_get_ptr_pos (this, &orig_row, &orig_col);

  TERM_SEND_ESC_SEQ (TERM_LAST_RIGHT_CORNER);
  term_cursor_get_ptr_pos (this, rows, cols);
  term_cursor_set_ptr_pos (this, orig_row, orig_col);
}

static void vwm_set_at_exit_cb (vwm_t *this, VwmAtExit_cb cb) {
  if (NULL is cb) return;

  $my(num_at_exit_cbs)++;

  ifnot ($my(num_at_exit_cbs) - 1)
    $my(at_exit_cbs) = Alloc (sizeof (VwmAtExit_cb));
  else
    $my(at_exit_cbs) = Realloc ($my(at_exit_cbs), sizeof (VwmAtExit_cb) * $my(num_at_exit_cbs));

  $my(at_exit_cbs)[$my(num_at_exit_cbs) -1] = cb;
}

static void vwm_set_size (vwm_t *this, int rows, int cols, int first_col) {
  $my(num_rows) = rows;
  $my(num_cols) = cols;
  $my(first_column) = first_col;
}

static void vwm_set_term  (vwm_t *this, vwm_term *term) {
  $my(term) = term;
}

static void vwm_set_state (vwm_t *this, int state) {
  $my(state) = state;
}

static void vwm_set_editor (vwm_t *this, char *editor) {
  if (NULL is editor) return;
  size_t len = bytelen (editor);
  ifnot (len) return;
  string_clear ($my(editor));
  string_append_with_len ($my(editor), editor, len);
}

static void vwm_set_default_app (vwm_t *this, char *app) {
  if (NULL is app) return;
  size_t len = bytelen (app);
  ifnot (len) return;
  string_clear ($my(default_app));
  string_append_with_len ($my(default_app), app, len);
}

static void vwm_set_on_tab_cb (vwm_t *this, VwmOnTab_cb cb) {
  $my(on_tab_cb) = cb;
}

static void vwm_set_object (vwm_t *this, void *object, int idx) {
  if (idx >= NUM_OBJECTS or idx < 0) return;
  $my(objects)[idx] = object;
}

static void vwm_set_rline_cb (vwm_t *this, VwmRLine_cb cb) {
  $my(rline_cb) = cb;
}

static void vwm_set_edit_file_cb (vwm_t *this, VwmEditFile_cb cb) {
  $my(edit_file_cb) = cb;
}

static void vwm_set_shell (vwm_t *this, char *shell) {
  if (NULL is shell) return;
  size_t len = bytelen (shell);
  ifnot (len) return;
  string_clear ($my(shell));
  string_append_with_len ($my(shell), shell, len);
}

static void vwm_set_debug_unimplemented (vwm_t *this, char *fname) {
  self(unset.debug.unimplemented);

  if (NULL is fname) {
    tmpname_t t = tmpfname (self(get.tmpdir),
        V_STR_FMT_LEN (64, "%d_unimplemented", getpid ()));

    if (-1 is t.fd) return;

    $my(unimplemented_fp) = fdopen (t.fd, "w+");
    $my(unimplemented_fname) = t.fname;

    return;
  }

  $my(unimplemented_fname) = string_new_with (fname);
  $my(unimplemented_fp) = fopen (fname, "w");
}

static void vwm_unset_debug_unimplemented (vwm_t *this) {
  if (NULL is $my(unimplemented_fp)) return;
  fclose ($my(unimplemented_fp));
  $my(unimplemented_fp) = NULL;
  string_free ($my(unimplemented_fname));
}

static void vwm_set_debug_sequences (vwm_t *this, char *fname) {
  self(unset.debug.sequences);

  if (NULL is fname) {
    tmpname_t t = tmpfname (self(get.tmpdir),
        V_STR_FMT_LEN (64, "%d_sequences", getpid ()));

    if (-1 is t.fd) return;

    $my(sequences_fp) = fdopen (t.fd, "w+");
    $my(sequences_fname) = t.fname;

    return;
  }

  $my(sequences_fname) = string_new_with (fname);
  $my(sequences_fp) = fopen (fname, "w");
}

static void vwm_unset_debug_sequences (vwm_t *this) {
  if (NULL is $my(sequences_fp)) return;
  fclose ($my(sequences_fp));
  $my(sequences_fp) = NULL;
  string_free ($my(sequences_fname));
}

static void vwm_unset_tmpdir (vwm_t *this) {
  string_free ($my(tmpdir));
  $my(tmpdir) = NULL;
}

static int vwm_set_tmpdir (vwm_t *this, char *dir, size_t len) {
  if (NULL is $my(tmpdir))
    $my(tmpdir) = string_new (32);

  string_clear ($my(tmpdir));

  if (NULL is dir)
    string_append ($my(tmpdir), TMPDIR);
  else
    string_append_with_len ($my(tmpdir), dir, len);

  string_append_byte ($my(tmpdir), '/');
  string_append ($my(tmpdir), V_STR_FMT_LEN (64, "%d-vwm_tmpdir", getpid ()));

  if (-1 is access ($my(tmpdir)->bytes, F_OK)) {
    if (-1 is mkdir ($my(tmpdir)->bytes, S_IRWXU))
      goto theerror;
  } else {
    ifnot (dir_is_directory ($my(tmpdir)->bytes))
      goto theerror;

    if (-1 is access ($my(tmpdir)->bytes, W_OK|R_OK|X_OK))
      goto theerror;
  }

  return OK;

theerror:
  self(unset.tmpdir);
  return NOTOK;
}

/* This is an extended version of the same function of the kilo editor at:
 * https://github.com/antirez/kilo.git
 *
 * It should work the same, under xterm, rxvt-unicode, st and linux terminals.
 * It also handles UTF8 byte sequences and it should return the integer represantation
 * of such sequence
 */

static utf8 vwm_getkey (vwm_t *this, int infd) {
  (void) this;
  char c;
  int n;
  char buf[5];

  while (0 == (n = fd_read (infd, buf, 1)));

  if (n == -1) return -1;

  c = buf[0];

  switch (c) {
    case ESCAPE_KEY:
      if (0 == fd_read (infd, buf, 1))
        return ESCAPE_KEY;

      /* recent (revailed through CTRL-[other than CTRL sequence]) and unused */
      if ('z' >= buf[0] && buf[0] >= 'a')
        return 0;

      if (buf[0] == ESCAPE_KEY /* probably alt->arrow-key */)
        if (0 == fd_read (infd, buf, 1))
          return 0;

      if (buf[0] != '[' && buf[0] != 'O')
        return 0;

      if (0 == fd_read (infd, buf + 1, 1))
        return ESCAPE_KEY;

      if (buf[0] == '[') {
        if ('0' <= buf[1] && buf[1] <= '9') {
          if (0 == fd_read (infd, buf + 2, 1))
            return ESCAPE_KEY;

          if (buf[2] == '~') {
            switch (buf[1]) {
              case '1': return HOME_KEY;
              case '2': return INSERT_KEY;
              case '3': return DELETE_KEY;
              case '4': return END_KEY;
              case '5': return PAGE_UP_KEY;
              case '6': return PAGE_DOWN_KEY;
              case '7': return HOME_KEY;
              case '8': return END_KEY;
              default: return 0;
            }
          } else if (buf[1] == '1') {
            if (fd_read (infd, buf, 1) == 0)
              return ESCAPE_KEY;

            switch (buf[2]) {
              case '1': return FN_KEY(1);
              case '2': return FN_KEY(2);
              case '3': return FN_KEY(3);
              case '4': return FN_KEY(4);
              case '5': return FN_KEY(5);
              case '7': return FN_KEY(6);
              case '8': return FN_KEY(7);
              case '9': return FN_KEY(8);
              default: return 0;
            }
          } else if (buf[1] == '2') {
            if (fd_read (infd, buf, 1) == 0)
              return ESCAPE_KEY;

            switch (buf[2]) {
              case '0': return FN_KEY(9);
              case '1': return FN_KEY(10);
              case '3': return FN_KEY(11);
              case '4': return FN_KEY(12);
              default: return 0;
            }
          } else { /* CTRL_[key other than CTRL sequence] */
                   /* lower case */
            if (buf[2] == 'h')
              return INSERT_KEY; /* sample/test (logically return 0) */
            else
              return 0;
          }
        } else if (buf[1] == '[') {
          if (fd_read (infd, buf, 1) == 0)
            return ESCAPE_KEY;

          switch (buf[0]) {
            case 'A': return FN_KEY(1);
            case 'B': return FN_KEY(2);
            case 'C': return FN_KEY(3);
            case 'D': return FN_KEY(4);
            case 'E': return FN_KEY(5);

            default: return 0;
          }
        } else {
          switch (buf[1]) {
            case 'A': return ARROW_UP_KEY;
            case 'B': return ARROW_DOWN_KEY;
            case 'C': return ARROW_RIGHT_KEY;
            case 'D': return ARROW_LEFT_KEY;
            case 'H': return HOME_KEY;
            case 'F': return END_KEY;
            case 'P': return DELETE_KEY;

            default: return 0;
          }
        }
      } else if (buf[0] == 'O') {
        switch (buf[1]) {
          case 'A': return ARROW_UP_KEY;
          case 'B': return ARROW_DOWN_KEY;
          case 'C': return ARROW_RIGHT_KEY;
          case 'D': return ARROW_LEFT_KEY;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
          case 'P': return FN_KEY(1);
          case 'Q': return FN_KEY(2);
          case 'R': return FN_KEY(3);
          case 'S': return FN_KEY(4);

          default: return 0;
        }
      }
    break;

  default:
    if (c < 0) {
      int len = ustring_charlen ((uchar) c);
      utf8 code = 0;
      code += (uchar) c;

      int idx;
      int invalid = 0;
      char cc;

      for (idx = 0; idx < len - 1; idx++) {
        if (0 >= fd_read (infd, &cc, 1))
          return -1;

        if (isnotutf8 ((uchar) cc)) {
          invalid = 1;
        } else {
          code <<= 6;
          code += (uchar) cc;
        }
      }

      if (invalid)
        return -1;

      code -= offsetsFromUTF8[len-1];
      return code;
    }

    if (127 == c) return BACKSPACE_KEY;

    return c;
  }

  return -1;
}

static vwm_win *vwm_set_current_at (vwm_t *this, int idx) {
  vwm_win *cur_win = $my(current);
  if (INDEX_ERROR isnot DListSetCurrent ($myprop, idx)) {
    if (NULL isnot cur_win)
      $my(last_win) = cur_win;
  }
  return $my(current);
}

static void vwm_set_mode_key (vwm_t *this, char c) {
  $my(mode_key) = c;
}

static char vwm_get_mode_key (vwm_t *this) {
  return $my(mode_key);
}

static int vwm_get_lines (vwm_t *this) {
  return $my(term)->lines;
}

static int vwm_get_columns (vwm_t *this) {
  return $my(term)->columns;
}

static void *vwm_get_object (vwm_t *this, int idx) {
  if (idx >= NUM_OBJECTS or idx < 0) return NULL;
  return $my(objects)[idx];
}

static char *vwm_get_tmpdir (vwm_t *this) {
  ifnot (NULL is $my(tmpdir))
    return $my(tmpdir)->bytes;

  return TMPDIR;
}

static int vwm_get_num_wins (vwm_t *this) {
  return $my(length);
}

static int vwm_get_win_idx (vwm_t *this, vwm_win *win) {
  int idx = DListGetIdx ($myprop, vwm_win, win);
  if (idx is INDEX_ERROR)
    return NOTOK;

  return idx;
}

static vwm_win *vwm_get_current_win (vwm_t *this) {
  return $my(current);
}

static vwm_frame *vwm_get_current_frame (vwm_t *this) {
  return $my(current)->current;
}

static vwm_term *vwm_get_term (vwm_t *this) {
  return $my(term);
}

static int vwm_get_state (vwm_t *this) {
  return $my(state);
}

static char *vwm_get_shell (vwm_t *this) {
  return $my(shell)->bytes;
}

static char *vwm_get_editor (vwm_t *this) {
  return $my(editor)->bytes;
}

static char *vwm_get_default_app (vwm_t *this) {
  return $my(default_app)->bytes;
}

static void frame_release_info (vframe_info *finfo) {
  if (NULL is finfo) return;
  free (finfo);
  finfo = NULL;
}

static void win_release_info (vwin_info *winfo) {
  if (NULL is winfo) return;

  for (int fidx = 0; fidx < winfo->num_frames; fidx++)
    frame_release_info (winfo->frames[fidx++]);

  free (winfo->frames);
  free (winfo);
  winfo = NULL;
}

static void vwm_release_info (vwm_t *this, vwm_info **vinfop) {
  (void) this;
  if (*vinfop is NULL) return;

  vwm_info *vinfo = *vinfop;

  for (int widx = 0; widx < vinfo->num_win; widx++)
    win_release_info (vinfo->wins[widx++]);

  free (vinfo->wins);
  free (vinfo);
  *vinfop = NULL;
}

static vframe_info *frame_get_info (vwm_frame *this) {
  vframe_info *finfo = Alloc (sizeof (vframe_info));
  finfo->pid = this->pid;
  finfo->first_row = this->first_row;
  finfo->num_rows = this->num_rows;
  finfo->last_row = this->first_row + this->num_rows - 1;
  finfo->is_visible = this->is_visible;
  finfo->is_current = this->parent->current is this;
  finfo->at_frame = (this->is_visible ? this->at_frame : -1);
  finfo->logfile = (NULL is this->logfile ? "" : this->logfile->bytes);

  int arg = 0;
  for (; arg < this->argc; arg++)
    finfo->argv[arg] = this->argv[arg];
  finfo->argv[arg] = NULL;

  return finfo;
}

static vwin_info *win_get_info (vwm_win *this) {
  vwm_t *vwm = this->parent;

  vwin_info *winfo = Alloc (sizeof (vwin_info));
  winfo->name = this->name;
  winfo->num_rows = this->num_rows;
  winfo->num_cols = this->num_cols;
  winfo->num_visible_frames = self(get.num_visible_frames);
  winfo->num_frames = this->length;
  winfo->cur_frame_idx = this->cur_idx;
  winfo->is_current = Vwm.get.current_win (vwm) is this;

  winfo->frames = Alloc (sizeof (vframe_info) * this->length);
  vwm_frame *frame = this->head;
  int fidx = 0;

  while (frame and fidx < this->length) {
    winfo->frames[fidx++] = frame_get_info (frame);
    frame = frame->next;
  }

  return winfo;
}

static vwm_info *vwm_get_info (vwm_t *this) {
  vwm_info *vinfo = Alloc (sizeof (vwm_info));
  vinfo->pid = getpid ();
  vinfo->num_win = $my(length);
  vinfo->cur_win_idx = $my(cur_idx);
  vinfo->sequences_fname = (NULL is $my(sequences_fname) ? "" :
      $my(sequences_fname)->bytes);
  vinfo->unimplemented_fname = (NULL is $my(unimplemented_fname) ? "" :
      $my(unimplemented_fname)->bytes);

  vinfo->wins = Alloc (sizeof (vwin_info *) * $my(length));
  vwm_win *win = $my(head);
  int idx = 0;
  while (win and idx < vinfo->num_win) {
    vinfo->wins[idx++] = win_get_info (win);
    win = win->next;
  }

  return vinfo;
}

static vwm_win *vwm_pop_win_at (vwm_t *this, int idx) {
  return DListPopAt ($myprop, vwm_win, idx);
}

static int **vwm_alloc_ints (int rows, int cols, int val) {
  int **obj = Alloc (rows * sizeof (int *));

  for (int i = 0; i < rows; i++) {
    obj[i] = Alloc (sizeof (int *) * cols);

    for (int j = 0; j < cols; j++)
      obj[i][j] = val;
 }

 return obj;
}

static int vt_video_line_to_str (int *line, char *buf, int len) {
  int idx = 0;
  utf8 c;

  for (int i = 0; i < len; i++) {
    c = line[i];

    ifnot (c) continue;

    if ((c & 0xFF) < 0x80)
      buf[idx++] = c & 0xFF;
    else if (c < 0x800) {
      buf[idx++] = (c >> 6) | 0xC0;
      buf[idx++] = (c & 0x3F) | 0x80;
    } else if (c < 0x10000) {
      buf[idx++] = (c >> 12) | 0xE0;
      buf[idx++] = ((c >> 6) & 0x3F) | 0x80;
      buf[idx++] = (c & 0x3F) | 0x80;
    } else if (c < 0x110000) {
      buf[idx++] = (c >> 18) | 0xF0;
      buf[idx++] = ((c >> 12) & 0x3F) | 0x80;
      buf[idx++] = ((c >> 6) & 0x3F) | 0x80;
      buf[idx++] = (c & 0x3F) | 0x80;
    } else {
      continue;
    }
  }

  buf[idx++] = '\n';

  buf[idx] = '\0';

  return idx;
}

static void vt_write (char *buf, FILE *fp) {
  fprintf (fp, "%s", buf);
  fflush (fp);
}

static string_t *vt_insline (string_t *buf, int num) {
  return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%dL", num));
}

static string_t *vt_insertchar (string_t *buf, int numcols) {
  return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%d@", numcols));
}

static string_t *vt_savecursor (string_t *buf) {
  return string_append_with_len (buf, "\0337", 2);
}

static string_t *vt_restcursor (string_t *buf) {
  return string_append_with_len (buf, "\0338", 2);
}

static string_t *vt_clreol (string_t *buf) {
  return string_append_with_len (buf, "\033[K", 3);
}

static string_t *vt_clrbgl (string_t *buf) {
  return string_append_with_len (buf, "\033[1K", 4);
}

static string_t *vt_clrline (string_t *buf) {
  return string_append_with_len (buf, "\033[2K", 4);
}

static string_t *vt_delunder (string_t *buf, int num) {
  return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%dP", num));
}

static string_t *vt_delline (string_t *buf, int num) {
  return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%dM", num));
}

static string_t *vt_attr_reset (string_t *buf) {
  return string_append_with_len (buf, "\033[m", 3);
}

static string_t *vt_reverse (string_t *buf, int on) {
  return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%sm", (on ? "7" : "27")));
}

static string_t *vt_underline (string_t *buf, int on) {
  return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%sm", (on ? "4" : "24")));
}

static string_t *vt_bold (string_t *buf, int on) {
  return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%sm", (on ? "1" : "22")));
}

static string_t *vt_italic (string_t *buf, int on) {
  return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%sm", (on ? "3" : "23")));
}

static string_t *vt_blink (string_t *buf, int on) {
  return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%sm", (on ? "5" : "25")));
}

static string_t *vt_bell (string_t *buf) {
  return string_append_with_len (buf, "\007", 1);
}

static string_t *vt_setfg (string_t *buf, int color) {
  return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%d;1m", color));
}

static string_t *vt_setbg (string_t *buf, int color) {
  return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%d;1m", color));
}

static string_t *vt_left (string_t *buf, int count) {
  return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%dD", count));
}

static string_t *vt_right (string_t *buf, int count) {
  return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%dC", count));
}

static string_t *vt_up (string_t *buf, int numrows) {
  return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%dA", numrows));
}

static string_t *vt_down (string_t *buf, int numrows) {
  return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%dB", numrows));
}

static string_t *vt_irm (string_t *buf) {
  return string_append_with_len (buf, "\033[4l", 4);
}

static string_t *vt_revscroll (string_t *buf) {
  return string_append_with_len (buf, "\033M", 2);
}

static string_t *vt_setscroll (string_t *buf, int first, int last) {
  if (0 is first and 0 is last)
    return string_append_with_len (buf, "\033[r", 3);
  else
    return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%d;%dr", first, last));
}

static string_t *vt_goto (string_t *buf, int row, int col) {
  return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%d;%dH", row, col));
}

static string_t *vt_attr_check (string_t *buf, int pixel, int lastattr, uchar *currattr) {
  uchar
    simplepixel,
    lastpixel,
    change,
    selected,
    reversed;

 /* Set the simplepixel REVERSE bit if SELECTED ^ REVERSE */
  simplepixel = ((pixel  >> 8) & (~SELECTED)  & (~REVERSE));
  selected    = (((pixel >> 8) & (~SELECTED)) ? 1 : 0);
  reversed    = (((pixel >> 8) & (~REVERSE))  ? 1 : 0);

  if (selected ^ reversed)
    simplepixel |= REVERSE;

  /* Set the lastpixel REVERSE bit if SELECTED ^ REVERSE */
  lastpixel = ((lastattr  >> 8) & (~SELECTED)  & (~REVERSE));
  selected  = (((lastattr >> 8) & (~SELECTED)) ? 1 : 0);
  reversed  = (((lastattr >> 8) & (~REVERSE))  ? 1 : 0);

  if (selected ^ reversed)
    lastpixel |= REVERSE;

 /* Thanks to Dan Dorough for the XOR code */
checkchange:
  change = (lastpixel ^ simplepixel);
  if (change) {
    if (change & REVERSE) {
      if ((*currattr) & REVERSE) {
#define GOTO_HACK          /* vt_reverse (0) doesn't work on xterms? */
#ifdef  GOTO_HACK          /* This goto hack resets all current attributes */
        vt_attr_reset (buf);
        *currattr &= ~REVERSE;
        simplepixel = 0;
        lastpixel &= (~REVERSE);
        goto checkchange;
#else
        vt_reverse (buf, 0);
        *currattr &= ~REVERSE;
#endif
      } else {
        vt_reverse (buf, 1);
        *currattr |= REVERSE;
      }
    }

    if (change & BOLD) {
      if ((*currattr) & BOLD) {
        vt_bold (buf, 0);
        *currattr &= ~BOLD;
      } else {
        vt_bold (buf, 1);
        *currattr |= BOLD;
      }
    }

    if (change & ITALIC) {
      if ((*currattr) & ITALIC) {
        vt_italic (buf, 0);
        *currattr &= ~ITALIC;
      } else {
        vt_italic (buf, 1);
        *currattr |= ITALIC;
      }
    }

    if (change & UNDERLINE) {
      if ((*currattr) & UNDERLINE) {
        vt_underline (buf, 0);
        *currattr &= ~UNDERLINE;
      } else {
        vt_underline (buf, 1);
        *currattr |= UNDERLINE;
      }
    }

    if (change & BLINK) {
      if ((*currattr) & BLINK) {
        vt_blink (buf, 0);
        *currattr &= ~BLINK;
      } else {
        vt_blink (buf, 1);
        *currattr |= BLINK;
      }
    }
  }

  return buf;
}

static string_t *vt_attr_set (string_t *buf, int textattr) {
  vt_attr_reset (buf);

  if (textattr & BOLD)
    vt_bold (buf, 1);

  if (textattr & UNDERLINE)
    vt_underline (buf, 1);

  if (textattr & BLINK)
    vt_blink (buf, 1);

  if (textattr & REVERSE)
    vt_reverse (buf, 1);

  if (textattr & ITALIC)
    vt_italic (buf, 1);

  return buf;
}

static void vt_video_add (vwm_frame *frame, utf8 c) {
  frame->videomem[frame->row_pos - 1][frame->col_pos - 1] = c;
  frame->videomem[frame->row_pos - 1][frame->col_pos - 1] |=
      (((int) frame->textattr) << 8);
}

static void vt_video_erase (vwm_frame *frame, int x1, int x2, int y1, int y2) {
  int i, j;

  for (i = x1 - 1; i < x2; ++i)
    for (j = y1 - 1; j < y2; ++j) {
      frame->videomem[i][j] = 0;
      frame->colors  [i][j] = COLOR_FG_NORM;
    }
}

static void vt_frame_video_rshift (vwm_frame *frame, int numcols) {
  for (int i = frame->num_cols - 1; i > frame->col_pos - 1; --i) {
    if (i - numcols >= 0) {
      frame->videomem[frame->row_pos-1][i] = frame->videomem[frame->row_pos-1][i-numcols];
      frame->colors[frame->row_pos-1][i] = frame->colors[frame->row_pos-1][i-numcols];
    } else {
      frame->videomem[frame->row_pos-1][i] = 0;
      frame->colors [frame->row_pos-1][i] = COLOR_FG_NORM;
    }
  }
}

static string_t *vt_frame_ech (vwm_frame *frame, string_t *buf, int num_cols) {
  for (int i = 0; i + frame->col_pos <= frame->num_cols and i < num_cols; i++) {
    frame->videomem[frame->row_pos-1][frame->col_pos - i - 1] = 0;
    frame->colors[frame->row_pos-1][frame->col_pos - i - 1] = COLOR_FG_NORM;
  }

  return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%dX", num_cols));
}

/*
static string_t *vt_frame_cha (vwm_frame *frame, string_t *buf, int param) {
  if (param < 2)
    frame->col_pos = 1;
  else {
    if (param > frame->num_cols)
      param = frame->num_cols;
    frame->col_pos = param;
  }

  return string_append (buf, V_STR_FMT_LEN (MAX_SEQ_LEN, "\033[%dG", param));
}
*/

static void vt_frame_video_scroll (vwm_frame *frame, int numlines) {
  int *tmpvideo;
  int *tmpcolors;
  int n;

  for (int i = 0; i < numlines; i++) {
    tmpvideo = frame->videomem[frame->scroll_first_row - 1];
    tmpcolors = frame->colors[frame->scroll_first_row - 1];

    ifnot (NULL is frame->logfile) {
      char buf[(frame->num_cols * 3) + 2];
      int len = vt_video_line_to_str (tmpvideo, buf, frame->num_cols);
      fd_write (frame->logfd, buf, len);
    }

    for (int j = 0; j < frame->num_cols; j++) {
      tmpvideo[j] = 0;
      tmpcolors[j] = COLOR_FG_NORM;
    }

    for (n = frame->scroll_first_row - 1; n < frame->last_row - 1; n++) {
      frame->videomem[n] = frame->videomem[n + 1];
      frame->colors[n] = frame->colors[n + 1];
    }

    frame->videomem[n] = tmpvideo;
    frame->colors[n] = tmpcolors;
  }
}

static void vt_frame_video_scroll_back (vwm_frame *frame, int numlines) {
  if (frame->row_pos < frame->scroll_first_row)
    return;

  int n;
  int *tmpvideo;
  int *tmpcolors;

  for (int i = 0; i < numlines; i++) {
    tmpvideo = frame->videomem[frame->last_row - 1];
    tmpcolors = frame->colors[frame->last_row - 1];

    for (int j = 0; j < frame->num_cols; j++) {
      tmpvideo[j] = 0;
      tmpcolors[j] = COLOR_FG_NORM;
    }

   for (n = frame->last_row - 1; n > frame->scroll_first_row - 1; --n) {
      frame->videomem[n] = frame->videomem[n - 1];
      frame->colors[n] = frame->colors[n - 1];
    }

    frame->videomem[n] = tmpvideo;
    frame->colors[n] = tmpcolors;
  }
}

static string_t *vt_frame_attr_set (vwm_frame *frame, string_t *buf) {
  uchar on = NORMAL;
  vt_attr_reset (buf);
  return vt_attr_check (buf, 0, frame->textattr, &on);
}

static string_t *vt_append (vwm_frame *frame, string_t *buf, utf8 c) {
  if (frame->col_pos > frame->num_cols) {
    if (frame->row_pos < frame->last_row)
      frame->row_pos++;
    else
      vt_frame_video_scroll (frame, 1);

    string_append (buf, "\r\n");
    frame->col_pos = 1;
  }

  vt_video_add (frame, c);

  if (frame->mb_len)
    string_append_with_len (buf, frame->mb_buf, frame->mb_len);
  else
    string_append_byte (buf, c);

  frame->col_pos++;
  return buf;
}

static string_t *vt_keystate_print (string_t *buf, int application) {
  if (application)
    return string_append (buf, "\033=\033[?1h");

  return string_append (buf, "\033>\033[?1l");
}

static string_t *vt_altcharset (string_t *buf, int charset, int type) {
  switch (type) {
    case UK_CHARSET:
      string_append_with_len (buf, V_STR_FMT_LEN (4, "\033%cA", (charset is G0 ? '(' : ')')), 3);
      break;

    case US_CHARSET:
      string_append_with_len (buf, V_STR_FMT_LEN (4, "\033%cB", (charset is G0 ? '(' : ')')), 3);
      break;

    case GRAPHICS:
      string_append_with_len (buf, V_STR_FMT_LEN (4, "\033%c0", (charset is G0 ? '(' : ')')), 3);
      break;

    default:  break;
  }
  return buf;
}

static string_t *vt_esc_scan (vwm_frame *, string_t *, int);

static void vt_frame_esc_set (vwm_frame *frame) {
  frame->process_char_cb = vt_esc_scan;

  for (int i = 0; i < MAX_PARAMS; i++)
    frame->esc_param[i] = 0;

  frame->param_idx = 0;
  frame->cur_param = &frame->esc_param[frame->param_idx];
}

static void frame_reset (vwm_frame *frame) {
  frame->row_pos = 1;
  frame->col_pos = 1;
  frame->saved_row_pos = 1;
  frame->saved_col_pos = 1;
  frame->scroll_first_row = 1;
  frame->last_row = frame->num_rows;
  frame->key_state = norm;
  frame->textattr = NORMAL;
  frame->saved_textattr = NORMAL;
  frame->charset[G0] = US_CHARSET;
  frame->charset[G1] = US_CHARSET;
  vt_frame_esc_set (frame);
}

static string_t *vt_esc_brace_q (vwm_frame *frame, string_t *buf, int c) {
  if (ISDIGIT (c)) {
    *frame->cur_param *= 10;
    *frame->cur_param += (c - '0');
    return buf;
  }

  /* Return inside the switch to prevent reset_esc() */
  switch (c) {
    case '\030': /* Processed as escape cancel */
    case '\032': vt_frame_esc_set (frame);
    return buf;

    case 'h': /* Set modes */
      switch (frame->esc_param[0]) {
        case 1: /* Cursorkeys in application mode */
          frame->key_state = appl;
          vt_keystate_print (buf, frame->key_state);
          break;

        case 7:
          string_append_with_len (buf, TERM_AUTOWRAP_ON, TERM_AUTOWRAP_ON_LEN);
          break;

        case 25:
          string_append_with_len (buf, TERM_CURSOR_SHOW, TERM_CURSOR_SHOW_LEN);
          break;

        case 47:
          string_append_with_len (buf, TERM_SCREEN_SAVE, TERM_SCREEN_SAVE_LEN);
          break;

        case 2: /* Set ansi mode */
        case 3: /* 132 char/row */
        case 4: /* Set jump scroll */
        case 5: /* Set reverse screen */
        case 6: /* Set relative coordinates */
        case 8: /* Set auto repeat on */
        default:
          frame->unimplemented_cb (frame, __func__, c, frame->esc_param[0]);
          break;
      }
      break;

  case 'l': /* Reset modes */
    switch (frame->esc_param[0]) {
      case 1: /* Cursorkeys in normal mode */
        frame->key_state = norm;
        vt_keystate_print (buf, frame->key_state);
        break;

        case 7:
          string_append_with_len (buf, TERM_AUTOWRAP_OFF, TERM_AUTOWRAP_OFF_LEN);
          break;

      case 25:
        string_append_with_len (buf, TERM_CURSOR_HIDE, TERM_CURSOR_HIDE_LEN);
        break;

      case 47:
        string_append_with_len (buf, TERM_SCREEN_RESTORE, TERM_SCREEN_RESTORE_LEN);
        break;

      case 2: /* Set VT52 mode */
      case 3:
      case 4: /* Set smooth scroll */
      case 5: /* Set non-reversed (normal) screen */
      case 6: /* Set absolute coordinates */
      case 8: /* Set auto repeat off */
      default:
        frame->unimplemented_cb (frame, __func__, c, frame->esc_param[0]);
        break;
    }
    break;

    default:
      break;
   }

  vt_frame_esc_set (frame);
  return buf;
}

static string_t *vt_esc_lparen (vwm_frame *frame, string_t *buf, int c) {
  /* Return inside the switch to prevent reset_esc() */
  switch (c) {
    case '\030': /* Processed as escape cancel */
    case '\032':
      vt_frame_esc_set (frame);
      break;

    /* Select character sets */
    case 'A': /* UK as G0 */
      frame->charset[G0] = UK_CHARSET;
      vt_altcharset (buf, G0, UK_CHARSET);
      break;

    case 'B': /* US as G0 */
      frame->charset[G0] = US_CHARSET;
      vt_altcharset (buf, G0, US_CHARSET);
      break;

    case '0': /* Special character set as G0 */
      frame->charset[G0] = GRAPHICS;
      vt_altcharset (buf, G0, GRAPHICS);
      break;

    case '1': /* Alternate ROM as G0 */
    case '2': /* Alternate ROM special character set as G0 */
    default:
      frame->unimplemented_cb (frame, __func__, c, frame->esc_param[0]);
      break;
  }

  vt_frame_esc_set (frame);
  return buf;
}

static string_t *vt_esc_rparen (vwm_frame *frame, string_t *buf, int c) {
  switch (c) {
    case '\030': /* Processed as escape cancel */
    case '\032':
      break;

    /* Select character sets */
    case 'A':
      frame->charset[G1] = UK_CHARSET;
      vt_altcharset (buf, G1, UK_CHARSET);
      break;

    case 'B':
      frame->charset[G1] = US_CHARSET;
      vt_altcharset (buf, G1, US_CHARSET);
      break;

    case '0':
      frame->charset[G1] = GRAPHICS;
      vt_altcharset (buf, G1, GRAPHICS);
      break;

    case '1': /* Alternate ROM as G1 */
    case '2': /* Alternate ROM special character set as G1 */
    default:
      frame->unimplemented_cb (frame, __func__, c, frame->esc_param[0]);
      break;
  }

  vt_frame_esc_set (frame);
  return buf;
}

static string_t *vt_esc_pound (vwm_frame *frame, string_t *buf, int c) {
  switch (c)   /* Line attributes not supported */ {
    case '3':  /* Double height (top half) */
    case '4':  /* Double height (bottom half) */
    case '5':  /* Single width, single height */
    case '6':  /* Double width */
    default:
      frame->unimplemented_cb (frame, __func__, c, frame->esc_param[0]);
      vt_frame_esc_set (frame);
      break;
  }

  return buf;
}

static string_t *vt_process_m (vwm_frame *frame, string_t *buf, int c) {
  int idx;

  switch (c) {
    case 0: /* Turn all attributes off */
      frame->textattr = NORMAL;
      vt_attr_reset (buf);
      idx = frame->num_cols - frame->col_pos - 1;
      if (0 <= idx)
        for (int i = 0; i < idx; i++)
          frame->colors[frame->row_pos -1][frame->col_pos - 1 + i] = COLOR_FG_NORM;
      break;

    case 1:
      frame->textattr |= BOLD;
      vt_bold (buf, 1);
      break;

    case 2: /* Half brightness */
      frame->unimplemented_cb (frame, __func__, c, frame->esc_param[0]);
      break;

    case 3:
      frame->textattr |= ITALIC;
      vt_italic (buf, 1);
      break;

    case 4:
      frame->textattr |= UNDERLINE;
      vt_underline (buf, 1);
      break;

    case 5:
      frame->textattr |= BLINK;
      vt_blink (buf, 1);
      break;

    case 7:
      frame->textattr |= REVERSE;
      vt_reverse (buf, 1);
      break;

    case 21: /* Normal brightness */
      frame->unimplemented_cb (frame, __func__, c, frame->esc_param[0]);
      break;

    case 22:
      frame->textattr &= ~BOLD;
      vt_bold (buf, 0);
      break;

    case 23:
      frame->textattr &= ~ITALIC;
      vt_italic (buf, 0);
      break;

    case 24:
      frame->textattr &= ~UNDERLINE;
      vt_underline (buf, 0);
      break;

    case 25:
      frame->textattr &= ~BLINK;
      vt_blink (buf, 0);
      break;

    case 27:
      frame->textattr &= ~REVERSE;
      vt_reverse (buf, 0);
      break;

    case 39:
      c = 30;
    case 30 ... 37:
      vt_setfg (buf, c);
      idx = frame->num_cols - frame->col_pos + 1;
      if (0 < idx)
        for (int i = 0; i < idx; i++)
          frame->colors[frame->row_pos - 1][frame->col_pos - 1 + i] = c;

      break;

    case 49:
      c = 47;
    case 40 ... 47:
      vt_setbg (buf, c);
      break;

    default:
      frame->unimplemented_cb (frame, __func__, c, frame->esc_param[0]);
      break;
  }

  return buf;
}

static string_t *vt_esc_brace (vwm_frame *frame, string_t *buf, int c) {
  int
    i,
    newx,
    newy;

  char reply[128];

  if (ISDIGIT (c)) {
    *frame->cur_param *= 10;
    *frame->cur_param += (c - '0');
    return buf;
  }

   /* Return inside the switch to prevent reset_esc() */
  switch (c) {
    case '\030': /* Processed as escape cancel */
    case '\032':
      break;

    case '?': /* Format should be \E[?<n> */
      if (*frame->cur_param) {

frame->unimplemented_cb (frame, "brace why", c, frame->esc_param[0]);
        vt_frame_esc_set (frame);
      } else {
        frame->process_char_cb = vt_esc_brace_q;
      }

      return buf;

    case ';':
      if (frame->param_idx + 1 < MAX_PARAMS)
        frame->cur_param = &frame->esc_param[++frame->param_idx];
      return buf;

    case 'h': /* Set modes */
      switch (frame->esc_param[0]) {
        case 2:  /* Lock keyboard */
        case 4:  /* Character insert mode */
        case 12: /* Local echo on */
        case 20: /* <Return> = CR */
        default:
          frame->unimplemented_cb (frame, __func__, c, frame->esc_param[0]);
          break;
      }
      break;

    case 'l': /* Reset modes */
      switch (frame->esc_param[0]) {
        case 4:  /* Character overstrike mode */
          vt_irm (buf); /* (ADDITION - unverified) */
          break;

        case 2:  /* Unlock keyboard */
        case 12: /* Local echo off */
        case 20: /* <Return> = CR-LF */
        default:
          frame->unimplemented_cb (frame, __func__, c, frame->esc_param[0]);
          break;
      }
      break;

    case 'r': /* Set scroll region */
      if (!frame->esc_param[0] and !frame->esc_param[1]) {
        frame->scroll_first_row = 1;
        frame->last_row = frame->num_rows;
      } else {
        /* Check parameters: VERY important. :) */
        if (frame->esc_param[0] < 1) /* Not needed */
          frame->scroll_first_row = 1;
        else
          frame->scroll_first_row = frame->esc_param[0];

        if (frame->esc_param[1] > frame->num_rows)
          frame->last_row = frame->num_rows;
        else
           frame->last_row = frame->esc_param[1];

        if (frame->scroll_first_row > frame->last_row) {
          /* Reset scroll region */
          frame->scroll_first_row = 1;
          frame->last_row = frame->num_rows;
        }
      }

      frame->row_pos = 1;
      frame->col_pos = 1;

      vt_setscroll (buf, frame->scroll_first_row + frame->first_row - 1,
        frame->last_row + frame->first_row - 1);
      vt_goto (buf, frame->row_pos + frame->first_row - 1, 1);
      break;

    case 'A': /* Cursor UP */
      if (frame->row_pos is frame->first_row)
        break;

      ifnot (frame->esc_param[0])
        frame->esc_param[0] = 1;

      newx = (frame->row_pos - frame->esc_param[0]);

      if (newx > frame->scroll_first_row) {
        frame->row_pos = newx;
        vt_up (buf, frame->esc_param[0]);
      } else {
        frame->row_pos = frame->scroll_first_row;
        vt_goto (buf, frame->row_pos + frame->first_row - 1,
          frame->col_pos);
      }
      break;

    case 'B': /* Cursor DOWN */
      if (frame->row_pos is frame->last_row)
        break;

      ifnot (frame->esc_param[0])
        frame->esc_param[0] = 1;

      newx = frame->row_pos + frame->esc_param[0];

      if (newx <= frame->last_row) {
        frame->row_pos = newx;
        vt_down (buf, frame->esc_param[0]);
      } else {
        frame->row_pos = frame->last_row;
        vt_goto (buf, frame->row_pos + frame->first_row - 1,
          frame->col_pos);
      }
      break;

    case 'C': /* Cursor RIGHT */
      if (frame->col_pos is frame->num_cols)
        break;

      ifnot (frame->esc_param[0])
        frame->esc_param[0] = 1;

      newy = (frame->col_pos + frame->esc_param[0]);

      if (newy < frame->num_cols) {
        frame->col_pos = newy;

        vt_right (buf, frame->esc_param[0]);
      } else {
        frame->col_pos = frame->num_cols;
        vt_goto (buf, frame->row_pos + frame->first_row - 1,
          frame->col_pos);
      }
      break;

    case 'D': /* Cursor LEFT */
      if (frame->col_pos is 1)
        break;

      ifnot (frame->esc_param[0])
        frame->esc_param[0] = 1;

      newy = (frame->col_pos - frame->esc_param[0]);

      if (newy > 1) {
        frame->col_pos = newy;
        vt_left (buf, frame->esc_param[0]);
      } else {
        frame->col_pos = 1;
        string_append (buf, "\r");
      }

      break;

    case 'G': { /* (hpa - horizontal) (ADDITION) */
      int col = frame->esc_param[0];
      int row = frame->esc_param[1];
      if (row <= 1) row = frame->row_pos;
      frame->esc_param[0] = row;
      frame->esc_param[1] = col;
    }

    case 'd': /* (vpa - HVP) (ADDITION) */
      frame->unimplemented_cb (frame, "ADDI param[0]", c, frame->esc_param[0]);
      frame->unimplemented_cb (frame, "ADDI param[1]", c, frame->esc_param[1]);
      frame->unimplemented_cb (frame, "ADDI row_pos", c, frame->row_pos);
      frame->unimplemented_cb (frame, "ADDI col_pos", c, frame->col_pos);

    case 'f':
    case 'H': /* Move cursor to coordinates */
      ifnot (frame->esc_param[0])
        frame->esc_param[0] = 1;

      ifnot (frame->esc_param[1])
        frame->esc_param[1] = 1;

      if ((frame->row_pos = frame->esc_param[0]) >
          frame->num_rows)
        frame->row_pos = frame->num_rows;

      if ((frame->col_pos = frame->esc_param[1]) >
          frame->num_cols)
        frame->col_pos = frame->num_cols;

      vt_goto (buf, frame->row_pos + frame->first_row - 1,
        frame->col_pos);
      break;

    case 'g': /* Clear tabstops */
      switch (frame->esc_param[0]) {
        case 0: /* Clear a tabstop */
          frame->tabstops[frame->col_pos-1] = 0;
          break;

        case 3: /* Clear all tabstops */
          for (newy = 0; newy < frame->num_cols; ++newy)
            frame->tabstops[newy] = 0;
          break;

        default:
          frame->unimplemented_cb (frame, __func__, c, frame->esc_param[0]);
          break;
      }
      break;

    case 'm': /* Set terminal attributes */
      vt_process_m (frame, buf, frame->esc_param[0]);
      for (i = 1; frame->esc_param[i] and i < MAX_PARAMS; i++)
        vt_process_m (frame, buf, frame->esc_param[i]);
      break;

    case 'J': /* Clear screen */
      switch (frame->esc_param[0]) {
        case 0: /* Clear from cursor down */
          vt_video_erase (frame, frame->row_pos,
            frame->num_rows, 1, frame->num_cols);

          newx = frame->row_pos;
          vt_savecursor (buf);
          string_append (buf, "\r");

          while (newx++ < frame->num_rows) {
            vt_clreol (buf);
            string_append (buf, "\n");
          }

          vt_clreol (buf);
          vt_restcursor (buf);
          break;

        case 1: /* Clear from cursor up */
          vt_video_erase (frame, 1, frame->row_pos,
            1, frame->num_cols);

          newx = frame->row_pos;
          vt_savecursor (buf);
          string_append (buf, "\r");

          while (--newx > 0) {
            vt_clreol (buf);
            vt_up (buf, 1);
          }

          vt_clreol (buf);
          vt_restcursor (buf);
          break;

        case 2: /* Clear whole screen */
          vt_video_erase (frame, 1, frame->num_rows,
            1, frame->num_cols);

          vt_goto (buf, frame->first_row + 1 - 1, 1);
          frame->row_pos = 1;
          frame->col_pos = 1;
          newx = frame->row_pos;
          vt_savecursor (buf);
          string_append (buf, "\r");

          while (newx++ < frame->num_rows) {
            vt_clreol (buf);
            string_append (buf, "\n");
          }

          vt_clreol (buf);
          vt_restcursor (buf);
          break;

        default:
          frame->unimplemented_cb (frame, __func__, c, frame->esc_param[0]);
          break;
      }
      break;

    case 'K': /* Clear line */
      switch (frame->esc_param[0]) {
        case 0: /* Clear to end of line */
          vt_video_erase (frame, frame->row_pos,
            frame->row_pos, frame->col_pos, frame->num_cols);

          vt_clreol (buf);
        break;

        case 1: /* Clear to beginning of line */
          vt_video_erase (frame, frame->row_pos,
            frame->row_pos, 1, frame->col_pos);

          vt_clrbgl (buf);
          break;

        case 2: /* Clear whole line */
          vt_video_erase (frame, frame->row_pos,
            frame->row_pos, 1, frame->num_cols);

          vt_clrline (buf);
          break;
        }
      break;

    case 'P': /* Delete under cursor */
      vt_video_erase (frame, frame->row_pos,
        frame->row_pos, frame->col_pos, frame->col_pos);

      vt_delunder (buf, frame->esc_param[0]);
      break;

    case 'M': /* Delete lines */
      vt_frame_video_scroll_back (frame, 1);
      vt_delline (buf, frame->esc_param[0]);
      break;

    case 'L': /* Insert lines */
      vt_insline (buf, frame->esc_param[0]);
      break;

    case '@': /* Insert characters */
      ifnot (frame->esc_param[0])
        frame->esc_param[0] = 1;

      vt_insertchar (buf, frame->esc_param[0]);
      vt_frame_video_rshift (frame, frame->esc_param[0]);
      break;

    case 'i': /* Printing */
      frame->unimplemented_cb (frame, __func__, c, frame->esc_param[0]);
      break;

    case 'n': /* Device status request */
      switch (frame->esc_param[0]) {
        case 5: /* Status report request */
          /* Say we're just fine. */
          write (frame->fd, "\033[0n", 4);
          break;

        case 6: /* Cursor position request */
          sprintf (reply, "\033[%d;%dR", frame->row_pos,
              frame->col_pos);

          write (frame->fd, reply, bytelen (reply));
          break;
        }
        break;

    case 'c': /* Request terminal identification string_t */
      /* Respond with "I am a vt102" */
      write (frame->fd, "\033[?6c", 5);
      break;

    case 'X': /* (ECH) Erase param chars (ADDITION) */
      vt_frame_ech (frame, buf, frame->esc_param[0]);
      break;

    //case 'G':
       /* (CHA) Cursor to column param (ADDITION) */
     // vt_frame_cha (frame, buf, frame->esc_param[0]);
     // break;

    default:
      frame->unimplemented_cb (frame, __func__, c, frame->esc_param[0]);
      break;
  }

  vt_frame_esc_set (frame);
  return buf;
}

static string_t *vt_esc_e (vwm_frame *frame, string_t *buf, int c) {
  /* Return inside the switch to prevent reset_esc() */
  switch (c) {
    case '\030': /* Processed as escape cancel */
    case '\032':
      break;

    case '[':
      frame->process_char_cb = vt_esc_brace;
      return buf;

    case '(':
      frame->process_char_cb = vt_esc_lparen;
      return buf;

    case ')':
      frame->process_char_cb = vt_esc_rparen;
      return buf;

    case '#':
      frame->process_char_cb = vt_esc_pound;
      return buf;

    case 'D': /* Cursor down with scroll up at margin */
      if (frame->row_pos < frame->last_row)
        frame->row_pos++;
      else
        vt_frame_video_scroll (frame, 1);

      string_append (buf, "\n");
      break;

    case 'M': /* Reverse scroll (move up; scroll at top) */
      if (frame->row_pos > frame->scroll_first_row)
        --frame->row_pos;
      else
        vt_frame_video_scroll_back (frame, 1);

      vt_revscroll (buf);
      break;

    case 'E': /* Next line (CR-LF) */
      if (frame->row_pos < frame->last_row)
        frame->row_pos++;
      else
        vt_frame_video_scroll (frame, 1);

      frame->col_pos = 1;
      string_append (buf, "\r\n");
      break;

    case '7': /* Save cursor and attribute */
      frame->saved_row_pos = frame->row_pos;
      frame->saved_col_pos = frame->col_pos;
      frame->saved_textattr = frame->textattr;
      break;

    case '8': /* Restore saved cursor and attribute */
      frame->row_pos = frame->saved_row_pos;
      frame->col_pos = frame->col_pos;
      if (frame->row_pos > frame->num_rows)
        frame->row_pos = frame->num_rows;

      if (frame->col_pos > frame->num_cols)
        frame->col_pos = frame->num_cols;

      vt_goto (buf, frame->row_pos + frame->first_row - 1,
        frame->col_pos);

      frame->textattr = frame->saved_textattr;
      vt_frame_attr_set (frame, buf);
      break;

    case '=': /* Set application keypad mode */
      frame->key_state = appl;
      vt_keystate_print (buf, frame->key_state);
      break;

    case '>': /* Set numeric keypad mode */
      frame->key_state = norm;
      vt_keystate_print (buf, frame->key_state);
      break;

    case 'N': /* Select charset G2 for one character */
    case 'O': /* Select charset G3 for one character */
      frame->unimplemented_cb (frame, __func__, c, frame->esc_param[0]);
      break;

    case 'H': /* Set horizontal tab */
      frame->tabstops[frame->col_pos - 1] = 1;
      break;

    case 'Z': /* Request terminal identification string_t */
      /* Respond with "I am a vt102" */
      write (frame->fd, "\033[?6c", 5);
      break;

    case 'c': /* Terminal reset */
      frame_reset (frame);
      break;

    default:
      frame->unimplemented_cb (frame, __func__, c, frame->esc_param[0]);
      break;
  }

  vt_frame_esc_set (frame);
  return buf;
}

static string_t *vt_esc_scan (vwm_frame *frame, string_t *buf, int c) {
  switch (c) {
    case '\000': /* NULL (fill character) */
      break;

    case '\003': /* EXT  (half duplex turnaround) */
    case '\004': /* EOT  (can be disconnect char) */
    case '\005': /* ENQ  (generate answerback) */
      string_append_byte (buf, c);
      break;

    case '\007': /* BEL  (sound terminal bell) */
      vt_bell (buf);
      break;

    case '\b': /* Backspace; move left one character */
      ifnot (1 is frame->col_pos) {
        --frame->col_pos;
        vt_left (buf, frame->esc_param[0]);
      }
      break;

    case '\t': /* Tab.  Handle with direct motion (Buggy) */
      {
        int i = frame->col_pos;
        do {
          ++frame->col_pos;
        } while (0 is frame->tabstops[frame->col_pos - 1]
            and (frame->col_pos < frame->num_cols));

        vt_right (buf, frame->col_pos - i);
      }
      break;

    case '\013': /* Processed as linefeeds */
    case '\014': /* Don't let the cursor move below winow or scrolling region */
    case '\n':
      if (frame->row_pos < frame->last_row)
        frame->row_pos++;
      else
        vt_frame_video_scroll (frame, 1);

      string_append (buf, "\n");
      break;

    case '\r': /* Move cursor to left margin */
      frame->col_pos = 1;
      string_append (buf, "\r");
      break;

    case '\016': /* S0 (selects G1 charset) */
    case '\017': /* S1 (selects G0 charset) */
    case '\021': /* XON (continue transmission) */
    case '\022': /* XOFF (stop transmission) */
      string_append_byte (buf, c);
      break;

    case '\030': /* Processed as escape cancel */
    case '\032':
      vt_frame_esc_set (frame);
      break;

    case '\033':
      frame->process_char_cb = vt_esc_e;
      return buf;

    default:
      if (c >= 0x80 or frame->mb_len) {
        if (frame->mb_len > 0) {
          frame->mb_buf[frame->mb_curlen++] = c;
          frame->mb_code <<= 6;
          frame->mb_code += c;

          if (frame->mb_curlen isnot frame->mb_len)
            return buf;

          frame->mb_code -= offsetsFromUTF8[frame->mb_len-1];

          vt_append (frame, buf, frame->mb_code);
          frame->mb_buf[0] = '\0';
          frame->mb_curlen = frame->mb_len = frame->mb_code = 0;
          return buf;
        } else {
          frame->mb_code = c;
          frame->mb_len = ({
            uchar uc = 0;
            if ((c & 0xe0) is 0xc0)
              uc = 2;
            else if ((c & 0xf0) is 0xe0)
              uc = 3;
            else if ((c & 0xf8) is 0xf0)
              uc = 4;
            else
              uc = -1;

            uc;
            });
          frame->mb_buf[0] = c;
          frame->mb_curlen = 1;
          return buf;
        }
      } else
        vt_append (frame, buf, c);
  }

  return buf;
}

static void vt_video_add_log_lines (vwm_frame *this) {
  struct stat st;
  if (-1 is this->logfd or -1 is fstat (this->logfd, &st))
    return;

  long size = st.st_size;

  char *mbuf = mmap (0, size, PROT_READ, MAP_SHARED, this->logfd, 0);

  if (NULL is mbuf) return;

  char *buf = mbuf + size - 1;

  int lines = this->num_rows;

  for (int i = 0; i < lines; i++)
    for (int j = 0; j < this->num_cols; j++)
      this->videomem[i][j] = 0;

  while (lines isnot 0 and size) {
    char b[BUFSIZE];
    char c;
    int rbts = 0;
    while (--size) {
      c = *--buf;

      if (c is '\n') break;

      ifnot (c) continue;
      b[rbts++] = c;
    }

    b[rbts] = '\0';

    int blen = bytelen (b);

    char nbuf[blen + 1];
    for (int i = 0; i < blen; i++)
      nbuf[i] = b[blen - i - 1];

    nbuf[blen] = '\0';

    int idx = 0;
    for (int i = 0; i < this->num_cols; i++) {
      if (idx >= blen) break;

      this->videomem[lines-1][i] =
         (int) ustring_to_code (nbuf, &idx);
    }

    lines--;
  }

  ftruncate (this->logfd, size);
  lseek (this->logfd, size, SEEK_SET);
  munmap (0, st.st_size);
}

static void frame_on_resize (vwm_frame *this, int rows, int cols) {
  int **videomem = vwm_alloc_ints (rows, cols, 0);
  int **colors = vwm_alloc_ints (rows, cols, COLOR_FG_NORMAL);
  int row_pos = 0;
  int i, j, nj, ni;

  int last_row = this->num_rows;
  if (rows < last_row)
    while (last_row > rows and 0 is this->videomem[last_row-1][0])
      last_row--;

  for (i = last_row, ni = rows; i and ni; i--, ni--) {
    if (this->row_pos is i)
      if ((row_pos = i + (this->num_rows - last_row) + rows - this->num_rows) < 1)
        row_pos = 1;

    for (j = 0, nj = 0; (j < this->num_cols) and (nj < cols); j++, nj++) {
      videomem[ni-1][nj] = this->videomem[i-1][j];
      colors[ni-1][nj] = this->colors[i-1][j];
    }
  }

  ifnot (row_pos) /* We never reached the old cursor */
    row_pos = 1;

  this->row_pos = row_pos;
  this->col_pos = (this->col_pos > cols ? cols : this->col_pos);

  for (i = 0; i < this->num_rows; i++)
    free (this->videomem[i]);
  free (this->videomem);

  for (i = 0; i < this->num_rows; i++)
    free (this->colors[i]);
  free (this->colors);

  this->videomem = videomem;
  this->colors = colors;
}

static void win_set_frame (vwm_win *this, vwm_frame *frame) {
  string_clear (frame->render);

  vt_setscroll (frame->render, frame->scroll_first_row + frame->first_row - 1,
      frame->last_row + frame->first_row - 1);

  if (this->draw_separators) {
    this->draw_separators = 0;
    string_append_with_len (frame->render, this->separators_buf->bytes, this->separators_buf->num_bytes);
  }

  vt_goto (frame->render, frame->row_pos + frame->first_row - 1, frame->col_pos);

  ifnot (NULL is this->last_frame) {
    ifnot (frame->key_state is this->last_frame->key_state)
      vt_keystate_print (frame->render, frame->key_state);

    ifnot (frame->textattr is this->last_frame->textattr)
      vt_attr_set (frame->render, frame->textattr);

    for (int i = 0; i < NCHARSETS; i++)
      if (frame->charset[i] isnot this->last_frame->charset[i])
        vt_altcharset (frame->render, i, frame->charset[i]);
  }

  vt_write (frame->render->bytes, stdout);
}

static void frame_process_output (vwm_frame *this, char *buf, int len) {
  this->process_output_cb (this, buf, len);
}

#ifndef DEBUG
static void frame_process_output_cb (vwm_frame *this, char *buf, int len) {
  string_clear (this->render);

  while (len--)
    this->process_char_cb (this, this->render, (uchar) *buf++);

  vt_write (this->render->bytes, stdout);
}
#else
static void frame_process_output_cb (vwm_frame *this, char *buf, int len) {
  string_clear (this->render);

  FILE *fout = this->root->prop->sequences_fp;

  fprintf (fout, "\n%s\n\n", buf);

  char seq_buf[MAX_SEQ_LEN + 1];
  int seq_idx = 1;
  seq_buf[0] = '\0';

  while (len--) {

    if (this->process_char_cb is vt_esc_scan) {
      ifnot (seq_idx) goto proceed;

      fprintf (fout, "ESC %s\n", seq_buf);

      seq_idx = 0;
      memset (seq_buf, 0, MAX_SEQ_LEN+1);
    } else
      seq_buf[seq_idx++] = *buf;

proceed:
    this->process_char_cb (this, this->render, (uchar) *buf++);
  }

  if (seq_idx)
    fprintf (fout, "ESC %s\n", seq_buf);

  fflush (fout);

  vt_write (this->render->bytes, stdout);
}
#endif /* DEBUG */

static void argv_release (char **argv, int *argc) {
  for (int i = 0; i <= *argc; i++) free (argv[i]);
  free (argv);
  *argc = 0;
  argv = NULL;
}

static void frame_release_argv (vwm_frame *this) {
  if (NULL is this->argv) return;
  argv_release (this->argv, &this->argc);
}

static char **parse_command (char *command, int *argc) {
  char *sp = command;
  char *tokbeg;
  size_t len;

  *argc = 0;
  char **argv = Alloc (sizeof (char *));

  while (*sp) {
    while (*sp and *sp is ' ') sp++;
    ifnot (*sp) break;

    tokbeg = sp;

    if (*sp is '"') {
      sp++;
      tokbeg++;

parse_quoted:
      while (*sp and *sp isnot '"') sp++;
      ifnot (*sp) goto theerror;
      if (*(sp - 1) is '\\') goto parse_quoted;
      len = (size_t) (sp - tokbeg);
      sp++;
      goto add_arg;
    }

    while (*sp and *sp isnot ' ') sp++;

    len = (size_t) (sp - tokbeg);

add_arg:
    *argc += 1;
    argv = Realloc (argv, sizeof (char *) * ((*argc) + 1));
    argv[*argc - 1] = Alloc (len + 1);
    cstring_cp (argv[*argc - 1], len + 1, tokbeg, len);

    ifnot (*sp) break;
    sp++;
  }

  argv[*argc] = (char *) NULL;

  return argv;

theerror:
  argv_release (argv, argc);
  return NULL;
}

static void frame_set_command (vwm_frame *this, char *command) {
  if (NULL is command or 0 is bytelen (command))
    return;

  self(release_argv);
  this->argv = parse_command (command, &this->argc);
}

static void frame_set_visibility (vwm_frame *this, int visibility) {
  if (this->is_visible) {
    ifnot (visibility) {
      this->parent->num_visible_frames--;
      this->parent->num_separators--;
    }
  } else {
    if (visibility) {
      this->parent->num_visible_frames++;
      this->parent->num_separators++;
    }
  }

  this->is_visible = (0 isnot visibility);
}

static void frame_set_argv (vwm_frame *this, int argc, char **argv) {
  if (argc <= 0) return;

  self(release_argv);

  this->argv = Alloc (sizeof (char *) * (argc + 1));
  for (int i = 0; i < argc; i++) {
    size_t len = bytelen (argv[i]);
    this->argv[i] = Alloc (len + 1);
    cstring_cp (this->argv[i], len + 1, argv[i], len);
  }

  this->argv[argc] = NULL;
  this->argc = argc;
}

static void frame_set_fd (vwm_frame *this, int fd) {
  this->fd = fd;
}

static int frame_get_fd (vwm_frame *this) {
  return this->fd;
}

static pid_t frame_get_pid (vwm_frame *this) {
  return this->pid;
}

static int frame_get_visibility (vwm_frame *this) {
  return this->is_visible;
}

static int frame_get_argc (vwm_frame *this) {
  return this->argc;
}

static char **frame_get_argv (vwm_frame *this) {
  return this->argv;
}

static vwm_win *frame_get_parent (vwm_frame *this) {
  return this->parent;
}

static vwm_t *frame_get_root (vwm_frame *this) {
  return this->root;
}

static int frame_get_num_rows (vwm_frame *this) {
  return this->num_rows;
}

static int frame_get_logfd (vwm_frame *this) {
  return this->logfd;
}

static char *frame_get_logfile (vwm_frame *this) {
  if (NULL is this->logfile) return NULL;
  return this->logfile->bytes;
}

static void frame_clear (vwm_frame *this, int state) {
  if (NULL is this) return;

  string_t *render = this->render;

  string_clear (render);
  vt_goto (render, this->first_row, 1);

  for (int i = 0; i < this->num_rows; i++) {
    for (int j = 0; j < this->num_cols; j++) {
      if (state & VFRAME_CLEAR_VIDEO_MEM) {
        this->videomem[i][j] = ' ';
        this->colors[i][j] = COLOR_FG_NORMAL;
      }

      string_append_byte (render, ' ');
    }

    string_append (render, "\r\n");
  }

  // clear the last newline, otherwise it scrolls one line more
  string_clear_at (render, -1); // this is visible when there is one frame

  if (state & VFRAME_CLEAR_LOG)
    if (this->logfd isnot -1)
      ftruncate (this->logfd, 0);

  vt_write (render->bytes, stdout);
}

static int frame_check_pid (vwm_frame *this) {
  if (NULL is this or -1 is this->pid) return -1;

  ifnot (0 is waitpid (this->pid, &this->status, WNOHANG)) {
    this->pid = -1;
    this->fd = -1;
    int state = (VFRAME_CLEAR_VIDEO_MEM|
      (this->logfd isnot -1 ?
        (this->remove_log ? VFRAME_CLEAR_LOG : 0) :
        0));
    self(clear, state);
    return 0;
  }

  return this->pid;
}

static int frame_kill_proc (vwm_frame *this) {
  if (this is NULL or this->pid is -1) return -1;

  int state = (VFRAME_CLEAR_VIDEO_MEM|
    (this->logfd isnot -1 ?
      (this->remove_log ? VFRAME_CLEAR_LOG : 0) :
      0));
  self(clear, state);

  kill (this->pid, SIGHUP);
  waitpid (this->pid, NULL, 0);
  this->pid = -1;
  this->fd = -1;
  return 0;
}

static FrameProcessOutput_cb frame_set_process_output_cb (vwm_frame *this, FrameProcessOutput_cb cb) {
  FrameProcessOutput_cb prev = this->process_output_cb;
  this->process_output_cb = cb;
  return prev;
}

static FrameAtFork_cb frame_set_at_fork_cb (vwm_frame *this, FrameAtFork_cb cb) {
  FrameAtFork_cb prev = this->at_fork_cb;
  this->at_fork_cb = cb;
  return prev;
}

static void frame_set_unimplemented_cb (vwm_frame *this, FrameUnimplemented_cb cb) {
  this->unimplemented_cb = cb;
}

static int frame_set_log (vwm_frame *this, char *fname, int remove_log) {
  self(release_log);

  if (NULL is fname) {
    tmpname_t t = tmpfname (this->root->prop->tmpdir->bytes, "libvwm");
    if (-1 is t.fd)
      return NOTOK;

    this->logfd = t.fd;
    this->logfile = t.fname;
    this->remove_log = remove_log;

    return this->logfd;
  }

  this->logfd = open (fname, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);

  if (this->logfd is NOTOK) return NOTOK;

  if (-1 is fchmod (this->logfd, 0600)) return NOTOK;

  this->logfile = string_new_with (fname);
  this->remove_log = remove_log;
  return this->logfd;
}

static int frame_at_fork_default_cb (vwm_frame *this, vwm_t *root, vwm_win *parent) {
  (void) this; (void) parent; (void) root;
  return 1;
}

static void frame_unimplemented_default_cb (vwm_frame *this, const char *fun, int c, int param) {
  if (this->root->prop->unimplemented_fp isnot NULL) {
    fprintf (this->root->prop->unimplemented_fp, "|%s| %c %d| param: %d\n", fun, c, c, param);
    fflush  (this->root->prop->unimplemented_fp);
  }
}

static int win_insert_frame_at (vwm_win *this, vwm_frame *frame, int idx) {
  return DListInsertAt (this, frame, idx);
}

static int win_append_frame (vwm_win *this, vwm_frame *frame) {
  return DListAppend (this, frame);
}

static vwm_frame *win_init_frame (vwm_win *this, frame_opts opts) {
  vwm_frame *frame = Alloc (sizeof (vwm_frame));

  frame->parent = (opts.parent isnot NULL ? opts.parent : this);

  if (frame->parent isnot NULL) {
    frame->root = frame->parent->parent;
    frame->win =  frame->parent->self;
    frame->self = frame->parent->frame;
  }

  frame->pid = opts.pid;
  frame->fd = opts.fd;
  frame->at_frame = opts.at_frame;
  frame->logfile = NULL;
  frame->remove_log = opts.remove_log;
  frame->is_visible = opts.is_visible;
  frame->num_rows = opts.num_rows;
  frame->first_row = opts.first_row;

  frame->num_cols = (opts.num_cols isnot -1 ? opts.num_cols :
    (NULL is this ? 78 : this->num_cols));
  frame->first_col = (opts.first_col isnot -1 ? opts.first_col :
    (NULL is this ? 1 : this->first_col));

  ifnot (NULL is opts.argv)
    Vframe.set.argv (frame, opts.argc, opts.argv);
  else
    if (NULL isnot opts.command)
      Vframe.set.command (frame, opts.command);

  frame->logfd = -1;

  if (opts.enable_log)
    Vframe.set.log (frame, opts.logfile, frame->remove_log);

  frame->mb_buf[0] = '\0';
  frame->mb_curlen = frame->mb_len = frame->mb_code = 0;
  frame->render = string_new (2048);
  frame->state = 0;

  frame->process_output_cb = (NULL is opts.process_output_cb ?
      frame_process_output_cb : opts.process_output_cb);

  frame->at_fork_cb = (NULL is opts.at_fork_cb ?
      frame_at_fork_default_cb : opts.at_fork_cb);

  frame->unimplemented_cb = frame_unimplemented_default_cb;

  frame->videomem = vwm_alloc_ints (frame->num_rows, frame->num_cols, 0);
  frame->colors = vwm_alloc_ints (frame->num_rows, frame->num_cols, COLOR_FG_NORMAL);
  frame->esc_param = Alloc (sizeof (int) * MAX_PARAMS);
  for (int i = 0; i < MAX_PARAMS; i++) frame->esc_param[i] = 0;
  frame->tabstops = Alloc (sizeof (int) * frame->num_cols);
  for (int i = 0; i < frame->num_cols; i++) {
    ifnot ((int) i % TABWIDTH)
      frame->tabstops[i] = 1;
    else
      frame->tabstops[i] = 0;
  }

  Vframe.reset (frame);

  if (opts.create_fd)
    Vframe.create_fd (frame);

  if (opts.fork and frame->argc)
    Vframe.fork (frame);

  return frame;
}

static vwm_frame *win_new_frame (vwm_win *this, frame_opts opts) {
  vwm_frame *frame = self(init_frame, opts);

  if (frame->at_frame < 0 or frame->at_frame > this->length)
    self(append_frame, frame);
  else
    self(insert_frame_at, frame, frame->at_frame - 1);

  if (frame->is_visible) {
    this->num_visible_frames++;
    this->num_separators++;

    int at = 1;
    vwm_frame *it = this->head;
    while (it) {
      if (it is frame) break;
      at += (it->is_visible isnot 0);
      it = it->next;
    }

    frame->at_frame = at;

  }

  return frame;
}

static vwm_frame *win_pop_frame_at (vwm_win *this, int idx) {
  return DListPopAt (this, vwm_frame, idx);
}

static vwm_frame *win_add_frame (vwm_win *this, int argc, char **argv, int draw) {
  int num_frames = self(get.num_visible_frames);

  if (num_frames is this->max_frames) return NULL;

  int frame_rows = 0;
  int mod = self(frame_rows, num_frames + 1, &frame_rows);
  int
    num_rows = frame_rows + mod,
    first_row = this->first_row;

  vwm_frame *frame = this->head;
  while (frame) {
    ifnot (frame->is_visible) goto next_frame;

    frame->new_rows = num_rows;
    first_row += num_rows + 1;
    num_rows = frame_rows;

    next_frame: frame = frame->next;
  }

  frame = self(new_frame, FrameOpts (
      .num_rows = num_rows,
      .first_row = first_row,
      .argc = argc,
      .argv = argv));

  frame->new_rows = num_rows;

  self(set.current_at, this->length - 1);
  self(on_resize, draw);
  return frame;
}

static int win_frame_rows (vwm_win *this, int num_frames, int *frame_rows) {
  int avail_rows = this->num_rows - (num_frames - 1);
  *frame_rows = avail_rows / num_frames;
  return avail_rows % num_frames;
}

static int win_delete_frame (vwm_win *this, vwm_frame *frame, int draw) {
  ifnot (this->length)
    return OK;

  if (1 is this->length) {
    Vframe.kill_proc (frame);;
    self(release_frame_at, 0);
  } else if (0 is frame->is_visible) {
    int idx = self(get.frame_idx, frame);
    Vframe.kill_proc (frame);
    self(release_frame_at, idx);
  } else {
    int is_last_frame = this->last_frame is frame;
    int idx = self(get.frame_idx, frame);
    Vframe.kill_proc (frame);
    self(release_frame_at, idx);

    int num_frames = self(get.num_visible_frames);
    ifnot (num_frames) return OK;

    int frame_rows = 0;
    int mod = self(frame_rows, num_frames, &frame_rows);
    int
      num_rows = frame_rows + mod,
      first_row = this->first_row;

    frame = this->head;
    while (frame) {
      ifnot (frame->is_visible) goto next_frame;

      frame->new_rows = num_rows;
      first_row += num_rows + 1;
      num_rows = frame_rows;

      next_frame: frame = frame->next;
    }

    if (is_last_frame)
      this->last_frame = this->current;

    self(on_resize, draw);
  }

  return OK;
}

static void frame_release_log (vwm_frame *this) {
  if (NULL is this->logfile) return;

  if (this->remove_log)
    unlink (this->logfile->bytes);

  string_free (this->logfile);
  this->logfile = NULL;

  close (this->logfd);
  this->logfd = -1;
}

static void win_release_frame_at (vwm_win *this, int idx) {
  vwm_frame *frame = self(pop_frame_at, idx);

  if (NULL is frame) return;

  if (frame->is_visible) {
    this->num_separators--;
    this->num_visible_frames--;

    vwm_frame *it = this->head;
    while (it) {
      if (it->is_visible)
        if (frame->at_frame < it->at_frame)
          it->at_frame--;

      it = it->next;
    }
  }

  Vframe.release_log (frame);

  for (int i = 0; i < frame->num_rows; i++)
    free (frame->videomem[i]);
  free (frame->videomem);

  for (int i = 0; i < frame->num_rows; i++)
    free (frame->colors[i]);
  free (frame->colors);

  free (frame->tabstops);
  free (frame->esc_param);

  Vframe.release_argv (frame);
  string_release (frame->render);

  free (frame);
}

static void vwm_make_separator (string_t *render, char *color, int cells, int row, int col) {
  vt_goto (render, row, col);
  string_append (render, color);
  for (int i = 0; i < cells; i++)
    string_append_with_len (render, "—", 3);

  vt_attr_reset (render);
}

static int win_set_separators (vwm_win *this, int draw) {
  string_clear (this->separators_buf);

  ifnot (this->num_separators) return NOTOK;

  vwm_frame *prev = this->head;
  vwm_frame *frame = this->head->next;
  while (prev->is_visible is 0) {
    prev = frame;
    frame = frame->next;
  }

  int num = 0;

  while (num < this->num_separators) {
    ifnot (frame->is_visible) goto next_frame;

    num++;
    vwm_make_separator (this->separators_buf,
       (prev is this->current ? COLOR_FOCUS : COLOR_UNFOCUS),
        frame->num_cols, prev->first_row + prev->last_row, frame->first_col);

    prev = frame;
    next_frame: frame = frame->next;
  }

  if (DRAW is draw)
    vt_write (this->separators_buf->bytes, stdout);

  return OK;
}

static vwm_frame *win_set_current_at (vwm_win *this, int idx) {
  DListSetCurrent (this, idx);
  return this->current;
}

static vwm_frame *win_set_frame_as_current (vwm_win *this, vwm_frame *frame) {
  int idx = DListGetIdx (this, vwm_frame, frame);
  return win_set_current_at (this, idx);
}

static void win_draw (vwm_win *this) {
  char buf[8];

  int
    len = 0,
    oldattr = 0,
    oldclr = COLOR_FG_NORM,
    clr = COLOR_FG_NORM;

  uchar on = NORMAL;
  utf8 chr = 0;

  string_t *render = this->render;
  string_clear (render);
  string_append (render, TERM_SCREEN_CLEAR);
  vt_setscroll (render, 0, 0);
  vt_attr_reset (render);
  vt_setbg (render, COLOR_BG_NORM);
  vt_setfg (render, COLOR_FG_NORM);
  vwm_frame *frame = this->head;
  while (frame) {
    ifnot (frame->is_visible) goto next_frame;

    vt_goto (render, frame->first_row, 1);

    for (int i = 0; i < frame->num_rows; i++) {
      for (int j = 0; j < frame->num_cols; j++) {
        chr = frame->videomem[i][j];
        clr = frame->colors[i][j];

        ifnot (clr is oldclr) {
          vt_setfg (render, clr);
          oldclr = clr;
        }

        if (chr & 0xFF) {
          vt_attr_check (render, chr & 0xFF, oldattr, &on);
          oldattr = (chr & 0xFF);

          if (oldattr >= 0x80) {
            ustring_character (chr, buf, &len);
            string_append_with_len (render, buf, len);
          } else
            string_append_byte (render, chr & 0xFF);
        } else {
          oldattr = 0;

          ifnot (on is NORMAL) {
            vt_attr_reset (render);
            on = NORMAL;
          }

          string_append_byte (render, ' ');
        }
      }

      string_append (render, "\r\n");
    }

    // clear the last newline, otherwise it scrolls one line more
    string_clear_at (render, -1); // this is visible when there is one frame

    /*  un-needed?
    vt_setscroll (render, frame->scroll_first_row + frame->first_row - 1,
        frame->last_row + frame->first_row - 1);
    vt_goto (render, frame->row_pos + frame->first_row - 1, frame->col_pos);
    */

    next_frame: frame = frame->next;
  }

  self(set.separators, DONOT_DRAW);
  string_append_with_len (render, this->separators_buf->bytes, this->separators_buf->num_bytes);

  frame = this->current;
  vt_goto (render, frame->row_pos + frame->first_row - 1, frame->col_pos);

  vt_write (render->bytes, stdout);
}

static void win_on_resize (vwm_win *this, int draw) {
  int frow = 1;
  vwm_frame *frame = this->head;
  while (frame) {
    ifnot (frame->is_visible) goto next_frame;

    Vframe.on_resize (frame, frame->new_rows, frame->num_cols);
    frame->num_rows = frame->new_rows;
    frame->last_row = frame->num_rows;
    frame->first_row = frow;
    frow += frame->num_rows + 1;
    if (frame->argv and frame->pid isnot -1) {
      struct winsize ws = {.ws_row = frame->num_rows, .ws_col = frame->num_cols};
      ioctl (frame->fd, TIOCSWINSZ, &ws);
      kill (frame->pid, SIGWINCH);
    }

    next_frame: frame = frame->next;
  }

  if (draw)
    self(draw);
}

static int win_min_rows (vwm_win *this) {
  int num_frames = self(get.num_visible_frames);
  return this->num_rows - num_frames -
      this->num_separators - (num_frames * 2);
}

static void win_frame_increase_size (vwm_win *this, vwm_frame *frame, int param, int draw) {
  if (this->length is 1)
    return;

  int min_rows = win_min_rows (this);

  ifnot (param) param = 1;
  if (param > min_rows) param = min_rows;

  int num_lines;
  int mod;
  int num_frames = self(get.num_visible_frames);

  if (param is 1 or param is num_frames - 1) {
    num_lines = 1;
    mod = 0;
  } else if (param > num_frames - 1) {
    num_lines = param / (num_frames - 1);
    mod = param % (num_frames - 1);
  } else {
    num_lines = (num_frames - 1) / param;
    mod = (num_frames - 1) % param;
  }

  int orig_param = param;

  vwm_frame *fr = this->head;
  while (fr) {
    fr->new_rows = fr->num_rows;
    fr = fr->next;
  }

  fr = this->head;
  int iter = 1;
  while (fr and param and iter++ < ((num_frames - 1) * 3)) {
    if (frame is fr or frame->is_visible is 0)
      goto next_frame;

    int num = num_lines;
    while (fr->new_rows > 2 and num--) {
      fr->new_rows = fr->new_rows - 1;
      param--;
    }

    if (fr->new_rows is 2)
      goto next_frame;

    if (mod) {
      fr->new_rows--;
      param--;
      mod--;
    }

    next_frame:
      if (fr->next is NULL)
        fr = this->head;
      else
        fr = fr->next;
  }

  frame->new_rows = frame->new_rows + (orig_param - param);

  self(on_resize, draw);
}

static void win_frame_decrease_size (vwm_win *this, vwm_frame *frame, int param, int draw) {
  if (1 is this->length or frame->num_rows <= MIN_ROWS)
    return;

  if (frame->num_rows - param <= 2)
    param = frame->num_rows - 2;

  if (0 >= param)
    param = 1;

  int num_lines;
  int mod;
  int num_frames = self(get.num_visible_frames);

  if (param is 1 or param is num_frames - 1) {
    num_lines = 1;
    mod = 0;
  } else if (param > num_frames - 1) {
    num_lines = param / (num_frames - 1);
    mod = param % (num_frames - 1);
  } else {
    num_lines = (num_frames - 1) / param;
    mod = (num_frames - 1) % param;
  }

  vwm_frame *fr = this->head;
  while (fr) {
    fr->new_rows = fr->num_rows;
    fr = fr->next;
  }

  int orig_param = param;

  fr = this->head;
  int iter = 1;
  while (fr and param and iter++ < ((this->length - 1) * 3)) {
    if (frame is fr or frame->is_visible is 0)
      goto next_frame;

    fr->new_rows = fr->num_rows + num_lines;

    param -= num_lines;

    if (mod) {
      fr->new_rows++;
      param--;
      mod--;
    }

  next_frame:
    if (fr->next is NULL)
      fr = this->head;
    else
      fr = fr->next;
  }

  frame->new_rows = frame->new_rows - (orig_param - param);

  self(on_resize, draw);
}

static void win_frame_set_size (vwm_win *this, vwm_frame *frame, int param, int draw) {
  if (param is frame->num_rows or 0 >= param)
    return;

  if (param > frame->num_rows)
    win_frame_increase_size (this, frame, param - frame->num_rows, draw);
  else
    win_frame_decrease_size (this, frame, frame->num_rows - param, draw);
}

static vwm_frame *win_frame_change (vwm_win *this, vwm_frame *frame, int dir, int draw) {
  int num_frames = self(get.num_visible_frames);
  if ((NULL is frame->next and NULL is frame->prev) or 1 is num_frames)
    return frame;

  int idx = -1;

  vwm_frame *lframe = frame;

again:
  if (dir is DOWN_POS) {
    if (NULL is lframe->next) {
      idx = 0;
      while (lframe) {
        if (lframe->is_visible) break;
        idx++;
        lframe = lframe->next;
      }
    } else {
      if (lframe->next->is_visible) {
        idx = self(get.frame_idx, lframe->next);
      } else {
        lframe = lframe->next;
        goto again;
      }
    }
  } else {
    if (NULL is lframe->prev) {
      idx = this->length - 1;
      while (lframe) {
        if (lframe->is_visible) break;
        idx--;
        lframe = lframe->prev;
      }
    } else {
      if (lframe->prev->is_visible) {
        idx = self(get.frame_idx, lframe->prev);
      } else {
        lframe = lframe->prev;
        goto again;
      }
    }
  }

  this->last_frame = frame;

  self(set.current_at, idx);

  if (OK is self(set.separators, draw))
    if (draw is DONOT_DRAW)
      this->draw_separators = 1;

  return this->current;
}

static void vwm_change_win (vwm_t *this, vwm_win *win, int dir, int draw) {
  if ($my(length) is 1)
    return;

  int idx = -1;
  switch (dir) {
    case LAST_POS:
      if ($my(last_win) is win)
        return;

      idx = self(get.win_idx, $my(last_win));
      break;

    case NEXT_POS:
      ifnot (NULL is win->next)
        idx = self(get.win_idx, win->next);
      else
        idx = 0;
      break;

    case PREV_POS:
      ifnot (NULL is win->prev)
        idx = self(get.win_idx, win->prev);
      else
        idx = $my(length) - 1;
      break;

    default:
      return;
  }

  $my(last_win) = win;

  win = self(set.current_at, idx);

  Vterm.screen.clear ($my(term));

  if (draw) {
    if (win->is_initialized)
      Vwin.draw (win);
    else
      Vwin.set.separators (win, DRAW);
  } else
    Vwin.set.separators (win, DONOT_DRAW);
}

static int vwm_default_edit_file_cb (vwm_t *this, vwm_frame *frame, char *file, void *object) {
  (void) object; (void) frame;

  size_t len = $my(editor)->num_bytes + bytelen (file);
  char command[len + 2];
  snprintf (command, len + 2, "%s %s", $my(editor)->bytes, file);
  int argc = 0;
  char **argv = parse_command (command, &argc);
  int retval = self(spawn, argv);
  argv_release (argv, &argc);
  return retval;
}

static void frame_reopen_log (vwm_frame *this) {
  if (this->logfile is NULL or 0 is this->logfile->num_bytes)
    return;

  if (-1 isnot this->logfd) close (this->logfd);

  this->logfd = open (this->logfile->bytes, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
}

static int frame_edit_log (vwm_frame *frame) {
  if (frame->logfile is NULL)
    return NOTOK;

  vwm_win *win = frame->parent;
  vwm_t *this = win->parent;

  int len;

  for (int i = 0; i < frame->num_rows; i++) {
    char buf[(frame->num_cols * 3) + 2];
    len = vt_video_line_to_str (frame->videomem[i], buf, frame->num_cols);
    write (frame->logfd, buf, len);
  }

  $my(edit_file_cb) (this, frame, frame->logfile->bytes, $my(objects)[VWMED_OBJECT]);

  vt_video_add_log_lines (frame);
  Vwin.draw (win);
  return OK;
}

static char *vwm_name_gen (int *name_gen, char *prefix, size_t prelen) {
  size_t num = (*name_gen / 26) + prelen;
  char *name = Alloc (num * sizeof (char *) + 1);
  size_t i = 0;
  for (; i < prelen; i++) name[i] = prefix[i];
  for (; i < num; i++) name[i] = 'a' + ((*name_gen)++ % 26);
  name[num] = '\0';
  return name;
}

static int win_get_num_frames (vwm_win *this) {
  return this->length;
}

static int win_get_num_visible_frames (vwm_win *this) {
  return this->num_visible_frames;
}

static vwm_frame *win_get_current_frame (vwm_win *this) {
  return this->current;
}

static vwm_frame *win_get_frame_at (vwm_win *this, int idx) {
  return DListGetAt (this, vwm_frame, idx);
}

static int win_get_frame_idx (vwm_win *this, vwm_frame *frame) {
  int idx = DListGetIdx (this, vwm_frame, frame);
  if (idx is INDEX_ERROR)
    return NOTOK;

  return idx;
}

static int vwm_append_win (vwm_t *this, vwm_win *win) {
  return DListAppend ($myprop, win);
}

static vwm_win *vwm_new_win (vwm_t *this, char *name, win_opts opts) {
  vwm_win *win = Alloc (sizeof (vwm_win));
  self(append_win, win);

  win->parent = this;
  win->self = this->win;
  win->frame = this->frame;

  if (NULL is name)
    win->name = vwm_name_gen (&$my(name_gen), "win:", 4);
  else
    win->name = strdup (name);

  int num_frames = opts.num_frames;

  win->max_frames = opts.max_frames;
  win->first_row = opts.first_row;
  win->first_col = opts.first_col;

  win->num_rows = opts.num_rows;
  win->num_cols = opts.num_cols;

  if (num_frames > win->max_frames) num_frames = win->max_frames;
  if (num_frames < 0) num_frames = 1;

  if (win->num_rows >= $my(num_rows))
    win->num_rows = $my(num_rows);

  win->num_visible_frames = 0;
  win->num_separators = -1;

  win->separators_buf = string_new ((win->num_rows * win->num_cols) + 32);
  win->render = string_new (4096);
  win->last_row = win->num_rows;

  if (win->first_col <= 0) win->first_col = 1;
  if (win->first_row <= 0) win->first_row = 1;
  if (win->first_row >= win->num_rows - (num_frames - 1) + num_frames)
    win->first_row = win->num_rows - (num_frames - 1) - num_frames;

  win->cur_row = win->first_row;
  win->cur_col = win->first_col;

  win->length = 0;
  win->cur_idx = -1;
  win->head = win->current = win->tail = NULL;

  int frame_rows = 0;
  int mod = Vwin.frame_rows (win, num_frames, &frame_rows);

  int
    num_rows = frame_rows + mod,
    first_row = win->first_row;

  for (int i = 0; i < num_frames; i++) {
    frame_opts fr_opts;

    if (i < WIN_OPTS_MAX_FRAMES)
      fr_opts = opts.frame_opts[i];
    else
      fr_opts = FrameOpts ();

    fr_opts.num_rows = num_rows;
    fr_opts.first_row = first_row;

    Vwin.new_frame (win, fr_opts);

    first_row += num_rows + 1;
    num_rows = frame_rows;
  }

  win->last_frame = win->head;

  if (opts.focus or opts.draw) {
    win = self(set.current_at, $my(length) - 1);
    Vwin.draw (win);
  }

  return win;
}

static void vwm_release_win (vwm_t *this, vwm_win *win) {
  int idx = self(get.win_idx, win);
  if (idx is NOTOK) return;

  vwm_win *w = self(pop_win_at, idx);

  int len = w->length;
  for (int i = 0; i < len; i++)
    Vwin.release_frame_at (w, 0);

  string_release (win->separators_buf);
  string_release (win->render);

  free (w->name);
  free (w);
}

static int vwm_spawn (vwm_t *this, char **argv) {
  int status = NOTOK;
  pid_t pid;

  Vterm.orig_mode ($my(term));

  if (-1 is (pid = fork ())) goto theend;

  ifnot (pid) {
    char lrows[4], lcols[4];
    snprintf (lrows, 4, "%d", $my(num_rows));
    snprintf (lcols, 4, "%d", $my(num_cols));

    setenv ("TERM", $my(term)->name, 1);
    setenv ("LINES", lrows, 1);
    setenv ("COLUMNS", lcols, 1);

    execvp (argv[0], argv);
    fprintf (stderr, "execvp failed\n");
    _exit (1);
  }

  if (-1 is waitpid (pid, &status, 0)) {
    status = -1;
    goto theend;
  }

  ifnot (WIFEXITED (status)) {
    status = -1;
    fprintf (stderr, "Failed to invoke %s\n", argv[0]);
    goto theend;
  }

  ifnot (status is WEXITSTATUS (status))
    fprintf (stderr, "Proc %s terminated with exit status: %d", argv[0], status);

theend:
  Vterm.raw_mode ($my(term));
  return status;
}

static int frame_create_fd (vwm_frame *frame) {
  if (frame->fd isnot -1) return frame->fd;

  int fd = -1;
  if (-1 is (fd = posix_openpt (O_RDWR|O_NOCTTY|O_CLOEXEC))) goto theerror;
  if (-1 is grantpt (fd)) goto theerror;
  if (-1 is unlockpt (fd)) goto theerror;
  char *name = ptsname (fd); ifnull (name) goto theerror;
  cstring_cp (frame->tty_name, MAX_TTYNAME, name, MAX_TTYNAME - 1);

  frame->fd = fd;
  return fd;

theerror:
  if (fd isnot -1)
    close (fd);

  return -1;
}

static pid_t frame_fork (vwm_frame *frame) {
  if (frame->pid isnot -1)
    return frame->pid;

  char pid[8]; snprintf (pid, sizeof (pid), "%d", getpid ());

  vwm_t *this = frame->parent->parent;

  signal (SIGWINCH, SIG_IGN);

  frame->pid = -1;

  int fd = -1;

  if (frame->fd is -1) {
    if (-1 is (fd = posix_openpt (O_RDWR|O_NOCTTY|O_CLOEXEC))) goto theerror;
    if (-1 is grantpt (fd)) goto theerror;
    if (-1 is unlockpt (fd)) goto theerror;
    char *name = ptsname (fd); ifnull (name) goto theerror;
    cstring_cp (frame->tty_name, MAX_TTYNAME, name, MAX_TTYNAME - 1);
  } else
    fd = frame->fd;

  frame->fd = fd;

  if (-1 is (frame->pid = fork ())) goto theerror;

  ifnot (frame->pid) {
    int slave_fd;

    setsid ();

    if (-1 is (slave_fd = open (frame->tty_name, O_RDWR|O_CLOEXEC|O_NOCTTY)))
      goto theerror;

    ioctl (slave_fd, TIOCSCTTY, 0);

    dup2 (slave_fd, 0);
    dup2 (slave_fd, 1);
    dup2 (slave_fd, 2);

    fd_set_size (slave_fd, frame->num_rows, frame->num_cols);

    close (slave_fd);
    close (fd);

    int maxfd;
#ifdef OPEN_MAX
    maxfd = OPEN_MAX;
#else
    maxfd = sysconf (_SC_OPEN_MAX);
#endif

    for (int fda = 3; fda < maxfd; fda++)
      if (close (fda) is -1 and errno is EBADF)
        break;

    sigset_t emptyset;
    sigemptyset (&emptyset);
    sigprocmask (SIG_SETMASK, &emptyset, NULL);

    char rows[4]; snprintf (rows, 4, "%d", frame->num_rows);
    char cols[4]; snprintf (cols, 4, "%d", frame->num_cols);
    setenv ("TERM",  $my(term)->name, 1);
    setenv ("LINES", rows, 1);
    setenv ("COLUMNS", cols, 1);
    setenv ("VWM", pid, 1);

    frame->pid = getpid ();

    int retval = frame->at_fork_cb (frame, this, frame->parent);

    if (retval is NOTOK)
      goto theerror;

    ifnot (retval)
      return 0;

    if (retval is 1) {
      execvp (frame->argv[0], frame->argv);

      fprintf (stderr, "execvp() failed for command: '%s'\n", frame->argv[0]);
      _exit (1);
    }
  }

  goto theend;

theerror:
  ifnot (-1 is frame->pid) {
    kill (frame->pid, SIGHUP);
    waitpid (frame->pid, NULL, 0);
  }

  frame->pid = -1;
  frame->fd = -1;

  ifnot (-1 is fd) close (fd);

theend:
  signal (SIGWINCH, vwm_sigwinch_handler);
  return frame->pid;
}

static void vwm_sigwinch_handler (int sig) {
  signal (sig, vwm_sigwinch_handler);
  vwm_t *this = VWM;
  $my(need_resize) = 1;
}

static void vwm_handle_sigwinch (vwm_t *this) {
  int rows; int cols;
  Vterm.init_size ($my(term), &rows, &cols);
  self(set.size, rows, cols, 1);

  vwm_win *win = $my(head);
  while (win) {
    win->num_rows = $my(num_rows);
    win->num_cols = $my(num_cols);
    win->last_row = win->num_rows;

    int frame_rows = 0;
    int num_frames = Vwin.get.num_visible_frames (win);
    int mod = Vwin.frame_rows (win, num_frames, &frame_rows);

    int
      num_rows = frame_rows + mod,
      first_row = win->first_row;

    vwm_frame *frame = win->head;
    while (frame) {
      Vframe.on_resize (frame, num_rows, win->num_cols);

      frame->first_row = first_row;
      frame->num_rows = num_rows;
      frame->last_row = frame->num_rows;

      if (frame->argv and frame->pid isnot -1) {
        struct winsize ws = {.ws_row = frame->num_rows, .ws_col = frame->num_cols};
        ioctl (frame->fd, TIOCSWINSZ, &ws);
        kill (frame->pid, SIGWINCH);
      }

      if (frame->is_visible) {
        first_row += num_rows + 1;
        num_rows = frame_rows;
      }

      frame = frame->next;
    }
    win = win->next;
  }

  win = $my(current);

  Vwin.draw (win);
  $my(need_resize) = 0;
}

static void vwm_exit_signal (int sig) {
  __deinit_vwm__ (&VWM);
  exit (sig);
}

static int vwm_main (vwm_t *this) {
  ifnot ($my(length)) return OK;

  if (NULL is $my(current)) {
    $my(current) = $my(head);
    $my(cur_idx) = 0;
  }

  if ($my(length) > 1) {
    if (NULL isnot $my(current)->prev)
      $my(last_win) = $my(current)->prev;
    else
      $my(last_win) = $my(current)->next;
  }

  setbuf (stdin, NULL);

  signal (SIGHUP,   vwm_exit_signal);
  signal (SIGINT,   vwm_exit_signal);
  signal (SIGQUIT,  vwm_exit_signal);
  signal (SIGTERM,  vwm_exit_signal);
  signal (SIGSEGV,  vwm_exit_signal);
  signal (SIGBUS,   vwm_exit_signal);
  signal (SIGWINCH, vwm_sigwinch_handler);

  fd_set read_mask;
  struct timeval *tv = NULL;

  char
    input_buf[MAX_CHAR_LEN],
    output_buf[BUFSIZE];

  int
    maxfd,
    numready,
    output_len,
    retval = NOTOK;

  vwm_win *win = $my(current);

  Vwin.set.separators (win, DRAW);

  vwm_frame *frame = win->head;
  while (frame) {
    if (frame->argv is NULL)
      Vframe.set.command (frame, $my(default_app)->bytes);

    Vframe.fork (frame);

    frame = frame->next;
  }

#define forever for (;;)

  forever {
    win = $my(current);
    if (NULL is win) {
      retval = OK;
      break;
    }

    check_length:

    ifnot (Vwin.get.num_visible_frames (win)) { // at_no_length_cb
      retval = OK;
      if (1 isnot $my(length))
        self(change_win, win, PREV_POS, DRAW);

      self(release_win, win);
      continue;
    }

    if ($my(need_resize))
      vwm_handle_sigwinch (this);

    Vwin.set.frame (win, win->current);

    maxfd = 1;

    FD_ZERO (&read_mask);
    FD_SET (STDIN_FILENO, &read_mask);

    frame = win->head;
    int num_frames = 0;
    while (frame) {
      ifnot (frame->is_visible) goto frame_next;

      if (frame->pid isnot -1) {
        if (0 is Vframe.check_pid (frame)) {
          vwm_frame *tmp = frame->next;
          Vwin.delete_frame (win, frame, DRAW);
          frame = tmp;
          continue;
        }
      }

      if (frame->fd isnot -1) {
        FD_SET (frame->fd, &read_mask);
        num_frames++;

        if (maxfd <= frame->fd)
          maxfd = frame->fd + 1;
      }

frame_next:
      frame = frame->next;
    }

    ifnot (num_frames) goto check_length;

    if (0 >= (numready = select (maxfd, &read_mask, NULL, NULL, tv))) {
      switch (errno) {
        case EIO:
        case EINTR:
        default:
          break;
      }

      continue;
    }

    frame = win->current;

    for (int i = 0; i < MAX_CHAR_LEN; i++) input_buf[i] = '\0';

    if (FD_ISSET (STDIN_FILENO, &read_mask)) {
      if (0 < fd_read (STDIN_FILENO, input_buf, 1)) {
        if (VWM_QUIT is self(process_input, win, frame, input_buf)) {
          retval = OK;
          break;
        }
      }
    }

    win = $my(current);

    frame = win->head;
    while (frame) {
      if (frame->fd is -1 or 0 is frame->is_visible)
        goto next_frame;

      if (FD_ISSET (frame->fd, &read_mask)) {
        output_buf[0] = '\0';
        if (0 > (output_len = read (frame->fd, output_buf, BUFSIZE))) {
          switch (errno) {
            case EIO:
            default:
              if (-1 isnot frame->pid) {
                if (0 is Vframe.check_pid (frame)) {
                  Vwin.delete_frame (win, frame, DRAW);
                  goto check_length;
                }
              }

              goto next_frame;
          }
        }

        output_buf[output_len] = '\0';

        Vwin.set.frame (win, frame);

        frame->process_output_cb (frame, output_buf, output_len);
      }

      next_frame:
        frame = frame->next;
    }

    win->is_initialized = 1;
  }

  if (retval is 1 or retval is OK) return OK;

  return NOTOK;
}

static int vwm_default_on_tab_cb (vwm_t *this, vwm_win *win, vwm_frame *frame, void *object) {
  (void) this; (void) win; (void) frame; (void) object;
  return OK;
}

static int vwm_default_rline_cb (vwm_t *this, vwm_win *win, vwm_frame *frame, void *object) {
  (void) this; (void) win; (void) frame; (void) object;
  return OK;
}

static int vwm_process_input (vwm_t *this, vwm_win *win, vwm_frame *frame, char *input_buf) {
  if (input_buf[0] isnot $my(mode_key)) {
    if (-1 isnot frame->fd)
      fd_write (frame->fd, input_buf, 1);
    return OK;
  }

  int param = 0;

  utf8 c;

getc_again:
  c = self(getkey, STDIN_FILENO);

  if (-1 is c) return OK;

  if (c is $my(mode_key)) {
    input_buf[0] = $my(mode_key); input_buf[1] = '\0';
    fd_write (frame->fd, input_buf, 1);
    return OK;
  }

  switch (c) {
    case ESCAPE_KEY:
      break;

    case '\t': {
        int retval = $my(on_tab_cb) (this, win, frame, $my(objects)[VWMED_OBJECT]);
        if (retval is VWM_QUIT or ($my(state) & VWM_QUIT))
          return VWM_QUIT;
      }
      break;

    case ':': {
        int retval = $my(rline_cb) (this, win, frame, $my(objects)[VWMED_OBJECT]);
        if (retval is VWM_QUIT or ($my(state) & VWM_QUIT))
          return VWM_QUIT;

      }
      break;

    case 'q':
      return VWM_QUIT;

    case '!':
    case 'c':
      if (frame->pid isnot -1)
        break;

      if (c is '!')
        Vframe.set.command (frame, $my(shell)->bytes);
      else
        if (NULL is frame->argv)
          Vframe.set.command (frame, $my(default_app)->bytes);

      Vframe.fork (frame);
      break;

    case 'n': {
        ifnot (param)
          param = 1;
        else
          if (param > MAX_FRAMES)
            param = MAX_FRAMES;

        win_opts w_opts = WinOpts (
            .num_rows = $my(num_rows),
            .num_cols = $my(num_cols),
            .num_frames = param,
            .draw = 1,
            .focus = 1);

        for (int i = 0; i < param; i++)
          w_opts.frame_opts[i].command = $my(default_app)->bytes;

        win = self(new.win, NULL, w_opts);
        }
      break;

    case 'K':
      if (0 is Vframe.kill_proc (frame))
        Vwin.delete_frame (win, frame, DRAW);
      break;

    case 'd':
      Vwin.delete_frame (win, frame, DRAW);
      break;

    case CTRL('l'):
      Vwin.draw (win);
      break;

    case 's': {
          vwm_frame *fr = Vwin.add_frame (win, 0, NULL, DRAW);
          ifnot (NULL is fr) {
            Vframe.set.command (fr, $my(default_app)->bytes);
            Vframe.fork (fr);
          }
        }
      break;

    case 'S': {
        utf8 w = self(getkey, STDIN_FILENO);
        char *command = $my(default_app)->bytes;

        switch (w) {
          case '!':
            command = $my(shell)->bytes;
            break;

          case 'c':
            command = $my(default_app)->bytes;
            break;

          case 'e':
            command = $my(editor)->bytes;
            break;

          default:
            return OK;
        }

        vwm_frame *fr = Vwin.add_frame (win, 0, NULL, DRAW);
        ifnot (NULL is fr) {
          Vframe.set.command (fr, command);
          Vframe.fork (fr);
        }
      }

      break;

    case PAGE_UP_KEY:
    case 'E':
      Vframe.edit_log (frame);
      break;

    case 'j':
    case 'k':
    case 'w':
    case ARROW_DOWN_KEY:
    case ARROW_UP_KEY:
      Vwin.frame.change (win, frame, (
        c is 'w' or c is 'j' or c is ARROW_DOWN_KEY) ? DOWN_POS : UP_POS, DONOT_DRAW);
      break;

    case 'h':
    case 'l':
    case '`':
    case ARROW_LEFT_KEY:
    case ARROW_RIGHT_KEY:
      self(change_win, win,
          (c is ARROW_RIGHT_KEY or c is 'l') ? NEXT_POS :
          (c is '`') ? LAST_POS : PREV_POS, DRAW);
      break;

    case '+':
      Vwin.frame.increase_size (win, frame, param, DRAW);
      break;

    case '-':
      Vwin.frame.decrease_size (win, frame, param, DRAW);
      break;

    case '=':
      Vwin.frame.set_size (win, frame, param, DRAW);
      break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      param *= 10;
      param += (c - '0');
      goto getc_again;
  }

  return OK;
}

mutable public void __alloc_error_handler__ (int err, size_t size,
                           char *file, const char *func, int line) {
  fprintf (stderr, "MEMORY_ALLOCATION_ERROR\n");
  fprintf (stderr, "File: %s\nFunction: %s\nLine: %d\n", file, func, line);
  fprintf (stderr, "Size: %zd\n", size);

  if (err is INTEGEROVERFLOW_ERROR)
    fprintf (stderr, "Error: Integer Overflow Error\n");
  else
    fprintf (stderr, "Error: Not Enouch Memory\n");

  __deinit_vwm__ (&VWM);

  exit (1);
}

public vwm_t *__init_vwm__ (void) {
  AllocErrorHandler = __alloc_error_handler__;

  vwm_t *this = Alloc (sizeof (vwm_t));
  vwm_prop *prop = Alloc (sizeof (vwm_prop));

  *this =  (vwm_t) {
    .self = (vwm_self) {
      .main = vwm_main,
      .spawn = vwm_spawn,
      .getkey = vwm_getkey,
      .pop_win_at = vwm_pop_win_at,
      .change_win = vwm_change_win,
      .append_win = vwm_append_win,
      .release_win = vwm_release_win,
      .release_info = vwm_release_info,
      .process_input = vwm_process_input,
      .get = (vwm_get_self) {
        .term = vwm_get_term,
        .info = vwm_get_info,
        .shell = vwm_get_shell,
        .state = vwm_get_state,
        .lines = vwm_get_lines,
        .object = vwm_get_object,
        .editor = vwm_get_editor,
        .tmpdir = vwm_get_tmpdir,
        .win_idx = vwm_get_win_idx,
        .columns = vwm_get_columns,
        .num_wins = vwm_get_num_wins,
        .mode_key = vwm_get_mode_key,
        .current_win = vwm_get_current_win,
        .default_app = vwm_get_default_app,
        .current_frame = vwm_get_current_frame
      },
      .set = (vwm_set_self) {
        .size = vwm_set_size,
        .term = vwm_set_term,
        .state = vwm_set_state,
        .shell =  vwm_set_shell,
        .editor = vwm_set_editor,
        .tmpdir = vwm_set_tmpdir,
        .mode_key = vwm_set_mode_key,
        .object = vwm_set_object,
        .current_at = vwm_set_current_at,
        .default_app = vwm_set_default_app,
        .rline_cb = vwm_set_rline_cb,
        .on_tab_cb = vwm_set_on_tab_cb,
        .at_exit_cb = vwm_set_at_exit_cb,
        .edit_file_cb = vwm_set_edit_file_cb,
        .debug = (vwm_set_debug_self) {
          .sequences = vwm_set_debug_sequences,
          .unimplemented = vwm_set_debug_unimplemented
        },
      },
      .unset = (vwm_unset_self) {
        .tmpdir = vwm_unset_tmpdir,
        .debug = (vwm_unset_debug_self) {
          .sequences = vwm_unset_debug_sequences,
          .unimplemented = vwm_unset_debug_unimplemented
        }
      },
      .new = (vwm_new_self) {
        .win = vwm_new_win,
        .term = vwm_new_term
      }
    },
    .term = (vwm_term_self) {
      .release = term_release,
      .raw_mode =  term_raw_mode,
      .sane_mode = term_sane_mode,
      .orig_mode = term_orig_mode,
      .init_size = term_init_size,
      .screen = (vwm_term_screen_self) {
        .save = term_screen_save,
        .clear = term_screen_clear,
        .restore = term_screen_restore
       }
    },
    .win = (vwm_win_self) {
      .draw = win_draw,
      .on_resize = win_on_resize,
      .new_frame = win_new_frame,
      .add_frame = win_add_frame,
      .init_frame = win_init_frame,
      .frame_rows = win_frame_rows,
      .append_frame = win_append_frame,
      .delete_frame = win_delete_frame,
      .pop_frame_at = win_pop_frame_at,
      .release_info = win_release_info,
      .insert_frame_at = win_insert_frame_at,
      .release_frame_at = win_release_frame_at,
      .set = (vwm_win_set_self) {
        .frame = win_set_frame,
        .current_at = win_set_current_at,
        .separators = win_set_separators,
        .frame_as_current = win_set_frame_as_current
      },
      .get = (vwm_win_get_self) {
        .info = win_get_info,
        .frame_at = win_get_frame_at,
        .frame_idx = win_get_frame_idx,
        .num_frames = win_get_num_frames,
        .num_visible_frames = win_get_num_visible_frames,
        .current_frame = win_get_current_frame
      },
      .frame = (vwm_win_frame_self) {
        .change = win_frame_change,
        .set_size = win_frame_set_size,
        .increase_size = win_frame_increase_size,
        .decrease_size = win_frame_decrease_size
      }
    },
    .frame = (vwm_frame_self) {
      .fork = frame_fork,
      .clear = frame_clear,
      .reset = frame_reset,
      .edit_log = frame_edit_log,
      .check_pid = frame_check_pid,
      .create_fd = frame_create_fd,
      .on_resize = frame_on_resize,
      .kill_proc = frame_kill_proc,
      .reopen_log = frame_reopen_log,
      .release_log = frame_release_log,
      .release_argv = frame_release_argv,
      .release_info = frame_release_info,
      .process_output = frame_process_output,
      .get = (vwm_frame_get_self) {
        .fd = frame_get_fd,
        .pid = frame_get_pid,
        .info = frame_get_info,
        .argc = frame_get_argc,
        .argv = frame_get_argv,
        .root = frame_get_root,
        .logfd = frame_get_logfd,
        .parent = frame_get_parent,
        .logfile = frame_get_logfile,
        .num_rows = frame_get_num_rows,
        .visibility = frame_get_visibility
      },
      .set = (vwm_frame_set_self) {
        .fd = frame_set_fd,
        .log = frame_set_log,
        .argv = frame_set_argv,
        .command = frame_set_command,
        .visibility = frame_set_visibility,
        .at_fork_cb = frame_set_at_fork_cb,
        .unimplemented_cb = frame_set_unimplemented_cb,
        .process_output_cb = frame_set_process_output_cb
      }
    },
    .prop = prop
  };

  $my(tmpdir) = NULL;

  $my(editor) = string_new_with (EDITOR);
  $my(shell) = string_new_with (SHELL);
  $my(default_app) = string_new_with (DEFAULT_APP);
  $my(mode_key) = MODE_KEY;

  $my(length) = 0;
  $my(cur_idx) = -1;
  $my(head) = $my(tail) = $my(current) = NULL;
  $my(name_gen) = ('z' - 'a') + 1;
  $my(num_at_exit_cbs) = 0;
  $my(objects)[VWMED_OBJECT] = NULL;

  self(new.term);

  self(set.rline_cb, vwm_default_rline_cb);
  self(set.on_tab_cb, vwm_default_on_tab_cb);
  self(set.edit_file_cb, vwm_default_edit_file_cb);
  self(set.tmpdir, NULL, 0);

  $my(sequences_fp) = NULL;
  $my(sequences_fname) = NULL;
  $my(unimplemented_fp) = NULL;
  $my(unimplemented_fname) = NULL;

#ifdef DEBUG
  self(set.debug.sequences, NULL);
  self(set.debug.unimplemented, NULL);
#endif

  VWM = this;
  return this;
}

public void __deinit_vwm__ (vwm_t **thisp) {
  if (NULL == *thisp) return;

  vwm_t *this = *thisp;

  Vterm.orig_mode ($my(term));
  Vterm.release (&$my(term));

  vwm_win *win = $my(head);
  while (win) {
    vwm_win *tmp = win->next;
    self(release_win, win);
    win = tmp;
  }

  for (int i = 0; i < $my(num_at_exit_cbs); i++)
    $my(at_exit_cbs)[i] (this);

  if ($my(num_at_exit_cbs))
    free ($my(at_exit_cbs));

  self(unset.debug.sequences);
  self(unset.debug.unimplemented);
  self(unset.tmpdir);

  string_release ($my(editor));
  string_release ($my(shell));
  string_release ($my(default_app));

  free (this->prop);
  free (this);
  *thisp = NULL;
}
