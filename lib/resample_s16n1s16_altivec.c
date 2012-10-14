/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#define LFR_ALTIVEC 1

#include "cpu.h"
#if defined(LFR_CPU_PPC)
#include "resample.h"
#include <stdint.h>
#include <string.h>

#define LOOP_LOADFIR \
    fir0 = fd[(fn+0)*flen + j];         \
    fir1 = fd[(fn+1)*flen + j];         \
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
            fir_shift))

#define LOOP_ACCUM \
    acc = vec_msum(dat0, fir0, acc)

void
lfr_resample_s16n1s16_altivec(
    lfr_fixed_t *pos, lfr_fixed_t inv_ratio, unsigned *dither,
    void *out, int outlen, const void *in, int inlen,
    const struct lfr_filter *filter)
{
    const vector signed short *fd;
    int flen, log2nfilt;
    lfr_fixed_t x;

    vector unsigned char perm_hi64 =
        { 0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23 };
    vector unsigned char perm_lo64, load_perm, store_perm;
    vector signed short fir0, fir1, fir_interp, dat0, dat1;
    vector signed short out0, out1;
    vector unsigned int acc_shift, fir_shift;
    vector signed int acc, acc0, acc1, acc2, zero;
    vector unsigned int dsv, lcg_a, lcg_c;;
    int fn, ff0, ff1, off, fidx0, fidx1;
    int accs, i, j, f, a0, a1;
    unsigned ds;

    union {
        unsigned short h[16];
        int w[8];
        vector signed short vh[2];
        vector signed int vs[2];
        vector unsigned int vw[2];
    } u;

    zero = vec_splat_s32(0);
    perm_lo64 = vec_add(perm_hi64, vec_splat_u8(8));
    acc_shift = vec_splat_u32(15);
    fir_shift = vec_splat_u32(INTERP_BITS);

    fd = (const vector signed short *) filter->data;
    flen = filter->nsamp >> 3;
    log2nfilt = filter->log2nfilt;

    x = *pos;
    ds = *dither;
    for (i = 0; i < 4; ++i) {
        u.w[i] = ds;
        ds = ds * LCG_A + LCG_C;
    }
    dsv = u.vw[0];
    u.w[0] = LCG_A4;
    u.w[1] = LCG_C4;
    lcg_a = u.vw[0];
    lcg_c = vec_splat(lcg_a, 1);
    lcg_a = vec_splat(lcg_a, 0);

    u.w[1] = 0;
    u.w[2] = 0;
    u.w[3] = 0;

    acc0 = acc1 = acc2 = vec_splat_s32(0);
    out0 = out1 = vec_splat_s16(0);
    store_perm = vec_lvsr(0, (short *) out);
    for (i = 0; i < outlen; ++i) {
        /* fn: filter number
           ff0: filter factor for filter fn
           ff1: filter factor for filter fn+1 */
        fn = (((unsigned) x >> 1) >> (31 - log2nfilt)) &
            ((1u << log2nfilt) - 1);
        ff1 = ((unsigned) x >> (32 - log2nfilt - INTERP_BITS)) &
            ((1u << INTERP_BITS) - 1);
        ff0 = (1u << INTERP_BITS) - ff1;
        u.h[0] = ff0;
        u.h[1] = ff1;
        fir_interp = (vector signed short)
            vec_splat((vector signed int) u.vs[0], 0);

        /* acc: FIR accumulator, used for accumulating 32-bit values
           in the format L R L R.  This corresponds to one frame of
           output, so the pair of values for left and the pair for
           right have to be summed later.  */
        acc = vec_splat_s32(0);
        /* off: offset in input corresponding to first sample in
           filter */
        off = (int) (x >> 32);
        /* fixd0, fidx1: start, end indexes of 8-word (16-byte) chunks
           of whole FIR data we will use */
        fidx0 = (-off + 7) >> 3;
        fidx1 = (inlen - off) >> 3;
        if (fidx0 > 0) {
            if (fidx0 > flen)
                goto accumulate;
            accs = 0;
            for (j = -off; j < fidx0 * 8; ++j) {
                f = (((const short *) fd)[(fn+0) * (flen*8) + j] * ff0 +
                     ((const short *) fd)[(fn+1) * (flen*8) + j] * ff1)
                    >> INTERP_BITS;
                accs += ((const short *) in)[j + off] * f;
            }
            u.w[0] = accs;
            acc = u.vs[0];
        } else {
            fidx0 = 0;
        }
        if (fidx1 < flen) {
            if (fidx1 < 0)
                goto accumulate;
            accs = 0;
            for (j = fidx1 * 8; j < inlen - off; ++j) {
                f = (((const short *) fd)[(fn+0) * (flen*8) + j] * ff0 +
                     ((const short *) fd)[(fn+1) * (flen*8) + j] * ff1)
                    >> INTERP_BITS;
                accs += ((const short *) in)[j + off] * f;
            }
            u.w[0] = accs;
            acc = vec_add(acc, u.vs[0]);
        } else {
            fidx1 = flen;
        }

        if (off & 7) {
            load_perm = vec_lvsl(off * 2, (unsigned char *) 0);
            dat1 = vec_ld(fidx0 * 16 + off * 2, (short *) in);
            for (j = fidx0; j < fidx1; ++j) {
                dat0 = dat1;
                dat1 = vec_ld((j + 1) * 16 + off * 2, (short *) in);
                dat0 = vec_perm(dat0, dat1, load_perm);
                LOOP_LOADFIR;
                LOOP_ACCUM;
            }
        } else {
            for (j = fidx0; j < fidx1; ++j) {
                dat0 = vec_ld(j * 16 + off * 2, (short *) in);
                LOOP_LOADFIR;
                LOOP_ACCUM;
            }
        }

    accumulate:
        switch (i & 7) {
        case 0: case 2: case 4: case 6:
            acc0 = acc;
            break;

        case 1: case 5:
            acc1 = vec_add(
                vec_mergeh(acc0, acc),
                vec_mergel(acc0, acc));
            break;

        case 3:
            acc0 = vec_add(
                vec_mergeh(acc0, acc),
                vec_mergel(acc0, acc));
            acc2 = vec_add(
                vec_perm(acc1, acc0, perm_hi64),
                vec_perm(acc1, acc0, perm_lo64));
            break;

        case 7:
            acc0 = vec_add(
                vec_mergeh(acc0, acc),
                vec_mergel(acc0, acc));
            acc1 = vec_add(
                vec_perm(acc1, acc0, perm_hi64),
                vec_perm(acc1, acc0, perm_lo64));

            acc2 = vec_add(
                acc2,
                (vector signed int) vec_sr(dsv, vec_splat_u32(17-32)));
            dsv = lfr_vecrand(dsv, lcg_a, lcg_c);
            acc1 = vec_add(
                acc1,
                (vector signed int) vec_sr(dsv, vec_splat_u32(17-32)));
            dsv = lfr_vecrand(dsv, lcg_a, lcg_c);

            out1 = vec_packs(
                vec_sra(acc2, acc_shift),
                vec_sra(acc1, acc_shift));
            out0 = vec_perm(out0, out1, store_perm);
            if (i >= 8) {
                vec_st(out0, i * 2, (short *) out);
            } else {
                a0 = (int) ((uintptr_t) out & 15);
                memcpy(out, (unsigned char *) &out0 + a0, 16 - a0);
            }
            out0 = out1;
            break;
        }

        x += inv_ratio;
    }

    u.vw[0] = dsv;
    ds = u.w[0];
    for (i = 0; i < (outlen & 7); ++i)
        ds = LCG_A * ds + LCG_C;
    *pos = x;
    *dither = ds;

    /* Store remaing bytes */
    if ((outlen & 7) == 0) {
        out0 = vec_perm(out0, out0, store_perm);
        i = outlen + 8;
        goto final;
    }
    acc = vec_splat_s32(0);
    for (i = outlen; ; ++i) {
        switch (i & 7) {
        case 0: case 2: case 4: case 6:
            acc0 = acc;
            break;

        case 1: case 5:
            acc1 = vec_add(
                vec_mergeh(acc0, acc),
                vec_mergel(acc0, acc));
            break;

        case 3:
            acc0 = vec_add(
                vec_mergeh(acc0, acc),
                vec_mergel(acc0, acc));
            acc2 = vec_add(
                vec_perm(acc1, acc0, perm_hi64),
                vec_perm(acc1, acc0, perm_lo64));
            break;

        case 7:
            acc0 = vec_add(
                vec_mergeh(acc0, acc),
                vec_mergel(acc0, acc));
            acc1 = vec_add(
                vec_perm(acc1, acc0, perm_hi64),
                vec_perm(acc1, acc0, perm_lo64));

            acc2 = vec_add(
                acc2,
                (vector signed int) vec_sr(dsv, vec_splat_u32(17-32)));
            dsv = lfr_vecrand(dsv, lcg_a, lcg_c);
            acc1 = vec_add(
                acc1,
                (vector signed int) vec_sr(dsv, vec_splat_u32(17-32)));

            out1 = vec_packs(
                vec_sra(acc2, acc_shift),
                vec_sra(acc1, acc_shift));
            out0 = vec_perm(out0, out1, store_perm);
            out1 = vec_perm(out1, out1, store_perm);
            goto final;
        }
    }

final:
    /* byte offset of data point i in (out0, out1) */
    off = (int) ((uintptr_t) out & 15) + 16;
    if (off > 2*i)
        a0 = off - 2*i;
    else
        a0 = 0;
    if (off + 2*(outlen - i) > 32)
        a1 = 32;
    else
        a1 = off + 2*(outlen - i);

    u.vh[0] = out0;
    u.vh[1] = out1;
    memcpy((char *) out + 2*i - off + a0, (char *) &u + a0, a1 - a0);
}

#endif
