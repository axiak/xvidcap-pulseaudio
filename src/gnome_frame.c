/* 
 * gnome_frame.c,
 *
 * Copyright (C) 2003,04,05,06 Karl H. Beckers, Frankfurt
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
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <X11/Intrinsic.h>
#include <gtk/gtk.h>
#include <bonobo.h>
#include <gnome.h>
#include <glade/glade.h>

#include "app_data.h"
#include "job.h"
#include "frame.h"
#include "gnome_frame.h"

#define DEBUGFILE "gnome_frame.c"


/* 
 * some globals
 * these are from frames.c
 */
extern AppData *app;

/* 
 * file globals (static)
 *
 */
static GtkWidget *gtk_frame_top,
    *gtk_frame_left, *gtk_frame_right, *gtk_frame_bottom,
    *gtk_frame_center;


void do_reposition_control(GtkWidget *toplevel) {
    #define DEBUGFUNCTION "do_reposition_control()"
    int max_width = 0, max_height = 0;
    int pwidth = 0, pheight = 0, x = 0, y = 0, topHeight = 0, width = 0, leftWidth = 0, height = 0;
    GdkScreen *myscreen;
    Display *dpy;

        myscreen = GTK_WINDOW(toplevel)->screen;
        g_assert(myscreen);

        max_width = gdk_screen_get_width(GDK_SCREEN(myscreen));
        max_height = gdk_screen_get_height(GDK_SCREEN(myscreen));

        gdk_window_get_size(GDK_WINDOW(toplevel->window),
                            &pwidth, &pheight);
        pwidth += FRAME_OFFSET;
        pheight += FRAME_OFFSET;

        gdk_window_get_size(GDK_WINDOW(gtk_frame_left->window), &leftWidth, &height);
        gdk_window_get_size(GDK_WINDOW(gtk_frame_top->window), &width, &topHeight);
        gtk_window_get_position(GTK_WINDOW(gtk_frame_top),(guint*) &x,(guint*) &y);

#ifdef DEBUG
    printf("%s %s: x %i y%i pheight %i pwidht %i height %i width%i\n", DEBUGFILE, DEBUGFUNCTION,
            x, y, pheight, pwidth, height, width );
#endif // DEBUG

        if ((y - pheight) >= 0) {
            gtk_window_move(GTK_WINDOW(toplevel), x,
                            (y - pheight));
        } else {
            GladeXML *xml = NULL;
            GtkWidget *w = NULL;
            gboolean ignore = TRUE;

            xml = glade_get_widget_tree(GTK_WIDGET(toplevel));
            g_assert(xml);

            w = glade_xml_get_widget(xml, "xvc_ctrl_lock_toggle");
            g_assert(w);
    
            gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(w), FALSE);
        
            if ((y + pheight + height) < max_height) {
                gtk_window_move(GTK_WINDOW(toplevel), x,
                                (y + height + FRAME_OFFSET));
            } else {
                if (x > pwidth) {
                    gtk_window_move(GTK_WINDOW(toplevel),
                                    (x - pwidth - FRAME_OFFSET), y);
                } else {
                    if ((x + width + pwidth) < max_width) {
                        gtk_window_move(GTK_WINDOW(toplevel),
                                        (x + width + FRAME_OFFSET), y);
                    }
                    // otherwise leave the UI where it is ...
                }
            }
        }
    #undef DEBUGFUNCTION
}


/* 
 * Change Frame due to user input
 *
 */
