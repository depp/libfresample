/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#define LFR_SSE2 1

#include "cpu.h"
#if defined(LFR_CPU_X86)
#include "resample.h"
#include <string.h>

#define LOOP_FIRCOEFF \
    fn = (((unsigned) x >> 1) >> (31 - log2nfilt)) &            \
        ((1u << log2nfilt) - 1);                                \
    ff1 = ((unsigned) x >> (32 - log2nfilt - INTERP_BITS)) &    \
        ((1u << INTERP_BITS) - 1);                              \
    ff0 = (1u << INTERP_BITS) - ff1;                            \
    fir_interp = _mm_set1_epi32(ff0 | (ff1 << 16));


#define LOOP_COMBINE \
    x1 = _mm_add_epi32(                                 \
        _mm_unpacklo_epi32(a[0], a[1]),                 \
        _mm_unpackhi_epi32(a[0], a[1]));                \
    x2 = _mm_add_epi32(                                 \
        _mm_unpacklo_epi32(a[2], a[3]),                 \
        _mm_unpackhi_epi32(a[2], a[3]));                \
    x3 = _mm_add_epi32(                                 \
        _mm_unpacklo_epi32(a[4], a[5]),                 \
        _mm_unpackhi_epi32(a[4], a[5]));                \
    x4 = _mm_add_epi32(                                 \
        _mm_unpacklo_epi32(a[6], a[7]),                 \
        _mm_unpackhi_epi32(a[6], a[7]));                \
                                                        \
    x1 = _mm_add_epi32(                                 \
        _mm_unpacklo_epi64(x1, x2),                     \
        _mm_unpackhi_epi64(x1, x2));                    \
    x2 = _mm_add_epi32(                                 \
        _mm_unpacklo_epi64(x3, x4),                     \
        _mm_unpackhi_epi64(x3, x4));                    \
                                                        \
    x1 = _mm_add_epi32(x1, _mm_srli_epi32(dsv, 17));    \
    dsv = lfr_rand_epu32(dsv, lcg_a, lcg_c);            \
    x2 = _mm_add_epi32(x2, _mm_srli_epi32(dsv, 17));    \
    dsv = lfr_rand_epu32(dsv, lcg_a, lcg_c);            \
                                                        \
    acc = _mm_packs_epi32(                              \
        _mm_srai_epi32(x1, 15),                         \
        _mm_srai_epi32(x2, 15))

#define LOOP_STORE \
    a[i & 7] = acc;                                                 \
    if ((i & 7) == 7) {                                             \
        LOOP_COMBINE;                                               \
        _mm_storeu_si128((__m128i *) ((short *) out + i - 7), acc); \
    }

