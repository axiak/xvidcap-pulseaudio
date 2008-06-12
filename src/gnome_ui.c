/**
 * \file gnome_ui.c
 *
 * This file contains the main control UI for xvidcap
 * \todo rename the global variables pthread* to xvc_pthread*
 */

/* Copyright (C) 2003-07 Karl H. Beckers, Frankfurt
 * EMail: khb@jarre-de-the.net
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
 *
 *
 */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define DEBUGFILE "gnome_ui.c"
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#include <sys/time.h>                  // for timeval struct and related
#include <sys/poll.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef SOLARIS
#include <X11/X.h>
#include <X11/Xlib.h>
#endif     // SOLARIS
#include <X11/cursorfont.h>
#include <X11/Xmu/WinUtil.h>
#include <X11/Xatom.h>

#ifdef USE_XDAMAGE
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xdamage.h>
#include <X11/Xproto.h>
#endif     // USE_XDAMAGE

#include <glib.h>
#include <gdk/gdkx.h>
#include <glade/glade.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#include "led_meter.h"
#include "job.h"
#include "app_data.h"
#include "control.h"
#include "colors.h"
#include "codecs.h"
#include "frame.h"
#include "gnome_warning.h"
#include "gnome_options.h"
#include "gnome_ui.h"
#include "gnome_frame.h"
#include "xvidcap-intl.h"
#include "eggtrayicon.h"

/*
 * globals
 */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern GtkWidget *xvc_pref_main_window;
extern GtkWidget *xvc_warn_main_window;
#endif     // DOXYGEN_SHOULD_SKIP_THIS

/** \brief make error numbers accessible */
extern int errno;

/** \brief make the main control panel globally available */
GtkWidget *xvc_ctrl_main_window = NULL;

/** \brief popup menu for the system tray icon */
GtkWidget *xvc_tray_icon_menu = NULL;

/**
 * \brief make the main control panel's menu globally available
 *
 * @note this is needed because the warning dialog may need to change the
 *      status of capture type select buttons. If you change from e. g.
 *      single-frame capture to multi-frame capture, xvidcap checks your
 *      preferences for the new capture type and finding errors allows you
 *      to cancel back out of the change ... and needs to reset the menu.
 */
GtkWidget *xvc_ctrl_m1 = NULL;

/**
 * \brief the value here is picked up by the LED frame drop monitor widget.
 *      Set to 0 to clear the widget.
 */
int xvc_led_time = 0;

/** \brief the recording thread */
static pthread_t recording_thread;

/** \brief attributes of the recording thread */
static pthread_attr_t recording_thread_attr;

/** \brief make the results dialog globally available to callbacks here */
static GtkWidget *xvc_result_dialog = NULL;

/** \brief remember the previous led_time set */
static int last_led_time = 0;

/** \brief remember the previously set picture number to allow for a quick
 *      check on the next call if we need to do anything */
static int last_pic_no = 0;

/** \brief store a timer that may be registered when setting a max recording
 *      time */
static guint stop_timer_id = 0;

/** calculate recording time */
static long start_time = 0, pause_time = 0, time_captured = 0;

/** \brief used to faciltate passing the filename selection from a file
 *      selector dialog back off the results dialog back to the results
 *      dialog and eventually the main dialog */
static char *target_file_name = NULL;

/**
 * \brief the errors found on submitting a set of preferences
 *
 * Used to pass this between the various components involved in deciding
 * what to do when OK is clicked
 * \todo the whole display of preferences and warning dialogs should be
 *      simplified through _run_dialog()
 */
static XVC_ErrorListItem *errors_after_cli = NULL;

/**
 * \brief remember the number of attempts to submit a set of preferences
 *
 * Hitting OK on a warning may make automatic changes to the preferences.
 * But it may not resolve all conflicts in one go and pop up the warning
 * dialog again, again offering automatic resultion actions. This should be
 * a rare case, however
 */
static int OK_attempts = 0;

/** \brief eggtrayicon for minimizing xvidcap to the system tray */
static GtkWidget *tray_icon = NULL;

/** \brief frame drop monitor for inclusion in the system tray */
static GtkWidget *tray_frame_mon = NULL;

/*
 * HELPER FUNCTIONS ...
 *
 *
 */
/**
 * \brief create the LED frame drop meter widget
 *
 * This function needs to conform to the custom widget creation functions
 * as libglade uses them
 * @param widget_name the name of the widget as set in glade
 * @param string1 not used
 * @param string2 not used
 * @param int1 not used
 * @param int2 not used
 * @return the created LED frame drop meter widget
 */
GtkWidget *
glade_create_led_meter (gchar * widget_name, gchar * string1,
                        gchar * string2, gint int1, gint int2)
{
#define DEBUGFUNCTION "glade_create_led_meter()"
    GtkWidget *frame_monitor = led_meter_new ();

    g_assert (frame_monitor);

    gtk_widget_set_name (GTK_WIDGET (frame_monitor), widget_name);
    gtk_widget_show (GTK_WIDGET (frame_monitor));

    return frame_monitor;
#undef DEBUGFUNCTION
}

/*
 * this isn't used atm
 *
 gint keySnooper (GtkWidget *grab_widget, GdkEventKey *event, gpointer func_data) {
 printf("keyval: %i - mods: %i\n", event->keyval, event->state);
 } */

/**
 * \brief change value of frame/filename display
 *
 * @param pic_no update the display according to this frame number
 */
void
GtkChangeLabel (int pic_no)
{
#define DEBUGFUNCTION "GtkChangeLabel()"
    static char file[PATH_MAX + 1], tmp_buf[PATH_MAX + 1];
    PangoLayout *layout = NULL;
    gint width = 0, height = 0;
    int filename_length = 0;
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    Job *jobp = xvc_job_ptr ();
    XVC_AppData *app = xvc_appdata_ptr ();

    // generate the string to display in the filename button
    if (app->current_mode > 0) {
        sprintf (tmp_buf, jobp->file, jobp->movie_no);
        sprintf (file, "%s[%04d]", tmp_buf, pic_no);
    } else {
        sprintf (file, jobp->file, pic_no);
    }
    // cut the string to a sensible maximum length
    filename_length = strlen (file);
    if (filename_length > 60) {
        char tmp_file[PATH_MAX + 1];
        char *tmp_file2, *ptr;
        int n;

        strncpy (tmp_file, file, 20);
        tmp_file[20] = 0;
        n = filename_length - 20;
        ptr = (char *) &file + n;
        tmp_file2 = strncat (tmp_file, "...", 4);
        tmp_file2 = strncat (tmp_file, ptr, 45);
        strncpy (file, tmp_file2, 45);
    }
    // get the filename button widget
    xml = glade_get_widget_tree (xvc_ctrl_main_window);
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_ctrl_filename_button");
    g_assert (w);

    // set the text
    gtk_button_set_label (GTK_BUTTON (w), file);

    // adjust the size of the filname button
    layout = gtk_widget_create_pango_layout (w, file);
    g_assert (layout);
    pango_layout_get_pixel_size (layout, &width, &height);
    g_object_unref (layout);

    gtk_widget_set_size_request (GTK_WIDGET (w), (width + 30), -1);
#undef DEBUGFUNCTION
}

/**
 * \brief implements the actions required when successfully submitting the
 *      warning dialog, mainly resetting the control UI to the current
 *      state of the preferences.
 */
void
warning_submit ()
{
#define DEBUGFUNCTION "warning_submit()"
#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
    XVC_AppData *app = xvc_appdata_ptr ();

    xvc_job_set_from_app_data (app);

    // set controls active/inactive/sensitive/insensitive according to
    // current options
    if (!(app->flags & FLG_NOGUI)) {
        xvc_reset_ctrl_main_window_according_to_current_prefs ();
    }

    if (xvc_errorlist_delete (errors_after_cli)) {
        fprintf (stderr,
                 "%s %s: Unrecoverable error while freeing error list, please contact the xvidcap project.\n",
                 DEBUGFILE, DEBUGFUNCTION);
        exit (1);
    }
    // reset attempts so warnings will be shown again next time ...
    OK_attempts = 0;

    // when starting, we are ready for remote commands via dbus after this

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}


/**
 * \brief retrieves the main controls window's extent rectangle. This is needed
 *      because of bug # 524110 in gdk_window_get_frame_extents.
 *      The code below is partly taken from the suggested fix for the bug.
 */
void
xvc_get_ctrl_frame_extents (GdkWindow * window, GdkRectangle * rect)
{
#define DEBUGFUNCTION "get_frame_extents()"
    XVC_AppData *app = xvc_appdata_ptr ();
    Window xwindow;
    Atom type_return;
    gint format_return;
    gulong nitems_return;
    gulong bytes_after_return;
    guchar *data;
    gboolean got_frame_extents = FALSE;
    Window root;
    guint ww, wh, wb, wd;
    gint wx, wy;

    xwindow = GDK_WINDOW_XID (window);

    Window *wm_child = NULL;
    Atom nwm_atom, utf8_string, wm_name_atom, rt;
    unsigned long nbytes, nitems;
    char *wm_name_str = NULL;
    int fmt;

    utf8_string = XInternAtom (app->dpy, "UTF8_STRING", False);

    nwm_atom = XInternAtom (app->dpy, "_NET_SUPPORTING_WM_CHECK", True);
    wm_name_atom = XInternAtom (app->dpy, "_NET_WM_NAME", True);

    if (nwm_atom != None && wm_name_atom != None) {
        if (XGetWindowProperty (app->dpy, app->root_window,
                                nwm_atom, 0, 100,
                                False, XA_WINDOW,
                                &rt, &fmt, &nitems, &nbytes,
                                (unsigned char **) ((void *)
                                                    &wm_child))
            != Success) {
            fprintf (stderr,
                     "%s %s: Error while trying to get a window to identify the window manager.\n",
                     DEBUGFILE, DEBUGFUNCTION);
        }
        if ((wm_child == NULL) ||
            (XGetWindowProperty
             (app->dpy, *wm_child, wm_name_atom, 0, 100, False,
              utf8_string, &rt, &fmt, &nitems, &nbytes,
              (unsigned char **) ((void *)
                                  &wm_name_str))
             != Success)) {
            fprintf (stderr,
                     "%s %s: Warning!!!\nYour window manager appears to be non-compliant!\n",
                     DEBUGFILE, DEBUGFUNCTION);
        }
    }
    // Right now only wm's that I know of performing 3d compositing
    // are beryl and compiz. names can be compiz for compiz and
    // beryl/beryl-co/beryl-core for beryl (so it's strncmp )
    if (wm_name_str && (!strcmp (wm_name_str, "compiz") ||
                        !strncmp (wm_name_str, "beryl", 5))) {

        /* first try: use _NET_FRAME_EXTENTS */
        if (XGetWindowProperty (app->dpy, xwindow,
                                XInternAtom (app->dpy,
                                             "_NET_FRAME_EXTENTS", TRUE),
                                0, G_MAXLONG, False, XA_CARDINAL, &type_return,
                                &format_return, &nitems_return,
                                &bytes_after_return, &data)
            == Success) {

            if ((type_return == XA_CARDINAL) && (format_return == 32) &&
                (nitems_return == 4) && (data)) {
                guint32 *ldata = (guint32 *) data;

                got_frame_extents = TRUE;

                /* try to get the real client window geometry */
                if (XGetGeometry (app->dpy, xwindow,
                                  &root, &wx, &wy, &ww, &wh, &wb, &wd)) {
                    rect->x = wx;
                    rect->y = wy;
                    rect->width = ww;
                    rect->height = wh;
                }

                /* _NET_FRAME_EXTENTS format is left, right, top, bottom */
                rect->x -= ldata[0];
                rect->y -= ldata[2];
                rect->width += ldata[0] + ldata[1];
                rect->height += ldata[2] + ldata[3];
            }

            if (data)
                XFree (data);
        }

        if (wm_name_str)
            XFree (wm_name_str);
    }

    if (got_frame_extents == FALSE) {
        gdk_window_get_frame_extents (window, rect);
    }
#undef DEBUGFUNCTION
}


#ifdef USE_FFMPEG
/**
 * \brief this toggles the type of capture (sf vs. mf) currently active
 *
 * This is global because it can be necessary to call this from the warning
 * dialog.
 */
void
xvc_toggle_cap_type ()
{
#define DEBUGFUNCTION "xvc_toggle_cap_type()"
    int count_non_info_messages, rc;
    XVC_AppData *app = xvc_appdata_ptr ();

    count_non_info_messages = 0;
    rc = 0;
    errors_after_cli = xvc_appdata_validate (app, 1, &rc);

    if (rc == -1) {
        fprintf (stderr,
                 "%s %s: Unrecoverable error while validating options, please contact the xvidcap project.\n",
                 DEBUGFILE, DEBUGFUNCTION);
        exit (1);
    }
    // warning dialog
    if (errors_after_cli != NULL) {
        XVC_ErrorListItem *err;

        err = errors_after_cli;
        for (; err != NULL; err = err->next) {
            if (err->err->type != XVC_ERR_INFO)
                count_non_info_messages++;
        }
        if (count_non_info_messages > 0
            || (app->flags & FLG_RUN_VERBOSE && OK_attempts == 0)) {
            xvc_warn_main_window =
                xvc_create_warning_with_errors (errors_after_cli, 1);
            OK_attempts++;
        } else {
            warning_submit ();
        }
    } else {
        warning_submit ();
    }
#undef DEBUGFUNCTION
}

