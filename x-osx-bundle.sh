#!/bin/bash

set -e

: ${XJSTACK=$HOME/xjstack}
: ${XJARCH=-arch i386 -arch ppc -arch x86_64}
: ${OSXCOMPAT="-isysroot /Developer/SDKs/MacOSX10.5.sdk -mmacosx-version-min=10.5 -headerpad_max_install_names"}

if test -z "$NOREBUILD"; then
#############################################################################
# BUILD ./configure

aclocal -I /usr/local/share/aclocal/
autoheader
autoconf
automake --gnu --add-missing --copy

#############################################################################
# BUILD XJADEO

PKG_CONFIG_PATH=$XJSTACK/lib/pkgconfig:/usr/local/lib/pkgconfig \
CPPFLAGS="-I${XJSTACK}/include" \
CFLAGS="${XJARCH} ${OSXCOMPAT}" \
OBJCFLAGS="${XJARCH} ${OSXCOMPAT}" \
LDFLAGS="${XJARCH} ${OSXCOMPAT} -headerpad_max_install_names -L${XJSTACK}/lib" \
./configure --disable-xv --disable-qtgui --disable-sdl --with-fontfile=../Resources/VeraMono.ttf --disable-dependency-tracking $@ || exit 1
make clean
make

fi ## NOREBUILD

VERSION=$(awk '/define VERSION /{print $3;}' config.h | sed 's/"//g')

echo
file src/xjadeo/xjadeo
file src/xjadeo/xjremote

#############################################################################
# DEPLOY TO LOCAL APP DIR

export PRODUCT_NAME="Jadeo"
export APPNAME="${PRODUCT_NAME}.app"
export CONTRIB_DIR="$(pwd)/contrib/"
export TARGET_BUILD_DIR="${CONTRIB_DIR}${APPNAME}/"
export TARGET_CONTENTS="${TARGET_BUILD_DIR}Contents/"

rm -f ${TARGET_CONTENTS}MacOS/Jadeo-bin
rm -f ${TARGET_CONTENTS}MacOS/xjremote
cp src/xjadeo/xjadeo ${TARGET_CONTENTS}MacOS/${PRODUCT_NAME}-bin
cp src/xjadeo/xjremote ${TARGET_CONTENTS}MacOS/xjremote

echo

##############################################################################
# add dependancies..

