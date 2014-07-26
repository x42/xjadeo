noinst_PROGRAMS=jadeo-rcli

jadeo_rcli_SOURCES = jadeo.c ../../aclocal.m4 ../../config.h \
                     ../../src/xjadeo/jack.c ../../src/xjadeo/midi.c \
                     ../../src/xjadeo/smpte.c ../../src/xjadeo/common_jack.c \
                     ../../src/xjadeo/weak_libjack.c

jadeo_rcli_CFLAGS = -Wall -g -O3  -I../../src/xjadeo/ @MIDI_CFLAGS@ @JACK_CFLAGS@
jadeo_rcli_LDADD = @MIDI_LIBS@ -lm

if TARGET_LINUX
jadeo_rcli_LDADD+=@JACK_LIBS@
endif
if TARGET_WIN32
jadeo_rcli_CFLAGS+=-DUSE_WEAK_JACK
jadeo_rcli_LDADD+=-lws2_32 -lwinmm
endif
if TARGET_OSX
jadeo_rcli_CFLAGS+=-DUSE_WEAK_JACK
jadeo_rcli_LDADD+=-ldl
endif

MAINTAINERCLEANFILES = Makefile.in

