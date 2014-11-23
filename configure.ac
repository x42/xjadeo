dnl Process this file with autoconf to produce a configure script.

AC_INIT(xjadeo.c)
AC_CONFIG_SRCDIR(src/xjadeo/xjadeo.c)

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

VERSION_MAJOR=0
VERSION_MINOR=8
VERSION_SUB=1

AM_INIT_AUTOMAKE(xjadeo, [${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_SUB}])
AM_CONFIG_HEADER(config.h)

AC_ARG_ENABLE(xv,          AC_HELP_STRING([--disable-xv], [Disable Xvideo extension (default:auto)]))
AC_ARG_ENABLE(opengl,      AC_HELP_STRING([--disable-opengl], [Disable OpenGL display (default:auto)]))
AC_ARG_ENABLE(imlib2,      AC_HELP_STRING([--disable-imlib2], [Disable use of imlib2 for display (default:auto)]))
AC_ARG_ENABLE(sdl,         AC_HELP_STRING([--disable-sdl], [Disable use of SDL display (default:auto)]))
AC_ARG_ENABLE(ft,          AC_HELP_STRING([--disable-ft], [Disable freetype On-Screen.Display support (default:auto)]))
AC_ARG_WITH(fontfile,      AS_HELP_STRING([--with-fontfile=file.ttf], [specify a path to a truetype font file to be used as default OSD font.]), fontfile="$withval", fontfile="")
AC_ARG_ENABLE(jacksession, AC_HELP_STRING([--disable-jacksession], [disable JACK-Session support (default:auto)]))
AC_ARG_ENABLE(mq,          AC_HELP_STRING([--disable-mq], [disable POSIX message-queue remote control (default:auto)]))
AC_ARG_ENABLE(ipc,         AC_HELP_STRING([--disable-ipc], [disable IPC message-queue remote control (default:auto)]))
AC_ARG_ENABLE(osc,         AC_HELP_STRING([--disable-osc], [disable OSC remote control support (default:auto)]))
AC_ARG_ENABLE(ltc,         AC_HELP_STRING([--disable-ltc], [disable LTC sync support (default:auto)]))
AC_ARG_ENABLE(midi,        AC_HELP_STRING([--disable-midi], [Do not build any of the MIDI drivers (default:on)]))
AC_ARG_ENABLE(portmidi,    AC_HELP_STRING([--disable-portmidi], [disable portmidi support (default:auto)]))
AC_ARG_ENABLE(alsamidi,    AC_HELP_STRING([--disable-alsamidi], [disable alsamidi support (default:auto)]))
AC_ARG_ENABLE(embed-font,  AC_HELP_STRING([--embed-font], [include OnScreenDisplay font in binary (default: off)]))
AC_ARG_ENABLE(qtgui,       AC_HELP_STRING([--enable-qtgui],[Build the Qt GUI controller (default:off)]))
AC_ARG_WITH(qt4prefix,     AS_HELP_STRING([--with-qt4prefix], [prefix for Qt4 installation, e.g. "/usr/lib/qt4"]), qt4prefix="$withval", qt4prefix="")
AC_ARG_WITH(qmakeargs,     AS_HELP_STRING([--with-qmakeargs], [specify custom qmake arguments]), qmakeargs="$withval", qmakeargs="")
AC_ARG_ENABLE(contrib,     AC_HELP_STRING([--enable-contrib], [Compile and install code in contrib folder (default:off)]))
AC_ARG_ENABLE(timescale,   AC_HELP_STRING([--enable-timescale], [enable time-mapping/loop-playback support - experimental - DO NOT USE]))
AC_ARG_ENABLE(framecrop,   AC_HELP_STRING([--enable-framecrop], [hardcoded frame cropping - experimental - DO NOT USE]))


LIBPREF=lib
LIBSUF=.a

