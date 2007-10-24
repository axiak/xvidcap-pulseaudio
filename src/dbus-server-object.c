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

#include "dbus-server-object.h"
#include "xvidcap-dbus-glue.h"

extern GtkWidget *xvc_ctrl_main_window;

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

// dbus_glib_marshal_server_object_BOOLEAN__STRING_POINTER_POINTER,
gboolean
xvc_dbus_echo_string (XvcServerObject * server, gchar * original, gchar ** echo,
                      GError ** error)
{
    gboolean problem = FALSE;

    system ("touch /tmp/out.txt");
    *echo = g_strdup (original);

    if (problem) {
        /* We have an error, set the gerror */
        g_set_error (error, g_quark_from_static_string ("echo"),
                     0xdeadbeef, "Some random problem occured, you're screwed");
        return FALSE;
    }

    return TRUE;
}

// dbus_glib_marshal_xvc_server_BOOLEAN__POINTER,
gboolean
xvc_dbus_stop (XvcServerObject * server, GError ** error) {
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
 //   gboolean active = FALSE;

    // get the filename button widget
    xml = glade_get_widget_tree (xvc_ctrl_main_window);
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_ctrl_stop_toggle");
    g_assert (w);
	
//	active = !gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(w));
    gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (w), 
	TRUE);
//	active);

	return TRUE;

}

// dbus_glib_marshal_xvc_server_BOOLEAN__POINTER,
gboolean
xvc_dbus_start (XvcServerObject * server, GError ** error) {
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    //gboolean active = FALSE;

    // get the filename button widget
    xml = glade_get_widget_tree (xvc_ctrl_main_window);
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_ctrl_record_toggle");
    g_assert (w);
	
	//active = !gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(w));
    gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (w), 
	TRUE);
//	active);

	return TRUE;

}

// dbus_glib_marshal_xvc_server_BOOLEAN__POINTER,
gboolean
xvc_dbus_pause (XvcServerObject * server, GError ** error) {
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    gboolean active = FALSE;

    // get the filename button widget
    xml = glade_get_widget_tree (xvc_ctrl_main_window);
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_ctrl_pause_toggle");
    g_assert (w);
	
	active = !gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(w));
    gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (w), 
	active);

	return TRUE;

}

