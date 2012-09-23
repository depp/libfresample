/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "s16.h"
#include <stdint.h>
#include <emmintrin.h>

LFR_PUBLIC void
lfr_s16_resample_stereo(
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

    __m128i acc, acc0, acc1, fir0, fir1, fir_interp, dat0, dat1, dat2, mask;
    int fn, ff0, ff1, off0, off, fidx0, fidx1;
    int accs0, accs1, i, f;

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
        /* acc: FIR accumulator, used for accumulating 32-bit values
           in the format L R L R.  This corresponds to one frame of
           output, so the pair of values for left and the pair for
           right have to be summed later.  */
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
            accs0 = 0;
            accs1 = 0;
            for (i = in0; i < fidx0 * 8 + off; ++i) {
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
            for (i = fidx1 * 8 + off; i < in1; ++i) {
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

        switch (off & 3) {
        case 0:
            for (i = fidx0; i < fidx1; ++i) {
                fir0 = firp[(fn+0)*flen + i];
                fir1 = firp[(fn+1)*flen + i];
                dat0 = inp[(off >> 2) + i*2 + 0];
                dat1 = inp[(off >> 2) + i*2 + 1];

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

                fir1 = _mm_unpackhi_epi16(fir0, fir0);
                fir0 = _mm_unpacklo_epi16(fir0, fir0);

                acc = _mm_add_epi32(
                    acc,
                    _mm_add_epi32(
                        _mm_madd_epi16(
                            _mm_unpacklo_epi16(dat0, dat1),
                            _mm_unpacklo_epi16(fir0, fir1)),
                        _mm_madd_epi16(
                            _mm_unpackhi_epi16(dat0, dat1),
                            _mm_unpackhi_epi16(fir0, fir1))));
            }
            break;

        case 1:
            dat2 = inp[(off >> 2) + fidx0];
            for (i = fidx0; i < fidx1; ++i) {
                fir0 = firp[(fn+0)*flen + i];
                fir1 = firp[(fn+1)*flen + i];
                dat0 = dat2;
                dat1 = inp[(off >> 2) + i*2 + 1];
                dat2 = inp[(off >> 2) + i*2 + 2];
                dat0 = _mm_or_si128(
                    _mm_srli_si128(dat0, 4),
                    _mm_slli_si128(dat1, 12));
                dat1 = _mm_or_si128(
                    _mm_srli_si128(dat1, 4),
                    _mm_slli_si128(dat2, 12));

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

                fir1 = _mm_unpackhi_epi16(fir0, fir0);
                fir0 = _mm_unpacklo_epi16(fir0, fir0);

                acc = _mm_add_epi32(
                    acc,
                    _mm_add_epi32(
                        _mm_madd_epi16(
                            _mm_unpacklo_epi16(dat0, dat1),
                            _mm_unpacklo_epi16(fir0, fir1)),
                        _mm_madd_epi16(
                            _mm_unpackhi_epi16(dat0, dat1),
                            _mm_unpackhi_epi16(fir0, fir1))));
            }
            break;

        case 2:
            dat2 = inp[(off >> 2) + fidx0];
            for (i = fidx0; i < fidx1; ++i) {
                fir0 = firp[(fn+0)*flen + i];
                fir1 = firp[(fn+1)*flen + i];
                dat0 = dat2;
                dat1 = inp[(off >> 2) + i*2 + 1];
                dat2 = inp[(off >> 2) + i*2 + 2];
                dat0 = _mm_or_si128(
                    _mm_srli_si128(dat0, 8),
                    _mm_slli_si128(dat1, 8));
                dat1 = _mm_or_si128(
                    _mm_srli_si128(dat1, 8),
                    _mm_slli_si128(dat2, 8));

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

                fir1 = _mm_unpackhi_epi16(fir0, fir0);
                fir0 = _mm_unpacklo_epi16(fir0, fir0);

                acc = _mm_add_epi32(
                    acc,
                    _mm_add_epi32(
                        _mm_madd_epi16(
                            _mm_unpacklo_epi16(dat0, dat1),
                            _mm_unpacklo_epi16(fir0, fir1)),
                        _mm_madd_epi16(
                            _mm_unpackhi_epi16(dat0, dat1),
                            _mm_unpackhi_epi16(fir0, fir1))));
            }
            break;

        case 3:
            dat2 = inp[(off >> 2) + fidx0];
            for (i = fidx0; i < fidx1; ++i) {
                fir0 = firp[(fn+0)*flen + i];
                fir1 = firp[(fn+1)*flen + i];
                dat0 = dat2;
                dat1 = inp[(off >> 2) + i*2 + 1];
                dat2 = inp[(off >> 2) + i*2 + 2];
                dat0 = _mm_or_si128(
                    _mm_srli_si128(dat0, 12),
                    _mm_slli_si128(dat1, 4));
                dat1 = _mm_or_si128(
                    _mm_srli_si128(dat1, 12),
                    _mm_slli_si128(dat2, 4));

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

                fir1 = _mm_unpackhi_epi16(fir0, fir0);
                fir0 = _mm_unpacklo_epi16(fir0, fir0);

                acc = _mm_add_epi32(
                    acc,
                    _mm_add_epi32(
                        _mm_madd_epi16(
                            _mm_unpacklo_epi16(dat0, dat1),
                            _mm_unpacklo_epi16(fir0, fir1)),
                        _mm_madd_epi16(
                            _mm_unpackhi_epi16(dat0, dat1),
                            _mm_unpackhi_epi16(fir0, fir1))));
            }
            break;
        }

    accumulate:
        if ((outidx & 1) == 0) {
            acc0 = acc;
        } else if ((outidx & 2) == 0) {
            acc1 = _mm_add_epi32(
                _mm_unpacklo_epi64(acc0, acc),
                _mm_unpackhi_epi64(acc0, acc));
        } else {
            acc0 = _mm_add_epi32(
                _mm_unpacklo_epi64(acc0, acc),
                _mm_unpackhi_epi64(acc0, acc));
            acc = _mm_packs_epi32(
                _mm_srai_epi32(acc1, 15),
                _mm_srai_epi32(acc0, 15));
            if (outidx - out0 >= 3) {
                *outp = acc;
            } else {
                mask = _mm_set1_epi32(-1);
                switch (out0 & 3) {
                case 0:
                    break;

                case 1:
                    mask = _mm_slli_si128(mask, 4);
                    _mm_maskmoveu_si128(acc, mask, (char *) outp);
                    break;

                case 2:
                    mask = _mm_slli_si128(mask, 8);
                    _mm_maskmoveu_si128(acc, mask, (char *) outp);
                    break;

                case 3:
                    mask = _mm_slli_si128(mask, 12);
                    _mm_maskmoveu_si128(acc, mask, (char *) outp);
                    break;
                }
            }
            outp += 1;
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
    mask = _mm_set1_epi32(-1);
    switch (out1 & 3) {
    case 0:
        break;

    case 1:
        acc0 = _mm_add_epi32(
            _mm_unpacklo_epi64(acc0, acc),
            _mm_unpackhi_epi64(acc0, acc));
        acc0 = _mm_packs_epi32(
            _mm_srai_epi32(acc0, 15),
            acc);
        mask = _mm_srli_si128(mask, 24);
        _mm_maskmoveu_si128(acc, mask, (char *) outp);
        break;

    case 2:
        acc0 = _mm_packs_epi32(
            _mm_srai_epi32(acc0, 15),
            acc);
        mask = _mm_srli_si128(mask, 8);
        _mm_maskmoveu_si128(acc, mask, (char *) outp);
        break;

    case 3:
        acc0 = _mm_add_epi32(
            _mm_unpacklo_epi64(acc0, acc),
            _mm_unpackhi_epi64(acc0, acc));
        *outp = _mm_packs_epi32(
            _mm_srai_epi32(acc1, 15),
            _mm_srai_epi32(acc0, 15));

        mask = _mm_srli_si128(mask, 4);
        _mm_maskmoveu_si128(acc, mask, (char *) outp);
        break;
    }
}
