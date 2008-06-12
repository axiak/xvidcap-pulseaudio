/**
 * \file gnome_frame.c
 *
 * This file contains functions needed for creation and maintenance of the
 * frame enclosing the area to capture.
 */

/*
 * Copyright (C) 2003-07 Karl H. Beckers, Frankfurt
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
 * This file contains routines for setting up and handling the selection
 * rectangle. Both Xt and GTK2 versions are in here now.
 *
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define DEBUGFILE "gnome_frame.c"
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#include <X11/Intrinsic.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glade/glade.h>
#include <stdlib.h>
#include <sys/time.h>                  // for timeval struct and related
#include <pthread.h>

#include "app_data.h"
#include "job.h"
#include "frame.h"
#include "gnome_frame.h"
#include "gnome_ui.h"

#define XVC_FRAME_DIM_SHOW_TIME 2

/*
 * file globals (static)
 *
 */
/**
 * \brief remember the Display if already retrieved
 *
 * This assumes that the Display to capture from cannot change once xvidcap
 * has been started. We reuse any Display we once retrieved to avoid
 * potential X server roundtrip
 */
static Display *xvc_dpy = NULL;

/** \brief make the frame parts available everywhere in this file */
static GtkWidget *gtk_frame_top,
    *gtk_frame_left, *gtk_frame_right, *gtk_frame_bottom, *gtk_frame_center;
static GtkWidget *xvc_frame_dimensions_window;
static gboolean button_pressed = FALSE;
static int button_press_rel_x = 0;
static int button_press_rel_y = 0;
static gint frame_drag_cursor = GDK_X_CURSOR;
static long frame_dimensions_hide_time = 0;
static GdkDisplay *gdpy;

/**
 * \brief gets the Display to capture from
 *
 * If the frame has been created we retrieve the Display from GDK. Otherwise,
 * (either we're running without GUI or we're doing this before the frame
 * has been created, which is the case if we pass --window) we use
 * XOpenDisplay if xvc_dpy is not already set.
 *
 * @return a pointer to the Display to capture from. This Display can be
 *      expected to be always open and only closed on program exit.
 * @see xvc_frame_drop_capture_display
 */
Display *
xvc_frame_get_capture_display ()
{
#define DEBUGFUNCTION "xvc_frame_get_capure_display()"
    XVC_AppData *app = xvc_appdata_ptr ();

    if (!(app->flags & FLG_NOGUI) && !(app->flags & FLG_NOFRAME)
        && gtk_frame_top) {
        xvc_dpy = GDK_DRAWABLE_XDISPLAY (GTK_WIDGET (gtk_frame_top)->window);
    } else {
        if (!xvc_dpy)
            gdpy = gdk_display_get_default ();
        xvc_dpy = gdk_x11_display_get_xdisplay (gdpy);
    }
    g_assert (xvc_dpy);
    return xvc_dpy;
#undef DEBUGFUNCTION
}

/**
 * \brief cleans up the Display to capture from, i. e. close and set to NULL
 */
void
xvc_frame_drop_capture_display ()
{
#define DEBUGFUNCTION "xvc_frame_drop_capture_display()"
    XVC_AppData *app = xvc_appdata_ptr ();

    if ((app->flags & FLG_NOGUI || app->flags & FLG_NOFRAME) && xvc_dpy) {
        gdk_display_close (gdpy);
        gdpy = NULL;
        xvc_dpy = NULL;
    }
#undef DEBUGFUNCTION
}

/**
 * \brief repositions the main control according to the frame's current
 *      position
 *
 * @param toplevel a pointer to the main control
 */
void
do_reposition_control (GtkWidget * toplevel)
{
#define DEBUGFUNCTION "do_reposition_control()"
    int max_width = 0, max_height = 0;
    int pwidth = 0, pheight = 0;
    XRectangle *x_rect = xvc_get_capture_area ();
    int x = x_rect->x, y = x_rect->y;
    int width = x_rect->width, height = x_rect->height;
    GdkScreen *myscreen;
    GdkRectangle rect;

    myscreen = GTK_WINDOW (toplevel)->screen;
    g_assert (myscreen);

    max_width = gdk_screen_get_width (GDK_SCREEN (myscreen));
    max_height = gdk_screen_get_height (GDK_SCREEN (myscreen));

    xvc_get_ctrl_frame_extents (GDK_WINDOW (toplevel->window), &rect);
//    gdk_window_get_frame_extents (GDK_WINDOW (toplevel->window), &rect);
    pwidth = rect.width + FRAME_OFFSET;
    pheight = rect.height + FRAME_OFFSET;

#ifdef DEBUG
    printf ("%s %s: x %i y %i pwidth %i pheight %i width %i height %i\n",
            DEBUGFILE, DEBUGFUNCTION, x, y, pwidth, pheight, width, height);
#endif     // DEBUG
    if ((y - pheight - FRAME_WIDTH) >= 0) {
        gtk_window_move (GTK_WINDOW (toplevel), x, (y - pheight - FRAME_WIDTH));
    } else {
        GladeXML *xml = NULL;
        GtkWidget *w = NULL;

        xml = glade_get_widget_tree (GTK_WIDGET (toplevel));
        g_assert (xml);

        w = glade_xml_get_widget (xml, "xvc_ctrl_lock_toggle");
        g_assert (w);

        gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (w), FALSE);

        if ((y + pheight + height + FRAME_WIDTH) < max_height) {
            gtk_window_move (GTK_WINDOW (toplevel), x,
                             (y + height + FRAME_OFFSET + FRAME_WIDTH));
        } else {
            if (x > pwidth + FRAME_WIDTH + FRAME_OFFSET) {
                gtk_window_move (GTK_WINDOW (toplevel),
                                 (x - pwidth - FRAME_WIDTH), y);
            } else {
                if ((x + width + pwidth + FRAME_WIDTH) < max_width) {
                    gtk_window_move (GTK_WINDOW (toplevel),
                                     (x + width + FRAME_OFFSET +
                                      FRAME_WIDTH), y);
                }
                // otherwise leave the UI where it is ...
            }
        }
    }