mkdir -p ${TARGET_CONTENTS}Frameworks
rm -f ${TARGET_CONTENTS}Frameworks/*.dylib

echo "bundle libraries ..."
while [ true ] ; do
	missing=false
	for file in ${TARGET_CONTENTS}MacOS/* ${TARGET_CONTENTS}Frameworks/*; do
		set +e # grep may return 1
		if ! file $file | grep -qs Mach-O ; then
			continue;
		fi
		deps=`otool -arch all -L $file \
			| awk '{print $1}' \
			| egrep "($XJSTACK|/opt/|/local/|libs/)" \
			| grep -v 'libjack\.'`
		set -e
		for dep in $deps ; do
			base=`basename $dep`
			if ! test -f ${TARGET_CONTENTS}Frameworks/$base; then
				cp -v $dep ${TARGET_CONTENTS}Frameworks/
			  missing=true
			fi
		done
	done
	if test x$missing = xfalse ; then
		break
	fi
done

echo "update executables ..."
for exe in ${TARGET_CONTENTS}MacOS/*; do
	set +e # grep may return 1
	if ! file $exe | grep -qs Mach-O ; then
		continue
	fi
	changes=""
	libs=`otool -arch all -L $exe \
		| awk '{print $1}' \
		| egrep "($XJSTACK|/opt/|/local/|libs/)" \
		| grep -v 'libjack\.'`
	set -e
	for lib in $libs; do
		base=`basename $lib`
		changes="$changes -change $lib @executable_path/../Frameworks/$base"
	done
	if test "x$changes" != "x" ; then
		install_name_tool $changes $exe
	fi
done

echo "update libraries ..."
for dylib in ${TARGET_CONTENTS}Frameworks/*.dylib ; do
	# skip symlinks
	if test -L $dylib ; then
		continue
	fi

	# change all the dependencies
	changes=""
	libs=`otool -arch all -L $dylib \
		| awk '{print $1}' \
		| egrep "($XJSTACK|/opt/|/local/|libs/)" \
		| grep -v 'libjack\.'`
	for lib in $libs; do
		base=`basename $lib`
		changes="$changes -change $lib @executable_path/../Frameworks/$base"
	done

	if test "x$changes" != x ; then
		if  install_name_tool $changes $dylib ; then
			:
		else
			exit 1
		fi
	fi

	# now the change what the library thinks its own name is
	base=`basename $dylib`
	install_name_tool -id @executable_path/../Frameworks/$base $dylib
done

echo "all bundled up."

##############################################################################
#roll a DMG

UC_DMG="/tmp/${PRODUCT_NAME}-${VERSION}.dmg"

DMGBACKGROUND=${CONTRIB_DIR}dmgbg.png
VOLNAME=$PRODUCT_NAME-${VERSION}
EXTRA_SPACE_MB=5


DMGMEGABYTES=$[ `du -sk "${TARGET_BUILD_DIR}" | cut -f 1` * 1024 / 1048576 + $EXTRA_SPACE_MB ]
echo "DMG MB = " $DMGMEGABYTES

MNTPATH=`mktemp -d -t /xjadeoimg`
TMPDMG=`mktemp -t xjadeo`
ICNSTMP=`mktemp -t appicon`

trap "rm -rf $MNTPATH $TMPDMG ${TMPDMG}.dmg $ICNSTMP" EXIT

rm -f $UC_DMG "$TMPDMG" "${TMPDMG}.dmg" "$ICNSTMP ${ICNSTMP}.icns ${ICNSTMP}.rsrc"
rm -rf "$MNTPATH"
mkdir -p "$MNTPATH"

TMPDMG="${TMPDMG}.dmg"

hdiutil create -megabytes $DMGMEGABYTES "$TMPDMG"
DiskDevice=$(hdid -nomount "$TMPDMG" | grep Apple_HFS | cut -f 1 -d ' ')
newfs_hfs -v "${VOLNAME}" "${DiskDevice}"
mount -t hfs "${DiskDevice}" "${MNTPATH}"

cp -a ${TARGET_BUILD_DIR} "${MNTPATH}/${APPNAME}"
mkdir "${MNTPATH}/.background"
cp -vi ${DMGBACKGROUND} "${MNTPATH}/.background/dmgbg.png"

echo "setting DMG background ..."

echo '
   tell application "Finder"
     tell disk "'${VOLNAME}'"
	   open
	   set current view of container window to icon view
	   set toolbar visible of container window to false
	   set statusbar visible of container window to false
	   set the bounds of container window to {400, 200, 800, 440}
	   set theViewOptions to the icon view options of container window
	   set arrangement of theViewOptions to not arranged
	   set icon size of theViewOptions to 64
	   set background picture of theViewOptions to file ".background:dmgbg.png"
	   make new alias file at container window to POSIX file "/Applications" with properties {name:"Applications"}
	   set position of item "'${APPNAME}'" of container window to {100, 100}
	   set position of item "Applications" of container window to {310, 100}
	   close
	   open
	   update without registering applications
	   delay 5
	   eject
     end tell
   end tell
' | osascript

set +e
chmod -Rf go-w "${MNTPATH}"
set -e
sync

echo "compressing Image ..."

# Umount the image
umount "${DiskDevice}"
hdiutil eject "${DiskDevice}"
# Create a read-only version, use zlib compression
hdiutil convert -format UDZO "${TMPDMG}" -imagekey zlib-level=9 -o "${UC_DMG}"
# Delete the temporary files
rm "$TMPDMG"
rm -rf "$MNTPATH"

echo "setting file icon ..."

cp ${TARGET_CONTENTS}Resources/Jadeo.icns ${ICNSTMP}.icns
/usr/bin/sips -i ${ICNSTMP}.icns
/Developer/Tools/DeRez -only icns ${ICNSTMP}.icns > ${ICNSTMP}.rsrc
/Developer/Tools/Rez -append ${ICNSTMP}.rsrc -o "$UC_DMG"
/Developer/Tools/SetFile -a C "$UC_DMG"

rm ${ICNSTMP}.icns ${ICNSTMP}.rsrc

echo
echo "packaging suceeded:"
ls -l "$UC_DMG"
echo "Done."
