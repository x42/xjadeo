dnl Process this file with autoconf to produce a configure script.

AC_INIT(xjadeo.c)
AC_CONFIG_SRCDIR(src/xjadeo/xjadeo.c)

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

VERSION_MAJOR=0
VERSION_MINOR=7
VERSION_SUB=5

dnl Every other copy of the package version number gets its value from here
AM_INIT_AUTOMAKE(xjadeo, [${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_SUB}])
#AC_REVISION([$Id: configure.ac $])

dnl create a config.h file (Automake will add -DHAVE_CONFIG_H)
AM_CONFIG_HEADER(config.h)

AC_ARG_ENABLE(xv,    AC_HELP_STRING([--disable-xv],  [Disable Xvideo extension ]))
#AC_ARG_ENABLE(imlib, AC_HELP_STRING([--disable-imlib], [Disable use of imlib for display]))
AC_ARG_ENABLE(imlib2, AC_HELP_STRING([--disable-imlib2], [Disable use of imlib2 for display]))
AC_ARG_ENABLE(sdl, AC_HELP_STRING([--disable-sdl], [Disable use of SDL display]))
AC_ARG_ENABLE(ft,    AC_HELP_STRING([--disable-ft],  [Disable freetype On-Screen.Display support]))
AC_ARG_ENABLE(qtgui, AC_HELP_STRING([--enable-qtgui],[Build the Qt GUI controller]))
AC_ARG_ENABLE(contrib,  AC_HELP_STRING([--enable-contrib],  [Compile and install code in contrib folder]))
AC_ARG_ENABLE(lash, AC_HELP_STRING([--enable-lash],[enable LASH support (deprecated)]))
AC_ARG_ENABLE(timescale, AC_HELP_STRING([--disable-timescale],[disable time-mapping/loop-playback support]))
AC_ARG_ENABLE(framecrop, AC_HELP_STRING([--enable-framecrop],[hardcoded frame cropping - DO NOT USE]))
AC_ARG_ENABLE(tiff, AC_HELP_STRING([--disable-tiff],[disable libtiff support (affects only contrib/tsmm/)]))
AC_ARG_ENABLE(mq, AC_HELP_STRING([--disable-mq],[disable POSIX message-queue remote control]))
AC_ARG_ENABLE(jacksession, AC_HELP_STRING([--disable-jacksession],[disable JACK-Session support]))
AC_ARG_ENABLE(ipc, AC_HELP_STRING([--disable-ipc],[disable IPC message-queue remote control]))
AC_ARG_ENABLE(osc, AC_HELP_STRING([--disable-osc],[disable OSC remote support]))
AC_ARG_ENABLE(midi, AC_HELP_STRING([--disable-midi],[Do not build any of the MIDI drivers]))
AC_ARG_ENABLE(ltc, AC_HELP_STRING([--disable-ltc],[disable LTC sync support]))
AC_ARG_ENABLE(portmidi, AC_HELP_STRING([--disable-portmidi],[disable portmidi support]))
AC_ARG_ENABLE(alsamidi, AC_HELP_STRING([--disable-alsamidi],[disable alsamidi support]))

AC_ARG_WITH(fontfile, AS_HELP_STRING([--with-fontfile=file.ttf], [specify a path to a truetype font file to be used as default OSD font.]), fontfile="$withval", fontfile="")

AC_ARG_WITH(portmidisrc, AS_HELP_STRING([--with-portmidisrc], [statically link against portmidi]), portmidisrc="$withval", portmidisrc="")
AC_ARG_WITH(freetypesrc, AS_HELP_STRING([--with-freetypesrc], [statically link against freetype]), freetypesrc="$withval", freetypesrc="")
AC_ARG_WITH(liblosrc, AS_HELP_STRING([--with-liblosrc], [statically link against liblo]), liblosrc="$withval", liblosrc="")
AC_ARG_WITH(ffmpegprefix, AS_HELP_STRING([--with-ffmpegprefix], [override pkg-config - specify prefix to ffmpeg installation]), ffmpegprefix="$withval", ffmpegprefix="")
AC_ARG_WITH(ffmpegsrc, AS_HELP_STRING([--with-ffmpegsrc], [location of ffmpeg source, use to statically link against ffmpeg]), ffmpegsrc="$withval", ffmpegsrc="")
AC_ARG_WITH(ffmpegldadd, AS_HELP_STRING([--with-ffmpegldadd], [ld options for static ffmpeg eg. "-lmp3lame -ldts -lfaac -logg -lxvidcore"]), ffmpegldadd="$withval", ffmpegldadd="")

AC_ARG_WITH(qt4prefix, AS_HELP_STRING([--with-qt4prefix], [prefix for Qt4 installation, e.g. "/usr/lib/qt4"]), qt4prefix="$withval", qt4prefix="")

LIBPREF=lib
LIBSUF=.a

