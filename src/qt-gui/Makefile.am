ACLOCAL_AMFLAGS = -I m4

qmake   = @ac_qmake@
qt3to4   = @ac_qt3to4@

name    = qjadeo

headers = qjadeo.h

sources = qjadeo.cpp \
	mainwindow.ui

all:	$(name)

$(name).mak:	$(name).pro
	if test "x$(qt3to4)" != "x"; then $(qt3to4) -alwaysOverwrite $(name).pro; fi
	$(qmake) -o $(name).mak "DEFINES += BINDIR=\\\"$(bindir)\\\"" $(name).pro

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
	rm portinglog.txt
	rm -rf .tmp

distclean:	clean

maintainer-clean:	clean
	rm -f Makefile
	rm -f Makefile.in
	rm -f $(name).pro

