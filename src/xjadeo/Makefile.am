bin_PROGRAMS=xjadeo

xjadeo_SOURCES=xjadeo.c ../../aclocal.m4 ../../config.h getopt.c getopt1.c getopt.h display.c jack.c midi.c freetype.c xjadeo.h display_sdl.c display.h display_x11.c display_gtk.c smpte.c main.c remote.c configfile.c

xjadeo_LDADD = -ljack @FFMPEG_LIBS@ $(GTK_LIBS) $(SDL_LIBS) $(XV_LIBS) $(IMLIB_LIBS) $(MIDI_LIBS) @FREETYPE_LIBS@ $(GDK_PIXBUF_LIBS)

xjadeo_CFLAGS = -Wall -g -O3 @FFMPEG_CFLAGS@ $(GTK_CFLAGS) $(SDL_CFLAGS) $(XV_CFLAGS) $(IMLIB_CFLAGS) @FREETYPE_CFLAGS@ $(GDK_PIXBUF_CFLAGS)

#ctags:
#	find . -type f -name "*.[ch]*" | xargs @CTAGS@

MAINTAINERCLEANFILES = Makefile.in

