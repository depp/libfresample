/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#define LFR_SSE2 1

#include "cpu.h"
#if defined(LFR_CPU_X86)
#include "resample.h"
#include <stdint.h>
#include <string.h>
#include <math.h>

#define LOOP_COMBINE \
    x1 = _mm_add_ps(                            \
        _mm_unpacklo_ps(a[0], a[1]),            \
        _mm_unpackhi_ps(a[0], a[1]));           \
    x2 = _mm_add_ps(                            \
        _mm_unpacklo_ps(a[2], a[3]),            \
        _mm_unpackhi_ps(a[2], a[3]));           \
    x3 = _mm_add_ps(                            \
        _mm_unpacklo_ps(a[4], a[5]),            \
        _mm_unpackhi_ps(a[4], a[5]));           \
    x4 = _mm_add_ps(                            \
        _mm_unpacklo_ps(a[6], a[7]),            \
        _mm_unpackhi_ps(a[6], a[7]));           \
                                                \
    x1 = _mm_add_ps(                            \
        _mm_movelh_ps(x1, x2),                  \
        _mm_movehl_ps(x2, x1));                 \
    x2 = _mm_add_ps(                            \
        _mm_movelh_ps(x3, x4),                  \
        _mm_movehl_ps(x4, x3));                 \
                                                \
    z1 = _mm_add_epi32(                         \
        _mm_cvtps_epi32(x1),                    \
        _mm_srli_epi32(dsv, 17));               \
    dsv = lfr_rand_epu32(dsv, lcg_a, lcg_c);    \
    z2 = _mm_add_epi32(                         \
        _mm_cvtps_epi32(x2),                    \
        _mm_srli_epi32(dsv, 17));               \
    dsv = lfr_rand_epu32(dsv, lcg_a, lcg_c);    \
                                                \
    accr = _mm_packs_epi32(                     \
        _mm_srai_epi32(z1, 15),                 \
        _mm_srai_epi32(z2, 15))

void
lfr_resample_s16n1f32_sse2(
    lfr_fixed_t *pos, lfr_fixed_t inv_ratio, unsigned *dither,
    void *out, int outlen, const void *in, int inlen,
    const struct lfr_filter *filter)
{
    int i, j, log2nfilt, fn, ff0, ff1, off, flen, fidx0, fidx1;
    float accs, ff0f, ff1f, f;
    __m128i datilo, datihi, zv, z1, z2, accr;
    __m128 ff0v, ff1v, fir0, fir1, fir2, fir3, dat0, dat1;
    __m128 acc_a, acc_b, a[8], x1, x2, x3, x4;
    __m128i dsv, lcg_a, lcg_c;
    const __m128 *fd;
    lfr_fixed_t x;
    unsigned ds;

    union {
        unsigned d[4];
        __m128i x;
    } u;

    fd = filter->data;
    flen = filter->nsamp >> 3;
    log2nfilt = filter->log2nfilt;
    x = *pos;

    ds = *dither;
    for (i = 0; i < 4; ++i) {
        u.d[i] = ds;
        ds = ds * LCG_A + LCG_C;
    }
    dsv = u.x;
    lcg_a = _mm_set1_epi32(LCG_A4);
    lcg_c = _mm_set1_epi32(LCG_C4);

    for (i = 0; i < outlen; ++i) {
        /* fn: filter number
           ff0: filter factor for filter fn
           ff1: filter factor for filter fn+1 */
        fn = (((unsigned) x >> 1) >> (31 - log2nfilt)) &
            ((1u << log2nfilt) - 1);
        ff1 = ((unsigned) x >> (32 - log2nfilt - INTERP_BITS)) &
            ((1u << INTERP_BITS) - 1);
        ff0 = (1u << INTERP_BITS) - ff1;
        ff0f = (float) ff0 * (32768.0f / (1 << INTERP_BITS));
        ff1f = (float) ff1 * (32768.0f / (1 << INTERP_BITS));

        acc_a = _mm_set_ss(0.0f);
        acc_b = _mm_set_ss(0.0f);
        /* off: offset in input corresponding to first sample in filter */
        off = (int) (x >> 32) - (flen >> 1);
        /* fidx0, fidx1: start, end indexes in FIR data */
        fidx0 = (-off + 7) >> 3;
        fidx1 = (inlen - off) >> 3;
        if (fidx0 > 0) {
            if (fidx0 >= flen)
                goto accumulate;
            accs = 0.0f;
            for (j = -off; j < fidx0 * 8; ++j) {
                f = ((const float *) fd)[(fn+0) * (flen*8) + j] * ff0f +
                    ((const float *) fd)[(fn+1) * (flen*8) + j] * ff1f;
                accs += ((const short *) in)[j + off] * f;
            }
            acc_a = _mm_set_ss(accs);
        } else {
            fidx0 = 0;
        }
        if (fidx1 < flen) {
            if (fidx1 < 0)
                goto accumulate;
            accs = 0.0f;
            for (j = fidx1 * 8; j < inlen - off; ++j) {
                f = ((const float *) fd)[(fn+0) * (flen*8) + j] * ff0f +
                    ((const float *) fd)[(fn+1) * (flen*8) + j] * ff1f;
                accs += ((const short *) in)[j + off] * f;
            }
            acc_b = _mm_set_ss(accs);
        } else {
            fidx1 = flen;
        }

        ff0v = _mm_set1_ps(ff0f);
        ff1v = _mm_set1_ps(ff1f);
        zv = _mm_set1_epi32(0);
        for (j = fidx0; j < fidx1; ++j) {
            datilo = _mm_loadu_si128(
                (const __m128i *) ((const short *) in + off + j*8));
            datihi = _mm_cmpgt_epi16(zv, datilo);
            fir0 = fd[(fn+0)*flen*2 + j*2 + 0];
            fir1 = fd[(fn+0)*flen*2 + j*2 + 1];
            fir2 = fd[(fn+1)*flen*2 + j*2 + 0];
            fir3 = fd[(fn+1)*flen*2 + j*2 + 1];
            dat0 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(datilo, datihi));
            dat1 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(datilo, datihi));
            fir0 = _mm_add_ps(_mm_mul_ps(fir0, ff0v), _mm_mul_ps(fir2, ff1v));
            fir1 = _mm_add_ps(_mm_mul_ps(fir1, ff0v), _mm_mul_ps(fir3, ff1v));
            dat0 = _mm_mul_ps(dat0, fir0);
            dat1 = _mm_mul_ps(dat1, fir1);
            acc_a = _mm_add_ps(acc_a, dat0);
            acc_b = _mm_add_ps(acc_b, dat1);
        }

    accumulate:
        a[i & 7] = _mm_add_ps(acc_a, acc_b);
        if ((i & 7) == 7) {
            LOOP_COMBINE;
            _mm_storeu_si128((__m128i *) ((short *) out + i - 7), accr);
        }

        x += inv_ratio;
    }

    ds = _mm_cvtsi128_si32(dsv);
    for (i = 0; i < (outlen & 7); ++i)
        ds = LCG_A * ds + LCG_C;
    *pos = x;
    *dither = ds;

    if ((outlen & 7) != 0) {
        LOOP_COMBINE;
        u.x = accr;
        memcpy((short *) out + (outlen & ~7), &u, (outlen & 7) * 2);
    }

}

#endif
