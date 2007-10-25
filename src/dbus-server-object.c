#include <stdlib.h>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define DEBUGFILE "dbus-server-object.c"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif     // HAVE_CONFIG_H
#endif     // DOXYGEN_SHOULD_SKIP_THIS
#include <glade/glade.h>

#include <dbus/dbus-glib-bindings.h>
#include <gtk/gtktoggletoolbutton.h>
#include <gtk/gtk.h>

#include "dbus-server-object.h"
#include "xvidcap-dbus-glue.h"
#include "app_data.h"
#include "control.h"

extern GtkWidget *xvc_ctrl_main_window;
extern GtkWidget *xvc_tray_icon_menu;

static void xvc_server_object_class_init (XvcServerObjectClass * klass);
static void xvc_server_object_init (XvcServerObject * server);

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

    if (!org_freedesktop_DBus_request_name (driver_proxy, "net.jarre_de_the.Xvidcap", 0, &request_ret,  /* See tutorial for more infos about these */
                                            &error)) {
        g_warning ("Unable to register service: %s", error->message);
        g_error_free (error);
    }
    g_object_unref (driver_proxy);
}

XvcServerObject *
xvc_server_object_new ()
{
    return
        XVC_SERVER_OBJECT (g_object_new (xvc_server_object_get_type (), NULL));
}

// dbus_glib_marshal_xvc_server_BOOLEAN__POINTER,
gboolean
xvc_dbus_stop (XvcServerObject * server, GError ** error)
{
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

    //   gboolean active = FALSE;

    // get the filename button widget
    xml = glade_get_widget_tree (xvc_ctrl_main_window);
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_ctrl_stop_toggle");
    g_assert (w);

//  active = !gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(w));
    gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (w), TRUE);
//  active);

    return TRUE;

}

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

    //active = !gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(w));
    gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (w), TRUE);

    return FALSE;
}

// dbus_glib_marshal_xvc_server_BOOLEAN__POINTER,
gboolean
xvc_dbus_start (XvcServerObject * server, GError ** error)
{

    xvc_idle_add (dbus_capture_start, (void *) NULL);

    return TRUE;

}

// dbus_glib_marshal_xvc_server_BOOLEAN__POINTER,
gboolean
xvc_dbus_pause (XvcServerObject * server, GError ** error)
{
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    gboolean active = FALSE;

    if (xvc_tray_icon_menu != NULL) {
        // get the widget for the pause menu item in the tray icon
        xml = glade_get_widget_tree (xvc_tray_icon_menu);
        g_assert (xml);
        w = glade_xml_get_widget (xml, "xvc_ti_pause");
        g_assert (w);

        active = !gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (w));
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w), active);
    } else {

        // get the pause button widget
        xml = glade_get_widget_tree (xvc_ctrl_main_window);
        g_assert (xml);
        w = glade_xml_get_widget (xml, "xvc_ctrl_pause_toggle");
        g_assert (w);

        active = !gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (w));
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w), active);

    }

    return TRUE;

}
