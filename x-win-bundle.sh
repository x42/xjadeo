#!/bin/sh
#NORECONF=1

: ${WINPREFIX="$HOME/.wine/drive_c/x-prefix"}
: ${WINLIB="$WINPREFIX/bin/"}
: ${QTDLL="$HOME/.wine/drive_c/Qt/2010.04/qt/bin"}
: ${QTSPECPATH=win32-x-g++}
: ${NSISEXE="$HOME/.wine/drive_c/Program Files/NSIS/makensis.exe"}
: ${NSIDIR=contrib/nsi/}

set -e

# XXX if NSIDIR is empty, we should use mktemp here and clean up..
mkdir -p "$NSIDIR"

unset CC
if test -z "$NORECONF"; then
	PKG_CONFIG_PATH=$WINPREFIX/lib/pkgconfig/ \
	CPPFLAGS="-I$WINPREFIX/include" \
	LDFLAGS="-L$WINPREFIX/lib/ -L$WINLIB" \
	./configure --host=i686-w64-mingw32 --build=i386-linux --prefix="" \
		--disable-xv --disable-imlib2 --disable-mq --disable-ipc --disable-osc --enable-qtgui \
		--with-fontfile=ArdourMono.ttf \
		--with-qmakeargs="-spec ${QTSPECPATH} -config release" \
		$@
fi

#make clean
make -C src/xjadeo paths.h
make -C src/xjadeo xjadeo.exe
cp -v src/xjadeo/xjadeo.exe "$NSIDIR"
cp -v src/xjadeo/icons/xjadeo-color.ico "$NSIDIR"/xjadeo.ico

cp -v $WINLIB/avcodec-54.dll "$NSIDIR"
cp -v $WINLIB/avformat-54.dll "$NSIDIR"
cp -v $WINLIB/avutil-51.dll "$NSIDIR"
cp -v $WINLIB/swscale-2.dll "$NSIDIR"

cp -v $WINLIB/freetype6.dll "$NSIDIR"
cp -v $WINLIB/SDL.dll "$NSIDIR"
cp -v $WINLIB/zlib1.dll "$NSIDIR"
cp -v $WINLIB/pthreadVSE2.dll "$NSIDIR"
cp -v $WINLIB/libltc-11.dll "$NSIDIR"

cp -v contrib/Jadeo.app/Contents/Resources/ArdourMono.ttf "$NSIDIR"

make -C src/qt-gui
cp -v src/qt-gui/release/qjadeo.exe "$NSIDIR"
cp -v src/qt-gui/qjadeo_fr.qm "$NSIDIR"
cp -v src/qt-gui/qjadeo_ru.qm "$NSIDIR"
cp -v src/qt-gui/qjadeo_el_GR.qm "$NSIDIR"
cp -v src/qt-gui/qjadeo_cs.qm "$NSIDIR"

cp -v $QTDLL/QtGui4.dll "$NSIDIR"
cp -v $QTDLL/QtCore4.dll "$NSIDIR"
cp -v $QTDLL/QtSvg4.dll "$NSIDIR"
cp -v $QTDLL/QtTest4.dll "$NSIDIR"
cp -v $QTDLL/libgcc_s_dw2-1.dll "$NSIDIR"
cp -v $QTDLL/mingwm10.dll "$NSIDIR"

VERSION=$(grep " VERSION " config.h | cut -d ' ' -f3 | sed 's/"//g'| sed 's/\./_/g')
echo $VERSION

sed 's/VERSION/'$VERSION'/' \
	contrib/nsi/xjadeo.nsi.tpl \
	> "$NSIDIR"/xjadeo.nsi

#XXX winepath should probably be moved into a wrapper script "$NSISEXE"
WP=`winepath -w "$NSIDIR/xjadeo.nsi"`
echo
echo "$NSISEXE" "$WP"
"$NSISEXE" "$WP"

cp -v "$NSIDIR/jadeo_installer_v$VERSION.exe" /tmp/
ls -lt "/tmp/jadeo_installer_v$VERSION.exe" | head -n 1
