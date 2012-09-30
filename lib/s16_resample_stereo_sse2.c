/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#define LFR_SSE2 1

#include "cpu.h"
#if defined(LFR_CPU_X86)
#include "s16.h"
#include <stdint.h>

#define UNALIGNED_LOAD 1

#define LOOP_LOADFIR \
    fir0 = firp[(fn+0)*flen + i];                   \
    fir1 = firp[(fn+1)*flen + i];                   \
                                                    \
    fir0 = _mm_packs_epi32(                         \
        _mm_srai_epi32(                             \
            _mm_madd_epi16(                         \
            _mm_unpacklo_epi16(fir0, fir1),         \
            fir_interp),                            \
            INTERP_BITS),                           \
        _mm_srai_epi32(                             \
            _mm_madd_epi16(                         \
                _mm_unpackhi_epi16(fir0, fir1),     \
                fir_interp),                        \
            INTERP_BITS));                          \
                                                    \
    fir1 = _mm_unpackhi_epi16(fir0, fir0);          \
    fir0 = _mm_unpacklo_epi16(fir0, fir0)

#define LOOP_ACCUM \
    acc = _mm_add_epi32(                            \
        acc,                                        \
        _mm_add_epi32(                              \
            _mm_madd_epi16(                         \
                _mm_unpacklo_epi16(dat0, dat1),     \
                _mm_unpacklo_epi16(fir0, fir1)),    \
            _mm_madd_epi16(                         \
                _mm_unpackhi_epi16(dat0, dat1),     \
                _mm_unpackhi_epi16(fir0, fir1))))


#define LOOP_ALIGN0 \
    for (i = fidx0; i < fidx1; ++i) {       \
        dat0 = inp[(off >> 2) + i*2 + 0];   \
        dat1 = inp[(off >> 2) + i*2 + 1];   \
        LOOP_LOADFIR;                       \
        LOOP_ACCUM;                         \
    }

#define LOOP_ALIGN(n) \
    dat2 = inp[(off >> 2) + fidx0*2];           \
    for (i = fidx0; i < fidx1; ++i) {           \
        dat0 = dat2;                            \
        dat1 = inp[(off >> 2) + i*2 + 1];       \
        dat2 = inp[(off >> 2) + i*2 + 2];       \
        dat0 = _mm_or_si128(                    \
            _mm_srli_si128(dat0, (n)*2),        \
            _mm_slli_si128(dat1, 16-(n)*2));    \
        dat1 = _mm_or_si128(                    \
            _mm_srli_si128(dat1, (n)*2),        \
            _mm_slli_si128(dat2, 16-(n)*2));    \
        LOOP_LOADFIR;                           \
        LOOP_ACCUM;                             \
    }

#define LOOP_UNALIGNED \
    for (i = fidx0; i < fidx1; ++i) {                   \
        dat0 = _mm_loadu_si128(                         \
            (const __m128i *)                           \
            ((const short *) inp + off*2 + i*16));      \
        dat1 = _mm_loadu_si128(                         \
            (const __m128i *)                           \
            ((const short *) inp + off*2 + i*16+8));    \
        LOOP_LOADFIR;                                   \
        LOOP_ACCUM;                                     \
    }

static __inline void
lfr_storepartial0_epi16(__m128i x, int b, __m128i *dest)
{
    union {
        unsigned short h[8];
        __m128i x;
    } u;
    int i;
    u.x = x;
    for (i = (b & 7); i < 8; ++i)
        ((short *) dest)[i] = u.h[i];
}

static __inline void
lfr_storepartial1_epi16(__m128i x, int b, __m128i *dest)
{
    union {
        unsigned short h[8];
        __m128i x;
    } u;
    int i;
    u.x = x;
    for (i = 0; i < (b & 7); ++i)
        ((short *) dest)[i] = u.h[i];
}