#undef DEBUGFUNCTION
}

/**
 * \brief hides the frame dimensions display if the time scheduled for
 *      hiding it is in the past and the widget is available
 *
 * This is used through g_timeout_add to schedule hiding of the frame
 * dimensions display.
 * @return always zero to remove the timeout function after execution
 */
static int
frame_dimensions_hide ()
{
    struct timeval curr_time;
    long current_time = 0;

    gettimeofday (&curr_time, NULL);
    current_time = curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000;
    if (xvc_frame_dimensions_window &&
        GTK_IS_WIDGET (xvc_frame_dimensions_window) &&
        current_time >= frame_dimensions_hide_time)
        gtk_widget_hide (xvc_frame_dimensions_window);

    return 0;
}

/**
 * \brief changes frame due to user input
 *
 * @param x x-position to change to
 * @param y y-position to change to
 * @param width new frame width
 * @param height new frame height
 * @param reposition_control TRUE for main control should be repositioned
 *      if frame moves, or FALSE if not
 * @param show_dimensions should the frame dimensions display be shown or not?
 */
void
xvc_change_gtk_frame (int x, int y, int width, int height,
                      Boolean reposition_control, Boolean show_dimensions)
{
#define DEBUGFUNCTION "xvc_change_gtk_frame()"
    extern GtkWidget *xvc_ctrl_main_window;
    XRectangle *x_rect = xvc_get_capture_area ();
    XVC_AppData *app = xvc_appdata_ptr ();
    struct timeval curr_time;
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    char buf[64];
    int wwidth, wheight;

#ifdef USE_FFMPEG
    //
    // make sure we have even width and height for ffmpeg
    //
    if (app->current_mode > 0) {
        Boolean changed = FALSE;

        if ((width % 2) > 0) {
            width--;
            changed = TRUE;
        }
        if ((height % 2) > 0) {
            height--;
            changed = TRUE;
        }
        if (width < 10) {
            width = 10;
            changed = TRUE;
        }
        if (height < 10) {
            height = 10;
            changed = TRUE;
        }

        if (changed) {
            if (app->flags & FLG_RUN_VERBOSE) {
                printf
                    ("Modified Selection geometry: %dx%d+%d+%d\n",
                     width, height, x, y);
                if (reposition_control)
                    printf ("Need to reposition the maincontrol\n");
            }
        }
    }
#endif     // USE_FFMPEG

    // we have to adjust it to viewable areas
#ifdef DEBUG
    printf ("%s %s: screen = %dx%d selection=%dx%d\n", DEBUGFILE,
            DEBUGFUNCTION, app->max_width, app->max_height, width, height);
#endif

    if (x < 0)
        x = 0;
    if (width > app->max_width)
        width = app->max_width;
    if (x + width > app->max_width)
        x = app->max_width - width;

    if (y < 0)
        y = 0;
    if (height > app->max_height)
        height = app->max_height;
    if (y + height > app->max_height)
        y = app->max_height - height;

    if ((app->flags & FLG_NOGUI) == 0 && (app->flags & FLG_NOFRAME) == 0) {
        // move the frame if not running without GUI
        if (width != app->area->width) {
            gtk_widget_set_size_request (GTK_WIDGET (gtk_frame_top),
                                         (width + 2 * FRAME_WIDTH),
                                         FRAME_WIDTH);
            gtk_widget_set_size_request (GTK_WIDGET (gtk_frame_bottom),
                                         (width + 2 * FRAME_WIDTH),
                                         FRAME_WIDTH);
        }
        if (height != app->area->height) {
            gtk_widget_set_size_request (GTK_WIDGET (gtk_frame_left),
                                         FRAME_WIDTH, height);
            gtk_widget_set_size_request (GTK_WIDGET (gtk_frame_right),
                                         FRAME_WIDTH, height);
        }
        if (x != app->area->x || y != app->area->y) {
            gtk_window_move (GTK_WINDOW (gtk_frame_top), (x - FRAME_WIDTH),
                             (y - FRAME_WIDTH));
            gtk_window_move (GTK_WINDOW (gtk_frame_left), (x - FRAME_WIDTH), y);
        }
        if (x != app->area->x || y != app->area->y || width != app->area->width
            || height != app->area->height) {
            gtk_window_move (GTK_WINDOW (gtk_frame_bottom), (x - FRAME_WIDTH),
                             (y + height));
            gtk_window_move (GTK_WINDOW (gtk_frame_right), (x + width), y);
        }
#ifdef HasVideo4Linux
        // if we have a v4l blind, move it, too
        if (gtk_frame_center != NULL)
            gtk_window_move (GTK_WINDOW (gtk_frame_center), x, y);
#endif     // HasVideo4Linux
    }
    // store coordinates in rectangle for further reference
    x_rect->x = x;
    x_rect->y = y;
    x_rect->width = width;
    x_rect->height = height;

    // if the frame is locked, we have a GUI, and we also want to
    // reposition the GUI (we don't want to except when repositioning the
    // frame due too the cap_geometry cli parameter because the move of
    // the frame would because triggered through a move of the control ...
    // thus causing an infinite loop), move the control window, too
    if (((app->flags & FLG_NOGUI) == 0) && reposition_control)
        do_reposition_control (xvc_ctrl_main_window);

    if (show_dimensions && ((app->flags & FLG_NOFRAME) == 0)) {
        xml = glade_get_widget_tree (GTK_WIDGET (xvc_frame_dimensions_window));
        g_assert (xml);
        w = glade_xml_get_widget (xml, "xvc_frame_size_label");
        g_assert (w);
        sprintf (buf, "%i x %i", width, height);
        gtk_label_set_text (GTK_LABEL (w), buf);
        w = glade_xml_get_widget (xml, "xvc_frame_position_label");
        g_assert (w);
        sprintf (buf, "%i + %i", x, y);
        gtk_label_set_text (GTK_LABEL (w), buf);

        gtk_widget_show (GTK_WIDGET (xvc_frame_dimensions_window));
        gtk_window_set_gravity (GTK_WINDOW (xvc_frame_dimensions_window),
                                GDK_GRAVITY_CENTER);
        gtk_window_get_size (GTK_WINDOW (xvc_frame_dimensions_window),
                             &wwidth, &wheight);
        gtk_window_move (GTK_WINDOW (xvc_frame_dimensions_window),
                         (x + (width >> 1) - (wwidth >> 1)),
                         (y + (height >> 1) - (wheight >> 1)));
        gettimeofday (&curr_time, NULL);
        frame_dimensions_hide_time =
            (curr_time.tv_sec + XVC_FRAME_DIM_SHOW_TIME) * 1000;
        g_timeout_add ((guint32) (XVC_FRAME_DIM_SHOW_TIME * 1000 + 100),
                       (GtkFunction) frame_dimensions_hide, NULL);
    }
#undef DEBUGFUNCTION
}

