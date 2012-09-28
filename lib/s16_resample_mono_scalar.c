/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#define LFR_IMPLEMENTATION 1

#include "s16.h"

void
lfr_s16_resample_mono_scalar(
    lfr_fixed_t *LFR_RESTRICT pos, lfr_fixed_t inv_ratio,
    short *LFR_RESTRICT out, int outlen,
    const short *LFR_RESTRICT in, int inlen,
    const struct lfr_s16 *LFR_RESTRICT filter)
{
    int i, j, acc, f, b, fn, ff0, ff1, off, flen;
    const short *fd;
    lfr_fixed_t x;

    fd = filter->data;
    flen = filter->nsamp;
    b = filter->log2nfilt;
    x = *pos;

    for (i = 0; i < outlen; ++i) {
        /* acc: FIR accumulator */
        acc = 0;
        /* fn: filter number
           ff0: filter factor for filter fn
           ff1: filter factor for filter fn+1 */
        fn = (((unsigned) x >> 1) >> (31 - b)) & ((1u << b) - 1);
        ff1 = ((unsigned) x >> (32 - b - INTERP_BITS)) &
            ((1u << INTERP_BITS) - 1);
        ff0 = (1u << INTERP_BITS) - ff1;
        /* off: offset in input corresponding to first sample in filter */
        off = (int) (x >> 32) - (flen >> 1);
        for (j = 0; j < flen; ++j) {
            if (j + off < 0 || j + off >= inlen)
                continue;
            f = (fd[(fn+0) * flen + j] * ff0 +
                 fd[(fn+1) * flen + j] * ff1) >> INTERP_BITS;
            acc += in[j + off] * f;
        }
        acc >>= 15;
        if (acc > 0x7fff)
            acc = 0x7fff;
        else if (acc < -0x8000)
            acc = -0x8000;
        out[i] = acc;

        x += inv_ratio;
    }

    *pos = x;
}
