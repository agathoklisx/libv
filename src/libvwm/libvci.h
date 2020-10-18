#ifndef VC_H
#define VC_H

#ifndef OK
#define OK 0
#endif

#ifndef NOTOK
#define NOTOK -1
#endif

#ifndef ifnot
#define ifnot(__expr__) if (0 == (__expr__))
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

#ifndef bytelen
#define bytelen strlen
#endif

#ifndef ALLOC
#define INDEX_ERROR            -1000
#define INTEGEROVERFLOW_ERROR  -1002

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
#endif /* Alloc */

#ifdef $my
#undef $my
#endif

#ifdef self
#undef self
#endif

#endif /* VC_H */
