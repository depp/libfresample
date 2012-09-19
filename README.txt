LibFResample
============

LibFResample is a library for resampling audio with a permissive
(FreeBSD style) license.  Like other audio resampling libraries, it is
based on the bandlimited interpolation algorithm described by Julius
Orion Smith.  LibFResample is designed to use SIMD operations where
available.

The "F" stands for "fast" or "free", whichever you prefer.

Theory of operation
-------------------

Resampling audio takes two steps: first you design a low-pass filter,
then you use the filter to resample the audio.  In general, a new
filter must be created for each combination of sample rates, filter
parameters, and bit depth.

The low-pass filter is used to remove aliasing from the resampled
output -- aliasing is undesirable noise created during the resampling
process.  Adjusting the filter parameters is the only way to change
the quality of the audio output.  There are two major parameters to
filter design:

1. Signal to noise ratio.  A filter with a higher SNR more efficiently
excludes aliasing noise, but increasing the SNR also makes the filter
larger.  Note that the noise is not pleasant noise like white noise;
the noise has a gritty, low-fi sound.

2. Bandwidth.  A filter with a higher bandwidth includes more of the
original signal, but increasing the bandwidth also makes the filter
larger.  (The crucial parameter is actually transition bandwidth,
between the filter's pass band and stop band.  Decreasing the
transition bandwidth increases the filter size.)

LibFResample includes a function which creates a filter with the
desired SNR and transition bandwidth, and there are also some preset
SNR and transition bandwidth combinations.  The function also performs
a number of adjustments of the design parameters to ensure that the
pass band is large enough and that the filter isn't overdesigned to
preserve ultrasonic frequencies.

Finally, the audio resampler supports 16-bit signed integer and 32-bit
floating point audio in mono and stereo.  24-bit and multi-channel
audio must be converted before it is resampled.

Internal details
----------------

The filter is a simple sinc filter windowed with the Kaiser window.
Choosing the parameter for the Kaiser window allows us to adjust the
level of the side lobes, and higher side lobes contribute to aliasing
noise.  Increasing the filter size decreases the width of the window's
main lobe, which decreases the transition bandwidth in the resulting
filter.

Many copies of the filter are generated, each with a fractional (less
than one sample) offset from each other.  Choosing a copy with a given
offset allows us to sample the original audio at that offset.
Interpolating between copies allows us to sample the audio at finer
intervals without a significant increase in memory usage.
