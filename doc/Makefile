#
# the docbook is not inteded to be built automatically
# as it requires an internet connection to get the xslt files.
# and would introduce XML* dependencies...
# 
# precompiled manpages are in included in the distribution,
# as is the docbook source.
#
# an HTML version of the docbook is available online:
#  http://xjadeo.sourceforge.net/
#

.PHONY: all validate html clean man 

all: html man

dtdvalid:
	xmllint --noout --valid -dtdvalid http://www.oasis-open.org/docbook/xml/4.5/docbookx.dtd  xjadeo.xml

validate:
	xmllint --noout --valid xjadeo.xml

html: validate
	mkdir -p html 
	(cd html; xsltproc --encoding UTF-8 ../html.xsl ../xjadeo.xml)
	cp -f style.css favicon.ico html/
	cp -f xjadeo.png html/

man: validate
	xsltproc --encoding UTF-8 man.xsl xjadeo.xml

clean:
	-rm -fr qjadeo.1 xjadeo.1 xjinfo.1 xjremote.1 html xjadeo.ps xjadeo.pdf

.PHONY: xjadeo.ps xjadeo.pdf

xjadeo.ps: xjadeo.xml
	jw -f docbook -b ps xjadeo.xml

xjadeo.pdf: xjadeo.xml
	dblatex -T native \
	  -P newtbl.autowidth=default \
	  -P latex.hyperparam=colorlinks,linkcolor=blue \
	  -P doc.collab.show=0 \
	  -P latex.output.revhistory=0 \
	  -P make.year.ranges=1 \
	  -P doc.lot.show="" \
	  xjadeo.xml
