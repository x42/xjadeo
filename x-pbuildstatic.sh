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

if [ "$(id -u)" != "0" -a -z "$SUDO" ]; then
	echo "This script must be run as root in pbuilder" 1>&2
  echo "e.g sudo cowbuilder --architecture amd64 --distribution wheezy --bindmounts /tmp --execute $0"
	exit 1
fi

$SUDO apt-get -y install git build-essential yasm \
	libass-dev libbluray-dev libgmp3-dev \
	libbz2-dev libfreetype6-dev libgsm1-dev liblzo2-dev \
	libmp3lame-dev libopenjpeg-dev libopus-dev librtmp-dev \
	libschroedinger-dev libspeex-dev libtheora-dev \
	libvorbis-dev libvpx-dev libx264-dev \
	libxvidcore-dev zlib1g-dev zlib1g-dev \
	libpng12-dev libjpeg8-dev \
	libxv-dev libjack-jackd2-dev libx11-dev  libfreetype6-dev \
	libltc-dev libxpm-dev liblo-dev autoconf automake

cd $SRC
git clone -b release/1.2 --depth 1 git://source.ffmpeg.org/ffmpeg
git clone -b master git://github.com/x42/xjadeo.git

cd $SRC/xjadeo
VERSION=$(git describe --tags HEAD)
git archive --format=tar --prefix=xjadeo-${VERSION}/ HEAD | gzip -9 > /tmp/xjadeo-${VERSION}.tar.gz

cd $SRC/ffmpeg
#FFVERSION=$(git describe --tags)
#git archive --format=tar --prefix=ffmpeg-${FFVERSION}/ HEAD | gzip -9 > /tmp/ffmpeg-${FFVERSION}.tar.gz

./configure --enable-gpl \
	--enable-libmp3lame --enable-libx264 --enable-libxvid --enable-libtheora  --enable-libvorbis \
	--enable-libvpx --enable-libopenjpeg --enable-libopus --enable-libschroedinger \
	--enable-libspeex --enable-libbluray --enable-libgsm \
	--disable-vaapi --disable-x11grab \
	--disable-devices \
	--enable-shared --enable-static --prefix=$PFX $@

make -j4 || exit 1
make install || exit 1

cd $SRC/xjadeo
autoreconf -i
./x-static.sh || exit 1

ls -l /tmp/xjadeo*.tgz
