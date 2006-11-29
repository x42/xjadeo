bin_PROGRAMS=jadeo-rcli

jadeo_rcli_SOURCES = jadeo.c ../../aclocal.m4 ../../config.h ../../src/xjadeo/jack.c ../../src/xjadeo/midi.c ../../src/xjadeo/smpte.c
jadeo_rcli_CFLAGS = -Wall -g -O3  -I../../src/xjadeo/ $(LASH_CFLAGS)
jadeo_rcli_LDADD = -ljack $(MIDI_LIBS) $(LASH_LIBS)

MAINTAINERCLEANFILES = Makefile.in

