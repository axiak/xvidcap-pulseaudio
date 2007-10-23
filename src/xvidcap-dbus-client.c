#ifdef DOXYGEN_SHOULD_SKIP_THIS
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif     // HAVE_CONFIG_H
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#include <dbus/dbus-glib-bindings.h>

#include "xvidcap-client-bindings.h"

int
main (int argc, char *argv[])
{

/* Somewhere in the code, we want to execute EchoString remote method */
    DBusGProxy *proxy;
    DBusGConnection *connection;
    GError *error = NULL;
    gchar *result;

    g_type_init ();

    connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
    if (connection == NULL) {
        g_warning ("Unable to connect to dbus: %sn", error->message);
        g_error_free (error);
        /* Basically here, there is a problem, since there is no dbus :) */
        return;
    }

/* This won't trigger activation! */
    proxy = dbus_g_proxy_new_for_name (connection,
                                       "net.jarre_de_the.Xvidcap",
                                       "/net/jarre_de_the/Xvidcap",
                                       "net.jarre_de_the.Xvidcap");

/* The method call will trigger activation, more on that later */
    if (!net_jarre_de_the_Xvidcap_echo_string
        (proxy, "The string we want echo-ed", &result, &error)) {
        /* Method failed, the GError is set, let's warn everyone */
        g_warning ("Woops remote method failed: %s", error->message);
        g_error_free (error);
        return;
    }

    g_print ("We got the folowing result: %s", result);

/* Cleanup */
    g_free (result);
    g_object_unref (proxy);

/* The DBusGConnection should never be unreffed, it lives once and is shared amongst the process */

}
