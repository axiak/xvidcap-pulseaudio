/*
 * gnome_warning.c
 *
 * this file contains the warnings dialog for the options dialog of
 * the GTK2 control
 *
 * Copyright (C) 2003-06 Karl H. Beckers, Frankfurt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include "control.h"
#include "gnome_options.h"
#include "gnome_ui.h"
#include "xv_error_item.h"

#define GLADE_FILE PACKAGE_DATA_DIR"/xvidcap/glade/gnome-xvidcap.glade"
#define DEBUGFILE "gnome_warning.c"




/* 
 * global variables
 */
// static gint xvc_warn_label_width = 0;
static int called_from_where = 0;   // tells from where the warning
// originates
// 0 = preferences dialog
// 1 = capture type toggle
// 2 = initial validation in main.c
static guint scheduled_warning_resize_id = 0;
static xvErrorListItem *warning_elist = NULL;


/*const char *XVC_WARN_LABEL_TEXT =
    "Your input bears a number of inconsistencies! \nPlease review the list below and click \"OK\" to accept the suggested actions or \"Cancel\" to return to the Preferences dialog.";
*/

GtkWidget *xvc_warn_main_window;
extern AppData *app;
extern GtkWidget *xvc_ctrl_m1;
extern GtkWidget *xvc_pref_main_window;


/* 
 * callbacks
 */

/* 
 * // // recalculate line breaks when size changes typedef struct
 * _LabelWrapWidth LabelWrapWidth; struct _LabelWrapWidth { gint width;
 * PangoFontDescription *font_desc; };
 * 
 * static void set_label_wrap_width (GtkLabel *label, gint set_width) { // 
 * PangoLayout *layout; GtkStyle *style = GTK_WIDGET (label)->style;
 * g_assert(style);
 * 
 * LabelWrapWidth *wrap_width = g_object_get_data (G_OBJECT (style),
 * "gtk-label-wrap-width"); if (!wrap_width) { wrap_width = g_new0
 * (LabelWrapWidth, 1); g_object_set_data_full (G_OBJECT (style),
 * "gtk-label-wrap-width", wrap_width, label_wrap_width_free ); }
 * 
 * // printf("gtk2_warning: setting wrap_width to %i\n", ( set_width *
 * 1000 ) ); wrap_width->width = ( set_width * 1000 ); }
 * 
 * 
 * void on_xvc_warn_main_window_size_allocate(GtkWidget *w, gpointer
 * user_data) { gint new_width = w->allocation.width; GladeXML *xml =
 * NULL; GtkWidget *label = NULL;
 * 
 * xml = glade_get_widget_tree(xvc_warn_main_window); g_assert(xml); label 
 * = glade_xml_get_widget(xml, "xvc_warn_label"); g_assert(label);
 * 
 * // printf("gtk2_warning: got wrap_width: %i\n", wrap_width); //
 * GtkStyle *style = GTK_WIDGET (label)->style; // g_assert(style);
 * 
 * // LabelWrapWidth *wrap = g_object_get_data (G_OBJECT (style),
 * "gtk-label-wrap-width"); // printf("gtk2_warning: old_width %i -
 * new_width %i\n", (int) label2_width, (int) new_width);
 * 
 * if ( xvc_warn_label_width != new_width ) { xvc_warn_label_width =
 * new_width;
 * 
 * set_label_wrap_width( GTK_LABEL(label), new_width );
 * gtk_label_set_text(GTK_LABEL(label), XVC_WARN_LABEL_TEXT); } } 
 */

gboolean
on_xvc_warn_main_window_delete_event(GtkWidget * widget, GdkEvent * event,
                                     gpointer user_data)
{
    if (called_from_where == 2) return TRUE;
    else return FALSE;
}


void on_xvc_warn_main_window_close(GtkDialog * dialog, gpointer user_data)
{
    // empty as yet ...
}


void
on_xvc_warn_main_window_destroy(GtkButton * button, gpointer user_data)
{
    // empty as yet ...
}


static void doHelp()
{
    if (app->help_cmd != NULL)
        system((char *) app->help_cmd);
}