case $target_os in
  *darwin*)
		AC_MSG_NOTICE([TARGET: OSX])
		PM_OS=mac
		AC_DEFINE([HAVE_MACOSX], [], [use macintosh native display])
		HAVE_MACOSX=1
		LDFLAGS="$LDFLAGS -lz -lbz2 -framework Carbon -framework Quicktime"
dnl 2006/11/13 : 32 bit RGBA is known not to work on OSX ffmpeg+imlib2
dnl IMLIB2RGBA 
  	;;
  *linux*)
		AC_MSG_NOTICE([TARGET: LINUX])
		PM_OS="linux";
		AC_DEFINE([IMLIB2RGBA], [], [use native RGB32 ffmpeg -> imlib.])
	;;
  *mingw32*|*win*)
		AC_MSG_NOTICE([TARGET: WINDOWS])
		PM_OS=win
		AC_DEFINE([HAVE_WINDOWS], [], [windows SDL version])
		HAVE_WINDOWS=1
		IMLIB_LIBS="-lws2_32" # LDFLAGS are before object files. -pragmatically re-used IMLIB_LIBS
		LIBSUF=.dll
  	;;
  *)	
		AC_MSG_WARN([*** unknown target OS ***])
  ;;
esac

AC_SUBST(VERSION)
ISODATE=`date +%Y-%m-%d`
AC_SUBST(ISODATE)

dnl Checks for programs.
AC_PROG_INSTALL
#AC_PROG_CC
AM_PROG_CC_C_O

# TODO: check for xsltproc, xmllint and 
# automatically build documentation...

#AC_C_VOLATILE
AC_C_CONST
AC_C_BIGENDIAN

dnl Checks for header files.
AC_HEADER_STDC
#AC_HEADER_TIME
AC_CHECK_SIZEOF(float)
AC_CHECK_SIZEOF(double)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(unsigned long)
AC_CHECK_HEADERS(time.h string.h)

dnl Checks for libraries.

