#
# the docbook is not inteded to be built automatically
# as it requires an internet connection to get the xslt files.
# and would introduce xml build dependencies...
# 
# precompiled manpages are in included in the distribution,
# as is the docbook source.
#
#

.PHONY: all validate html clean man 

all: html man

validate:
	xmllint --noout --valid xjadeo.xml

html: validate
	mkdir -p html
	(cd html; xsltproc ../html.xsl ../xjadeo.xml)
	cp -f *.css html

man: validate
	xsltproc man.xsl xjadeo.xml

clean:
	-rm -fr qjadeo.1 xjadeo.1 html xjadeo.ps

#.PHONY: xjadeo.ps

#xjadeo.ps: xjadeo.xml
#	jw -f docbook -b ps xjadeo.xml

