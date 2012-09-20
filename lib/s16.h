/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#include "fresample.h"

/*
  Array of filters for filtering 16-bit integer data.

  Let N=2^log2nfilt, M=nsamp.

  This is an array of N+1 filters, 0..N, with each filter M samples
  long.  Filter number I will be centered on the sample with offset:

  floor(M/2) + I/N

  This means that filter N will be a copy of filter 0, except shifted
  to the right by one sample.  This extra filter makes the
  interpolation code simpler.
*/
struct lfr_s16 {
    /*
      FIR filter data.  This is not allocated separately, but is part
      of the same block of memory as this structure.
    */
    short *data;

    /*
      Length of each filter, in samples.  This is chosen to make each
      filter size a multiple of the SIMD register size.  Most SIMD
      implementations today use 16 byte registers, so this will
      usually be a multiple of 8.
    */
    int nsamp;

    /*
      Base 2 logarithm of the number of different filters.  There is
      always an additional filter at the end of the filter array to
      simplify interpolation, this extra filter is a copy of the first
      filter shifted right by one sample.
    */
    int log2nfilt;
};
