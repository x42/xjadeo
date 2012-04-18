#TODO: use a dedicated script for 'git desc' - this is "non-POSIX" way:
REV = $(shell test -d ../../.git && git describe --always --tags HEAD 2>/dev/null)

bin_PROGRAMS=xjadeo xjremote xjinfo

xjadeo_SOURCES=xjadeo.c ../../aclocal.m4 ../../config.h display.c jack.c midi.c freetype.c xjadeo.h display.h display_x11.c smpte.c main.c remote.c configfile.c lash.c mqueue.c icons/osd_bitmaps.h display_mac.c xjosc.c display_sdl.c ltc-jack.c ffcompat.h

xjadeo_LDADD = @JACK_LIBS@ @MQ_LIBS@ @FFMPEG_LIBS@ @XV_LIBS@ @IMLIB_LIBS@ @MIDI_LIBS@ @FREETYPE_LIBS@ @IMLIB2_LIBS@ @LASH_LIBS@ @Xpm_LIBS@ @LIBLO_LIBS@ @SDL_LIBS@ @LTCSMPTE_LIBS@ -lm

xjadeo_CFLAGS = -Wall -g -O3 @JACK_CFLAGS@ @FFMPEG_CFLAGS@ @XV_CFLAGS@ @IMLIB_CFLAGS@ @MIDI_CFLAGS@ @FREETYPE_CFLAGS@ @IMLIB2_CFLAGS@ @LASH_CFLAGS@ @LIBLO_CFLAGS@ @SDL_CFLAGS@ @LTCSMPTE_CFLAGS@ "-DSUBVERSION=\"$(REV)\""


xjremote_SOURCES = xjremote.c

xjremote_LDADD = @MQ_LIBS@


xjinfo_SOURCES = avinfo.c ffcompat.h

xjinfo_LDADD = @FFMPEG_LIBS@  

xjinfo_CFLAGS = -Wall -g -O3 @FFMPEG_CFLAGS@ 

#ctags:
#	find . -type f -name "*.[ch]*" | xargs @CTAGS@

MAINTAINERCLEANFILES = Makefile.in

CLEANFILES = paths.h
BUILT_SOURCES = paths.h 

xjadeo_DEPENDENCIES= \
	icons/brightness.bitmap    icons/gamma.bitmap    icons/osd_bitmaps.h        icons/splash_mask.xbm \
	icons/brightness_mask.xbm  icons/gamma_mask.xbm  icons/saturation.bitmap    icons/xjadeo-color.xpm \
	icons/contrast.bitmap      icons/hue.bitmap      icons/saturation_mask.xbm  icons/xjadeo.bitmap \
	icons/contrast_mask.xbm    icons/hue_mask.xbm    icons/splash.bitmap        icons/xjadeo_mask.xbm 

paths.h: Makefile
	@echo '#define BINDIR "$(DESTDIR)/$(bindir)/"' >$@
	@echo '#define SYSCONFDIR "$(DESTDIR)/$(sysconfdir)/"' >>$@