AH_TEMPLATE([HAVE_LIBXV], [Define as 1 if  you have the Xv library (-lXv)])
#AH_TEMPLATE([HAVE_IMLIB], [Define as 1 if you have imlib])
AH_TEMPLATE([HAVE_IMLIB2], [Define as 1 if you have imlib2])
AH_TEMPLATE([HAVE_MIDI], [Define as 1 if to enable MIDI in general])
AH_TEMPLATE([HAVE_PORTMIDI], [Define as 1 if to enable the portmidi driver])
AH_TEMPLATE([HAVE_JACKMIDI], [Define as 1 to enable JACK-MIDI])
AH_TEMPLATE([ALSA_SEQ_MIDI], [Define as 1 to enable ALSA sequencer midi])
AH_TEMPLATE([ALSA_RAW_MIDI], [Define as 1 to enable ALSA-raw midi driver])
AH_TEMPLATE([JACK_SESSION], [Define as 1 to enable JACK Session support])
AH_TEMPLATE([HAVE_LTCSMPTE], [Define as 1 if you have ltcsmpte.sf.net ]) 
AH_TEMPLATE([HAVE_LTC], [Define as 1 if you have libltc - http://github.com/x42/libltc ])
AH_TEMPLATE([HAVE_LASH], [Define as 1 if you have lash])
AH_TEMPLATE([HAVE_XPM], [Define as 1 if you have libXpm])
AH_TEMPLATE([HAVE_SDL], [Define as 1 if  you have the SDL toolkit])
AH_TEMPLATE([HAVE_MQ], [Define as 1 if you have POSIX message queues (libc, librt)])
AH_TEMPLATE([HAVE_IPCMSG], [Define as 1 if you have IPC  message queues (system-V)])
AH_TEMPLATE([HAVE_LIBLO], [Define as 1 if you have the loblo OSC library])
AH_TEMPLATE([HAVE_TIFF], [Define as 1 if you have libtiff (contrib/tsmm)])
AH_TEMPLATE([TTFFONTFILE], [absolute path to truetype default OSD font file])
AH_TEMPLATE([CFG_WARN_ONLY], [errors in the resource config file only result in a warning message instead of forcing xjadeo to exit.])
AH_TEMPLATE([DND], [enable xjadeo x11 drag and drop])
AH_TEMPLATE([COLOREQ], [enable XV and imlib2 color equalizer])
AH_TEMPLATE([TIMEMAP], [enable dynamic time scaling])
AH_TEMPLATE([CROPIMG], [enable frame cropping])
AH_TEMPLATE([DRAW_CROSS], [paint a X on screen when unable to decode or display a frame.])
AH_TEMPLATE([HAVE_JACK_VIDEO], [experimental code to use jack_video, jack-svn >= r591])
AH_TEMPLATE([NEW_JACK_LATENCY_API], [use new JACK latency API])
AH_TEMPLATE([HAVE_SWSCALE], [Define as 1 if you have ffmpeg with swscale support])
AH_TEMPLATE([OLD_FFMPEG], [Define as 1 if you older versions of ffmpeg that provide all include headers in one folder. ])

AC_DEFINE(CFG_WARN_ONLY)
AC_DEFINE(DND)
AC_DEFINE(DRAW_CROSS)
AC_DEFINE(COLOREQ)

AC_HEADER_MAJOR
AC_FUNC_ALLOCA
AC_STRUCT_TM
#AC_STRUCT_ST_BLOCKS
#AC_FUNC_CLOSEDIR_VOID
#AC_CHECK_FUNCS(mkfifo)
#AC_CHECK_FUNC(mknod)

if test "x$enable_timescale" != "xno"; then
  AC_DEFINE(TIMEMAP)
fi

if test "x$enable_framecrop" == "xyes"; then
  AC_DEFINE(CROPIMG)
fi

# FIXME - needed fi. on osx
LIBS="$LIBS -L/usr/X11R6/lib"
#X_LIBS="-L/usr/X11R6/lib/"

PKG_CHECK_MODULES(JACK, jack, , [AC_MSG_ERROR(*** You need the JACK audio development kit (jackaudio.org). ***)])
AC_SUBST(JACK_CFLAGS)
AC_SUBST(JACK_LIBS)
if test "x$enable_jacksession" != "xno"; then
  AC_CHECK_HEADERS([jack/session.h] , AC_DEFINE(JACK_SESSION) JACK_SESSION=1)
fi

AC_SEARCH_LIBS(jack_port_get_latency_range, jack, AC_DEFINE(NEW_JACK_LATENCY_API) NEW_JACK_LATENCY_API=1 )

MQ_LIBS=""
if test "x$enable_mq" != "xno"; then
	AC_SEARCH_LIBS( mq_open, rt, AC_DEFINE(HAVE_MQ) MQ_LIBS="-lrt -lpthread" HAVE_MQ=1 )
fi
AC_SUBST(MQ_LIBS)
AC_DEFINE(MQLEN, 512, [ max length of a remote control line. ])

dnl prefer POSIX RT mqueue over IPC..
if test "x$enable_ipc" != "xno"; then
	if test -z "$HAVE_MQ"; then
		AC_CHECK_HEADERS([sys/ipc.h sys/msg.h] , AC_DEFINE(HAVE_IPCMSG) HAVE_IPCMSG=1 )
	fi
fi

if test "x$enable_xv" != "xno"; then
dnl for this to work properly X11/Xlib.h needs to be included for the check.
dnl '-' as INCLUDE path simply checks if the header file is present
dnl FIXME; add a proper INCLUDE path to AC_CHECK_HEADERS
dnl	AC_CHECK_HEADERS([X11/extensions/Xvlib.h X11/extensions/XShm.h], , [AC_MSG_ERROR(*** X11: You need to install the X11 development package. ***)], -)
	AC_CHECK_LIB(Xv, XvQueryAdaptors, [AC_DEFINE(HAVE_LIBXV) AC_SUBST(XV_LIBS) AC_SUBST(XV_CFLAGS) XV_LIBS="-lXv" HAVE_LIBXV=1 ], [AC_MSG_ERROR([Could not find XVideo Lib (libXv)])], $X_LIBS)
fi

dnl remove this if IFF you want to build an X11 version for OSX
AS_IF([test -z "$HAVE_MACOSX$HAVE_WINDOWS"], [
	AC_CHECK_HEADERS([X11/Xlib.h], , [AC_MSG_ERROR(*** X11: You need to install the X11 development package. ***)])
	AC_CHECK_LIB([X11], [XOpenDisplay], [XV_LIBS="$XV_LIBS -lXext"], AC_MSG_ERROR([*** X11: You need to install the X11 development package: libx11-dev ***]))
	PKG_CHECK_MODULES(Xpm, xpm, [ AC_DEFINE(HAVE_XPM) HAVE_XPM=1 ], [AC_MSG_ERROR(*** X11: You need to install the libxpm development package. ***)])
	#AC_SEARCH_LIBS( XpmCreatePixmapFromData, Xpm, AC_DEFINE(HAVE_XPM) XPM_LIBS="-lXpm" HAVE_XPM=1 )
])

AS_IF([test "x$enable_osc" != "xno"], [
	if test -n "$liblosrc"; then
		AC_CHECK_FILES([$liblosrc/src/.libs/liblo.a $liblosrc/lo/lo.h],[AC_DEFINE(HAVE_LIBLO) HAVE_LIBLO=1 LIBLO_CFLAGS="-I$liblosrc/lo" LIBLO_LIBS="$liblosrc/src/.libs/liblo.a" ])
	else
		PKG_CHECK_MODULES(LIBLO, liblo, [ AC_DEFINE(HAVE_LIBLO) HAVE_LIBLO=1 ], [ NULL=0] )
	fi
])

AC_SUBST(LIBLO_CFLAGS)
AC_SUBST(LIBLO_LIBS)

AS_IF([test "x$enable_sdl" != "xno"], [
	PKG_CHECK_MODULES(SDL, sdl, [ AC_DEFINE(HAVE_SDL) HAVE_SDL=1 ], [ NULL=1])
])
AC_SUBST(SDL_LIBS)
AC_SUBST(SDL_CFLAGS) 


dnl ----------------------------------------------
dnl midi lib detection
dnl ----------------------------------------------
if test "x$enable_midi" != "xno"; then
	MIDI_CFLAGS=""
	MIDI_CFLAGS=""
	if test "x$enable_portmidi" != "xno" -a -z "$portmidisrc" ; then
		AC_CHECK_HEADERS(portmidi.h porttime.h , [FOUND_H_PORTMIDI=yes])
		AC_CHECK_LIB([portmidi], [Pm_CountDevices], [FOUND_L_PORTMIDI=yes])
	fi
	if test "x$FOUND_H_PORTMIDI$FOUND_L_PORTMIDI" = "xyesyes"; then
		MIDI_LIBS="-lportmidi"
		if test "$PM_OS" != "mac"; then
		  MIDI_LIBS="${MIDI_LIBS} -lporttime"
	  fi
	fi

	if test "x$enable_portmidi" != "xno" -a -n "$portmidisrc" -a -n "$PM_OS"; then
		AC_CHECK_FILES([$portmidisrc/porttime/porttime.h $portmidisrc/pm_common/portmidi.h],[FOUND_H_PORTMIDI=yes])
dnl ppc-portmidi : cc -dynamiclib pm_mac/*.o porttime/*.o -flat_namespace -undefined suppress -o portmidi.dylib

		AC_CHECK_FILES([$portmidisrc/porttime/${LIBPREF}porttime$LIBSUF $portmidisrc/pm_${PM_OS}/${LIBPREF}portmidi$LIBSUF],[FOUND_L_PORTMIDI=yes MIDI_LIBS="$portmidisrc/porttime/${LIBPREF}porttime$LIBSUF $portmidisrc/pm_${PM_OS}/${LIBPREF}portmidi$LIBSUF"])
		AC_CHECK_FILES([$portmidisrc/pm_${PM_OS}/build/${LIBPREF}portmidi$LIBSUF],[FOUND_L_PORTMIDI=yes MIDI_LIBS="$portmidisrc/pm_${PM_OS}/build/${LIBPREF}portmidi$LIBSUF"])
		if test $PM_OS = "mac" ; then MIDI_LIBS="$MIDI_LIBS -framework CoreMIDI -framework CoreFoundation -framework CoreAudio"; fi

		MIDI_CFLAGS="-I$portmidisrc/porttime/ -I$portmidisrc/pm_common/"
	fi

	if test "x$JACKMIDI" != "xno"; then
		AC_CHECK_HEADERS(jack/midiport.h, [FOUND_H_JACKMIDI=yes])
	fi

	if test "x$enable_alsamidi" != "xno"; then
		AC_CHECK_HEADERS(alsa/asoundlib.h, [FOUND_H_ALSAMIDI=yes])
		AC_CHECK_HEADERS(pthread.h, [FOUND_H_PTHREAD=yes])
		AC_CHECK_LIB([asound], [snd_rawmidi_open], [FOUND_L_ALSAMIDI=yes])
	fi

	if test "x$FOUND_H_ALSAMIDI$FOUND_L_ALSAMIDI$FOUND_H_PTHREAD" = "xyesyesyes"; then
		AC_DEFINE(ALSA_SEQ_MIDI) 
		AC_DEFINE(ALSA_RAW_MIDI) 
		AC_DEFINE(HAVE_MIDI) 
		HAVE_MIDI=1
		ALSA_SEQ_MIDI=1
		ALSA_RAW_MIDI=1
		MIDI_LIBS="$MIDI_LIBS -lasound -lpthread"
	fi
	if test "x$FOUND_H_PORTMIDI$FOUND_L_PORTMIDI" = "xyesyes"; then
		AC_DEFINE(HAVE_MIDI) 
		AC_DEFINE(HAVE_PORTMIDI) 
		HAVE_MIDI=1
		HAVE_PORTMIDI=1
		dnl LIBS have been set above
	fi
	if test "x$FOUND_H_JACKMIDI" = "xyes"; then
		AC_DEFINE(HAVE_MIDI) 
		AC_DEFINE(HAVE_JACKMIDI) 
		HAVE_MIDI=1
		HAVE_JACKMIDI=1
		dnl no extra libs are needed (jack is already required)
	fi
fi

AC_SUBST(MIDI_LIBS)
AC_SUBST(MIDI_CFLAGS)

dnl ----------------------------------------------
dnl Check for ffmpeg: avformat/avcodec 
dnl ----------------------------------------------

AS_IF([test -z "$ffmpegprefix" -a -z "$ffmpegsrc"], [
	PKG_CHECK_MODULES(FFMPEG, libavformat libavcodec libavutil, HAVE_FFMPEG=yes, 
		AC_MSG_WARN([
		Could not find ffmpeg package. please set PKG_CONFIG_PATH to 
		where FFMPEG installed its pkg files.
		])) 
])


if test -n "$ffmpegsrc" ; then
	AC_CHECK_FILES([$ffmpegsrc/libavcodec/${LIBPREF}avcodec$LIBSUF $ffmpegsrc/libavformat/${LIBPREF}avformat$LIBSUF],[
		FFMPEG_LIBS="$ffmpegsrc/libavformat/${LIBPREF}avformat$LIBSUF $ffmpegsrc/libavcodec/${LIBPREF}avcodec$LIBSUF $ffmpegsrc/libavutil/${LIBPREF}avutil$LIBSUF $ffmpegsrc/libavdevice/${LIBPREF}avdevice$LIBSUF -lz $ffmpegldadd" 
		HAVE_FFMPEG=yes
		FFMPEG_INCDIR="$ffmpegsrc"
		FFMPEG_CFLAGS="-I$FFMPEG_INCDIR -I$FFMPEG_INCDIR/libavcodec -I$FFMPEG_INCDIR/libavformat -I$FFMPEG_INCDIR/libavutil"
		AC_MSG_NOTICE([located a ffmpeg installation without pkg-information.])
	])

elif test -n "$ffmpegprefix" ; then
	FFMPEG_LIBS="-L$ffmpegprefix/lib -lavcodec -lavutil -lavformat"
	FFMPEG_INCDIR="$ffmpegprefix/include"
	FFMPEG_CFLAGS="-I$FFMPEG_INCDIR" #default
	FFMPEG_CFLAGS="$FFMPEG_CFLAGS -I$FFMPEG_INCDIR/libavcodec -I$FFMPEG_INCDIR/libavformat -I$FFMPEG_INCDIR/libavutil" # new
	FFMPEG_CFLAGS="$FFMPEG_CFLAGS -I$FFMPEG_INCDIR/ffmpeg/" # old
	FFMPEG_CFLAGS="$FFMPEG_CFLAGS -I$FFMPEG_INCDIR/ffmpeg/libavcodec -I$FFMPEG_INCDIR/ffmpeg/libavformat -I$FFMPEG_INCDIR/ffmpeg/libavutil" # even older
	HAVE_FFMPEG=yes
	AC_MSG_NOTICE([located a ffmpeg installation without pkg-information.])
fi

if test "x$HAVE_FFMPEG" != "xyes"; then
		AC_MSG_ERROR([
	libavcodec, libavformat and libavutil from ffmpeg are required.
	If you are sure that ffmpeg is installed, point
	configure to your local copy with f.i.
	./configure --with-ffmpegprefix=/usr/local/
        or
	./configure --with-ffmpegsrc=/usr/local/src/ffmpeg/
	])
fi


ac_save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $FFMPEG_CFLAGS"
dnl backwards compat
AC_CHECK_HEADERS([ffmpeg/avcodec.h], [AC_DEFINE(OLD_FFMPEG) OLD_FFMPEG=1;])

dnl check for libswscale (required for ffmpeg > 0.49)
if test -n "$ffmpegsrc"; then 
	AC_CHECK_HEADERS($ffmpegsrc/libswscale/swscale.h , [AC_DEFINE(HAVE_SWSCALE) HAVE_SWSCALE=1; FFMPEG_LIBS="$FFMPEG_LIBS $ffmpegsrc/libswscale/${LIBPREF}swscale$LIBSUF"; FFMPEG_CFLAGS="$FFMPEG_CFLAGS -I$FFMPEG_INCDIR/libswscale"])
elif test -n "$ffmpegprefix"; then 
	AC_CHECK_HEADERS($ffmpegprefix/include/libswscale/swscale.h , [AC_DEFINE(HAVE_SWSCALE) HAVE_SWSCALE=1; FFMPEG_LIBS="$FFMPEG_LIBS -lswscale"; FFMPEG_CFLAGS="$FFMPEG_CFLAGS -I$ffmpegprefix/include/libswscale/"]) # new
	AC_CHECK_HEADERS($ffmpegprefix/include/ffmpeg/libswscale/swscale.h , [AC_DEFINE(HAVE_SWSCALE) HAVE_SWSCALE=1; FFMPEG_LIBS="$FFMPEG_LIBS -lswscale"; FFMPEG_CFLAGS="$FFMPEG_CFLAGS -I$ffmpegprefix/include/ffmpeg/"]) # old
	AC_CHECK_HEADERS($ffmpegprefix/include/ffmpeg/swscale.h , [AC_DEFINE(HAVE_SWSCALE) HAVE_SWSCALE=1; FFMPEG_LIBS="$FFMPEG_LIBS -lswscale"; FFMPEG_CFLAGS="$FFMPEG_CFLAGS -I$ffmpegprefix/include/ffmpeg/libswscale/"]) # older
elif test -n "$OLD_FFMPEG"; then 
	AC_CHECK_HEADERS(swscale.h , [AC_DEFINE(HAVE_SWSCALE) HAVE_SWSCALE=1; FFMPEG_LIBS="$FFMPEG_LIBS -lswscale"])
else 
	PKG_CHECK_MODULES(SWSCALE, libswscale, [AC_DEFINE(HAVE_SWSCALE) HAVE_SWSCALE=1; FFMPEG_LIBS="$FFMPEG_LIBS $SWSCALE_LIBS"; FFMPEG_CFLAGS="$FFMPEG_CFLAGS $SWSCALE_CFLAGS"])
	if test -z "$HAVE_SWSCALE";then
		AC_CHECK_HEADERS(libswscale/swscale.h , [AC_DEFINE(HAVE_SWSCALE) HAVE_SWSCALE=1; FFMPEG_LIBS="$FFMPEG_LIBS -lswscale";])
	fi
fi
CFLAGS="$ac_save_CFLAGS"

dnl phew, done with ffmpeg.
AC_SUBST(FFMPEG_CFLAGS)
AC_SUBST(FFMPEG_LIBS)

dnl ----------------------------------------------
dnl Check for freetype 
dnl ----------------------------------------------

FREETYPE_LIBS=
FREETYPE_CFLAGS=

FOUND_H_FREETYPE=
FOUND_L_FREETYPE=

if test -n "$freetypesrc" ; then
	AC_CHECK_FILES([$freetypesrc/include/ft2build.h],[FOUND_H_FREETYPE=yes])
	AC_CHECK_FILES([$freetypesrc/objs/.libs/${LIBPREF}freetype$LIBSUF],[FOUND_L_FREETYPE=yes])
fi

if test "x$FOUND_H_FREETYPE$FOUND_L_FREETYPE" = "xyesyes" ; then
	FREETYPE_CFLAGS="-I$freetypesrc/include/"
	FREETYPE_LIBS="$freetypesrc/objs/.libs/${LIBPREF}freetype$LIBSUF"
else 
	PKG_CHECK_MODULES(FREETYPE, freetype2, [freetype_pkgconfig=yes], [freetype_pkgconfig=no])

	if test "x$freetype_pkgconfig" = "xyes"; then
		if test ! "x$enable_ft" = "xno"; then
		  AC_DEFINE(HAVE_FREETYPE_H, 1, [Have FreeType2 include files])
		  AC_DEFINE(HAVE_FT, 1, [Use freetype for OSD])
		  HAVE_FT=1
		else
			FREETYPE_LIBS=
			FREETYPE_CFLAGS=
		fi
	else
		AC_PATH_PROG(FREETYPE_CONFIG, freetype-config, no)
		if test "x$FREETYPE_CONFIG" != "xno" -a ! "x$enable_ft" = "xno"; then
			FREETYPE_CFLAGS=`$FREETYPE_CONFIG --cflags`
			FREETYPE_LIBS=`$FREETYPE_CONFIG --libs`
			AC_DEFINE(HAVE_FREETYPE_H, 1, [Have FreeType2 include files])
			AC_DEFINE(HAVE_FT, 1, [Use freetype for OSD])
			HAVE_FT=1
		fi
	fi
fi

#if test -z "$fontfile" ; then
	# TODO try some arch-typical font files here ?
	# PPC: "/usr/X11R6/lib/X11/fonts/TTF/luxisb.ttf"
	# Debian: "/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf"
#fi

if test -n "$fontfile" ; then
	# I'm not sure if this is the proper way to escape a #define string.
	# AC_DEFINE(TTFFONTFILE, "\"$fontfile\"") - no
	# FREETYPE_CFLAGS="$FREETYPE_CFLAGS -DTTFFONTFILE=\\\"$fontfile\\\"" - maybe
	dnl hack alert - we 'should' use our own .h file for this
	dnl this might not work with some autoconf's
	echo "#define TTFFONTFILE \"$fontfile\"" >> confdefs.h
fi

AC_SUBST(FREETYPE_CFLAGS)
AC_SUBST(FREETYPE_LIBS)

dnl ----------------------------------------------
dnl PKG checks - video display libraries
dnl ----------------------------------------------

#if test "x${enable_imlib}" != "xno"; then
#	PKG_CHECK_MODULES(IMLIB,imlib,  [ AC_DEFINE(HAVE_IMLIB) HAVE_IMLIB=1 ], [ NULL=0 ])
#fi
AC_SUBST(IMLIB_CFLAGS)
AC_SUBST(IMLIB_LIBS)

AS_IF([test "x${enable_imlib2}" != "xno"], [
	PKG_CHECK_MODULES(IMLIB2, imlib2 >= 1.3.0,  [ AC_DEFINE(HAVE_IMLIB2) HAVE_IMLIB2=1 ], [ NULL=0 ])
])
AC_SUBST(IMLIB2_CFLAGS)
AC_SUBST(IMLIB2_LIBS)

dnl ----------------------------------------------
dnl http://lash.nongnu.org
dnl ----------------------------------------------
AS_IF([test "x$enable_lash" == "xyes"], [
  PKG_CHECK_MODULES(LASH, lash-1.0 >= 0.4.0, LASH_FOUND="yes", [ NULL=0 ])
  
  if test "x$LASH_FOUND" = "xyes"; then
    AC_DEFINE(HAVE_LASH)
    HAVE_LASH=1;
    #LASH_VERSION=$( pkg-config --modversion lash-1.0 )
    #AC_DEFINE_UNQUOTED(LASH_VERSION, "$LASH_VERSION", [The version of lash we're compiling against])
  fi
])
AC_SUBST(LASH_CFLAGS)
AC_SUBST(LASH_LIBS)

dnl ----------------------------------------------
dnl https://github.com/x42/libltc
dnl ----------------------------------------------
AS_IF([ test "x$enable_ltc" != "xno"], [
	PKG_CHECK_MODULES(LTC, ltc, [ AC_DEFINE(HAVE_LTC) HAVE_LTC=1 ], [ NULL=0] )
])
AC_SUBST(LTC_CFLAGS)
AC_SUBST(LTC_LIBS)


dnl ----------------------------------------------
dnl http://ltcsmpte.sf.net -- deprecated
dnl ----------------------------------------------
AS_IF([ test "x$enable_ltc" != "xno" -a -z "$HAVE_LTC" ], [
	PKG_CHECK_MODULES(LTCSMPTE, ltcsmpte, [ AC_DEFINE(HAVE_LTCSMPTE) HAVE_LTCSMPTE=1 ], [ NULL=0] )
])
AC_SUBST(LTCSMPTE_CFLAGS)
AC_SUBST(LTCSMPTE_LIBS)

dnl ----------------------------------------------
dnl contrib/tsmm
dnl ----------------------------------------------

AS_IF([test "x${enable_contrib}" != "xno"], [
  AC_CHECK_HEADERS(tiffio.h, [FOUND_H_TIFF=yes])
  AC_CHECK_LIB([tiff], [TIFFOpen], [FOUND_L_TIFF=yes])

  if test "x$FOUND_H_TIFF$FOUND_L_TIFF" = "xyesyes"; then
    AC_DEFINE(HAVE_TIFF) HAVE_TIFF=1 
    TIFF_LIBS="-ltiff"	
    TIFF_CFLAGS=""	
  else
    PKG_CHECK_MODULES(TIFF,tiff,  [ AC_DEFINE(HAVE_TIFF) HAVE_TIFF=1 ], [ NULL=0 ])
  fi
])
AC_SUBST(TIFF_CFLAGS)
AC_SUBST(TIFF_LIBS)

dnl ----------------------------------------------
dnl Qt GUI stuff
dnl ----------------------------------------------

if test "$enable_qtgui" = "yes"; then

 if test "x$qt4prefix" = "x"; then
   qt4prefix=$($PKG_CONFIG --variable=prefix QtGui)
 fi
 
 # Check for Qt qmake utility.
 AC_PATH_PROG(ac_qmake, qmake, [no], $qt4prefix/bin)
 AC_PATH_PROG(ac_lrelease, lrelease, [no], $qt4prefix/bin)
 if test "x$ac_qmake" = "xno" -o "x$ac_lrelease" = "xno" ; then
    AC_MSG_ERROR([qmake not found: please install the Qt4 development
    package of your distribution. Try setting the --with-qt4config parameter; or use --disable-qtgui to compile xjadeo.])
 fi
 
 HAVE_QT4=1
 
 if test -n "$HAVE_WINDOWS"; then
   ac_qmake_args="-spec win32-x-g++ -config release"
 fi
 AC_SUBST(ac_qmake)
 AC_SUBST(ac_qmake_args)
 AC_SUBST(ac_lrelease)

 ac_qtgui_dir="qt-gui"
 XJINFO_BIN="xjinfo"

 AC_CONFIG_FILES(src/qt-gui/Makefile src/qt-gui/qjadeo.pro)

else
 ac_qtgui_dir=""
 XJINFO_BIN=""

fi

AC_SUBST(ac_qtgui_dir)
AC_SUBST(XJINFO_BIN)

dnl ---------- End of Qt GUI stuff ---------------


ac_contrib_dir=""

if test "x$enable_contrib" = "xyes"; then
  ac_contrib_dir="contrib/cli-remote/"
  if test -n "$HAVE_TIFF"; then 
    ac_contrib_dir="$ac_contrib_dir contrib/tsmm"
  fi 
fi

AC_SUBST(ac_contrib_dir)

if test "x$enable_contrib" = "xyes"; then
  AC_CONFIG_FILES([contrib/cli-remote/Makefile])
  if test -n "$HAVE_TIFF"; then 
    AC_CONFIG_FILES([contrib/tsmm/Makefile])
  fi
fi

AC_CONFIG_FILES([Makefile src/Makefile src/xjadeo/Makefile xjadeo.lsm])

AC_OUTPUT

MIDIREPORT=""
if test -n "$HAVE_PORTMIDI"; then MIDIREPORT="${MIDIREPORT}portmidi "; fi
if test -n "$HAVE_JACKMIDI"; then MIDIREPORT="${MIDIREPORT}jack-midi "; fi
if test -n "$ALSA_SEQ_MIDI"; then MIDIREPORT="${MIDIREPORT}alsa-seq "; fi
if test -n "$ALSA_RAW_MIDI"; then MIDIREPORT="${MIDIREPORT}alsa-RAW "; fi
if test -z "$HAVE_MIDI"; then     MIDIREPORT="${MIDIREPORT}disabled."; fi

if test -n "$HAVE_MQ"; then 
	RCTLREPORT="POSIX-MQ"; 
elif test -n "$HAVE_IPCMSG"; then 
	RCTLREPORT="IPC-MSG"; 
else 
	RCTLREPORT="not avail."; 
fi

if test -z "$HAVE_FT"; then 
	OSDREPORT="disabled"
else
	OSDREPORT="
   - freetype:       yes
   - default font:   "
  if test -n "$fontfile" ; then 
		OSDREPORT="${OSDREPORT}$fontfile";
	else
		OSDREPORT="${OSDREPORT}-"
	fi
fi

if test -n "$HAVE_SWSCALE"; then 
	FFSWREPORT="swscale"; 
else 
	FFSWREPORT="img_convert"; 
fi

CONTREPORT="no"
if test "x$enable_contrib" = "xyes"; then
	CONTREPORT="yes
 libtiff (xjtsmm):   ";
	if test -n "$HAVE_TIFF"; then 
		CONTREPORT="${CONTREPORT}yes";
	else 
		CONTREPORT="${CONTREPORT}not avail.";
	fi
fi

if test -n "$HAVE_LIBXV"; then RPT_LIBXV="yes"; else RPT_LIBXV="not avail."; fi
if test -n "$HAVE_IMLIB2"; then RPT_IMLIB2="yes"; else RPT_IMLIB2="not avail."; fi
if test -n "$HAVE_MACOSX"; then RPT_MACOSX="yes"; else RPT_MACOSX="not avail."; fi
if test -n "$HAVE_SDL"; then RPT_SDL="yes"; else RPT_SDL="not avail."; fi
if test -n "$HAVE_LASH"; then RPT_LASH="yes"; else RPT_LASH="not avail."; fi
if test -n "$HAVE_LIBLO"; then RPT_LIBLO="yes"; else RPT_LIBLO="not avail."; fi
if test -n "$HAVE_QT4"; then RPT_QT4="yes"; else RPT_QT4="no"; fi
if test -n "$HAVE_MACOSX"; then RPT_QT4="${RPT_QT4} (OSX-built-in)"; fi
if test -n "$JACK_SESSION"; then RPT_JACKSESSION="yes"; else RPT_JACKSESSION="not avail."; fi

if test -n "$HAVE_LTC"; then RPT_LTC="yes";
elif test -n "$HAVE_LTCSMPTE"; then RPT_LTC="yes, libltcsmpte -- deprecated, use https://github.com/x42/libltc";
else RPT_LTC="not avail."; fi

AC_MSG_NOTICE([

------------------------------------------------------------------------
  $PACKAGE $VERSION  - Configuration Report
------------------------------------------------------------------------

 prefix:             $prefix
 exec-prefix:        $exec_prefix
 install path:       $bindir
 System wide config: $sysconfdir/xjadeorc

 Video Displays
   - libxv:          $RPT_LIBXV
   - libx11/imlib2:  $RPT_IMLIB2
   - mac/osx:        $RPT_MACOSX
   - SDL:            $RPT_SDL

 Sync:
   - JACK-transport: yes
   - LTC:            $RPT_LTC
   - MIDI:           $MIDIREPORT

 JACK-session:       $RPT_JACKSESSION
 ffmpeg-scaling:     $FFSWREPORT
 remote control:     $RCTLREPORT
 OSC remote control: $RPT_LIBLO

 LASH:               $RPT_LASH
 On-screen-display:  $OSDREPORT

 GUI (qjadeo):       $RPT_QT4
 build contrib/*:    $CONTREPORT

])

if test "x$enable_contrib" = "xyes"; then
AC_MSG_NOTICE([
 Note for Packagers: --enable-contrib does not make a difference when packaging
 all Programs in contrib are noinst targets. They are intended for advanced
 users, developers or testing only.
])
fi

AC_MSG_NOTICE([
 type "make" followed my "make install" as root.
])
