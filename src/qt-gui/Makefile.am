# quick hack - else make dist won't include these files.
EXTRA_DIST= qjadeo.cpp qjadeo.pro.in mainwindow.ui qjadeo.h\
	images/qjadeo.png images/qjadeo.ico\
	importdialog.ui prefdialog.ui importprogress.ui \
	importdialog.h importprogress.h prefdialog.h \
	importdialog.cpp importprogress.cpp prefdialog.cpp \
	qjadeo_fr.ts qjadeo_ru.ts qjadeo_el_GR.ts qjadeo_cs.ts \
	Makefile.am qjadeo.qrc qjadeo.rc

qmake = @ac_qmake@
lrelease  = @ac_lrelease@
qmakeargs = @ac_qmake_args@

COPY = /bin/cp
DEL_FILE = /bin/rm
MKDIR = /bin/mkdir

name    = qjadeo

headers = qjadeo.h \
	importdialog.h  importprogress.h prefdialog.h

sources = qjadeo.cpp \
	mainwindow.ui \
	importdialog.cpp  importprogress.cpp prefdialog.cpp \
	importdialog.ui prefdialog.ui importprogress.ui 

TRANSLATIONS = qjadeo_fr.ts qjadeo_ru.ts qjadeo_el_GR.ts qjadeo_cs.ts

all:	$(name)

$(name).mak:	$(name).pro
	$(qmake) -o $(name).mak $(name).pro $(qmakeargs)
	$(lrelease) $(TRANSLATIONS)

$(name):	$(name).mak $(sources) $(headers) mydirs.h
	$(MAKE) -f $(name).mak

install:	$(name)
	$(INSTALL) $(name) $(DESTDIR)/$(bindir)/
	mkdir -p $(DESTDIR)/$(datadir)/qjadeo/locale/
	$(INSTALL) -m 644 -t $(DESTDIR)/$(datadir)/qjadeo/locale/ $(subst .ts,.qm,$(TRANSLATIONS))

uninstall: $(DESTDIR)/$(bindir)/$(name)
	$(DEL_FILE) -vf $(DESTDIR)/$(bindir)/$(name)
	cd $(DESTDIR)/$(datadir)/qjadeo/locale/ && $(DEL_FILE) -vf *.qm
	rmdir $(DESTDIR)/$(datadir)/qjadeo/locale/


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
	$(DEL_FILE) -f mydirs.h mainwindow.h

distclean:	clean

maintainer-clean:	clean
	$(DEL_FILE) -f Makefile
	$(DEL_FILE) -f Makefile.in
	$(DEL_FILE) -f $(name).pro

mydirs.h: Makefile
	echo '#ifdef WIN32' >$@
	echo '#define MENCODER "mencoder.exe"' >>$@
	echo '#define XJREMOTE "xjremote.bat"' >>$@
	echo '#define XJINFO "xjinfo.exe"' >>$@
	echo '#define BINDIR ""' >>$@
	echo '#define CONFIG_PREFIX ""' >>$@
	echo '#else' >>$@
	echo '#define MENCODER "mencoder"' >>$@
	echo '#define XJREMOTE "xjremote"' >>$@
	echo '#define XJINFO "xjinfo"' >>$@
	echo '#define BINDIR "$(bindir)/"' >>$@
	echo '#define CONFIG_PREFIX "$(datadir)/qjadeo/locale"' >>$@
	echo '#endif' >>$@
