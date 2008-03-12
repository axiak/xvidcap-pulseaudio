/**
 * \file xvc_error_item.h
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
 */

#ifndef _xvc_XVC_ERROR_ITEM_H__
#define _xvc_XVC_ERROR_ITEM_H__

#include <gdk/gdk.h>
#include <gtk/gtkhbox.h>
#include "app_data.h"

#ifdef __cplusplus
extern "C"
{
#endif     /* __cplusplus */

#define XVC_ERROR_ITEM(obj) GTK_CHECK_CAST (obj, xvc_error_item_get_type (), XVC_ErrorItem)
#define XVC_ERROR_ITEM_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, xvc_error_item_get_type (), XVC_ErrorItemClass)
#define IS_XVC_ERROR_ITEM(obj) GTK_CHECK_TYPE (obj, xvc_error_item_get_type ())
#define XVC_ERROR_ITEM_TEXT_WIDTH 200

    typedef struct _xvc_ErrorItem XVC_ErrorItem;
    typedef struct _xvc_ErrorItemClass XVC_ErrorItemClass;

    struct _xvc_ErrorItem
    {
        GtkHBox hbox;
        GtkWidget *ebox;
        GtkWidget *image;
        GtkWidget *desc_tag;
        GtkWidget *desc_text;
        GtkWidget *title_tag;
        GtkWidget *title_text;
        GtkWidget *action_tag;
        GtkWidget *action_text;
    };

    struct _xvc_ErrorItemClass
    {
        GtkHBoxClass parent_class;

        void (*xvc_error_item) (XVC_ErrorItem * ei);
    };

    GtkType xvc_error_item_get_type (void);
    GtkWidget *xvc_error_item_new (void);
    GtkWidget *xvc_error_item_new_with_error (const XVC_Error * err);
    void xvc_error_item_set_error (XVC_ErrorItem * ei, const XVC_Error * err);

#ifdef __cplusplus
}
#endif     /* __cplusplus */
#endif     /* _xvc_XVC_ERROR_ITEM_H__ */
