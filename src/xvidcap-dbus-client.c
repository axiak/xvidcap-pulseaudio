#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifdef HAVE_CONFIG_H
//#include <config.h>
#include "../config.h"
#endif     // HAVE_CONFIG_H
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#include <dbus/dbus-glib-bindings.h>

#include "xvidcap-client-bindings.h"


enum ACTION_TYPES
{
	ACTION_START,
	ACTION_STOP,
	ACTION_PAUSE
};


void
usage (char *prog)
{
    printf
//        (_
         ("Usage: %s, ver %s, (c) khb (c) 2003,04,05,06\n"
// )
	, prog, VERSION);
//    printf (_
//            ("[--action #]      action to perform (start|stop|pause)\n"));
	printf("[--action #]      action to perform (start|stop|pause)\n");

    exit(1);
}


int
main (int argc, char *argv[])
{
    struct option options[] = {
        {"action", required_argument, NULL, 0}
    };
    int action = -1;
    int opt_index = 0, c;

    DBusGProxy *proxy;
    DBusGConnection *connection;
    GError *error = NULL;
    gchar *result;

    while ((c = getopt_long (argc, argv, "v", options, &opt_index)) != -1) {
        switch (c) {
        case 0:                       // it's a long option
            switch (opt_index) {
            case 0:                   // action
		if (strcasecmp(optarg,"start")==0) {
			action = ACTION_START;
		} else if (strcasecmp(optarg,"stop")==0) {
			action = ACTION_STOP;
		} else if (strcasecmp(optarg,"pause")==0) {
			action = ACTION_PAUSE;
		}
                break;
	   }
    }
    }
    if (action < 0) { 
	usage(argv[0]); 
    }

// Somewhere in the code, we want to execute EchoString remote method 

    g_type_init ();

    connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
    if (connection == NULL) {
        g_warning ("Unable to connect to dbus: %sn", error->message);
        g_error_free (error);
        // Basically here, there is a problem, since there is no dbus :) 
        return;
    }

// This won't trigger activation! 
    proxy = dbus_g_proxy_new_for_name (connection,
                                       "net.jarre_de_the.Xvidcap",
                                       "/net/jarre_de_the/Xvidcap",
                                       "net.jarre_de_the.Xvidcap");

// The method call will trigger activation, more on that later 
    if (!net_jarre_de_the_Xvidcap_echo_string
        (proxy, "The string we want echo-ed", &result, &error)) {
        // Method failed, the GError is set, let's warn everyone 
        g_warning ("Woops remote method failed: %s", error->message);
        g_error_free (error);
        return;
    }

    g_print ("We got the folowing result: %s", result);

    switch (action) {
	case ACTION_START:
		
    if (!net_jarre_de_the_Xvidcap_start
        (proxy, &error)) {
        g_warning ("Woops remote method failed: %s", error->message);
        g_error_free (error);
        return;
    }
		break;
	case ACTION_STOP:
		
    if (!net_jarre_de_the_Xvidcap_stop
        (proxy, &error)) {
        g_warning ("Woops remote method failed: %s", error->message);
        g_error_free (error);
        return;
    }
		break;
	case ACTION_PAUSE:
		
    if (!net_jarre_de_the_Xvidcap_pause
        (proxy, &error)) {
        g_warning ("Woops remote method failed: %s", error->message);
        g_error_free (error);
        return;
    }
		break;
    }

// Cleanup 
    g_free (result);
    g_object_unref (proxy);

// The DBusGConnection should never be unreffed, it lives once and is shared amongst the process 

    return 0;
}
