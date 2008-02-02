/**
 * \file led_meter.c
 *
 * This file contains a composite widget drawing a led display used for
 * monitoring frame drop.
 */
/*
 * Copyright (C) 2003-07 Karl, Frankfurt
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
#include <gtk/gtktable.h>
#include <gtk/gtkdrawingarea.h>
#include <gtk/gtkviewport.h>
#include <gtk/gtktooltips.h>
#include <math.h>

#include "gtk/gtksignal.h"
#include "led_meter.h"

enum
{
    LED_METER_SIGNAL,
    LAST_SIGNAL
};

static void led_meter_class_init (LedMeterClass * klass);
static void led_meter_init (LedMeter * ttt);

static gint led_meter_signals[LAST_SIGNAL] = { 0 };

GType
led_meter_get_type ()
{
    static GType lm_type = 0;

    if (!lm_type) {
        static const GTypeInfo lm_info = {
            sizeof (LedMeterClass),
            NULL,
            NULL,
            (GClassInitFunc) led_meter_class_init,
            NULL,
            NULL,
            sizeof (LedMeter),
            0,
            (GInstanceInitFunc) led_meter_init,
        };

        lm_type =
            g_type_register_static (GTK_TYPE_VBOX, "LedMeter", &lm_info, 0);
    }

    return lm_type;
}

static void
led_meter_class_init (LedMeterClass * class)
{
    GtkObjectClass *object_class;

    object_class = (GtkObjectClass *) class;

    led_meter_signals[LED_METER_SIGNAL] = g_signal_new ("led_meter",
                                                        G_TYPE_FROM_CLASS
                                                        (object_class),
                                                        G_SIGNAL_RUN_FIRST,
                                                        0, NULL, NULL,
                                                        g_cclosure_marshal_VOID__VOID,
                                                        G_TYPE_NONE, 0, NULL);

    class->led_meter = NULL;
}

static void
led_meter_init (LedMeter * lm)
{
    GdkColor g_col;
    GtkWidget *table, *viewport1;
    int i;

    // set colours [x][0] for inactive [x][1] for active
    lm->led_background[LM_LOW][0].red = 0;
    lm->led_background[LM_LOW][0].green = 0x3333;
    lm->led_background[LM_LOW][0].blue = 0;
    lm->led_background[LM_LOW][1].red = 0;
    lm->led_background[LM_LOW][1].green = 0xFFFF;
    lm->led_background[LM_LOW][1].blue = 0;
    lm->led_background[LM_MEDIUM][0].red = 0x3333;
    lm->led_background[LM_MEDIUM][0].green = 0x3333;
    lm->led_background[LM_MEDIUM][0].blue = 0;
    lm->led_background[LM_MEDIUM][1].red = 0xFFFF;
    lm->led_background[LM_MEDIUM][1].green = 0xFFFF;
    lm->led_background[LM_MEDIUM][1].blue = 0;
    lm->led_background[LM_HIGH][0].red = 0x3333;
    lm->led_background[LM_HIGH][0].green = 0;
    lm->led_background[LM_HIGH][0].blue = 0;
    lm->led_background[LM_HIGH][1].red = 0xFFFF;
    lm->led_background[LM_HIGH][1].green = 0;
    lm->led_background[LM_HIGH][1].blue = 0;
    // set current max_da to 0 (no led active)
    lm->max_da = 0;
    lm->old_max_da = 0;

    gtk_container_set_border_width (GTK_CONTAINER (lm), 0);

    // add viewport to get shadow
    viewport1 = gtk_viewport_new (NULL, NULL);
    gtk_widget_show (viewport1);
    gtk_container_add (GTK_CONTAINER (lm), viewport1);
    // gtk_container_set_border_width (GTK_CONTAINER (viewport1), 8);

    // add eventbox to be able to set a background colour
    lm->ebox = gtk_event_box_new ();
    gtk_container_add (GTK_CONTAINER (viewport1), lm->ebox);

    g_col.red = 0;
    g_col.green = 0;
    g_col.blue = 0;
    gtk_widget_modify_bg (lm->ebox, GTK_STATE_NORMAL, &g_col);

    // add a table for the da/led layout
    table = gtk_table_new (LM_NUM_DAS, 1, TRUE);
    gtk_container_add (GTK_CONTAINER (lm->ebox), table);
    gtk_widget_show (table);

    // add the das/leds
    for (i = 0; i < LM_NUM_DAS; i++) {
        lm->da[i] = gtk_drawing_area_new ();

        // set colours according to threshholds defined elsewhere
        if (i < LM_LOW_THRESHOLD)
            g_col = lm->led_background[LM_LOW][0];
        else if (i < LM_MEDIUM_THRESHOLD)
            g_col = lm->led_background[LM_MEDIUM][0];
        else
            g_col = lm->led_background[LM_HIGH][0];
        gtk_widget_modify_bg (lm->da[i], GTK_STATE_NORMAL, &g_col);

        gtk_widget_set_size_request (lm->da[i], 8, 1);
        gtk_table_attach (GTK_TABLE (table), lm->da[i], 0, 1,
                          (LM_NUM_DAS - i - 1), (LM_NUM_DAS - i),
                          (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
                          (GTK_EXPAND | GTK_SHRINK | GTK_FILL), 0, 0);
        gtk_widget_show (lm->da[i]);
        gtk_table_set_row_spacing (GTK_TABLE (table), i, 1);
    }

    gtk_widget_show (lm->ebox);
}

/**
 * \brief creates a new led_meter widget
 *
 * @return new led_meter widget
 */
