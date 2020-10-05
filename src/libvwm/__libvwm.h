#define INDEX_ERROR            -1000
#define INTEGEROVERFLOW_ERROR  -1002

#ifdef self
#undef self
#endif

#define self(__f__, ...) this->self.__f__ (this, ## __VA_ARGS__)

typedef void (*AllocErrorHandlerF) (int, size_t, char *, const char *, int);

AllocErrorHandlerF AllocErrorHandler;

#define __REALLOC__ realloc
#define __CALLOC__  calloc

/* reallocarray:
 * $OpenBSD: reallocarray.c,v 1.1 2014/05/08 21:43:49 deraadt Exp $
 * Copyright (c) 2008 Otto Moerbeek <otto@drijf.net>
 */

#define MUL_NO_OVERFLOW ((size_t) 1 << (sizeof (size_t) * 4))
#define MEM_IS_INT_OVERFLOW(nmemb, ssize)                             \
 (((nmemb) >= MUL_NO_OVERFLOW || (ssize) >= MUL_NO_OVERFLOW) &&       \
  (nmemb) > 0 && SIZE_MAX / (nmemb) < (ssize))

#define Alloc(size) ({                                                \
  void *ptr__ = NULL;                                                 \
  if (MEM_IS_INT_OVERFLOW (1, (size))) {                              \
    errno = INTEGEROVERFLOW_ERROR;                                    \
    AllocErrorHandler (errno, (size),  __FILE__, __func__, __LINE__); \
  } else {                                                            \
    if (NULL == (ptr__ = __CALLOC__ (1, (size))))                     \
      AllocErrorHandler (errno, (size), __FILE__, __func__, __LINE__);\
    }                                                                 \
  ptr__;                                                              \
  })

#define Realloc(ptr, size) ({                                         \
  void *ptr__ = NULL;                                                 \
  if (MEM_IS_INT_OVERFLOW (1, (size))) {                              \
    errno = INTEGEROVERFLOW_ERROR;                                    \
    AllocErrorHandler (errno, (size),  __FILE__, __func__, __LINE__); \
  } else {                                                            \
    if (NULL == (ptr__ = __REALLOC__ ((ptr), (size))))                \
      AllocErrorHandler (errno, (size), __FILE__, __func__, __LINE__);\
    }                                                                 \
  ptr__;                                                              \
  })

#define DListAppend(list, node)                                     \
({                                                                  \
  if ((list)->head is NULL) {                                       \
    (list)->head = (node);                                          \
    (list)->tail = (node);                                          \
    (list)->current = (list)->head;                                 \
    (list)->cur_idx = 0;                                            \
  } else {                                                          \
    (list)->tail->next = (node);                                    \
    (node)->prev = (list)->tail;                                    \
    (node)->next = NULL;                                            \
    (list)->tail = (node);                                          \
  }                                                                 \
                                                                    \
  (list)->length++;                                                 \
})

#define DListPush(list, node)                                       \
({                                                                  \
  if ((list)->head == NULL) {                                       \
    (list)->head = (node);                                          \
    (list)->tail = (node);                                          \
    (list)->current = (node);                                       \
    (list)->head->next = NULL;                                      \
    (list)->head->prev = NULL;                                      \
  } else {                                                          \
    (list)->head->prev = (node);                                    \
    (node)->next = (list)->head;                                    \
    (list)->head = (node);                                          \
  }                                                                 \
                                                                    \
  (list)->length++;                                                 \
  list;                                                             \
})

#define DListPopTail(list, type)                                    \
({                                                                  \
type *node = NULL;                                                  \
do {                                                                \
  if ((list)->tail is NULL) break;                                  \
  node = (list)->tail;                                              \
  (list)->tail->prev->next = NULL;                                  \
  (list)->tail = (list)->tail->prev;                                \
  (list)->length--;                                                 \
} while (0);                                                        \
  node;                                                             \
})

#define DListPopCurrent(list, type)                                 \
({                                                                  \
type *node = NULL;                                                  \
do {                                                                \
  if ((list)->current is NULL) break;                               \
  node = (list)->current;                                           \
  if (1 is (list)->length) {                                        \
    (list)->head = NULL;                                            \
    (list)->tail = NULL;                                            \
    (list)->current = NULL;                                         \
    break;                                                          \
  }                                                                 \
  if (0 is (list)->cur_idx) {                                       \
    (list)->current = (list)->current->next;                        \
    (list)->current->prev = NULL;                                   \
    (list)->head = (list)->current;                                 \
    break;                                                          \
  }                                                                 \
  if ((list)->cur_idx is (list)->length - 1) {                      \
    (list)->current = (list)->current->prev;                        \
    (list)->current->next = NULL;                                   \
    (list)->cur_idx--;                                              \
    (list)->tail = (list)->current;                                 \
    break;                                                          \
  }                                                                 \
  (list)->current->next->prev = (list)->current->prev;              \
  (list)->current->prev->next = (list)->current->next;              \
  (list)->current = (list)->current->next;                          \
} while (0);                                                        \
  if (node isnot NULL) (list)->length--;                            \
  node;                                                             \
})