void
lfr_resample_s16n1s16_sse2(
    lfr_fixed_t *pos, lfr_fixed_t inv_ratio, unsigned *dither,
    void *out, int outlen, const void *in, int inlen,
    const struct lfr_filter *filter)
{
    const __m128i *fd;
    int flen, log2nfilt;
    lfr_fixed_t x;

    __m128i acc, fir0, fir1, fir_interp, dat0;
    __m128i a[8], x1, x2, x3, x4;
    __m128i dsv, lcg_a, lcg_c;
    int fn, ff0, ff1, off, fidx0, fidx1;
    int accs, i, j, f;
    unsigned ds;

    union {
        unsigned d[4];
        __m128i x;
    } u;

    /* fd: Pointer to beginning of filter coefficients, aligned.  */
    fd = filter->data;
    /* flen: Length of filter, measured in 128-bit words.  */
    flen = filter->nsamp >> 3;
    /* log2nfilt: Base 2 logarithm of the number of filters.  */
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

    switch (flen) {
    case 1: goto flen1;
    case 2: goto flen2;
    case 3: goto flen3;
    default: goto flenv;
    }

flenv:
    for (i = 0; i < outlen; ++i) {
        LOOP_FIRCOEFF;

        /* acc: FIR accumulator, as four 32-bit words.  The four words
           must be added, shifted, and packed before they can be
           stored.  */
        acc = _mm_set1_epi32(0);
        /* off: offset in input corresponding to first sample in
           filter */
        off = (int) (x >> 32);
        /* fixd0, fidx1: start, end indexes of 8-word (16-byte) chunks
           of whole FIR data we will use */
        fidx0 = (-off + 7) >> 3;
        fidx1 = (inlen - off) >> 3;
        if (fidx0 > 0) {
            if (fidx0 > flen)
                goto flenv_accumulate;
            accs = 0;
            for (j = -off; j < fidx0 * 8; ++j) {
                f = (((const short *) fd)[(fn+0) * (flen*8) + j] * ff0 +
                     ((const short *) fd)[(fn+1) * (flen*8) + j] * ff1)
                    >> INTERP_BITS;
                accs += ((const short *) in)[j + off] * f;
            }
            acc = _mm_cvtsi32_si128(accs);
        } else {
            fidx0 = 0;
        }
        if (fidx1 < flen) {
            if (fidx1 < 0)
                goto flenv_accumulate;
            accs = 0;
            for (j = fidx1 * 8; j < inlen - off; ++j) {
                f = (((const short *) fd)[(fn+0) * (flen*8) + j] * ff0 +
                     ((const short *) fd)[(fn+1) * (flen*8) + j] * ff1)
                    >> INTERP_BITS;
                accs += ((const short *) in)[j + off] * f;
            }
            acc = _mm_add_epi32(acc, _mm_cvtsi32_si128(accs));
        } else {
            fidx1 = flen;
        }

        for (j = fidx0; j < fidx1; ++j) {
            dat0 = _mm_loadu_si128(
                (const __m128i *)
                ((const short *) in + off + j*8));
            fir0 = fd[(fn+0)*flen + j];
            fir1 = fd[(fn+1)*flen + j];
            fir0 = _mm_packs_epi32(
                _mm_srai_epi32(
                    _mm_madd_epi16(
                        _mm_unpacklo_epi16(fir0, fir1),
                        fir_interp),
                    INTERP_BITS),
                _mm_srai_epi32(
                    _mm_madd_epi16(
                        _mm_unpackhi_epi16(fir0, fir1),
                        fir_interp),
                    INTERP_BITS));
            acc = _mm_add_epi32(acc, _mm_madd_epi16(dat0, fir0));
        }

    flenv_accumulate:
        LOOP_STORE;

        x += inv_ratio;
    }
    goto final;

flen1:
    for (i = 0; i < outlen; ++i) {
        LOOP_FIRCOEFF;

        /* off: offset in input corresponding to first sample in
           filter */
        off = (int) (x >> 32);
        /* fixd0, fidx1: start, end indexes of 8-word (16-byte) chunks
           of whole FIR data we will use */
        fidx0 = (-off + 7) >> 3;
        fidx1 = (inlen - off) >> 3;
        if (fidx0 > 0)
            goto flen1_slow;
        if (fidx1 < 1)
            goto flen1_slow;

        dat0 = _mm_loadu_si128(
            (const __m128i *)
            ((const short *) in + off));
        fir0 = fd[fn+0];
        fir1 = fd[fn+1];
        fir0 = _mm_packs_epi32(
            _mm_srai_epi32(
                _mm_madd_epi16(
                    _mm_unpacklo_epi16(fir0, fir1),
                    fir_interp),
                INTERP_BITS),
            _mm_srai_epi32(
                _mm_madd_epi16(
                    _mm_unpackhi_epi16(fir0, fir1),
                    fir_interp),
                INTERP_BITS));

        acc = _mm_madd_epi16(dat0, fir0);

        goto flen1_accumulate;

    flen1_slow:
        fidx0 = -off;
        if (fidx0 < 0)
            fidx0 = 0;
        fidx1 = inlen - off;
        if (fidx1 > 8)
            fidx1 = 8;

        accs = 0;
        for (j = fidx0; j < fidx1; ++j) {
            f = (((const short *) fd)[(fn+0) * 8 + j] * ff0 +
                 ((const short *) fd)[(fn+1) * 8 + j] * ff1)
                >> INTERP_BITS;
            accs += ((const short *) in)[j + off] * f;
        }
        acc = _mm_cvtsi32_si128(accs);
        goto flen1_accumulate;

    flen1_accumulate:
        LOOP_STORE;

        x += inv_ratio;
    }
    goto final;

flen2:
    for (i = 0; i < outlen; ++i) {
        LOOP_FIRCOEFF;

        /* off: offset in input corresponding to first sample in
           filter */
        off = (int) (x >> 32);
        /* fixd0, fidx1: start, end indexes of 8-word (16-byte) chunks
           of whole FIR data we will use */
        fidx0 = (-off + 7) >> 3;
        fidx1 = (inlen - off) >> 3;
        if (fidx0 > 0)
            goto flen2_slow;
        if (fidx1 < 2)
            goto flen2_slow;

        dat0 = _mm_loadu_si128(
            (const __m128i *)
            ((const short *) in + off + 0));
        fir0 = fd[(fn+0)*2 + 0];
        fir1 = fd[(fn+1)*2 + 0];
        fir0 = _mm_packs_epi32(
            _mm_srai_epi32(
                _mm_madd_epi16(
                    _mm_unpacklo_epi16(fir0, fir1),
                    fir_interp),
                INTERP_BITS),
            _mm_srai_epi32(
                _mm_madd_epi16(
                    _mm_unpackhi_epi16(fir0, fir1),
                    fir_interp),
                INTERP_BITS));

        acc = _mm_madd_epi16(dat0, fir0);

        dat0 = _mm_loadu_si128(
            (const __m128i *)
            ((const short *) in + off + 8));
        fir0 = fd[(fn+0)*2 + 1];
        fir1 = fd[(fn+1)*2 + 1];
        fir0 = _mm_packs_epi32(
            _mm_srai_epi32(
                _mm_madd_epi16(
                    _mm_unpacklo_epi16(fir0, fir1),
                    fir_interp),
                INTERP_BITS),
            _mm_srai_epi32(
                _mm_madd_epi16(
                    _mm_unpackhi_epi16(fir0, fir1),
                    fir_interp),
                INTERP_BITS));
        acc = _mm_add_epi32(acc, _mm_madd_epi16(dat0, fir0));

        goto flen2_accumulate;

    flen2_slow:
        fidx0 = -off;
        if (fidx0 < 0)
            fidx0 = 0;
        fidx1 = inlen - off;
        if (fidx1 > 16)
            fidx1 = 16;

        accs = 0;
        for (j = fidx0; j < fidx1; ++j) {
            f = (((const short *) fd)[(fn+0) * 16 + j] * ff0 +
                 ((const short *) fd)[(fn+1) * 16 + j] * ff1)
                >> INTERP_BITS;
            accs += ((const short *) in)[j + off] * f;
        }
        acc = _mm_cvtsi32_si128(accs);
        goto flen2_accumulate;

    flen2_accumulate:
        LOOP_STORE;

        x += inv_ratio;
    }
    goto final;

flen3:
    for (i = 0; i < outlen; ++i) {
        LOOP_FIRCOEFF;

        /* off: offset in input corresponding to first sample in
           filter */
        off = (int) (x >> 32);
        /* fixd0, fidx1: start, end indexes of 8-word (16-byte) chunks
           of whole FIR data we will use */
        fidx0 = (-off + 7) >> 3;
        fidx1 = (inlen - off) >> 3;
        if (fidx0 > 0)
            goto flen3_slow;
        if (fidx1 < 3)
            goto flen3_slow;

        dat0 = _mm_loadu_si128(
            (const __m128i *)
            ((const short *) in + off + 0));
        fir0 = fd[(fn+0)*3 + 0];
        fir1 = fd[(fn+1)*3 + 0];
        fir0 = _mm_packs_epi32(
            _mm_srai_epi32(
                _mm_madd_epi16(
                    _mm_unpacklo_epi16(fir0, fir1),
                    fir_interp),
                INTERP_BITS),
            _mm_srai_epi32(
                _mm_madd_epi16(
                    _mm_unpackhi_epi16(fir0, fir1),
                    fir_interp),
                INTERP_BITS));

        acc = _mm_madd_epi16(dat0, fir0);

        dat0 = _mm_loadu_si128(
            (const __m128i *)
            ((const short *) in + off + 8));
        fir0 = fd[(fn+0)*3 + 1];
        fir1 = fd[(fn+1)*3 + 1];
        fir0 = _mm_packs_epi32(
            _mm_srai_epi32(
                _mm_madd_epi16(
                    _mm_unpacklo_epi16(fir0, fir1),
                    fir_interp),
                INTERP_BITS),
            _mm_srai_epi32(
                _mm_madd_epi16(
                    _mm_unpackhi_epi16(fir0, fir1),
                    fir_interp),
                INTERP_BITS));
        acc = _mm_add_epi32(acc, _mm_madd_epi16(dat0, fir0));

        dat0 = _mm_loadu_si128(
            (const __m128i *)
            ((const short *) in + off + 16));
        fir0 = fd[(fn+0)*3 + 2];
        fir1 = fd[(fn+1)*3 + 2];
        fir0 = _mm_packs_epi32(
            _mm_srai_epi32(
                _mm_madd_epi16(
                    _mm_unpacklo_epi16(fir0, fir1),
                    fir_interp),
                INTERP_BITS),
            _mm_srai_epi32(
                _mm_madd_epi16(
                    _mm_unpackhi_epi16(fir0, fir1),
                    fir_interp),
                INTERP_BITS));
        acc = _mm_add_epi32(acc, _mm_madd_epi16(dat0, fir0));

        goto flen3_accumulate;

    flen3_slow:
        fidx0 = -off;
        if (fidx0 < 0)
            fidx0 = 0;
        fidx1 = inlen - off;
        if (fidx1 > 24)
            fidx1 = 24;

        accs = 0;
        for (j = fidx0; j < fidx1; ++j) {
            f = (((const short *) fd)[(fn+0) * 24 + j] * ff0 +
                 ((const short *) fd)[(fn+1) * 24 + j] * ff1)
                >> INTERP_BITS;
            accs += ((const short *) in)[j + off] * f;
        }
        acc = _mm_cvtsi32_si128(accs);
        goto flen3_accumulate;

    flen3_accumulate:
        LOOP_STORE;

        x += inv_ratio;
    }
    goto final;

final:
    ds = _mm_cvtsi128_si32(dsv);
    for (i = 0; i < (outlen & 7); ++i)
        ds = LCG_A * ds + LCG_C;
    *pos = x;
    *dither = ds;

    /* Store remaing bytes */
    acc = _mm_set1_epi32(0);
    if ((outlen & 7) != 0) {
        LOOP_COMBINE;
        u.x = acc;
        memcpy((short *) out + (outlen & ~7), &u, 2 * (outlen & 7));
    }
}

#endif