/**
 * \brief this undoes an attempt to toggle the type of capture (sf vs. mf)
 *      currently active
 *
 * This is global because it can be necessary to call this from the warning
 * dialog.
 */
void
xvc_undo_toggle_cap_type ()
{
#define DEBUGFUNCTION "xvc_undo_toggle_cap_type()"
    GladeXML *xml = NULL;
    GtkWidget *mitem = NULL;
    XVC_AppData *app = xvc_appdata_ptr ();

    xml = glade_get_widget_tree (xvc_ctrl_m1);
    g_assert (xml);

    OK_attempts = 0;

#ifdef DEBUG
    printf ("%s %s: pre current_mode %i\n", DEBUGFILE, DEBUGFUNCTION,
            app->current_mode);
#endif     // DEBUG

    app->current_mode = (app->current_mode == 0) ? 1 : 0;

#ifdef DEBUG
    printf ("%s %s: post current_mode %i\n", DEBUGFILE, DEBUGFUNCTION,
            app->current_mode);
#endif     // DEBUG

    if (app->current_mode == 0) {
        mitem = glade_xml_get_widget (xml, "xvc_ctrl_m1_mitem_sf_capture");
        g_assert (mitem);
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mitem), TRUE);
    } else {
        mitem = glade_xml_get_widget (xml, "xvc_ctrl_m1_mitem_mf_capture");
        g_assert (mitem);
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mitem), TRUE);
    }

#undef DEBUGFUNCTION
}
#endif     // USE_FFMPEG

#ifdef USE_XDAMAGE
/**
 * \brief event filter to register with gdk to retrieve X11 events
 *
 * @param xevent pointer to the wrapped X11 event
 * @param event pointer to the GdkEvent
 * @param user_data pointer to other data
 * @return flag to gdk whether to pass the event on or drop it
 */
GdkFilterReturn
xvc_xdamage_event_filter (GdkXEvent * xevent, GdkEvent * event, void *user_data)
{
    XVC_AppData *app = xvc_appdata_ptr ();
    XEvent *xev = (XEvent *) xevent;
    XDamageNotifyEvent *e = (XDamageNotifyEvent *) (xevent);
    static XserverRegion region = None;
    Job *job = xvc_job_ptr ();

    // the following bits are purely for perormance reasons
    if (!app->recording_thread_running || job == NULL ||
        (job->flags & FLG_USE_XDAMAGE) == 0) {
        if (region != None) {
            XFixesDestroyRegion (app->dpy, region);
            region = None;
        }
        return GDK_FILTER_CONTINUE;
    }

    if (region == None)
        region = XFixesCreateRegion (app->dpy, 0, 0);

    if (xev->type == MapNotify) {
        XWindowAttributes attribs;
        XWindowAttributes root_attrs;
        Status ret;

        gdk_error_trap_push ();
        XGetWindowAttributes (app->dpy, app->root_window, &root_attrs);
        ret = XGetWindowAttributes (app->dpy,
                                    xev->xcreatewindow.window, &attribs);
        gdk_error_trap_pop ();

//        XSelectInput (app->dpy, xev->xcreatewindow.window, StructureNotifyMask);
        if (!attribs.override_redirect && attribs.depth == root_attrs.depth) {
//            gdk_error_trap_push ();
            XDamageCreate (app->dpy, xev->xcreatewindow.window,
                           XDamageReportRawRectangles);
//            gdk_error_trap_pop ();
        }
    } else if (xev->type == app->dmg_event_base) {
        XRectangle rect = {
            e->area.x, e->area.y, e->area.width, e->area.height
/*
            XVC_MAX (e->area.x - 10, 0),
            XVC_MAX (e->area.y - 10, 0),
            e->area.width + 20,
            e->area.height + 20
*/
        };

        // set the region required by XDamageSubtract unclipped
        XFixesSetRegion (app->dpy, region, &(e->area), 1);

        // Offset the region with the windows' position
        XFixesTranslateRegion (app->dpy, region, e->geometry.x, e->geometry.y);

        // subtract the damage
        // this will not work on a locked display, so we need to synchronize
        // this with the capture thread
        pthread_mutex_lock (&(app->capturing_mutex));
        XDamageSubtract (app->dpy, e->damage, region, None);
        XSync (app->dpy, False);

        // clip the damaged rectangle
        // need to do that in the lock or otherwise an event triggered before
        // the caputre of a moved frame may queue up and add to regions after
        // the full capture. And then the area of the event may be outside the
        // new capture area and anyway, we don't need this anymore because
        // we've captured this change in the full frame captured
        if (rect.x < app->area->x && (rect.x + rect.width) > app->area->x) {
            rect.width -= (app->area->x - rect.x);
            rect.x = app->area->x;
        }
        if ((rect.x + rect.width) > (app->area->x + app->area->width)) {
            rect.width = (app->area->x + app->area->width) - rect.x;
        }
        if (rect.y < app->area->y && (rect.y + rect.height) > app->area->y) {
            rect.height -= (app->area->y - rect.y);
            rect.y = app->area->y;
        }
        if ((rect.y + rect.height) > (app->area->y + app->area->height)) {
            rect.height = (app->area->y + app->area->height) - rect.y;
        }

        if ((rect.x + rect.width) < app->area->x ||
            rect.x > (app->area->x + app->area->width) ||
            (rect.y + rect.height) < app->area->y ||
            rect.y > (app->area->y + app->area->height)) {
            rect.x = rect.width = rect.y = rect.height = 0;
        } else {
            // remember the damage done
            pthread_mutex_lock (&(app->damage_regions_mutex));
            XUnionRectWithRegion (&rect, job->dmg_region, job->dmg_region);
            pthread_mutex_unlock (&(app->damage_regions_mutex));
        }
        pthread_mutex_unlock (&(app->capturing_mutex));
    }

    return GDK_FILTER_CONTINUE;
}
#endif     // USE_XDAMAGE

/**
 * \brief this is what the thread spawned on record actually does. It is
 *      normally stopped by setting the state machine to VC_STOP
 */
