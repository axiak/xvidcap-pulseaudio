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
# xv_ENCODING = iso8859-1
# the desired encoding for the manpage

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
		if test -r $$i-$(xv_LANG).po ; then \
			if test x$(XML2PO) = xno ; then \
				echo "Cannot find xml2po to create current, translated manpage. Trying to use previous translation of $$i for locale $(xv_LANG)" ; \
			else \
				echo "translating manpage for lang $(xv_LANG)" ; \
				xml2po -a -p $$i-$(xv_LANG).po -o $$i.xml ../C/$$i.xml ; \
			fi ; \
		fi ; \
		if test -r $$i.xml ; then \
			echo "converting docbook to manpage for lang $(xv_LANG)" ; \
			$(DOCBOOK2X_MAN) --encoding=$(xv_ENCODING) "$$i.xml" ; \
		else \
			echo "Cannot find $$i.xml as source for manpage of locale $(xv_LANG)" ; \
			if test -r $$i ; then \
				echo "Installing pre-generated manpage" ; \
			else \
				echo "Skipping manpage" ; \
				continue ; \
			fi ; \
		fi ; \
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


