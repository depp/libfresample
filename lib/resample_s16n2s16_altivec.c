/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#define LFR_ALTIVEC 1

#include "cpu.h"
#if defined(LFR_CPU_PPC)
#include "resample.h"
#include <stdint.h>

#define UNALIGNED_LOAD 1

#define LOOP_LOADFIR \
    fir0 = firp[(fn+0)*flen + i];       \
    fir1 = firp[(fn+1)*flen + i];       \
                                        \
    fir0 = vec_pack(                    \
        vec_sra(                        \
            vec_msum(                   \
                vec_mergeh(fir0, fir1), \
                fir_interp,             \
                zero),                  \
            fir_shift),                 \
        vec_sra(                        \
            vec_msum(                   \
                vec_mergel(fir0, fir1), \
                fir_interp,             \
                zero),                  \
            fir_shift));                \
                                        \
    fir1 = vec_mergel(fir0, fir0);      \
    fir0 = vec_mergeh(fir0, fir0)

#define LOOP_ACCUM \
    acc_a = vec_msum(           \
        vec_mergeh(dat0, dat1), \
        vec_mergeh(fir0, fir1), \
        acc_a);                 \
    acc_b = vec_msum(           \
        vec_mergel(dat0, dat1), \
        vec_mergel(fir0, fir1), \
        acc_b)

static __inline void
lfr_storepartial0_vec16(vector signed short x, int b,
                        vector signed short *dest)
{
    union {
        unsigned short h[8];
        vector signed short x;
    } u;
    int i;
    u.x = x;
    for (i = (b & 7); i < 8; ++i)
        ((unsigned short *) dest)[i] = u.h[i];
}

static __inline void
lfr_storepartial1_vec16(vector signed short x, int b,
                        vector signed short *dest)
{
    union {
        unsigned short h[8];
        vector signed short x;
    } u;
    int i;
    u.x = x;
    for (i = 0; i < (b & 7); ++i)
        ((unsigned short *) dest)[i] = u.h[i];
}

