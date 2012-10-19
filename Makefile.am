AUTOMAKE_OPTIONS = 1.5 foreign subdir-objects
ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libfresample.la
noinst_PROGRAMS = fresample

AM_CPPFLAGS = -I@srcdir@/include
AM_CFLAGS = @CWARN@

libfresample_la_SOURCES = \
	lib/convert.h \
	lib/cpu.c \
	lib/cpu.h \
	lib/defs.h \
	lib/filter_delay.c \
	lib/filter_free.c \
	lib/filter_get.c \
	lib/filter.h \
	lib/filter_new.c \
	lib/filter_new_window.c \
	lib/info_name.c \
	lib/param.c \
	lib/param.h \
	lib/param_name.c \
	lib/resample.c \
	lib/resample.h \
	lib/resample_s16func.c \
	lib/resample_s16n1f32_altivec.c \
	lib/resample_s16n1f32_scalar.c \
	lib/resample_s16n1f32_sse2.c \
	lib/resample_s16n1s16_altivec.c \
	lib/resample_s16n1s16_scalar.c \
	lib/resample_s16n1s16_sse2.c \
	lib/resample_s16n2f32_altivec.c \
	lib/resample_s16n2f32_scalar.c \
	lib/resample_s16n2f32_sse2.c \
	lib/resample_s16n2s16_altivec.c \
	lib/resample_s16n2s16_scalar.c \
	lib/resample_s16n2s16_sse2.c \
	lib/swap16.c \
	lib/swap16_scalar.c

include_HEADERS = \
	include/fresample.h

fresample_SOURCES = \
	src/audio.c \
	src/audio_format.c \
	src/audio.h \
	src/audio_rate.c \
	src/audio_raw_load.c \
	src/audio_wav_check.c \
	src/audio_wav_load.c \
	src/audio_wav_save.c \
	src/binary.h \
	src/common.c \
	src/common.h \
	src/file.c \
	src/file.h \
	src/main.c \
	src/riff.c \
	src/riff.h

fresample_LDADD = libfresample.la

check-local: fresample
	cd tests && $(MAKE) check

clean-local:
	cd tests && $(MAKE) clean

EXTRA_DIST = \
	autogen.sh \
	LibFResample.xcodeproj/project.pbxproj \
	LICENSE.txt \
	README.txt \
	tests/benchmark.py \
	tests/common.py \
	tests/dumpspecs.py \
	tests/genmake.py \
	tests/quality.py
