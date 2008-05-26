/**
 * \file xvc_error_item.c
 *
 * this file contains the widget used for displaying an individual error in
 * the warning dialog.
 */
/*
 * Copyright (C) 2003 Karl H. Beckers, Frankfurt
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
 */

#include <stdio.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkimage.h>
#include <gtk/gtksignal.h>
#include <math.h>

#include "xvc_error_item.h"
#include "xvidcap-intl.h"

enum
{
    XVC_ERROR_ITEM_SIGNAL,
    LAST_SIGNAL
};

static void xvc_error_item_class_init (XVC_ErrorItemClass * klass);
static void xvc_error_item_init (XVC_ErrorItem * ei);

static gint xvc_error_item_signals[LAST_SIGNAL] = { 0 };

GType
xvc_error_item_get_type ()
{
    static GType ei_type = 0;

    if (!ei_type) {
        static const GTypeInfo ei_info = {
            sizeof (XVC_ErrorItemClass),
            NULL,
            NULL,
            (GClassInitFunc) xvc_error_item_class_init,
            NULL,
            NULL,
            sizeof (XVC_ErrorItem),
            0,
            (GInstanceInitFunc) xvc_error_item_init,
        };

        ei_type =
            g_type_register_static (GTK_TYPE_HBOX, "XVC_ErrorItem", &ei_info,
                                    0);
    }

    return ei_type;
}

static void
xvc_error_item_class_init (XVC_ErrorItemClass * class)
{
    GtkObjectClass *object_class;

    object_class = (GtkObjectClass *) class;

    xvc_error_item_signals[XVC_ERROR_ITEM_SIGNAL] =
        g_signal_new ("xvc_error_item", G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, NULL);

    class->xvc_error_item = NULL;
}

static void
xvc_error_item_init (XVC_ErrorItem * ei)
{
    GdkColor g_col;
    GtkWidget *frame;
    GtkWidget *hbox;
    GtkWidget *table;
    GtkWidget *title_text_spacer;
    int i, max_width = 0;

    // gtk_container_set_border_width (GTK_CONTAINER(ei), 1);

    // add eventbox to be able to set a background colour
    ei->ebox = gtk_event_box_new ();
    gtk_container_add (GTK_CONTAINER (ei), ei->ebox);

    g_col.red = 0xFFFF;
    g_col.green = 0xFFFF;
    g_col.blue = 0xFFFF;
    gtk_widget_modify_bg (ei->ebox, GTK_STATE_NORMAL, &g_col);

    frame = gtk_frame_new (NULL);
    gtk_widget_show (frame);
    gtk_container_add (GTK_CONTAINER (ei->ebox), frame);
    gtk_frame_set_label_align (GTK_FRAME (frame), 0, 0);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show (hbox);
    gtk_container_add (GTK_CONTAINER (frame), hbox);

    ei->image =
        gtk_image_new_from_stock ("gtk-dialog-warning",
                                  GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_widget_show (ei->image);
    gtk_box_pack_start (GTK_BOX (hbox), ei->image, FALSE, FALSE, 0);
    gtk_misc_set_alignment (GTK_MISC (ei->image), 0, 0);
    gtk_misc_set_padding (GTK_MISC (ei->image), 4, 4);

    table = gtk_table_new (3, 2, FALSE);
    gtk_widget_show (table);
    gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, TRUE, 0);

    // calculate maximum width for left column, esp. for i18n
    for (i = 0; i < 5; i++) {
        char buf[256];
        PangoLayout *layout = NULL;
        int width, height;

        switch (i) {
        case 0:
            snprintf (buf, 255, _("FATAL\nERROR (%i):"), 100);
            break;
        case 1:
            snprintf (buf, 255, _("ERROR (%i):"), 100);
            break;
        case 2:
            snprintf (buf, 255, _("WARNING (%i):"), 100);
            break;
        case 3:
            snprintf (buf, 255, _("INFO (%i):"), 100);
            break;
        case 4:
            snprintf (buf, 255, _("???? (%i):"), 100);
            break;
        }

        title_text_spacer = gtk_label_new (buf);

        layout = gtk_widget_create_pango_layout (title_text_spacer, buf);
        g_assert (layout);
        pango_layout_get_pixel_size (layout, &width, &height);
        g_object_unref (layout);
        gtk_widget_destroy (title_text_spacer);

        if (width > max_width)
            max_width = width;
    }

    ei->title_tag = gtk_label_new ("ERROR (n):");
    gtk_widget_show (ei->title_tag);
    gtk_table_attach (GTK_TABLE (table), ei->title_tag, 0, 1, 0, 1,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (GTK_FILL), 0, 0);
    gtk_widget_set_size_request (ei->title_tag, max_width, -1);
    gtk_misc_set_alignment (GTK_MISC (ei->title_tag), 0, 0);
    gtk_misc_set_padding (GTK_MISC (ei->title_tag), 2, 1);

    ei->title_text = gtk_label_new ("this is an error");
    gtk_widget_show (ei->title_text);
    gtk_table_attach (GTK_TABLE (table), ei->title_text, 1, 2, 0, 1,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
    // gtk_widget_set_size_request(ei->title_text, 200, -1);
    gtk_label_set_line_wrap (GTK_LABEL (ei->title_text), TRUE);
    gtk_misc_set_alignment (GTK_MISC (ei->title_text), 0, 0);
    gtk_misc_set_padding (GTK_MISC (ei->title_text), 2, 1);

    ei->desc_tag = gtk_label_new (_("Details:"));
    gtk_widget_show (ei->desc_tag);
    gtk_table_attach (GTK_TABLE (table), ei->desc_tag, 0, 1, 1, 2,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (GTK_FILL), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (ei->desc_tag), 0, 0);
    gtk_misc_set_padding (GTK_MISC (ei->desc_tag), 2, 1);

    ei->desc_text = gtk_text_view_new ();
    gtk_widget_modify_base (ei->desc_text, GTK_STATE_NORMAL, &g_col);
    gtk_widget_set_size_request (ei->desc_text, 200, -1);
    gtk_widget_show (ei->desc_text);
    gtk_table_attach (GTK_TABLE (table), ei->desc_text, 1, 2, 1, 2,
                      (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                      (GtkAttachOptions) (GTK_FILL), 0, 0);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (ei->desc_text), FALSE);
    gtk_text_view_set_accepts_tab (GTK_TEXT_VIEW (ei->desc_text), FALSE);
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (ei->desc_text), GTK_WRAP_WORD);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (ei->desc_text), FALSE);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (ei->desc_text), 2);

    gtk_text_buffer_set_text (gtk_text_view_get_buffer
                              (GTK_TEXT_VIEW (ei->desc_text)),
                              "description, this is a test description with multiple lines",
                              -1);

    ei->action_tag = gtk_label_new (_("Action:"));
    gtk_widget_show (ei->action_tag);
    gtk_table_attach (GTK_TABLE (table), ei->action_tag, 0, 1, 2, 3,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (GTK_FILL), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (ei->action_tag), 0, 0);
    gtk_misc_set_padding (GTK_MISC (ei->action_tag), 2, 1);

    ei->action_text = gtk_label_new_with_mnemonic ("what to do ...");
    gtk_widget_show (ei->action_text);
    gtk_table_attach (GTK_TABLE (table), ei->action_text, 1, 2, 2, 3,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
    gtk_label_set_line_wrap (GTK_LABEL (ei->action_text), TRUE);
    // gtk_widget_set_size_request(ei->action_text, 200, -1);
    gtk_misc_set_alignment (GTK_MISC (ei->action_text), 0, 0);
    gtk_misc_set_padding (GTK_MISC (ei->action_text), 2, 1);

    gtk_widget_show (ei->ebox);
}