void xvc_change_gtk_frame(int x, int y, int width, int height, 
                     Boolean reposition_control)
{
    #define DEBUGFUNCTION "xvc_change_gtk_frame()"
    int max_width, max_height;
    int pwidth, pheight, px1, py1, px2, py2;
    extern GtkWidget *xvc_ctrl_main_window;
    GdkScreen *myscreen;
    Display *dpy;

    // we have to adjust it to viewable areas
    if ((app->flags & FLG_NOGUI) == 0) {
        myscreen = GTK_WINDOW(gtk_frame_top)->screen;
        g_assert(myscreen);

        max_width = gdk_screen_get_width(GDK_SCREEN(myscreen));
        max_height = gdk_screen_get_height(GDK_SCREEN(myscreen));
    } else {
        dpy = XOpenDisplay(NULL);
        g_assert(dpy);

        max_width = WidthOfScreen(DefaultScreenOfDisplay(dpy));
        max_height = HeightOfScreen(DefaultScreenOfDisplay(dpy));
    }

#ifdef DEBUG
    printf("%s %s: screen = %dx%d selection=%dx%d\n", DEBUGFILE, DEBUGFUNCTION,
           max_width, max_height, width, height);
#endif

    if (x < 0)
        x = 0;
    if (width > max_width)
        width = max_width;
    if (x + width > max_width)
        x = max_width - width;

    if (height > max_height)
        height = max_height;
    if (y + height > max_height)
        y = max_height - height;
    if (y < 0)
        y = 0;

    if ((app->flags & FLG_NOGUI) == 0) {
        // move the frame if not running without GUI
        gtk_widget_set_size_request(GTK_WIDGET(gtk_frame_top),
                                    (width + 2 * FRAME_WIDTH),
                                    FRAME_WIDTH);
        gtk_window_move(GTK_WINDOW(gtk_frame_top), (x - FRAME_WIDTH),
                        (y - FRAME_WIDTH));
        gtk_widget_set_size_request(GTK_WIDGET(gtk_frame_left),
                                    FRAME_WIDTH, height);
        gtk_window_move(GTK_WINDOW(gtk_frame_left), (x - FRAME_WIDTH), y);
        gtk_widget_set_size_request(GTK_WIDGET(gtk_frame_bottom),
                                    (width + 2 * FRAME_WIDTH),
                                    FRAME_WIDTH);
        gtk_window_move(GTK_WINDOW(gtk_frame_bottom), (x - FRAME_WIDTH),
                        (y + height));
        gtk_widget_set_size_request(GTK_WIDGET(gtk_frame_right),
                                    FRAME_WIDTH, height);
        gtk_window_move(GTK_WINDOW(gtk_frame_right), (x + width), y);

        // if we have a v4l blind, move it, too
        if (gtk_frame_center != NULL)
            gtk_window_move(GTK_WINDOW(gtk_frame_center), x, y);
    }
    // store coordinates in rectangle for further reference
    xvc_frame_rectangle.x = x;
    xvc_frame_rectangle.y = y;
    xvc_frame_rectangle.width = width;
    xvc_frame_rectangle.height = height;

    // if the frame is locked, we have a GUI, and we also want to
    // reposition
    // the GUI (we don't want to except when repositioning the frame due
    // too
    // the cap_geometry cli parameter because the move of the frame would
    // because
    // triggered through a move of the control ... thus causing an
    // infinite loop), 
    // move the control window, too
    if (((app->flags & FLG_NOGUI) == 0) && reposition_control) 
        do_reposition_control(xvc_ctrl_main_window);

    #undef DEBUGFUNCTION
}


// 
// on move of window move frame too if locked
static gint
on_gtk_frame_configure_event(GtkWidget * w, GdkEventConfigure * e)
{
    #define DEBUGFUNCTION "on_gtk_frame_configure_event()"
    gint x, y, pwidth, pheight;

    if (xvc_is_frame_locked()) {
        x = ((GdkEventConfigure *) e)->x;
        y = ((GdkEventConfigure *) e)->y;
        pwidth = ((GdkEventConfigure *) e)->width;
        pheight = ((GdkEventConfigure *) e)->height;
        y += pheight + FRAME_OFFSET;
        xvc_change_gtk_frame(x, y, xvc_frame_rectangle.width,
                             xvc_frame_rectangle.height, FALSE);
    }

    return FALSE;
    #undef DEBUGFUNCTION
}


