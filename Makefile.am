## Process this file with automake to produce Makefile.in
## Created by Anjuta

doc_SUBDIRS = doc
SUBDIRS = $(doc_SUBDIRS) \
    src \
    po \
    debian

pkgdata_DATA = xvidcap.desktop \
	xvidcap.png \
    ppm2mpeg.sh

xvidcapdocdir = ${datadir}/doc/xvidcap
xvidcapdoc_DATA = \
	README\
	COPYING\
	AUTHORS\
	ChangeLog

EXTRA_DIST = $(xvidcapdoc_DATA) \
    autogen.sh \
    intltool-extract.in \
    intltool-merge.in \
    intltool-update.in \
    xvidcap.png \
    xvidcap.desktop \
    ppm2mpeg.sh \
    TODO.tasks \
    INSTALL


# Copy all the spec files. Of cource, only one is actually used.
dist-hook:
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done; \
    mkdir $(distdir)/ffmpeg ; \
    ( cd $(top_srcdir)/ffmpeg/ && tar cf - . | ( cd ../$(distdir)/ffmpeg && tar xf - ) ) ; \
    rm -rf `find $(distdir) -name CVS` ; \
    rm -rf `find $(distdir) -name .svn` ; \
    ( cd $(distdir)/ffmpeg/ && $(MAKE) distclean ) || ( echo "FAILED TO DISTCLEAN FFMPEG" && exit ) ; \
    ( test -f $(distdir)/ffmpeg/config.log && rm $(distdir)/ffmpeg/config.log ) || echo "no ffmpeg config.log"

install-exec-hook:
	mkdir $(datadir)/pixmaps ; \
	cp $(top_srcdir)/xvidcap.png $(datadir)/pixmaps/ ; \
	mkdir $(datadir)/applications ; \
	cp $(top_srcdir)/xvidcap.desktop $(datadir)/applications/

install-data-hook:
	chmod 755 $(datadir)/xvidcap/ppm2mpeg.sh

uninstall-hook:
	rm $(datadir)/pixmaps/xvidcap.png ; \
	rm $(datadir)/applications/xvidcap.desktop ; \
    rm -r $(datadir)/doc/xvidcap ; \
    rm -r $(datadir)/xvidcap


install-doc: 
	cd doc && $(MAKE) $(AM_MAKEFLAGS) install-doc

uninstall-doc: 
	cd doc && $(MAKE) $(AM_MAKEFLAGS) uninstall-doc