GtkWidget *
xvc_error_item_new ()
{
    return GTK_WIDGET (g_object_new (xvc_error_item_get_type (), NULL));
}

GtkWidget *
xvc_error_item_new_with_error (const XVC_Error * err)
{
    GtkWidget *wid;

    wid = GTK_WIDGET (g_object_new (xvc_error_item_get_type (), NULL));
    xvc_error_item_set_error (XVC_ERROR_ITEM (wid), err);

    return wid;
}

void
xvc_error_item_set_error (XVC_ErrorItem * ei, const XVC_Error * err)
{
    if (err != NULL && ei != NULL) {
        char ttag[128];

        switch (err->type) {
        case XVC_ERR_FATAL:
            gtk_image_set_from_stock (GTK_IMAGE (ei->image),
                                      "gtk-dialog-error",
                                      GTK_ICON_SIZE_LARGE_TOOLBAR);
            snprintf (ttag, 128, _("FATAL\nERROR (%i):"), err->code);
            break;
        case XVC_ERR_ERROR:
            gtk_image_set_from_stock (GTK_IMAGE (ei->image),
                                      "gtk-dialog-warning",
                                      GTK_ICON_SIZE_LARGE_TOOLBAR);
            snprintf (ttag, 128, _("ERROR (%i):"), err->code);
            break;
        case XVC_ERR_WARN:
            gtk_image_set_from_stock (GTK_IMAGE (ei->image),
                                      "gtk-dialog-info",
                                      GTK_ICON_SIZE_LARGE_TOOLBAR);
            snprintf (ttag, 128, _("WARNING (%i):"), err->code);
            break;
        case XVC_ERR_INFO:
            gtk_image_set_from_stock (GTK_IMAGE (ei->image),
                                      "gtk-dialog-info",
                                      GTK_ICON_SIZE_LARGE_TOOLBAR);
            snprintf (ttag, 128, _("INFO (%i):"), err->code);
            break;
        default:
            gtk_image_set_from_stock (GTK_IMAGE (ei->image),
                                      "gtk-dialog-info",
                                      GTK_ICON_SIZE_LARGE_TOOLBAR);
            snprintf (ttag, 128, "???? (%i):", err->code);
        }
        gtk_label_set_text (GTK_LABEL (ei->title_tag), strdup (ttag));
        gtk_label_set_text (GTK_LABEL (ei->title_text), _(err->short_msg));
        gtk_text_buffer_set_text (gtk_text_view_get_buffer
                                  (GTK_TEXT_VIEW (ei->desc_text)),
                                  _(err->long_msg), -1);
        gtk_label_set_text (GTK_LABEL (ei->action_text), _(err->action_msg));
    }

}
