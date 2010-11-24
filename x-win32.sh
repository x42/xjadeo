#!/bin/sh
#NORECONF=1

WINEBASEDIR=/home/rgareus/.wine/drive_c/x-prefix
NSISEXE=/home/rgareus/.wine/drive_c/Program\ Files/NSIS/makensis.exe
unset CC
if test -z "$NORECONF"; then
PKG_CONFIG_PATH=$WINEBASEDIR/lib/pkgconfig/ CFLAGS="-I$WINEBASEDIR/include -I.." LDFLAGS="-L$WINEBASEDIR/lib/ -L$WINEBASEDIR/bin" \
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
make -C src/xjadeo xjinfo.exe || exit
cp -v src/xjadeo/xjinfo.exe $NSIDIR

WINEBIN=$WINEBASEDIR/bin/
cp -v src/xjadeo/xjadeo.exe $NSIDIR
cp -v $WINEBIN/avcodec-52.dll $NSIDIR
cp -v $WINEBIN/avformat-52.dll $NSIDIR
cp -v $WINEBIN/avutil-49.dll $NSIDIR
cp -v $WINEBIN/swscale-0.dll $NSIDIR
cp -v $WINEBIN/freetype6.dll $NSIDIR
cp -v $WINEBIN/SDL.dll $NSIDIR
cp -v $WINEBIN/zlib1.dll $NSIDIR
cp -v $WINEBIN/libltcsmpte-0.dll $NSIDIR
cp -v contrib/Jadeo.app/Contents/Resources/FreeMonoBold.ttf $NSIDIR


make -C src/qt-gui || exit
cp -v src/qt-gui/release/qjadeo.exe $NSIDIR
cp -v src/qt-gui/qjadeo_fr.qm $NSIDIR
cp -v src/qt-gui/qjadeo_ru.qm $NSIDIR
QTBIN=/home/rgareus/.wine/drive_c/Qt/2010.04/qt/bin
cp -v $QTBIN/QtGui4.dll $NSIDIR
cp -v $QTBIN/QtCore4.dll $NSIDIR
cp -v $QTBIN/QtSvg4.dll $NSIDIR
cp -v $QTBIN/QtTest4.dll $NSIDIR
cp -v $QTBIN/libgcc_s_dw2-1.dll $NSIDIR
cp -v $QTBIN/mingwm10.dll $NSIDIR

VERSION=$(grep " VERSION " config.h | cut -d ' ' -f3 | sed 's/"//g'| sed 's/\./_/g')
echo $VERSION
cat $NSIDIR/xjadeo.nsi.tpl | sed 's/VERSION/'$VERSION'/' > $NSIDIR/xjadeo.nsi
wine "$NSISEXE" $NSIDIR/xjadeo.nsi

echo scp contrib/nsi/jadeo_installer_v$VERSION.exe  rg42.org:/var/sites/robwiki/data/media/oss/xjadeo/
