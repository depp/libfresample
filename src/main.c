/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "audio.h"
#include "file.h"

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
    struct file_data din;
    struct audio ain;
    char frate[AUDIO_RATE_FMTLEN], fnchan[32];

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
    audio_wav_load(&ain, din.data, din.length);

    audio_rate_format(frate, sizeof(frate), ain.rate);
    nchan_format(fnchan, sizeof(fnchan), ain.nchan);
    printf("Input: %s, %s, %s, %zu samples\n",
           audio_format_name(ain.fmt), frate, fnchan, ain.nframe);

    audio_destroy(&ain);

    return 0;
}