#define DListSetCurrent(list, idx_)                                 \
({                                                                  \
  int idx__ = idx_;                                                 \
  do {                                                              \
    if (0 > idx__) idx__ += (list)->length;                         \
    if (idx__ < 0 or idx__ >= (list)->length) {                     \
      idx__ = INDEX_ERROR;                                          \
      break;                                                        \
    }                                                               \
    if (idx__ is (list)->cur_idx) break;                            \
    int idx___ = (list)->cur_idx;                                   \
    (list)->cur_idx = idx__;                                        \
    if (idx___ < idx__)                                             \
      while (idx___++ < idx__)                                      \
        (list)->current = (list)->current->next;                    \
    else                                                            \
      while (idx___-- > idx__)                                      \
        (list)->current = (list)->current->prev;                    \
  } while (0);                                                      \
  idx__;                                                            \
})

#define DListPopAt(list, type, idx_)                                \
({                                                                  \
  int cur_idx = (list)->cur_idx;                                    \
  int __idx__ = DListSetCurrent (list, idx_);                       \
  type *cnode = NULL;                                               \
  do {                                                              \
    if (__idx__ is INDEX_ERROR) break;                              \
    cnode = DListPopCurrent (list, type);                           \
    if (cur_idx is __idx__) break;                                  \
    if (cur_idx > __idx__) cur_idx--;                               \
    DListSetCurrent (list, cur_idx);                                \
  } while (0);                                                      \
  cnode;                                                            \
})

#define DListGetAt(list_, type_, idx_)                              \
({                                                                  \
  type_ *node = NULL;                                               \
  int idx__ = idx_;                                                 \
  do {                                                              \
    if (0 > idx__) idx__ += (list_)->length;                        \
    if (idx__ < 0 or idx__ >= (list_)->length) {                    \
      idx__ = INDEX_ERROR;                                          \
      break;                                                        \
    }                                                               \
    if ((list_)->length / 2 < idx__) {                              \
      node = (list_)->head;                                         \
      while (idx__--)                                               \
        node = node->next;                                          \
    } else {                                                        \
      node = (list_)->tail;                                         \
      while (idx__++ < (list_)->length - 1)                         \
        node = node->prev;                                          \
    }                                                               \
  } while (0);                                                      \
  node;                                                             \
})

#define DListGetIdx(list, type, node)               \
({                                                  \
  int idx__ = INDEX_ERROR;                          \
  type *node__ = (list)->head;                      \
  if (list->length isnot 0 and NULL isnot node__) { \
    idx__ = -1;                                     \
    do {                                            \
      idx__++;                                      \
      if (node__ is node or node__ is (list)->tail) \
        break;                                      \
      node__ = node__->next;                        \
    } while (1);                                    \
  }                                                 \
  idx__;                                            \
})

#define DListPrependCurrent(list, node)                             \
({                                                                  \
  if ((list)->current is NULL) {                                    \
    (list)->head = (node);                                          \
    (list)->tail = (node);                                          \
    (list)->cur_idx = 0;                                            \
    (list)->current = (list)->head;                                 \
  } else {                                                          \
    if ((list)->cur_idx == 0) {                                     \
      (list)->head->prev = (node);                                  \
      (node)->next = (list)->head;                                  \
      (list)->head = (node);                                        \
      (list)->current = (list)->head;                               \
    } else {                                                        \
      (list)->current->prev->next = (node);                         \
      (list)->current->prev->next->next = (list)->current;          \
      (list)->current->prev->next->prev = (list)->current->prev;    \
      (list)->current->prev = (list)->current->prev->next;          \
      (list)->current = (list)->current->prev;                      \
    }                                                               \
  }                                                                 \
                                                                    \
  (list)->length++;                                                 \
  (list)->current;                                                  \
})

#define DListAppendCurrent(list, node)                              \
({                                                                  \
  if ((list)->current is NULL) {                                    \
    (list)->head = (node);                                          \
    (list)->tail = (node);                                          \
    (list)->cur_idx = 0;                                            \
    (list)->current = (list)->head;                                 \
  } else {                                                          \
    if ((list)->cur_idx is (list)->length - 1) {                    \
      (list)->current->next = (node);                               \
      (node)->prev = (list)->current;                               \
      (list)->current = (node);                                     \
      (node)->next = NULL;                                          \
      (list)->cur_idx++;                                            \
      (list)->tail = (node);                                        \
    } else {                                                        \
      (node)->next = (list)->current->next;                         \
      (list)->current->next = (node);                               \
      (node)->prev = (list)->current;                               \
      (node)->next->prev = (node);                                  \
      (list)->current = (node);                                     \
      (list)->cur_idx++;                                            \
    }                                                               \
  }                                                                 \
                                                                    \
  (list)->length++;                                                 \
  (list)->current;                                                  \
})

#define MAXLEN_LINE 4096
#define STR_FMT_(fmt_, ...)                                            \
({                                                                    \
  char buf_[MAXLEN_LINE];                                             \
  snprintf (buf_, MAXLEN_LINE, fmt_, __VA_ARGS__);                    \
  buf_;                                                               \
})

#define debug_append(fmt, ...)                            \
({                                                        \
  char *file_ = STR_FMT_ ("/tmp/%s.debug", __func__);      \
  FILE *fp_ = fopen (file_, "a+");                        \
  if (fp_ isnot NULL) {                                   \
    fprintf (fp_, (fmt), ## __VA_ARGS__);                 \
    fclose (fp_);                                         \
  }                                                       \
})


private int vwm_spawn (vwm_t *, char **);
