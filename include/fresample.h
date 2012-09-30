/* Copyright 2012 Dietrich Epp <depp@zdome.net> */
#ifndef FRESAMPLE_H
#define FRESAMPLE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#define LFR_RESTRICT
#define LFR_PUBLIC
#if defined(LFR_IMPLEMENTATION)
# define LFR_PRIVATE
#endif

#if defined(_MSC_VER)
# undef LFR_RESTRICT
# define LFR_RESTRICT __restrict
# define LFR_INT64 __int64
#endif

#if defined(__GNUC__)
# undef LFR_RESTRICT
# define LFR_RESTRICT __restrict
# undef LFR_PRIVATE
# undef LFR_PUBLIC
# if defined(LFR_IMPLEMENTATION)
#  if defined(__ELF__)
#   define LFR_PRIVATE __attribute__((visibility("internal")))
#   define LFR_PUBLIC __attribute__((visibility("protected")))
#  else
#   define LFR_PRIVATE __attribute__((visibility("hidden")))
#   define LFR_PUBLIC __attribute__((visibility("default")))
#  endif
# else
#  define LFR_PUBLIC
# endif
# define LFR_INT64 long long
#endif

#if defined(__STDC_VERSION__)
# if __STDC_VERSION__ >= 199901L
#  undef LFR_RESTRICT
#  define LFR_RESTRICT restrict
# endif
#endif

#if defined(_M_X64) || defined(__x86_64__)
# define LFR_CPU_X64 1
# define LFR_CPU_X86 1
#elif defined(_M_IX86) || defined(__i386__)
# define LFR_CPU_X86 1
#elif defined(__ppc64__)
# define LFR_CPU_PPC64 1
# define LFR_CPU_PPC 1
#elif defined(__ppc__)
# define LFR_CPU_PPC 1
#endif

#define LFR_LITTLE_ENDIAN 1234
#define LFR_BIG_ENDIAN 4321

#if defined(__BYTE_ORDER__)
# if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define LFR_BYTE_ORDER LFR_BIG_ENDIAN
# elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#  define LFR_BYTE_ORDER LFR_LITTLE_ENDIAN
# endif
#elif defined(__BIG_ENDIAN__)
# define LFR_BYTE_ORDER LFR_BIG_ENDIAN
#elif defined(__LITTLE_ENDIAN__)
# define LFR_BYTE_ORDER LFR_LITTLE_ENDIAN
#endif

#if !defined(LFR_BYTE_ORDER)
# if defined(LFR_CPU_X86)
#  define LFR_BYTE_ORDER LFR_LITTLE_ENDIAN
# elif defined(LFR_CPU_PPC)
#  define LFR_BYTE_ORDER LFR_BIG_ENDIAN
# else
#  error "cannot determine machine byte order"
# endif
#endif

/*
  CPU features to use or disable.
*/
enum {
#if defined(LFR_CPU_X86)
    LFR_CPUF_MMX     = (1u << 0),
    LFR_CPUF_SSE     = (1u << 1),
    LFR_CPUF_SSE2    = (1u << 2),
    LFR_CPUF_SSE3    = (1u << 3),
    LFR_CPUF_SSSE3   = (1u << 4),
    LFR_CPUF_SSE4_1  = (1u << 5),
    LFR_CPUF_SSE4_2  = (1u << 6),
#else
    LFR_CPUF_MMX     = 0u,
    LFR_CPUF_SSE     = 0u,
    LFR_CPUF_SSE2    = 0u,
    LFR_CPUF_SSE3    = 0u,
    LFR_CPUF_SSSE3   = 0u,
    LFR_CPUF_SSE4_1  = 0u,
    LFR_CPUF_SSE4_2  = 0u,
#endif

#if defined(LFR_CPU_PPC)
    LFR_CPUF_ALTIVEC = (1u << 0),
#else
    LFR_CPUF_ALTIVEC = 0u,
#endif

