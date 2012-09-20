/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "audio.h"

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
