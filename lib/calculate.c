#include "calculate.h"
#include <math.h>

/*
  Simple sinc function implementation.  Note that this is the
  unnormalized sinc function, with zeroes at nonzero multiples of pi.
*/
static double
sinc(double x)
{
    return fabs(x) < 1e-8 ? 1.0 : sin(x) / x;
}

/*
  Modified Bessel function of the first kind of order 0, i0.  Only
  considered valid over the domain [0,18].

  This is the naive algorithm.

  In "Computation of Special Functions" by Shanjie Zhang and Jianming
  Jin, this technique is only used for 0<X<18.  However, we only need
  it for the Kaiser window, and a beta of 18 is fairly ridiculous --
  it gives a side lobe height of -180 dB, which is not only beyond the
  actual SNR of high end recording systems, it is beyond the SNR of
  theoretically perfect 24-bit systems.
*/
static double
bessel_i0(double x)
{
    double a = 1.0, y = 1.0, x2 = x*x;
    int i;
    for (i = 1; i <= 50; ++i) {
        a *= 0.25 * x2 / (i * i);
        y += a;
        if (a < y * 1e-15)
            break;
    }
    return y;
}

void
lfr_s16_calculate(short *LFR_RESTRICT data, int nsamp, int nfilt,
                  double offset, double cutoff, double beta)
{
    double kscale, yscale, sscale, t, w, x;
    int i, j;
    kscale = 2.0 / (nsamp - 1);
    yscale = 32767.0 * 2.0 * cutoff / bessel_i0(beta);
    sscale = (8.0 * atan(1.0)) * cutoff;
    for (i = 0; i < nsamp; ++i) {
        t = kscale * i - 1.0;
        w = yscale * bessel_i0(beta * sqrt(1.0 - t * t));
        for (j = 0; j <= nfilt; ++j) {
            x = (i - (nsamp - 1) / 2) - j * offset;
            data[j * nsamp + i] = (short) (w * sinc(sscale * x));
        }
    }
}

void
lfr_f32_calculate(float *LFR_RESTRICT data, int nsamp, int nfilt,
                  double offset, double cutoff, double beta)
{
    double kscale, yscale, sscale, t, w, x;
    int i, j;
    kscale = 2.0 / (nsamp - 1);
    yscale = 2.0 * cutoff / bessel_i0(beta);
    sscale = (8.0 * atan(1.0)) * cutoff;
    for (i = 0; i < nsamp; ++i) {
        t = kscale * i - 1.0;
        w = yscale * bessel_i0(beta * sqrt(1.0 - t * t));
        for (j = 0; j <= nfilt; ++j) {
            x = (i - (nsamp - 1) / 2) - j * offset;
            data[j * nsamp + i] = (float) (w * sinc(sscale * x));
        }
    }
}
