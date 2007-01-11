/**
 * \file xv_error_item.h
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

#ifndef __XV_ERROR_ITEM_H__
#define __XV_ERROR_ITEM_H__

#include <gdk/gdk.h>
#include <gtk/gtkhbox.h>
#include "app_data.h"

#ifdef __cplusplus
extern "C"
{
#endif     /* __cplusplus */

#define XV_ERROR_ITEM(obj) GTK_CHECK_CAST (obj, xv_error_item_get_type (), XvErrorItem)
#define XV_ERROR_ITEM_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, xv_error_item_get_type (), XvErrorItemClass)
#define IS_XV_ERROR_ITEM(obj) GTK_CHECK_TYPE (obj, xv_error_item_get_type ())
#define XV_ERROR_ITEM_TEXT_WIDTH 200

    typedef struct _XvErrorItem XvErrorItem;
    typedef struct _XvErrorItemClass XvErrorItemClass;

    struct _XvErrorItem
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

    struct _XvErrorItemClass
    {
        GtkHBoxClass parent_class;

        void (*xv_error_item) (XvErrorItem * ei);
    };

    GtkType xv_error_item_get_type (void);
    GtkWidget *xv_error_item_new (void);
    GtkWidget *xv_error_item_new_with_error (const XVC_Error * err);
    void xv_error_item_set_error (XvErrorItem * ei, const XVC_Error * err);

#ifdef __cplusplus
}
#endif     /* __cplusplus */
#endif     /* __XV_ERROR_ITEM_H__ */
