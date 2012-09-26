/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "audio.h"
#include "common.h"
#include "file.h"
#include "fresample.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void
cpu_features_set(const char *str)
{
    char tmp[8];
    const char *p = str, *q;
    size_t n, i;
    int c;
    unsigned flags = 0, has_feature;

    while (1) {
        q = strchr(p, ',');
        n = q ? (size_t) (q - p) : strlen(p);
        if (n >= sizeof(tmp))
            goto unknown;
        if (!n)
            goto next;
        for (i = 0; i < n; ++i) {
            c = p[i];
            if (c >= 'A' && c <= 'Z')
                c += 'a' - 'A';
            tmp[i] = c;
        }
        tmp[n] = '\0';
        if (!strcmp(tmp, "all")) {
            flags |= LFR_CPUF_ALL;
            goto next;
        }
        if (!strcmp(tmp, "none"))
            goto next;
        for (i = 0; LFR_CPUF[i].name[0]; ++i) {
            if (!strcmp(tmp, LFR_CPUF[i].name)) {
                flags |= LFR_CPUF[i].flag;
                goto next;
            }
        }
        goto unknown;

    unknown:
        fprintf(stderr, "unknown CPU feature: %.*s\n", (int) n, tmp);
        goto next;

    next:
        p = p + n;
        if (!*p)
            break;
        p++;
    }

    flags = lfr_setcpufeatures(flags);
    has_feature = 0;
    fputs("CPU features enabled: ", stderr);
    for (i = 0; LFR_CPUF[i].name[0]; ++i) {
        if ((flags & LFR_CPUF[i].flag) == 0)
            continue;
        if (has_feature)
            fputs(", ", stderr);
        fputs(LFR_CPUF[i].name, stderr);
        has_feature = 1;
    }
    if (!has_feature) {
        fputs("none (scalar implementations only)", stderr);
    }
    fputc('\n', stderr);
}

#define AUDIO_MINRATE 8000
#define AUDIO_MAXRATE 192000

static void
nchan_format(char *buf, size_t buflen, int nchan)
{
    if (nchan == 1)
        snprintf(buf, buflen, "mono");
    else if (nchan == 2)
        snprintf(buf, buflen, "stereo");
    else
        snprintf(buf, buflen, "%d channels", nchan);
}

static const char USAGE[] =
    "usage:\n"
    "  fresample [-r RATE] [-q QUALITY] IN OUT\n"
    "  fresample -h\n"
    "  fresample -v\n";

int
main(int argc, char *argv[])
{
    long v, benchmark, bi;
    int rate, opt, quality, nfiles;
    size_t len;
    struct file_data din;
    struct audio ain, aout;
    char frate[AUDIO_RATE_FMTLEN], fnchan[32], *e, *files[2];
    struct lfr_s16 *fp;
    FILE *file;
    clock_t t0, t1;

    benchmark = -1;
    nfiles = 0;
    rate = -1;
    quality = -1;
    while ((opt = getopt(argc, argv, ":b:c:hq:r:v")) != -1) {
        switch (opt) {
        case 'b':
            benchmark = strtol(optarg, &e, 10);
            if (!*optarg || *e || benchmark < 1) {
                fprintf(stderr, "error: invalid benchmark count '%s'\n",
                        optarg);
                return 1;
            }
            break;

        case 'c':
            cpu_features_set(optarg);
            break;

        case 'h':
            fputs(USAGE, stderr);
            return 1;

        case 'q':
            v = strtol(optarg, &e, 10);
            if (!*optarg || *e) {
                fprintf(stderr, "error: invalid quality '%s'\n", optarg);
                return 1;
            }
            if (v > 3) v = 3;
            else if (v < 0) v = 0;
            quality = (int) v;
            break;

        case 'r':
            rate = audio_rate_parse(optarg);
            if (rate < 0) {
                fprintf(stderr, "error: invalid sample rate '%s'\n", argv[1]);
                return 1;
            }
            break;

        case 'v':
            fputs("FResample version 0.0\n", stdout);
            break;

        case ':':
            fprintf(stderr, "error: option -%c requires an argument\n",
                    optopt);
            return 1;

        default:
        case '?':
            fprintf(stderr, "error: unrecognized option -%c\n", optopt);
            return 1;
        }
    }
    for (; optind < argc; ++optind) {
        if (nfiles >= 2) {
            fputs("error: too many files specified\n", stderr);
            return 1;
        }
        files[nfiles++] = argv[optind];
    }
    if (nfiles != 2) {
        fputs(USAGE, stderr);
        return 1;
    }
    if (rate < 0) {
        fputs("error: no rate specified\n", stderr);
        return 1;
    }
    if (quality < 0)
        quality = 3;

    file_read(&din, files[0]);

    audio_init(&ain);
    audio_init(&aout);
    audio_wav_load(&ain, din.data, din.length);

    audio_rate_format(frate, sizeof(frate), ain.rate);
    nchan_format(fnchan, sizeof(fnchan), ain.nchan);
    printf("Input: %s, %s, %s, %zu samples\n",
           audio_format_name(ain.fmt), frate, fnchan, ain.nframe);

    if (ain.fmt != AUDIO_S16LE)
        error("unsupported format");
    if (ain.nchan != 1 && ain.nchan != 2)
        error("unsupported number of channels "
              "(only mono and stereo supported)");

    if (ain.rate == aout.rate) {
        fputs("No rate conversion necessary\n", stdout);
        audio_alias(&aout, &ain);
    } else {
        len = (size_t) floor(
            (double) ain.nframe * (double) rate / (double) ain.rate + 0.5);

        audio_rate_format(frate, sizeof(frate), rate);
        printf("Output: %s, %s, %s, %zu samples\n",
               audio_format_name(ain.fmt), frate, fnchan, len);
        audio_alloc(&aout, len, ain.fmt, ain.nchan, rate);

        fp = lfr_s16_new_preset(ain.rate, aout.rate, quality);

        if (ain.nchan == 1) {
            lfr_s16_resample_mono(
                aout.alloc, aout.nframe, aout.rate,
                ain.data, ain.nframe, ain.rate,
                fp);

            if (benchmark > 0) {
                t0 = clock();
                for (bi = 0; bi < benchmark; ++bi) {
                    lfr_s16_resample_mono(
                        aout.alloc, aout.nframe, aout.rate,
                        ain.data, ain.nframe, ain.rate,
                        fp);
                }
                t1 = clock();
            }
        } else {
            lfr_s16_resample_stereo(
                aout.alloc, aout.nframe, aout.rate,
                ain.data, ain.nframe, ain.rate,
                fp);

            if (benchmark > 0) {
                t0 = clock();
                for (bi = 0; bi < benchmark; ++bi) {
                    lfr_s16_resample_stereo(
                        aout.alloc, aout.nframe, aout.rate,
                        ain.data, ain.nframe, ain.rate,
                        fp);
                }
                t1 = clock();
            }
        }

        if (benchmark > 0) {
            printf("Average time: %g s\n"
                   "Speed: %g\n",
                   (t1 - t0) / ((double) CLOCKS_PER_SEC * benchmark),
                   ((double) CLOCKS_PER_SEC * benchmark * ain.nframe) /
                   ((double) (t1 - t0) * ain.rate));
        }

        lfr_s16_free(fp);

        file_destroy(&din);
        audio_destroy(&ain);
    }

    file = fopen(files[1], "wb");
    if (!file)
        error("error opening output file");
    audio_wav_save(file, &aout);
    fclose(file);

    file_destroy(&din);
    audio_destroy(&ain);
    audio_destroy(&aout);

    return 0;
}
