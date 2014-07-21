#!/bin/sh
# this script creates a windows32 version of xjadeo
# cross-compiled on GNU/Linux
#
# It is intended to run in a pristine chroot or VM of a minimal
# debian system. see http://wiki.debian.org/cowbuilder
#

: ${SRC=/usr/src}
: ${PFX=$HOME/local}

if [ "$(id -u)" != "0" -a -z "$SUDO" ]; then
	echo "This script must be run as root in pbuilder" 1>&2
  echo "e.g sudo cowbuilder --architecture amd64 --distribution wheezy --bindmounts /tmp --execute $0"
	exit 1
fi

apt-get -y install build-essential \
	gcc-mingw-w64-i686 g++-mingw-w64-i686 mingw-w64-tools mingw32 \
	wget git autoconf automake pkg-config \
	qt4-linguist-tools qt4-qmake \
	wine libwine-gl xauth xvfb

cd $SRC
git clone -b master git://github.com/x42/xjadeo.git

## HACK ALERT ##
# running .exe binary installers is not feasible here, and
# cross-compiling all the build-deps is a bit over the top, for now
# in the prototyping stage.
# We just use a pre-preared zip with (unmodified upstream binaries) .dll,
# include headers and pkg-config files.
# Some day we can switch to compile the whole stack
# (it's already done for OSX and Linux)

if test -f /tmp/xjadeo_windows_stack.tar.xz; then
	tar xJf /tmp/xjadeo_windows_stack.tar.xz
else
	wget http://robin.linuxaudio.org/xjadeo_windows_stack.tar.xz
	tar xJf xjadeo_windows_stack.tar.xz
fi

$SRC/xj-win-stack/update_pc_prefix.sh

# major hack alert. For now we use the hosts QT build system
# (qmake, lreleae) but run the pre-processor via wine
# using the target's chain.
cat > /usr/bin/rcc << EOF
xvfb-run wine $SRC/xj-win-stack/Qt/bin/rcc.exe \$@
sleep 1
EOF
chmod +x /usr/bin/rcc

###############################################################################

cd $SRC/xjadeo
aclocal
autoconf
autoreconf -i

export WINPREFIX=$SRC/xj-win-stack
export WINLIB=$SRC/xj-win-stack/lib
export QTDLL=$WINPREFIX/Qt/lib
export QTSPECPATH="$WINPREFIX/Qt/mkspecs/win32-x-g++"
export NSISEXE=$WINPREFIX/NSIS/makensis

./x-win-bundle.sh --with-qt4prefix=/usr
