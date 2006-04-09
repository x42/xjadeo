qmake   = @ac_qmake@
qt3to4   = @ac_qt3to4@

COPY = /bin/cp
DEL_FILE = /bin/rm
MKDIR = /bin/mkdir

name    = qjadeo

headers = qjadeo.h

sources = qjadeo.cpp \
	mainwindow.ui

all:	$(name)

$(name).mak:	$(name).pro
	if test "x$(qt3to4)" != "x"; then \
	 $(MKDIR) -p qt3; \
	 $(COPY) -f qjadeo.cpp qjadeo.h mainwindow.ui qt3; \
	 $(qt3to4) -alwaysOverwrite $(name).pro; \
	fi
	$(qmake) -o $(name).mak "DEFINES += BINDIR=\\\"$(bindir)\\\"" $(name).pro

$(name):	$(name).mak $(sources) $(headers)
	$(MAKE) -f $(name).mak

install:	$(name)
	$(INSTALL) $(name) $(bindir)/

uninstall:	$(bindir)/$(name)
	$(DEL_FILE) -vf $(bindir)/$(name)

.PHONY:	clean
clean:	$(name).mak
	$(MAKE) -f $(name).mak clean
	if test -d qt3; then \
	 $(COPY) -f qt3/* .; \
	 $(DEL_FILE) -rf qt3; \
	fi
	$(DEL_FILE) -f $(name) $(name).mak
	$(DEL_FILE) -rf *.cache *.log *.status
	$(DEL_FILE) -rf .ui         
	$(DEL_FILE) -rf .obj
	$(DEL_FILE) -rf .moc
	$(DEL_FILE) -f portinglog.txt
	$(DEL_FILE) -rf .tmp

distclean:	clean

maintainer-clean:	clean
	$(DEL_FILE) -f Makefile
	$(DEL_FILE) -f Makefile.in
	$(DEL_FILE) -f $(name).pro

