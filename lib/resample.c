/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "defs.h"
#include "cpu.h"
#include "filter.h"
#include "resample.h"

typedef void
(*lfr_resample_func_t)(
    lfr_fixed_t *pos, lfr_fixed_t inv_ratio, unsigned *dither,
    void *out, int outlen, const void *in, int inlen,
    const struct lfr_filter *filter);

static lfr_resample_func_t
lfr_resample_s16func(int nchan, const struct lfr_filter *filter)
{
    unsigned f = CPU_FLAGS();
    lfr_ftype_t ftype = filter->type;
    (void) f;

    switch (nchan) {
    case 1:
        switch (ftype) {
        case LFR_FTYPE_S16:
#if defined(LFR_CPU_X86)
            if (f & LFR_CPUF_SSE2)
                return lfr_resample_s16n1s16_sse2;
#elif defined(LFR_CPU_PPC)
            if (f & LFR_CPUF_ALTIVEC)
                return lfr_resample_s16n1s16_altivec;
#endif
            return lfr_resample_s16n1s16_scalar;

        case LFR_FTYPE_F32:
#if defined(LFR_CPU_X86)
            if (f & LFR_CPUF_SSE2)
                return lfr_resample_s16n1f32_sse2;
#elif defined(LFR_CPU_PPC) && 0
            if (f & LFR_CPUF_ALTIVEC)
                return lfr_resample_s16n1f32_altivec;
#endif
            return lfr_resample_s16n1f32_scalar;

        default:
            break;
        }
        break;

    case 2:
        switch (ftype) {
        case LFR_FTYPE_S16:
#if defined(LFR_CPU_X86)
            if (f & LFR_CPUF_SSE2)
                return lfr_resample_s16n2s16_sse2;
#elif defined(LFR_CPU_PPC)
            if (f & LFR_CPUF_ALTIVEC)
                return lfr_resample_s16n2s16_altivec;
#endif
            return lfr_resample_s16n2s16_scalar;

        case LFR_FTYPE_F32:
#if defined(LFR_CPU_X86)
            if (f & LFR_CPUF_SSE2)
                return lfr_resample_s16n2f32_sse2;
#elif defined(LFR_CPU_PPC) && 0
            if (f & LFR_CPUF_ALTIVEC)
                return lfr_resample_s16n2f32_altivec;
#endif
            return lfr_resample_s16n2f32_scalar;

        default:
            break;
        }
        break;

    default:
        break;
    }

    return NULL;
}

LFR_PUBLIC void
lfr_resample(
    lfr_fixed_t *pos, lfr_fixed_t inv_ratio,
    unsigned *dither, int nchan,
    void *out, lfr_fmt_t outfmt, int outlen,
    const void *in, lfr_fmt_t infmt, int inlen,
    const struct lfr_filter *filter)
{
    lfr_resample_func_t func;
    if (outfmt == LFR_FMT_S16_NATIVE && infmt == LFR_FMT_S16_NATIVE) {
        func = lfr_resample_s16func(nchan, filter);
        if (!func)
            return;
        func(pos, inv_ratio, dither, out, outlen, in, inlen, filter);
    }
}