void
lfr_resample_s16n2s16_altivec(
    lfr_fixed_t *pos, lfr_fixed_t inv_ratio, unsigned *dither,
    void *out, int outlen, const void *in, int inlen,
    const struct lfr_filter *filter)
{
    const vector signed short *firp, *inp;
    vector signed short *outp;
    int in0, in1, out0, out1, outidx;
    int flen, log2nfilt;
    lfr_fixed_t x;

    vector unsigned char perm_hi64 =
        { 0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23 };
    vector unsigned char perm_lo64, load_perm;
    vector signed short fir0, fir1, fir_interp, dat0, dat1, dat2, acc_r;
    vector unsigned int acc_shift, fir_shift;
    vector signed int acc_a, acc_b, acc, acc0, acc1, zero;
    vector unsigned int dsv;
    vector unsigned int lcg_a = { LCG_A4, LCG_A4, LCG_A4, LCG_A4 };
    vector unsigned int lcg_c = { LCG_C4, LCG_C4, LCG_C4, LCG_C4 };
    int fn, ff0, ff1, off0, off, fidx0, fidx1;
    int accs0, accs1, i, f, t;
    unsigned ds;

    union {
        unsigned short h[8];
        int w[4];
        vector signed int x;
    } un;

    zero = vec_splat_s32(0);
    perm_lo64 = vec_add(perm_hi64, vec_splat_u8(8));
    acc_shift = vec_splat_u32(15);
    fir_shift = vec_splat_u32(INTERP_BITS);

    /* firp: Pointer to beginning of filter coefficients, aligned.  */
    firp = (const vector signed short *) filter->data;
    /* flen: Length of filter, measured in 128-bit words.  */
    flen = filter->nsamp >> 3;
    off0 = (filter->nsamp >> 1);
    /* log2nfilt: Base 2 logarithm of the number of filters.  */
    log2nfilt = filter->log2nfilt;

    /* in0, in1: Frame index of input start and end, measured from
       aligned input pointer.  inp: aligned input pointer.  */
    in0 = ((uintptr_t) in >> 2) & 3;
    in1 = inlen + in0;
    inp = (const vector signed short *) ((const char *) in - in0 * 4);

    /* out0, out1: Frame index of output start and end, measured from
       aligned output pointer.  outp: aligned output pointer.  */
    out0 = ((uintptr_t) out >> 2) & 3;
    out1 = outlen + out0;
    outp = (vector signed short *) ((char *) out - out0 * 4);

    x = *pos + ((lfr_fixed_t) in0 << 32);
    ds = *dither;
    for (i = 0; i < (out0 & 3) * 2; ++i)
        ds = LCG_AI * ds + LCG_CI;
    for (i = 0; i < 4; ++i) {
        un.w[i] = ds;
        ds = LCG_A * ds + LCG_C;
    }
    dsv = un.x;

    un.w[2] = 0;
    un.w[3] = 0;

    acc0 = vec_splat_s32(0);
    acc1 = vec_splat_s32(0);
    for (outidx = out0; outidx < out1; ++outidx) {
        /* acc: FIR accumulator, used for accumulating 32-bit values
           in the format L R L R.  This corresponds to one frame of
           output, so the pair of values for left and the pair for
           right have to be summed later.  */
        acc_a = vec_splat_s32(0);
        acc_b = vec_splat_s32(0);

        /* fn: filter number
           ff0: filter factor for filter fn
           ff1: filter factor for filter fn+1 */
        fn = (((unsigned) x >> 1) >> (31 - log2nfilt)) &
            ((1u << log2nfilt) - 1);
        ff1 = ((unsigned) x >> (32 - log2nfilt - INTERP_BITS)) &
            ((1u << INTERP_BITS) - 1);
        ff0 = (1u << INTERP_BITS) - ff1;
        un.h[0] = ff0;
        un.h[1] = ff1;
        fir_interp = (vector signed short)
            vec_splat((vector signed int) un.x, 0);

        /* off: offset in input corresponding to first sample in
           filter */
        off = (int) (x >> 32) - off0;

        /* fixd0, fidx1: start, end indexes of 8-word (16-byte) chunks
           of whole FIR data we will use */
        fidx0 = (in0 - off + 7) >> 3;
        fidx1 = (in1 - off) >> 3;
        if (fidx0 > 0) {
            if (fidx0 > flen) {
                acc = vec_splat_s32(0);
                goto accumulate;
            }
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
            un.w[0] = accs0;
            un.w[1] = accs1;
            acc_a = un.x;
        } else {
            fidx0 = 0;
        }
        if (fidx1 < flen) {
            if (fidx1 < 0) {
                acc = vec_splat_s32(0);
                goto accumulate;
            }
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
            un.w[0] = accs0;
            un.w[1] = accs1;
            acc_b = un.x;
        } else {
            fidx1 = flen;
        }

        if (off & 3) {
            load_perm = vec_lvsl(off * 4, (unsigned char *) 0);
            dat2 = inp[(off >> 2) + fidx0*2];
            for (i = fidx0; i < fidx1; ++i) {
                dat0 = dat2;
                dat1 = inp[(off >> 2) + i*2 + 1];
                dat2 = inp[(off >> 2) + i*2 + 2];
                dat0 = vec_perm(dat0, dat1, load_perm);
                dat1 = vec_perm(dat1, dat2, load_perm);
                LOOP_LOADFIR;
                LOOP_ACCUM;
            }
        } else {
            for (i = fidx0; i < fidx1; ++i) {
                dat0 = inp[(off >> 2) + i*2 + 0];
                dat1 = inp[(off >> 2) + i*2 + 1];
                LOOP_LOADFIR;
                LOOP_ACCUM;
            }
        }
        acc = vec_add(acc_a, acc_b);

    accumulate:
        switch (outidx & 3) {
        case 0: case 2:
            acc0 = acc;
            break;

        case 1:
            acc1 = vec_add(
                vec_perm(acc0, acc, perm_hi64),
                vec_perm(acc0, acc, perm_lo64));
            break;

        case 3:
            acc0 = vec_add(
                vec_perm(acc0, acc, perm_hi64),
                vec_perm(acc0, acc, perm_lo64));

            acc1 = vec_add(
                acc1,
                (vector signed int) vec_sr(dsv, vec_splat_u32(17-32)));
            dsv = lfr_vecrand(dsv, lcg_a, lcg_c);
            acc0 = vec_add(
                acc0,
                (vector signed int) vec_sr(dsv, vec_splat_u32(17-32)));
            dsv = lfr_vecrand(dsv, lcg_a, lcg_c);

            acc_r = vec_packs(
                vec_sra(acc1, acc_shift),
                vec_sra(acc0, acc_shift));
            if (outidx - out0 >= 3)
                *outp = acc_r;
            else
                lfr_storepartial0_vec16(acc_r, out0 * 2, outp);
            outp += 1;
            break;
        }

        x += inv_ratio;
    }

    un.x = dsv;
    ds = un.w[0];
    for (i = 0; i < (out1 & 7); ++i)
        ds = LCG_A * ds + LCG_C;
    *pos = x - ((lfr_fixed_t) in0 << 32);
    *dither = ds;

    /* Store remaing bytes */
    if ((outidx & 3) == 0)
        return;
    acc = vec_splat_s32(0);
    for (; ; ++outidx) {
        switch (outidx & 3) {
        case 0: case 2:
            acc0 = acc;
            break;

        case 1:
            acc1 = vec_add(
                vec_perm(acc0, acc, perm_hi64),
                vec_perm(acc0, acc, perm_lo64));
            break;

        case 3:
            acc0 = vec_add(
                vec_perm(acc0, acc, perm_hi64),
                vec_perm(acc0, acc, perm_lo64));

            acc1 = vec_add(
                acc1,
                (vector signed int) vec_sr(dsv, vec_splat_u32(17-32)));
            dsv = lfr_vecrand(dsv, lcg_a, lcg_c);
            acc0 = vec_add(
                acc0,
                (vector signed int) vec_sr(dsv, vec_splat_u32(17-32)));
            dsv = lfr_vecrand(dsv, lcg_a, lcg_c);

            acc_r = vec_packs(
                vec_sra(acc1, acc_shift),
                vec_sra(acc0, acc_shift));
            lfr_storepartial1_vec16(acc_r, out1 * 2, outp);
            return;
       }
    }
}

#endif
