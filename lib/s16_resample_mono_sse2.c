/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#define LFR_IMPLEMENTATION 1

#include "s16.h"
#include <stdint.h>
#include <emmintrin.h>

#define UNALIGNED_LOAD 1

#define LOOP_LOADFIR \
    fir0 = firp[(fn+0)*flen + i];               \
    fir1 = firp[(fn+1)*flen + i];               \
    fir0 = _mm_packs_epi32(                     \
        _mm_srai_epi32(                         \
            _mm_madd_epi16(                     \
                _mm_unpacklo_epi16(fir0, fir1), \
                fir_interp),                    \
            INTERP_BITS),                       \
        _mm_srai_epi32(                         \
            _mm_madd_epi16(                     \
                _mm_unpackhi_epi16(fir0, fir1), \
                fir_interp),                    \
            INTERP_BITS))

#define LOOP_ACCUM \
    acc = _mm_add_epi32(acc, _mm_madd_epi16(dat0, fir0))

#define LOOP_ALIGN0 \
    for (i = fidx0; i < fidx1; ++i) {   \
        dat0 = inp[(off >> 3) + i + 0]; \
        LOOP_LOADFIR;                   \
        LOOP_ACCUM;                     \
    }

#define LOOP_ALIGN(n) \
    dat1 = inp[(off >> 3) + fidx0];             \
    for (i = fidx0; i < fidx1; ++i) {           \
        dat0 = dat1;                            \
        dat1 = inp[(off >> 3) + i + 1];         \
        dat0 = _mm_or_si128(                    \
            _mm_srli_si128(dat0, (n)*2),        \
            _mm_slli_si128(dat1, 16-(n)*2));    \
        LOOP_LOADFIR;                           \
        LOOP_ACCUM;                             \
    }

