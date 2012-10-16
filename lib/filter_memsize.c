/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "filter.h"

int
lfr_filter_memsize(const struct lfr_filter *fp)
{
    int sz = fp->nsamp * ((1 << fp->log2nfilt) + 1);
    switch (fp->type) {
    case LFR_FTYPE_S16: return sz * 2;
    case LFR_FTYPE_F32: return sz * 4;
    default: return 0;
    }
}
