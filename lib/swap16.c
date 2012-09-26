/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#define LFR_IMPLEMENTATION 1

#include "convert.h"
#include "cpu.h"

void
lfr_swap16(void *dest, const void *src, size_t count)
{
    lfr_swap16_scalar(dest, src, count);
}