void
do_record_thread ()
{
#define DEBUGFUNCTION "do_record_thread()"
    XVC_AppData *app = xvc_appdata_ptr ();
    Job *job = xvc_job_ptr ();
    long pause = 1000;

#ifdef DEBUG
    printf ("%s %s: Entering with state = %i\n", DEBUGFILE,
            DEBUGFUNCTION, job->state);
#endif     // DEBUG

    app->recording_thread_running = TRUE;

    // if the frame was moved before this, ignore
    job->frame_moved_x = 0;
    job->frame_moved_y = 0;

    if (!(job->flags & FLG_NOGUI)) {
        xvc_idle_add (xvc_change_filename_display, (void *) NULL);
        xvc_idle_add (xvc_frame_monitor, (void *) NULL);
    }

    while ((job->state & VC_READY) == 0) {
#ifdef DEBUG
        printf ("%s %s: going for next frame with state %i\n", DEBUGFILE,
                DEBUGFUNCTION, job->state);
#endif
        if ((job->state & VC_PAUSE) && !(job->state & VC_STEP)) {
            // make the led monitor stop for pausing
            xvc_led_time = 0;

            pthread_mutex_lock (&(app->recording_paused_mutex));
            pthread_cond_wait (&(app->recording_condition_unpaused),
                               &(app->recording_paused_mutex));
            pthread_mutex_unlock (&(app->recording_paused_mutex));
#ifdef DEBUG
            printf ("%s %s: unpaused\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
        }

        pause = job->capture ();

        if (pause > 0)
            usleep (pause * 1000);
#ifdef DEBUG
        printf ("%s %s: woke up\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
    }

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    app->recording_thread_running = FALSE;
    pthread_exit (NULL);
#undef DEBUGFUNCTION
}

/**
 * \brief implements those actions required for stopping a capture session
 *      that are not GUI related
 */
static void
stop_recording_nongui_stuff ()
{
#define DEBUGFUNCTION "stop_recording_nongui_stuff()"
    int state = 0;
    struct timeval curr_time;
    long stop_time = 0;
    XVC_AppData *app = xvc_appdata_ptr ();
    Job *jobp = xvc_job_ptr ();

#ifdef DEBUG
    printf ("%s %s: Entering with thread running %i\n",
            DEBUGFILE, DEBUGFUNCTION, app->recording_thread_running);
#endif     // DEBUG

    if (pause_time) {
        stop_time = pause_time;
    } else {
        gettimeofday (&curr_time, NULL);
        stop_time = curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000;
        time_captured += (stop_time - start_time);
    }

    if (stop_timer_id)
        g_source_remove (stop_timer_id);

    state = VC_STOP;
    if (app->flags & FLG_AUTO_CONTINUE && jobp->capture_returned_errno == 0) {
        state |= VC_CONTINUE;
    }
    xvc_job_set_state (state);

    if (app->recording_thread_running) {
        pthread_join (recording_thread, NULL);
#ifdef DEBUG
        printf ("%s %s: joined thread\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
    }
#ifdef USE_XDAMAGE
    if (app->flags & FLG_USE_XDAMAGE)
        gdk_window_remove_filter (NULL,
                                  (GdkFilterFunc) xvc_xdamage_event_filter,
                                  NULL);
#endif     // USE_XDAMAGE

    if ((jobp->flags & FLG_NOGUI) != 0 && jobp->capture_returned_errno != 0) {
        char file[PATH_MAX + 1];

        if (app->current_mode > 0) {
            sprintf (file, jobp->file, jobp->movie_no);
        } else {
            sprintf (file, jobp->file, jobp->pic_no);
        }

        fprintf (stderr, "Error Capturing\nFile used: %s\nError: %s\n",
                 file, strerror (jobp->capture_returned_errno));

        jobp->capture_returned_errno = 0;
    }
#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#undef DEBUGFUNCTION
}

/**
 * \brief displays the xvidcap user manual.
 */
void
do_results_help ()
{
    system ((char *) "yelp ghelp:xvidcap?xvidcap-results &");
}

/**
 * \brief implements the GUI related actions involved when stopping a
 *      capture session.
 */
static void
stop_recording_gui_stuff ()
{
#define DEBUGFUNCTION "stop_recording_gui_stuff()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    XVC_CapTypeOptions *target = NULL;
    Job *jobp = xvc_job_ptr ();
    XVC_AppData *app = xvc_appdata_ptr ();

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#ifdef USE_FFMPEG
    if (app->current_mode > 0)
        target = &(app->multi_frame);
    else
#endif     // USE_FFMPEG
        target = &(app->single_frame);

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_ctrl_main_window));
    g_assert (xml);

    // GUI stuff
    w = glade_xml_get_widget (xml, "xvc_ctrl_pause_toggle");
    g_assert (w);
    gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (w), FALSE);

    w = glade_xml_get_widget (xml, "xvc_ctrl_select_toggle");
    g_assert (w);
    gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);

    w = glade_xml_get_widget (xml, "xvc_ctrl_record_toggle");
    g_assert (w);
    gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
    if (gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON (w)))
        gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (w), FALSE);

    w = glade_xml_get_widget (xml, "xvc_ctrl_filename_button");
    g_assert (w);
    gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);

    w = glade_xml_get_widget (xml, "xvc_ctrl_edit_button");
    g_assert (w);
    gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);

    w = glade_xml_get_widget (xml, "xvc_ctrl_step_button");
    g_assert (w);
    gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);

    if (app->current_mode > 0) {

        if (xvc_is_filename_mutable (jobp->file) == TRUE) {
            if (jobp->movie_no > 0) {
                w = glade_xml_get_widget (xml, "xvc_ctrl_back_button");
                g_assert (w);
                gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
            }

            w = glade_xml_get_widget (xml, "xvc_ctrl_forward_button");
            g_assert (w);
            gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
        }
    } else {
        if (xvc_is_filename_mutable (jobp->file) == TRUE) {
            if (jobp->pic_no >= target->step) {
                w = glade_xml_get_widget (xml, "xvc_ctrl_back_button");
                g_assert (w);
                gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
            }

            if (target->frames == 0
                || jobp->pic_no < (target->frames - target->step)) {
                w = glade_xml_get_widget (xml, "xvc_ctrl_forward_button");
                g_assert (w);
                gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
            }
        }
    }

    w = glade_xml_get_widget (xml, "xvc_ctrl_pause_toggle");
    g_assert (w);
    if (gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON (w)))
        gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (w), FALSE);
    w = glade_xml_get_widget (xml, "xvc_ctrl_stop_toggle");
    g_assert (w);
    if (!gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON (w)))
        gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (w), TRUE);

    GtkChangeLabel (jobp->pic_no);

    if (app->flags & FLG_TO_TRAY) {
        gtk_widget_destroy (GTK_WIDGET (tray_frame_mon));
        tray_frame_mon = NULL;
        gtk_widget_destroy (GTK_WIDGET (tray_icon));
        tray_icon = NULL;
        gtk_widget_destroy (GTK_WIDGET (xvc_tray_icon_menu));
        xvc_tray_icon_menu = NULL;
        gtk_window_set_skip_taskbar_hint (GTK_WINDOW (xvc_ctrl_main_window),
                                          FALSE);
        gtk_window_deiconify (GTK_WINDOW (xvc_ctrl_main_window));
    }

    if (jobp->capture_returned_errno != 0) {
        GtkWidget *dialog = NULL;
        char file[PATH_MAX + 1];

        if (app->current_mode > 0) {
            sprintf (file, jobp->file, jobp->movie_no);
        } else {
            sprintf (file, jobp->file, jobp->pic_no);
        }

        dialog = gtk_message_dialog_new (GTK_WINDOW (xvc_warn_main_window),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         _("Error Capturing"));
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                  _("File used: %s\nError: %s"),
                                                  file,
                                                  g_strerror (jobp->
                                                              capture_returned_errno));
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);

        jobp->capture_returned_errno = 0;
    } else if ((strlen (target->file) < 1 ||
                (app->flags & FLG_ALWAYS_SHOW_RESULTS) > 0) &&
               (app->flags & FLG_AUTO_CONTINUE) == 0) {
        int result = 0;
        float fps_ratio = 0;
        char buf[100];

        GladeXML *xml = NULL;
        GtkWidget *w = NULL;

        // load the interface
        xml = glade_xml_new (XVC_GLADE_FILE, "xvc_result_dialog", NULL);
        g_assert (xml);
        // connect the signals in the interface
        glade_xml_signal_autoconnect (xml);
        // get the widget
        xvc_result_dialog = glade_xml_get_widget (xml, "xvc_result_dialog");
        g_assert (xvc_result_dialog);

        // width
        w = glade_xml_get_widget (xml, "xvc_result_dialog_width_label");
        g_assert (w);
        snprintf (buf, 99, "%i", app->area->width);
        gtk_label_set_text (GTK_LABEL (w), buf);

        // height
        w = glade_xml_get_widget (xml, "xvc_result_dialog_height_label");
        g_assert (w);
        snprintf (buf, 99, "%i", app->area->height);
        gtk_label_set_text (GTK_LABEL (w), buf);

        // video format
        w = glade_xml_get_widget (xml, "xvc_result_dialog_video_format_label");
        g_assert (w);
        gtk_label_set_text (GTK_LABEL (w),
                            _(xvc_formats[jobp->target].longname));

        // video codec
        w = glade_xml_get_widget (xml, "xvc_result_dialog_video_codec_label");
        g_assert (w);
        gtk_label_set_text (GTK_LABEL (w), xvc_codecs[jobp->targetCodec].name);

        // audio codec
        w = glade_xml_get_widget (xml, "xvc_result_dialog_audio_codec_label");
        g_assert (w);
        gtk_label_set_text (GTK_LABEL (w),
                            ((jobp->flags & FLG_REC_SOUND) ?
                             xvc_audio_codecs[jobp->au_targetCodec].name :
                             _("NONE")));

        // set fps
        w = glade_xml_get_widget (xml, "xvc_result_dialog_fps_label");
        g_assert (w);
        snprintf (buf, 99, "%.2f",
                  ((float) target->fps.num / (float) target->fps.den));
        gtk_label_set_text (GTK_LABEL (w), buf);

        // achieved fps
        w = glade_xml_get_widget (xml, "xvc_result_dialog_actual_fps_label");
        g_assert (w);
        {
            char *str_template = NULL;
            float total_frames =
                ((float) jobp->pic_no / (float) target->step) + 1;
            float actual_fps =
                ((float) total_frames) / ((float) time_captured / 1000);

            if (actual_fps >
                ((float) target->fps.num / (float) target->fps.den))
                actual_fps = (float) target->fps.num / (float) target->fps.den;
            fps_ratio =
                actual_fps / ((float) target->fps.num /
                              (float) target->fps.den);

            if (fps_ratio > (((float) LM_NUM_DAS - LM_LOW_THRESHOLD) / 10)) {
                str_template = "%.2f";
            } else if (fps_ratio >
                       (((float) LM_NUM_DAS - LM_MEDIUM_THRESHOLD) / 10)) {
                str_template = "<span background=\"#EEEE00\">%.2f</span>";
            } else {
                str_template = "<span background=\"#EE0000\">%.2f</span>";
            }

            snprintf (buf, 99, str_template, actual_fps);
            gtk_label_set_markup (GTK_LABEL (w), buf);
        }

        // fps ratio
        w = glade_xml_get_widget (xml,
                                  "xvc_result_dialog_fps_ratio_progressbar");
        g_assert (w);
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (w), fps_ratio);
        snprintf (buf, 99, "%.2f %%", fps_ratio * 100);
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (w), buf);

        // captured frames
        w = glade_xml_get_widget (xml, "xvc_result_dialog_total_frames_label");
        g_assert (w);
        snprintf (buf, 99, "%i", (jobp->pic_no / target->step) + 1);
        gtk_label_set_text (GTK_LABEL (w), buf);

        // captured time
        w = glade_xml_get_widget (xml, "xvc_result_dialog_video_length_label");
        g_assert (w);
        snprintf (buf, 99, "%.2f seconds", ((float) time_captured / 1000));
        gtk_label_set_text (GTK_LABEL (w), buf);

        if (strlen (target->file) > 0) {
            w = glade_xml_get_widget (xml,
                                      "xvc_result_dialog_select_filename_button");
            g_assert (w);
            gtk_widget_hide (GTK_WIDGET (w));

            w = glade_xml_get_widget (xml, "xvc_result_dialog_filename_label");
            g_assert (w);
            gtk_widget_hide (GTK_WIDGET (w));

            w = glade_xml_get_widget (xml, "xvc_result_dialog_no_button");
            g_assert (w);
            gtk_widget_hide (GTK_WIDGET (w));

            w = glade_xml_get_widget (xml, "xvc_result_dialog_save_button");
            g_assert (w);
            gtk_widget_hide (GTK_WIDGET (w));

            w = glade_xml_get_widget (xml,
                                      "xvc_result_dialog_show_next_time_checkbutton");
            g_assert (w);
            gtk_widget_show (GTK_WIDGET (w));

            w = glade_xml_get_widget (xml, "xvc_result_dialog_close_button");
            g_assert (w);
            gtk_widget_show (GTK_WIDGET (w));
        }

        do {
            result = gtk_dialog_run (GTK_DIALOG (xvc_result_dialog));

            switch (result) {
            case GTK_RESPONSE_OK:
                if (target_file_name != NULL) {
                    char cmd_buf[PATH_MAX * 2 + 5];
                    int errnum = 0;

                    snprintf (cmd_buf, (PATH_MAX * 2 + 5), "mv %s %s",
                              target->file, target_file_name);
                    errnum = system ((char *) cmd_buf);
                    if (errnum != 0) {
                        GtkWidget *err_dialog =
                            gtk_message_dialog_new (GTK_WINDOW
                                                    (xvc_ctrl_main_window),
                                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    GTK_MESSAGE_ERROR,
                                                    GTK_BUTTONS_CLOSE,
                                                    "Error moving file from '%s' to '%s'\n%s",
                                                    target->file,
                                                    target_file_name,
                                                    g_strerror (errnum));

                        gtk_dialog_run (GTK_DIALOG (err_dialog));
                        gtk_widget_destroy (err_dialog);
                    }
                }
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_CLOSE:
                w = glade_xml_get_widget (xml,
                                          "xvc_result_dialog_show_next_time_checkbutton");
                g_assert (w);
                if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
                    app->flags &= ~FLG_ALWAYS_SHOW_RESULTS;
                break;
            case GTK_RESPONSE_HELP:
                do_results_help ();
                break;
            case GTK_RESPONSE_ACCEPT:
                if (!app->current_mode) {
                    xvc_command_execute (target->play_cmd, 1, 0,
                                         target->file, target->start_no,
                                         jobp->pic_no, app->area->width,
                                         app->area->height, target->fps);
                } else {
                    xvc_command_execute (target->play_cmd, 2,
                                         jobp->movie_no, target->file,
                                         target->start_no, jobp->pic_no,
                                         app->area->width,
                                         app->area->height, target->fps);
                }
                break;
            default:
                break;
            }
        } while (result == GTK_RESPONSE_HELP || result == GTK_RESPONSE_ACCEPT);

        gtk_widget_destroy (xvc_result_dialog);
        xvc_result_dialog = NULL;

        // FIXME: realize move in a way that gives me real error codes
    }
#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#undef DEBUGFUNCTION
}

/**
 * \brief this function is hooked up as the action to execute when a
 *      capture session with max_time set times out.
 *
 * \todo might be able to trigger xvc_capture_stop_signal directly
 */
gboolean
timer_stop_recording ()
{
#define DEBUGFUNCTION "timer_stop_recording()"
    Job *jobp = xvc_job_ptr ();

    if ((jobp->flags & FLG_AUTO_CONTINUE) != 0) {
        xvc_job_merge_and_remove_state ((VC_STOP | VC_CONTINUE),
                                        (VC_START | VC_REC));
        return TRUE;
    } else {
        xvc_job_set_state (VC_STOP);
        return FALSE;
    }
#undef DEBUGFUNCTION
}

static gboolean
on_tray_icon_button_press_event (GtkWidget * widget,
                                 GdkEvent * event, gpointer user_data)
{
    GdkEventButton *bevent = NULL;

    g_assert (widget);
    g_assert (event);
    bevent = (GdkEventButton *) event;

    if (bevent->button == (guint) 3) {
        g_assert (xvc_tray_icon_menu);

        gtk_menu_popup (GTK_MENU (xvc_tray_icon_menu), NULL, NULL,
                        NULL, widget, bevent->button, bevent->time);
        // Tell calling code that we have handled this event; the buck
        // stops here.
        return TRUE;
    }
    return FALSE;
}

/**
 * \brief implements the GUI related actions involved in starting a
 *      capture session
 */
static void
start_recording_gui_stuff ()
{
#define DEBUGFUNCTION "start_recording_gui_stuff()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    Job *jobp = xvc_job_ptr ();
    XVC_CapTypeOptions *target = NULL;
    XVC_AppData *app = xvc_appdata_ptr ();

#ifdef USE_FFMPEG
    if (app->current_mode > 0)
        target = &(app->multi_frame);
    else
#endif     // USE_FFMPEG
        target = &(app->single_frame);

    if (app->flags & FLG_TO_TRAY) {
        GtkWidget *ebox = NULL, *hbox = NULL, *pause_cb;

//        GtkWidget *image = NULL;
//        GdkBitmap *bm = NULL;

        gtk_window_set_skip_taskbar_hint (GTK_WINDOW (xvc_ctrl_main_window),
                                          TRUE);

        tray_icon = GTK_WIDGET (egg_tray_icon_new ("xvc_tray_icon"));
        g_assert (tray_icon);
        ebox = gtk_event_box_new ();
        g_assert (ebox);
        hbox = gtk_hbox_new (FALSE, 2);
        g_assert (hbox);
        tray_frame_mon = NULL;
        tray_frame_mon = glade_create_led_meter ("tray_frame_mon", NULL,
                                                 NULL, 0, 0);
        g_assert (tray_frame_mon);
        gtk_container_add (GTK_CONTAINER (hbox), tray_frame_mon);

//        image = gtk_image_new_from_stock (GTK_STOCK_MEDIA_RECORD,
//                                          GTK_ICON_SIZE_SMALL_TOOLBAR);
//        g_assert (image);
//        gtk_container_add (GTK_CONTAINER (hbox), image);
        gtk_container_add (GTK_CONTAINER (ebox), hbox);
        gtk_container_add (GTK_CONTAINER (tray_icon), ebox);
        gtk_widget_show_all (tray_icon);

        g_signal_connect (G_OBJECT (ebox), "button_press_event",
                          G_CALLBACK (on_tray_icon_button_press_event), NULL);

        // load the interface
        xml = glade_xml_new (XVC_GLADE_FILE, "xvc_ti_menu", NULL);
        g_assert (xml);

        // connect the signals in the interface
        glade_xml_signal_autoconnect (xml);
        // store the toplevel widget for further reference
        xvc_tray_icon_menu = glade_xml_get_widget (xml, "xvc_ti_menu");
        if (jobp->state & VC_PAUSE) {
            pause_cb = glade_xml_get_widget (xml, "xvc_ti_pause");
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (pause_cb),
                                            TRUE);
        }

        gtk_window_iconify (GTK_WINDOW (xvc_ctrl_main_window));
    }
    xml = glade_get_widget_tree (GTK_WIDGET (xvc_ctrl_main_window));
    g_assert (xml);

    // GUI stuff
    w = glade_xml_get_widget (xml, "xvc_ctrl_record_toggle");
    g_assert (w);
    gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);

    w = glade_xml_get_widget (xml, "xvc_ctrl_stop_toggle");
    g_assert (w);
    if ((jobp->state & VC_PAUSE) == 0) {
        gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
    }
    gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (w), FALSE);

    w = glade_xml_get_widget (xml, "xvc_ctrl_select_toggle");
    g_assert (w);
    gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);

    w = glade_xml_get_widget (xml, "xvc_ctrl_pause_toggle");
    g_assert (w);
    gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);

    w = glade_xml_get_widget (xml, "xvc_ctrl_filename_button");
    g_assert (w);
    gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);

    w = glade_xml_get_widget (xml, "xvc_ctrl_edit_button");
    g_assert (w);
    gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);

    if (jobp->state & VC_PAUSE) {
        w = glade_xml_get_widget (xml, "xvc_ctrl_step_button");
        g_assert (w);
        if (app->current_mode == 0) {
            gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);

            if (target->frames == 0
                || jobp->pic_no <= (target->frames - target->step)) {
                w = glade_xml_get_widget (xml, "xvc_ctrl_forward_button");
                g_assert (w);
                gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
            }
            if (jobp->pic_no >= target->step) {
                w = glade_xml_get_widget (xml, "xvc_ctrl_back_button");
                g_assert (w);
                gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
            }
        } else {
            gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);
            if (!xvc_is_filename_mutable (target->file)) {
                w = glade_xml_get_widget (xml, "xvc_ctrl_forward_button");
                g_assert (w);
                gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);

                w = glade_xml_get_widget (xml, "xvc_ctrl_back_button");
                g_assert (w);
                gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);
            }
        }
    } else {
        w = glade_xml_get_widget (xml, "xvc_ctrl_forward_button");
        g_assert (w);
        gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);

        w = glade_xml_get_widget (xml, "xvc_ctrl_back_button");
        g_assert (w);
        gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);
    }