    LFR_CPUF_NONE = 0u,
    LFR_CPUF_ALL = 0xffffffffu
};

struct lfr_cpuf {
    char name[8];
    unsigned flag;
};

/*
  Array of names for the CPU features this architecture supports.
  Names are the lower case version of the flag names above, e.g.,
  LFR_CPUF_MMX becomes "mmx".  Terminated by a zeroed entry.
*/
LFR_PUBLIC extern const struct lfr_cpuf LFR_CPUF[];

/*
  Set which CPU features are allowed or disallowed.  This is primarily
  used for comparing the performance and correctness of vector
  implementations and scalar implementations.  It can also be used to
  prohibit features that your CPU supports but which your OS does not.

  Returns the CPU flags actually enabled, which will be the
  intersection of the set of allowed flags (the argument) with the set
  of features that the current CPU actually supports.
*/
LFR_PUBLIC unsigned
lfr_setcpufeatures(unsigned flags);

/*
  Audio sample formats.
*/
typedef enum {
    LFR_FMT_U8,
    LFR_FMT_S16BE,
    LFR_FMT_S16LE,
    LFR_FMT_S24BE,
    LFR_FMT_S24LE,
    LFR_FMT_F32BE,
    LFR_FMT_F32LE
} lfr_fmt_t;

#define LFR_FMT_COUNT ((int) LFR_FMT_F32LE + 1)

#if LFR_BYTE_ORDER == LFR_BIG_ENDIAN
# define LFR_FMT_S16_NATIVE LFR_FMT_S16BE
# define LFR_FMT_S16_SWAPPED LFR_FMT_S16LE
# define LFR_FMT_F32_NATIVE LFR_FMT_F32BE
# define LFR_FMT_F32_SWAPPED LFR_FMT_F32LE
#else
# define LFR_FMT_S16_NATIVE LFR_FMT_S16LE
# define LFR_FMT_S16_SWAPPED LFR_FMT_S16BE
# define LFR_FMT_F32_NATIVE LFR_FMT_F32LE
# define LFR_FMT_F32_SWAPPED LFR_FMT_F32BE
#endif

/*
  Swap the byte order on 16-bit data.  The destination can either be
  the same buffer as the source, or it can be a non-overlapping
  buffer.  Behavior is undefined if the two buffers partially overlap.
*/
LFR_PUBLIC void
lfr_swap16(void *dest, const void *src, size_t count);

/*
  Names for filter quality presets.
*/
enum {
    LFR_QUALITY_LOW,
    LFR_QUALITY_MEDIUM,
    LFR_QUALITY_HIGH,
    LFR_QUALITY_ULTRA
};

/*
  A 32.32 fixed point number.  This is used for expressing fractional
  positions in a buffer.

  When used for converting from sample rate f_s to f_d, the timing
  error at time t is bounded by t * 2^-33 * r_d / r_s.  For a
  pessimistic conversion ratio, 8 kHz -> 192 kHz, this means that it
  will take at least five days to accumulate one millisecond of error.
*/
typedef LFR_INT64 lfr_fixed_t;

/*
  A low-pass filter for resampling 16-bit integer audio.
*/
struct lfr_s16;

/*
  Free a low-pass filter.
*/
LFR_PUBLIC void
lfr_s16_free(struct lfr_s16 *fp);

/*
  Create a new windowed sinc filter with the given parameters.
  Normally this function is not called directly.

  nsamp: filter size, in samples

  log2nfilt: base 2 logarithm of the number of filters

  cutoff: cutoff frequency, in cycles per sample

  beta: Kaiser window beta parameter

  Returns NULL when out of memory.
*/
LFR_PUBLIC struct lfr_s16 *
lfr_s16_new_sinc(
    int nsamp, int log2nfilt, double cutoff, double beta);

