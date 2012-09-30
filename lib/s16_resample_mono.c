/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "cpu.h"
#include "s16.h"

void
lfr_s16_resample_mono(
    lfr_fixed_t *LFR_RESTRICT pos, lfr_fixed_t inv_ratio,
    short *LFR_RESTRICT out, int outlen,
    const short *LFR_RESTRICT in, int inlen,
    const struct lfr_s16 *LFR_RESTRICT filter)
{
#if defined(LFR_CPU_X86)
    unsigned f = CPU_FLAGS();
    if (f & LFR_CPUF_SSE2) {
        lfr_s16_resample_mono_sse2(
            pos, inv_ratio, out, outlen, in, inlen, filter);
        return;
    }
#elif defined(LFR_CPU_PPC)
    unsigned f = CPU_FLAGS();
    if (f & LFR_CPUF_ALTIVEC) {
        lfr_s16_resample_mono_altivec(
            pos, inv_ratio, out, outlen, in, inlen, filter);
        return;
    }
#endif
    lfr_s16_resample_mono_scalar(
        pos, inv_ratio, out, outlen, in, inlen, filter);
}