/**
 * \brief callback for the configure event on the main control to move
 *      the frame if the main control moves
 */
static gint
on_gtk_frame_configure_event (GtkWidget * w, GdkEventConfigure * e)
{
#define DEBUGFUNCTION "on_gtk_frame_configure_event()"
    gint x, y, pwidth, pheight;
    XVC_AppData *app = xvc_appdata_ptr ();
    Job *job = xvc_job_ptr ();

    if ((app->flags & FLG_LOCK_FOLLOWS_MOUSE) == 0 && xvc_is_frame_locked ()) {
        GdkRectangle rect;

        xvc_get_ctrl_frame_extents (GDK_WINDOW (w->window), &rect);
//        gdk_window_get_frame_extents (GDK_WINDOW (w->window), &rect);

        x = rect.x;
        y = rect.y;
        pwidth = rect.width;
        pheight = rect.height;
        y += pheight + FRAME_OFFSET + FRAME_WIDTH;

        // don't move the frame in the middle of capturing
        // let the capture thread take care of that
        if (!app->recording_thread_running) {
            xvc_change_gtk_frame (x, y, app->area->width, app->area->height,
                                  FALSE, FALSE);
        } else {
            // tell the capture job we moved the frame
            job->frame_moved_x = x - app->area->x;
            job->frame_moved_y = y - app->area->y;
        }
    }
    return FALSE;
#undef DEBUGFUNCTION
}

/**
 * \brief callback to start the procedure for dragging the capture area frame
 *      if we're not capturing and the mouse button is not pressed already
 */
gint
on_gtk_frame_enter_notify_event (GtkWidget * w, GdkEventCrossing * event)
{
    gint x, y;
    GdkModifierType mt;
    GdkCursor *cursor;
    Job *job = xvc_job_ptr ();

    if ((job->state & VC_STOP) == 0)
        return 0;

    if (!button_pressed) {
        gdk_window_get_pointer (w->window, &x, &y, &mt);

        if (w == gtk_frame_right) {
            if (y < XVC_MIN (FRAME_EDGE_SIZE, w->allocation.height / 3)) {
                frame_drag_cursor = GDK_TOP_RIGHT_CORNER;
                cursor = gdk_cursor_new (frame_drag_cursor);
            } else if (y > XVC_MAX (w->allocation.height - FRAME_EDGE_SIZE,
                                    (w->allocation.height << 1) / 3)) {
                frame_drag_cursor = GDK_BOTTOM_RIGHT_CORNER;
                cursor = gdk_cursor_new (frame_drag_cursor);
            } else {
                frame_drag_cursor = GDK_RIGHT_SIDE;
                cursor = gdk_cursor_new (frame_drag_cursor);
            }
            gdk_window_set_cursor (GTK_WIDGET (gtk_frame_right)->window,
                                   cursor);
            gdk_cursor_destroy (cursor);
        } else if (w == gtk_frame_top) {
            if (x < XVC_MIN (FRAME_EDGE_SIZE, w->allocation.width / 3)) {
                frame_drag_cursor = GDK_TOP_LEFT_CORNER;
                cursor = gdk_cursor_new (frame_drag_cursor);
            } else if (x > XVC_MAX (w->allocation.width - FRAME_EDGE_SIZE,
                                    (w->allocation.width << 1) / 3)) {
                frame_drag_cursor = GDK_TOP_RIGHT_CORNER;
                cursor = gdk_cursor_new (frame_drag_cursor);
            } else {
                frame_drag_cursor = GDK_TOP_SIDE;
                cursor = gdk_cursor_new (frame_drag_cursor);
            }
            gdk_window_set_cursor (GTK_WIDGET (gtk_frame_top)->window, cursor);
            gdk_cursor_destroy (cursor);
        } else if (w == gtk_frame_bottom) {
            if (x < XVC_MIN (FRAME_EDGE_SIZE, w->allocation.width / 3)) {
                frame_drag_cursor = GDK_BOTTOM_LEFT_CORNER;
                cursor = gdk_cursor_new (frame_drag_cursor);
            } else if (x > XVC_MAX (w->allocation.width - FRAME_EDGE_SIZE,
                                    (w->allocation.width << 1) / 3)) {
                frame_drag_cursor = GDK_BOTTOM_RIGHT_CORNER;
                cursor = gdk_cursor_new (frame_drag_cursor);
            } else {
                frame_drag_cursor = GDK_BOTTOM_SIDE;
                cursor = gdk_cursor_new (frame_drag_cursor);
            }
            gdk_window_set_cursor (GTK_WIDGET (gtk_frame_bottom)->window,
                                   cursor);
            gdk_cursor_destroy (cursor);
        } else {
            if (y < XVC_MIN (FRAME_EDGE_SIZE, w->allocation.height / 3)) {
                frame_drag_cursor = GDK_TOP_LEFT_CORNER;
                cursor = gdk_cursor_new (frame_drag_cursor);
            } else if (y > XVC_MAX (w->allocation.height - FRAME_EDGE_SIZE,
                                    (w->allocation.height << 1) / 3)) {
                frame_drag_cursor = GDK_BOTTOM_LEFT_CORNER;
                cursor = gdk_cursor_new (frame_drag_cursor);
            } else {
                frame_drag_cursor = GDK_LEFT_SIDE;
                cursor = gdk_cursor_new (frame_drag_cursor);
            }
            gdk_window_set_cursor (GTK_WIDGET (gtk_frame_left)->window, cursor);
            gdk_cursor_destroy (cursor);
        }

    }
    return 0;
}

