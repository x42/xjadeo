#!/bin/bash
set -e

# re-use x42-stack for build-tools
test -f $HOME/src/rtk_stack/robtk.lv2.stack
test -d $HOME/src/rtk_build/x/bin

export PATH="$HOME/src/rtk_build/x/bin:$HOME/src/rtk_stack/bin/:$HOME/gtk/tool/bin/:/Users/rgareus/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:/Library/Apple/usr/bin"
export ACLOCAL_PATH=$HOME/src/rtk_stack/share/aclocal
export PKG_CONFIG_PATH="$HOME/src/rtk_stack/lib/pkgconfig:$HOME/src/xj/stack/lib/pkgconfig"

MAKEFLAGS="-j2"

SRCDIR=$HOME/src/src_cache
BUILDD=$HOME/src/xj/build
PREFIX=$HOME/src/xj/stack

MLARCH="-arch arm64"
OSXCOMPAT="-mmacosx-version-min=11.0"
OSXCOMPAT="$OSXCOMPAT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64"

################################################################################
function download {
	echo "--- Downloading.. $2"
	test -f ${SRCDIR}/$1 || curl -L -o ${SRCDIR}/$1 $2
}

function src {
	download ${1}.${2} $3
	cd ${BUILDD}
	rm -rf $1
	tar xf ${SRCDIR}/${1}.${2}
	cd $1
}

function autoconfconf {
	set -e
	echo "======= $(pwd) ======="
	CPPFLAGS="-I${PREFIX}/include $CPPFLAGS" \
	CFLAGS="${MLARCH}${OSXCOMPAT:+ $OSXCOMPAT} -O3  $CFLAGS" \
	OBJCFLAGS="${MLARCH}${OSXCOMPAT:+ $OSXCOMPAT} -O3  $CFLAGS" \
	CXXFLAGS="${MLARCH}${OSXCOMPAT:+ $OSXCOMPAT} -O3  $CXXFLAGS" \
	LDFLAGS="${MLARCH}${OSXCOMPAT:+ $OSXCOMPAT} -headerpad_max_install_names -L${PREFIX}/lib $LDFLAGS" \
	./configure --disable-dependency-tracking \
	--build=x86_64-apple-darwin --host=aarch64-apple-darwin \
	--prefix=$PREFIX $@
}

function autoconfbuild {
	set -e
	autoconfconf $@
	make $MAKEFLAGS
	make install
}

if test -n "$WITHBUILDSTACK"; then
	rm -rf ${BUILDD}
	rm -rf ${PREFIX}

	mkdir -p ${SRCDIR}
	mkdir -p ${PREFIX}
	mkdir -p ${BUILDD}

################################################################################
src zlib-1.2.7 tar.gz https://downloads.sourceforge.net/project/libpng/zlib/1.2.7/zlib-1.2.7.tar.gz
CFLAGS="${OSXCOMPAT} -O3" \
LDFLAGS="${OSXCOMPAT} -headerpad_max_install_names ${OSXLINK}" \
./configure \
--archs="$MLARCH" \
--prefix=$PREFIX $@
make $MAKEFLAGS
make install

src liblo-0.28 tar.gz http://downloads.sourceforge.net/liblo/liblo-0.28.tar.gz
CFLAGS=" -Wno-absolute-value" \
autoconfconf
ed src/Makefile << EOF
/noinst_PROGRAMS
.,+3d
wq
EOF
ed Makefile << EOF
%s/examples//
wq
EOF
make $MAKEFLAGS
make install

################################################################################
src libltc-1.3.2 tar.gz https://github.com/x42/libltc/releases/download/v1.3.2/libltc-1.3.2.tar.gz
autoconfbuild

################################################################################
src freetype-2.9 tar.gz https://downloads.sourceforge.net/project/freetype/freetype2/2.9/freetype-2.9.tar.gz
autoconfbuild -with-harfbuzz=no --with-bzip2=no --with-png=no --disable-static

################################################################################
# svn checkout svn://svn.code.sf.net/p/portmedia/code/portmidi/trunk/
src portmidi-2.0.4 tar.gz https://github.com/PortMidi/portmidi/archive/refs/tags/v2.0.4.tar.gz
sed -i '' 's/VERSION 3.21/VERSION 3.18/g' CMakeLists.txt

CFLAGS="${MLARCH} ${OSXCOMPAT}" \
CXXFLAGS="${MLARCH} ${OSXCOMPAT}" \
LDFLAGS="${MLARCH} ${OSXCOMPAT} -headerpad_max_install_names" \
make -f pm_mac/Makefile.osx configuration=Release PF=${PREFIX}

#cd Release; sudo make install
cp Release/libportmidi.dylib ${PREFIX}/lib/
install_name_tool -id ${PREFIX}/lib/libportmidi.dylib ${PREFIX}/lib/libportmidi.dylib
cp pm_common/portmidi.h ${PREFIX}/include
cp porttime/porttime.h ${PREFIX}/include

################################################################################
FFVERSION=7.1.2
src ffmpeg-${FFVERSION} tar.bz2 http://www.ffmpeg.org/releases/ffmpeg-${FFVERSION}.tar.bz2
./configure --prefix=${PREFIX} ${FFFLAGS} \
  --enable-shared --enable-gpl --disable-static --disable-debug --disable-doc \
  --disable-programs --disable-iconv \
  --disable-sdl2 --disable-avfoundation --disable-coreimage \
  --arch=arm64 --enable-cross-compile --target-os=darwin \
  --extra-cflags="-arch arm64 ${OSXCOMPAT}  -I${PREFIX}/include" \
  --extra-ldflags="-arch arm64 ${OSXCOMPAT} -L${PREFIX}/lib -headerpad_max_install_names"
make $MAKEFLAGS
make install

################################################################################
echo "Build Stack Completed"
fi
################################################################################

cd ${BUILDD}
rm -rf xjadeo
git clone -b master --single-branch https://github.com/x42/xjadeo.git
cd xjadeo
aclocal
autoheader
autoconf
automake --gnu --add-missing --copy

autoconfconf --disable-xv --disable-sdl --with-fontfile=../Resources/ArdourMono.ttf --disable-dependency-tracking
make clean
make

export XJSTACK="$HOME/src"
NOREBUILD=1 ./x-osx-bundle.sh

VERSION=$(awk '/define VERSION /{print $3;}' config.h | sed 's/"//g')
mv -v /tmp/Jadeo-${VERSION}.dmg /tmp/Jadeo-arm64-${VERSION}.dmg
ls -lh /tmp/Jadeo-arm64-${VERSION}.dmg

#rsync -Pa /tmp/Jadeo-arm64-${VERSION}.dmg ardour.org:/persist/community.ardour.org/files/video-tools/jadeo-arm64-${VERSION}.dmg
