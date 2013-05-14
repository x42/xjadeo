#/bin/sh

# update version in
# - configure.ac
# - debian/changelog

SFUSER=x42

make clean
sh autogen.sh
./configure --enable-contrib --enable-qtgui
make -C doc html xjadeo.pdf
make dist

VERSION=$(awk '/define VERSION /{print $3;}' config.h | sed 's/"//g')
WINVERS=$(grep " VERSION " config.h | cut -d ' ' -f3 | sed 's/"//g'| sed 's/\./_/g')

if [ -z "$VERSION" ]; then 
  echo "unknown VERSION number"
  exit 1;
fi

if [ -z "$SRCONLY" ]; then
  if [ ! -f /tmp/jadeo-${VERSION}.dmg ]; then
    echo "BUILD OSX package first!";
    exit
  fi

  if [ ! -f contrib/nsi/jadeo_installer_v${WINVERS}.exe ]; then
    ./x-win32.sh
    ./configure
  fi
fi

git commit -a
git tag "v$VERSION" || (echo -n "version tagging failed. - press Enter to continue, CTRL-C to stop."; read; )
# upload to rg42.org git
git push origin
git push --tags
# upload to github git
git push github
git push --tags github


#upload files to sourceforge
sftp $SFUSER,xjadeo@frs.sourceforge.net << EOF
cd /home/frs/project/x/xj/xjadeo/xjadeo
mkdir v${VERSION}
cd v${VERSION}
put contrib/nsi/jadeo_installer_v${WINVERS}.exe
put xjadeo-${VERSION}.tar.gz
put /tmp/jadeo-${VERSION}.dmg
EOF

#upload doc to sourceforge
sftp $SFUSER,xjadeo@web.sourceforge.net << EOF
cd htdocs/
rm *
lcd doc/
put -P xjadeo.pdf
lcd html
put -P *
chmod 0664 *.*
EOF
