#!/bin/sh

# we keep a copy of the sources here:
: ${SRCDIR=$HOME/src/stack}
# actual build location
: ${BUILDD=$HOME/src/xj_build}
# target install dir:
: ${PREFIX=$HOME/src/xj_stack}
# concurrency
: ${MAKEFLAGS="-j4"}

case `sw_vers -productVersion | cut -d'.' -f1,2` in
	"10.4")
		echo "Tiger"
		XJARCH="-arch i386 -arch ppc"
		OSXCOMPAT=""
		;;
	"10.5")
		echo "Leopard"
		XJARCH="-arch i386 -arch ppc"
		OSXCOMPAT=""
		;;
	"10.6")
		echo "Snow Leopard"
		XJARCH="-arch i386 -arch ppc -arch x86_64"
		OSXCOMPAT="-isysroot /Developer/SDKs/MacOSX10.5.sdk -mmacosx-version-min=10.5"
		;;
	"10.10")
		XJARCH="-arch x86_64"
		OSXCOMPAT="-mmacosx-version-min=10.9"
		;;
	"10.11")
		XJARCH="-arch x86_64"
		OSXCOMPAT="-mmacosx-version-min=10.10"
		;;
	*)
		echo "**UNTESTED OSX VERSION**"
		echo "if it works, please report back :)"
		XJARCH="-arch i386 -arch x86_64"
		OSXCOMPAT="-mmacosx-version-min=10.5"
		;;
	esac

################################################################################
set -e

# start with a clean slate:
if test -z "$NOCLEAN"; then
	rm -rf ${BUILDD}
	rm -rf ${PREFIX}
fi

mkdir -p ${SRCDIR}
mkdir -p ${PREFIX}
mkdir -p ${BUILDD}

unset PKG_CONFIG_PATH
export PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig
export PREFIX
export SRCDIR

OSXCOMPAT="$OSXCOMPAT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64"

export PATH=${PREFIX}/bin:${HOME}/bin:/usr/local/git/bin/:/usr/bin:/bin:/usr/sbin:/sbin

function autoconfbuild {
echo "======= $(pwd) ======="
CFLAGS="${XJARCH}${OSXCOMPAT:+ $OSXCOMPAT}" \
CXXFLAGS="${XJARCH}${OSXCOMPAT:+ $OSXCOMPAT}" \
LDFLAGS="${XJARCH}${OSXCOMPAT:+ $OSXCOMPAT} -headerpad_max_install_names" \
./configure --disable-dependency-tracking --prefix=$PREFIX --enable-shared $@
make $MAKEFLAGS && make install
}

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

################################################################################

src m4-1.4.17 tar.gz http://ftp.gnu.org/gnu/m4/m4-1.4.17.tar.gz
autoconfbuild

################################################################################
src pkg-config-0.28 tar.gz http://pkgconfig.freedesktop.org/releases/pkg-config-0.28.tar.gz
./configure --prefix=$PREFIX --with-internal-glib
make $MAKEFLAGS
make install

################################################################################

src autoconf-2.69 tar.xz http://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.gz
autoconfbuild
hash autoconf
hash autoreconf

src automake-1.14 tar.gz http://ftp.gnu.org/gnu/automake/automake-1.14.tar.gz
autoconfbuild
hash automake

src libtool-2.4 tar.gz http://ftp.gnu.org/gnu/libtool/libtool-2.4.tar.gz
autoconfbuild
hash libtoolize

src make-4.1 tar.gz http://ftp.gnu.org/gnu/make/make-4.1.tar.gz
autoconfbuild
hash make

###############################################################################

src cmake-2.8.12.2 tar.gz http://www.cmake.org/files/v2.8/cmake-2.8.12.2.tar.gz
./bootstrap --prefix=$PREFIX
make $MAKEFLAGS
make install

################################################################################

download jack_headers.tar.gz http://robin.linuxaudio.org/jack_headers.tar.gz
cd "$PREFIX"
tar xzf ${SRCDIR}/jack_headers.tar.gz
"$PREFIX"/update_pc_prefix.sh