/**
 * \brief resets the cursor on the capture area frame so that it will never
 *      change as for dragging during capture.
 */
gint
on_gtk_frame_leave_notify_event (GtkWidget * w, GdkEventCrossing * event)
{
    if (!button_pressed) {
        frame_drag_cursor = GDK_X_CURSOR;
        gdk_window_set_cursor (GTK_WIDGET (gtk_frame_top)->window, NULL);
        gdk_window_set_cursor (GTK_WIDGET (gtk_frame_bottom)->window, NULL);
        gdk_window_set_cursor (GTK_WIDGET (gtk_frame_right)->window, NULL);
        gdk_window_set_cursor (GTK_WIDGET (gtk_frame_left)->window, NULL);
    }
    return 0;
}

/**
 * \brief handle dragging or changing mouse pointer
 */
gint
on_gtk_frame_motion_notify_event (GtkWidget * w, GdkEventMotion * event)
{
    gint x, y, x_root, y_root, twidth, theight;
    GdkCursor *cursor;
    XVC_AppData *app = xvc_appdata_ptr ();
    Job *job = xvc_job_ptr ();

    if ((job->state & VC_STOP) == 0)
        return 0;

    x = (int) event->x;
    y = (int) event->y;
    x_root = (int) event->x_root;
    y_root = (int) event->y_root;

//    printf("xr: %i - off: %i --- x: %i -- width: %i\n", x_root, button_press_rel_x, app->area->x, app->area->width);
//    printf("yr: %i - off: %i --- y: %i\n", y_root, button_press_rel_y, app->area->y);

    if (!button_pressed) {
        if (w == gtk_frame_right) {
            if (y < XVC_MIN (FRAME_EDGE_SIZE, w->allocation.height / 3)) {
                frame_drag_cursor = GDK_TOP_RIGHT_CORNER;
                cursor = gdk_cursor_new (frame_drag_cursor);
            } else if (y > XVC_MAX (w->allocation.height - FRAME_EDGE_SIZE,
                                    (w->allocation.height << 1) / 3)) {
                frame_drag_cursor = GDK_BOTTOM_RIGHT_CORNER;
                cursor = gdk_cursor_new (frame_drag_cursor);
            } else {
                frame_drag_cursor = GDK_RIGHT_SIDE;
                cursor = gdk_cursor_new (frame_drag_cursor);
            }
            gdk_window_set_cursor (GTK_WIDGET (gtk_frame_right)->window,
                                   cursor);
            gdk_cursor_destroy (cursor);
        } else if (w == gtk_frame_top) {
            if (x < XVC_MIN (FRAME_EDGE_SIZE, w->allocation.width / 3)) {
                frame_drag_cursor = GDK_TOP_LEFT_CORNER;
                cursor = gdk_cursor_new (frame_drag_cursor);
            } else if (x > XVC_MAX (w->allocation.width - FRAME_EDGE_SIZE,
                                    (w->allocation.width << 1) / 3)) {
                frame_drag_cursor = GDK_TOP_RIGHT_CORNER;
                cursor = gdk_cursor_new (frame_drag_cursor);
            } else {
                frame_drag_cursor = GDK_TOP_SIDE;
                cursor = gdk_cursor_new (frame_drag_cursor);
            }
            gdk_window_set_cursor (GTK_WIDGET (gtk_frame_top)->window, cursor);
            gdk_cursor_destroy (cursor);
        } else if (w == gtk_frame_bottom) {
            if (x < XVC_MIN (FRAME_EDGE_SIZE, w->allocation.width / 3)) {
                frame_drag_cursor = GDK_BOTTOM_LEFT_CORNER;
                cursor = gdk_cursor_new (frame_drag_cursor);
            } else if (x > XVC_MAX (w->allocation.width - FRAME_EDGE_SIZE,
                                    (w->allocation.width << 1) / 3)) {
                frame_drag_cursor = GDK_BOTTOM_RIGHT_CORNER;
                cursor = gdk_cursor_new (frame_drag_cursor);
            } else {
                frame_drag_cursor = GDK_BOTTOM_SIDE;
                cursor = gdk_cursor_new (frame_drag_cursor);
            }
            gdk_window_set_cursor (GTK_WIDGET (gtk_frame_bottom)->window,
                                   cursor);
            gdk_cursor_destroy (cursor);
        } else {
            if (y < XVC_MIN (FRAME_EDGE_SIZE, w->allocation.height / 3)) {
                frame_drag_cursor = GDK_TOP_LEFT_CORNER;
                cursor = gdk_cursor_new (frame_drag_cursor);
            } else if (y > XVC_MAX (w->allocation.height - FRAME_EDGE_SIZE,
                                    (w->allocation.height << 1) / 3)) {
                frame_drag_cursor = GDK_BOTTOM_LEFT_CORNER;
                cursor = gdk_cursor_new (frame_drag_cursor);
            } else {
                frame_drag_cursor = GDK_LEFT_SIDE;
                cursor = gdk_cursor_new (frame_drag_cursor);
            }
            gdk_window_set_cursor (GTK_WIDGET (gtk_frame_left)->window, cursor);
            gdk_cursor_destroy (cursor);
        }
    } else {
        if (frame_drag_cursor == GDK_TOP_RIGHT_CORNER) {
            x_root = x_root - button_press_rel_x;
            y_root = y_root + button_press_rel_y;

            if (x_root < (app->area->x + 20))
                x_root = app->area->x + 20;
            if (y_root > (app->area->y + app->area->height - 20))
                y_root = app->area->y + app->area->height - 20;
            twidth = x_root - app->area->x;
            if ((twidth % 2) > 0) {
                twidth--;
                x_root--;
            }
            theight = app->area->height - y_root + app->area->y;
            if ((theight % 2) > 0) {
                theight--;
                y_root++;
            }

            xvc_change_gtk_frame (app->area->x, y_root,
                                  twidth, theight, FALSE, TRUE);
        } else if (frame_drag_cursor == GDK_BOTTOM_RIGHT_CORNER) {
            x_root = x_root - button_press_rel_x;
            y_root = y_root - button_press_rel_y;

            if (x_root < (app->area->x + 20))
                x_root = app->area->x + 20;
            if (y_root < (app->area->y + 20))
                y_root = app->area->y + 20;
            twidth = x_root - app->area->x;
            if ((twidth % 2) > 0) {
                twidth--;
                x_root--;
            }
            theight = y_root - app->area->y;
            if ((theight % 2) > 0) {
                theight--;
                y_root--;
            }

            xvc_change_gtk_frame (app->area->x, app->area->y,
                                  twidth, theight, FALSE, TRUE);
        } else if (frame_drag_cursor == GDK_RIGHT_SIDE) {
            x_root = x_root - button_press_rel_x;

            if (x_root < (app->area->x + 20))
                x_root = app->area->x + 20;
            twidth = x_root - app->area->x;
            if ((twidth % 2) > 0) {
                twidth--;
                x_root--;
            }

            xvc_change_gtk_frame (app->area->x, app->area->y,
                                  twidth, app->area->height, FALSE, TRUE);
        } else if (frame_drag_cursor == GDK_TOP_SIDE) {
            y_root = y_root + button_press_rel_y;

            if (y_root > (app->area->y + app->area->height - 20))
                y_root = app->area->y + app->area->height - 20;
            theight = app->area->height - y_root + app->area->y;
            if ((theight % 2) > 0) {
                theight--;
                y_root++;
            }

            xvc_change_gtk_frame (app->area->x, y_root,
                                  app->area->width, theight, FALSE, TRUE);
        } else if (frame_drag_cursor == GDK_TOP_LEFT_CORNER) {
            x_root = x_root + button_press_rel_x;
            y_root = y_root + button_press_rel_y;

            if (x_root > (app->area->x + app->area->width - 20))
                x_root = app->area->x + app->area->width - 20;
            if (y_root > (app->area->y + app->area->height - 20))
                y_root = app->area->y + app->area->height - 20;
            twidth = app->area->width - x_root + app->area->x;
            if ((twidth % 2) > 0) {
                twidth--;
                x_root++;
            }
            theight = app->area->height - y_root + app->area->y;
            if ((theight % 2) > 0) {
                theight--;
                y_root++;
            }

            xvc_change_gtk_frame (x_root, y_root, twidth, theight, FALSE, TRUE);
        } else if (frame_drag_cursor == GDK_LEFT_SIDE) {
            x_root = x_root + button_press_rel_x;
            if (x_root > (app->area->x + app->area->width - 20))
                x_root = app->area->x + app->area->width - 20;
            twidth = app->area->width - (x_root - app->area->x);
            if ((twidth % 2) > 0) {
                x_root++;
                twidth--;
            }

            xvc_change_gtk_frame (x_root, app->area->y,
                                  twidth, app->area->height, FALSE, TRUE);
        } else if (frame_drag_cursor == GDK_BOTTOM_LEFT_CORNER) {
            x_root = x_root + button_press_rel_x;
            y_root = y_root - button_press_rel_y;

            if (x_root > (app->area->x + app->area->width - 20))
                x_root = app->area->x + app->area->width - 20;
            if (y_root < (app->area->y + 20))
                y_root = app->area->y + 20;
            twidth = app->area->width - x_root + app->area->x;
            if ((twidth % 2) > 0) {
                twidth--;
                x_root++;
            }
            theight = y_root - app->area->y;
            if ((theight % 2) > 0) {
                theight--;
                y_root--;
            }

            xvc_change_gtk_frame (x_root, app->area->y,
                                  twidth, theight, FALSE, TRUE);
        } else {                       // GDK_BOTTOM_SIDE
            y_root = y_root - button_press_rel_y;

            if (y_root < (app->area->y + 20))
                y_root = app->area->y + 20;
            theight = y_root - app->area->y;
            if ((theight % 2) > 0) {
                theight--;
                y_root--;
            }

            xvc_change_gtk_frame (app->area->x, app->area->y,
                                  app->area->width, theight, FALSE, TRUE);
        }
    }
    return 0;
}

