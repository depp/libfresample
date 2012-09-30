/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "defs.h"

struct lfr_s16 *
lfr_s16_new_resample(
    int f_inrate, int f_outrate,
    double snr, double transition, int loose)
{
    double f_minrate, f_stop, f_pass, f_limit = 2e4, t;
    f_minrate = (double) (f_inrate < f_outrate ? f_inrate : f_outrate);

    f_stop = 0.5 * f_minrate;
    if (loose) {
        t = f_minrate - f_limit;
        if (t > f_stop)
            f_stop = t;
    }

    f_pass = f_stop - transition * (double) f_inrate;
    t = 0.25 * f_minrate;
    if (t > f_pass)
        f_pass = t;
    if (f_pass > f_limit)
        f_pass = f_limit;

    return lfr_s16_new_lowpass(
        (double) f_inrate, f_pass, f_stop, snr);
}
