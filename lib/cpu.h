/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "defs.h"

#if defined(LFR_CPU_X86) || defined(LFR_CPU_PPC)
# define CPU_HASFLAGS 1
#endif

#if defined(CPU_HASFLAGS)

LFR_PRIVATE extern unsigned lfr_cpuflags;

unsigned
lfr_getcpuflags(void);

#define CPU_FLAGS() \
    (lfr_cpuflags ? lfr_cpuflags : (lfr_cpuflags = lfr_getcpuflags()))

#endif
