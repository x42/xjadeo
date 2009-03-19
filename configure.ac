dnl Process this file with autoconf to produce a configure script.

AC_INIT(xjadeo.c)
AC_CONFIG_SRCDIR(src/xjadeo/xjadeo.c)

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

VERSION_MAJOR=0
VERSION_MINOR=4
VERSION_SUB="6"

dnl Every other copy of the package version number gets its value from here
AM_INIT_AUTOMAKE(xjadeo, [${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_SUB}])
#AC_REVISION([$Id: configure.ac $])

dnl create a config.h file (Automake will add -DHAVE_CONFIG_H)
AM_CONFIG_HEADER(config.h)

AC_ARG_ENABLE(xv,    AC_HELP_STRING([--disable-xv],  [Disable Xvideo extension ]))
#AC_ARG_ENABLE(imlib, AC_HELP_STRING([--disable-imlib], [Disable use of imlib for display]))
AC_ARG_ENABLE(imlib2, AC_HELP_STRING([--disable-imlib2], [Disable use of imlib2 for display]))
AC_ARG_ENABLE(ft,    AC_HELP_STRING([--disable-ft],  [Disable freetype On-Screen.Display support]))
AC_ARG_ENABLE(qtgui, AC_HELP_STRING([--disable-qtgui],[Don't build the Qt GUI controller]))
AC_ARG_ENABLE(contrib,  AC_HELP_STRING([--enable-contrib],  [Compile and install code in contrib folder]))
AC_ARG_ENABLE(lash, AC_HELP_STRING([--disable-lash],[disable LASH support]))
AC_ARG_ENABLE(timescale, AC_HELP_STRING([--disable-timescale],[disable time-mapping/loop-playback support]))
AC_ARG_ENABLE(tiff, AC_HELP_STRING([--disable-tiff],[disable libtiff support]))
AC_ARG_ENABLE(mq, AC_HELP_STRING([--disable-mq],[disable mq remote support]))

dnl FPU emulation core.
AC_ARG_ENABLE(midi, AC_HELP_STRING([--enable-midi=auto], [select MTC support: auto,alsa,portmidi,off]),
[ case "$enableval" in
	auto)		MIDI=yes PORTMIDI=yes ALSAMIDI=yes ;;
	pm)		MIDI=yes PORTMIDI=yes ALSAMIDI=no ;;
	portmidi)	MIDI=yes PORTMIDI=yes ALSAMIDI=no ;;
	alsamidi)	MIDI=yes PORTMIDI=no  ALSAMIDI=yes ;;
	alsa)		MIDI=yes PORTMIDI=no  ALSAMIDI=yes ;;
	off)		MIDI=no  PORTMIDI=no  ALSAMIDI=no ;;
	*)		AC_MSG_ERROR([--enable-midi takes only one of the following values: auto, portmidi, alsa, off]);;
  esac
], [ MIDI=yes PORTMIDI=yes ALSAMIDI=yes])

AC_ARG_WITH(fontfile, AS_HELP_STRING([--with-fontfile], [specify an absolte path to a truetype font file to be used as default OSD font.]), fontfile="$withval", fontfile="")

AC_ARG_WITH(portmidiprefix, AS_HELP_STRING([--with-portmidiprefix], [specify prefix to portmidi source)]), portmidiprefix="$withval", portmidiprefix="")
AC_ARG_WITH(freetypeprefix, AS_HELP_STRING([--with-freetypeprefix], [specify prefix to freetype source)]), freetypeprefix="$withval", freetypeprefix="")
AC_ARG_WITH(ffmpegprefix, AS_HELP_STRING([--with-ffmpegprefix], [prefix to ffmpeg source, if libavcodec and libavformat are not in the default path.)]), ffmpegprefix="$withval", ffmpegprefix="")
AC_ARG_WITH(ffmpegldadd, AS_HELP_STRING([--with-ffmpegldadd], [ld options for static ffmpeg eg. "-lmp3lame -ldts -lfaac -logg -lxvidcore"]), ffmpegldadd="$withval", ffmpegldadd="")

LIBPREF=lib
LIBSUF=.a

