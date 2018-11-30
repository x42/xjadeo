#!/bin/sh
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
: ${PFX=$HOME/local}

test -f "$HOME/.xjbuildcfg.sh" && . "$HOME/.xjbuildcfg.sh"

if [ "$(id -u)" != "0" -a -z "$SUDO" ]; then
	echo "This script must be run as root in pbuilder" 1>&2
  echo "e.g sudo cowbuilder --architecture amd64 --distribution wheezy --bindmounts /tmp --execute $0"
	exit 1
fi

$SUDO apt-get -y install git build-essential yasm \
	libass-dev libbluray-dev libgmp3-dev \
	libbz2-dev libfreetype6-dev libgsm1-dev liblzo2-dev \
	libmp3lame-dev libopenjpeg-dev librtmp-dev \
	libspeex-dev libtheora-dev \
	libvorbis-dev libvpx-dev libx264-dev \
	libxvidcore-dev zlib1g-dev zlib1g-dev \
	libpng12-dev libjpeg8-dev \
	libxv-dev libjack-jackd2-dev libx11-dev  libfreetype6-dev \
	libltc-dev libxpm-dev liblo-dev autoconf automake \
	wget libxrandr-dev libglu-dev libimlib2-dev \
	libdirectfb-dev libice-dev nasm


cd $SRC
git clone -b master --single-branch git://github.com/x42/xjadeo.git

FFVERSION=2.8.2
#git clone -b release/${FFVERSION} --depth 1 git://source.ffmpeg.org/ffmpeg-${FFVERSION}
if test -f /tmp/ffmpeg-${FFVERSION}.tar.bz2; then
	tar xjf /tmp/ffmpeg-${FFVERSION}.tar.bz2
else
	wget http://www.ffmpeg.org/releases/ffmpeg-${FFVERSION}.tar.bz2
	tar xjf ffmpeg-${FFVERSION}.tar.bz2
fi
if test -f /tmp/SDL-1.2.15.tar.gz; then
	tar xzf /tmp/SDL-1.2.15.tar.gz
else
	wget http://www.libsdl.org/release/SDL-1.2.15.tar.gz
	tar xzf SDL-1.2.15.tar.gz
fi

cd $SRC/xjadeo
VERSION=$(git describe --tags HEAD)
git archive --format=tar --prefix=xjadeo-${VERSION}/ HEAD | gzip -9 > /tmp/xjadeo-${VERSION}.tar.gz

cd $SRC/ffmpeg-${FFVERSION}/
#git archive --format=tar --prefix=ffmpeg-${FFVERSION}/ HEAD | gzip -9 > /tmp/ffmpeg-${FFVERSION}.tar.gz

./configure --enable-gpl \
	--enable-libmp3lame --enable-libx264 --enable-libxvid --enable-libtheora  --enable-libvorbis \
	--enable-libvpx --enable-libopenjpeg \
	--enable-libspeex --enable-libbluray --enable-libgsm \
	--disable-vaapi \
	--disable-devices \
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

cd $SRC/xjadeo
autoreconf -i
./x-static.sh || exit 1

ls -l /tmp/xjadeo*.tgz
