/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "resample.h"
#include <math.h>

void
lfr_resample_s16n1f32_scalar(
    lfr_fixed_t *pos, lfr_fixed_t inv_ratio, unsigned *dither,
    void *out, int outlen, const void *in, int inlen,
    const struct lfr_filter *filter)
{
    int i, j, val, log2nfilt, fn, ff0, ff1, off, flen;
    float acc, ff0f, ff1f, f;
    const float *fd;
    const short *inp;
    short *outp;
    lfr_fixed_t x;
    unsigned ds;

    inp = in;
    outp = out;
    fd = filter->data;
    flen = filter->nsamp;
    log2nfilt = filter->log2nfilt;
    x = *pos;
    ds = *dither;

    for (i = 0; i < outlen; ++i) {
        /* acc: FIR accumulator */
        acc = 0;
        /* fn: filter number
           ff0: filter factor for filter fn
           ff1: filter factor for filter fn+1 */
        fn = (((unsigned) x >> 1) >> (31 - log2nfilt)) &
            ((1u << log2nfilt) - 1);
        ff1 = ((unsigned) x >> (32 - log2nfilt - INTERP_BITS)) &
            ((1u << INTERP_BITS) - 1);
        ff0 = (1u << INTERP_BITS) - ff1;
        ff0f = (float) ff0 * (1.0f / (1 << INTERP_BITS));
        ff1f = (float) ff1 * (1.0f / (1 << INTERP_BITS));
        /* off: offset in input corresponding to first sample in filter */
        off = (int) (x >> 32) - (flen >> 1);
        for (j = 0; j < flen; ++j) {
            if (j + off < 0 || j + off >= inlen)
                continue;
            f = fd[(fn+0) * flen + j] * ff0f +
                fd[(fn+1) * flen + j] * ff1f;
            acc += inp[j + off] * f;
        }
        acc += (float) ds * (1.0f / 4294967296.0);
        acc = floorf(acc);
        if (acc > 0x7fff)
            val = 0x7fff;
        else if (acc < -0x8000)
            val = -0x8000;
        else
            val = (int) acc;
        outp[i] = val;

        x += inv_ratio;
        ds = LCG_A * ds + LCG_C;
    }

    *pos = x;
    *dither = ds;
}