target_os=$(uname -s | tr ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz)
case $target_os in
  darwin)
  	PM_OS=mac
  	AC_DEFINE([HAVE_MACOSX], [], [use macintosh's native display])
	HAVE_MACOSX=1
	LDFLAGS="-lz -lbz2 -framework Carbon -framework Quicktime"
dnl 2006/11/13 : 32 bit RGBA is known not to work on OSX ffmpeg+imlib2
dnl IMLIB2RGBA 
  	;;
  linux)
  	PM_OS="linux";
  	AC_DEFINE([IMLIB2RGBA], [], [use native RGB32 ffmpeg -> imlib.])
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
AC_CHECK_HEADERS(time.h string.h jack/jack.h jack/transport.h)

dnl Checks for libraries.

AH_TEMPLATE([HAVE_LIBXV], [Define as 1 if  you have the Xv library (-lXv)])
#AH_TEMPLATE([HAVE_IMLIB], [Define as 1 if you have imlib])
AH_TEMPLATE([HAVE_IMLIB2], [Define as 1 if you have imlib2])
AH_TEMPLATE([HAVE_PORTMIDI], [Define as 1 if you have portmidi and not asound])
AH_TEMPLATE([HAVE_MIDI], [Define as 1 if you have either alsa or portmidi])
AH_TEMPLATE([HAVE_LASH], [Define as 1 if you have lash])
AH_TEMPLATE([HAVE_XPM], [Define as 1 if you have libXpm])
AH_TEMPLATE([HAVE_MQ], [Define as 1 if you have POSIX message queues (libc, librt)])
AH_TEMPLATE([HAVE_TIFF], [Define as 1 if you have libtiff (contrib/tsmm)])
AH_TEMPLATE([TTFFONTFILE], [absolute path to truetype default OSD font file])
AH_TEMPLATE([CFG_WARN_ONLY], [errors in the resource config file only result in a warning message instead of forcing xjadeo to exit.])
AH_TEMPLATE([DND], [enable xjadeo x11 drag and drop])
AH_TEMPLATE([COLOREQ], [enable XV and imlib2 color equalizer])
AH_TEMPLATE([TIMEMAP], [enable dynamic time scaling])
AH_TEMPLATE([DRAW_CROSS], [paint a X on screen when unable to decode or display a frame.])
AH_TEMPLATE([HAVE_JACK_VIDEO], [experimental code to use jack_video, jack-svn >= r591])
AH_TEMPLATE([HAVE_SWSCALE], [Define as 1 if you have ffmpeg with swscale support])

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

# FIXME - needed fi. on osx
LIBS="$LIBS -L/usr/X11R6/lib"
#X_LIBS="-L/usr/X11R6/lib/"

#PKG_CHECK_MODULES(JACK, jack, HAVE_JACK=1, HAVE_JACK=0)
AC_CHECK_LIB([jack], [jack_client_new], , [AC_MSG_ERROR(*** AUDIO: You need to have the JACK audio development kit. ***)])

MQ_LIBS=""
if test "x$enable_mq" != "xno"; then
	AC_SEARCH_LIBS( mq_open, rt, AC_DEFINE(HAVE_MQ) MQ_LIBS="-lrt" HAVE_MQ=1 )
fi
AC_SUBST(MQ_LIBS)
AC_DEFINE(MQLEN, 512, [ max length of a remote control line. ])

dnl I experienced strange behaviour when using --with-ffmpegprefix:
dnl if the first pkg_check_module fails,  subsequent calls will fail too!
dnl workaround: move this one on top of ffmpeg detection..
PKG_CHECK_MODULES(Xpm, xpm, [ AC_DEFINE(HAVE_XPM) HAVE_XPM=1 ], [ NULL=0] )
#AC_SEARCH_LIBS( XpmCreatePixmapFromData, Xpm, AC_DEFINE(HAVE_XPM) XPM_LIBS="-lXpm" HAVE_XPM=1 )


if test "x$enable_xv" != "xno"; then
	AC_CHECK_LIB(Xv, XvQueryAdaptors, [AC_DEFINE(HAVE_LIBXV) AC_SUBST(XV_LIBS) AC_SUBST(XV_CFLAGS) XV_LIBS='-lXv' HAVE_LIBXV=1 ], [AC_MSG_ERROR([Could not find XVideo Lib (libXv)])], $X_LIBS)
fi

AC_CHECK_LIB([X11], [XOpenDisplay], [XV_LIBS="$XV_LIBS -lXext"] , if [ -z "$HAVE_MACOSX" ]; then AC_MSG_ERROR([*** X11: You need to install the X11 development package: libx11-dev ***]) else AC_MSG_WARN([*** X11: Could not find X11 development libs. - This is only ok if you're compiling for a macintosh. ***]); fi )

dnl ----------------------------------------------
dnl alsamidi/portmidi 
dnl ----------------------------------------------
if test "x$MIDI" != "xno"; then
	MIDI_CFLAGS=""
	MIDI_CFLAGS=""
	if test "x$PORTMIDI" != "xno" -a -z "$portmidiprefix" ; then
		AC_CHECK_HEADERS(portmidi.h porttime.h , [FOUND_H_PORTMIDI=yes])
		BACKUPLIBS="$LIBS"
		LIBS="$LIBS -lporttime"
		AC_CHECK_LIB([portmidi], [Pm_CountDevices], [FOUND_L_PORTMIDI=yes])
		LIBS="$BACKUPLIBS"
		MIDI_LIBS="-lportmidi -lporttime"
	fi

	if test "x$PORTMIDI" != "xno" -a -n "$portmidiprefix" -a -n "$PM_OS"; then
		AC_CHECK_FILES([$portmidiprefix/porttime/porttime.h $portmidiprefix/pm_common/portmidi.h],[FOUND_H_PORTMIDI=yes])
	# magic ppc-portmidi :  cc -dynamiclib pm_mac/*.o porttime/*.o -flat_namespace -undefined suppress -o portmidi.dylib

		AC_CHECK_FILES([$portmidiprefix/porttime/${LIBPREF}porttime$LIBSUF $portmidiprefix/pm_${PM_OS}/${LIBPREF}portmidi$LIBSUF],[FOUND_L_PORTMIDI=yes MIDI_LIBS="$portmidiprefix/porttime/${LIBPREF}porttime$LIBSUF $portmidiprefix/pm_${PM_OS}/${LIBPREF}portmidi$LIBSUF"])
		AC_CHECK_FILES([$portmidiprefix/pm_${PM_OS}/build/${LIBPREF}portmidi$LIBSUF],[FOUND_L_PORTMIDI=yes MIDI_LIBS="$portmidiprefix/pm_${PM_OS}/build/${LIBPREF}portmidi$LIBSUF"])
		if test $PM_OS = "mac" ; then MIDI_LIBS="$MIDI_LIBS -framework CoreMIDI -framework CoreFoundation -framework CoreAudio"; fi

		MIDI_CFLAGS="-I$portmidiprefix/porttime/ -I$portmidiprefix/pm_common/"
	fi

	if test "x$ALSAMIDI" != "xno"; then
		AC_CHECK_HEADERS(alsa/asoundlib.h, [FOUND_H_ALSAMIDI=yes])
		AC_CHECK_HEADERS(pthread.h, [FOUND_H_PTHREAD=yes])
		AC_CHECK_LIB([asound], [snd_rawmidi_open], [FOUND_L_ALSAMIDI=yes])
	fi

	if test "x$FOUND_H_ALSAMIDI$FOUND_L_ALSAMIDI$FOUND_H_PTHREAD" = "xyesyesyes"; then
		AC_DEFINE(HAVE_MIDI) 
		HAVE_MIDI=1
		MIDI_LIBS="-lasound"
	elif test "x$FOUND_H_PORTMIDI$FOUND_L_PORTMIDI" = "xyesyes"; then
		AC_DEFINE(HAVE_MIDI) 
		AC_DEFINE(HAVE_PORTMIDI) 
		HAVE_MIDI=1
		HAVE_PORTMIDI=1
	else 
		MIDI_LIBS=""
		MIDI_MIDI=""
		MIDI_CFLAGS=""
	fi
fi

AC_SUBST(MIDI_LIBS)
AC_SUBST(MIDI_CFLAGS)

dnl ----------------------------------------------
dnl Check for ffmpeg: avformat/avcodec 
dnl ----------------------------------------------

#
#if test -z "$ffmpegprefix" ; then
#dnl check for local ffmpeg ./ffmpeg copy
#PKG_CONFIG_PATH=./ffmpeg:$PKG_CONFIG_PATH
#else
#PKG_CONFIG_PATH=$ffmpegprefix:$PKG_CONFIG_PATH
#fi 
#
if test -z "$ffmpegprefix" ; then
	PKG_CHECK_MODULES(FFMPEG, libavformat libavcodec libavutil, HAVE_FFMPEG=yes, 
		AC_MSG_WARN([
		Could not find ffmpeg package. please update PKG_CONFIG_PATH to 
		point to a ffmpegs source folder.
		])) 
fi

if test "x$HAVE_FFMPEG" != "xyes" ; then 
	AC_CHECK_PROG(HAVE_FFMPEG_CONFIG, ffmpeg-config, yes)
fi

if test -z "$ffmpegprefix" -a "x$HAVE_FFMPEG_CONFIG" == xyes; then
	FFMPEG_CFLAGS=`ffmpeg-config --cflags`
	FFMPEG_LIBS=`ffmpeg-config --libs avformat`
	HAVE_FFMPEG=yes
        AC_MSG_NOTICE([
	located a ffmpeg installation without pkg-information.
	])
fi

if test  "x$HAVE_FFMPEG" != "xyes" -a -z "$ffmpegprefix" ; then
		AC_CHECK_HEADER(ffmpeg/avcodec.h,[HAVE_SHARED_AVCODEC=yes])
		AC_CHECK_HEADER(ffmpeg/avformat.h, [HAVE_SHARED_AVFORMAT=yes])
		AC_CHECK_FILE(/usr/local/include/ffmpeg, [HAVE_FFMPEG_LOCAL=yes])
fi

if test  "x$HAVE_FFMPEG" != "xyes" -a -n "$ffmpegprefix" ; then
		AC_CHECK_FILES([$ffmpegprefix/libavcodec/${LIBPREF}avcodec$LIBSUF $ffmpegprefix/libavformat/${LIBPREF}avformat$LIBSUF],[HAVE_SHARED_AVCODEC=yes HAVE_SHARED_AVFORMAT=yes])
fi

if test  "x$HAVE_FFMPEG" != "xyes" -a "x$HAVE_SHARED_AVCODEC$HAVE_SHARED_AVCODEC" != xyesyes; then
		AC_MSG_ERROR([
	libavcodec and libavformat from ffmpeg are required.
	If you are sure that ffmpeg is installed, point
	configure to your local copy with f.i.
	./configure --with-ffmpegprefix=/usr/local/src/ffmpeg
	])
fi

dnl backwards compat...
if test  "x$HAVE_FFMPEG" != "xyes"; then
	FFMPEG_LIBS="-L/usr/local/lib -L/usr/lib -lavformat -lavcodec -lavutil -lz"
	if test -n "$ffmpegprefix" ; then
		FFMPEG_INCDIR="$ffmpegprefix"
		FFMPEG_LIBS="$ffmpegprefix/libavformat/${LIBPREF}avformat$LIBSUF $ffmpegprefix/libavcodec/${LIBPREF}avcodec$LIBSUF $ffmpegprefix/libavutil/${LIBPREF}avutil$LIBSUF $ffmpegprefix/libswscale/${LIBPREF}swscale$LIBSUF $ffmpegprefix/libavdevice/${LIBPREF}avdevice$LIBSUF -lz $ffmpegldadd" 
		#FFMPEG_LIBS="$ffmpegprefix/libavformat/${LIBPREF}avformat$LIBSUF $ffmpegprefix/libavcodec/${LIBPREF}avcodec$LIBSUF $ffmpegprefix/libavutil/${LIBPREF}avutil$LIBSUF $ffmpegprefix/libswscale/${LIBPREF}swscale$LIBSUF $ffmpegprefix/libpostproc/${LIBPREF}postproc$LIBSUF $ffmpegprefix/libavdevice/${LIBPREF}avdevice$LIBSUF -lz $ffmpegldadd" 
		#FFMPEG_LIBS="-L$ffmpegprefix/libavdevice -L$ffmpegprefix/libavformat -L$ffmpegprefix/libavcodec -L$ffmpegprefix/libavutil -Wl,-rpath-link,$ffmpegprefix/libavcodec -Wl,-rpath-link,$ffmpegprefix/libavformat -Wl,-rpath-link,$ffmpegprefix/libavutil -Wl,-Bsymbolic -lavdevice -lavformat -lavcodec -lavutil  -lz -lm $ffmpegldadd" 
	elif test "x$HAVE_FFMPEG_LOCAL" != xyes; then
		FFMPEG_INCDIR="/usr/local/include/ffmpeg/"
	else
		FFMPEG_INCDIR="/usr/include/ffmpeg/"
	fi
	HAVE_FFMPEG=yes
        AC_MSG_NOTICE([
	located a ffmpeg installation without pkg-information.
	])

	FFMPEG_CFLAGS="-I$FFMPEG_INCDIR -I$FFMPEG_INCDIR/libavcodec -I$FFMPEG_INCDIR/libavformat -I$FFMPEG_INCDIR/libavutil -I$FFMPEG_INCDIR/libswscale"
else
dnl FFMPEG include workaround.
test -d /usr/local/include/libavcodec && FFMPEG_INCDIR="/usr/local/include/" || FFMPEG_INCDIR="/usr/include/ffmpeg/"
FFMPEG_CFLAGS="$FFMPEG_CFLAGS -I$FFMPEG_INCDIR/libavcodec -I$FFMPEG_INCDIR/libavformat -I$FFMPEG_INCDIR/libavutil -I$FFMPEG_INCDIR/libswscale"
fi

ac_save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $FFMPEG_CFLAGS"
AC_CHECK_HEADERS(swscale.h , [AC_DEFINE(HAVE_SWSCALE) HAVE_SWSCALE=1; if test -n "$ffmpegprefix" ; then FFMPEG_LIBS="$FFMPEG_LIBS $ffmpegprefix/libswscale/${LIBPREF}swscale$LIBSUF"; else FFMPEG_LIBS="$FFMPEG_LIBS -lswscale"; fi])
CFLAGS="$ac_save_CFLAGS"

AC_SUBST(FFMPEG_CFLAGS)
AC_SUBST(FFMPEG_LIBS)

dnl ----------------------------------------------
dnl Check for freetype 
dnl ----------------------------------------------

FREETYPE_LIBS=
FREETYPE_CFLAGS=

FOUND_H_FREETYPE=
FOUND_L_FREETYPE=

if test -n "$freetypeprefix" ; then
	AC_CHECK_FILES([$freetypeprefix/include/ft2build.h],[FOUND_H_FREETYPE=yes])
	AC_CHECK_FILES([$freetypeprefix/objs/.libs/${LIBPREF}freetime$LIBSUF],[FOUND_L_FREETYPE=yes])
fi

if test "x$FOUND_H_FREETYPE$FOUND_L_FREETYPE" = "xyesyes" ; then
	FREETYPE_CFLAGS="-I$freetypeprefix/include/"
	FREETYPE_LIBS="$freetypeprefix/objs/.libs/${LIBPREF}freetime$LIBSUF"
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

if test "x${enable_imlib2}" != "xno"; then
	PKG_CHECK_MODULES(IMLIB2, imlib2 >= 1.3.0,  [ AC_DEFINE(HAVE_IMLIB2) HAVE_IMLIB2=1 ], [ NULL=0 ])
fi
AC_SUBST(IMLIB2_CFLAGS)
AC_SUBST(IMLIB2_LIBS)

dnl ----------------------------------------------
dnl http://lash.nongnu.org
dnl ----------------------------------------------
if test "x$enable_lash" != "xno"; then
  PKG_CHECK_MODULES(LASH, lash-1.0 >= 0.4.0, LASH_FOUND="yes", [ NULL=0 ])
  
  if test "x$LASH_FOUND" = "xyes"; then
    AC_DEFINE(HAVE_LASH)
    HAVE_LASH=1;
    #LASH_VERSION=$( pkg-config --modversion lash-1.0 )
    #AC_DEFINE_UNQUOTED(LASH_VERSION, "$LASH_VERSION", [The version of lash we're compiling against])
  fi
fi
AC_SUBST(LASH_CFLAGS)
AC_SUBST(LASH_LIBS)

dnl ----------------------------------------------
dnl contrib/tsmm
dnl ----------------------------------------------

if test "x${enable_contrib}" != "xno"; then
  AC_CHECK_HEADERS(tiffio.h, [FOUND_H_TIFF=yes])
  AC_CHECK_LIB([tiff], [TIFFOpen], [FOUND_L_TIFF=yes])

  if test "x$FOUND_H_TIFF$FOUND_L_TIFF" = "xyesyes"; then
    AC_DEFINE(HAVE_TIFF) HAVE_TIFF=1 
    TIFF_LIBS="-ltiff"	
    TIFF_CFLAGS=""	
  else
    PKG_CHECK_MODULES(TIFF,tiff,  [ AC_DEFINE(HAVE_TIFF) HAVE_TIFF=1 ], [ NULL=0 ])
  fi
fi
AC_SUBST(TIFF_CFLAGS)
AC_SUBST(TIFF_LIBS)

dnl ----------------------------------------------
dnl Qt GUI stuff
dnl ----------------------------------------------

if test "x$enable_qtgui" != "xno"; then

 # Check for Qt qmake utility.
 AC_PATH_PROG(ac_qmake, qmake, [no], $QTDIR/bin:${PATH})
 if test "x$ac_qmake" = "xno"; then
    AC_MSG_ERROR([qmake not found: please install the Qt development
    package of your distribution (either QT3 or QT4+qt3compat). try setting the QTDIR env. variable; or use --disable-qtgui to compile xjadeo.])
 fi
 AC_SUBST(ac_qmake)

 # Check for Qt3

 AC_MSG_NOTICE([Checking for Qt >= 3...])

 cat > conftest.c << EOF
#include <qglobal.h>
#if QT_VERSION < 0x030000
#error Qt library is older than 3.0.0
#endif
EOF

 $ac_qmake -project -nopwd -t lib -o conftest.pro "CONFIG+=qt warn_on release" conftest.c &> /dev/null
 if test $? -ne 0; then
  AC_MSG_WARN([TEST_QT3: could not create a project file. Try to set the QTDIR env. variable.])
 fi

 $ac_qmake -makefile -o conftest.mak conftest.pro &> /dev/null
 if test $? -ne 0; then
  AC_MSG_WARN([TEST_QT3: could not create a Makefile.])
 fi

 make -s -f conftest.mak &>/dev/null
 if test $? -ne 0; then
  AC_MSG_WARN([TEST_QT3: QT3 is not installed or version of Qt is too old.])
 else
  make -s -f conftest.mak distclean &>/dev/null
  AC_MSG_NOTICE([Found Qt >= 3])
  HAVE_QT3=1
 fi

 # AC removes conftest*

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
  AC_PATH_PROG(ac_qt3to4, qt3to4, [no], $QTDIR/bin:${PATH})
  if test "x$ac_qt3to4" = "xno"; then
     AC_MSG_ERROR([qt3to4 not found: install Qt3Support, the Qt 3 compatibility module for Qt 4])
  fi
  AC_SUBST(ac_qt3to4)
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
echo -n " MIDI:               ";
if test -n "$HAVE_PORTMIDI"; then echo "portmidi";
else if test -n "$HAVE_MIDI"; then echo "alsa"
     else echo "disabled."; fi
fi
echo -n " LASH:               ";	if test -n "$HAVE_LASH"; then echo "ok"; else echo "not avail."; fi
#echo -n " Xpm:                ";	if test -n "$HAVE_XPM"; then echo "ok"; else echo "not avail."; fi
echo -n " POSIX msg queue:    ";	if test -n "$HAVE_MQ"; then echo "ok"; else echo "not avail."; fi
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

