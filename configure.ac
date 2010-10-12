dnl Process this file with autoconf to produce a configure script.

AC_INIT(xjadeo.c)
AC_CONFIG_SRCDIR(src/xjadeo/xjadeo.c)

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

VERSION_MAJOR=0
VERSION_MINOR=6
VERSION_SUB=0-rc1

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
AC_ARG_ENABLE(qtgui, AC_HELP_STRING([--disable-qtgui],[Do not build the Qt GUI controller]))
AC_ARG_ENABLE(contrib,  AC_HELP_STRING([--enable-contrib],  [Compile and install code in contrib folder]))
AC_ARG_ENABLE(lash, AC_HELP_STRING([--disable-lash],[disable LASH support]))
AC_ARG_ENABLE(timescale, AC_HELP_STRING([--disable-timescale],[disable time-mapping/loop-playback support]))
AC_ARG_ENABLE(framecrop, AC_HELP_STRING([--enable-framecrop],[hardcoded frame cropping - DO NOT USE]))
AC_ARG_ENABLE(tiff, AC_HELP_STRING([--disable-tiff],[disable libtiff support (affects only contrib/tsmm/)]))
AC_ARG_ENABLE(mq, AC_HELP_STRING([--disable-mq],[disable POSIX message-queue remote control]))
AC_ARG_ENABLE(ipc, AC_HELP_STRING([--disable-ipc],[disable IPC message-queue remote control]))
AC_ARG_ENABLE(osc, AC_HELP_STRING([--disable-osc],[disable OSC remote support]))
AC_ARG_ENABLE(midi, AC_HELP_STRING([--disable-midi],[Do not build any of the MIDI drivers]))
AC_ARG_ENABLE(ltc, AC_HELP_STRING([--disable-ltc],[disable LTC sync support]))

AC_ARG_WITH(fontfile, AS_HELP_STRING([--with-fontfile=file.ttf], [specify a path to a truetype font file to be used as default OSD font.]), fontfile="$withval", fontfile="")

AC_ARG_WITH(portmidisrc, AS_HELP_STRING([--with-portmidisrc], [statically link against portmidi]), portmidisrc="$withval", portmidisrc="")
AC_ARG_WITH(freetypesrc, AS_HELP_STRING([--with-freetypesrc], [statically link against freetype]), freetypesrc="$withval", freetypesrc="")
AC_ARG_WITH(liblosrc, AS_HELP_STRING([--with-liblosrc], [statically link against liblo]), liblosrc="$withval", liblosrc="")
AC_ARG_WITH(ffmpegprefix, AS_HELP_STRING([--with-ffmpegprefix], [override pkg-config - specify prefix to ffmpeg installation]), ffmpegprefix="$withval", ffmpegprefix="")
AC_ARG_WITH(ffmpegsrc, AS_HELP_STRING([--with-ffmpegsrc], [location of ffmpeg source, use to statically link against ffmpeg]), ffmpegsrc="$withval", ffmpegsrc="")
AC_ARG_WITH(ffmpegldadd, AS_HELP_STRING([--with-ffmpegldadd], [ld options for static ffmpeg eg. "-lmp3lame -ldts -lfaac -logg -lxvidcore"]), ffmpegldadd="$withval", ffmpegldadd="")

LIBPREF=lib
LIBSUF=.a

case $target_os in
  *darwin*)
		echo "TARGET: OSX"
		PM_OS=mac
		AC_DEFINE([HAVE_MACOSX], [], [use macintosh native display])
		HAVE_MACOSX=1
		LDFLAGS="$LDFLAGS -lz -lbz2 -framework Carbon -framework Quicktime"
dnl 2006/11/13 : 32 bit RGBA is known not to work on OSX ffmpeg+imlib2
dnl IMLIB2RGBA 
  	;;
  *linux*)
		echo "TARGET: LINUX"
		PM_OS="linux";
		AC_DEFINE([IMLIB2RGBA], [], [use native RGB32 ffmpeg -> imlib.])
	;;
  *mingw32*|*win*)
		echo "TARGET: WINDOWS"
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
AH_TEMPLATE([HAVE_LTCSMPTE], [Define as 1 if you have ltcsmpte.sf.net ]) 
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

