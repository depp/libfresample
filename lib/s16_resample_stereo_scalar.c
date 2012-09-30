/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "s16.h"

void
lfr_s16_resample_stereo_scalar(
    lfr_fixed_t *LFR_RESTRICT pos, lfr_fixed_t inv_ratio,
    unsigned *dither,
    short *LFR_RESTRICT out, int outlen,
    const short *LFR_RESTRICT in, int inlen,
    const struct lfr_s16 *LFR_RESTRICT filter)
{
    int i, j, acc0, acc1, f, b, fn, ff0, ff1, off, flen;
    const short *fd;
    lfr_fixed_t x;
    unsigned ds0, ds1;

    fd = filter->data;
    flen = filter->nsamp;
    b = filter->log2nfilt;
    x = *pos;
    ds0 = *dither;
    ds1 = LCG_A * ds0 + LCG_C;

    for (i = 0; i < outlen; ++i) {
        /* acc: FIR accumulator */
        acc0 = 0;
        acc1 = 0;
        /* fn: filter number
           ff0: filter factor for filter fn
           ff1: filter factor for filter fn+1 */
        fn = (((unsigned) x >> 1) >> (31 - b)) & ((1u << b) - 1);
        ff1 = ((unsigned) x >> (32 - b - INTERP_BITS))
            & ((1u << INTERP_BITS) - 1);
        ff0 = (1u << INTERP_BITS) - ff1;
        /* off: offset in input corresponding to first sample in filter */
        off = (int) (x >> 32) - (flen >> 1);
        for (j = 0; j < flen; ++j) {
            if (j + off < 0 || j + off >= inlen)
                continue;
            f = (fd[(fn+0) * flen + j] * ff0 +
                 fd[(fn+1) * flen + j] * ff1) >> INTERP_BITS;
            acc0 += in[(j + off)*2+0] * f;
            acc1 += in[(j + off)*2+1] * f;
        }
        acc0 += (int) (ds0 >> 17);
        acc1 += (int) (ds1 >> 17);
        acc0 >>= 15;
        acc1 >>= 15;
        if (acc0 > 0x7fff)
            acc0 = 0x7fff;
        else if (acc0 < -0x8000)
            acc0 = -0x8000;
        if (acc1 > 0x7fff)
            acc1 = 0x7fff;
        else if (acc1 < -0x8000)
            acc1 = -0x8000;
        out[i*2+0] = acc0;
        out[i*2+1] = acc1;

        x += inv_ratio;
        ds0 = LCG_A2 * ds0 + LCG_C2;
        ds1 = LCG_A2 * ds1 + LCG_C2;
    }

    *pos = x;
    *dither = ds0;
}
