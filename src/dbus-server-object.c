/**
 * \file dbus-server-object.c
 *
 * This file contains a GObject extension wrapping dbus functionality for
 * remote controlling xvidcap.
 *
 */

/*
 * Copyright (C) 2004-07 Karl, Frankfurt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define DEBUGFILE "dbus-server-object.c"
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif     // HAVE_CONFIG_H
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#include <stdlib.h>
#include <glade/glade.h>
#include <gtk/gtktoggletoolbutton.h>
#include <gtk/gtk.h>
#include <dbus/dbus-glib-bindings.h>

#include "dbus-server-object.h"
#include "xvidcap-dbus-glue.h"
#include "app_data.h"
#include "control.h"

extern GtkWidget *xvc_ctrl_main_window;
extern GtkWidget *xvc_tray_icon_menu;

/*
 * forward declarations
 */
static void xvc_server_object_class_init (XvcServerObjectClass * klass);
static void xvc_server_object_init (XvcServerObject * server);

/**
 * \brief get GType for this GObject extension
 *
 * @return GType for this GObject extension
 */
GType
xvc_server_object_get_type ()
{
    static GType xso_type = 0;

    if (!xso_type) {
        static const GTypeInfo xso_info = {
            sizeof (XvcServerObjectClass),
            NULL,
            NULL,
            (GClassInitFunc) xvc_server_object_class_init,
            NULL,
            NULL,
            sizeof (XvcServerObject),
            0,
            (GInstanceInitFunc) xvc_server_object_init,
        };

        xso_type =
            g_type_register_static (G_TYPE_OBJECT, "XvcServerObject", &xso_info,
                                    0);
    }

    return xso_type;
}

/**
 * \brief class initializer for this GObject extension class
 *
 * @param klass a pointer to this GObject extension class
 */
static void
xvc_server_object_class_init (XvcServerObjectClass * klass)
{
    GError *error = NULL;

    /* Init the DBus connection, per-klass */
    klass->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
    if (klass->connection == NULL) {
        g_warning ("Unable to connect to dbus: %s", error->message);
        g_error_free (error);
        return;
    }

    /* &dbus_glib__object_info is provided in the server-bindings.h file */
    /* OBJECT_TYPE_SERVER is the GType of your server object */
    dbus_g_object_type_install_info (xvc_server_object_get_type (),
                                     &dbus_glib_xvc_server_object_info);
}

/**
 * \brief object initializer for this GObject extension class
 *
 * @param server a pointer to an instance of this GObject extension class
 */
static void
xvc_server_object_init (XvcServerObject * server)
{
    GError *error = NULL;
    DBusGProxy *driver_proxy;
    XvcServerObjectClass *klass = XVC_SERVER_OBJECT_GET_CLASS (server);
    guint request_ret;

    /* Register DBUS path */
    dbus_g_connection_register_g_object (klass->connection,
                                         "/net/jarre_de_the/Xvidcap",
                                         G_OBJECT (server));

    /* Register the service name, the constant here are defined in dbus-glib-bindings.h */
    driver_proxy = dbus_g_proxy_new_for_name (klass->connection,
                                              DBUS_SERVICE_DBUS,
                                              DBUS_PATH_DBUS,
                                              DBUS_INTERFACE_DBUS);

    /* See tutorial for more infos about these */
    if (!org_freedesktop_DBus_request_name (driver_proxy,
                                            "net.jarre_de_the.Xvidcap", 0,
                                            &request_ret, &error)) {
        g_warning ("Unable to register service: %s", error->message);
        g_error_free (error);
    }
    g_object_unref (driver_proxy);
}

/**
 * \brief creates a new object for this class
 *
 * @return an instance for this class
 */
XvcServerObject *
xvc_server_object_new ()
{
    return
        XVC_SERVER_OBJECT (g_object_new (xvc_server_object_get_type (), NULL));
}

/**
 * \brief implementation of the stop method for remote execution through dbus
 *
 * @param server a pointer to an instance of this class
 * @param error pointer to a pointer to a GError
 * @return gboolean
 */
gboolean
xvc_dbus_stop (XvcServerObject * server, GError ** error)
{
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

    // get the filename button widget
    xml = glade_get_widget_tree (xvc_ctrl_main_window);
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_ctrl_stop_toggle");
    g_assert (w);

    // we can safely set the stop toggle always to active
    gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (w), TRUE);

    return TRUE;
}

/**
 * \brief function for starting a capture through xvc_idle_add
 *
 * @return gboolean FALSE to stop this after being run once
 */
static gboolean
dbus_capture_start ()
{
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

    // get the filename button widget
    xml = glade_get_widget_tree (xvc_ctrl_main_window);
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_ctrl_record_toggle");
    g_assert (w);

    // we can safely set the start toggle always to active
    gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (w), TRUE);

    return FALSE;
}

/**
 * \brief implementation of the start method for remote execution through dbus
 *
 * @param server a pointer to an instance of this class
 * @param error pointer to a pointer to a GError
 * @return gboolean
 */
gboolean
xvc_dbus_start (XvcServerObject * server, GError ** error)
{
    // we cannot just run this here and now, because we do not
    // know if the UI is ready, yet. In case of dbus activation
    // it may still be starting.
    xvc_idle_add (dbus_capture_start, (void *) NULL);

    return TRUE;
}

/**
 * \brief implementation of the pause method for remote execution through dbus
 *
 * @param server a pointer to an instance of this class
 * @param error pointer to a pointer to a GError
 * @return gboolean
 */
gboolean
xvc_dbus_pause (XvcServerObject * server, GError ** error)
{
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    gboolean active = FALSE;

    // if the tray_icon_menu is not NULL, we're running in use-tray-icon
    // mode and the tray icon is present. Then we need to use the toggle
    // there to update the checkmenuitem
    if (xvc_tray_icon_menu != NULL) {
        // get the widget for the pause menu item in the tray icon
        xml = glade_get_widget_tree (xvc_tray_icon_menu);
        g_assert (xml);
        w = glade_xml_get_widget (xml, "xvc_ti_pause");
        g_assert (w);

        // toggle the state from active to inactive and vice versa
        active = !gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (w));
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w), active);
    } else {
        // if the tray_icon_menu is not present, we use the main control
        // get the pause button widget
        xml = glade_get_widget_tree (xvc_ctrl_main_window);
        g_assert (xml);
        w = glade_xml_get_widget (xml, "xvc_ctrl_pause_toggle");
        g_assert (w);

        // toggle the state from active to inactive and vice versa
        active = !gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (w));
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w), active);
    }

    return TRUE;
}
