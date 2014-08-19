#!/bin/bash

# NORECONF=1 NSIDIR=build/win/ ./x-win-bundle.sh

: ${WINPREFIX="$HOME/.wine/drive_c/x-prefix"}
: ${WINLIB="$WINPREFIX/bin/"}
: ${NSISEXE="$HOME/.wine/drive_c/Program Files/NSIS/makensis.exe"}

set -e

if test -z "$NSIDIR"; then
	NSIDIR=$(mktemp -d)
	trap 'rm -rf $NSIDIR' exit
else
	mkdir -p "$NSIDIR"
fi

unset CC
if test -z "$NORECONF"; then
	PKG_CONFIG_PATH=$WINPREFIX/lib/pkgconfig/ \
	CPPFLAGS="-I$WINPREFIX/include" \
	LDFLAGS="-L$WINPREFIX/lib/ -L$WINLIB" \
	./configure --host=i686-w64-mingw32 --build=i386-linux --prefix="" \
		--disable-xv --disable-imlib2 --disable-mq --disable-ipc --disable-osc \
		--with-fontfile=ArdourMono.ttf \
		$@
fi

#make clean
make -C src/xjadeo paths.h
make -C src/xjadeo xjadeo.exe
cp -v src/xjadeo/xjadeo.exe "$NSIDIR"
cp -v src/xjadeo/icons/xjadeo_win.ico "$NSIDIR"/xjadeo.ico

cp -v $WINLIB/avcodec-55.dll "$NSIDIR"
cp -v $WINLIB/avformat-55.dll "$NSIDIR"
cp -v $WINLIB/avutil-52.dll "$NSIDIR"
cp -v $WINLIB/swscale-2.dll "$NSIDIR"
cp -v $WINLIB/libogg-0.dll "$NSIDIR"
cp -v $WINLIB/libtheora-0.dll "$NSIDIR"
cp -v $WINLIB/libtheoradec-1.dll "$NSIDIR"
cp -v $WINLIB/libtheoraenc-1.dll "$NSIDIR"
cp -v $WINLIB/libfreetype-6.dll "$NSIDIR"
cp -v $WINLIB/zlib1.dll "$NSIDIR"
cp -v $WINLIB/pthreadGC2.dll "$NSIDIR"
cp -v $WINLIB/libltc-11.dll "$NSIDIR"
cp -v $WINLIB/libiconv-2.dll "$NSIDIR"
cp -v $WINLIB/libx264-142.dll "$NSIDIR"
cp -v $WINLIB/libportmidi-0.dll "$NSIDIR"
cp -v $WINLIB/libporttime-0.dll "$NSIDIR"

cp -v src/xjadeo/fonts/ArdourMono.ttf "$NSIDIR"

VERSION=$(grep " VERSION " config.h | cut -d ' ' -f3 | sed 's/"//g'| sed 's/\./_/g')
echo $VERSION

sed 's/VERSION/'$VERSION'/' \
	contrib/pkg-win/xjadeo.nsi.tpl \
	> "$NSIDIR"/xjadeo.nsi

#XXX winepath should probably be moved into a wrapper script "$NSISEXE"
WP=`winepath -w "$NSIDIR/xjadeo.nsi"`
echo
echo "$NSISEXE" "$WP"
"$NSISEXE" "$WP"

cp -v "$NSIDIR/xjadeo_installer_v$VERSION.exe" /tmp/
ls -lt "/tmp/xjadeo_installer_v$VERSION.exe" | head -n 1