#undef DEBUGFUNCTION
}

/**
 * \brief implements those actions involved in starting a capture session
 *      that are not GUI related
 */
static void
start_recording_nongui_stuff ()
{
#define DEBUGFUNCTION "start_recording_nongui_stuff()"
    XVC_AppData *app = xvc_appdata_ptr ();

    if (!app->recording_thread_running) {
        struct timeval curr_time;
        Job *job = xvc_job_ptr ();
        XVC_CapTypeOptions *target = NULL;

#ifdef USE_FFMPEG
        if (app->current_mode > 0)
            target = &(app->multi_frame);
        else
#endif     // USE_FFMPEG
            target = &(app->single_frame);

        // the following also unsets VC_READY
        xvc_job_keep_and_merge_state (VC_PAUSE, (VC_REC | VC_START));

        if (app->current_mode > 0) {
            job->pic_no = target->start_no;
        }

        if ((job->state & VC_PAUSE) == 0) {
            // if (job->max_time != 0 && (job->state & VC_PAUSE) == 0) {
            gettimeofday (&curr_time, NULL);
            time_captured = 0;
            start_time = curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000;
            if (target->time != 0) {
                // install a timer which stops recording
                // we need milli secs ..
                stop_timer_id =
                    g_timeout_add ((guint32) (target->time * 1000),
                                   (GtkFunction) timer_stop_recording, job);
            }
        }
#ifdef USE_XDAMAGE
        if (job->flags & FLG_USE_XDAMAGE) {
            GdkDisplay *gdpy = gdk_x11_lookup_xdisplay (app->dpy);
            Window *children, root_return, parent_return;
            XWindowAttributes root_attrs;
            unsigned int nchildren, i;

            XGetWindowAttributes (app->dpy, app->root_window, &root_attrs);
            XSelectInput (app->dpy, app->root_window, StructureNotifyMask);
            XDamageCreate (app->dpy, app->root_window,
                           XDamageReportRawRectangles);
            XQueryTree (app->dpy, app->root_window,
                        &root_return, &parent_return, &children, &nchildren);

            for (i = 0; i < nchildren; i++) {
                XWindowAttributes attribs;
                Status ret;

                gdk_error_trap_push ();
                ret = XGetWindowAttributes (app->dpy, children[i], &attribs);
                gdk_error_trap_pop ();

                if (ret) {
//                    XSelectInput (app->dpy, children[i], StructureNotifyMask);
                    if (!attribs.
                        override_redirect
                        && attribs.depth == root_attrs.depth) {
//                        gdk_error_trap_push ();
                        XDamageCreate (app->dpy, children[i],
                                       XDamageReportRawRectangles);
//                        gdk_error_trap_pop ();
                    }
                }
            }
            XSync (app->dpy, False);
            XFree (children);

            gdk_window_add_filter (NULL,
                                   (GdkFilterFunc) xvc_xdamage_event_filter,
                                   NULL);

            g_assert (gdpy);
            gdk_x11_register_standard_event_type (gdpy,
                                                  app->dmg_event_base,
                                                  XDamageNumberEvents);
        }
#endif     // USE_XDAMAGE
        // initialize recording thread
        pthread_attr_init (&recording_thread_attr);
        pthread_attr_setdetachstate (&recording_thread_attr,
                                     PTHREAD_CREATE_JOINABLE);
        pthread_create (&recording_thread, &recording_thread_attr,
                        (void *) do_record_thread, (void *) job);

    }
#undef DEBUGFUNCTION
}

/**
 * \brief this positions the main control's menu (which is actually a
 *      popup menu a.k.a. context menu) in the same way it would if it
 *      were a normal menu
 *
 * @param menu pointer to the menu widget
 * @param x pointer to an int where the calculated x-position will be returned
 * @param y pointer to an int where the calculated y-position will be returned
 * @param push_in not actively used
 * @param user_data not actively used
 */
void
position_popup_menu (GtkMenu * menu, gint * x, gint * y,
                     gboolean * push_in, gpointer user_data)
{
#define DEBUGFUNCTION "position_popup_menu()"
    int pheight = 0, px = 0, py = 0, tx = 0, ty = 0;
    GtkWidget *w = NULL;

    w = GTK_WIDGET (user_data);

    g_return_if_fail (w != NULL);

    pheight = w->allocation.height;
    px = w->allocation.x;
    py = w->allocation.y;
    tx += px;
    ty += py;

    w = gtk_widget_get_toplevel (GTK_WIDGET (w));

    g_return_if_fail (w != NULL);

    gdk_window_get_origin (GDK_WINDOW (w->window), &px, &py);
    tx += px;
    ty += py;

    *x = tx;
    *y = ty + pheight;
#undef DEBUGFUNCTION
}

/**
 * \brief resets the main control window according to the preferences
 *      currently set
 */
void
xvc_reset_ctrl_main_window_according_to_current_prefs ()
{
#define DEBUGFUNCTION "xvc_reset_ctrl_main_window_according_to_current_prefs()"
    GladeXML *mwxml = NULL, *menuxml = NULL;
    GtkWidget *w = NULL;
    GtkTooltips *tooltips;
    Job *jobp = xvc_job_ptr ();
    XVC_CapTypeOptions *target = NULL;
    XVC_AppData *app = xvc_appdata_ptr ();

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#ifdef USE_FFMPEG
    if (app->current_mode > 0)
        target = &(app->multi_frame);
    else
#endif     // USE_FFMPEG
        target = &(app->single_frame);

    mwxml = glade_get_widget_tree (xvc_ctrl_main_window);
    g_assert (mwxml);
    menuxml = glade_get_widget_tree (xvc_ctrl_m1);
    g_assert (menuxml);

    // destroy or create a frame
    // and set show frame check button
    //
    w = NULL;
    w = glade_xml_get_widget (menuxml, "xvc_ctrl_m1_show_frame");
    g_return_if_fail (w != NULL);

    if (!(app->flags & FLG_NOFRAME)) {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w), TRUE);
    } else {
        // the callback destroys the frame
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w), FALSE);
    }

    //
    // the autocontinue menu item
    //
    // make autocontinue menuitem invisible if no ffmpeg
#ifndef USE_FFMPEG
    w = NULL;
    w = glade_xml_get_widget (menuxml, "xvc_ctrl_m1_mitem_autocontinue");
    g_return_if_fail (w != NULL);
    gtk_widget_hide (GTK_WIDGET (w));

    w = NULL;
    w = glade_xml_get_widget (menuxml, "xvc_ctrl_m1_separator1");
    g_return_if_fail (w != NULL);
    gtk_widget_hide (GTK_WIDGET (w));

#else      // USE_FFMPEG
    // the rest in case we have ffmpeg
    if (app->current_mode > 0) {
        w = glade_xml_get_widget (menuxml, "xvc_ctrl_m1_mitem_autocontinue");
        g_assert (w);

        if ((app->flags & FLG_AUTO_CONTINUE) != 0) {
            gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w), TRUE);
        } else {
            if (xvc_is_filename_mutable (jobp->file)) {
                gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
                gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w), FALSE);
            } else {
                gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);
                gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w), FALSE);
            }
        }
    } else {
        w = glade_xml_get_widget (menuxml, "xvc_ctrl_m1_mitem_autocontinue");
        g_assert (w);

        gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w), FALSE);
    }
#endif     // USE_FFMPEG

    //
    // the filename button
    //
    GtkChangeLabel (jobp->pic_no);

    // previous and next buttons have different meanings for on-the-fly
    // encoding and individual frame capture ...
    // this sets the tooltips accordingly
    if (app->current_mode == 0) {
        tooltips = gtk_tooltips_new ();
        g_assert (tooltips);

        w = NULL;
        w = glade_xml_get_widget (mwxml, "xvc_ctrl_back_button");
        g_assert (w);
        gtk_tool_item_set_tooltip (GTK_TOOL_ITEM (w), tooltips,
                                   _("Move cursor back one frame"),
                                   _("Move cursor back one frame"));
        if (jobp->pic_no >= target->step)
            gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
        else
            gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);

        w = glade_xml_get_widget (mwxml, "xvc_ctrl_forward_button");
        g_assert (w);
        gtk_tool_item_set_tooltip (GTK_TOOL_ITEM (w), tooltips,
                                   _("Move cursor to next frame"),
                                   _("Move cursor to next frame"));
        gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);

        w = glade_xml_get_widget (mwxml, "xvc_ctrl_filename_button");
        g_assert (w);
        gtk_tooltips_set_tip (GTK_TOOLTIPS (tooltips), GTK_WIDGET (w),
                              _
                              ("Left Click: Reset frame counter and filename\nRight Click: Popup Menu"),
                              _
                              ("Left Click: Reset frame counter and filename\nRight Click: Popup Menu"));

        w = glade_xml_get_widget (mwxml, "xvc_ctrl_edit_button");
        g_assert (w);
        gtk_tool_item_set_tooltip (GTK_TOOL_ITEM (w), tooltips,
                                   _("Edit current individual frame"),
                                   _("Edit current individual frame"));

        w = glade_xml_get_widget (mwxml, "xvc_ctrl_step_button");
        g_assert (w);
        gtk_tool_item_set_tooltip (GTK_TOOL_ITEM (w), tooltips,
                                   _("Capture single frame"),
                                   _("Capture single frame"));
    } else {
        GtkWidget *next = NULL, *previous = NULL, *filename = NULL, *w = NULL;

        next = glade_xml_get_widget (mwxml, "xvc_ctrl_forward_button");
        g_assert (next);
        previous = glade_xml_get_widget (mwxml, "xvc_ctrl_back_button");
        g_assert (previous);
        filename = glade_xml_get_widget (mwxml, "xvc_ctrl_filename_button");
        g_assert (filename);
        tooltips = gtk_tooltips_new ();
        g_assert (tooltips);

        if (xvc_is_filename_mutable (jobp->file)) {
            gtk_widget_set_sensitive (GTK_WIDGET (next), TRUE);
            if (jobp->movie_no > 0)
                gtk_widget_set_sensitive (GTK_WIDGET (previous), TRUE);
            else
                gtk_widget_set_sensitive (GTK_WIDGET (previous), FALSE);
        } else {
            gtk_widget_set_sensitive (GTK_WIDGET (next), FALSE);
            gtk_widget_set_sensitive (GTK_WIDGET (previous), FALSE);
        }
        gtk_tool_item_set_tooltip (GTK_TOOL_ITEM (previous), tooltips,
                                   _("Move cursor to previous movie"),
                                   _("Move cursor to previous movie"));
        gtk_tool_item_set_tooltip (GTK_TOOL_ITEM (next), tooltips,
                                   _("Move cursor to next movie"),
                                   _("Move cursor to next movie"));
        gtk_tooltips_set_tip (GTK_TOOLTIPS (tooltips),
                              GTK_WIDGET (filename),
                              _
                              ("Left Click: Reset movie counter to zero\nRight Click: Popup Menu"),
                              _
                              ("Left Click: Reset movie counter to zero\nRight Click: Popup Menu"));

        w = glade_xml_get_widget (mwxml, "xvc_ctrl_edit_button");
        g_assert (w);
        gtk_tool_item_set_tooltip (GTK_TOOL_ITEM (w), tooltips,
                                   _("Edit current movie"),
                                   _("Edit current movie"));

        w = glade_xml_get_widget (mwxml, "xvc_ctrl_step_button");
        g_assert (w);
        gtk_tool_item_set_tooltip (GTK_TOOL_ITEM (w), tooltips, NULL, NULL);
    }

    //
    // capture type radio buttons
    //
    // make capture type radio buttons invisible if no ffmpeg
#ifndef USE_FFMPEG
    w = NULL;
    w = glade_xml_get_widget (menuxml, "xvc_ctrl_m1_mitem_sf_capture");
    g_return_if_fail (w != NULL);
    gtk_widget_hide (GTK_WIDGET (w));

    w = NULL;
    w = glade_xml_get_widget (menuxml, "xvc_ctrl_m1_mitem_mf_capture");
    g_return_if_fail (w != NULL);
    gtk_widget_hide (GTK_WIDGET (w));
#else      // USE_FFMPEG

    if (app->current_mode == 0) {
        w = NULL;
        w = glade_xml_get_widget (menuxml, "xvc_ctrl_m1_mitem_mf_capture");
        g_return_if_fail (w != NULL);
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w), FALSE);

        w = NULL;
        w = glade_xml_get_widget (menuxml, "xvc_ctrl_m1_mitem_sf_capture");
        g_return_if_fail (w != NULL);
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w), TRUE);
    } else {
        w = NULL;
        w = glade_xml_get_widget (menuxml, "xvc_ctrl_m1_mitem_sf_capture");
        g_return_if_fail (w != NULL);
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w), FALSE);

        w = NULL;
        w = glade_xml_get_widget (menuxml, "xvc_ctrl_m1_mitem_mf_capture");
        g_return_if_fail (w != NULL);
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w), TRUE);
    }