GtkWidget *
led_meter_new ()
{
    return GTK_WIDGET (g_object_new (led_meter_get_type (), NULL));
}

/**
 * \brief sets the percentage to use for the amplitude in the led meter
 *
 * @param lm pointer to the led meter widget to work on
 * @param percent percentag value (1-100)
 */
void
led_meter_set_percent (LedMeter * lm, int percent)
{
    int n;
    GdkColor g_col;

    if (percent > 100)
        percent = 100;
    if (percent < 0)
        percent = 0;

    lm->max_da = (LM_NUM_DAS * percent / 100) - 1;

    if (lm->max_da == lm->old_max_da)
        return;
    if (lm->max_da > lm->old_max_da) {
        for (n = lm->old_max_da; n <= lm->max_da; n++) {
            if (n < LM_LOW_THRESHOLD)
                g_col = lm->led_background[LM_LOW][1];
            else if (n < LM_MEDIUM_THRESHOLD)
                g_col = lm->led_background[LM_MEDIUM][1];
            else
                g_col = lm->led_background[LM_HIGH][1];
            gtk_widget_modify_bg (GTK_WIDGET (lm->da[n]), GTK_STATE_NORMAL,
                                  &g_col);
        }
    } else {
        for (n = lm->old_max_da; n >= lm->max_da; n--) {
            if (n < LM_LOW_THRESHOLD)
                g_col = lm->led_background[LM_LOW][0];
            else if (n < LM_MEDIUM_THRESHOLD)
                g_col = lm->led_background[LM_MEDIUM][0];
            else
                g_col = lm->led_background[LM_HIGH][0];

            gtk_widget_modify_bg (GTK_WIDGET (lm->da[n]), GTK_STATE_NORMAL,
                                  &g_col);
        }
    }
    lm->old_max_da = lm->max_da;

}

/**
 * \brief sets tooltip on an led meter widget
 *
 * @param lm pointer to led meter widget to work on
 * @param tip string to set as tooltip
 */
void
led_meter_set_tip (LedMeter * lm, const char *tip)
{
    static GtkTooltips *tooltips;

    tooltips = gtk_tooltips_new ();

    gtk_tooltips_set_tip (tooltips, lm->ebox, tip, NULL);
}
