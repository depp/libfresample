/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#ifndef RESAMPLE_H
#define RESAMPLE_H
#include "defs.h"
#include "filter.h"

/* ======================================== */

LFR_PRIVATE void
lfr_resample_s16n1s16_scalar(
    lfr_fixed_t *pos, lfr_fixed_t inv_ratio, unsigned *dither,
    void *out, int outlen, const void *in, int inlen,
    const struct lfr_filter *filter);

LFR_PRIVATE void
lfr_resample_s16n1s16_sse2(
    lfr_fixed_t *pos, lfr_fixed_t inv_ratio, unsigned *dither,
    void *out, int outlen, const void *in, int inlen,
    const struct lfr_filter *filter);

LFR_PRIVATE void
lfr_resample_s16n1s16_altivec(
    lfr_fixed_t *pos, lfr_fixed_t inv_ratio, unsigned *dither,
    void *out, int outlen, const void *in, int inlen,
    const struct lfr_filter *filter);

/* ======================================== */

LFR_PRIVATE void
lfr_resample_s16n2s16_scalar(
    lfr_fixed_t *pos, lfr_fixed_t inv_ratio, unsigned *dither,
    void *out, int outlen, const void *in, int inlen,
    const struct lfr_filter *filter);

LFR_PRIVATE void
lfr_resample_s16n2s16_sse2(
    lfr_fixed_t *pos, lfr_fixed_t inv_ratio, unsigned *dither,
    void *out, int outlen, const void *in, int inlen,
    const struct lfr_filter *filter);

LFR_PRIVATE void
lfr_resample_s16n2s16_altivec(
    lfr_fixed_t *pos, lfr_fixed_t inv_ratio, unsigned *dither,
    void *out, int outlen, const void *in, int inlen,
    const struct lfr_filter *filter);

#endif