void
lfr_s16_resample_stereo_sse2(
    lfr_fixed_t *LFR_RESTRICT pos, lfr_fixed_t inv_ratio,
    unsigned *dither,
    short *LFR_RESTRICT out, int outlen,
    const short *LFR_RESTRICT in, int inlen,
    const struct lfr_s16 *LFR_RESTRICT filter)
{
    const __m128i *firp, *inp;
    __m128i *outp;
    int in0, in1, out0, out1, outidx;
    int flen, log2nfilt;
    lfr_fixed_t x;

    __m128i acc, acc0, acc1, fir0, fir1, fir_interp, dat0, dat1;
    __m128i dsv, lcg_a, lcg_c;
#if !UNALIGNED_LOAD
    __m128i dat2;
#endif
    int fn, ff0, ff1, off0, off, fidx0, fidx1;
    int accs0, accs1, i, f, t;
    unsigned ds;

    union {
        unsigned d[4];
        __m128i x;
    } u;

    (void) dither;

    /* firp: Pointer to beginning of filter coefficients, aligned.  */
    firp = (const __m128i *) filter->data;
    /* flen: Length of filter, measured in 128-bit words.  */
    flen = filter->nsamp >> 3;
    off0 = (filter->nsamp >> 1);
    /* log2nfilt: Base 2 logarithm of the number of filters.  */
    log2nfilt = filter->log2nfilt;

    /* in0, in1: Frame index of input start and end, measured from
       aligned input pointer.  inp: aligned input pointer.  */
    in0 = ((uintptr_t) in >> 2) & 3;
    in1 = inlen + in0;
    inp = (const __m128i *) (in - in0 * 2);

    /* out0, out1: Frame index of output start and end, measured from
       aligned output pointer.  outp: aligned output pointer.  */
    out0 = ((uintptr_t) out >> 2) & 3;
    out1 = outlen + out0;
    outp = (__m128i *) (out - out0 * 2);

    x = *pos + ((lfr_fixed_t) in0 << 32);
    ds = *dither;
    for (i = 0; i < (out0 & 3) * 2; ++i)
        ds = LCG_AI * ds + LCG_CI;
    for (i = 0; i < 4; ++i) {
        u.d[i] = ds;
        ds = ds * LCG_A + LCG_C;
    }
    dsv = u.x;
    lcg_a = _mm_set1_epi32(LCG_A4);
    lcg_c = _mm_set1_epi32(LCG_C4);

    acc0 = _mm_set1_epi32(0);
    acc1 = _mm_set1_epi32(0);
    for (outidx = out0; outidx < out1; ++outidx) {
        /* acc: FIR accumulator, used for accumulating 32-bit values
           in the format L R L R.  This corresponds to one frame of
           output, so the pair of values for left and the pair for
           right have to be summed later.  */
        acc = _mm_set1_epi32(0);

        /* fn: filter number
           ff0: filter factor for filter fn
           ff1: filter factor for filter fn+1 */
        fn = (((unsigned) x >> 1) >> (31 - log2nfilt)) &
            ((1u << log2nfilt) - 1);
        ff1 = ((unsigned) x >> (32 - log2nfilt - INTERP_BITS)) &
            ((1u << INTERP_BITS) - 1);
        ff0 = (1u << INTERP_BITS) - ff1;
        fir_interp = _mm_set1_epi32(ff0 | (ff1 << 16));

        /* off: offset in input corresponding to first sample in
           filter */
        off = (int) (x >> 32) - off0;

        /* fixd0, fidx1: start, end indexes of 8-word (16-byte) chunks
           of whole FIR data we will use */
        fidx0 = (in0 - off + 7) >> 3;
        fidx1 = (in1 - off) >> 3;
        if (fidx0 > 0) {
            if (fidx0 > flen)
                goto accumulate;
            accs0 = 0;
            accs1 = 0;
            t = (off > in0) ? off : in0;
            for (i = t; i < fidx0 * 8 + off; ++i) {
                f = (((const short *) firp)[(fn+0)*flen*8 + i - off] * ff0 +
                     ((const short *) firp)[(fn+1)*flen*8 + i - off] * ff1)
                    >> INTERP_BITS;
                accs0 += ((const short *) inp)[i*2+0] * f;
                accs1 += ((const short *) inp)[i*2+1] * f;
            }
            acc = _mm_set_epi32(0, 0, accs1, accs0);
        } else {
            fidx0 = 0;
        }
        if (fidx1 < flen) {
            if (fidx1 < 0)
                goto accumulate;
            accs0 = 0;
            accs1 = 0;
            t = (off + flen*8 < in1) ? (off + flen*8) : in1;
            for (i = fidx1 * 8 + off; i < t; ++i) {
                f = (((const short *) firp)[(fn+0)*flen*8 + i - off] * ff0 +
                     ((const short *) firp)[(fn+1)*flen*8 + i - off] * ff1)
                    >> INTERP_BITS;
                accs0 += ((const short *) inp)[i*2+0] * f;
                accs1 += ((const short *) inp)[i*2+1] * f;
            }
            acc = _mm_add_epi32(acc, _mm_set_epi32(0, 0, accs1, accs0));
        } else {
            fidx1 = flen;
        }

#if UNALIGNED_LOAD
        LOOP_UNALIGNED;
#else
        switch (off & 3) {
        case 0: LOOP_ALIGN0; break;
        case 1: LOOP_ALIGN(2); break;
        case 2: LOOP_ALIGN(4); break;
        case 3: LOOP_ALIGN(6); break;
        }
#endif

    accumulate:
        switch (outidx & 3) {
        case 0: case 2:
            acc0 = acc;
            break;

        case 1:
            acc1 = _mm_add_epi32(
                _mm_unpacklo_epi64(acc0, acc),
                _mm_unpackhi_epi64(acc0, acc));
            break;

        case 3:
            acc0 = _mm_add_epi32(
                _mm_unpacklo_epi64(acc0, acc),
                _mm_unpackhi_epi64(acc0, acc));

            /* Apply dither */
            acc1 = _mm_add_epi32(acc1, _mm_srli_epi32(dsv, 17));
            dsv = _mm_add_epi32(
                _mm_unpacklo_epi32(
                    _mm_shuffle_epi32(
                        _mm_mul_epu32(dsv, lcg_a),
                        _MM_SHUFFLE(0, 0, 2, 0)),
                    _mm_shuffle_epi32(
                        _mm_mul_epu32(_mm_srli_si128(dsv, 4), lcg_a),
                        _MM_SHUFFLE(0, 0, 2, 0))),
                lcg_c);
            acc0 = _mm_add_epi32(acc0, _mm_srli_epi32(dsv, 17));
            dsv = _mm_add_epi32(
                _mm_unpacklo_epi32(
                    _mm_shuffle_epi32(
                        _mm_mul_epu32(dsv, lcg_a),
                        _MM_SHUFFLE(0, 0, 2, 0)),
                    _mm_shuffle_epi32(
                        _mm_mul_epu32(_mm_srli_si128(dsv, 4), lcg_a),
                        _MM_SHUFFLE(0, 0, 2, 0))),
                lcg_c);

            acc = _mm_packs_epi32(
                _mm_srai_epi32(acc1, 15),
                _mm_srai_epi32(acc0, 15));
            if (outidx - out0 >= 3)
                *outp = acc;
            else
                lfr_storepartial_epi16(outp, acc, (out0 & 3) * 2, 8);
            outp += 1;
            break;
        }

        x += inv_ratio;
    }

    *pos = x - ((lfr_fixed_t) in0 << 32);

    /* Store remaing bytes */
    acc = _mm_set1_epi32(0);
    if ((outidx & 3) == 0)
        return;
    for (; ; ++outidx) {
        switch (outidx & 3) {
        case 0: case 2:
            acc0 = acc;
            break;

        case 1:
            acc1 = _mm_add_epi32(
                _mm_unpacklo_epi64(acc0, acc),
                _mm_unpackhi_epi64(acc0, acc));
            break;

        case 3:
            acc0 = _mm_add_epi32(
                _mm_unpacklo_epi64(acc0, acc),
                _mm_unpackhi_epi64(acc0, acc));
            acc = _mm_packs_epi32(
                _mm_srai_epi32(acc1, 15),
                _mm_srai_epi32(acc0, 15));
            lfr_storepartial_epi16(
                outp, acc,
                (unsigned) (out0 ^ out1) < 4 ? (out0 & 3) * 2 : 0,
                (out1 & 3) * 2);
            return;
        }
    }
}

#endif
