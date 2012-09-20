/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "audio.h"

#include <string.h>

const char AUDIO_FORMAT_NAME[AUDIO_NUMFMT][6] = {
    "U8",
    "S16BE",
    "S16LE",
    "S24BE",
    "S24LE",
    "F32BE",
    "F32LE"
};

const char *
audio_format_name(afmt_t fmt)
{
    return AUDIO_FORMAT_NAME[fmt];
}

int
audio_format_lookup(const char *name)
{
    int i, c;
    char tmp[sizeof(*AUDIO_FORMAT_NAME)];
    for (i = 0; i < sizeof(tmp); ++i) {
        c = name[i];
        if (c >= 'a' && c <= 'z')
            c += 'A' - 'a';
        tmp[i] = c;
        if (!c)
            break;
    }
    if (i >= sizeof(tmp))
        return -1;
    for (i = 0; i < sizeof(tmp); ++i)
        tmp[i] = '\0';
    for (i = 0; i < AUDIO_NUMFMT; ++i) {
        if (!memcmp(AUDIO_FORMAT_NAME[i], tmp, sizeof(tmp)))
            return i;
    }
    return -1;
}

static const unsigned char AUDIO_FORMAT_SIZE[AUDIO_NUMFMT] = {
    1, 2, 2, 3, 3, 4, 4
};

size_t
audio_format_size(afmt_t fmt)
{
    return AUDIO_FORMAT_SIZE[fmt];
}