#endif     // USE_FFMPEG

#undef DEBUGFUNCTION
}

/**
 * XVIDCAP GUI API<br>
 * ( IF THAT WORD IS ALLOWED HERE ;) )<br>
 * <br><table>
 * <tr><td>xvc_init_pre</td><td>do gui preintialization like setting fallback options, initializing thread libraries and the like</td></tr>
 * <tr><td>xvc_ui_create</td><td>create the gui</td></tr>
 * <tr><td>xvc_frame_create</td><td>create the frame for selecting the area to capture</td></tr>
 * <tr><td>xvc_check_start_options</td><td>check the preferences on program start</td></tr>
 * <tr><td>xvc_ui_init</td><td>gui initialization</td></tr>
 * <tr><td>xvc_ui_run</td><td>start the ui's main loop</td></tr>
 * <tr><td>xvc_idle_add</td><td>queue an idle action</td></tr>
 * <tr><td>xvc_change_filename_display</td><td>update the display of the current frame/file</td></tr>
 * <tr><td>xvc_capture_stop_signal</td><td>tell the ui you want it to stop recording</td></tr>
 * <tr><td>xvc_capture_stop</td><td>implements the functions for actually stopping</td></tr>
 * <tr><td>xvc_capture_start</td><td>implements the functions for starting a recording</td></tr>
 * <tr><td>xvc_frame_change</td><td>change the area to capture</td></tr>
 * <tr><td>xvc_frame_monitor</td><td>update a widget monitoring frame rate</td></tr>
 * </table>
 */

/**
 * \brief does gui preintialization, mainly initializing thread libraries
 *      and calling gtk_init with the command line arguments
 *
 * @param argc number of command line arguments
 * @param argv pointer to the command line arguments
 * @return Could be FALSE for failure, but de facto always TRUE
 */
Boolean
xvc_init_pre (int argc, char **argv)
{
#define DEBUGFUNCTION "xvc_init_pre()"
    g_thread_init (NULL);
    gdk_threads_init ();

    gtk_init (&argc, &argv);
    return TRUE;
#undef DEBUGFUNCTION
}

/**
 * \brief creates the main control and menu from the glade definition,
 *      connects the signals and stores the widget pointers for further
 *      reference.
 *
 * @return Could be FALSE for failure, but de facto always TRUE
 */
Boolean
xvc_ui_create ()
{
#define DEBUGFUNCTION "xvc_ui_create()"
    GladeXML *xml = NULL;
    XVC_AppData *app = xvc_appdata_ptr ();
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    // only show the ui if not in nogui
    if ((app->flags & FLG_NOGUI) == 0) {
        // main window
        // load the interface
        xml = glade_xml_new (XVC_GLADE_FILE, "xvc_ctrl_main_window", NULL);

        g_assert (xml);

        // connect the signals in the interface
        glade_xml_signal_autoconnect (xml);

        // store the toplevel widget for further reference
        xvc_ctrl_main_window =
            glade_xml_get_widget (xml, "xvc_ctrl_main_window");

#if GTK_CHECK_VERSION(2, 5, 0)
#else
        {
            GtkWidget *w = NULL;

/*
 *            replace the following stock ids
 *
gtk-media-stop gtk-stop
gtk-media-pause gtk-go-up (funky, but it works)
gtk-media-record gtk-convert
gtk-media-next gtk-goto-last
gtk-media-rewind gtk-go-back
gtk-media-forward gtk-go-forward
gtk-edit gtk-paste (2nd choice would be gtk-open)
*
*/
            w = glade_xml_get_widget (xml, "xvc_ctrl_stop_toggle");
            if (w) {
                gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (w), NULL);
                gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON (w), "gtk-stop");
            }

            w = glade_xml_get_widget (xml, "xvc_ctrl_pause_toggle");
            if (w) {
                gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (w), NULL);
                gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON (w), "gtk-go-up");
            }

            w = glade_xml_get_widget (xml, "xvc_ctrl_record_toggle");
            if (w) {
                gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (w), NULL);
                gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON (w),
                                              "gtk-convert");
            }

            w = glade_xml_get_widget (xml, "xvc_ctrl_step_button");
            if (w) {
                gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (w), NULL);
                gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON (w),
                                              "gtk-goto-last");
            }

            w = glade_xml_get_widget (xml, "xvc_ctrl_back_button");
            if (w) {
                gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (w), NULL);
                gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON (w),
                                              "gtk-go-back");
            }

            w = glade_xml_get_widget (xml, "xvc_ctrl_forward_button");
            if (w) {
                gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (w), NULL);
                gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON (w),
                                              "gtk-go-forward");
            }

            w = glade_xml_get_widget (xml, "xvc_ctrl_edit_button");
            if (w) {
                gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (w), NULL);
                gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON (w), "gtk-paste");
            }

            w = NULL;
        }
#endif     // GTK_CHECK_VERSION

#if GTK_CHECK_VERSION(2, 6, 0)
#else
        {
            GtkWidget *w = NULL;

            w = glade_xml_get_widget (xml, "xvc_ctrl_m1_mitem_about");
            if (w)
                gtk_widget_hide (w);
        }
#endif     // GKT_CHECK_VERSION

        xml = NULL;
        // popup window
        // load the interface
        xml = glade_xml_new (XVC_GLADE_FILE, "xvc_ctrl_m1", NULL);

        g_assert (xml);

        xvc_ctrl_m1 = glade_xml_get_widget (xml, "xvc_ctrl_m1");

        // this needs to go here to avoid double frame when done later
        // should trigger any callback here
        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_ctrl_m1_show_frame");
        g_assert (w);

        if (!(app->flags & FLG_NOFRAME)) {
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w), TRUE);
        } else {
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w), FALSE);
        }

        // connect the signals in the interface
        glade_xml_signal_autoconnect (xml);

    }
#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    return TRUE;
#undef DEBUGFUNCTION
}

/**
 * \brief creates the frame around the area to capture by calling the
 *      actual implementation in xvc_create_gtk_frame
 *
 * @see xvc_create_gtk_frame
 * @param win a Window (ID) to allow passing a window id for selecting
 *      the area to capture (i.e. capture that window ... but not if you
 *      move the window afterwards)
 * @return Could be FALSE for failure, but de facto always TRUE
 */
Boolean
xvc_frame_create (Window win)
{
#define DEBUGFUNCTION "xvc_frame_create()"
    XVC_AppData *app = xvc_appdata_ptr ();

    if ((app->flags & FLG_NOGUI) == 0) {    /* there's one good reason for not
                                             * having a main window */
        g_assert (xvc_ctrl_main_window);
    }

    if (win == None) {
        // display and window attributes seem to be set correctly only if
        // retrieved after the UI was mapped
        gtk_init_add ((GtkFunction) xvc_appdata_set_window_attributes,
                      (void *) app->root_window);

        if (app->area->width == 0)
            app->area->width = 10;
        if (app->area->height == 0)
            app->area->height = 10;

        xvc_create_gtk_frame (xvc_ctrl_main_window, app->area->width,
                              app->area->height, app->area->x, app->area->y);
    } else {
        int x, y;
        Window temp = None;

        // display and window attributes seem to be set correctly only if
        // retrieved after the UI was mapped
        gtk_init_add ((GtkFunction) xvc_appdata_set_window_attributes,
                      (void *) win);
        XTranslateCoordinates (app->dpy, win, app->root_window,
                               0, 0, &x, &y, &temp);

        app->area->x = x;
        app->area->y = y;
        app->area->width = app->win_attr.width;
        app->area->height = app->win_attr.height;

        xvc_create_gtk_frame (xvc_ctrl_main_window, app->area->width,
                              app->area->height, app->area->x, app->area->y);
    }
    return TRUE;
#undef DEBUGFUNCTION
}

/**
 * \brief checks the current preferences and should be used to check
 *      preferences right before running the UI
 *
 * This does not return anything but must react on errors found on its own.
 * It is global mainly for allowing it to be called from the warning dialog
 * in case the startup check produced an error with a default action, you
 * clicked OK and then the validation needs to run again.
 */
void
xvc_check_start_options ()
{
#define DEBUGFUNCTION "xvc_check_start_options()"
    int count_non_info_messages = 0;
    int rc = 0;
    XVC_AppData *app = xvc_appdata_ptr ();

#ifdef DEBUG
    printf ("%s %s: Entering with errors_after_cli = %p\n", DEBUGFILE,
            DEBUGFUNCTION, errors_after_cli);
#endif     // DEBUG

    if (OK_attempts > 0 && errors_after_cli != NULL) {
        errors_after_cli = xvc_appdata_validate (app, 1, &rc);

#ifdef DEBUG
        printf ("%s %s: new errors_after_cli = %p\n", DEBUGFILE,
                DEBUGFUNCTION, errors_after_cli);
#endif     // DEBUG

        if (rc == -1) {
            fprintf (stderr,
                     "%s %s: Unrecoverable error while validating options, please contact the xvidcap project.\n",
                     DEBUGFILE, DEBUGFUNCTION);
            exit (1);
        }
    }

    if ((app->flags & FLG_NOGUI) == 0) {    // we're running with gui
        if (errors_after_cli != NULL) {
            XVC_ErrorListItem *err;

            err = errors_after_cli;
            count_non_info_messages = 0;

            for (; err != NULL; err = err->next) {
                if (err->err->type != XVC_ERR_INFO)
                    count_non_info_messages++;
            }
            if (count_non_info_messages > 0
                || (app->flags & FLG_RUN_VERBOSE && OK_attempts == 0)) {
                xvc_warn_main_window =
                    xvc_create_warning_with_errors (errors_after_cli, 2);
                OK_attempts++;
            } else {
                warning_submit ();
            }
        } else {
            warning_submit ();
        }
    } else {                           // or without
        while (errors_after_cli != NULL && OK_attempts < 6) {
            XVC_ErrorListItem *err;

            err = errors_after_cli;
            count_non_info_messages = rc = 0;

            for (; err != NULL; err = err->next) {
                if (err->err->type != XVC_ERR_INFO)
                    count_non_info_messages++;
            }
            if (count_non_info_messages > 0
                || (app->flags & FLG_RUN_VERBOSE && OK_attempts == 0)) {
                err = errors_after_cli;
                for (; err != NULL; err = err->next) {
                    if (err->err->type != XVC_ERR_INFO ||
                        app->flags & FLG_RUN_VERBOSE) {
                        xvc_error_write_msg (err->err->code,
                                             ((app->
                                               flags &
                                               FLG_RUN_VERBOSE) ? 1 : 0));
                        (*err->err->action) (err);
                    }
                }
            }

            OK_attempts++;

            errors_after_cli = xvc_appdata_validate (app, 1, &rc);
            if (rc == -1) {
                fprintf (stderr,
                         "%s %s: Unrecoverable error while validating options, please contact the xvidcap project.\n",
                         DEBUGFILE, DEBUGFUNCTION);
                exit (1);
            }
        }

        if (errors_after_cli != NULL && count_non_info_messages > 0) {
            fprintf (stderr,
                     "%s %s: You have specified some conflicting settings which could not be resolved automatically.\n",
                     DEBUGFILE, DEBUGFUNCTION);
            exit (1);
        }
        warning_submit ();
    }

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#undef DEBUGFUNCTION
}

/**
 * \brief initializes the UI by mainly ensuring xvc_check_start_options is
 *      called with the errors found in main
 *
 * @see xvc_check_start_options
 * @param errors the errors in the preferences found in the main function
 * @return Could be FALSE for failure, but de facto always TRUE
 */
Boolean
xvc_ui_init (XVC_ErrorListItem * errors)
{
#define DEBUGFUNCTION "xvc_ui_init()"
    XVC_AppData *app = xvc_appdata_ptr ();

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    // display warning dialog if required
    errors_after_cli = errors;
    // the gui warning dialog needs a realized window, therefore we
    // schedule it do be displayed (potentially) when the main loop starts
    // this does not seem to work with nogui
    if (!(app->flags & FLG_NOGUI)) {
        gtk_init_add ((GtkFunction) xvc_check_start_options, NULL);
    } else {
        xvc_check_start_options ();
        gtk_init_add ((GtkFunction) xvc_capture_start, NULL);
    }

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    return TRUE;
#undef DEBUGFUNCTION
}

/**
 * \brief runs the UI by executing the main loop
 *
 * @return Could be any integer for failure, but de facto always 0
 */
int
xvc_ui_run ()
{
#define DEBUGFUNCTION "xvc_ui_run()";

    gtk_main ();

    return 0;
#undef DEBUGFUNCTION
}

/**
 * \brief adds an action to the event queue to be executed when there's time
 *
 * The functions added here are executed whenever there's time until they
 * return FALSE.
 *
 * @param func a pointer to a function to queue
 * @param data a pointer to an argument to that function
 */
void
xvc_idle_add (void *func, void *data)
{
    g_idle_add (func, data);
}

/**
 * \brief updates the display of the current frame/filename according to
 *      the current state of job.
 *
 * This is meant to be used through xvc_idle_add during recording
 * @return TRUE for as long as the recording thread is running or FALSE
 *      otherwise
 */
