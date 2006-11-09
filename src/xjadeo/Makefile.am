bin_PROGRAMS=xjadeo

xjadeo_SOURCES=xjadeo.c ../../aclocal.m4 ../../config.h getopt.c getopt1.c getopt.h display.c jack.c midi.c freetype.c xjadeo.h display_sdl.c display.h display_x11.c smpte.c main.c remote.c configfile.c lash.c

xjadeo_LDADD = -ljack -lXext @FFMPEG_LIBS@  $(SDL_LIBS) $(XV_LIBS) $(IMLIB_LIBS) $(MIDI_LIBS) @FREETYPE_LIBS@ $(IMLIB2_LIBS) $(LASH_LIBS)

xjadeo_CFLAGS = -Wall -g -O3 @FFMPEG_CFLAGS@ $(SDL_CFLAGS) $(XV_CFLAGS) $(IMLIB_CFLAGS) $(MIDI_CFLAGS) @FREETYPE_CFLAGS@ $(IMLIB2_CFLAGS) $(LASH_CFLAGS)

#ctags:
#	find . -type f -name "*.[ch]*" | xargs @CTAGS@

MAINTAINERCLEANFILES = Makefile.in

