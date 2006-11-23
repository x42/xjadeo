bin_PROGRAMS=xjadeo xjremote xjinfo

xjadeo_SOURCES=xjadeo.c ../../aclocal.m4 ../../config.h getopt.c getopt1.c getopt.h display.c jack.c midi.c freetype.c xjadeo.h display_sdl.c display.h display_x11.c smpte.c main.c remote.c configfile.c lash.c mqueue.c icons/osd_bitmaps.h

xjadeo_LDADD = -ljack -lXext $(MQ_LIBS) @FFMPEG_LIBS@  $(SDL_LIBS) $(XV_LIBS) $(IMLIB_LIBS) $(MIDI_LIBS) @FREETYPE_LIBS@ $(IMLIB2_LIBS) $(LASH_LIBS) $(Xpm_LIBS)

xjadeo_CFLAGS = -Wall -g -O3 @FFMPEG_CFLAGS@ $(SDL_CFLAGS) $(XV_CFLAGS) $(IMLIB_CFLAGS) $(MIDI_CFLAGS) @FREETYPE_CFLAGS@ $(IMLIB2_CFLAGS) $(LASH_CFLAGS)


xjremote_SOURCES = xjremote.c getopt.c getopt1.c getopt.h

xjremote_LDADD = $(MQ_LIBS)


xjinfo_SOURCES = avinfo.c getopt.c getopt1.c getopt.h

xjinfo_LDADD = @FFMPEG_LIBS@  

xjinfo_CFLAGS = -Wall -g -O3 @FFMPEG_CFLAGS@ 

#ctags:
#	find . -type f -name "*.[ch]*" | xargs @CTAGS@

MAINTAINERCLEANFILES = Makefile.in

CLEANFILES = paths.h
BUILT_SOURCES = paths.h icons/xjadeo-color.xpm  icons/xjadeo.bitmap  icons/xjadeo_mask.xbm

paths.h: Makefile
	@echo '#define BINDIR "$(DESTDIR)/$(bindir)/"' >$@
	@echo '#define SYSCONFDIR "$(DESTDIR)/$(sysconfdir)/"' >>$@
