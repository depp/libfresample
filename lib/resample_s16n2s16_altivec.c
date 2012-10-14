/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#define LFR_ALTIVEC 1

#include "cpu.h"
#if defined(LFR_CPU_PPC)
#include "resample.h"
#include <stdint.h>
#include <string.h>

#define LOOP_FIRCOEFF \
    fn = (((unsigned) x >> 1) >> (31 - log2nfilt)) &            \
        ((1u << log2nfilt) - 1);                                \
    ff1 = ((unsigned) x >> (32 - log2nfilt - INTERP_BITS)) &    \
        ((1u << INTERP_BITS) - 1);                              \
    ff0 = (1u << INTERP_BITS) - ff1;                            \
    u.h[0] = ff0;                                               \
    u.h[1] = ff1;                                               \
    fir_interp = (vector signed short)                          \
        vec_splat((vector signed int) u.vs[0], 0)

#define LOOP_ACCUM(flen, j) \
    fir0 = fd[(fn+0)*(flen) + (j)];     \
    fir1 = fd[(fn+1)*(flen) + (j)];     \
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
    fir0 = vec_mergeh(fir0, fir0);      \
                                        \
    acc_a = vec_msum(                   \
        vec_mergeh(dat0, dat1),         \
        vec_mergeh(fir0, fir1),         \
        acc_a);                         \
    acc_b = vec_msum(                   \
        vec_mergel(dat0, dat1),         \
        vec_mergel(fir0, fir1),         \
        acc_b)

#define LOOP_STORE \
    switch (i & 3) {                                    \
    case 0: case 2:                                     \
        acc0 = acc;                                     \
        break;                                          \
                                                        \
    case 1:                                             \
        acc1 = vec_add(                                 \
            vec_perm(acc0, acc, perm_hi64),             \
            vec_perm(acc0, acc, perm_lo64));            \
        break;                                          \
                                                        \
    case 3:                                             \
        acc0 = vec_add(                                 \
            vec_perm(acc0, acc, perm_hi64),             \
            vec_perm(acc0, acc, perm_lo64));            \
                                                        \
        acc1 = vec_add(                                 \
            acc1,                                       \
            (vector signed int)                         \
            vec_sr(dsv, vec_splat_u32(17-32)));         \
        dsv = lfr_vecrand(dsv, lcg_a, lcg_c);           \
        acc0 = vec_add(                                 \
            acc0,                                       \
            (vector signed int)                         \
            vec_sr(dsv, vec_splat_u32(17-32)));         \
        dsv = lfr_vecrand(dsv, lcg_a, lcg_c);           \
                                                        \
        out1 = vec_packs(                               \
            vec_sra(acc1, acc_shift),                   \
            vec_sra(acc0, acc_shift));                  \
        out0 = vec_perm(out0, out1, store_perm);        \
        if (i >= 4) {                                   \
            vec_st(out0, (i - 3) * 4, (short *) out);   \
        } else {                                        \
            a0 = (int) ((uintptr_t) out & 15);          \
            memcpy(out, (char *) &out0 + a0, 16 - a0);  \
        }                                               \
        out0 = out1;                                    \
        break;                                          \
    }

void
lfr_resample_s16n2s16_altivec(
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
    vector signed short fir0, fir1, fir_interp;
    vector signed short dat0, dat1, dat2, dat3, dat4, dat5, dat6;
    vector signed short out0, out1;
    vector unsigned int acc_shift, fir_shift;
    vector signed int acc_a, acc_b, acc, acc0, acc1, zero;
    vector unsigned int dsv, lcg_a, lcg_c;
    int fn, ff0, ff1, off, fidx0, fidx1;
    int accs0, accs1, i, j, f, a0, a1;
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
        ds = LCG_A * ds + LCG_C;
    }
    dsv = u.vw[0];
    u.w[0] = LCG_A4;
    u.w[1] = LCG_C4;
    lcg_a = u.vw[0];
    lcg_c = vec_splat(lcg_a, 1);
    lcg_a = vec_splat(lcg_a, 0);

    u.w[2] = 0;
    u.w[3] = 0;

    acc0 = acc1 = vec_splat_s32(0);
    out0 = out1 = vec_splat_s16(0);
    store_perm = vec_lvsr(0, (short *) out);

    switch (flen) {
    case 2: goto flen2;
    case 3: goto flen3;
    default: goto flenv;
    }

