#/bin/sh

# update version in
# - configure.ac
# - debian/changelog

VERSION=$(awk '/define VERSION /{print $3;}' config.h | sed 's/"//g')
SFUSER=x42

if [ -z "$VERSION" ]; then 
  echo "unknown VERSION number"
  exit 1;
fi

if [ ! -f /tmp/jadeo-${VERSION}.dmg ]; then
	echo "BUILD OSX package first!";
	exit
fi

make clean
sh autogen.sh
./configure --enable-contrib
make -C doc html xjadeo.pdf
make dist

./x-win32.sh
./configure

git commit -a
git tag "v$VERSION" || (echo -n "version tagging failed. - press Enter to continue, CTRL-C to stop."; read; )
# upload to rg42.org git
git push
git push --tags
# upload to sourceforge git
git push sf
git push --tags sf


#upload files to sourceforge
sftp $SFUSER,xjadeo@frs.sourceforge.net << EOF
cd /home/frs/project/x/xj/xjadeo/xjadeo
mkdir v${VERSION}
cd v${VERSION}
put contrib/nsi/jadeo_installer_v${VERSION}.exe
put xjadeo-${VERSION}.tar.gz
put /tmp/jadeo-${VERSION}.dmg
EOF

#upload doc to sourceforge
sftp $SFUSER,xjadeo@web.sourceforge.net << EOF
cd htdocs/
rm *
lcd doc/html
put -P xjadeo.pdf
lcd html
put -P *
chmod 0664 *.*
EOF