void
xvc_create_gtk_frame(GtkWidget * toplevel, int pwidth, int pheight,
                     int px, int py)
{
    #define DEBUGFUNCTION "xvc_create_gtk_frame()"
    gint x = 0, y = 0, width = 0, height = 0;
    GdkColor g_col;
    GdkColormap *colormap;
    int flags = app->flags;

#ifdef DEBUG
    printf("%s %s: x %d y %d width %d height %d\n", DEBUGFILE, DEBUGFUNCTION, px, py, pwidth, pheight);
#endif


    if (!(flags & FLG_NOGUI)) {
        g_assert(toplevel);

        if (px < 0 && py < 0) {
            // compute position for frame
            gdk_window_get_origin(GDK_WINDOW(toplevel->window), &x, &y);
            gtk_window_get_size(GTK_WINDOW(toplevel), &width, &height);

            if (x < 0)
                x = 0;
            y += height + FRAME_OFFSET;
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

    // FIXME: make sure width and height are on screen

    // store rectangle properties for further reference
    g_assert(&xvc_frame_rectangle);

    xvc_frame_rectangle.width = pwidth;
    xvc_frame_rectangle.height = pheight;
    xvc_frame_rectangle.x = x;
    xvc_frame_rectangle.y = y;

    printf("gnome_frame.c ... rectangle width %i\n",
           xvc_frame_rectangle.width);
    printf("gnome_frame.c ... rectangle height %i\n",
           xvc_frame_rectangle.height);
    printf("gnome_frame.c ... rectangle x %i\n", xvc_frame_rectangle.x);
    printf("gnome_frame.c ... rectangle y %i\n", xvc_frame_rectangle.y);

    // create frame
    if (!(flags & FLG_NOGUI)) {

        // top of frame
        gtk_frame_top = gtk_dialog_new();

        g_assert(gtk_frame_top);

        gtk_widget_set_size_request(GTK_WIDGET(gtk_frame_top),
                                    (pwidth + 2 * FRAME_WIDTH),
                                    FRAME_WIDTH);
        gtk_widget_set_sensitive(GTK_WIDGET(gtk_frame_top), FALSE);
        gtk_window_set_title(GTK_WINDOW(gtk_frame_top), "gtk_frame_top");
        GTK_WINDOW(gtk_frame_top)->type = GTK_WINDOW_POPUP;
        gtk_window_set_resizable(GTK_WINDOW(gtk_frame_top), FALSE);
        gtk_dialog_set_has_separator(GTK_DIALOG(gtk_frame_top), FALSE);
        colormap = gtk_widget_get_colormap(GTK_WIDGET(gtk_frame_top));

        g_assert(colormap);

        if (gdk_color_parse("red", &g_col)) {
            // do the following only if parsing color red succeeded ...
            // if not, we met an error but handle it gracefully by
            // ignoring the frame
            // color
            if (gdk_colormap_alloc_color
                (GDK_COLORMAP(colormap), &g_col, FALSE, TRUE)) {
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_top),
                                     GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_top),
                                     GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_top),
                                     GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_top),
                                     GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_top),
                                     GTK_STATE_SELECTED, &g_col);

                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_top),
                                     GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_top),
                                     GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_top),
                                     GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_top),
                                     GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_top),
                                     GTK_STATE_SELECTED, &g_col);
            }
        }
        gtk_widget_show(gtk_frame_top);
        gtk_window_move(GTK_WINDOW(gtk_frame_top), (x - FRAME_WIDTH),
                        (y - FRAME_WIDTH));

        // left side of frame
        gtk_frame_left = gtk_dialog_new();

        g_assert(gtk_frame_left);

        gtk_widget_set_size_request(GTK_WIDGET(gtk_frame_left),
                                    FRAME_WIDTH, pheight);
        gtk_widget_set_sensitive(GTK_WIDGET(gtk_frame_left), FALSE);
        gtk_window_set_title(GTK_WINDOW(gtk_frame_left), "gtk_frame_left");
        GTK_WINDOW(gtk_frame_left)->type = GTK_WINDOW_POPUP;
        gtk_window_set_resizable(GTK_WINDOW(gtk_frame_left), FALSE);
        gtk_dialog_set_has_separator(GTK_DIALOG(gtk_frame_left), FALSE);
        colormap = gtk_widget_get_colormap(GTK_WIDGET(gtk_frame_left));

        g_assert(colormap);

        if (gdk_color_parse("red", &g_col)) {
            // do the following only if parsing color red succeeded ...
            // if not, we met an error but handle it gracefully by
            // ignoring the frame
            // color
            if (gdk_colormap_alloc_color
                (GDK_COLORMAP(colormap), &g_col, FALSE, TRUE)) {
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_left),
                                     GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_left),
                                     GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_left),
                                     GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_left),
                                     GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_left),
                                     GTK_STATE_SELECTED, &g_col);

                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_left),
                                     GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_left),
                                     GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_left),
                                     GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_left),
                                     GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_left),
                                     GTK_STATE_SELECTED, &g_col);
            }
        }
        gtk_widget_show(GTK_WIDGET(gtk_frame_left));
        gtk_window_move(GTK_WINDOW(gtk_frame_left), (x - FRAME_WIDTH), y);

        // bottom of frame
        gtk_frame_bottom = gtk_dialog_new();

        g_assert(gtk_frame_bottom);

        gtk_widget_set_size_request(GTK_WIDGET(gtk_frame_bottom),
                                    (pwidth + 2 * FRAME_WIDTH),
                                    FRAME_WIDTH);
        gtk_widget_set_sensitive(GTK_WIDGET(gtk_frame_bottom), FALSE);
        gtk_window_set_title(GTK_WINDOW(gtk_frame_bottom),
                             "gtk_frame_bottom");
        GTK_WINDOW(gtk_frame_bottom)->type = GTK_WINDOW_POPUP;
        gtk_window_set_resizable(GTK_WINDOW(gtk_frame_bottom), FALSE);
        gtk_dialog_set_has_separator(GTK_DIALOG(gtk_frame_bottom), FALSE);
        colormap = gtk_widget_get_colormap(gtk_frame_bottom);

        g_assert(colormap);

        if (gdk_color_parse("red", &g_col)) {
            // do the following only if parsing color red succeeded ...
            // if not, we met an error but handle it gracefully by
            // ignoring the frame
            // color
            if (gdk_colormap_alloc_color
                (GDK_COLORMAP(colormap), &g_col, FALSE, TRUE)) {
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_bottom),
                                     GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_bottom),
                                     GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_bottom),
                                     GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_bottom),
                                     GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_bottom),
                                     GTK_STATE_SELECTED, &g_col);

                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_bottom),
                                     GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_bottom),
                                     GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_bottom),
                                     GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_bottom),
                                     GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_bottom),
                                     GTK_STATE_SELECTED, &g_col);
            }
        }
        gtk_widget_show(GTK_WIDGET(gtk_frame_bottom));
        gtk_window_move(GTK_WINDOW(gtk_frame_bottom), (x - FRAME_WIDTH),
                        (y + pheight));

        // right side of frame
        gtk_frame_right = gtk_dialog_new();

        g_assert(gtk_frame_right);

        gtk_widget_set_size_request(GTK_WIDGET(gtk_frame_right),
                                    FRAME_WIDTH, pheight);
        gtk_widget_set_sensitive(GTK_WIDGET(gtk_frame_right), FALSE);
        gtk_window_set_title(GTK_WINDOW(gtk_frame_right),
                             "gtk_frame_right");
        GTK_WINDOW(gtk_frame_right)->type = GTK_WINDOW_POPUP;
        gtk_window_set_resizable(GTK_WINDOW(gtk_frame_right), FALSE);
        gtk_dialog_set_has_separator(GTK_DIALOG(gtk_frame_right), FALSE);
        colormap = gtk_widget_get_colormap(gtk_frame_right);

        g_assert(colormap);

        if (gdk_color_parse("red", &g_col)) {
            // do the following only if parsing color red succeeded ...
            // if not, we met an error but handle it gracefully by
            // ignoring the frame
            // color
            if (gdk_colormap_alloc_color
                (GDK_COLORMAP(colormap), &g_col, FALSE, TRUE)) {
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_right),
                                     GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_right),
                                     GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_right),
                                     GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_right),
                                     GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_bg(GTK_WIDGET(gtk_frame_right),
                                     GTK_STATE_SELECTED, &g_col);

                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_right),
                                     GTK_STATE_NORMAL, &g_col);
                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_right),
                                     GTK_STATE_ACTIVE, &g_col);
                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_right),
                                     GTK_STATE_PRELIGHT, &g_col);
                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_right),
                                     GTK_STATE_INSENSITIVE, &g_col);
                gtk_widget_modify_fg(GTK_WIDGET(gtk_frame_right),
                                     GTK_STATE_SELECTED, &g_col);
            }
        }
        gtk_widget_show(GTK_WIDGET(gtk_frame_right));
        gtk_window_move(GTK_WINDOW(gtk_frame_right), (x + pwidth), y);

        if (flags & FLG_USE_V4L) {
            gtk_frame_center = gtk_dialog_new();

            g_assert(gtk_frame_center);

            gtk_widget_set_size_request(GTK_WIDGET(gtk_frame_center),
                                        pwidth, pheight);
            gtk_widget_set_sensitive(GTK_WIDGET(gtk_frame_center), FALSE);
            gtk_window_set_title(GTK_WINDOW(gtk_frame_right),
                                 "gtk_frame_center");
            GTK_WINDOW(gtk_frame_center)->type = GTK_WINDOW_POPUP;
            gtk_window_set_resizable(GTK_WINDOW(gtk_frame_center), FALSE);
            gtk_dialog_set_has_separator(GTK_DIALOG(gtk_frame_center),
                                         FALSE);
            gtk_widget_show(GTK_WIDGET(gtk_frame_center));
            gtk_window_move(GTK_WINDOW(gtk_frame_center), x, y);

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
        // connect event-handler to configure event of gtk control window
        // to redraw the
        // selection frame if the control is moved and the frame is locked
        g_signal_connect((gpointer) toplevel, "configure-event",
                         G_CALLBACK(on_gtk_frame_configure_event), NULL);

    }
    xvc_frame_lock = 1;

    if (!(flags & FLG_NOGUI) && ( px >= 0 || py >= 0 ) )
        do_reposition_control(toplevel);

    #undef DEBUGFUNCTION
}


void xvc_destroy_gtk_frame()
{
    #define DEBUGFUNCTION "xvc_destroy_gtk_frame()"
    
    gtk_widget_destroy(gtk_frame_bottom);
    gtk_widget_destroy(gtk_frame_right);
    gtk_widget_destroy(gtk_frame_left);
    gtk_widget_destroy(gtk_frame_top);
    if (gtk_frame_center) {
        gtk_widget_destroy(gtk_frame_center);
    }

    #undef DEBUGFUNCTION
}