case $target_os in
	*darwin*)
		AC_MSG_NOTICE([TARGET: OSX])
		PM_OS=mac
		AC_DEFINE([PLATFORM_OSX], [], [Mac OSX version])
		PLATFORM_OSX=1
		LDFLAGS="$LDFLAGS -lz -lbz2 -framework Carbon -framework Quicktime"
		GL_LIBS="-framework Cocoa -framework OpenGL"
		AC_DEFINE(HAVE_GL)
		HAVE_GL=1
		AM_CONDITIONAL([TARGET_OSX], true)
		AM_CONDITIONAL([TARGET_LINUX], false)
		AM_CONDITIONAL([TARGET_WIN32], false)
		dnl 2006/11/13 : 32 bit RGBA is known not to work on OSX ffmpeg+imlib2
		dnl IMLIB2RGBA
		;;
	*linux*|gnu*|k*bsd*-gnu)
		AC_DEFINE([PLATFORM_LINUX], [], [GNU/Linux version])
		PLATFORM_LINUX=1
		AC_MSG_NOTICE([TARGET: LINUX])
		PM_OS="linux";
		AC_DEFINE([IMLIB2RGBA], [], [use native RGB32 ffmpeg -> imlib.])
		AM_CONDITIONAL([TARGET_OSX], false)
		AM_CONDITIONAL([TARGET_LINUX], true)
		AM_CONDITIONAL([TARGET_WIN32], false)
		LIBS="$LIBS -L/usr/X11R6/lib"
		;;
	*mingw32*|*win*)
		AC_MSG_NOTICE([TARGET: WINDOWS])
		PM_OS=win
		AC_DEFINE([PLATFORM_WINDOWS], [], [Windows version])
		PLATFORM_WINDOWS=1
		LIBSUF=.dll
		EXESUF=.exe
		GL_LIBS="-lopengl32 -lglu32 -lgdi32"
		AC_DEFINE(HAVE_GL)
		HAVE_GL=1
		AM_CONDITIONAL([TARGET_OSX], false)
		AM_CONDITIONAL([TARGET_LINUX], false)
		AM_CONDITIONAL([TARGET_WIN32], true)
		AC_CHECK_TOOL(WINDRES, windres, false)
		if test "$WINDRES" = "false"; then
			AC_MSG_ERROR([Could not find an implementation of windres in PATH.])
		fi
		;;
	*)
		AC_MSG_WARN([*** unknown target OS ***])
		;;
esac

if test "x$enable_embed_font" == "xyes"; then
	AM_CONDITIONAL([EMBED_FONT], true)
else
	AM_CONDITIONAL([EMBED_FONT], false)
fi

dnl AC_SUBST(DISPLAY_OPENGL)
AC_SUBST(VERSION)
dnl ISODATE=`date +%Y-%m-%d`
dnl AC_SUBST(ISODATE)

dnl Checks for compiler(s).
AC_PROG_INSTALL
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_OBJC
AC_C_CONST
AC_C_BIGENDIAN

dnl Standard Checks
AC_HEADER_STDC
AC_CHECK_SIZEOF(float)
AC_CHECK_SIZEOF(double)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(unsigned long)
AC_CHECK_HEADERS(time.h string.h)

dnl Checks for libraries.

dnl video output
AH_TEMPLATE([HAVE_GL], [Define to enable openGL (win,osx,linux)])
AH_TEMPLATE([HAVE_SDL], [Define as 1 if  you have the SDL toolkit (win,osx,linux)])
AH_TEMPLATE([HAVE_LIBXV], [Video Output: XVideo support (linux)])
AH_TEMPLATE([HAVE_IMLIB2], [Video Output: plain old imlib2 (linux)])
AH_TEMPLATE([IMLIB2RGBA], [Define for RGBA32 imlib2 (not RGB24)])

dnl MTC sync
AH_TEMPLATE([HAVE_MIDI], [general MIDI (MTC) support])
AH_TEMPLATE([HAVE_JACKMIDI], [Define as 1 to enable JACK-MIDI])
AH_TEMPLATE([HAVE_PORTMIDI], [Define as 1 to enable the portmidi driver (win,osx)])
AH_TEMPLATE([ALSA_SEQ_MIDI], [Define as 1 to enable ALSA sequencer (linux)])
AH_TEMPLATE([ALSA_RAW_MIDI], [Define as 1 to enable ALSA-raw midi (linux)])

