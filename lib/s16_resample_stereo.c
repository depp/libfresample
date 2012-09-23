/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "s16.h"
#include <stdint.h>

LFR_PUBLIC void
lfr_s16_resample_stereo(
    short *LFR_RESTRICT out, size_t outlen, int outrate,
    const short *LFR_RESTRICT in, size_t inlen, int inrate,
    const struct lfr_s16 *LFR_RESTRICT filter)
{
    int i, j, pf, sf, si, acc0, acc1, f, b, fn, ff0, ff1, off, flen;
    const short *fd;
    uint64_t pi, tmp64;

    fd = filter->data;
    flen = filter->nsamp;
    b = filter->log2nfilt;

    /* pi, pf: position, integral and fractional
        si, sf: delta position, integral and fractional */
    pi = 0;
    pf = 0;
    tmp64 = (uint64_t) inrate << (b + INTERP_BITS);
    si = (int) (tmp64 / outrate);
    sf = (int) (tmp64 % outrate);

    for (i = 0; i < outlen; ++i) {
        /* acc: FIR accumulator */
        acc0 = 0;
        acc1 = 0;
        /* fn: filter number
           ff0: filter factor for filter fn
           ff1: filter factor for filter fn+1 */
        fn = ((unsigned) pi >> INTERP_BITS) & ((1u << b) - 1);
        ff1 = (unsigned) pi & ((1u << INTERP_BITS) - 1);
        ff0 = (1u << INTERP_BITS) - ff1;
        /* off: offset in input corresponding to first sample in filter */
        off = (int) (pi >> (INTERP_BITS + b)) - (flen >> 1);
        for (j = 0; j < flen; ++j) {
            if (j + off < 0 || j + off >= inlen)
                continue;
            f = (fd[(fn+0) * flen + j] * ff0 +
                 fd[(fn+1) * flen + j] * ff1) >> INTERP_BITS;
            acc0 += in[(j + off)*2+0] * f;
            acc1 += in[(j + off)*2+1] * f;
        }
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

        pf += sf;
        pi += si;
        if (pf >= outrate) {
            pf -= outrate;
            pi += 1;
        }
    }
}
