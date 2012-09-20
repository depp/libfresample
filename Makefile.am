AUTOMAKE_OPTIONS = 1.5 foreign subdir-objects

lib_LIBRARIES = libfresample.a

AM_CPPFLAGS = -Iinclude
AM_CFLAGS = @CWARN@

libfresample_a_SOURCES = \
	lib/s16.h \
	lib/s16_new_lowpass.c

include_HEADERS = \
	include/fresample.h
