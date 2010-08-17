#!/bin/sh
#NORECONF=1

unset CC
if test -z "$NORECONF"; then
PKG_CONFIG_PATH=/home/rgareus/.wine/drive_c/x-prefix/lib/pkgconfig/ CFLAGS="-I/home/rgareus/.wine/drive_c/x-prefix/include -I.." LDFLAGS="-L/home/rgareus/.wine/drive_c/x-prefix/lib/ -L/home/rgareus/.wine/drive_c/x-prefix/bin" \
	./configure --host=i586-mingw32msvc --build=i386-linux --prefix=/home/rgareus/.wine/drive_c/x-prefix/ \
	--disable-xv --disable-imlib2 --disable-lash --disable-mq --disable-ipc --disable-osc \
	--with-fontfile=./FreeMonoBold.ttf \
	|| exit

# TODO FONTFILE 
#--sysconfdir=./ \
fi

NSIDIR=contrib/nsi/

make -C src/xjadeo xjadeo.exe || exit
cp -v src/xjadeo/xjadeo.exe $NSIDIR

WINEBIN=/home/rgareus/.wine/drive_c/x-prefix/bin/
cp -v src/xjadeo/xjadeo.exe $NSIDIR
cp -v $WINEBIN/avcodec-52.dll $NSIDIR
cp -v $WINEBIN/avformat-52.dll $NSIDIR
cp -v $WINEBIN/avutil-49.dll $NSIDIR
cp -v $WINEBIN/swscale-0.dll $NSIDIR
cp -v $WINEBIN/freetype6.dll $NSIDIR
cp -v $WINEBIN/SDL.dll $NSIDIR
cp -v $WINEBIN/zlib1.dll $NSIDIR
cp -v contrib/Jadeo.app/Contents/Resources/FreeMonoBold.ttf $NSIDIR


make -C src/qt-gui || exit
cp -v src/qt-gui/release/qjadeo.exe $NSIDIR
QTBIN=/home/rgareus/.wine/drive_c/Qt/2010.04/qt/bin
cp -v $QTBIN/QtGui4.dll $NSIDIR
cp -v $QTBIN/QtCore4.dll $NSIDIR
cp -v $QTBIN/QtTest4.dll $NSIDIR
cp -v $QTBIN/libgcc_s_dw2-1.dll $NSIDIR
cp -v $QTBIN/mingwm10.dll $NSIDIR

VERSION=$(grep " VERSION " config.h | cut -d ' ' -f3 | sed 's/"//g'| sed 's/\./_/g')
echo $VERSION
cat $NSIDIR/xjadeo.nsi.tpl | sed 's/VERSION/'$VERSION'/' > $NSIDIR/xjadeo.nsi
#qmake -spec win32-x-g++ -config release \
#	&& make clean \
#	&& make -j2 \
#	&& wine /home/rgareus/.wine/drive_c/Program\ Files/NSIS/makensis.exe release/movemovie.nsi
wine /home/rgareus/.wine/drive_c/Program\ Files/NSIS/makensis.exe $NSIDIR/xjadeo.nsi