/**
 * \brief track mouse pointer presses on frame
 */
gint
on_gtk_frame_button_press_event (GtkWidget * w, GdkEventButton * event)
{
    XVC_AppData *app = xvc_appdata_ptr ();

    button_pressed = TRUE;
    if (w == gtk_frame_right) {
        button_press_rel_x = event->x + 1;
        button_press_rel_y = 0;
    } else if (w == gtk_frame_top) {
        if (event->x < FRAME_WIDTH) {
            button_press_rel_x = FRAME_WIDTH - event->x - 1;
        } else if (event->x > app->area->width + FRAME_WIDTH) {
            button_press_rel_x = event->x - app->area->width - FRAME_WIDTH;
        } else {
            button_press_rel_x = 0;
        }
        button_press_rel_y = FRAME_WIDTH - event->y - 1;
    } else if (w == gtk_frame_bottom) {
        if (event->x < FRAME_WIDTH) {
            button_press_rel_x = FRAME_WIDTH - event->x - 1;
        } else if (event->x > app->area->width + FRAME_WIDTH) {
            button_press_rel_x = event->x - app->area->width - FRAME_WIDTH;
        } else {
            button_press_rel_x = 0;
        }
        button_press_rel_y = event->y + 1;
    } else {
        button_press_rel_x = FRAME_WIDTH - event->x - 1;
        button_press_rel_y = 0;
    }
//    printf("x %i y %i\n", button_press_rel_x, button_press_rel_y);
    return 0;
}

