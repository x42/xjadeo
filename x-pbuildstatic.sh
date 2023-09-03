#!/bin/bash
# this script creates a statically linked version of xjadeo
#
# It is intended to run in a pristine chroot or VM of a minimal
# debian system. see http://wiki.debian.org/cowbuilder
#
# This script
#  - git clone the source to $SRC (default /usr/src)
#  - build and install ffmpeg to $PFX (default ~/local/)
#  - build xjadeo and bundle it to /tmp/xjadeo*.tgz
#    (/tmp/ is fixed set by x-static.sh )
#

#use environment variables if set for SRC and PFX
: ${SRC=/usr/src}
: ${SRCDIR=/tmp/winsrc}
: ${PFX=$HOME/local}

test -f "$HOME/.xjbuildcfg.sh" && . "$HOME/.xjbuildcfg.sh"

if [ "$(id -u)" != "0" -a -z "$SUDO" ]; then
	echo "This script must be run as root in pbuilder" 1>&2
  echo "e.g sudo cowbuilder --architecture amd64 --distribution wheezy --bindmounts /tmp --execute $0"
	exit 1
fi

$SUDO apt-get update
$SUDO apt-get install -y --force-yes \
	libbz2-1.0=1.0.6-7+b3 libexpat1=2.1.0-6+deb8u4 zlib1g=1:1.2.8.dfsg-2+b1 \
	libdbus-1-3=1.8.22-0+deb8u1 libtasn1-6=4.2-3+deb8u3 libgnutls-deb0-28=3.3.8-6+deb8u7 \
	libgnutls-deb0-28=3.3.8-6+deb8u7 libgnutls-deb0-28=3.3.8-6+deb8u7

$SUDO apt-get -y install git build-essential yasm \
	libass-dev libbluray-dev libgmp3-dev liblzma-dev\
	libbz2-dev libfreetype6-dev libgsm1-dev liblzo2-dev \
	libmp3lame-dev librtmp-dev libxml2-dev \
	libspeex-dev libtheora-dev \
	libvorbis-dev libx264-dev \
	libxvidcore-dev zlib1g-dev \
	libpng-dev libjpeg-dev \
	libxv-dev libjack-jackd2-dev libx11-dev  libfreetype6-dev \
	libxpm-dev liblo-dev autoconf automake \
	curl wget libxrandr-dev libglu-dev libimlib2-dev \
	libdirectfb-dev libice-dev nasm

mkdir -p ${SRCDIR}

function download {
echo "--- Downloading.. $2"
test -f ${SRCDIR}/$1 || curl -k -L -o ${SRCDIR}/$1 $2
}

cd $SRC
GIT_SSL_NO_VERIFY=true git clone -b master --single-branch https://github.com/x42/xjadeo.git

FFVERSION=5.0
download ffmpeg-${FFVERSION}.tar.bz2 http://www.ffmpeg.org/releases/ffmpeg-${FFVERSION}.tar.bz2
download SDL-1.2.15.tar.gz http://www.libsdl.org/release/SDL-1.2.15.tar.gz
download libltc-1.3.1.tar.gz https://github.com/x42/libltc/releases/download/v1.3.1/libltc-1.3.1.tar.gz

tar xjf ${SRCDIR}/ffmpeg-${FFVERSION}.tar.bz2
tar xzf ${SRCDIR}/SDL-1.2.15.tar.gz
tar xzf ${SRCDIR}/libltc-1.3.1.tar.gz


cd $SRC/xjadeo
VERSION=$(git describe --tags HEAD)
git archive --format=tar --prefix=xjadeo-${VERSION}/ HEAD | gzip -9 > /tmp/xjadeo-${VERSION}.tar.gz

cd $SRC/ffmpeg-${FFVERSION}/

./configure --enable-gpl --disable-programs --disable-debug --disable-doc \
	--enable-libmp3lame --enable-libx264 --enable-libxvid --enable-libtheora  --enable-libvorbis \
	--enable-libspeex --enable-libbluray --enable-libgsm \
	--disable-vaapi --disable-devices \
	--extra-cflags="-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64" \
	--enable-shared --enable-static --prefix=$PFX $@

make -j4 || exit 1
make install || exit 1

cd $SRC/SDL-1.2.15
./configure --prefix=/usr \
	--enable-static --disable-shared --disable-rpath \
	--disable-assembly \
	--disable-audio \
	--disable-x11-shared --disable-input-tslib \
	--enable-video-directfb --enable-video-x11-xrandr \
	--disable-joystick --disable-cdrom --disable-loadso \
	--disable-video-ggi --disable-video-svga --disable-video-aalib
make -j4
# hack to allow 'normal' make build w/o errror (irrelevant for static builds, but hey)
sed -i 's/^Libs:.*$/Libs: -L${libdir}  -lSDL  -lpthread -lXrandr -ldirectfb/' sdl.pc
make install

cd $SRC/libltc-1.3.1
./configure --prefix=/usr
make install

cd $SRC/xjadeo
autoreconf -i
./x-static.sh || exit 1

ls -l /tmp/xjadeo*.tgz
