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

dtdvalid:
	xmllint --noout --valid -dtdvalid http://www.oasis-open.org/docbook/xml/4.4/docbookx.dtd  xjadeo.xml

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

.PHONY: pub
pub: dtdvalid html
	scp html/* sf:/home/groups/x/xj/xjadeo/htdocs/doc/
	ssh sf chmod -R g+w /home/groups/x/xj/xjadeo/htdocs/doc/
#make pub requires setup in ~/.ssh/config
# Host sf
# HostName shell.sourceforge.net
# User <username>
# 

#.PHONY: xjadeo.ps

#xjadeo.ps: xjadeo.xml
#	jw -f docbook -b ps xjadeo.xml

