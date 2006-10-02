# this makefile is included by the makefiles of the individual
# localized manpages. Each manpage should reside in a
# subdirectory named the language code. The man page
# should be a docbook xml format and named:
# <index>.<manvolnumber>.xml
# e. g.: xvidcap.1.xml
#
# each Makefile.am in the subdirectory must also define the
# following variables:
# xv_MANPAGES = xvidcap.1 
# this is a list of manpages available for the language
# xv_LANG = C
# this is the language code

install: 
	for i in $(xv_MANPAGES) ; do \
	xv_XML_FILE="$$i"".xml" ; \
	if test x$(DOCBOOK2X_MAN) = xno ; then \
		if test -r $$i ; then \
			echo "Cannot find docbook2x-man, installing pre-generated manpage $$i for locale $(xv_LANG)" ; \
		else \
			echo "Cannot find docbook2x-man or pre-generated manpage $$i for locale $(xv_LANG), skipping manpage" ; \
			continue ; \
		fi ; \
	else \
		$(DOCBOOK2X_MAN) --encoding=utf8 "$$i.xml" ; \
	fi ; \
	xv_MANVOLNUM=`echo $$i | sed -e 's,^[^\.]*\.,,'` ; \
	if test x$(xv_LANG) = xC; then \
		mkdir -p $(DESTDIR)$(mandir)/man$$xv_MANVOLNUM ; \
		$(INSTALL_DATA) $$i $(DESTDIR)$(mandir)/man$$xv_MANVOLNUM/ ; \
	else \
		mkdir -p $(DESTDIR)$(mandir)/$(xv_LANG)/man$$xv_MANVOLNUM ; \
		$(INSTALL_DATA) $$i $(DESTDIR)$(mandir)/$(xv_LANG)/man$$xv_MANVOLNUM/ ; \
	fi ; \
	done

uninstall:
	for i in $(xv_MANPAGES) ; do \
	xv_MANVOLNUM=`echo $$i | sed -e 's,^[^\.]*\.,,'` ; \
	if test x$(xv_LANG) = xC; then \
		rm $(DESTDIR)$(mandir)/man$$xv_MANVOLNUM/$$i ; \
	else \
		rm $(DESTDIR)$(mandir)/$(xv_LANG)/man$$xv_MANVOLNUM/$$i ; \
	fi ; \
	done


