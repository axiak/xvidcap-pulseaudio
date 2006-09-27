xv_MANPAGES = xvidcap.1
xv_LANG = de

install:
	for i in $(xv_MANPAGES) ; do \
	xv_XML_FILE="$$i"".xml" ; \
	if test "$$xv_XML_FILE"; then \
		$(DOCBOOK2X_MAN) --encoding=utf8 "$$i.xml" ; \
	fi ;\
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