MQ_LIBS=""
if test "x$enable_mq" != "xno"; then
	AC_SEARCH_LIBS( mq_open, rt, AC_DEFINE(HAVE_MQ) MQ_LIBS="-lrt" HAVE_MQ=1 )
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
	PKG_CHECK_MODULES(Xpm, xpm, [ AC_DEFINE(HAVE_XPM) HAVE_XPM=1 ], [ NULL=0] )
	#AC_SEARCH_LIBS( XpmCreatePixmapFromData, Xpm, AC_DEFINE(HAVE_XPM) XPM_LIBS="-lXpm" HAVE_XPM=1 )

])

AS_IF([test "x$enable_osc" != "xno"], [
	if test -n "$liblosrc"; then
		AC_CHECK_FILES([$liblosrc/src/.libs/liblo.a $liblosrc/lo/lo.h],[AC_DEFINE(HAVE_LIBLO) HAVE_LIBLO=1 LIBLO_CFLAGS="-I$liblosrc/lo" LIBLO_LIBS="$liblosrc/src/.libs/liblo.a" ])
	else
		PKG_CHECK_MODULES(LIBLO, liblo, [ AC_DEFINE(HAVE_LIBLO) HAVE_LIBLO=1 ])
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
	if test "x$PORTMIDI" != "xno" -a -z "$portmidisrc" ; then
		AC_CHECK_HEADERS(portmidi.h porttime.h , [FOUND_H_PORTMIDI=yes])
		AC_CHECK_LIB([portmidi], [Pm_CountDevices], [FOUND_L_PORTMIDI=yes])
	fi
	if test "x$FOUND_H_PORTMIDI$FOUND_L_PORTMIDI" = "xyesyes"; then
		MIDI_LIBS="-lportmidi"
	fi

	if test "x$PORTMIDI" != "xno" -a -n "$portmidisrc" -a -n "$PM_OS"; then
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

	if test "x$ALSAMIDI" != "xno"; then
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
		MIDI_LIBS="$MIDI_LIBS -lasound"
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
AS_IF([test "x$enable_lash" != "xno"], [
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
dnl http://ltcsmpte.sf.net
dnl ----------------------------------------------
AS_IF([ test "x$enable_ltc" != "xno"], [
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

if test "x$enable_qtgui" != "xno"; then

 # Check for Qt qmake utility.
 AC_PATH_PROG(ac_qmake, qmake, [no], $QTDIR/bin:${PATH})
 if test "x$ac_qmake" = "xno"; then
    AC_MSG_ERROR([qmake not found: please install the Qt4 development
    package of your distribution. try setting the QTDIR env. variable; or use --disable-qtgui to compile xjadeo.])
 fi
 if test -n "$HAVE_WINDOWS"; then
   ac_qmake_args="-spec win32-x-g++ -config release"
 fi
 AC_SUBST(ac_qmake)
 AC_SUBST(ac_qmake_args)

 # Check for Qt4

 AC_MSG_NOTICE([Checking for Qt4...])

 cat > conftest.c << EOF
#include <qglobal.h>
#if QT_VERSION < 0x040000
#error Qt library is older than 4.0.0
#endif
EOF

 $ac_qmake -project -nopwd -t lib -o conftest.pro "CONFIG+=qt warn_on release" conftest.c &> /dev/null
 if test $? -ne 0; then
  AC_MSG_WARN([TEST_QT4: could not create a project file. Try to set the QTDIR env. variable.])
 fi

 $ac_qmake -makefile -o conftest.mak conftest.pro &> /dev/null
 if test $? -ne 0; then
  AC_MSG_WARN([TEST_QT4: could not create a Makefile.])
 fi

 make -s -f conftest.mak &>/dev/null
 if test $? -ne 0; then
  AC_MSG_NOTICE([Qt4 not found.])
 else
  make -f conftest.mak distclean &>/dev/null
  AC_MSG_NOTICE([Found Qt4])
  # Check for Qt qmake utility.
  #AC_PATH_PROG(ac_qt3to4, qt3to4, [no], $QTDIR/bin:${PATH})
  #if test "x$ac_qt3to4" = "xno"; then
  #   AC_MSG_ERROR([qt3to4 not found: install Qt3Support, the Qt 3 compatibility module for Qt 4])
  #fi
  #AC_SUBST(ac_qt3to4)
  HAVE_QT4=1
 fi

 ac_qtgui_dir="qt-gui"

 AC_CONFIG_FILES(src/qt-gui/Makefile src/qt-gui/qjadeo.pro)

 if test -z "$HAVE_QT4" -a -z "$HAVE_QT3"; then 
     AC_MSG_ERROR([TEST_QT3/4: no usable Qt installation found. try --disable-qtgui])
 fi

else
 ac_qtgui_dir=""

fi

AC_SUBST(ac_qtgui_dir)

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

AC_CONFIG_FILES([Makefile src/Makefile src/xjadeo/Makefile xjadeo.lsm xjadeo.spec])

AC_OUTPUT

echo " "
echo "------------------------------------------------------------------------"
echo "  $PACKAGE $VERSION  - Configuration Report"
echo "------------------------------------------------------------------------"
echo " "
echo    " prefix:             $prefix"
echo    " exec-prefix:        $exec_prefix"
echo    " install path:       $bindir"
echo    " System wide config: $sysconfdir/xjadeorc"
echo " "
echo    " Video Displays"
echo -n "   - libxv:          ";	if test -n "$HAVE_LIBXV"; then echo "ok"; else echo "not avail."; fi
#echo -n "   - libx11/imlib:   ";	if test -n "$HAVE_IMLIB"; then echo "ok"; else echo "not avail."; fi
echo -n "   - libx11/imlib2:  ";	if test -n "$HAVE_IMLIB2"; then echo "ok"; else echo "not avail."; fi
echo -n "   - mac/osx:        ";	if test -n "$HAVE_MACOSX"; then echo "ok"; else echo "not avail."; fi
echo -n "   - SDL:            ";	if test -n "$HAVE_SDL"; then echo "ok"; else echo "not avail."; fi
echo -n " MIDI:               ";
if test -n "$HAVE_PORTMIDI"; then echo -n "portmidi "; fi
if test -n "$HAVE_JACKMIDI"; then echo -n "jack-midi "; fi
if test -n "$ALSA_SEQ_MIDI"; then echo -n "alsa-seq "; fi
if test -n "$ALSA_RAW_MIDI"; then echo -n "alsa-RAW "; fi
if test -z "$HAVE_MIDI"; then echo -n "disabled."; fi
echo 
echo -n " LASH:               ";	if test -n "$HAVE_LASH"; then echo "ok"; else echo "not avail."; fi
echo -n " LTCSMPTE:           ";  if test -n "$HAVE_LTCSMPTE"; then echo "ok"; else echo "not avail."; fi 
#echo -n " Xpm:                ";	if test -n "$HAVE_XPM"; then echo "ok"; else echo "not avail."; fi
echo -n " remote control:     ";	if test -n "$HAVE_MQ"; then echo "POSIX-MQ"; elif test -n "$HAVE_IPCMSG"; then echo "IPC-MSG"; else echo "not avail."; fi
echo -n " OSC remote control: ";	if test -n "$HAVE_LIBLO"; then echo "ok"; else echo "not avail."; fi
echo -n " On screen display:  ";
if test -z "$HAVE_FT"; then 
echo "disabled";
else echo 
echo    "   - freetype:       ok";
echo -n "   - default font:   ";	if test -n "$fontfile" ; then echo "$fontfile"; else echo " - "; fi
fi
echo -n " ffmpeg-scaling:     ";	
if test -n "$HAVE_SWSCALE"; then echo "swscale"; else echo "img_convert"; fi
echo -n " GUI:                ";	
if test -n "$HAVE_QT4"; then echo "QT4";
else if test -n "$HAVE_QT3"; then echo "QT3"
     else echo "none"; fi
fi
echo -n " build contib/*:     ";	if test "x$enable_contrib" = "xyes"; then echo "yes."; else echo "no."; fi
if test "x$enable_contrib" = "xyes"; then
     echo -n " libtiff (xjtsmm):   ";	if test -n "$HAVE_TIFF"; then echo "ok"; else echo "not avail."; fi
fi
echo " "
echo "now type 'make'; followed by 'make install' as root."
echo " "

