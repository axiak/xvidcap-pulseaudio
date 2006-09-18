## Process this file with automake to produce Makefile.in
## Created by Anjuta

SUBDIRS = src \
    po \
    man \
    doc

pkgdata_DATA = xvidcap.desktop \
	xvidcap.png

xvidcapdocdir = ${prefix}/doc/xvidcap
xvidcapdoc_DATA = \
	README\
	COPYING\
	AUTHORS\
	ChangeLog\
	INSTALL\
	NEWS

EXTRA_DIST = $(xvidcapdoc_DATA) \
    autogen.sh \
    intltool-extract.in \
    intltool-merge.in \
    intltool-update.in 
# FIXME: do I need to add xvidcap.desktop and xvidcap.png to extra_dist even if they are in pkgdatadir_DATA?

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

uninstall-hook:
	rm $(datadir)/pixmaps/xvidcap.png ; \
	rm $(datadir)/applications/xvidcap.desktop