################################################################################
src libiconv-1.14 tar.gz ftp://ftp.gnu.org/gnu/libiconv/libiconv-1.14.tar.gz
autoconfbuild --with-included-gettext --with-libiconv-prefix=$PREFIX

################################################################################
#git://liblo.git.sourceforge.net/gitroot/liblo/liblo
src liblo-0.28 tar.gz http://downloads.sourceforge.net/liblo/liblo-0.28.tar.gz
autoconfbuild

################################################################################
#git://github.com/x42/libltc.git
src libltc-1.1.4 tar.gz https://github.com/x42/libltc/releases/download/v1.1.4/libltc-1.1.4.tar.gz
autoconfbuild

################################################################################
src freetype-2.5.3 tar.gz http://download.savannah.gnu.org/releases/freetype/freetype-2.5.3.tar.gz
autoconfbuild -with-harfbuzz=no --with-png=no

################################################################################
# svn checkout svn://svn.code.sf.net/p/portmedia/code/portmidi/trunk/
download portmidi-src-217.zip http://sourceforge.net/projects/portmedia/files/portmidi/217/portmidi-src-217.zip/download
cd ${BUILDD}
unzip ${SRCDIR}/portmidi-src-217.zip
cd portmidi
if ! echo "$XJARCH" | grep -q "ppc"; then
sed -i '' 's/ ppc//g' CMakeLists.txt
#XXX better pass though cmake args somehow
# -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=10.5 -DCMAKE_OSX_ARCHITECTURES="i386;x86_64"
fi
CFLAGS="${XJARCH} ${OSXCOMPAT}" \
CXXFLAGS="${XJARCH} ${OSXCOMPAT}" \
LDFLAGS="${XJARCH} ${OSXCOMPAT} -headerpad_max_install_names" \
make -f pm_mac/Makefile.osx configuration=Release PF=${PREFIX}
#cd Release; sudo make install
cp Release/libportmidi.dylib ${PREFIX}/lib/
install_name_tool -id ${PREFIX}/lib/libportmidi.dylib ${PREFIX}/lib/libportmidi.dylib
cp pm_common/portmidi.h ${PREFIX}/include
cp porttime/porttime.h ${PREFIX}/include


################################################################################
src yasm-1.2.0 tar.gz http://www.tortall.net/projects/yasm/releases/yasm-1.2.0.tar.gz
autoconfbuild

################################################################################
#function x264build {
#CFLAGS="-arch $1 ${OSXCOMPAT}" \
#LDFLAGS="-arch $1 ${OSXCOMPAT} -headerpad_max_install_names" \
#./configure --host=$1-macosx-darwin --enable-shared --disable-cli
#make $MAKEFLAGS
#DYL=`ls libx264.*.dylib`
#cp ${DYL} ${DYL}-$1
#}
#
#### ftp://ftp.videolan.org/pub/x264/snapshots/last_x264.tar.bz2
#### ftp://ftp.videolan.org/pub/x264/snapshots/last_stable_x264.tar.bz2
#download x264.tar.bz2 ftp://ftp.videolan.org/pub/x264/snapshots/last_stable_x264.tar.bz2 # XXX
#cd ${BUILDD}
#tar xjf  ${SRCDIR}/x264.tar.bz2
#cd x264*
#x264build i386
#make install prefix=${PREFIX}
#make clean
#x264build x86_64
#if echo "$XJARCH" | grep -q "ppc"; then
#	make clean
#	x264build ppc
#fi
#
#DYL=`ls libx264.*.dylib`
#lipo -create -output ${PREFIX}/lib/${DYL} ${DYL}-*
#install_name_tool -id ${PREFIX}/lib/${DYL} ${PREFIX}/lib/${DYL}
#

################################################################################
src libvpx-v1.3.0 tar.bz2 https://webm.googlecode.com/files/libvpx-v1.3.0.tar.bz2

function buildvpx {
cd ${BUILDD}/libvpx-v1.3.0
./configure --prefix=$PREFIX --target=$1
make clean
make $MAKEFLAGS && make install
make clean
}

