#!/bin/bash

# NORECONF=1 NSIDIR=build/win/ ./x-win-bundle.sh

: ${WINPREFIX="$HOME/.wine/drive_c/x-prefix"}
: ${WINLIB="$WINPREFIX/bin/"}

test -f "$HOME/.xjbuildcfg.sh" && . "$HOME/.xjbuildcfg.sh"

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
	CFLAGS="-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64" \
	LDFLAGS="-L$WINPREFIX/lib/ -L$WINLIB" \
	./configure --host=i686-w64-mingw32 --build=i386-linux --prefix="" \
		--disable-xv --disable-imlib2 --disable-mq --disable-ipc \
		--with-fontfile=ArdourMono.ttf \
		$@
fi

#make clean
make -C src/xjadeo paths.h
make -C src/xjadeo xjadeo.exe
cp -v src/xjadeo/xjadeo.exe "$NSIDIR"
cp -v src/xjadeo/icons/xjadeo_win.ico "$NSIDIR"/xjadeo.ico

ffdlls="avcodec- avformat- avutil- libfreetype- libiconv- liblo- libltc- libportmidi- libporttime- pthreadGC2 swscale- swresample- zlib1"
for fname in $ffdlls; do
	cp -v ${WINPREFIX}/bin/${fname}*.dll $NSIDIR
done

cp -v src/xjadeo/fonts/ArdourMono.ttf "$NSIDIR"

GITVERSION=$(git describe --tags HEAD)
VERSION=$(grep " VERSION " config.h | cut -d ' ' -f3 | sed 's/"//g'| sed 's/\./_/g')
echo $VERSION

TARDIR=$(mktemp -d)
cd $TARDIR
ln -s $NSIDIR xjadeo
tar cJhf /tmp/xjadeo_win-$GITVERSION.tar.xz xjadeo
rm -rf $TARDIR
cd -

sed 's/VERSION/'$VERSION'/' \
	contrib/pkg-win/xjadeo.nsi.tpl \
	> "$NSIDIR"/xjadeo.nsi

echo "makensis $NSIDIR/xjadeo.nsi"
makensis "$NSIDIR/xjadeo.nsi"

cp -v "$NSIDIR/xjadeo_installer_v$VERSION.exe" /tmp/
ls -lt "/tmp/xjadeo_installer_v$VERSION.exe" | head -n 1
ls -lt "/tmp/xjadeo_win-$GITVERSION.tar.xz" | head -n 1
