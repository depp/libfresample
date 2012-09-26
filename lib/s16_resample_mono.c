/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#define LFR_IMPLEMENTATION 1

#include "cpu.h"
#include "s16.h"

void
lfr_s16_resample_mono(
    short *LFR_RESTRICT out, size_t outlen, int outrate,
    const short *LFR_RESTRICT in, size_t inlen, int inrate,
    const struct lfr_s16 *LFR_RESTRICT filter)
{
#if defined(LFR_CPU_X86)
    unsigned f = CPU_FLAGS();
    if (f & LFR_CPUF_SSE2) {
        lfr_s16_resample_mono_sse2(
            out, outlen, outrate, in, inlen, inrate, filter);
        return;
    }
#endif
    lfr_s16_resample_mono_scalar(
        out, outlen, outrate, in, inlen, inrate, filter);
}