flenv:
    for (i = 0; i < outlen; ++i) {
        LOOP_FIRCOEFF;

        /* acc: FIR accumulator, used for accumulating 32-bit values
           in the format L R L R.  This corresponds to one frame of
           output, so the pair of values for left and the pair for
           right have to be summed later.  */
        acc_a = vec_splat_s32(0);
        acc_b = vec_splat_s32(0);
        /* off: offset in input corresponding to first sample in
           filter */
        off = (int) (x >> 32);
        /* fixd0, fidx1: start, end indexes of 8-word (16-byte) chunks
           of whole FIR data we will use */
        fidx0 = (-off + 7) >> 3;
        fidx1 = (inlen - off) >> 3;
        if (fidx0 > 0) {
            if (fidx0 > flen) {
                acc = vec_splat_s32(0);
                goto flenv_accumulate;
            }
            accs0 = 0;
            accs1 = 0;
            for (j = -off; j < fidx0 * 8; ++j) {
                f = (((const short *) fd)[(fn+0) * (flen*8) + j] * ff0 +
                     ((const short *) fd)[(fn+1) * (flen*8) + j] * ff1)
                    >> INTERP_BITS;
                accs0 += ((const short *) in)[(j + off)*2 + 0] * f;
                accs1 += ((const short *) in)[(j + off)*2 + 1] * f;
            }
            u.w[0] = accs0;
            u.w[1] = accs1;
            acc_a = u.vs[0];
        } else {
            fidx0 = 0;
        }
        if (fidx1 < flen) {
            if (fidx1 < 0) {
                acc = vec_splat_s32(0);
                goto flenv_accumulate;
            }
            accs0 = 0;
            accs1 = 0;
            for (j = fidx1 * 8; j < inlen - off; ++j) {
                f = (((const short *) fd)[(fn+0) * (flen*8) + j] * ff0 +
                     ((const short *) fd)[(fn+1) * (flen*8) + j] * ff1)
                    >> INTERP_BITS;
                accs0 += ((const short *) in)[(j + off)*2 + 0] * f;
                accs1 += ((const short *) in)[(j + off)*2 + 1] * f;
            }
            u.w[0] = accs0;
            u.w[1] = accs1;
            acc_b = u.vs[0];
        } else {
            fidx1 = flen;
        }

        if (((uintptr_t) in + off * 4) & 15) {
            load_perm = vec_lvsl(off * 4, (unsigned char *) in);
            dat2 = vec_ld(fidx0 * 32 + off * 4, (short *) in);
            for (j = fidx0; j < fidx1; ++j) {
                dat0 = dat2;
                dat1 = vec_ld(j * 32 + off * 4 + 16, (short *) in);
                dat2 = vec_ld(j * 32 + off * 4 + 32, (short *) in);
                dat0 = vec_perm(dat0, dat1, load_perm);
                dat1 = vec_perm(dat1, dat2, load_perm);
                LOOP_ACCUM(flen, j);
            }
        } else {
            for (j = fidx0; j < fidx1; ++j) {
                dat0 = vec_ld(j * 32 + off * 4 + 0, (short *) in);
                dat1 = vec_ld(j * 32 + off * 4 + 16, (short *) in);
                LOOP_ACCUM(flen, j);
            }
        }
        acc = vec_add(acc_a, acc_b);

    flenv_accumulate:
        LOOP_STORE;
        x += inv_ratio;
    }
    goto done;

flen2:
    for (i = 0; i < outlen; ++i) {
        LOOP_FIRCOEFF;

        off = (int) (x >> 32);
        fidx0 = (-off + 7) >> 3;
        fidx1 = (inlen - off) >> 3;
        if (fidx0 > 0)
            goto flen2_slow;
        if (fidx1 < 2)
            goto flen2_slow;

        acc_a = vec_splat_s32(0);
        acc_b = vec_splat_s32(0);
        load_perm = vec_lvsl(off * 4, (short *) in);
        dat0 = vec_ld( 0 + off * 4, (short *) in);
        dat1 = vec_ld(16 + off * 4, (short *) in);
        dat2 = vec_ld(32 + off * 4, (short *) in);
        dat3 = vec_ld(48 + off * 4, (short *) in);
        dat4 = vec_ld(63 + off * 4, (short *) in);

        dat0 = vec_perm(dat0, dat1, load_perm);
        dat1 = vec_perm(dat1, dat2, load_perm);
        LOOP_ACCUM(2, 0);

        dat0 = vec_perm(dat2, dat3, load_perm);
        dat1 = vec_perm(dat3, dat4, load_perm);
        LOOP_ACCUM(2, 1);

        acc = vec_add(acc_a, acc_b);
        goto flen2_accumulate;

    flen2_slow:
        fidx0 = -off;
        if (fidx0 < 0)
            fidx0 = 0;
        fidx1 = inlen - off;
        if (fidx1 > 16)
            fidx1 = 16;

        accs0 = 0;
        accs1 = 0;
        for (j = fidx0; j < fidx1; ++j) {
            f = (((const short *) fd)[(fn+0) * 16 + j] * ff0 +
                 ((const short *) fd)[(fn+1) * 16 + j] * ff1)
                >> INTERP_BITS;
            accs0 += ((const short *) in)[(j + off)*2 + 0] * f;
            accs1 += ((const short *) in)[(j + off)*2 + 1] * f;
        }
        u.w[0] = accs0;
        u.w[1] = accs1;
        acc = u.vs[0];
        goto flen2_accumulate;

    flen2_accumulate:
        LOOP_STORE;
        x += inv_ratio;
    }
    goto done;