/**
 * \brief track mouse pointer presses on frame
 */
gint
on_gtk_frame_button_release_event (GtkWidget * w, GdkEventButton * event)
{
    if (xvc_is_frame_locked ()) {
        XVC_AppData *app = xvc_appdata_ptr ();

        xvc_change_gtk_frame (app->area->x, app->area->y,
                              app->area->width, app->area->height, TRUE, FALSE);
    }
    button_pressed = FALSE;
    return 0;
}

/**
 * \brief creates a frame around the area to capture
 *
 * @param toplevel a pointer to the main control
 * @param pwidth width for the frame
 * @param pheight height for the frame
 * @param px x-position for the frame
 * @param py y-position for the frame
 */
void
xvc_create_gtk_frame (GtkWidget * toplevel, int pwidth, int pheight,
                      int px, int py)
{
#define DEBUGFUNCTION "xvc_create_gtk_frame()"
    gint x = 0, y = 0;
    GdkColor g_col;
    GdkColormap *colormap;
    GdkRectangle rect;
    XVC_AppData *app = xvc_appdata_ptr ();
    int flags = app->flags;
    XRectangle *x_rect = xvc_get_capture_area ();
    GladeXML *xml = NULL;

#ifdef DEBUG
    printf ("%s %s: x %d y %d width %d height %d\n", DEBUGFILE,
            DEBUGFUNCTION, px, py, pwidth, pheight);
#endif

#ifdef USE_FFMPEG
    //
    // make sure we have even width and height for ffmpeg
    //
    if (app->current_mode > 0) {
        Boolean changed = FALSE;

        if ((pwidth % 2) > 0) {
            pwidth--;
            changed = TRUE;
        }
        if ((pheight % 2) > 0) {
            pheight--;
            changed = TRUE;
        }
        if (pwidth < 10) {
            pwidth = 10;
            changed = TRUE;
        }
        if (pheight < 10) {
            pheight = 10;
            changed = TRUE;
        }

        if (changed) {
            if (app->flags & FLG_RUN_VERBOSE) {
                printf
                    ("Modified Selection geometry: %dx%d+%d+%d\n",
                     pwidth, pheight, x, y);
            }
        }
    }
#endif     // USE_FFMPEG

    // we have to adjust it to viewable areas
//    max_width = WidthOfScreen (DefaultScreenOfDisplay (app->dpy));
//    max_height = HeightOfScreen (DefaultScreenOfDisplay (app->dpy));

#ifdef DEBUG
    printf ("%s %s: screen = %dx%d selection=%dx%d\n", DEBUGFILE,
            DEBUGFUNCTION, app->max_width, app->max_height, pwidth, pheight);
#endif

    if (x < 0)
        x = 0;
    if (pwidth > app->max_width)
        pwidth = app->max_width;
    if (x + pwidth > app->max_width)
        x = app->max_width - pwidth;

    if (y < 0)
        y = 0;
    if (pheight > app->max_height)
        pheight = app->max_height;
    if (y + pheight > app->max_height)
        y = app->max_height - pheight;

    // now for the real work
    if (!(flags & FLG_NOGUI)) {
        g_assert (toplevel);

        if (px < 0 && py < 0) {
            // compute position for frame
            xvc_get_ctrl_frame_extents (GDK_WINDOW (toplevel->window), &rect);
//            gdk_window_get_frame_extents (GDK_WINDOW (toplevel->window), &rect);

            if (x < 0)
                x = 0;

            y = rect.y;
            y += rect.height + FRAME_OFFSET;
            if (y < 0)
                y = 0;
        } else {
            x = (px < 0 ? 0 : px);
            y = (py < 0 ? 0 : py);
        }
    } else {
        x = (px < 0 ? 0 : px);
        y = (py < 0 ? 0 : py);
    }

    // store rectangle properties for further reference
    g_assert (x_rect);

    x_rect->width = pwidth;
    x_rect->height = pheight;
    x_rect->x = x;
    x_rect->y = y;

    // create frame
    if (!(flags & FLG_NOGUI) && !(flags & FLG_NOFRAME)) {

        // top of frame
        gtk_frame_top = gtk_window_new (GTK_WINDOW_POPUP);
        g_assert (gtk_frame_top);

        gtk_widget_set_size_request (GTK_WIDGET (gtk_frame_top),
                                     (pwidth + 2 * FRAME_WIDTH), FRAME_WIDTH);
        gtk_window_set_title (GTK_WINDOW (gtk_frame_top), "gtk_frame_top");
        gtk_window_set_resizable (GTK_WINDOW (gtk_frame_top), FALSE);
        gtk_widget_add_events (GTK_WIDGET (gtk_frame_top),
                               (GDK_POINTER_MOTION_MASK |
                                GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_PRESS_MASK
                                | GDK_BUTTON_RELEASE_MASK));

        colormap = gtk_widget_get_colormap (GTK_WIDGET (gtk_frame_top));
        g_assert (colormap);

        if (gdk_color_parse ("red", &g_col)) {
            // do the following only if parsing color red succeeded ...
            // if not, we met an error but handle it gracefully by
            // ignoring the frame
            // color
            if (gdk_colormap_alloc_color
                (GDK_COLORMAP (colormap), &g_col, FALSE, TRUE)) {
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_SELECTED, &g_col);

                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_top),
                                      GTK_STATE_SELECTED, &g_col);
            }
        }
        gtk_widget_show (gtk_frame_top);
        gtk_window_move (GTK_WINDOW (gtk_frame_top), (x - FRAME_WIDTH),
                         (y - FRAME_WIDTH));

        // left side of frame
        gtk_frame_left = gtk_window_new (GTK_WINDOW_POPUP);
        g_assert (gtk_frame_left);

        gtk_widget_set_size_request (GTK_WIDGET (gtk_frame_left),
                                     FRAME_WIDTH, pheight);
        gtk_window_set_title (GTK_WINDOW (gtk_frame_left), "gtk_frame_left");
        gtk_window_set_resizable (GTK_WINDOW (gtk_frame_left), FALSE);
        gtk_widget_add_events (GTK_WIDGET (gtk_frame_left),
                               (GDK_POINTER_MOTION_MASK |
                                GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_PRESS_MASK
                                | GDK_BUTTON_RELEASE_MASK));

        colormap = gtk_widget_get_colormap (GTK_WIDGET (gtk_frame_left));
        g_assert (colormap);

        if (gdk_color_parse ("red", &g_col)) {
            // do the following only if parsing color red succeeded ...
            // if not, we met an error but handle it gracefully by
            // ignoring the frame color
            if (gdk_colormap_alloc_color
                (GDK_COLORMAP (colormap), &g_col, FALSE, TRUE)) {
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_SELECTED, &g_col);

                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_left),
                                      GTK_STATE_SELECTED, &g_col);
            }
        }
        gtk_widget_show (GTK_WIDGET (gtk_frame_left));
        gtk_window_move (GTK_WINDOW (gtk_frame_left), (x - FRAME_WIDTH), y);

        // bottom of frame
        gtk_frame_bottom = gtk_window_new (GTK_WINDOW_POPUP);
        g_assert (gtk_frame_bottom);

        gtk_widget_set_size_request (GTK_WIDGET (gtk_frame_bottom),
                                     (pwidth + 2 * FRAME_WIDTH), FRAME_WIDTH);
        gtk_window_set_title (GTK_WINDOW (gtk_frame_bottom),
                              "gtk_frame_bottom");
        gtk_window_set_resizable (GTK_WINDOW (gtk_frame_bottom), FALSE);
        gtk_widget_add_events (GTK_WIDGET (gtk_frame_bottom),
                               (GDK_POINTER_MOTION_MASK |
                                GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_PRESS_MASK
                                | GDK_BUTTON_RELEASE_MASK));

        colormap = gtk_widget_get_colormap (gtk_frame_bottom);
        g_assert (colormap);

        if (gdk_color_parse ("red", &g_col)) {
            // do the following only if parsing color red succeeded ...
            // if not, we met an error but handle it gracefully by
            // ignoring the frame color
            if (gdk_colormap_alloc_color
                (GDK_COLORMAP (colormap), &g_col, FALSE, TRUE)) {
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_SELECTED, &g_col);

                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_bottom),
                                      GTK_STATE_SELECTED, &g_col);
            }
        }
        gtk_widget_show (GTK_WIDGET (gtk_frame_bottom));
        gtk_window_move (GTK_WINDOW (gtk_frame_bottom), (x - FRAME_WIDTH),
                         (y + pheight));

        // right side of frame
        gtk_frame_right = gtk_window_new (GTK_WINDOW_POPUP);
        g_assert (gtk_frame_right);

        gtk_widget_set_size_request (GTK_WIDGET (gtk_frame_right),
                                     FRAME_WIDTH, pheight);
        gtk_window_set_title (GTK_WINDOW (gtk_frame_right), "gtk_frame_right");
        gtk_window_set_resizable (GTK_WINDOW (gtk_frame_right), FALSE);
        gtk_widget_add_events (GTK_WIDGET (gtk_frame_right),
                               (GDK_POINTER_MOTION_MASK |
                                GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_PRESS_MASK
                                | GDK_BUTTON_RELEASE_MASK));

        colormap = gtk_widget_get_colormap (gtk_frame_right);
        g_assert (colormap);

        if (gdk_color_parse ("red", &g_col)) {
            // do the following only if parsing color red succeeded ...
            // if not, we met an error but handle it gracefully by
            // ignoring the frame color
            if (gdk_colormap_alloc_color
                (GDK_COLORMAP (colormap), &g_col, FALSE, TRUE)) {
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_bg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_SELECTED, &g_col);

                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_fg (GTK_WIDGET (gtk_frame_right),
                                      GTK_STATE_SELECTED, &g_col);
            }
        }
        gtk_widget_show (GTK_WIDGET (gtk_frame_right));
        gtk_window_move (GTK_WINDOW (gtk_frame_right), (x + pwidth), y);

        xml =
            glade_xml_new (XVC_GLADE_FILE, "xvc_frame_dimensions_window", NULL);
        g_assert (xml);
        xvc_frame_dimensions_window =
            glade_xml_get_widget (xml, "xvc_frame_dimensions_window");

