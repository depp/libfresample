AUTOMAKE_OPTIONS = 1.5 foreign subdir-objects

lib_LIBRARIES = libfresample.a
bin_PROGRAMS = fresample

AM_CPPFLAGS = -Iinclude
AM_CFLAGS = @CWARN@

libfresample_a_SOURCES = \
	lib/calculate.h \
	lib/calculate.c \
	lib/s16.h \
	lib/s16_free.c \
	lib/s16_new_lowpass.c \
	lib/s16_new_preset.c \
	lib/s16_new_resample.c \
	lib/s16_new_sinc.c \
	lib/s16_resample_mono.c \
	lib/s16_resample_stereo.c

include_HEADERS = \
	include/fresample.h

fresample_SOURCES = \
	src/audio.c \
	src/audio.h \
	src/audio_format.c \
	src/audio_rate.c \
	src/audio_raw_load.c \
	src/audio_wav_check.c \
	src/audio_wav_load.c \
	src/audio_wav_save.c \
	src/common.c \
	src/common.h \
	src/file.c \
	src/file.h \
	src/main.c \
	src/riff.c \
	src/riff.h

fresample_LDADD = libfresample.a
