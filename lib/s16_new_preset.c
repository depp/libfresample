/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "defs.h"

struct lfr_quality {
    unsigned short snr;
    unsigned short transition;
    unsigned short loose;
};

static const struct lfr_quality LFR_QUALITY[4] = {
    { 40, 65536/ 6, 1 }, /* low */
    { 75, 65536/ 6, 1 }, /* medium */
    { 96, 65536/12, 0 }, /* high */
    { 96, 65536/24, 0 }  /* ultra */
};

struct lfr_s16 *
lfr_s16_new_preset(
    int f_inrate, int f_outrate, int quality)
{
    const struct lfr_quality *q;

    if (quality < 0)
        q = &LFR_QUALITY[0];
    else if (quality > 3)
        q = &LFR_QUALITY[3];
    else
        q = &LFR_QUALITY[quality];

    return lfr_s16_new_resample(
        f_inrate, f_outrate,
        (double) q->snr, (double) q->transition * (1.0 / 65536), q->loose);
}