#ifdef HasVideo4Linux
        if (flags & FLG_USE_V4L) {
            gtk_frame_center = gtk_dialog_new ();

            g_assert (gtk_frame_center);

            gtk_widget_set_size_request (GTK_WIDGET (gtk_frame_center),
                                         pwidth, pheight);
            gtk_widget_set_sensitive (GTK_WIDGET (gtk_frame_center), FALSE);
            gtk_window_set_title (GTK_WINDOW (gtk_frame_right),
                                  "gtk_frame_center");
            GTK_WINDOW (gtk_frame_center)->type = GTK_WINDOW_POPUP;
            gtk_window_set_resizable (GTK_WINDOW (gtk_frame_center), FALSE);
            gtk_dialog_set_has_separator (GTK_DIALOG (gtk_frame_center), FALSE);
            gtk_widget_show (GTK_WIDGET (gtk_frame_center));
            gtk_window_move (GTK_WINDOW (gtk_frame_center), x, y);

            // FIXME: right now the label for the Video Source is missing
            // gtk_frame_center = XtVaCreatePopupShell ("blind",
            // overrideShellWidgetClass, parent,
            // XtNx, x+FRAME_WIDTH,
            // XtNy, y+FRAME_WIDTH,
            // XtNwidth, width,
            // XtNheight, height,
            // NULL);
            // XtVaCreateManagedWidget ("text", xwLabelWidgetClass, blind,
            // XtNlabel, "Source: Video4Linux", NULL);
            // XtPopup(blind, XtGrabNone);
        }
