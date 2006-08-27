## Process this file with automake to produce Makefile.in
## Created by Anjuta

SUBDIRS = src \
    po \
    man

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