#define LOOP_UNALIGNED \
    for (i = fidx0; i < fidx1; ++i) {           \
        dat0 = _mm_loadu_si128(                 \
            (const __m128i *)                   \
            ((const short *) inp + off + i*8)); \
        LOOP_LOADFIR;                           \
        LOOP_ACCUM;                             \
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
lfr_s16_resample_mono_sse2(
    short *LFR_RESTRICT out, size_t outlen, int outrate,
    const short *LFR_RESTRICT in, size_t inlen, int inrate,
    const struct lfr_s16 *LFR_RESTRICT filter)
{
    const __m128i *firp, *inp;
    __m128i *outp;
    int in0, in1, out0, out1, outidx;
    int flen, log2nfilt;
    int pf, si, sf;
    uint64_t pi, tmp64;

    __m128i acc, acc0, acc1, acc2, fir0, fir1, fir_interp, dat0;
#if !UNALIGNED_LOAD
    __m128i dat1;
#endif
    int fn, ff0, ff1, off0, off, fidx0, fidx1;
    int accs, i, f, t;

    /* firp: Pointer to beginning of filter coefficients, aligned.  */
    firp = (const __m128i *) filter->data;
    /* flen: Length of filter, measured in 128-bit words.  */
    flen = filter->nsamp >> 3;
    off0 = (filter->nsamp >> 1);
    /* log2nfilt: Base 2 logarithm of the number of filters.  */
    log2nfilt = filter->log2nfilt;

    /* in0, in1: Sample index of input start and end, measured from
       aligned input pointer.  inp: aligned input pointer.  */
    in0 = ((uintptr_t) in >> 1) & 7;
    in1 = inlen + in0;
    inp = (const __m128i *) (in - in0);

    /* out0, out1: Sample index of output start and end, measured from
       aligned output pointer.  outp: aligned output pointer.  */
    out0 = ((uintptr_t) out >> 1) & 7;
    out1 = outlen + out0;
    outp = (__m128i *) (out - out0);

    /* pi, pf: integral and fractional position, measured from
       the aligned input pointer.  si, sf: integral and fractional step per
       frame.  */
    pi = (uint64_t) in0 << (INTERP_BITS + log2nfilt);
    pf = 0;
    tmp64 = (uint64_t) inrate << (INTERP_BITS + log2nfilt);
    si = (int) (tmp64 / outrate);
    sf = (int) (tmp64 % outrate);

    acc0 = _mm_set1_epi32(0);
    acc1 = _mm_set1_epi32(0);
    for (outidx = out0; outidx < out1; ++outidx) {
        /* acc: FIR accumulator, as four 32-bit words.  The four words
           must be added, shifted, and packed before they can be
           stored.  */
        acc = _mm_set1_epi32(0);

        /* fn: filter number
           ff0: filter factor for filter fn
           ff1: filter factor for filter fn+1 */
        fn = ((unsigned) pi >> INTERP_BITS) & ((1u << log2nfilt) - 1);
        ff1 = (unsigned) pi & ((1u << INTERP_BITS) - 1);
        ff0 = (1u << INTERP_BITS) - ff1;
        fir_interp = _mm_set1_epi32(ff0 | (ff1 << 16));

        /* off: offset in input corresponding to first sample in
           filter */
        off = (int) (pi >> (INTERP_BITS + log2nfilt)) - off0;

        /* fixd0, fidx1: start, end indexes of 8-word (16-byte) chunks
           of whole FIR data we will use */
        fidx0 = (in0 - off + 7) >> 3;
        fidx1 = (in1 - off) >> 3;
        if (fidx0 > 0) {
            if (fidx0 > flen)
                goto accumulate;
            accs = 0;
            t = (off > in0) ? off : in0;
            for (i = t; i < fidx0 * 8 + off; ++i) {
                f = (((const short *) firp)[(fn+0)*flen*8 + i - off] * ff0 +
                     ((const short *) firp)[(fn+1)*flen*8 + i - off] * ff1)
                    >> INTERP_BITS;
                accs += ((const short *) inp)[i] * f;
            }
            acc = _mm_cvtsi32_si128(accs);
        } else {
            fidx0 = 0;
        }
        if (fidx1 < flen) {
            if (fidx1 < 0)
                goto accumulate;
            accs = 0;
            t = (off + flen*8 < in1) ? (off + flen*8) : in1;
            for (i = fidx1 * 8 + off; i < t; ++i) {
                f = (((const short *) firp)[(fn+0)*flen*8 + i - off] * ff0 +
                     ((const short *) firp)[(fn+1)*flen*8 + i - off] * ff1)
                    >> INTERP_BITS;
                accs += ((const short *) inp)[i] * f;
            }
            acc = _mm_add_epi32(acc, _mm_cvtsi32_si128(accs));
        } else {
            fidx1 = flen;
        }

#if UNALIGNED_LOAD
        LOOP_UNALIGNED;
#else
        switch (off & 7) {
        case 0: LOOP_ALIGN0; break;
        case 1: LOOP_ALIGN(1); break;
        case 2: LOOP_ALIGN(2); break;
        case 3: LOOP_ALIGN(3); break;
        case 4: LOOP_ALIGN(4); break;
        case 5: LOOP_ALIGN(5); break;
        case 6: LOOP_ALIGN(6); break;
        case 7: LOOP_ALIGN(7); break;
        }
#endif

    accumulate:
        switch (outidx & 7) {
        case 0: case 2: case 4: case 6:
            acc0 = acc;
            break;

        case 1: case 5:
            acc1 = _mm_add_epi32(
                _mm_unpacklo_epi32(acc0, acc),
                _mm_unpackhi_epi32(acc0, acc));
            break;

        case 3:
            acc0 = _mm_add_epi32(
                _mm_unpacklo_epi32(acc0, acc),
                _mm_unpackhi_epi32(acc0, acc));
            acc2 = _mm_add_epi32(
                _mm_unpacklo_epi64(acc1, acc0),
                _mm_unpackhi_epi64(acc1, acc0));
            break;

        case 7:
            acc0 = _mm_add_epi32(
                _mm_unpacklo_epi32(acc0, acc),
                _mm_unpackhi_epi32(acc0, acc));
            acc1 = _mm_add_epi32(
                _mm_unpacklo_epi64(acc1, acc0),
                _mm_unpackhi_epi64(acc1, acc0));
            acc = _mm_packs_epi32(
                _mm_srai_epi32(acc2, 15),
                _mm_srai_epi32(acc1, 15));
            if (outidx - out0 >= 7)
                *outp = acc;
            else
                lfr_storepartial0_epi16(acc, out0, outp);
            outp += 1;
            break;
        }

        pf += sf;
        pi += si;
        if (pf >= outrate) {
            pf -= outrate;
            pi += 1;
        }
    }

    /* Store remaing bytes */
    acc = _mm_set1_epi32(0);
    if ((outidx & 7) == 0)
        return;
    for (; ; ++outidx) {
        switch (outidx & 7) {
        case 0: case 2: case 4: case 6:
            acc0 = acc;
            break;

        case 1: case 5:
            acc1 = _mm_add_epi32(
                _mm_unpacklo_epi32(acc0, acc),
                _mm_unpackhi_epi32(acc0, acc));
            break;

        case 3:
            acc0 = _mm_add_epi32(
                _mm_unpacklo_epi32(acc0, acc),
                _mm_unpackhi_epi32(acc0, acc));
            acc2 = _mm_add_epi32(
                _mm_unpacklo_epi64(acc1, acc0),
                _mm_unpackhi_epi64(acc1, acc0));
            break;

        case 7:
            acc0 = _mm_add_epi32(
                _mm_unpacklo_epi32(acc0, acc),
                _mm_unpackhi_epi32(acc0, acc));
            acc1 = _mm_add_epi32(
                _mm_unpacklo_epi64(acc1, acc0),
                _mm_unpackhi_epi64(acc1, acc0));
            acc = _mm_packs_epi32(
                _mm_srai_epi32(acc2, 15),
                _mm_srai_epi32(acc1, 15));
            lfr_storepartial1_epi16(acc, out1, outp);
            return;
        }
    }
}
