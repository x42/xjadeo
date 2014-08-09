.PHONY: all validate html clean man 

VERSION=$(shell awk '/define VERSION /{print $$3;}' ../config.h | sed 's/"//g')
WINVERS=$(shell grep " VERSION " ../config.h | cut -d ' ' -f3 | sed 's/"//g'| sed 's/\./_/g')
RELDATE=$(shell date '+%B %Y')

all: man

html: xjadeo.1 xjremote.1
	mkdir -p html/static
	cp -f favicon.ico html/
	cp -a static/* html/static/
	groff -m man -T xhtml -P -l xjadeo.1 | sed 's/cellspacing="0" cellpadding="0"//g;s/width="100%" border="0" rules="none" frame="void"//g;s/valign="top" align="left"//g;s/width="/style="width:/g;s/><\/col>/\/>/g' > pages/xjadeo.html
	groff -m man -T xhtml -P -l xjremote.1 | sed 's/cellspacing="0" cellpadding="0"//g' > pages/xjremote.html
	echo -e "/<body>\n1,.d\n/<hr\\>\n.d\n/<hr\\>\n.d\n/<\\/body>\n.,\$$d\nw\nq" | ed -s pages/xjadeo.html
	echo -e "/<body>\n1,.d\n/<hr\\>\n.d\n/<hr\\>\n.d\n/<\\/body>\n.,\$$d\nw\nq" | ed -s pages/xjremote.html
	for file in pages/*; do echo $$file; cat tpl/head tpl/page $$file tpl/epage tpl/foot > html/`basename $$file`; done
	cat tpl/head tpl/front tpl/foot > html/index.html
	sed -i 's/@VERSION@/$(VERSION)/g;s/@WINVERSION@/$(WINVERS)/g;s/@DATE@/$(RELDATE)/g' html/download.html
	rm -f pages/xjadeo.html pages/xjremote.html

clean:
	-rm -fr xjadeo.1 xjremote.1 html/

../src/xjadeo/xjremote:
	$(MAKE) -C ../src/xjadeo/

../src/xjadeo/xjadeo:
	$(MAKE) -C ../src/xjadeo/

xjadeo.1: ../src/xjadeo/xjadeo
	help2man -N -n 'X Jack Video Monitor' -o xjadeo.1 ../src/xjadeo/xjadeo
	sed -i 's/^\.SS "/.SH "/;s/^Video Codecs and Formats:/.SH "Video Codecs and Formats:"/;s/^Configuration Files:/.SH "Configuration Files:"/;s/^User Interaction:/.SH "User Interaction:"/' xjadeo.1

xjremote.1: ../src/xjadeo/xjremote
	help2man -N -n 'X Jack Video Monitor Remote Control' -o xjremote.1 ../src/xjadeo/xjremote

man: xjadeo.1 xjremote.1

.PHONY: man all dist-clean html
