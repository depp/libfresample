/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#define LFR_IMPLEMENTATION 1

#include "cpu.h"

#if defined(CPU_HASFLAGS)

unsigned lfr_cpuflags;

struct lfr_cpu_flagmap { unsigned x; unsigned y; };

static const struct lfr_cpu_flagmap LFR_CPU_FLAGS[] = {
#if defined(CPU_X86)
    { LFR_CPU_MMX, CPUF_MMX },
    { LFR_CPU_SSE, CPUF_SSE },
    { LFR_CPU_SSE2, CPUF_SSE2 },
    { LFR_CPU_SSE3, CPUF_SSE3 },
    { LFR_CPU_SSSE3, CPUF_SSSE3 },
    { LFR_CPU_SSE4_1, CPUF_SSE4_1 },
    { LFR_CPU_SSE4_2, CPUF_SSE4_2 }
#else
# error "should not get here"
#endif
};

unsigned
lfr_setcpufeatures(unsigned flags)
{
    int i, n = sizeof(LFR_CPU_FLAGS) / sizeof(*LFR_CPU_FLAGS);
    unsigned mask = 1, cpuflags, result;
    for (i = 0; i < n; ++i) {
        if (flags & LFR_CPU_FLAGS[i].x)
            mask |= 1u << LFR_CPU_FLAGS[i].y;
    }
    lfr_cpuflags = cpuflags = lfr_getcpuflags() & mask;
    result = 0;
    for (i = 0; i < n; ++i) {
        if (cpuflags & (1u << LFR_CPU_FLAGS[i].y))
            result |= LFR_CPU_FLAGS[i].x;
    }
    return result;
}

#endif

#if defined(CPU_X86)

struct lfr_cpu_idmap { signed char idflag; signed char cpuflag; };

static const struct lfr_cpu_idmap LFR_CPU_EDX[] = {
    { 23, CPUF_MMX },
    { 25, CPUF_SSE },
    { 26, CPUF_SSE2 },
    { -1, -1 }
};

static const struct lfr_cpu_idmap LFR_CPU_ECX[] = {
    { 0, CPUF_SSE3 },
    { 9, CPUF_SSSE3 },
    { 19, CPUF_SSE4_1 },
    { 20, CPUF_SSE4_2 },
    { -1, -1 }
};

static unsigned
lfr_getcpuflags_x86_1(unsigned reg, const struct lfr_cpu_idmap *mp)
{
    int i;
    unsigned fl = 0;
    for (i = 0; mp[i].idflag >= 0; ++i) {
        if ((reg & (1u << mp[i].idflag)) != 0 && mp[i].cpuflag >= 0)
            fl |= 1u << mp[i].cpuflag;
    }
    return fl;
}

static unsigned
lfr_getcpuflags_x86(unsigned edx, unsigned ecx)
{
    return 1 |
        lfr_getcpuflags_x86_1(edx, LFR_CPU_EDX) |
        lfr_getcpuflags_x86_1(ecx, LFR_CPU_ECX);
}

#if defined(__GNUC__)

unsigned
lfr_getcpuflags(void)
{
    unsigned a, b, c, d;
    __asm__(
        "cpuid"
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
        : "0"(1));
    return lfr_getcpuflags_x86(d, c);
}

#elif defined(_MSC_VER)

unsigned
lfr_getcpuflags(void)
{
    int info[4];
    __cpuid(info, 1);
    return lfr_getcpuflags_x86(info[3], info[2]);
}

#else
#warning "Unknown compiler, no CPUID support"

unsigned
lfr_getcpuflags(void)
{
    return 1;
}

#endif

#endif
