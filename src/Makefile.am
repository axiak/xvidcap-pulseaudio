## Process this file with automake to produce Makefile.in

## Created by Anjuta

gladedir = $(datadir)/xvidcap/glade
glade_DATA = gnome-xvidcap.glade \
    pixmaps/xvidcap_logo.png

INCLUDES = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	$(PACKAGE_CFLAGS)

AM_CFLAGS =\
	 -Wall\
##	 -DDEBUG -DDEBUG2\
	 -g

bin_PROGRAMS = xvidcap

xvidcap_SOURCES = \
    app_data.c \
    app_data.h \
    capture.c \
    capture.h \
    codecs.c \
	codecs.h \
    colors.c \
    colors.h \
    frame.c \
    frame.h \
    gnome_frame.c \
    gnome_frame.h \
    gnome_ui.c \
    gnome_ui.h \
    gnome_warning.c \
    gnome_warning.h \
    gnome_options.c \
    gnome_options.h \
    led_meter.c \
    led_meter.h \
    control.h \
	main.c \
    options.c \
    xtoffmpeg.c \
    xtoffmpeg.h \
    xtoxwd.c \
    xtoxwd.h \
    job.c \
    job.h \
    xv_error_item.c \
    xv_error_item.h

xvidcap_LDFLAGS = -export-dynamic

xvidcap_LDADD = $(PACKAGE_LIBS)

EXTRA_DIST = $(glade_DATA)
