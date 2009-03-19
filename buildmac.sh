#!/bin/sh

#############################################################################
# BUILD XJADEO

./configure --disable-xv --disable-qtgui --with-fontfile=../Resources/FreeMonoBold.ttf $@ || exit 1
make || exit 1
cp src/xjadeo/xjadeo src/xjadeo/xjadeo-`uname -p`

##############################################################################
# only built-host architecture by default
cp src/xjadeo/xjadeo contrib/Jadeo.app/Contents/MacOS/Jadeo-bin

##############################################################################
# universal executable
#
# **reconfigure --arch i386 ?! - dual compile -arch ppc -arch i386 , link separately
#cp  src/xjadeo/xjadeo src/xjadeo/xjadeo-i386
#cp  src/xjadeo/xjadeo src/xjadeo/xjadeo-ppc

if [ -x src/xjadeo/xjadeo-i386 -a -x src/xjadeo/xjadeo-ppc ]; then
  echo "creating universal binary"
  rm contrib/Jadeo.app/Contents/MacOS/Jadeo-bin
  lipo -create -output contrib/Jadeo.app/Contents/MacOS/Jadeo-bin src/xjadeo/xjadeo-ppc src/xjadeo/xjadeo-i386
fi


##############################################################################
#roll a DMG from the anyIdentity.app
TMPFILE=/tmp/xjtmp.dmg
DMGFILE=/tmp/jadeo.dmg
MNTPATH=/tmp/mnt/
VOLNAME=Jadeo

mkdir -p $MNTPATH
if [ -e $TMPFILE -o -e $DMGFILE -o ! -d $MNTPATH ]; then
  echo
  echo "could not make DMG. tmp-file or destination file exists."
  exit;
fi

hdiutil create -megabytes 40 $TMPFILE
DiskDevice=$(hdid -nomount "${TMPFILE}" | grep Apple_HFS | cut -f 1 -d ' ')
newfs_hfs -v "${VOLNAME}" "${DiskDevice}"
mount -t hfs "${DiskDevice}" "${MNTPATH}"

cp -r contrib/Jadeo.app ${MNTPATH}/

# Umount the image
umount "${DiskDevice}"
hdiutil eject "${DiskDevice}"
# Create a read-only version, use zlib compression
hdiutil convert -format UDZO "${TMPFILE}" -o "${DMGFILE}"
# Delete the temporary files
rm $TMPFILE
rmdir $MNTPATH

echo
echo "packaging suceeded. $DMGFILE"