void
on_xvc_warn_main_window_response(GtkDialog * dialog, gint response_id,
                                 gpointer user_data)
{
    #undef DEBUGFUNCTION
    #define DEBUGFUNCTION "on_xvc_warn_main_window_response()"
    GladeXML *xml = NULL;
    GtkWidget *mitem = NULL;

#ifdef DEBUG
    printf("%s %s: Entering with response_id %i, called from %i\n", DEBUGFILE, DEBUGFUNCTION, 
            response_id, called_from_where);
#endif // DEBUG

    switch (response_id) {
    case GTK_RESPONSE_OK:

        if (warning_elist != NULL) {
            xvErrorListItem *err;
            err = warning_elist;
            for (; err != NULL; err = err->next) {
                (*err->err->action) (err);
            }
        }

        warning_elist = xvc_errors_delete_list(warning_elist);

        switch (called_from_where) {
        case 0:
            gtk_widget_destroy(xvc_warn_main_window);
            xvc_pref_do_OK();
            break;
#ifdef USE_FFMPEG
        case 1:
            gtk_widget_destroy(xvc_warn_main_window);
            xvc_toggle_cap_type();
            break;
#endif                          // USE_FFMPEG
        case 2:
            gtk_widget_destroy(xvc_warn_main_window);
            xvc_check_start_options();
        }
        break;
    case GTK_RESPONSE_CANCEL:
        switch (called_from_where) {
        case 0:
            xvc_pref_reset_OK_attempts();
            gtk_widget_destroy(xvc_warn_main_window);
            break;
#ifdef USE_FFMPEG
        case 1:
            xvc_undo_toggle_cap_type();
            gtk_widget_destroy(xvc_warn_main_window);
            break;
#endif                          // USE_FFMPEG
        }
        break;
    case GTK_RESPONSE_HELP:
        doHelp();
        break;
    case GTK_RESPONSE_REJECT:
        xml = glade_get_widget_tree(xvc_ctrl_m1);
        g_assert(xml);
        mitem = glade_xml_get_widget(xml, "xvc_ctrl_m1_mitem_preferences");
        g_assert(mitem);
        gtk_menu_item_activate(GTK_MENU_ITEM(mitem));

        xml = NULL;
        xml = glade_get_widget_tree(xvc_pref_main_window);
        g_assert(xml);
        mitem = glade_xml_get_widget(xml, "xvc_pref_cancel_button");
        g_assert(mitem);
        gtk_widget_set_sensitive(GTK_WIDGET(mitem), FALSE);

        warning_elist = xvc_errors_delete_list(warning_elist);
        xvc_pref_reset_OK_attempts();

        gtk_widget_destroy(xvc_warn_main_window);
        break;
    default:
        break;
    }
    
#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
}



/* 
 * this is one ugly hack
 */
void auto_resize_warning_dialog()
{
    #undef DEBUGFUNCTION
    #define DEBUGFUNCTION "auto_resize_warning_dialog()"
    GtkRequisition vp_size;
    int set_size = 0, orig_vp_size = 0;
    gint win_width = 0;
    gint win_height = 0;
    GtkWidget *vbox = NULL, *viewport = NULL, *eitem = NULL;
    GList *elist = NULL;
    int ind = 0, height = 0;
    GladeXML *xml = NULL;
    
#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    g_assert(xvc_warn_main_window);
    xml = glade_get_widget_tree(GTK_WIDGET(xvc_warn_main_window));
    vbox = glade_xml_get_widget(xml, "xvc_warn_errors_vbox");
    g_assert(vbox);


    elist = gtk_container_get_children(GTK_CONTAINER(vbox));
    eitem = g_list_nth_data(elist, 0);
    // the hack keeps getting uglier ...
    if (eitem->allocation.height > 1) {

        for (ind = 0; ind < g_list_length(elist); ind++) {
            eitem = g_list_nth_data(elist, ind);
            g_assert(eitem);
            // printf("eitem height: %i\n", eitem->allocation.height);
            height += (eitem->allocation.height + 1);
        }

        set_size = (height > 400 ? 400 : height);

        viewport = glade_xml_get_widget(xml, "xvc_warn_errors_viewport");
        g_assert(viewport);

        gtk_widget_size_request(viewport, &vp_size);
        orig_vp_size = vp_size.height;
        // printf("gtk2_warning: setting vp height to: %i ... old height:
        // %i \n", set_size, orig_vp_size);

        gtk_widget_set_size_request(viewport, -1, set_size);
        gtk_widget_size_request(viewport, &vp_size);
        // printf("gtk2_warning: new vp height: %i \n", vp_size.height);

        gtk_window_get_size(GTK_WINDOW(xvc_warn_main_window), &win_width,
                            &win_height);
        // printf("gtk2_warning: the window height is: %i \n",
        // win_height);
        gtk_window_resize(GTK_WINDOW(xvc_warn_main_window), win_width,
                          win_height - (orig_vp_size - vp_size.height) + 1);

        g_source_remove(scheduled_warning_resize_id);
    }
    
#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
}



