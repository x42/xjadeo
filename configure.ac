dnl Process this file with autoconf to produce a configure script.

AC_INIT(xjadeo.c)
AC_CONFIG_SRCDIR(src/xjadeo/xjadeo.c)

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

dnl Every other copy of the package version number gets its value from here
AM_INIT_AUTOMAKE(xjadeo, 0.3.7)

dnl create a config.h file (Automake will add -DHAVE_CONFIG_H)
AM_CONFIG_HEADER(config.h)

AC_ARG_ENABLE(gtk,     [  --enable-gtk            Enable use of gtk for display])
AC_ARG_ENABLE(noxv,    [  --enable-noxv           Disable use of XVideo extension for display])
AC_ARG_ENABLE(noiml,   [  --enable-noiml          Disable use of imlib for display])
AC_ARG_ENABLE(nosdl,   [  --enable-nosdl          Disable use of SDL for display])
AC_ARG_ENABLE(midi,    [  --enable-midi           Offer MTC sync as an alternative to jack (portmidi) ])
# not supported by xjadeo-SDL yet
#AC_ARG_ENABLE(ttf,    [  --enable-ttf            Enable TTF font support],,enable_ttf=no) 
AC_ARG_ENABLE(noft,    [  --enable-noft           Disable freetype OSD support]) 
AC_ARG_ENABLE(qtgui, AC_HELP_STRING([--disable-qtgui],[Don't build the Qt GUI controller]))


AC_ARG_WITH(ffmpegprefix, AS_HELP_STRING([--with-ffmpegprefix], [prefix to ffmpeg source, if libavcodec and libavformat are not in . (default=.)]), ffmpegprefix="$withval", ffmpegprefix="ffmpeg")

AC_SUBST(VERSION)

ISODATE=`date +%Y-%m-%d`
AC_SUBST(ISODATE)

dnl Checks for programs.
AC_PROG_INSTALL
AC_PROG_CC

dnl Checks for libraries.

AH_TEMPLATE([HAVE_GTK], [Define as 1 if you have gtk])
AH_TEMPLATE([HAVE_SDL], [Define as 1 if you have libsdl])
AH_TEMPLATE([HAVE_LIBXV], [Define as 1 if  you have the Xv library (-lXv)])
AH_TEMPLATE([HAVE_IMLIB], [Define as 1 if you have imlib])
AH_TEMPLATE([HAVE_MIDI], [Define as 1 if you have portmidi and asound])

if test "$enable_gtk" = "yes"; then
	REQUIRES='glib >= 1.2.4 gtk+ >= 1.2.4'
	PKG_CHECK_MODULES(GTK,$REQUIRES)
	AM_PATH_GTK(1.2.4, [ AC_DEFINE(HAVE_GTK) ])
	AM_PATH_GDK_PIXBUF(0.22.0, [ AC_DEFINE(HAVE_GDK_PIXBUF,1, [Use pixbuf for gtk scaling]) ])
fi


# FIXME
X_LIBS="-L/usr/X11R6/lib/"

if test ! "$enable_noxv" = "yes"; then
	AC_CHECK_LIB(Xv, XvQueryAdaptors, [AC_DEFINE(HAVE_LIBXV) AC_SUBST(XV_LIBS) XV_LIBS='-lXv' ], [AC_MSG_ERROR([Could not find Xv Lib])], $X_LIBS)
fi

if test  "$enable_midi" = "yes"; then
	AC_CHECK_HEADERS(portmidi.h porttime.h )
	BACKUPLIBS="$LIBS"
	LIBS="$LIBS -lporttime"
	AC_CHECK_LIB([portmidi], [Pm_CountDevices], , [AC_MSG_ERROR(*** MIDI: portmidi-dev not found ***)])
	LIBS="$BACKUPLIBS"
	AC_DEFINE(HAVE_MIDI) 
	MIDI_LIBS="-lportmidi -lporttime"
	AC_SUBST(MIDI_LIBS)
fi

if test ! "$enable_noiml" = "yes"; then
#	AC_CHECK_HEADERS(Imlib.h)
#	AC_CHECK_LIB([Imlib], [Imlib_init], , [AC_MSG_ERROR(*** IMAGE: You should install imlib-dev first ***)])
        AM_PATH_IMLIB(1.9.13, [ AC_DEFINE(HAVE_IMLIB) ])
fi

if ! test "$enable_nosdl" = "yes"; then
        AM_PATH_SDL(1.1.6, [ AC_DEFINE(HAVE_SDL) ])

	# SDL TTF detect

	AC_CHECK_LIB(SDL_ttf, TTF_Init, HAVESDLTTF=yes, HAVESDLTTF=no)
	BACKUPCFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $SDL_CFLAGS" # `sdl-config --cflags`
	AC_CHECK_HEADER(SDL_ttf.h, HAVESDLTTF=yes, HAVESDLTTF=no)
	CFLAGS="$BACKUPCFLAGS"

fi

if test x$HAVESDLTTF = xyes; then
    if test x$enable_ttf = xyes; then
        AC_MSG_RESULT(yes)
        AC_DEFINE(USE_SDLTTF, 1, [Use SDL_ttf])
        LIBS="$LIBS -lSDL_ttf"
    fi
fi



dnl Check for freetype headers
FREETYPE_LIBS=
FREETYPE_CFLAGS=

PKG_CHECK_MODULES(FREETYPE, freetype2,
	  [freetype_pkgconfig=yes], [freetype_pkgconfig=no])

if test "x$freetype_pkgconfig" = "xyes"; then
	if test ! "$enable_noft" = "yes"; then
	  AC_DEFINE(HAVE_FREETYPE_H, 1, [Have FreeType2 include files])
	  AC_DEFINE(HAVE_FT, 1, [Use freetype for OSD])
	else
		FREETYPE_LIBS=
		FREETYPE_CFLAGS=
	fi
else
	AC_PATH_PROG(FREETYPE_CONFIG, freetype-config, no)
	if test "x$FREETYPE_CONFIG" != "xno" ; then
	    if test ! "$enable_noft" = "yes"; then
		FREETYPE_CFLAGS=`$FREETYPE_CONFIG --cflags`
		FREETYPE_LIBS=`$FREETYPE_CONFIG --libs`
		AC_DEFINE(HAVE_FREETYPE_H, 1, [Have FreeType2 include files])
		AC_DEFINE(HAVE_FT, 1, [Use freetype for OSD])
	    fi
	fi
fi
AC_SUBST(FREETYPE_CFLAGS)
AC_SUBST(FREETYPE_LIBS)




dnl used in Makefile.am
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)


dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(time.h string.h jack/jack.h jack/transport.h)


#jm_CHECK_TYPE_STRUCT_UTIMBUF
AC_HEADER_MAJOR
AC_FUNC_ALLOCA
AC_STRUCT_TM
#AC_STRUCT_ST_BLOCKS
#AC_FUNC_CLOSEDIR_VOID
#AC_CHECK_FUNCS(mkfifo)
#AC_CHECK_FUNC(mknod)

dnl Checks for typedefs, structures, and compiler characteristics.

dnl Checks for library functions.

LIBS="$LIBS -L/usr/X11R6/lib"

AC_CHECK_LIB([X11], [XOpenDisplay], , [AC_MSG_ERROR(*** X11: You need libx11-dev installed ***)])

AC_CHECK_LIB([jack], [jack_client_new], , [AC_MSG_ERROR(*** AUDIO: You need JACK audio library (libjack-dev) installed.  http://jackit.sourceforge.net/ ***)])

AC_CHECK_PROG(HAVE_FFMPEG_CONFIG, ffmpeg-config, yes)
if test "x$HAVE_FFMPEG_CONFIG" == xyes; then
	FFMPEG_CFLAGS=`ffmpeg-config --cflags`
	FFMPEG_LIBS=`ffmpeg-config --libs avformat`
else
	if test "$ffmpegprefix" == ffmpeg; then
		AC_CHECK_HEADER(ffmpeg/avcodec.h,[HAVE_SHARED_AVCODEC=yes])
		AC_CHECK_HEADER(ffmpeg/avformat.h, [HAVE_SHARED_AVFORMAT=yes])
		AC_CHECK_FILE(/usr/local/include/ffmpeg, [HAVE_FFMPEG_LOCAL=yes])
	fi
	if test "x$HAVE_SHARED_AVCODEC$HAVE_SHARED_AVCODEC" != xyesyes; then
		AC_MSG_ERROR([
	you need libavcodec and libavformat from ffmpeg cvs
	to get it run get_ffmpeg_cvs.sh or point configure to
	your local copy with i.e.
	./configure --with-ffmpegprefix=/usr/local/src/ffmpeg
	(see INSTALL for more information)
	])
	else
		if test "x$HAVE_FFMPEG_LOCAL" != xyes; then
			FFMPEG_CFLAGS="-I/usr/include/ffmpeg"
		else
			FFMPEG_CFLAGS="-I/usr/local/include/ffmpeg"
		fi
		FFMPEG_LIBS="-L/usr/local/lib -L/usr/lib -lavformat -lavcodec"
	fi
fi

AC_SUBST(FFMPEG_CFLAGS)
AC_SUBST(FFMPEG_LIBS)

dnl ----------------------------------------------
dnl Qt GUI stuff
dnl ----------------------------------------------

if test "x$enable_qtgui" != "xno"; then

 # Check for Qt qmake utility.
 AC_PATH_PROG(ac_qmake, qmake, [no], $QTDIR/bin:${PATH})
 if test "x$ac_qmake" = "xno"; then
    AC_MSG_ERROR([qmake not found: install the Qt development package of your distribution])
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
  AC_MSG_ERROR([TEST_QT3: could not create a project file])
 fi

 $ac_qmake -makefile -o conftest.mak conftest.pro &> /dev/null
 if test $? -ne 0; then
  AC_MSG_ERROR([TEST_QT3: could not create a Makefile])
 fi

 make -s -f conftest.mak &>/dev/null
 if test $? -ne 0; then
  AC_MSG_ERROR([TEST_QT3: version of Qt is too old])
 else
  make -s -f conftest.mak distclean &>/dev/null
  AC_MSG_NOTICE([Found Qt >= 3])
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
  AC_MSG_ERROR([TEST_QT4: could not create a project file])
 fi

 $ac_qmake -makefile -o conftest.mak conftest.pro &> /dev/null
 if test $? -ne 0; then
  AC_MSG_ERROR([TEST_QT4: could not create a Makefile])
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
 fi

 ac_qtgui_dir="qt-gui"

 AC_CONFIG_FILES(src/qt-gui/Makefile src/qt-gui/qjadeo.pro)

else
 ac_qtgui_dir=""

fi

AC_SUBST(ac_qtgui_dir)

dnl ---------- End of Qt GUI stuff ---------------


#AC_CHECK_LIB([Imlib2], [Imlib_init], foundimlib=yes , foundimlib=no) 
#
#if test "x$foundimlib" = "xyes"; then
# LIBS="$LIBS -lImlib2"
#else 
# AC_CHECK_LIB([Imlib], [Imlib_init], foundimlib=yes , foundimlib=no) 
# if test "x$foundimlib" = "xyes"; then
#  LIBS="$LIBS -lImlib"
# else
#  AC_MSG_ERROR(*** IMAGE: You should install imlib-dev first ***)
# fi
#fi

AC_CONFIG_FILES([Makefile src/Makefile src/xjadeo/Makefile xjadeo.lsm xjadeo.spec])

AC_OUTPUT
