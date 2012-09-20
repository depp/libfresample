/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "audio.h"
#include "common.h"

#include <stdlib.h>

void
audio_init(struct audio *a)
{
    a->alloc = NULL;
    a->data = NULL;
    a->nframe = 0;
    a->fmt = AUDIO_U8;
    a->nchan = 0;
    a->rate = 0;
}

void
audio_destroy(struct audio *a)
{
    if (a->alloc)
        free(a->alloc);
    a->alloc = NULL;
}

void
audio_alloc(struct audio *a,
            size_t nframe, afmt_t format, int nchan, int rate)
{
    size_t ssz, fsz;

    ssz = audio_format_size(format);
    if ((size_t) nchan > (size_t) -1 / ssz)
        error("audio too large");
    fsz = ssz * nchan;
    if (nframe > (size_t) -1 / fsz)
        error("audio too large");

    if (a->alloc)
        free(a->alloc);
    a->alloc = NULL;
    a->alloc = xmalloc(fsz * nframe);
    a->data = a->alloc;
    a->nframe = nframe;
    a->fmt = format;
    a->nchan = nchan;
    a->rate = rate;
}