Boolean
xvc_change_filename_display ()
{
#define DEBUGFUNCTION "xvc_change_filename_display()"
#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
    XVC_AppData *app = xvc_appdata_ptr ();
    Job *job = xvc_job_ptr ();
    int ret = app->recording_thread_running;

    if (!ret) {
        last_pic_no = 0;
        GtkChangeLabel (job->pic_no);
    } else if (job->pic_no != last_pic_no) {
        last_pic_no = job->pic_no;
        GtkChangeLabel (last_pic_no);
    }
#ifdef DEBUG
    printf ("%s %s: Leaving! ...continuing? %i\n", DEBUGFILE, DEBUGFUNCTION,
            (int) ret);
#endif     // DEBUG

    return ret;
#undef DEBUGFUNCTION
}

/**
 * \brief tell the recording thread to stop
 *
 * The actual stopping is done through the capture's state machine. This
 * just sets the state to VC_STOP and sends the recording thread an ALARM
 * signal to terminate a potential usleep. It may then wait for the thread
 * to finish.
 * @param wait should this function actually wait for the thread to finish?
 */
void
xvc_capture_stop_signal (Boolean wait)
{
#define DEBUGFUNCTION "xvc_capture_stop_signal()"
    int status = -1;
    XVC_AppData *app = xvc_appdata_ptr ();

#ifdef DEBUG
    printf ("%s %s: Entering, should we wait? %i\n",
            DEBUGFILE, DEBUGFUNCTION, wait);
#endif     // DEBUG

    xvc_job_set_state (VC_STOP);

    if (app->recording_thread_running) {
#ifdef DEBUG
        printf ("%s %s: stop pressed while recording\n",
                DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
        // stop waiting for next frame to capture
        status = pthread_kill (recording_thread, SIGALRM);
#ifdef DEBUG
        printf ("%s %s: thread %i kill with rc %i\n",
                DEBUGFILE, DEBUGFUNCTION, (int) recording_thread, status);
#endif     // DEBUG
    }

    if (wait) {
        while (app->recording_thread_running) {
            usleep (100);
        }
    }
#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

/**
 * \brief implements the actual actions required when stopping a recording
 *      session, both GUI related an not.
 */
Boolean
xvc_capture_stop ()
{
#define DEBUGFUNCTION "xvc_capture_stop()"
    Job *job = xvc_job_ptr ();

#ifdef DEBUG
    printf ("%s %s: stopping\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
    stop_recording_nongui_stuff (job);
#ifdef DEBUG
    printf ("%s %s: done stopping non-gui stuff\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
    if (!(job->flags & FLG_NOGUI)) {
        gdk_threads_enter ();
        stop_recording_gui_stuff (job);
        gdk_flush ();
        gdk_threads_leave ();
    } else {
        gtk_main_quit ();
    }

    return FALSE;
#undef DEBUGFUNCTION
}

/**
 * \brief starts a recording session
 */
void
xvc_capture_start ()
{
#define DEBUGFUNCTION "xvc_capture_start()"
    Job *job = xvc_job_ptr ();

    start_recording_nongui_stuff (job);
    if (!(job->flags & FLG_NOGUI))
        start_recording_gui_stuff (job);

#undef DEBUGFUNCTION
}

/*
 * \brief changes the frame around the area to capture by calling the
 *      implementing function xvc_change_gtk_frame
 *
 * @see xvc_change_gtk_frame
 */
void
xvc_frame_change (int x, int y, int width, int height,
                  Boolean reposition_control, Boolean show_dimensions)
{
#define DEBUGFUNCTION "xvc_frame_change()"
    xvc_change_gtk_frame (x, y, width, height, reposition_control,
                          show_dimensions);
#undef DEBUGFUNCTION
}

/**
 * \brief updates the widget monitoring the current frame rate based on
 *      the current value of xvc_led_time and last_led_time
 *
 * This is meant to be used through xvc_idle_add during recording
 * @return TRUE for as long as the recording thread is running or FALSE
 *      otherwise
 * @see xvc_led_time
 * @see last_led_time
 * @see xvc_idle_add
 */
Boolean
xvc_frame_monitor ()
{
#define DEBUGFUNCTION "xvc_frame_monitor()"

    Job *job = xvc_job_ptr ();
    XVC_AppData *app = xvc_appdata_ptr ();
    int percent = 0, diff = 0;
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    int ret = app->recording_thread_running;

#ifdef DEBUG
    printf ("%s %s: Entering with time = %i\n", DEBUGFILE, DEBUGFUNCTION,
            xvc_led_time);
#endif     // DEBUG

    // fastpath
    if (xvc_led_time != 0 && last_led_time == xvc_led_time)
        return TRUE;

    if (app->flags & FLG_TO_TRAY && tray_frame_mon) {
        w = tray_frame_mon;
        g_return_val_if_fail (w != NULL, FALSE);
    } else {
        xml = glade_get_widget_tree (xvc_ctrl_main_window);
        g_return_val_if_fail (xml != NULL, FALSE);
        w = glade_xml_get_widget (xml, "xvc_ctrl_led_meter");
        g_return_val_if_fail (w != NULL, FALSE);
    }

    if (!ret) {
        xvc_led_time = last_led_time = 0;
    }

    if (xvc_led_time == 0) {
        percent = 0;
    } else if (xvc_led_time <= job->time_per_frame)
        percent = 30;
    else if (xvc_led_time >= (job->time_per_frame * 2))
        percent = 100;
    else {
        diff = xvc_led_time - job->time_per_frame;
        percent = diff * 70 / job->time_per_frame;
        percent += 30;
    }

    led_meter_set_percent (LED_METER (w), percent);

    if (percent == 0)
        LED_METER (w)->old_max_da = 0;

#ifdef DEBUG
    printf ("%s %s: Leaving with percent = %i ... continuing? %i\n",
            DEBUGFILE, DEBUGFUNCTION, percent, ret);
#endif     // DEBUG

    return ret;
#undef DEBUGFUNCTION
}

/*
 * callbacks here ....
 *
 */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
gboolean
on_xvc_ctrl_main_window_delete_event (GtkWidget * widget,
                                      GdkEvent * event, gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_main_window_delete_event()"
    Job *jobp = xvc_job_ptr ();

    if (jobp && (jobp->state & VC_STOP) == 0) {
        xvc_capture_stop_signal (TRUE);
    }

    xvc_destroy_gtk_frame ();
    gtk_main_quit ();                  /** \todo why does this seem to be
necessary with libglade where it was not previously */
    return FALSE;
#undef DEBUGFUNCTION
}

gboolean
on_xvc_ctrl_main_window_destroy_event (GtkWidget * widget,
                                       GdkEvent * event, gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_main_window_destroy_event()"
    gtk_main_quit ();
    return FALSE;
#undef DEBUGFUNCTION
}

void
on_xvc_ctrl_m1_mitem_preferences_activate (GtkMenuItem * menuitem,
                                           gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_m1_mitem_preferences_activate()"
    XVC_AppData *app = xvc_appdata_ptr ();

#ifdef DEBUG
    printf ("%s %s: Entering with app %p\n", DEBUGFILE, DEBUGFUNCTION, app);
#endif     // DEBUG

    xvc_create_pref_dialog (app);

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

void
on_xvc_ctrl_m1_mitem_save_preferences_activate (GtkMenuItem *
                                                menuitem, gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_m1_mitem_save_preferences_activate()"
    Job *jobp = xvc_job_ptr ();

    xvc_write_options_file (jobp);
#undef DEBUGFUNCTION
}

void
on_xvc_ctrl_m1_mitem_sf_capture_activate (GtkMenuItem * menuitem,
                                          gpointer user_data)
{
#ifdef USE_FFMPEG
#define DEBUGFUNCTION "on_xvc_ctrl_m1_mitem_sf_capture_activate()"
    XVC_AppData *app = xvc_appdata_ptr ();

    if (app->current_mode == 1
        && gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menuitem)) !=
        0) {
        app->current_mode = (app->current_mode == 0) ? 1 : 0;
        xvc_toggle_cap_type ();
    }
#undef DEBUGFUNCTION
#endif     // USE_FFMPEG
}

void
on_xvc_ctrl_m1_mitem_mf_capture_activate (GtkMenuItem * menuitem,
                                          gpointer user_data)
{
#ifdef USE_FFMPEG
#define DEBUGFUNCTION "on_xvc_ctrl_m1_mitem_mf_capture_activate()"
    XVC_AppData *app = xvc_appdata_ptr ();

    if (app->current_mode == 0
        && gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menuitem)) !=
        0) {
        app->current_mode = (app->current_mode == 0) ? 1 : 0;
        xvc_toggle_cap_type ();
    }
#undef DEBUGFUNCTION
#endif     // USE_FFMPEG
}

void
on_xvc_ctrl_m1_mitem_autocontinue_activate (GtkMenuItem * menuitem,
                                            gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_m1_mitem_autocontinue_activate()"
    Job *jobp = xvc_job_ptr ();
    XVC_AppData *app = xvc_appdata_ptr ();

    if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menuitem))) {
        if ((!xvc_is_filename_mutable (jobp->file))
            || app->current_mode == 0) {
            printf
                ("Output not a video file or no counter in filename\nDisabling autocontinue!\n");
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM
                                            (menuitem), FALSE);
        } else {
            jobp->flags |= FLG_AUTO_CONTINUE;
            app->flags |= FLG_AUTO_CONTINUE;
        }
    } else {
        jobp->flags &= ~FLG_AUTO_CONTINUE;
        app->flags &= ~FLG_AUTO_CONTINUE;
    }
#undef DEBUGFUNCTION
}

void
on_xvc_ctrl_m1_show_frame_activate (GtkMenuItem * menuitem, gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_m1_show_frame_activate()"
    Job *jobp = xvc_job_ptr ();
    XVC_AppData *app = xvc_appdata_ptr ();

    if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menuitem))) {
        app->flags &= ~FLG_NOFRAME;
        jobp->flags &= ~FLG_NOFRAME;
        xvc_create_gtk_frame (xvc_ctrl_main_window, app->area->width,
                              app->area->height, app->area->x, app->area->y);
    } else {
        jobp->flags |= FLG_NOFRAME;
        app->flags |= FLG_NOFRAME;
        xvc_destroy_gtk_frame ();
    }
#undef DEBUGFUNCTION
}

void
on_xvc_ctrl_m1_mitem_make_activate (GtkMenuItem * menuitem, gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_m1_mitem_make_activate()"
    Job *jobp = xvc_job_ptr ();
    XVC_CapTypeOptions *target = NULL;
    XVC_AppData *app = xvc_appdata_ptr ();

#ifdef USE_FFMPEG
    if (app->current_mode > 0)
        target = &(app->multi_frame);
    else
#endif     // USE_FFMPEG
        target = &(app->single_frame);

    if (!app->current_mode) {
        xvc_command_execute (target->video_cmd, 0, 0,
                             jobp->file, target->start_no, jobp->pic_no,
                             app->area->width, app->area->height, target->fps);
    } else {
        xvc_command_execute (target->video_cmd, 2,
                             jobp->movie_no, jobp->file, target->start_no,
                             jobp->pic_no, app->area->width,
                             app->area->height, target->fps);
    }
#undef DEBUGFUNCTION
}

void
on_xvc_ctrl_m1_mitem_help_activate (GtkMenuItem * menuitem, gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_m1_mitem_help_activate()"
    system ("yelp ghelp:xvidcap &");
#undef DEBUGFUNCTION
}

void
on_xvc_ctrl_m1_mitem_quit_activate (GtkMenuItem * menuitem, gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_m1_mitem_quit_activate()"
    gboolean ignore = TRUE;

    g_signal_emit_by_name ((GtkObject *) xvc_ctrl_main_window,
                           "destroy_event", 0, &ignore);
#undef DEBUGFUNCTION
}

gint
on_xvc_result_dialog_key_press_event (GtkWidget * widget, GdkEvent * event)
{
#define DEBUGFUNCTION "on_xvc_result_dialog_key_press_event()"
    GdkEventKey *kevent = NULL;

    g_assert (widget);
    g_assert (event);
    kevent = (GdkEventKey *) event;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    if (kevent->keyval == 65470) {
        do_results_help ();
    }
#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    // Tell calling code that we have not handled this event; pass it on.
    return FALSE;
#undef DEBUGFUNCTION
}

void
on_xvc_result_dialog_select_filename_button_clicked (GtkButton * button,
                                                     gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_result_select_filename_button_clicked()"
    int result = 0;

    GladeXML *xml = NULL;
    GtkWidget *w = NULL, *dialog = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    // load the interface
    xml = glade_xml_new (XVC_GLADE_FILE, "xvc_save_filechooserdialog", NULL);
    g_assert (xml);
    // connect the signals in the interface
    glade_xml_signal_autoconnect (xml);

    dialog = glade_xml_get_widget (xml, "xvc_save_filechooserdialog");
    g_assert (dialog);
    gtk_window_set_title (GTK_WINDOW (dialog), "Save Video As:");

    result = gtk_dialog_run (GTK_DIALOG (dialog));

    if (result == GTK_RESPONSE_OK) {
        target_file_name =
            gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

        if (xvc_result_dialog != NULL) {
            xml = NULL;
            xml = glade_get_widget_tree (GTK_WIDGET (xvc_result_dialog));
            g_assert (xml);

            w = NULL;
            w = glade_xml_get_widget (xml, "xvc_result_dialog_filename_label");
            g_assert (w);

            gtk_label_set_text (GTK_LABEL (w), target_file_name);

            w = NULL;
            w = glade_xml_get_widget (xml, "xvc_result_dialog_save_button");
            g_assert (w);
            printf ("setting save button sensitive\n");
            gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
        }
    }

    gtk_widget_destroy (dialog);

#ifdef DEBUG
    printf ("%s %s: Leaving with filename %s\n", DEBUGFILE, DEBUGFUNCTION,
            target_file_name);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

void
on_xvc_ctrl_stop_toggle_toggled (GtkToggleToolButton * button,
                                 gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_stop_toggle_toggled()"
    XVC_AppData *app = xvc_appdata_ptr ();

#ifdef DEBUG
    printf ("%s %s: stopp button toggled (%i)\n", DEBUGFILE, DEBUGFUNCTION,
            gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON
                                               (button)));
#endif     // DEBUG

    if (gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON (button))) {
        if (app->recording_thread_running)
            xvc_capture_stop_signal (FALSE);
        else {
            GladeXML *xml = NULL;
            GtkWidget *w = NULL;

            xml = glade_get_widget_tree (GTK_WIDGET (xvc_ctrl_main_window));
            g_assert (xml);
            w = glade_xml_get_widget (xml, "xvc_ctrl_pause_toggle");
            g_assert (w);
            if (gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON (w)))
                gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON
                                                   (w), FALSE);
        }
    } else {
        // empty
    }
