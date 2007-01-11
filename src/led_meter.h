/**
 * \file led_meter.h
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

#ifndef __LED_METER_H__
#define __LED_METER_H__

#include <gdk/gdk.h>
#include <gtk/gtkvbox.h>

#ifdef __cplusplus
extern "C"
{
#endif     /* __cplusplus */

#define LED_METER(obj) GTK_CHECK_CAST (obj, led_meter_get_type (), LedMeter)
#define LED_METER_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, led_meter_get_type (), LedMeterClass)
#define IS_LED_METER(obj) GTK_CHECK_TYPE (obj, led_meter_get_type ())
/** \brief the number of leds */
#define LM_NUM_DAS 10
/** \brief the highest led number regarded as low frame drop */
#define LM_LOW_THRESHOLD 3
/** \brief the highest led number regarded as medium frame drop */
#define LM_MEDIUM_THRESHOLD 6
/** \brief the highest led number regarded as high frame drop */
#define LM_HIGH_THRESHOLD LM_NUM_DAS

/** \brief the categories the led display is divided in */
    enum LED_METER_DACategory
    {
        LM_LOW,
        LM_MEDIUM,
        LM_HIGH
    };

    typedef struct _LedMeter LedMeter;
    typedef struct _LedMeterClass LedMeterClass;

    struct _LedMeter
    {
        GtkVBox vbox;
        GtkWidget *ebox;
        GtkWidget *da[LM_NUM_DAS];
        GdkColor led_background[3][2];
        gint max_da, old_max_da;
    };

    struct _LedMeterClass
    {
        GtkVBoxClass parent_class;

        void (*led_meter) (LedMeter * lm);
    };

    GtkType led_meter_get_type (void);
    GtkWidget *led_meter_new (void);
    void led_meter_set_percent (LedMeter *, int);
    void led_meter_set_tip (LedMeter * lm, char *tip);

#ifdef __cplusplus
}
#endif     /* __cplusplus */
#endif     /* __LED_METER_H__ */
