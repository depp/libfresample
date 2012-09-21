/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "audio.h"
#include "common.h"
#include "file.h"
#include "fresample.h"

#include <math.h>
#include <stdio.h>

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

int
main(int argc, char *argv[])
{
    int rate;
    size_t len;
    struct file_data din;
    struct audio ain, aout;
    char frate[AUDIO_RATE_FMTLEN], fnchan[32];
    struct lfr_s16 *fp;
    FILE *file;

    if (argc != 4) {
        fputs("usage: fresample RATE IN OUT\n", stderr);
        return 1;
    }

    rate = audio_rate_parse(argv[1]);
    if (rate < 0) {
        fprintf(stderr, "error: invalid sample rate '%s'\n", argv[1]);
        return 1;
    }

    file_read(&din, argv[2]);

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

    len = (size_t) floor(
        (double) ain.nframe * (double) rate / (double) ain.rate + 0.5);

    audio_rate_format(frate, sizeof(frate), rate);
    printf("Output: %s, %s, %s, %zu samples\n",
           audio_format_name(ain.fmt), frate, fnchan, len);
    audio_alloc(&aout, len, ain.fmt, ain.nchan, rate);

    fp = lfr_s16_new_preset(ain.rate, aout.rate, 3);

    if (ain.nchan == 1) {
        lfr_s16_resample_mono(
            aout.alloc, aout.nframe, aout.rate,
            ain.data, ain.nframe, ain.rate,
            fp);
    } else {
        lfr_s16_resample_stereo(
            aout.alloc, aout.nframe, aout.rate,
            ain.data, ain.nframe, ain.rate,
            fp);
    }

    file_destroy(&din);
    audio_destroy(&ain);

    file = fopen(argv[3], "wb");
    if (!file)
        error("error opening output file");
    audio_wav_save(file, &aout);
    fclose(file);

    audio_destroy(&aout);

    return 0;
}
