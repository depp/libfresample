/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "s16.h"
#include <stdint.h>

LFR_PUBLIC void
lfr_s16_resample_mono(
    short *LFR_RESTRICT out, size_t outlen, int outrate,
    const short *LFR_RESTRICT in, size_t inlen, int inrate,
    const struct lfr_s16 *LFR_RESTRICT filter)
{
    int i, j, pf, sf, si, acc, f, b, fn, ff0, ff1, off, flen;
    const short *fd;
    uint64_t pi, tmp64;

    fd = filter->data;
    flen = filter->nsamp;
    b = filter->log2nfilt;

    /* pi, pf: position, integral and fractional
        si, sf: delta position, integral and fractional */
    pi = 0;
    pf = 0;
    tmp64 = (uint64_t) inrate << (b + 16);
    si = (int) (tmp64 / outrate);
    sf = (int) (tmp64 % outrate);

    for (i = 0; i < outlen; ++i) {
        /* acc: FIR accumulator */
        acc = 0;
        /* fn: filter number
           ff0: filter factor for filter fn
           ff1: filter factor for filter fn+1 */
        fn = ((unsigned) pi >> 16) & ((1u << b) - 1);
        ff1 = (unsigned) pi & ((1u << 16) - 1);
        ff0 = (1u << 16) - ff1;
        /* off: offset in input corresponding to first sample in filter */
        off = (int) (pi >> (16 + b)) - flen / 2;
        for (j = 0; j < flen; ++j) {
            if (j + off < 0 || j + off >= inlen)
                continue;
            f = (fd[(fn+0) * flen + j] * ff0 +
                 fd[(fn+1) * flen + j] * ff1) >> 16;
            acc += in[j + off] * f;
        }
        acc >>= 15;
        if (acc > 0x7fff)
            acc = 0x7fff;
        else if (acc < -0x8000)
            acc = -0x8000;
        out[i] = acc;

        pf += sf;
        pi += si;
        if (pf >= outrate) {
            pf -= outrate;
            pi += 1;
        }
    }
}
