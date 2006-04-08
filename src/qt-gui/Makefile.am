qmake   = @ac_qmake@
qt3to4   = @ac_qt3to4@

name    = qjadeo

headers = qjadeo.h

sources = qjadeo.cpp \
	mainwindow.ui

all:	$(name)

$(name).mak:	$(name).pro
	if test "x$(qt3to4)" != "x"; then \
	 mkdir -p qt3; \
	 cp -f qjadeo.cpp cp qjadeo.h mainwindow.ui qt3; \
	 $(qt3to4) -alwaysOverwrite $(name).pro; \
	fi
	$(qmake) -o $(name).mak "DEFINES += BINDIR=\\\"$(bindir)\\\"" $(name).pro

$(name):	$(name).mak $(sources) $(headers)
	$(MAKE) -f $(name).mak

install:	$(name)
	$(INSTALL) $(name) $(bindir)/

uninstall:	$(bindir)/$(name)
	rm -vf $(bindir)/$(name)

.PHONY:	clean
clean:	$(name).mak
	$(MAKE) -f $(name).mak clean
	if test -d qt3; then \
	 cp -f qt3/* .; \
	 rm -rf qt3; \
	fi
	rm -f $(name) $(name).mak
	rm -rf *.cache *.log *.status
	rm -rf .ui         
	rm -rf .obj
	rm -rf .moc
	rm -f portinglog.txt
	rm -rf .tmp

distclean:	clean

maintainer-clean:	clean
	rm -f Makefile
	rm -f Makefile.in
	rm -f $(name).pro