flen3:
    for (i = 0; i < outlen; ++i) {
        LOOP_FIRCOEFF;

        off = (int) (x >> 32);
        fidx0 = (-off + 7) >> 3;
        fidx1 = (inlen - off) >> 3;
        if (fidx0 > 0)
            goto flen3_slow;
        if (fidx1 < 3)
            goto flen3_slow;

        acc_a = vec_splat_s32(0);
        acc_b = vec_splat_s32(0);
        load_perm = vec_lvsl(off * 4, (short *) in);
        dat0 = vec_ld( 0 + off * 4, (short *) in);
        dat1 = vec_ld(16 + off * 4, (short *) in);
        dat2 = vec_ld(32 + off * 4, (short *) in);
        dat3 = vec_ld(48 + off * 4, (short *) in);
        dat4 = vec_ld(64 + off * 4, (short *) in);
        dat5 = vec_ld(80 + off * 4, (short *) in);
        dat6 = vec_ld(95 + off * 4, (short *) in);

        dat0 = vec_perm(dat0, dat1, load_perm);
        dat1 = vec_perm(dat1, dat2, load_perm);
        LOOP_ACCUM(3, 0);

        dat0 = vec_perm(dat2, dat3, load_perm);
        dat1 = vec_perm(dat3, dat4, load_perm);
        LOOP_ACCUM(3, 1);

        dat0 = vec_perm(dat4, dat5, load_perm);
        dat1 = vec_perm(dat5, dat6, load_perm);
        LOOP_ACCUM(3, 2);

        acc = vec_add(acc_a, acc_b);
        goto flen3_accumulate;

    flen3_slow:
        fidx0 = -off;
        if (fidx0 < 0)
            fidx0 = 0;
        fidx1 = inlen - off;
        if (fidx1 > 24)
            fidx1 = 24;

        accs0 = 0;
        accs1 = 0;
        for (j = fidx0; j < fidx1; ++j) {
            f = (((const short *) fd)[(fn+0) * 24 + j] * ff0 +
                 ((const short *) fd)[(fn+1) * 24 + j] * ff1)
                >> INTERP_BITS;
            accs0 += ((const short *) in)[(j + off)*2 + 0] * f;
            accs1 += ((const short *) in)[(j + off)*2 + 1] * f;
        }
        u.w[0] = accs0;
        u.w[1] = accs1;
        acc = u.vs[0];
        goto flen3_accumulate;

    flen3_accumulate:
        LOOP_STORE;
        x += inv_ratio;
    }
    goto done;

done:
    u.vw[0] = dsv;
    ds = u.w[0];
    for (i = 0; i < (outlen & 7); ++i)
        ds = LCG_A * ds + LCG_C;
    *pos = x;
    *dither = ds;

    /* Store remaing bytes */
    if ((outlen & 3) == 0) {
        out0 = vec_perm(out0, out0, store_perm);
        i = outlen + 4;
        goto final;
    }
    acc = vec_splat_s32(0);
    for (i = outlen; ; ++i) {
        switch (i & 3) {
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

            out1 = vec_packs(
                vec_sra(acc1, acc_shift),
                vec_sra(acc0, acc_shift));
            out0 = vec_perm(out0, out1, store_perm);
            out1 = vec_perm(out1, out1, store_perm);
            goto final;
       }
    }

final:
    /* byte offset of data point i in (out0, out1) */
    off = (int) ((uintptr_t) out & 15) + 16;
    if (off > 4*i)
        a0 = off - 4*i;
    else
        a0 = 0;
    if (off + 4*(outlen - i) > 32)
        a1 = 32;
    else
        a1 = off + 4*(outlen - i);

    u.vh[0] = out0;
    u.vh[1] = out1;
    memcpy((char *) out + 4*i - off + a0, (char *) &u + a0, a1 - a0);
}

#endif