/*
  Create a new low-pass filter for 16-bit data with the given
  parameters: sample rate, pass band frequency, stop band frequency,
  and signal to noise ratio.  Frequencies are measured in Hz, the SNR
  is measured in dB.  The SNR is clipped at 96 due to limitations when
  working with 16-bit data.  Normally, this function is not called
  directly.

  Returns NULL when out of memory.

  This function works by calling lfr_s16_new_sinc().
*/
LFR_PUBLIC struct lfr_s16 *
lfr_s16_new_lowpass(
    double f_rate, double f_pass,
    double f_stop, double snr);

/*
  Create a new low-pass filter for 16-bit data for resampling with the
  given parameters: input sample rate, output sample rate, signal to
  noise ratio, transition width, and "loose" flag.

  The input and output frequencies are measured in Hz.

  The signal to noise ratio is measured in dB, and clipped at 96 dB
  due to limitations when working with 16-bit data.

  The transition width is specified as a fraction of the input sample
  rate.  This allows controlling the trade-off between bandwidth and
  filter size.  Note that the transition band will be narrowed
  (increasing filter size) to ensure that the pass band is at least
  50% of the input or output bandwidth, whichever is smaller.  The
  transition band will also be widened (reducing filter size) to limit
  the pass band to 20 kHz, as there's no sense in wasting CPU cycles
  to preserve ultrasonic frequencies.

  The "loose" flag, when set, allows the stop band to start at higher
  frequencies.  This will create ultrasonic artifacts above the noise
  floor.  Most people probably won't hear them, but some people have
  better hearing, and poor audio systems can modulate ultrasonics to
  audible frequencies.

  Returns NULL when out of memory.

  This function works by calling lfr_s16_new_lowpass().
*/
LFR_PUBLIC struct lfr_s16 *
lfr_s16_new_resample(
    int f_inrate, int f_outrate,
    double snr, double transition, int loose);

/*
  Create a new low-passs filter for 16-bit data for resampling between
  the given sample rates.  The filter quality is an integer in the
  range 0..3 which specifies the filter quality.  Quality levels 2 and
  3 are considered high quality.  The predefined LFR_QUALITY constants
  can be used here.

  Returns NULL when out of memory.

  This function works by calling lfr_s16_new_resample().
*/
LFR_PUBLIC struct lfr_s16 *
lfr_s16_new_preset(
    int f_inrate, int f_outrate, int quality);

/*
  Resample 16-bit integer audio.

  pos: Current position relative to the start of the input buffer,
  expressed as a 32.32 fixed point number.  On return, this will
  contain the updated position.  Positions outside the input buffer
  are acceptable, it will be treated as if the input buffer were
  padded with an unlimited number of zeroes on either side.

  inv_ratio: Inverse of the resampling ratio, expressed as a 32.32
  fixed point number.  This number is equal to the input sample rate
  divided by the output sample rate.

  dither: State of the PRNG used for dithering.

  out, in: Input and output buffers.  The buffers are not permitted to
  alias each other.

  outlen, inlen: Length of buffers, in frames.  Note that the length
  type is 'int' instead of 'size_t'; this matches the precision of
  buffer positions.

  filter: A suitable low-pass filter for resampling at the given
  ratio.
*/

LFR_PUBLIC void
lfr_s16_resample_mono(
    lfr_fixed_t *LFR_RESTRICT pos, lfr_fixed_t inv_ratio,
    unsigned *dither,
    short *LFR_RESTRICT out, int outlen,
    const short *LFR_RESTRICT in, int inlen,
    const struct lfr_s16 *LFR_RESTRICT filter);

LFR_PUBLIC void
lfr_s16_resample_stereo(
    lfr_fixed_t *LFR_RESTRICT pos, lfr_fixed_t inv_ratio,
    unsigned *dither,
    short *LFR_RESTRICT out, int outlen,
    const short *LFR_RESTRICT in, int inlen,
    const struct lfr_s16 *LFR_RESTRICT filter);

#ifdef __cplusplus
}
#endif
#endif
