#/bin/bash

# update version in
# - configure.ac
# - ChangeLog
# - doc/pages/whatsnew.html

: ${SFUSER:=}
: ${OSXUSER:=}
: ${OSXMACHINE:=cowbuilder.local}
: ${COWBUILDER:=osxbuilder.local}

test -f "$HOME/.buildcfg.sh" && . "$HOME/.buildcfg.sh"

if test -z "$BINARYONLY"; then
	make clean
	sh autogen.sh
	./configure --enable-contrib --enable-qtgui --enable-weakjack

	VERSION=$(awk '/define VERSION /{print $3;}' config.h | sed 's/"//g')
	WINVERS=$(grep " VERSION " config.h | cut -d ' ' -f3 | sed 's/"//g'| sed 's/\./_/g')

	make REV="v$VERSION" # update binaries for help2man.
	make -C doc man html
	make clean
	make dist

	git commit -a
	git tag "v$VERSION" || (echo -n "version tagging failed. - press Enter to continue, CTRL-C to stop."; read; )

	echo -n "git push? [Y/n] "
	read -n1 a
	echo
	if test "$a" = "n" -o "$a" = "N"; then
		exit 1
	fi

	# upload to rg42.org git
	git push rg42
	git push rg42 --tags
	# upload to github git
	git push github
	git push --tags github

	#upload doc to sourceforge
	sftp $SFUSER,xjadeo@frs.sourceforge.net << EOF
cd htdocs/
rm *
lcd doc/html
mput -P -r *
chmod 0664 *.*
chmod 0664 static/*.*
chmod 0664 static/lb/*.*
chmod 0664 static/js/*.*
EOF
	#upload source to sourceforge
	sftp $SFUSER,xjadeo@frs.sourceforge.net << EOF
cd /home/frs/project/x/xj/xjadeo/xjadeo
mkdir v${VERSION}
cd v${VERSION}
put xjadeo-${VERSION}.tar.gz
EOF

fi

## build binaries..

VERSION=$(awk '/define VERSION /{print $3;}' config.h | sed 's/"//g')
WINVERS=$(grep " VERSION " config.h | cut -d ' ' -f3 | sed 's/"//g'| sed 's/\./_/g')

if [ -z "$VERSION" ]; then 
  echo "unknown VERSION number"
  exit 1;
fi

echo -n "build and upload? [Y/n] "
read -n1 a
echo
if test "$a" == "n" -o "$a" == "N"; then
	exit 1
fi

/bin/ping -q -c1 ${OSXMACHINE} &>/dev/null \
	&& /usr/sbin/arp -n ${OSXMACHINE} &>/dev/null
ok=$?
if test "$ok" != 0; then
	echo "OSX build host can not be reached."
	exit
fi

/bin/ping -q -c1 ${COWBUILDER} &>/dev/null \
	&& /usr/sbin/arp -n ${COWBUILDER} &>/dev/null
ok=$?
if test "$ok" != 0; then
	echo "Linux cowbuild host can not be reached."
	exit
fi

echo "building linux static and windows versions"
rm -f /tmp/xjadeo-*.tgz
rm -f /tmp/xjadeo_win-*.tar.xz
ssh $COWBUILDER ~/bin/build-xjadeo.sh

ok=$?
if test "$ok" != 0; then
	echo "remote build failed"
	exit
fi

if test -n "$OSXFROMSCRATCH"; then
  echo "building osx package from scratch"
  ssh ${OSXUSER}${OSXMACHINE} << EOF
exec /bin/bash -l
curl -L -o /tmp/xjadeo-x-pbuildstatic.sh https://raw.github.com/x42/xjadeo/master/x-osx-buildstack.sh
chmod +x /tmp/xjadeo-x-pbuildstatic.sh
/tmp/xjadeo-x-pbuildstatic.sh
EOF
else
  echo "building osx package with existing stack"
  ssh ${OSXUSER}${OSXMACHINE} << EOF
exec /bin/bash -l
rm -rf xjadeo
git clone -b master --single-branch git://github.com/x42/xjadeo.git
cd xjadeo
./x-osx-bundle.sh
cd ..
rm -rf xjadeo
EOF
fi

ok=$?
if test "$ok" != 0; then
	echo "remote build failed"
	exit
fi

# collect binaries from build-hosts
rsync -Pa $COWBUILDER:/tmp/xjadeo-i386-linux-gnu-v${VERSION}.tgz /tmp/ || exit
rsync -Pa $COWBUILDER:/tmp/xjadeo-x86_64-linux-gnu-v${VERSION}.tgz /tmp/ || exit
rsync -Pa $COWBUILDER:/tmp/xjadeo_installer_w32_v${WINVERS}.exe /tmp/ || exit
rsync -Pa $COWBUILDER:/tmp/xjadeo_installer_w64_v${WINVERS}.exe /tmp/ || exit
rsync -Pa $COWBUILDER:/tmp/xjadeo_w32-v${VERSION}.tar.xz /tmp/ || exit
rsync -Pa $COWBUILDER:/tmp/xjadeo_w64-v${VERSION}.tar.xz /tmp/ || exit
rsync -Pa ${OSXUSER}${OSXMACHINE}:/tmp/Jadeo-${VERSION}.dmg /tmp/jadeo-${VERSION}.dmg || exit

#upload files to sourceforge
sftp $SFUSER,xjadeo@frs.sourceforge.net << EOF
cd /home/frs/project/x/xj/xjadeo/xjadeo
mkdir v${VERSION}
cd v${VERSION}
put /tmp/xjadeo_installer_w32_v${WINVERS}.exe
put /tmp/xjadeo_installer_w64_v${WINVERS}.exe
put /tmp/xjadeo-i386-linux-gnu-v${VERSION}.tgz
put /tmp/xjadeo-x86_64-linux-gnu-v${VERSION}.tgz
put /tmp/jadeo-${VERSION}.dmg
EOF

# custom upload hook
test -x x-releasestatic.sh && ./x-releasestatic.sh