dnl LTC sync
AH_TEMPLATE([HAVE_LTC], [Define as 1 if you have libltc - http://github.com/x42/libltc ])

dnl jack options
AH_TEMPLATE([JACK_SESSION], [add JACK Session support])
AH_TEMPLATE([HAVE_JACK_VIDEO], [experimental code to use jack_transport fps, jack-svn >= r591])
AH_TEMPLATE([NEW_JACK_LATENCY_API], [use JACKs new latency API (ltc)])

dnl remote control
AH_TEMPLATE([HAVE_MQ], [Define as 1 if you have POSIX message queues (libc, librt)])
AH_TEMPLATE([HAVE_IPCMSG], [Define as 1 if you have IPC  message queues (system-V)])
AH_TEMPLATE([HAVE_LIBLO], [Define as 1 if you have the loblo OSC library])
AH_TEMPLATE([TTFFONTFILE], [absolute path to truetype default OSD font file])

dnl misc
AH_TEMPLATE([CFG_WARN_ONLY], [errors in the resource config file only result in a warning message instead of forcing xjadeo to exit.])
AH_TEMPLATE([DND], [enable xjadeo x11/glx drag and drop])
AH_TEMPLATE([XDLG], [enable xjadeo x11/glx context menu])
AH_TEMPLATE([XFIB], [enable xjadeo x11 file browser])
AH_TEMPLATE([COLOREQ], [enable XV and imlib2 color equalizer])
AH_TEMPLATE([TIMEMAP], [enable dynamic time scaling])
AH_TEMPLATE([CROPIMG], [enable frame cropping])
AH_TEMPLATE([DRAW_CROSS], [paint a X on screen when unable to decode or display a frame.])
AH_TEMPLATE([WINMENU], [enable xjadeo windows context-menu])

dnl fixed configuration
AC_DEFINE(CFG_WARN_ONLY)
AC_DEFINE(DND)
AC_DEFINE(WINMENU)
AC_DEFINE(XDLG)
AC_DEFINE(XFIB)
AC_DEFINE(DRAW_CROSS)
AC_DEFINE(COLOREQ)

if test "x$enable_timescale" != "xyes"; then
	AC_DEFINE(TIMEMAP)
fi

if test "x$enable_framecrop" == "xyes"; then
	AC_DEFINE(CROPIMG)
fi

AC_HEADER_MAJOR
AC_FUNC_ALLOCA
AC_STRUCT_TM

dnl ---------------------------------------------------------------------------
dnl JACK first.
PKG_CHECK_MODULES(JACK, jack, ,
	AC_MSG_ERROR([Could not fine JACK audio development kit - jackaudio.org.]))
AC_SUBST(JACK_CFLAGS)
AC_SUBST(JACK_LIBS)

if test "x$enable_jacksession" != "xno"; then
	tmp_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $JACK_CFLAGS"
	AC_CHECK_HEADERS([jack/session.h], AC_DEFINE(JACK_SESSION) JACK_SESSION=1)
	CPPFLAGS="$tmp_CPPFLAGS"
fi

tmp_LDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS $JACK_LDFLAGS"
AC_CHECK_LIB(jack, jack_port_get_latency_range, [AC_DEFINE(NEW_JACK_LATENCY_API) NEW_JACK_LATENCY_API=1])
LDFLAGS="$tmp_LDFLAGS"

dnl ---------------------------------------------------------------------------
dnl Check for ffmpeg: avformat/avcodec
PKG_CHECK_MODULES(FFMPEG, libavformat libavcodec libavutil libswscale, ,
	AC_MSG_ERROR([
	Could not find ffmpeg. Please set PKG_CONFIG_PATH to
	where FFMPEG installed its pkg files.
	If you have the choice prefer ffmpeg.org over libav.org.
	Both are supported but xjadeo is developed and tested with the former.
	]))
AC_SUBST(FFMPEG_CFLAGS)
AC_SUBST(FFMPEG_LIBS)


dnl ---------------------------------------------------------------------------
dnl remote control..
MQ_LIBS=""
if test "x$enable_mq" != "xno"; then
	AC_CHECK_LIB(rt, mq_open, [AC_DEFINE(HAVE_MQ) MQ_LIBS="-lrt -lpthread" HAVE_MQ=1])
fi
AC_SUBST(MQ_LIBS)
AC_DEFINE(MQLEN, 512, [ max length of a remote control line. ])

dnl prefer POSIX RT mqueue over IPC..
if test "x$enable_ipc" != "xno"; then
	if test -z "$HAVE_MQ"; then
		AC_CHECK_HEADERS([sys/ipc.h sys/msg.h] , AC_DEFINE(HAVE_IPCMSG) HAVE_IPCMSG=1 )
	fi
fi

AS_IF([test "x$enable_osc" != "xno"], [
	PKG_CHECK_MODULES(LIBLO, liblo, [AC_DEFINE(HAVE_LIBLO) HAVE_LIBLO=1], [NOFAIL=0])
])
AC_SUBST(LIBLO_CFLAGS)
AC_SUBST(LIBLO_LIBS)

dnl ---------------------------------------------------------------------------
dnl Display related

AS_IF([test -z "$PLATFORM_OSX$PLATFORM_WINDOWS"], [
	AC_CHECK_HEADERS([X11/Xlib.h], ,
		AC_MSG_ERROR([X11 is mandatory on this platform - install libx11/xcb development package.]))
	AC_CHECK_LIB([X11], [XOpenDisplay], [XV_LIBS="-lXext"],
		AC_MSG_ERROR([X11 is mandatory on this platform - install libx11/xcb development package.]))
	PKG_CHECK_MODULES(XPM, xpm, ,
		AC_MSG_ERROR([Xpm support is mandatory on this platform - install libxpm development package.]))
])

if test "x$enable_xv" != "xno"; then
	AC_CHECK_LIB(Xv, XvQueryAdaptors, [AC_DEFINE(HAVE_LIBXV) XV_LIBS="$XV_LIBS -lXv" HAVE_LIBXV=1 ], [], $X_LIBS)
fi
AC_SUBST(XV_LIBS)
AC_SUBST(XV_CFLAGS)

AS_IF([test "x$enable_sdl" != "xno"], [
	PKG_CHECK_MODULES(SDL, sdl, [AC_DEFINE(HAVE_SDL) HAVE_SDL=1], [NOFAIL=0])
])
AC_SUBST(SDL_LIBS)
AC_SUBST(SDL_CFLAGS)

AS_IF([test "x$enable_opengl" != "xno" -a -z "$PLATFORM_WINDOWS$PLATFORM_OSX"], [
	PKG_CHECK_MODULES(GL, glu gl, [AC_DEFINE(HAVE_GL) HAVE_GL=1], [NOFAIL=0])
])
AC_SUBST(GL_LIBS)
AC_SUBST(GL_CFLAGS)

AS_IF([test "x${enable_imlib2}" != "xno"], [
	PKG_CHECK_MODULES(IMLIB2, imlib2 >= 1.3.0,  [AC_DEFINE(HAVE_IMLIB2) HAVE_IMLIB2=1], [NOFAIL=0])
])
AC_SUBST(IMLIB2_CFLAGS)
AC_SUBST(IMLIB2_LIBS)


dnl ---------------------------------------------------------------------------
dnl midi lib detection
if test "x$enable_midi" != "xno"; then
	MIDI_CFLAGS=""
	MIDI_CFLAGS=""
	if test "x$enable_portmidi" != "xno"; then
		AC_CHECK_HEADERS(portmidi.h porttime.h , [FOUND_H_PORTMIDI=yes])
		AC_CHECK_LIB([portmidi], [Pm_CountDevices], [FOUND_L_PORTMIDI=yes])
	fi
	if test "x$FOUND_H_PORTMIDI$FOUND_L_PORTMIDI" = "xyesyes"; then
		MIDI_LIBS="-lportmidi"
dnl TODO check if porttime is needed for threading
		if test "$PM_OS" != "mac"; then
		  MIDI_LIBS="${MIDI_LIBS} -lporttime"
	  fi
	fi

	if test "x$JACKMIDI" != "xno"; then
		tmp_CPPFLAGS="$CPPFLAGS"
		CPPFLAGS="$CPPFLAGS $JACK_CFLAGS"
		AC_CHECK_HEADERS(jack/midiport.h, [FOUND_H_JACKMIDI=yes])
		CPPFLAGS=$tmp_CPPFLAGS
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

dnl ---------------------------------------------------------------------------
dnl Check for freetype (On Screen Display)
FREETYPE_LIBS=
FREETYPE_CFLAGS=
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
AC_SUBST(FREETYPE_CFLAGS)
AC_SUBST(FREETYPE_LIBS)


if test -n "$fontfile" ; then
	# I'm not sure if this is the proper way to escape a #define string.
	# AC_DEFINE(TTFFONTFILE, "\"$fontfile\"") - no
	# FREETYPE_CFLAGS="$FREETYPE_CFLAGS -DTTFFONTFILE=\\\"$fontfile\\\"" - maybe
	dnl hack alert - we 'should' use our own .h file for this
	dnl this might not work with some autoconf's
	echo "#define TTFFONTFILE \"$fontfile\"" >> confdefs.h
fi

dnl ---------------------------------------------------------------------------
dnl https://github.com/x42/libltc
AS_IF([ test "x$enable_ltc" != "xno"], [
	PKG_CHECK_MODULES(LTC, ltc, [AC_DEFINE(HAVE_LTC) HAVE_LTC=1], [NOFAIL=0] )
])
AC_SUBST(LTC_CFLAGS)
AC_SUBST(LTC_LIBS)

dnl ---------------------------------------------------------------------------
dnl Qt GUI stuff (unmaintained)
if test "$enable_qtgui" = "yes"; then

	if test "x$qt4prefix" = "x"; then
		qt4prefix=$($PKG_CONFIG --variable=prefix QtGui)
	fi

	# Check for Qt qmake utility.
	AC_PATH_PROG(ac_qmake, qmake, [no], $qt4prefix/bin)
	AC_PATH_PROG(ac_lrelease, lrelease, [no], $qt4prefix/bin)
	if test "x$ac_qmake" = "xno" -o "x$ac_lrelease" = "xno" ; then
		AC_MSG_ERROR([qmake not found: please install the Qt4 development
		              package of your distribution. Try setting --with-qt4prefix=QTROOT
		              or use --disable-qtgui to compile xjadeo.])
	fi

	HAVE_QT4=1

	if test "x$qmakeargs" = "x"; then
		if test -n "$PLATFORM_WINDOWS"; then
			ac_qmake_args="-spec win32-x-g++ -config release"
		else
			ac_qmake_args="-config release"
		fi
	else
		ac_qmake_args="$qmakeargs"
	fi
	AC_SUBST(ac_qmake)
	AC_SUBST(ac_qmake_args)
	AC_SUBST(ac_lrelease)

	ac_qtgui_dir="qt-gui"

	AC_CONFIG_FILES(src/qt-gui/Makefile src/qt-gui/qjadeo.pro)

else
	ac_qtgui_dir=""
fi

AC_SUBST(ac_qtgui_dir)

dnl ---------- End of Qt GUI stuff ---------------

ac_contrib_dir=""

if test "x$enable_contrib" = "xyes"; then
	ac_contrib_dir="contrib/cli-remote/"
fi

AC_SUBST(ac_contrib_dir)

if test "x$enable_contrib" = "xyes"; then
	AC_CONFIG_FILES([contrib/cli-remote/Makefile])
fi

dnl ---------------------------------------------------------------------------
dnl We're done!

AC_CONFIG_FILES([Makefile src/Makefile src/xjadeo/Makefile xjadeo.lsm])

dnl write a niiiice report
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
	RCTLREPORT="N/A (stdio only)";
fi

if test -z "$HAVE_FT"; then
	OSDREPORT="disabled"
else
	OSDREPORT="
   - freetype:       yes
   - custom font:    "
	if test -n "$fontfile" ; then
		OSDREPORT="${OSDREPORT}$fontfile";
	else
		OSDREPORT="${OSDREPORT}-"
	fi
	if test "x$enable_embed_font" == "xyes"; then
		OSDREPORT="${OSDREPORT}
   - embed font:     yes"
	 else
		OSDREPORT="${OSDREPORT}
   - embed font:     no"
	fi
fi

CONTREPORT="no"
if test "x$enable_contrib" = "xyes"; then
	CONTREPORT="yes"
fi

if test -n "$HAVE_GL"; then RPT_OPENGL="yes"; else RPT_OPENGL="not avail."; fi
if test -n "$HAVE_LIBXV"; then RPT_LIBXV="yes"; else RPT_LIBXV="not avail."; fi
if test -n "$HAVE_IMLIB2"; then RPT_IMLIB2="yes"; else RPT_IMLIB2="not avail."; fi
if test -n "$PLATFORM_OSX"; then RPT_MACOSX="yes"; else RPT_MACOSX="not avail."; fi
if test -n "$HAVE_SDL"; then RPT_SDL="yes"; else RPT_SDL="not avail."; fi
if test -n "$HAVE_LIBLO"; then RPT_LIBLO="yes"; else RPT_LIBLO="not avail."; fi
if test -n "$HAVE_QT4"; then RPT_QT4="yes"; else RPT_QT4="no"; fi
if test -n "$PLATFORM_OSX"; then RPT_QT4="${RPT_QT4} (OSX-built-in)"; fi
if test -n "$JACK_SESSION"; then RPT_JACKSESSION="yes"; else RPT_JACKSESSION="not avail."; fi
if test -n "$NEW_JACK_LATENCY_API"; then RPT_JACK_LATENCY="yes"; else RPT_JACK_LATENCY="not avail."; fi
if test -n "$HAVE_LTC"; then RPT_LTC="yes"; else RPT_LTC="not avail."; fi

AC_MSG_NOTICE([

------------------------------------------------------------------------
  $PACKAGE $VERSION  - Configuration Report
------------------------------------------------------------------------

 prefix:             $prefix
 exec-prefix:        $exec_prefix
 install path:       $bindir
 System wide config: $sysconfdir/xjadeorc

 Video Displays
   - openGL:         $RPT_OPENGL
   - libxv:          $RPT_LIBXV
   - libx11/imlib2:  $RPT_IMLIB2
   - mac/quartz:     $RPT_MACOSX
   - SDL:            $RPT_SDL

 Sync:
   - JACK-transport: yes
   - LTC:            $RPT_LTC
   - MIDI:           $MIDIREPORT

 JACK-session:       $RPT_JACKSESSION
 JACK-latency-api:   $RPT_JACK_LATENCY
 remote control:     $RCTLREPORT
 OSC remote control: $RPT_LIBLO

 On-screen-display:  $OSDREPORT

 GUI (qjadeo):       $RPT_QT4
 build contrib/*:    $CONTREPORT

])

if test "x$enable_contrib" = "xyes"; then
AC_MSG_NOTICE([
 Note for Packagers: --enable-contrib does not make a difference when packaging.
 All programs in contrib/ are noinst targets. They are intended for advanced
 users, developers and testing only.
])
fi

if test -n "$PLATFORM_LINUX" -a -n "$HAVE_PORTMIDI"; then
AC_MSG_NOTICE([
 PortMidi is intended for non Un*x Platforms only.
 On Linux, please use JACK, ALSA-seq or ALSA-raw.
])
fi

if test -n "$PLATFORM_LINUX$PLATFORM_OSX" -a -n "$HAVE_QT4"; then
AC_MSG_WARN([
 qjadeo is deprecated an not intended to be used.
 It will be removed in future releases.
 Xjadeo now features a complete built-in user interface.
 Do not use --enable-qtgui unless you really know what you are doing.
])
fi

AC_MSG_NOTICE([
 type "make" followed my "make install" as root.
])