#endif     // HasVideo4Linux
        g_signal_connect (G_OBJECT (gtk_frame_top), "enter_notify_event",
                          G_CALLBACK (on_gtk_frame_enter_notify_event), NULL);
        g_signal_connect (G_OBJECT (gtk_frame_left), "enter_notify_event",
                          G_CALLBACK (on_gtk_frame_enter_notify_event), NULL);
        g_signal_connect (G_OBJECT (gtk_frame_bottom), "enter_notify_event",
                          G_CALLBACK (on_gtk_frame_enter_notify_event), NULL);
        g_signal_connect (G_OBJECT (gtk_frame_right), "enter_notify_event",
                          G_CALLBACK (on_gtk_frame_enter_notify_event), NULL);

        g_signal_connect (G_OBJECT (gtk_frame_top), "leave_notify_event",
                          G_CALLBACK (on_gtk_frame_leave_notify_event), NULL);
        g_signal_connect (G_OBJECT (gtk_frame_left), "leave_notify_event",
                          G_CALLBACK (on_gtk_frame_leave_notify_event), NULL);
        g_signal_connect (G_OBJECT (gtk_frame_bottom), "leave_notify_event",
                          G_CALLBACK (on_gtk_frame_leave_notify_event), NULL);
        g_signal_connect (G_OBJECT (gtk_frame_right), "leave_notify_event",
                          G_CALLBACK (on_gtk_frame_leave_notify_event), NULL);

        g_signal_connect (G_OBJECT (gtk_frame_top), "motion_notify_event",
                          G_CALLBACK (on_gtk_frame_motion_notify_event), NULL);
        g_signal_connect (G_OBJECT (gtk_frame_left), "motion_notify_event",
                          G_CALLBACK (on_gtk_frame_motion_notify_event), NULL);
        g_signal_connect (G_OBJECT (gtk_frame_bottom), "motion_notify_event",
                          G_CALLBACK (on_gtk_frame_motion_notify_event), NULL);
        g_signal_connect (G_OBJECT (gtk_frame_right), "motion_notify_event",
                          G_CALLBACK (on_gtk_frame_motion_notify_event), NULL);

        g_signal_connect (G_OBJECT (gtk_frame_top), "button_press_event",
                          G_CALLBACK (on_gtk_frame_button_press_event), NULL);
        g_signal_connect (G_OBJECT (gtk_frame_left), "button_press_event",
                          G_CALLBACK (on_gtk_frame_button_press_event), NULL);
        g_signal_connect (G_OBJECT (gtk_frame_bottom), "button_press_event",
                          G_CALLBACK (on_gtk_frame_button_press_event), NULL);
        g_signal_connect (G_OBJECT (gtk_frame_right), "button_press_event",
                          G_CALLBACK (on_gtk_frame_button_press_event), NULL);

        g_signal_connect (G_OBJECT (gtk_frame_top), "button_release_event",
                          G_CALLBACK (on_gtk_frame_button_release_event), NULL);
        g_signal_connect (G_OBJECT (gtk_frame_left), "button_release_event",
                          G_CALLBACK (on_gtk_frame_button_release_event), NULL);
        g_signal_connect (G_OBJECT (gtk_frame_bottom), "button_release_event",
                          G_CALLBACK (on_gtk_frame_button_release_event), NULL);
        g_signal_connect (G_OBJECT (gtk_frame_right), "button_release_event",
                          G_CALLBACK (on_gtk_frame_button_release_event), NULL);

    }
    // connect event-handler to configure event of gtk control window
    // to redraw the selection frame if the control is moved and the
    // frame is locked
    // this is also required with FLG_NOFRAME
    if (!(flags & FLG_NOGUI)) {
        g_signal_connect (G_OBJECT (toplevel), "configure-event",
                          G_CALLBACK (on_gtk_frame_configure_event), NULL);
    }

    xvc_set_frame_locked (1);

    if (!(flags & FLG_NOGUI) && (px >= 0 || py >= 0))
        do_reposition_control (toplevel);

#undef DEBUGFUNCTION
}

/**
 * \brief destroys the frame around the capture area
 */
void
xvc_destroy_gtk_frame ()
{
#define DEBUGFUNCTION "xvc_destroy_gtk_frame()"

    if (gtk_frame_bottom) {
        gtk_widget_destroy (gtk_frame_bottom);
        gtk_frame_bottom = NULL;
    }
    if (gtk_frame_right) {
        gtk_widget_destroy (gtk_frame_right);
        gtk_frame_right = NULL;
    }
    if (gtk_frame_left) {
        gtk_widget_destroy (gtk_frame_left);
        gtk_frame_left = NULL;
    }
    if (gtk_frame_top) {
        gtk_widget_destroy (gtk_frame_top);
        gtk_frame_top = NULL;
    }
    if (xvc_frame_dimensions_window) {
        gtk_widget_destroy (xvc_frame_dimensions_window);
        xvc_frame_dimensions_window = NULL;
    }
    if (gtk_frame_center) {
        gtk_widget_destroy (gtk_frame_center);
        gtk_frame_center = NULL;
    }
#undef DEBUGFUNCTION
}
