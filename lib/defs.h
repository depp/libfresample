/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#ifndef DEFS_H
#define DEFS_H

#define LFR_IMPLEMENTATION 1

#include "fresample.h"

#define ATTR_ARTIFICIAL
#define INLINE_SPEC

#if defined(__CLANG__)
# if __has_attribute(artificial)
#  undef ATTR_ARTIFICIAL
#  define ATTR_ARTIFICIAL __attribute__((artificial))
# endif
# undef INLINE_SPEC
# define INLINE_SPEC __inline
#elif defined(__GNUC__)
# if (__GNUC__ >= 4 && __GNUC_MINOR__ >= 3) || __GNUC__ > 4
#  undef ATTR_ARTIFICIAL
#  define ATTR_ARTIFICIAL __attribute__((artificial))
# endif
# undef INLINE_SPEC
# define INLINE_SPEC __inline
#endif

#define INLINE static INLINE_SPEC ATTR_ARTIFICIAL

#if defined(LFR_SSE2)
#include <emmintrin.h>

INLINE void
lfr_storepartial_epi16(__m128i *dest, __m128i x, int i0, int i1)
{
    union {
        unsigned short h[8];
        __m128i x;
    } u;
    int i;
    u.x = x;
    for (i = i0; i < i1; ++i)
        ((unsigned short *) dest)[i] = u.h[i];
}

#endif

#if defined(LFR_ALTIVEC)
#include <altivec.h>
#endif

#endif
