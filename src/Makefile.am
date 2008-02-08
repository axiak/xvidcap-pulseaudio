## Process this file with automake to produce Makefile.in

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
	 -g \
	 -DDBUS_SERVICE_DIR=\""$(datadir)/dbus-1/service"\"
#	 -DDEBUG -DDEBUG2

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
    xvc_error_item.c \
    xvc_error_item.h \
    xvidcap-intl.h \
        eggtrayicon.h \
        eggtrayicon.c

xvidcap_LDADD = $(PACKAGE_LIBS)

if SOLARIS
xvidcap_LDFLAGS = 
xvidcap_LDADD += -lXext -lX11 
else
xvidcap_LDFLAGS = -export-dynamic
endif

EXTRA_DIST = $(glade_DATA)

if USE_DBUS
xvidcap_SOURCES += \
	dbus-server-object.h \
	dbus-server-object.c

bin_PROGRAMS += xvidcap-dbus-client

xvidcap_dbus_client_SOURCES = xvidcap-dbus-client.c

xvidcap_dbus_client_LDADD = $(PACKAGE_LIBS)

if SOLARIS
xvidcap_dbus_client_LDFLAGS =
else
xvidcap_dbus_client_LDFLAGS = -export-dynamic
endif

BUILT_SOURCES = xvidcap-dbus-glue.h xvidcap-client-bindings.h
# We don't want to install this header
noinst_HEADERS = $(BUILT_SOURCES)

# Correctly clean the generated headers, but keep the xml description
CLEANFILES = $(BUILT_SOURCES)

#Rule to generate the binding headers
xvidcap-dbus-glue.h:  xvidcap-dbus.xml
	dbus-binding-tool --prefix=xvc_server --mode=glib-server $<> $@

xvidcap-client-bindings.h:  xvidcap-dbus.xml
	dbus-binding-tool --prefix=xvc_server --mode=glib-client $<> $@

EXTRA_DIST += xvidcap-dbus.xml
endif

