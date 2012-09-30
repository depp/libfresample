/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "calculate.h"
#include "s16.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/*
  NOTE: log2nfilt is clipped to 0..8.  The paper by JOS recommends
  using 2^(N/2) filters, where N is the number of bits per sample, so
  8 is the maximum here.
*/
struct lfr_s16 *
lfr_s16_new_sinc(
    int nsamp, int log2nfilt, double cutoff, double beta)
{
    int nfilt;
    struct lfr_s16 *fp;
    size_t align = 16, sz;

    if (nsamp < 1 || log2nfilt < 0)
        return NULL;
    if (log2nfilt > 8)
        log2nfilt = 8;
    nfilt = 1 << log2nfilt;
    if (((size_t) -1 - (align - 1) - sizeof(*fp)) /
        (((size_t) 1 << log2nfilt) + 1) < (size_t) nsamp * sizeof(short))
        return NULL;

    sz = nsamp * (((size_t) 1 << log2nfilt) + 1) * sizeof(short);
    fp = malloc(sizeof(*fp) + sz + align - 1);
    if (!fp)
        return NULL;

    /* The casts to 'void *' suppress alignment warnings.  */
    fp->data = (short *) (void *)
        (((uintptr_t) (void *) (fp + 1) + align - 1) &
         ~((uintptr_t) align - 1));
    fp->nsamp = nsamp;
    fp->log2nfilt = log2nfilt;

    lfr_s16_calculate(fp->data, nsamp, nfilt,
                      1.0 / (double) nfilt, cutoff, beta);

    return fp;
}
