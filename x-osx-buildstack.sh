#!/bin/sh

# we keep a copy of the sources here:
: ${SRCDIR=$HOME/xjsrc}
# actual build location
: ${BUILDD=$HOME/xjbuildd}
# target install dir:
: ${PREFIX=$HOME/xjstack}
# concurrency
: ${MAKEFLAGS="-j2"}

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
	*)
		echo "**UNTESTED OSX VERSION**"
		echo "if it works, please report back :)"
		XJARCH="-arch i386 -arch x86_64"
		OSXCOMPAT=""
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

function autoconfbuild {
echo "======= $(pwd) ======="
PATH=${PREFIX}/bin:/usr/bin:/bin:/usr/sbin:/sbin \
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

################################################################################
download libiconv-1.14.tar.gz ftp://ftp.gnu.org/gnu/libiconv/libiconv-1.14.tar.gz
cd ${BUILDD}
tar xzf ${SRCDIR}/libiconv-1.14.tar.gz
cd libiconv-1.14
autoconfbuild --with-included-gettext --with-libiconv-prefix=$PREFIX

################################################################################
#git://liblo.git.sourceforge.net/gitroot/liblo/liblo
download liblo-0.28.tar.gz http://downloads.sourceforge.net/liblo/liblo-0.28.tar.gz
cd ${BUILDD}
tar xzf ${SRCDIR}/liblo-0.28.tar.gz
cd liblo-0.28
autoconfbuild

################################################################################
#git://github.com/x42/libltc.git
download libltc-1.1.4.tar.gz https://github.com/x42/libltc/releases/download/v1.1.4/libltc-1.1.4.tar.gz
cd ${BUILDD}
tar zxf ${SRCDIR}/libltc-1.1.4.tar.gz
cd libltc-1.1.4
autoconfbuild

################################################################################
#git clone -b VER-2-5-3 --depth 1  git://git.sv.gnu.org/freetype/freetype2.git
download freetype-2.5.3.tar.gz http://download.savannah.gnu.org/releases/freetype/freetype-2.5.3.tar.gz
cd ${BUILDD}
tar xzf ${SRCDIR}/freetype-2.5.3.tar.gz
cd freetype-2.5.3
autoconfbuild -with-harfbuzz=no --with-png=no

################################################################################
# svn checkout svn://svn.code.sf.net/p/portmedia/code/portmidi/trunk/
download portmidi-src-217.zip http://sourceforge.net/projects/portmedia/files/portmidi/217/portmidi-src-217.zip/download
cd ${BUILDD}
unzip ${SRCDIR}/portmidi-src-217.zip
cd portmidi
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
download libogg-1.3.2.tar.gz http://downloads.xiph.org/releases/ogg/libogg-1.3.2.tar.gz
cd ${BUILDD}
tar xzf ${SRCDIR}/libogg-1.3.2.tar.gz
cd libogg-1.3.2
autoconfbuild

################################################################################
download libtheora-1.1.1.tar.bz2 http://downloads.xiph.org/releases/theora/libtheora-1.1.1.tar.bz2
cd ${BUILDD}
tar xjf ${SRCDIR}/libtheora-1.1.1.tar.bz2
cd libtheora-1.1.1
autoconfbuild --disable-sdltest --disable-vorbistest --disable-oggtest --disable-asm --disable-examples --with-ogg=${PREFIX}

################################################################################
function x264build {
CFLAGS="-arch $1 ${OSXCOMPAT}" \
LDFLAGS="-arch $1 ${OSXCOMPAT} -headerpad_max_install_names" \
./configure --host=$1-macosx-darwin --enable-shared --disable-cli
make $MAKEFLAGS
DYL=`ls libx264.*.dylib`
cp ${DYL} ${DYL}-$1
}

### ftp://ftp.videolan.org/pub/x264/snapshots/last_x264.tar.bz2
### ftp://ftp.videolan.org/pub/x264/snapshots/last_stable_x264.tar.bz2
download x264.tar.bz2 ftp://ftp.videolan.org/pub/x264/snapshots/last_stable_x264.tar.bz2 # XXX
cd ${BUILDD}
#git clone --depth 1 git://git.videolan.org/x264.git
tar xjf  ${SRCDIR}/x264.tar.bz2
cd x264*
x264build i386
make install prefix=${PREFIX}
make clean
x264build x86_64
if test -z "$NOPPC"; then
	make clean
	x264build ppc
fi

DYL=`ls libx264.*.dylib`
lipo -create -output ${PREFIX}/lib/${DYL} ${DYL}-*
install_name_tool -id ${PREFIX}/lib/${DYL} ${PREFIX}/lib/${DYL}

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

./configure --prefix=${PREFIX} \
--enable-libx264 --enable-libtheora --enable-shared --enable-gpl --disable-static --disable-programs --disable-debug \
--arch=x86_32 --target-os=darwin --cpu=i686 --enable-cross-compile \
--extra-cflags="-arch i386 ${OSXCOMPAT}  -I${PREFIX}/include" \
--extra-ldflags="-arch i386 ${OSXCOMPAT} -L${PREFIX}/lib -headerpad_max_install_names"
make $MAKEFLAGS && make install

find . -iname "*dylib" -type f -exec echo cp -v {} ${PREFIX}/fflipo/\`basename {}\`-i386 \; | bash -
make clean

./configure --prefix=${PREFIX} \
--enable-libx264 --enable-libtheora --enable-shared --enable-gpl --disable-static --disable-programs --disable-debug \
--arch=x86_64 \
--extra-cflags="-arch x86_64 ${OSXCOMPAT}  -I${PREFIX}/include" \
--extra-ldflags="-arch x86_64 ${OSXCOMPAT} -L${PREFIX}/lib -headerpad_max_install_names"
make $MAKEFLAGS
find . -iname "*dylib" -type f -exec echo cp -v {} ${PREFIX}/fflipo/\`basename {}\`-x86_64 \; | bash -
make clean

if test -z "$NOPPC"; then
./configure --prefix=${PREFIX} \
--enable-libx264 --enable-libtheora --enable-shared --enable-gpl --disable-static --disable-programs --disable-debug \
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
cd ${BUILDD}
rm -rf xjadeo
git clone -b master git://github.com/x42/xjadeo.git
cd xjadeo

export XJARCH
export OSXCOMPAT
./x-osx-bundle.sh