#undef DEBUGFUNCTION
}

void
on_xvc_ctrl_record_toggle_toggled (GtkToggleToolButton * button,
                                   gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_record_toggle_toggled()"

    if (gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON (button))) {
        xvc_capture_start ();
    } else {
        // empty
    }
#undef DEBUGFUNCTION
}

void
on_xvc_ctrl_pause_toggle_toggled (GtkToggleToolButton * button,
                                  gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_pause_toggle_toggled()"
    struct timeval curr_time;
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    Job *jobp = xvc_job_ptr ();
    XVC_CapTypeOptions *target = NULL;
    XVC_AppData *app = xvc_appdata_ptr ();

#ifdef USE_FFMPEG
    if (app->current_mode > 0)
        target = &(app->multi_frame);
    else
#endif     // USE_FFMPEG
        target = &(app->single_frame);

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_ctrl_main_window));
    g_assert (xml);

#ifdef DEBUG
    printf ("%s %s: is paused? (%d)\n", DEBUGFILE, DEBUGFUNCTION,
            gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON
                                               (button)));
#endif

    if (gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON (button))) {
        gettimeofday (&curr_time, NULL);
        pause_time = curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000;
        if ((jobp->state & VC_REC) != 0) {
            time_captured += (pause_time - start_time);
        } else {
            time_captured = 0;
        }
        // stop timer handling only if max_time is configured
        if (target->time != 0) {
            if (stop_timer_id)
                g_source_remove (stop_timer_id);
        }
        xvc_job_merge_and_remove_state (VC_PAUSE, VC_STOP);

        // GUI stuff
        w = glade_xml_get_widget (xml, "xvc_ctrl_stop_toggle");
        g_assert (w);
        gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (w), FALSE);

        if (app->current_mode == 0 && jobp->state & VC_REC) {
            if (target->frames == 0
                || jobp->pic_no <= (target->frames - target->step)) {
                w = glade_xml_get_widget (xml, "xvc_ctrl_step_button");
                g_assert (w);
                gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);

                w = glade_xml_get_widget (xml, "xvc_ctrl_forward_button");
                g_assert (w);
                gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
            }

            if (jobp->pic_no >= target->step) {
                w = glade_xml_get_widget (xml, "xvc_ctrl_back_button");
                g_assert (w);
                gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
            }

            w = glade_xml_get_widget (xml, "xvc_ctrl_edit_button");
            g_assert (w);
            gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
        }
    } else {
        xvc_job_remove_state (VC_PAUSE | VC_STEP);
        // step is always only active if a running recording session is
        // paused
        // so releasing pause can always deactivate it

        w = glade_xml_get_widget (xml, "xvc_ctrl_step_button");
        g_assert (w);
        gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);
        // the following only when recording is going on (not when just
        // pressing and
        // releasing pause
        if (jobp->state & VC_REC) {
            w = glade_xml_get_widget (xml, "xvc_ctrl_forward_button");
            g_assert (w);
            gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);

            w = glade_xml_get_widget (xml, "xvc_ctrl_back_button");
            g_assert (w);
            gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);

            w = glade_xml_get_widget (xml, "xvc_ctrl_edit_button");
            g_assert (w);
            gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);
        } else {
            xvc_job_merge_state (VC_STOP);
        }

        pause_time = 0;
        gettimeofday (&curr_time, NULL);
        start_time = curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000;
        // restart timer handling only if max_time is configured
        if (target->time != 0) {
            // install a timer which stops recording
            // we need milli secs ..
            stop_timer_id = g_timeout_add ((guint32)
                                           (target->time * 1000 -
                                            time_captured), (GtkFunction)
                                           timer_stop_recording, jobp);
        }
    }

#undef DEBUGFUNCTION
}

void
on_xvc_ctrl_step_button_clicked (GtkButton * button, gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_step_button_clicked()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    Job *jobp = xvc_job_ptr ();
    int pic_no = jobp->pic_no;
    XVC_CapTypeOptions *target = NULL;
    XVC_AppData *app = xvc_appdata_ptr ();

#ifdef USE_FFMPEG
    if (app->current_mode > 0)
        target = &(app->multi_frame);
    else
#endif     // USE_FFMPEG
        target = &(app->single_frame);

    if (app->current_mode == 0) {
        if (!(jobp->state & (VC_PAUSE | VC_REC)))
            return;

        xvc_job_merge_state (VC_STEP);

        xml = glade_get_widget_tree (GTK_WIDGET (xvc_ctrl_main_window));
        g_assert (xml);

        if (pic_no == 0) {
            w = glade_xml_get_widget (xml, "xvc_ctrl_back_button");
            g_assert (w);
            gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
        }

        if (target->frames > 0 && pic_no >= (target->frames - target->step)) {
            if (pic_no > (target->frames - target->step)) {
                w = glade_xml_get_widget (xml, "xvc_ctrl_step_button");
                g_assert (w);
                gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);
            }

            w = glade_xml_get_widget (xml, "xvc_ctrl_forward_button");
            g_assert (w);
            gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);
        }
    }
#undef DEBUGFUNCTION
}

void
on_xvc_ctrl_filename_button_clicked (GtkButton * button, gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_filename_button_clicked()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    Job *jobp = xvc_job_ptr ();
    XVC_CapTypeOptions *target = NULL;
    XVC_AppData *app = xvc_appdata_ptr ();

#ifdef USE_FFMPEG
    if (app->current_mode > 0)
        target = &(app->multi_frame);
    else
#endif     // USE_FFMPEG
        target = &(app->single_frame);

    if (app->current_mode == 0) {
        if (jobp->pic_no != target->start_no
            && ((jobp->state & VC_STOP) > 0 || (jobp->state & VC_PAUSE) > 0)) {
            xml = glade_get_widget_tree (GTK_WIDGET (xvc_ctrl_main_window));
            g_assert (xml);

            w = glade_xml_get_widget (xml, "xvc_ctrl_back_button");
            g_assert (w);
            gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);

            jobp->pic_no = target->start_no;
            GtkChangeLabel (jobp->pic_no);
        }
    } else {
        if (jobp->movie_no != 0 && (jobp->state & VC_STOP) > 0) {
            jobp->movie_no = 0;
            jobp->pic_no = target->start_no;
            GtkChangeLabel (jobp->pic_no);
        }
    }
#undef DEBUGFUNCTION
}

void
on_xvc_ctrl_back_button_clicked (GtkButton * button, gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_back_button_clicked()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    Job *jobp = xvc_job_ptr ();
    XVC_CapTypeOptions *target = NULL;
    XVC_AppData *app = xvc_appdata_ptr ();

#ifdef USE_FFMPEG
    if (app->current_mode > 0)
        target = &(app->multi_frame);
    else
#endif     // USE_FFMPEG
        target = &(app->single_frame);

    if (app->current_mode == 0) {
        if (jobp->pic_no >= target->step) {
            jobp->pic_no -= target->step;
            GtkChangeLabel (jobp->pic_no);
        } else {
            fprintf (stderr,
                     "%s %s: back button active although picture number < step. this should never happen\n",
                     DEBUGFILE, DEBUGFUNCTION);
        }
        if (jobp->pic_no < target->step)
            gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);

        if (target->frames == 0
            || jobp->pic_no == (target->frames - target->step)) {
            xml = glade_get_widget_tree (GTK_WIDGET (xvc_ctrl_main_window));
            g_assert (xml);

            if (jobp->state & VC_PAUSE) {
                w = glade_xml_get_widget (xml, "xvc_ctrl_step_button");
                g_assert (w);
                if (app->current_mode == 0)
                    gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
            }

            w = glade_xml_get_widget (xml, "xvc_ctrl_forward_button");
            g_assert (w);
            gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
        }
    } else {
        if (jobp->movie_no > 0) {
            jobp->movie_no -= 1;
            GtkChangeLabel (jobp->pic_no);
        } else {
            fprintf (stderr,
                     "%s %s: back button active although movie number == 0. this should never happen\n",
                     DEBUGFILE, DEBUGFUNCTION);
        }
        if (jobp->movie_no == 0)
            gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
    }
#undef DEBUGFUNCTION
}

void
on_xvc_ctrl_forward_button_clicked (GtkButton * button, gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_forward_button_clicked()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    Job *jobp = xvc_job_ptr ();
    XVC_CapTypeOptions *target = NULL;
    XVC_AppData *app = xvc_appdata_ptr ();

#ifdef USE_FFMPEG
    if (app->current_mode > 0)
        target = &(app->multi_frame);
    else
#endif     // USE_FFMPEG
        target = &(app->single_frame);

    if (app->current_mode == 0) {
        jobp->pic_no += target->step;
        GtkChangeLabel (jobp->pic_no);

        xml = glade_get_widget_tree (GTK_WIDGET (xvc_ctrl_main_window));
        g_assert (xml);

        if (jobp->pic_no == target->step) {
            w = glade_xml_get_widget (xml, "xvc_ctrl_back_button");
            g_assert (w);

            gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
        }
        if (target->frames > 0
            && jobp->pic_no > (target->frames - target->step)) {
            if (jobp->pic_no >= (target->frames - target->step)) {
                w = glade_xml_get_widget (xml, "xvc_ctrl_step_button");
                g_assert (w);
                gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);
            }

            w = glade_xml_get_widget (xml, "xvc_ctrl_forward_button");
            g_assert (w);
            gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);
        }

    } else {
        jobp->movie_no += 1;
        GtkChangeLabel (jobp->pic_no);
        if (jobp->movie_no == 1) {
            xml = glade_get_widget_tree (GTK_WIDGET (xvc_ctrl_main_window));
            g_assert (xml);

            w = glade_xml_get_widget (xml, "xvc_ctrl_back_button");
            g_assert (w);

            gtk_widget_set_sensitive (GTK_WIDGET (w), TRUE);
        }
    }
#undef DEBUGFUNCTION
}

void
on_xvc_ctrl_lock_toggle_toggled (GtkToggleToolButton *
                                 togglebutton, gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_lock_toggle_toggled()"
    GtkTooltips *tooltips;

    tooltips = gtk_tooltips_new ();

    if (gtk_toggle_tool_button_get_active (togglebutton)) {
        XRectangle *frame_rectangle = NULL;
        GdkRectangle ctrl_rect;
        int x, y, height;
        
        xvc_set_frame_locked (1);      /* button pressed = move frame with
                                        * control */
        gtk_tool_item_set_tooltip (GTK_TOOL_ITEM (togglebutton), tooltips,
                                   _("Detach selection frame"),
                                   _("Detach selection frame"));

/*
        gtk_window_set_gravity (GTK_WINDOW (xvc_ctrl_main_window),
                                GDK_GRAVITY_NORTH_WEST);
        gtk_window_get_position (GTK_WINDOW (xvc_ctrl_main_window), &x, &y);
        gtk_window_get_size (GTK_WINDOW (xvc_ctrl_main_window), &pwidth,
                             &pheight);
        gtk_window_set_gravity (GTK_WINDOW (xvc_ctrl_main_window),
                                GDK_GRAVITY_STATIC);
        gtk_window_get_position (GTK_WINDOW (xvc_ctrl_main_window), &x, &y);
        gtk_window_get_size (GTK_WINDOW (xvc_ctrl_main_window), &pwidth,
                             &pheight);
*/
        xvc_get_ctrl_frame_extents(GDK_WINDOW(xvc_ctrl_main_window->window),
                &ctrl_rect);
        x = ctrl_rect.x;
        y = ctrl_rect.y;
        height = ctrl_rect.height;
//        width = ctrl_rect.width;
        
        if (x < 0)
            x = 0;
        y += height + FRAME_OFFSET + FRAME_WIDTH;
        if (y < 0)
            y = 0;
        frame_rectangle = xvc_get_capture_area ();
        xvc_change_gtk_frame (x, y, frame_rectangle->width,
                              frame_rectangle->height, FALSE, FALSE);
    } else {
        xvc_set_frame_locked (0);
        gtk_tool_item_set_tooltip (GTK_TOOL_ITEM (togglebutton), tooltips,
                                   _("Attach selection frame"),
                                   _("Attach selection frame"));
    }
#undef DEBUGFUNCTION
}