################################################################################
FFVERSION=2.2.5
download ffmpeg-${FFVERSION}.tar.bz2 http://www.ffmpeg.org/releases/ffmpeg-${FFVERSION}.tar.bz2
cd ${BUILDD}
tar xjf ${SRCDIR}/ffmpeg-${FFVERSION}.tar.bz2
cd ffmpeg-${FFVERSION}/
ed configure << EOF
%s/jack_jack_h/xxjack_jack_h/
%s/enabled jack_indev/enabled xxjack_indev/
%s/sdl_outdev_deps="sdl"/sdl_outdev_deps="xxxsdl"/
%s/enabled sdl/enabled xxsdl/
wq
EOF

rm -rf ${PREFIX}/fflipo
mkdir ${PREFIX}/fflipo

buildvpx x86-darwin9-gcc
cd ${BUILDD}/ffmpeg-${FFVERSION}/

./configure --prefix=${PREFIX} \
	--enable-libvpx \
	--enable-shared --enable-gpl --disable-static --disable-debug \
	--disable-programs --disable-iconv \
	--arch=x86_32 --target-os=darwin --cpu=i686 --enable-cross-compile \
	--extra-cflags="-arch i386 ${OSXCOMPAT}  -I${PREFIX}/include" \
	--extra-ldflags="-arch i386 ${OSXCOMPAT} -L${PREFIX}/lib -headerpad_max_install_names"
make $MAKEFLAGS
make install

find . -iname "*dylib" -type f -exec echo cp -v {} ${PREFIX}/fflipo/\`basename {}\`-i386 \; | bash -
make clean

buildvpx x86_64-darwin9-gcc
cd ${BUILDD}/ffmpeg-${FFVERSION}/
./configure --prefix=${PREFIX} \
	--enable-libvpx \
	--enable-shared --enable-gpl --disable-static --disable-debug \
	--disable-programs --disable-iconv \
	--arch=x86_64 \
	--extra-cflags="-arch x86_64 ${OSXCOMPAT}  -I${PREFIX}/include" \
	--extra-ldflags="-arch x86_64 ${OSXCOMPAT} -L${PREFIX}/lib -headerpad_max_install_names"
make $MAKEFLAGS
find . -iname "*dylib" -type f -exec echo cp -v {} ${PREFIX}/fflipo/\`basename {}\`-x86_64 \; | bash -
make clean

if echo "$XJARCH" | grep -q "ppc"; then
buildvpx ppc32-darwin9-gcc
cd ${BUILDD}/ffmpeg-${FFVERSION}/
./configure --prefix=${PREFIX} \
	--enable-libvpx \
	--enable-shared --enable-gpl --disable-static --disable-debug \
	--disable-programs --disable-iconv \
	--arch=ppc \
	--extra-cflags="-arch ppc ${OSXCOMPAT}  -I${PREFIX}/include" \
	--extra-ldflags="-arch ppc ${OSXCOMPAT} -L${PREFIX}/lib -headerpad_max_install_names"
make $MAKEFLAGS
find . -iname "*dylib" -type f -exec echo cp -v {} ${PREFIX}/fflipo/\`basename {}\`-ppc \; | bash -
fi

for file in ${PREFIX}/fflipo/*.dylib-i386; do
  BN=$(basename $file -i386)
  TN=$(readlink ${PREFIX}/lib/${BN})
  lipo -create -output ${PREFIX}/lib/${TN} ${PREFIX}/fflipo/${BN}-*
done

rm -rf ${PREFIX}/fflipo

################################################################################
if test -n "$DOCLEAN"; then
	rm -rf ${BUILDD}
	mkdir ${BUILDD}
fi
################################################################################
cd ${BUILDD}
rm -rf xjadeo
git clone -b master --single-branch git://github.com/x42/xjadeo.git
cd xjadeo

export XJARCH
export XJSTACK="$PREFIX"
export OSXCOMPAT
./x-osx-bundle.sh
