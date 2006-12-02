bin_PROGRAMS=xjtsmm

xjtsmm_SOURCES = tsmm.c ../../aclocal.m4 ../../config.h  ../../src/xjadeo/smpte.c ../../src/xjadeo/freetype.c
xjtsmm_CFLAGS = -Wall -g -O3  -I../../src/xjadeo/ $(TIFF_CFLAGS) @FREETYPE_CFLAGS@ $(LASH_CFLAGS)
xjtsmm_LDADD = $(TIFF_LIBS) @FREETYPE_LIBS@ 

MAINTAINERCLEANFILES = Makefile.in

# TODO: properly install perl-script this when it's completed
EXTRA_DIST=tsmm.pl