void
on_xvc_ctrl_select_toggle_toggled (GtkToggleToolButton *
                                   togglebutton, gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_select_toggle_toggled()"
    Job *jobp = xvc_job_ptr ();
    XVC_AppData *app = xvc_appdata_ptr ();

    if (gtk_toggle_tool_button_get_active (togglebutton)) {
        Cursor cursor;
        Window target_win = None, temp = None;
        XEvent event;
        int buttons = 0;
        int x_down, y_down, x_up, y_up, x, y;
        int width, height;
        XGCValues gcv;
        GC gc;
        XVC_CapTypeOptions *target = NULL;
        int toggled_frame_on = FALSE;
        GladeXML *menuxml = NULL;
        GtkWidget *show_frame_mitem = NULL;

#ifdef USE_FFMPEG
        if (app->current_mode > 0)
            target = &(app->multi_frame);
        else
#endif     // USE_FFMPEG
            target = &(app->single_frame);

        g_assert (app->dpy);

        if ((app->flags & FLG_NOFRAME)) {
            // if "show_frame" is off we have to switch it
            // on before using the non existing frame
            menuxml = glade_get_widget_tree (xvc_ctrl_m1);
            g_assert (menuxml);

            show_frame_mitem = glade_xml_get_widget (menuxml,
                                                     "xvc_ctrl_m1_show_frame");
            g_assert (show_frame_mitem);
        }

        cursor = XCreateFontCursor (app->dpy, XC_crosshair);

        gcv.background = XBlackPixel (app->dpy, app->default_screen_num);
        gcv.foreground = XWhitePixel (app->dpy, app->default_screen_num);
        gcv.function = GXinvert;
        gcv.plane_mask = gcv.background ^ gcv.foreground;
        gcv.subwindow_mode = IncludeInferiors;
        gc = XCreateGC (app->dpy, app->root_window,
                        GCBackground | GCForeground | GCFunction |
                        GCPlaneMask | GCSubwindowMode, &gcv);

        // grab the mouse
        //
        if (XGrabPointer (app->dpy, app->root_window, False,
                          PointerMotionMask | ButtonPressMask |
                          ButtonReleaseMask, GrabModeSync,
                          GrabModeAsync, app->root_window, cursor, CurrentTime)
            != GrabSuccess) {
            fprintf (stderr, "%s %s: Can't grab mouse!\n", DEBUGFILE,
                     DEBUGFUNCTION);
            return;
        }
        x_down = y_down = x_up = y_up = width = height = 0;

        while (buttons < 2) {
            // allow pointer events
            XAllowEvents (app->dpy, SyncPointer, CurrentTime);
            // search in the queue for button events
            XWindowEvent (app->dpy, app->root_window,
                          PointerMotionMask | ButtonPressMask |
                          ButtonReleaseMask, &event);
            switch (event.type) {
            case ButtonPress:
                x_down = event.xbutton.x;
                y_down = event.xbutton.y;
                target_win = event.xbutton.subwindow;   // window selected
                //
                if (target_win == None) {
                    target_win = app->root_window;
                }
                buttons++;

                // in case the frame is off, set it on and store the fact that
                // we did so
                if ((app->flags & FLG_NOFRAME)) {
                    app->area->x = x_down;
                    app->area->y = y_down;
                    app->area->width = app->area->height = 0;

                    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM
                                                    (show_frame_mitem), TRUE);
                    while (gtk_events_pending ()) {
                        gtk_main_iteration ();
                    }
                    toggled_frame_on = TRUE;
                } else {
                    xvc_destroy_gtk_frame ();
                    xvc_create_gtk_frame (xvc_ctrl_main_window, 0, 0,
                                          x_down, y_down);
                    while (gtk_events_pending ()) {
                        gtk_main_iteration ();
                    }
                }
                break;
            case ButtonRelease:
                x_up = event.xbutton.x;
                y_up = event.xbutton.y;
                buttons++;
                break;
            default:
                // motion notify
                if (buttons == 1) {
                    // button is pressed
                    if (x_down > event.xbutton.x) {
                        width = x_down - event.xbutton.x + 1;
                        x = event.xbutton.x;
                    } else {
                        width = event.xbutton.x - x_down + 1;
                        x = x_down;
                    }
                    if (y_down > event.xbutton.y) {
                        height = y_down - event.xbutton.y + 1;
                        y = event.xbutton.y;
                    } else {
                        height = event.xbutton.y - y_down + 1;
                        y = y_down;
                    }
                    xvc_change_gtk_frame (x, y, width, height, FALSE, TRUE);
                    // the previous call changes the frame edges, which are
                    // gtk windows. we need to call the gtk main loop for
                    // them to be drawn properly within this callback
                    while (gtk_events_pending ())
                        gtk_main_iteration ();
                }
                break;
            }
        }

        XUngrabPointer (app->dpy, CurrentTime); // Done with pointer

        XFreeCursor (app->dpy, cursor);
        XFreeGC (app->dpy, gc);

        if ((x_down != x_up) && (y_down != y_up)) {
            // an individual frame was selected
            if (x_down > x_up) {
                width = x_down - x_up + 1;
                x = x_up;
            } else {
                width = x_up - x_down + 1;
                x = x_down;
            }
            if (y_down > y_up) {
                height = y_down - y_up + 1;
                y = y_up;
            } else {
                height = y_up - y_down + 1;
                y = y_down;
            }
            xvc_appdata_set_window_attributes (target_win);
        } else {
            if (target_win != app->root_window) {
                // get the real window
                target_win = XmuClientWindow (app->dpy, target_win);
            }
            xvc_appdata_set_window_attributes (target_win);
            XTranslateCoordinates (app->dpy, target_win, app->root_window, 0, 0,
                                   &x, &y, &temp);
            width = app->win_attr.width;
            height = app->win_attr.height;
        }

        xvc_change_gtk_frame (x, y, width,
                              height, xvc_is_frame_locked (), FALSE);

        // update colors and colormap
        xvc_job_set_colors ();

        if (app->flags & FLG_RUN_VERBOSE) {
            fprintf (stderr, "%s %s: color_table first entry: 0x%.8X\n",
                     DEBUGFILE, DEBUGFUNCTION,
                     *(u_int32_t *) jobp->color_table);
        }
        // in case the frame is off, set it on and store the fact that
        // we did so
        if (toggled_frame_on) {
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM
                                            (show_frame_mitem), FALSE);
            while (gtk_events_pending ()) {
                gtk_main_iteration ();
            }
        }

        xvc_job_set_save_function (jobp->target);

#ifdef DEBUG
        printf ("%s%s: new visual: %d\n", DEBUGFILE, DEBUGFUNCTION,
                app->win_attr.visual->class);
#endif

    }
    gtk_toggle_tool_button_set_active (togglebutton, FALSE);

#undef DEBUGFUNCTION
}

void
on_xvc_ctrl_m1_mitem_animate_activate (GtkButton * button, gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_m1_mitem_animate_activate()"
    Job *jobp = xvc_job_ptr ();
    XVC_CapTypeOptions *target = NULL;
    XVC_AppData *app = xvc_appdata_ptr ();

#ifdef USE_FFMPEG
    if (app->current_mode > 0)
        target = &(app->multi_frame);
    else
#endif     // USE_FFMPEG
        target = &(app->single_frame);

    if (!app->current_mode) {
        xvc_command_execute (target->play_cmd, 1, 0,
                             jobp->file, target->start_no, jobp->pic_no,
                             app->area->width, app->area->height, target->fps);
    } else {
        xvc_command_execute (target->play_cmd, 2,
                             jobp->movie_no, jobp->file, target->start_no,
                             jobp->pic_no, app->area->width,
                             app->area->height, target->fps);
    }
#undef DEBUGFUNCTION
}

void
on_xvc_ctrl_edit_button_clicked (GtkToolButton * button, gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_ctrl_edit_button_clicked()"
    Job *jobp = xvc_job_ptr ();
    XVC_CapTypeOptions *target = NULL;
    XVC_AppData *app = xvc_appdata_ptr ();

#ifdef USE_FFMPEG
    if (app->current_mode > 0)
        target = &(app->multi_frame);
    else
#endif     // USE_FFMPEG
        target = &(app->single_frame);

    if (!app->current_mode) {
        xvc_command_execute (target->edit_cmd, 2,
                             jobp->pic_no, jobp->file, target->start_no,
                             jobp->pic_no, app->area->width,
                             app->area->height, target->fps);
    } else {
        xvc_command_execute (target->edit_cmd, 2,
                             jobp->movie_no, jobp->file, target->start_no,
                             jobp->pic_no, app->area->width,
                             app->area->height, target->fps);
    }
#undef DEBUGFUNCTION
}

gint
on_xvc_ctrl_filename_button_button_press_event (GtkWidget * widget,
                                                GdkEvent * event)
{
#define DEBUGFUNCTION "on_xvc_ctrl_filename_button_press_event()"
    gboolean is_sensitive = FALSE;
    GdkEventButton *bevent = NULL;

    g_assert (widget);
    g_assert (event);
    bevent = (GdkEventButton *) event;

    // FIXME: changing from menu sensitive or not to button sensitive or
    // not ... make sure that that's what I set elsewhere, too.
    g_object_get ((gpointer) widget, (gchar *) "sensitive", &is_sensitive,
                  NULL);
    if (bevent->button == (guint) 3 && is_sensitive == TRUE) {

        g_assert (xvc_ctrl_m1);

        gtk_menu_popup (GTK_MENU (xvc_ctrl_m1), NULL, NULL,
                        position_popup_menu, widget, bevent->button,
                        bevent->time);
        // Tell calling code that we have handled this event; the buck
        // stops here.
        return TRUE;
    }
    // Tell calling code that we have not handled this event; pass it on.
    return FALSE;
#undef DEBUGFUNCTION
}

// this handles the shortcut keybindings for the menu
gint
on_xvc_ctrl_main_window_key_press_event (GtkWidget * widget, GdkEvent * event)
{
#define DEBUGFUNCTION "on_xvc_ctrl_main_window_key_press_event()"
    gboolean is_sensitive = FALSE;
    GtkWidget *button = NULL, *mitem = NULL;
    GdkEventKey *kevent = NULL;
    GladeXML *xml = NULL;

    g_assert (widget);
    g_assert (event);
    kevent = (GdkEventKey *) event;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    xml = glade_get_widget_tree (xvc_ctrl_main_window);
    g_assert (xml);
    button = glade_xml_get_widget (xml, "xvc_ctrl_filename_button");
    g_assert (button);

    g_object_get ((gpointer) button, (gchar *) "sensitive", &is_sensitive,
                  NULL);

    if (kevent->keyval == 112 && kevent->state == 4) {
        xml = glade_get_widget_tree (xvc_ctrl_m1);
        g_assert (xml);
        mitem = glade_xml_get_widget (xml, "xvc_ctrl_m1_mitem_preferences");
        g_assert (mitem);
        gtk_menu_item_activate (GTK_MENU_ITEM (mitem));
        return TRUE;
    } else if (kevent->keyval == 113 && kevent->state == 4) {
        xml = glade_get_widget_tree (xvc_ctrl_m1);
        g_assert (xml);
        mitem = glade_xml_get_widget (xml, "xvc_ctrl_m1_mitem_quit");
        g_assert (mitem);
        gtk_menu_item_activate (GTK_MENU_ITEM (mitem));
        return TRUE;
    } else if (kevent->keyval == 65470
               || (kevent->state == 4 && kevent->keyval == 104)) {
        xml = glade_get_widget_tree (xvc_ctrl_m1);
        g_assert (xml);
        mitem = glade_xml_get_widget (xml, "xvc_ctrl_m1_mitem_help");
        g_assert (mitem);
        gtk_menu_item_activate (GTK_MENU_ITEM (mitem));
        return TRUE;
    }
#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    // Tell calling code that we have not handled this event; pass it on.
    return FALSE;
#undef DEBUGFUNCTION
}

#if GTK_CHECK_VERSION(2, 6, 0)
void
on_xvc_about_main_window_close (GtkAboutDialog * window, gpointer user_data)
{
    gtk_widget_destroy (GTK_WIDGET (window));
}
#endif     // GTK_CHECK_VERSION

void
on_xvc_ctrl_m1_mitem_about_activate (GtkMenuItem * menuitem, gpointer user_data)
{
    GladeXML *xml = NULL;

    // load the interface
    xml = glade_xml_new (XVC_GLADE_FILE, "xvc_about_main_window", NULL);
    g_assert (xml);
    // connect the signals in the interface
    glade_xml_signal_autoconnect (xml);
}

void
on_xvc_ti_stop_selected (GtkMenuItem * menuitem, gpointer user_data)
{
    GtkWidget *button = NULL;
    GladeXML *xml = NULL;

    xml = glade_get_widget_tree (xvc_ctrl_main_window);
    g_assert (xml);
    button = glade_xml_get_widget (xml, "xvc_ctrl_stop_toggle");
    g_assert (button);

    gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (button), TRUE);
}

void
on_xvc_ti_pause_selected (GtkMenuItem * menuitem, gpointer user_data)
{
    GtkWidget *button = NULL;
    GladeXML *xml = NULL;

    xml = glade_get_widget_tree (xvc_ctrl_main_window);
    g_assert (xml);
    button = glade_xml_get_widget (xml, "xvc_ctrl_pause_toggle");
    g_assert (button);

    gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (button),
                                       gtk_check_menu_item_get_active
                                       (GTK_CHECK_MENU_ITEM (menuitem)));
}

#endif     // DOXYGEN_SHOULD_SKIP_THIS
