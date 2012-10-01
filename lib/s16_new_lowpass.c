/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "s16.h"
#include <math.h>

struct lfr_s16 *
lfr_s16_new_lowpass(
    double f_rate, double f_pass,
    double f_stop, double snr)
{
    /* FIXME: do we want to increase align for AVX? */
    int length, align=8, log2nfilt, nfilt;
    double nsnr, beta, dw, length0;

    /* Calculate length, and round up to multiple of 'align' */
    dw = (8 * atan(1.0)) * (f_stop - f_pass) / f_rate;
    length0 = (snr - 8) / (4.57 * dw) + 1;
    length = (int) ceil(length0 / align) * align;
    if (length < align)
        length = align;

    /* Recalculate side lobe height from new length */
    nsnr = (length - 1) * 4.57 * dw + 8;
    if (nsnr > 96)
        nsnr = 96;

    /* Calculate beta from side lobe height */
    if (nsnr > 50)
        beta = 0.1102 * (nsnr - 8.7);
    else if (nsnr > 21)
        beta = 0.5842 * pow(nsnr - 21, 0.4) + 0.07886 * (nsnr - 21);
    else
        beta = 0;

    /* Match sideband noise to interpolation noise */
    nfilt = (int) ceil(exp(snr * (log(10) / 40.0)) * (2.0 * f_pass) / f_rate);
    for (log2nfilt = 0;
         (1 << log2nfilt) < nfilt && log2nfilt < 8;
         ++log2nfilt) {}

    /*
    printf("nsamp=%d; cutoff=%.4f; beta=%.4f; snr=%.1f; nfilt=%d; size=%d\n",
        length, f_pass/f_rate, beta, nsnr, nfilt, length << (1 + log2nfilt));
    */

    return lfr_s16_new_sinc(length, log2nfilt, f_pass/f_rate, beta);
}
