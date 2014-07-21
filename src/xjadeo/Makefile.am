#TODO: use a dedicated script for 'git desc' - this is "non-POSIX" way:
REV = $(shell test -d ../../.git && git describe --always --tags HEAD 2>/dev/null)

bin_PROGRAMS=xjadeo xjremote

xjadeo_SOURCES=main.c ../../aclocal.m4 ../../config.h \
	xjadeo.c xjadeo.h ffcompat.h \
	remote.c remote.h mqueue.c xjosc.c \
	configfile.c \
	jack.c ltc-jack.c \
	midi.c freetype.c smpte.c \
	display.c display.h \
	display_mac.c display_x11.c display_sdl.c \
	display_gl_common.h \
	weak_libjack.c weak_libjack.h \
	gtime.c gtime.h

xjadeo_LDADD = @MQ_LIBS@ @FFMPEG_LIBS@ @XV_LIBS@ @MIDI_LIBS@ @FREETYPE_LIBS@ @IMLIB2_LIBS@ @XPM_LIBS@ @LIBLO_LIBS@ @SDL_LIBS@ @LTC_LIBS@ @GL_LIBS@ -lm

xjadeo_CFLAGS = -Wall -g -O3 \
	@FFMPEG_CFLAGS@ @XV_CFLAGS@ @MIDI_CFLAGS@ @FREETYPE_CFLAGS@ @IMLIB2_CFLAGS@ @XPM_CFLAGS@ @LIBLO_CFLAGS@ @SDL_CFLAGS@ @LTC_CFLAGS@ @GL_CFLAGS@ @JACK_CFLAGS@ "-DSUBVERSION=\"$(REV)\""
xjadeo_OBJCFLAGS = -Wall -g -O3 \
	@FFMPEG_CFLAGS@ @XV_CFLAGS@ @MIDI_CFLAGS@ @FREETYPE_CFLAGS@ @IMLIB2_CFLAGS@ @XPM_CFLAGS@ @LIBLO_CFLAGS@ @SDL_CFLAGS@ @LTC_CFLAGS@ @GL_CFLAGS@ @JACK_CFLAGS@ "-DSUBVERSION=\"$(REV)\""

if TARGET_LINUX
xjadeo_SOURCES+=display_glx.c
xjadeo_LDADD+=@JACK_LIBS@
endif
if TARGET_WIN32
xjadeo_SOURCES+=display_gl_win.c
xjadeo_CFLAGS+=-DUSE_WEAK_JACK
xjadeo_LDADD+=-lws2_32 -lwinmm
endif
if TARGET_OSX
xjadeo_SOURCES+=display_gl_osx.m
xjadeo_CFLAGS+=-DUSE_WEAK_JACK
xjadeo_LDADD+=-ldl
endif

xjremote_SOURCES = xjremote.c

xjremote_LDADD = @MQ_LIBS@

MAINTAINERCLEANFILES = Makefile.in

CLEANFILES = paths.h
BUILT_SOURCES = paths.h

xjadeo_DEPENDENCIES= \
	icons/brightness.bitmap    icons/gamma.bitmap    icons/saturation.bitmap    icons/osd_bitmaps.h \
	icons/brightness_mask.xbm  icons/gamma_mask.xbm  icons/saturation_mask.xbm  icons/xjadeo-color.xpm \
	icons/contrast.bitmap      icons/hue.bitmap      icons/splash.bitmap        icons/xjadeo-color-ico.h \
	icons/contrast_mask.xbm    icons/hue_mask.xbm    icons/splash_mask.xbm      paths.h

paths.h: Makefile
	@echo '#define BINDIR "$(DESTDIR)/$(bindir)/"' >$@
	@echo '#define SYSCONFDIR "$(DESTDIR)/$(sysconfdir)/"' >>$@
