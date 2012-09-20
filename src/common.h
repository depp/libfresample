/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#ifndef COMMON_H
#define COMMON_H
#include <stddef.h>

#define error(msg) error_func(__FILE__, __LINE__, msg)

#define INLINE static inline __attribute__((artificial))

__attribute__((noreturn))
void
error_func(const char *file, int line, const char *msg);

__attribute__((malloc))
void *
xmalloc(size_t size);

__attribute__((malloc))
void *
xcalloc(size_t nmemb, size_t size);

__attribute__((warn_unused_result))
void *
xrealloc(void *ptr, size_t size);

#endif
