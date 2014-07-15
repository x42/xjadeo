#!/bin/sh

#############################################################################
# BUILD ./configure

aclocal -I /usr/local/share/aclocal/
autoheader 
autoconf 
automake --gnu --add-missing --copy

#############################################################################
# BUILD XJADEO

PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH \
CFLAGS="-arch i386 -arch ppc -arch x86_64 -isysroot /Developer/SDKs/MacOSX10.5.sdk -mmacosx-version-min=10.5 -headerpad_max_install_names" \
LDFLAGS="-arch i386 -arch ppc -arch x86_64 -isysroot /Developer/SDKs/MacOSX10.5.sdk -mmacosx-version-min=10.5 -headerpad_max_install_names" \
OBJCFLAGS="-arch i386 -arch ppc -arch x86_64 -isysroot /Developer/SDKs/MacOSX10.5.sdk -mmacosx-version-min=10.5 -headerpad_max_install_names" \
./configure --disable-xv --disable-qtgui --disable-sdl --with-fontfile=../Resources/VeraMono.ttf --disable-dependency-tracking $@ || exit 1
make clean
make || exit 1

VERSION=$(awk '/define VERSION /{print $3;}' config.h | sed 's/"//g')

echo
file src/xjadeo/xjadeo 
file src/xjadeo/xjremote

rm contrib/Jadeo.app/Contents/MacOS/Jadeo-bin
rm contrib/Jadeo.app/Contents/MacOS/xjremote
cp src/xjadeo/xjadeo contrib/Jadeo.app/Contents/MacOS/Jadeo-bin
cp src/xjadeo/xjremote contrib/Jadeo.app/Contents/MacOS/xjremote

##############################################################################
# add dependancies..

export TARGET_BUILD_DIR="$(pwd)/contrib/"
export PRODUCT_NAME="Jadeo"

export INSTALLED="libjack.0.dylib"
export LIBS_PATH="$TARGET_BUILD_DIR/$PRODUCT_NAME.app/Contents/Frameworks"
mkdir -p $LIBS_PATH
export TARGET="$TARGET_BUILD_DIR/$PRODUCT_NAME.app/Contents/MacOS/${PRODUCT_NAME}-bin"

echo $TARGET_BUILD_DIR;
echo $LIBS_PATH;

rm -f $LIBS_PATH/*.dylib

follow_dependencies () {
    libname=$1
    cd "$TARGET_BUILD_DIR/$PRODUCT_NAME.app/Contents/Frameworks"
    dependencies=`otool -arch all -L "$libname"  | egrep '\/((opt|usr)\/local\/lib|gtk\/inst\/lib)'| awk '{print $1}'`
    for l in $dependencies; do
        depname=`basename $l`
        deppath=`dirname $l`
        if [ ! -f "$TARGET_BUILD_DIR/$PRODUCT_NAME.app/Contents/Frameworks/$depname" ]; then
            deploy_lib $depname "$deppath"
        fi
    done
}

update_links () {
    libname=$1
    libpath=$2
    for n in `ls $LIBS_PATH/*`; do
        install_name_tool \
            -change "$libpath/$libname" \
            @executable_path/../Frameworks/$libname \
            "$n"
    done
}

deploy_lib () {
    libname=$1
    libpath=$2
    check=`echo $INSTALLED | grep $libname`
    if [ "X$check" = "X" ]; then
        if [ ! -f "$TARGET_BUILD_DIR/$PRODUCT_NAME.app/Contents/Frameworks/$libname" ]; then
            cp -f "$libpath/$libname" "$TARGET_BUILD_DIR/$PRODUCT_NAME.app/Contents/Frameworks/$libname"
            install_name_tool \
                -id @executable_path/../Frameworks/$libname \
                "$TARGET_BUILD_DIR/$PRODUCT_NAME.app/Contents/Frameworks/$libname"
            follow_dependencies $libname
        fi
        export INSTALLED="$INSTALLED $libname"
    fi
    update_links $libname $libpath
}

update_executable() {
    LIBS=`otool -arch all -L "$TARGET" | egrep '\/((opt|usr)\/local\/lib|gtk\/inst\/lib)'| awk '{print $1}'`
    for l in $LIBS; do
        libname=`basename $l`
        libpath=`dirname $l`
        deploy_lib $libname $libpath
        echo "$libname" | grep "libjack" >/dev/null && continue 
        echo "install_name_tool -change $libpath/$libname @executable_path/../Frameworks/$libname \"$TARGET\""
        install_name_tool \
            -change $libpath/$libname \
            @executable_path/../Frameworks/$libname \
            "$TARGET"
    done
}

update_executable

cd $LIBS_PATH && MORELIBS=`otool -arch all -L * | egrep '\/((opt|usr)\/local\/lib|gtk\/inst\/lib)'| awk '{print $1}'` && cd -
while [ "X$MORELIBS" != "X" ]; do
    for l in $MORELIBS; do
        libname=`basename $l`
        libpath=`dirname $l`
        deploy_lib "$libname" "$libpath"
    done
    cd $LIBS_PATH && MORELIBS=`otool -arch all -L * | egrep '\/((opt|usr)\/local\/lib|gtk\/inst\/lib)'| awk '{print $1}'` && cd -
done
update_executable

##############################################################################
#roll a DMG

TMPFILE=/tmp/xjtmp.dmg
DMGFILE=/tmp/jadeo-${VERSION}.dmg
MNTPATH=/tmp/mnt/
VOLNAME=Jadeo
APPNAME="Jadeo.app"

BGPIC=$TARGET_BUILD_DIR/dmgbg.png

mkdir -p $MNTPATH
if [ -e $TMPFILE -o -e $DMGFILE -o ! -d $MNTPATH ]; then
  echo
  echo "could not make DMG. tmp-file or destination file exists."
  exit;
fi

hdiutil create -megabytes 200 $TMPFILE
DiskDevice=$(hdid -nomount "${TMPFILE}" | grep Apple_HFS | cut -f 1 -d ' ')
newfs_hfs -v "${VOLNAME}" "${DiskDevice}"
mount -t hfs "${DiskDevice}" "${MNTPATH}"

cp -r ${TARGET_BUILD_DIR}/${PRODUCT_NAME}.app ${MNTPATH}/

# TODO: remove .svn files..

mkdir ${MNTPATH}/.background
BGFILE=$(basename $BGPIC)
cp -vi ${BGPIC} ${MNTPATH}/.background/${BGFILE}

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
	   set background picture of theViewOptions to file ".background:'${BGFILE}'"
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

sync

# Umount the image
umount "${DiskDevice}"
hdiutil eject "${DiskDevice}"
# Create a read-only version, use zlib compression
hdiutil convert -format UDZO "${TMPFILE}" -imagekey zlib-level=9 -o "${DMGFILE}"
# Delete the temporary files
rm $TMPFILE
rmdir $MNTPATH

echo
echo "packaging suceeded."
ls -l $DMGFILE
