/**
 * \file xvidcap-dbus-client.c
 *
 * This file contains a command line application for remote controlling
 * xvidcap itself through dbus remote function calls.
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif     // HAVE_CONFIG_H
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <dbus/dbus-glib-bindings.h>

#include "xvidcap-client-bindings.h"
#include "xvidcap-intl.h"

/**
 * \brief enumeration of types of actions supported to be used in a switch
 */
enum ACTION_TYPES
{
    ACTION_START,
    ACTION_STOP,
    ACTION_PAUSE
};

/**
 * \brief displays command line usage
 *
 * @param prog a string containing the name of the program
 */
void
usage (char *prog)
{
    printf (_("Usage: %s, ver %s, khb (c) 2003-07\n"), prog, VERSION);
    printf
        (_
         ("[--action #]      action to perform (\"start\"|\"stop\"|\"pause\")\n"));

    exit (1);
}

/**
 * \brief main function of the application to do the remote function invocation
 *
 * @return completion status
 */

int
main (int argc, char *argv[])
{
    // there's one one argument supported
    struct option options[] = {
        {"action", required_argument, NULL, 0}
    };
    int action = -1;
    int opt_index = 0, c;

    DBusGProxy *proxy;
    DBusGConnection *connection;
    GError *error = NULL;

    while ((c = getopt_long (argc, argv, "v", options, &opt_index)) != -1) {
        switch (c) {
        case 0:                       // it's a long option
            switch (opt_index) {
            case 0:                   // action
                if (strcasecmp (optarg, "start") == 0) {
                    action = ACTION_START;
                } else if (strcasecmp (optarg, "stop") == 0) {
                    action = ACTION_STOP;
                } else if (strcasecmp (optarg, "pause") == 0) {
                    action = ACTION_PAUSE;
                }
                break;
            }
        }
    }
    if (action < 0) {
        usage (argv[0]);
    }

    g_type_init ();

    connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
    if (connection == NULL) {
        g_warning ("Unable to connect to dbus: %sn", error->message);
        g_error_free (error);
        // Basically here, there is a problem, since there is no dbus :)
    }
    // This won't trigger activation!
    proxy = dbus_g_proxy_new_for_name (connection,
                                       "net.jarre_de_the.Xvidcap",
                                       "/net/jarre_de_the/Xvidcap",
                                       "net.jarre_de_the.Xvidcap");

    switch (action) {
    case ACTION_START:

        if (!net_jarre_de_the_Xvidcap_start (proxy, &error)) {
            g_warning (_("Could not send start command to xvidcap: %s"),
                       error->message);
            g_error_free (error);
        }
        break;
    case ACTION_STOP:

        if (!net_jarre_de_the_Xvidcap_stop (proxy, &error)) {
            g_warning (_("Could not send stop command to xvidcap: %s"),
                       error->message);
            g_error_free (error);
        }
        break;
    case ACTION_PAUSE:

        if (!net_jarre_de_the_Xvidcap_pause (proxy, &error)) {
            g_warning (_("Could not pause/unpause xvidcap: %s"),
                       error->message);
            g_error_free (error);
        }
        break;
    }

    // Cleanup
    g_object_unref (proxy);

    // The DBusGConnection should never be unreffed, it lives once and is
    // shared amongst the process

    return 0;
}
