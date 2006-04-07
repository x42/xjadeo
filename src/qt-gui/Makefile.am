ACLOCAL_AMFLAGS = -I m4

qmake   = @ac_qmake@

name    = qjadeo

headers = qjadeo.h

sources = qjadeo.cpp \
	mainwindow.ui

all:	$(name)

$(name).mak:	$(name).pro
	$(qmake) -o $(name).mak $(name).pro

$(name):	$(name).mak $(sources) $(headers)
	make -f $(name).mak

install:	$(name)
	$(INSTALL) $(name) $(bindir)/

uninstall:	$(bindir)/$(name)
	rm -vf $(bindir)/$(name)

.PHONY:	clean
clean:	$(name).mak
	make -f $(name).mak clean
	rm -f $(name) $(name).mak
	rm -rf *.cache *.log *.status
	rm -rf .ui         
	rm -rf .obj
	rm -rf .moc
	rm -rf .tmp

distclean:	clean

maintainer-clean:	clean
	rm -f Makefile
	rm -f Makefile.in
	rm -f $(name).pro

