/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
/*
  CPU type macros

  CPU_X86: x86 or x64
  CPU_X64: x64
  CPU_PPC: PowerPC or PowerPC 64-bit
  CPU_PPC64: PowerPC 64-bit
*/
#include "fresample.h"

#if defined(_M_X64) || defined(__x86_64__)
# define CPU_X64 1
# define CPU_X86 1
#elif defined(_M_IX86) || defined(__i386__)
# define CPU_X86 1
#elif defined(__ppc64__)
# define CPU_PPC64 1
# define CPU_PPC 1
#elif defined(__ppc__)
# define CPU_PPC 1
#endif

#if defined(CPU_X86)
# define CPU_HASFLAGS 1
# define CPUF_MMX    1
# define CPUF_SSE    2
# define CPUF_SSE2   3
# define CPUF_SSE3   4
# define CPUF_SSSE3  5
# define CPUF_SSE4_1 6
# define CPUF_SSE4_2 7
#endif

#if defined(CPU_HASFLAGS)

#define CPU_SUPPORTS(f, x) (((f) & (1u << (CPUF_ ## x))) != 0)

LFR_PRIVATE extern unsigned lfr_cpuflags;

unsigned
lfr_getcpuflags(void);

#define CPU_FLAGS() \
    (lfr_cpuflags ? lfr_cpuflags : (lfr_cpuflags = lfr_getcpuflags()))

#endif