GtkWidget *xvc_create_warning_with_errors(xvErrorListItem * elist,
                                          int from_where)
{
    #undef DEBUGFUNCTION
    #define DEBUGFUNCTION "xvc_create_warning_with_errors()"
    GtkWidget *vbox = NULL, *w = NULL;
    GladeXML *xml = NULL;
    int count_fatal_messages = 0;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    // save list for cleanup
    warning_elist = elist;
    // from_where = 0 means called from preferences dialog
    // = 1 means from control
    // = 2 means from startup check
    called_from_where = from_where;

    // load the interface
    xml = glade_xml_new(GLADE_FILE, "xvc_warn_main_window", NULL);

    g_assert(xml);

    // connect the signals in the interface 
    glade_xml_signal_autoconnect(xml);
    // store the toplevel widget for further reference
    xvc_warn_main_window =
        glade_xml_get_widget(xml, "xvc_warn_main_window");
    g_assert(xvc_warn_main_window);

    // set the error list
    if (elist != NULL) {
        xvErrorListItem *err = NULL;

        vbox = glade_xml_get_widget(xml, "xvc_warn_errors_vbox");
        g_assert(vbox);

        err = elist;
        for (; err != NULL; err = err->next) {
            GtkWidget *eitem;
            if (err->err->type != XV_ERR_INFO
                || (app->flags & FLG_RUN_VERBOSE)) {
                if (err->err->type == XV_ERR_FATAL)
                    count_fatal_messages++;
                eitem = xv_error_item_new_with_error(err->err);
                gtk_box_pack_start(GTK_BOX(vbox), eitem, FALSE, FALSE, 0);
                gtk_widget_show(eitem);
            }
        }
    } else {
        fprintf(stderr,
                "%s %s: displaying a warning with a NULL error list\n", DEBUGFILE, DEBUGFUNCTION);
    }

    // FIXME: depending on where we're called from make different buttons
    // visible/sensitive

    switch( called_from_where ) {
        case 0:
            w = NULL;
            w = glade_xml_get_widget(xml, "xvc_warn_pref_button");
            g_assert(w);
            gtk_widget_hide(w);
            
            w = NULL;
            w = glade_xml_get_widget(xml, "xvc_warn_cancel_button");
            g_assert(w);
            gtk_widget_show(w);
            break;
        case 1:
        case 2:
            w = NULL;
            w = glade_xml_get_widget(xml, "xvc_warn_pref_button");
            g_assert(w);
            gtk_widget_show(w);
            
            w = NULL;
            w = glade_xml_get_widget(xml, "xvc_warn_cancel_button");
            g_assert(w);
            gtk_widget_hide(w);
            break;
        default:
            break;
    }
#ifdef DEBUG
    printf("%s %s: called from where %i\n", DEBUGFILE, DEBUGFUNCTION, called_from_where);
#endif // DEBUG

    // auto-resize the dialog ... this is one ugly hack but the only way
    // to do it
    scheduled_warning_resize_id = g_timeout_add((guint32) 5, (GtkFunction)
                                                auto_resize_warning_dialog,
                                                NULL);

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    return xvc_warn_main_window;
}

