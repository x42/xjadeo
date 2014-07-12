#!/bin/sh
#NORECONF=1

#path to the static ffmpeg installation
: ${PFX=$HOME/local}
#path to output directory -- /xjadeo*.tgz will end up there
: ${RESULT=/tmp}

LIBF=$PFX/lib
BINF=$PFX/bin
export PKG_CONFIG_PATH=${LIBF}/pkgconfig

if test -z "$NORECONF"; then
	./configure \
		--disable-lash --disable-mq --disable-ipc \
		--disable-qtgui --disable-jacksession --disable-portmidi --disable-alsamidi \
	  --with-fontfile=/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf \
	|| exit
fi

make -C src/xjadeo xjadeo

SRCVERSION=$(grep " VERSION " config.h | cut -d ' ' -f3 | sed 's/"//g')
VERSION=$(git describe --tags HEAD)
TRIPLET=$(gcc -print-multiarch)
echo $VERSION
OUTFN=xjadeo-$TRIPLET-$VERSION

# ffmpeg needs this libs
LIBDEPS=" \
 libpng12.a \
 libjpeg.a \
 libmp3lame.a \
 libspeex.a \
 libtheoraenc.a \
 libtheoradec.a \
 libogg.a \
 libvorbis.a \
 libvorbisenc.a \
 libvorbisfile.a \
 libschroedinger-1.0.a \
 liborc-0.4.a \
 libgsm.a \
 libbluray.a \
 libxvidcore.a \
 libopus.a \
 libbz2.a \
 libvpx.a \
 libopenjpeg.a \
 libx264.a \
 libz.a \
 libImlib2.a \
 libfreetype.a \
 libltc.a \
 liblo.a \
 libSDL.a \
 libXpm.a \
 libXv.a \
 libXext.a \
 libXrandr.a \
 libXrender.a \
 libdirectfb.a \
 libfusion.a \
 libdirect.a \
 libICE.a \
 "

# resolve paths to static libs on the system
SLIBS=""
for SLIB in $LIBDEPS; do
	echo "searching $SLIB.."
	SL=`find /usr/lib -name "$SLIB"`
	if test -z "$SL"; then
		echo "not found."
		exit 1
	fi
	SLIBS="$SLIBS $SL"
done


cd src/xjadeo
rm -f $OUTFN
gcc -Wall -O3 \
 -o $OUTFN \
 -DHAVE_CONFIG_H \
	"-DSUBVERSION=\"$SRCVERSION\"" \
	`pkg-config --cflags freetype2 jack` \
	 xjadeo-xjadeo.o xjadeo-display.o xjadeo-jack.o xjadeo-midi.o xjadeo-freetype.o xjadeo-display_x11.o \
	 xjadeo-smpte.o xjadeo-main.o xjadeo-remote.o xjadeo-configfile.o xjadeo-lash.o xjadeo-mqueue.o \
	 xjadeo-display_mac.o xjadeo-xjosc.o xjadeo-display_sdl.o xjadeo-ltc-jack.o \
	${LIBF}/libavformat.a \
	${LIBF}/libavcodec.a \
	${LIBF}/libswscale.a \
	${LIBF}/libavdevice.a \
	${LIBF}/libavutil.a \
	\
	$SLIBS \
	`pkg-config --libs jack` -lpthread -lm -lX11 \
|| exit 1

strip $OUTFN
ls -lh $OUTFN
ldd $OUTFN

# give any arg to disable bundle
test -n "$1" && exit 1

# build .tgz bundle
rm -rf $RESULT/$OUTFN $RESULT/$OUTFN.tgz
mkdir $RESULT/$OUTFN
cp $OUTFN $RESULT/$OUTFN/xjadeo
cp ../../doc/xjadeo.1 $RESULT/$OUTFN/xjadeo.1
cd $RESULT/ ; tar czf $RESULT/$OUTFN.tgz $OUTFN ; cd -
rm -rf $RESULT/$OUTFN
ls -lh $RESULT/$OUTFN.tgz
