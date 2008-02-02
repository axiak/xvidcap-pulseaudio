/**
 * \file gnome_options.c
 *
 * This file contains the options dialog for the GTK2 control
 * \todo the whole display of preferences and warning dialogs should be
 *      simplified through _run_dialog()
 */

/*
 * Copyright (C) 2003-07 Karl H. Beckers, Frankfurt
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
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define DEBUGFILE "gnome_options.c"
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Intrinsic.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glade/glade.h>

#include "job.h"
#include "app_data.h"
#include "codecs.h"
#include "gnome_warning.h"
#include "control.h"
#include "gnome_ui.h"
#include "xvidcap-intl.h"

/*
 * these are global
 */
/** \brief make the preferences window globally available */
GtkWidget *xvc_pref_main_window = NULL;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
/* the following is a pointer to the warning dialog */
extern GtkWidget *xvc_warn_main_window;

/* the following is a pointer to the main control */
extern GtkWidget *xvc_ctrl_main_window;
#endif     // DOXYGEN_SHOULD_SKIP_THIS

/**
 * \brief an XVC_AppData struct with the model used by the preferences dialog
 *
 * this is copied off the global app struct on creation of the preferences
 * dialog, set on submit and assimilated back to the global app struct
 */
static XVC_AppData pref_app;

/**
 * \brief holds the strings to fill the dropdown box for multi-frame capture
 * formats supported.
 *
 * They are formed of the combined longname and first extension
 */
static char *format_combo_entries[NUMCAPS];

/**
 * \brief remember the number of attempts to submit a set of preferences
 *
 * Hitting OK on a warning may make automatic changes to the preferences.
 * But it may not resolve all conflicts in one go and pop up the warning
 * dialog again, again offering automatic resultion actions. This should be
 * a rare case, however
 */
static int OK_attempts = 0;

/**
 * \brief the errors found on submitting a set of preferences
 *
 * Used to pass this between the various components involved in deciding
 * what to do when OK is clicked
 * \todo the whole display of preferences and warning dialogs should be
 *      simplified through _run_dialog()
 */
static XVC_ErrorListItem *errors_after_cli = NULL;

#ifdef USE_FFMPEG
/**
 * \brief Upon change of a file format for multi-frame capture the callback
 *      saves the new value here, so that next time around it is the old one.
 *
 * When the callback is executed, the current value in the dropdown box is
 * already the new one. To evaluate what needs to be done on codecs and
 * audio codecs we need to know what format was previously selected, because
 * the number of the selected codec is relative to the array of available
 * codecs for a given format. In other words: If codec 1 is selected from
 * the codecs combobox, that may be a different one depending on format
 */
static XVC_FFormatID old_selected_format = CAP_NONE;

/**
 * \brief Upon change of a codec for multi-frame capture the callback
 *      saves the new value here, so that next time around it is the old one.
 *
 * There is a similar relation between mf codec and fps as with
 * old_selected_format, format and codec.
 * @see old_selected_format
 */
static XVC_CodecID mf_fps_widget_save_old_codec = -1;
#endif     // USE_FFMPEG

// utility functions here ...
//
/**
 * \brief find an int element in an array of ints and return the index of
 *      the element. Such arrays are esp xvc_formats, xvc_codecs,
 *      xvc_audio_codecs
 *
 * @param size the size of the array to search in
 * @param haystack the array
 * @param needle the int value to look for
 * @return the index of the found element or -1 if not found
 */
static int
get_index_of_array_element (int size, int *haystack, int needle)
{
    int i, found = -1;

    for (i = 0; i < size; i++) {
        if (haystack[i] == needle) {
            found = i;
            return found;
        }
    }
    return found;
}

/**
 * \brief read the settings from the preferences window back into an
 *      XVC_AppData struct
 *
 * @param lapp pointer to an already created XVC_AppData struct
 */
static void
read_app_data_from_pref_gui (XVC_AppData * lapp)
{
#define DEBUGFUNCTION "read_app_data_from_pref_gui()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    XVC_FFormatID read_mf_format = CAP_NONE;
    XVC_CodecID read_mf_codec = CODEC_NONE;

#ifdef HAVE_FFMPEG_AUDIO
    XVC_AuCodecID read_mf_au_codec = AU_CODEC_NONE;
#endif     // HAVE_FFMPEG_AUDIO

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);

    // general tab
    // default capture mode
    w = NULL;
    w = glade_xml_get_widget (xml,
                              "xvc_pref_default_capture_mode_sf_radiobutton");
    g_assert (w);

    lapp->default_mode =
        ((gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) ? 0 : 1);

    // capture mouse pointer
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_capture_mouse_checkbutton");
    g_assert (w);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
        w = NULL;
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_capture_mouse_white_radiobutton");
        g_assert (w);

        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
            lapp->mouseWanted = 1;
        } else {
            lapp->mouseWanted = 2;
        }
    } else {
        lapp->mouseWanted = 0;
    }

    // capture area follows mouse pointer
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_follow_mouse_checkbutton");
    g_assert (w);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
        lapp->flags |= FLG_LOCK_FOLLOWS_MOUSE;
    } else {
        lapp->flags &= ~FLG_LOCK_FOLLOWS_MOUSE;
    }

    // hide frame around capture area
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_hide_frame_checkbutton");
    g_assert (w);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
        lapp->flags |= FLG_NOFRAME;
    } else {
        lapp->flags &= ~FLG_NOFRAME;
    }

    // save geometry
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_save_geometry_checkbutton");
    g_assert (w);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
        lapp->flags |= FLG_SAVE_GEOMETRY;
    } else {
        lapp->flags &= ~FLG_SAVE_GEOMETRY;
    }

#ifdef HAVE_SHMAT
    // use shared-memory
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_shared_mem_checkbutton");
    g_assert (w);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
        lapp->flags |= FLG_USE_SHM;
    } else {
        lapp->flags &= ~FLG_USE_SHM;
    }
#endif     // HAVE_SHMAT

#ifdef USE_XDAMAGE
    // use xdamage
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_use_xdamage_auto_checkbutton");
    g_assert (w);

    if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
        lapp->use_xdamage = -1;
    } else {
        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_use_xdamage_no_radiobutton");
        g_assert (w);
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
            lapp->use_xdamage = 0;
            lapp->flags &= ~FLG_USE_XDAMAGE;
        } else {
            lapp->use_xdamage = 1;
            if (lapp->dmg_event_base != 0)
                lapp->flags |= FLG_USE_XDAMAGE;
        }
    }
#endif     // USE_XDAMAGE

    // autocontinue
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_autocontinue_checkbutton");
    g_assert (w);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
        lapp->flags |= FLG_AUTO_CONTINUE;
    } else {
        lapp->flags &= ~FLG_AUTO_CONTINUE;
    }

    // always show results
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_always_show_results_checkbutton");
    g_assert (w);
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
        lapp->flags |= FLG_ALWAYS_SHOW_RESULTS;
    } else {
        lapp->flags &= ~FLG_ALWAYS_SHOW_RESULTS;
    }

    // minimize to system tray
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_minimize_to_tray");
    g_assert (w);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
        lapp->flags |= FLG_TO_TRAY;
    } else {
        lapp->flags &= ~FLG_TO_TRAY;
    }

    // rescale
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_rescale_hscale");
    g_assert (w);
    lapp->rescale = gtk_range_get_value (GTK_RANGE (w));

    // sf
    // file name
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_sf_filename_entry");
    g_assert (w);

    lapp->single_frame.file =
        strdup ((char *) gtk_entry_get_text (GTK_ENTRY (w)));

    // file format
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_sf_format_auto_checkbutton");
    g_assert (w);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
        lapp->single_frame.target = 0;
    } else {
        int m = -1;

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_sf_format_combobox");
        g_assert (w);

        m = gtk_combo_box_get_active (GTK_COMBO_BOX (w)) + 1;

        g_assert (m >= 0);
        lapp->single_frame.target = m;
    }

    // codec is always 0 for sf
    lapp->single_frame.targetCodec = 0;
#ifdef HAVE_FFMPEG_AUDIO
    // au_targetCodec is always 0 for sf
    lapp->single_frame.au_targetCodec = 0;
#endif     // HAVE_FFMPEG_AUDIO

    // fps
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_sf_fps_hscale");
    g_assert (w);

    lapp->single_frame.fps.num =
        (int) (gtk_range_get_value (GTK_RANGE (w)) *
               pow (10, gtk_scale_get_digits (GTK_SCALE (w))));
    lapp->single_frame.fps.den = pow (10, gtk_scale_get_digits (GTK_SCALE (w)));

    // quality
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_sf_quality_hscale");
    g_assert (w);

    lapp->single_frame.quality = gtk_range_get_value (GTK_RANGE (w));

    // max time
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_sf_max_time_spinbutton");
    g_assert (w);

    lapp->single_frame.time =
        (int) gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (w));

    // max frames
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_sf_max_frames_spinbutton");
    g_assert (w);

    lapp->single_frame.frames =
        (int) gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (w));

    // start_no
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_sf_start_no_spinbutton");
    g_assert (w);

    lapp->single_frame.start_no =
        (int) gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (w));

    // step
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_sf_frame_increment_spinbutton");
    g_assert (w);

    lapp->single_frame.step =
        (int) gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (w));

#ifdef USE_FFMPEG

    // mf

    // file name
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_filename_entry");
    g_assert (w);

    lapp->multi_frame.file =
        strdup ((char *) gtk_entry_get_text (GTK_ENTRY (w)));

    // format
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_format_auto_checkbutton");
    g_assert (w);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
        lapp->multi_frame.target = 0;
    } else {
        int n = -1;

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_format_combobox");
        g_assert (w);

        n = gtk_combo_box_get_active (GTK_COMBO_BOX (w)) + CAP_MF;

        g_assert (n >= CAP_MF);
        lapp->multi_frame.target = n;
    }

    read_mf_format = lapp->multi_frame.target;
    if (!read_mf_format)
        read_mf_format =
            xvc_codec_get_target_from_filename (lapp->multi_frame.file);

    // codec
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_codec_auto_checkbutton");
    g_assert (w);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
        lapp->multi_frame.targetCodec = 0;
    } else {
        int codec = -1;

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_codec_combobox");
        g_assert (w);

        codec =
            xvc_formats[read_mf_format].
            allowed_vid_codecs[gtk_combo_box_get_active (GTK_COMBO_BOX (w))];

        g_assert (codec >= 0);
        lapp->multi_frame.targetCodec = codec;
    }
    read_mf_codec = pref_app.multi_frame.targetCodec;
    if (!read_mf_codec)
        read_mf_codec = xvc_formats[read_mf_format].def_vid_codec;

    // fps
    // we need codec determined before here
    {
        gboolean combobox_visible = FALSE, hscale_visible = FALSE;
        GtkWidget *combobox = NULL, *hscale = NULL;

        combobox = glade_xml_get_widget (xml, "xvc_pref_mf_fps_combobox");
        g_assert (combobox);
        gtk_object_get (GTK_OBJECT (combobox), "visible", &combobox_visible,
                        NULL);

        hscale = glade_xml_get_widget (xml, "xvc_pref_mf_fps_hscale");
        g_assert (hscale);
        gtk_object_get (GTK_OBJECT (hscale), "visible", &hscale_visible, NULL);

        if (combobox_visible) {
            int selected_fps = gtk_combo_box_get_active (GTK_COMBO_BOX
                                                         (combobox));
            lapp->multi_frame.fps =
                xvc_codecs[read_mf_codec].allowed_fps[selected_fps];
        } else if (hscale_visible) {
            lapp->multi_frame.fps.num =
                (int) (gtk_range_get_value (GTK_RANGE (hscale)) *
                       pow (10, gtk_scale_get_digits (GTK_SCALE (hscale))));
            lapp->multi_frame.fps.den =
                pow (10, gtk_scale_get_digits (GTK_SCALE (hscale)));
        }
        if (((!combobox_visible) && (!hscale_visible)) ||
            (combobox_visible && hscale_visible)) {
            fprintf (stderr,
                     "%s %s: unable to determine type of mf_fps_widget, please file a bug!\n",
                     DEBUGFILE, DEBUGFUNCTION);
            exit (1);
        }
#ifdef DEBUG
        printf ("%s %s: read fps: %f\n", DEBUGFILE, DEBUGFUNCTION,
                ((float) lapp->multi_frame.fps.num /
                 (float) lapp->multi_frame.fps.den));
#endif     // DEBUG
    }

    // max time
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_max_time_spinbutton");
    g_assert (w);

    lapp->multi_frame.time =
        (int) gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (w));

    // max frames
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_max_frames_spinbutton");
    g_assert (w);

    lapp->multi_frame.frames =
        (int) gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (w));

    // quality
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_quality_hscale");
    g_assert (w);

    lapp->multi_frame.quality = gtk_range_get_value (GTK_RANGE (w));

#ifdef HAVE_FFMPEG_AUDIO
    // mf audio settings
    // au_targetCodec
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_codec_auto_checkbutton");
    g_assert (w);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
        lapp->multi_frame.au_targetCodec = 0;
    } else {
        int aucodec = -1;
        int selected_aucodec;

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_codec_combobox");
        g_assert (w);

        selected_aucodec = gtk_combo_box_get_active (GTK_COMBO_BOX (w));

        if (selected_aucodec >= 0 &&
            xvc_formats[read_mf_format].num_allowed_au_codecs > 0) {
            aucodec =
                xvc_formats[read_mf_format].allowed_au_codecs[selected_aucodec];
        }

        lapp->multi_frame.au_targetCodec = aucodec;
    }
    read_mf_au_codec = pref_app.multi_frame.au_targetCodec;
    if (!read_mf_au_codec)
        read_mf_au_codec = xvc_formats[read_mf_format].def_au_codec;

    // audio wanted
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_checkbutton");
    g_assert (w);

    lapp->multi_frame.audioWanted =
        (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)) ? 1 : 0);

    // sample rate
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_sample_rate_spinbutton");
    g_assert (w);

    lapp->multi_frame.sndrate =
        (int) gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (w));

    // bit rate
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_bit_rate_spinbutton");
    g_assert (w);

    lapp->multi_frame.sndsize =
        (int) gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (w));

    // audio in
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_input_device_entry");
    g_assert (w);

    lapp->snddev = strdup ((char *) gtk_entry_get_text (GTK_ENTRY (w)));

    // audio channels
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_channels_hscale");
    g_assert (w);

    lapp->multi_frame.sndchannels = gtk_range_get_value (GTK_RANGE (w));
#endif     // HAVE_FFMPEG_AUDIO

#endif     // USE_FFMPEG

    // commands

    // sf commands
    // sf playback command
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_commands_sf_play_entry");
    g_assert (w);

    lapp->single_frame.play_cmd =
        strdup ((char *) gtk_entry_get_text (GTK_ENTRY (w)));

    // sf encoding command
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_commands_sf_encode_entry");
    g_assert (w);

    lapp->single_frame.video_cmd =
        strdup ((char *) gtk_entry_get_text (GTK_ENTRY (w)));

    // sf edit command
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_commands_sf_edit_entry");
    g_assert (w);

    lapp->single_frame.edit_cmd =
        strdup ((char *) gtk_entry_get_text (GTK_ENTRY (w)));

#ifdef USE_FFMPEG
    // mf commands
    // mf playback command
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_commands_mf_play_entry");
    g_assert (w);

    lapp->multi_frame.play_cmd =
        strdup ((char *) gtk_entry_get_text (GTK_ENTRY (w)));

    // mf encoding command
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_commands_mf_encode_entry");
    g_assert (w);

    lapp->multi_frame.video_cmd =
        strdup ((char *) gtk_entry_get_text (GTK_ENTRY (w)));

    // mf edit command
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_commands_mf_edit_entry");
    g_assert (w);

    lapp->multi_frame.edit_cmd =
        strdup ((char *) gtk_entry_get_text (GTK_ENTRY (w)));
#endif     // USE_FFMPEG

#undef DEBUGFUNCTION
}

/**
 * \brief assimilates the changed preferences into xvidcap's current config
 */
void
preferences_submit ()
{
#define DEBUGFUNCTION "preferences_submit()"
    XVC_AppData *app = xvc_appdata_ptr ();

    xvc_appdata_copy (app, &pref_app);

    xvc_job_set_from_app_data (app);

    // set controls active/inactive/sensitive/insensitive according to
    // current options
    xvc_reset_ctrl_main_window_according_to_current_prefs ();

    if (xvc_errorlist_delete (errors_after_cli)) {
        fprintf (stderr,
                 "%s %s: Unrecoverable error while freeing error list, please contact the xvidcap project.\n",
                 DEBUGFILE, DEBUGFUNCTION);
        exit (1);
    }
    // reset attempts so warnings will be shown again next time ...
    OK_attempts = 0;

    gtk_widget_destroy (xvc_pref_main_window);

#undef DEBUGFUNCTION
}

/**
 * \brief resets the cound of attempts to submit a set of preferences to zero
 */
void
xvc_pref_reset_OK_attempts ()
{
    OK_attempts = 0;
}

/**
 * \brief tries to submit a set of preferences. This will validate the
 *      settings and pop up a warning in case of errors. This is called
 *      when a user clicks OK in the preferences window.
 */
void
xvc_pref_do_OK ()
{
#define DEBUGFUNCTION "xvc_pref_do_OK()"
    int count_non_info_messages = 0, rc = 0;
    XVC_AppData *app = xvc_appdata_ptr ();

    errors_after_cli = xvc_appdata_validate (&pref_app, 0, &rc);
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
                xvc_create_warning_with_errors (errors_after_cli, 0);
            // printf("gtk2_options: pointer to errors_after_cli: %p - rc:
            // %i\n", errors_after_cli, rc);
            OK_attempts++;

        } else {
            preferences_submit ();
        }
    } else {
        preferences_submit ();
    }

    // if ( count_non_info_messages > 0 ) {
    // fprintf (stderr, "gtk2_options: Can't resolve all conflicts in
    // input data. Please double-check your input.\n");
    // exit (1);
    // }
#undef DEBUGFUNCTION
}

/**
 * \brief displays the xvidcap user manual. This is used when a user
 *      clicks the Help button on the preferences GUI.
 */
static void
doHelp ()
{
#define DEBUGFUNCTION "doHelp()"
    GladeXML *xml = NULL;
    GtkWidget *notebook = NULL;
    int cur_tab = -1;
    const char *cmd = NULL;

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);

    notebook = glade_xml_get_widget (xml, "xvc_pref_notebook");
    g_assert (notebook);

    cur_tab = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
#ifdef HAVE_LIBAVCODEC
    switch (cur_tab) {
    case 0:
        cmd = "yelp ghelp:xvidcap?xvidcap-prefs-general &";
        break;
    case 1:
        cmd = "yelp ghelp:xvidcap?xvidcap-prefs-sf &";
        break;
    case 2:
        cmd = "yelp ghelp:xvidcap?xvidcap-prefs-mf &";
        break;
    case 3:
        cmd = "yelp ghelp:xvidcap?xvidcap-prefs-cmd &";
        break;
    }
#else      // HAVE_LIBAVCODEC
    switch (cur_tab) {
    case 0:
        cmd = "yelp ghelp:xvidcap?xvidcap-prefs-general &";
        break;
    case 1:
        cmd = "yelp ghelp:xvidcap?xvidcap-prefs-sf &";
        break;
    case 2:
        cmd = "yelp ghelp:xvidcap?xvidcap-prefs-cmd &";
        break;
    }
#endif     // HAVE_LIBAVCODEC

    if (cmd)
        system (cmd);

#undef DEBUGFUNCTION
}

/**
 * \brief fill the mf codec combobox with valid codecs for the given
 *      format
 *
 * @param format the id of the selected format
 */
static void
mf_codec_combo_set_contents_from_format (XVC_FFormatID format)
{
#define DEBUGFUNCTION "mf_codec_combo_set_contents_from_format()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering with format %i\n", DEBUGFILE, DEBUGFUNCTION,
            format);
#endif     // DEBUG

    if (format <= 0)
        return;

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_codec_combobox");

    if (w != NULL) {
        int a;

        GtkListStore *mf_codec_list_store =
            GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (w)));
        // NOTE: for this to work ther must ALWAYS be a dummy entry
        // present in glade otherwise glade will not create a model
        // at all (let alone a list store)
        g_assert (mf_codec_list_store);
        gtk_list_store_clear (GTK_LIST_STORE (mf_codec_list_store));

        for (a = 0; a < xvc_formats[format].num_allowed_vid_codecs; a++) {

#ifdef DEBUG
            printf
                ("%s %s: Adding this text (item %i) to mf codec combobox %s\n",
                 DEBUGFILE, DEBUGFUNCTION, a,
                 _(xvc_codecs[xvc_formats[format].allowed_vid_codecs[a]].
                   longname));
#endif     // DEBUG
            // FIXME: do I need to do the utf8 conversion?
            gtk_combo_box_append_text (GTK_COMBO_BOX (w),
                                       strdup (_
                                               (xvc_codecs
                                                [xvc_formats[format].
                                                 allowed_vid_codecs[a]].
                                                longname)));
        }
    }
#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

#ifdef HAVE_FFMPEG_AUDIO
/**
 * \brief fill the mf audio codec combobox with valid audio codecs for the
 *      given format
 *
 * @param format the id of the selected format
 */
static void
mf_audio_codec_combo_set_contents_from_format (XVC_FFormatID format)
{
#define DEBUGFUNCTION "mf_audio_codec_combo_set_contents_from_format()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering with format %i\n", DEBUGFILE, DEBUGFUNCTION,
            format);
#endif     // DEBUG

    if (format <= 0)
        return;

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_codec_combobox");

    if (w != NULL) {
        int a;

        GtkListStore *mf_au_codec_list_store =
            GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (w)));
        // NOTE: for this to work ther must ALWAYS be a dummy entry
        // present in glade otherwise glade will not create a model
        // at all (let alone a list store)
        g_assert (mf_au_codec_list_store);
        gtk_list_store_clear (GTK_LIST_STORE (mf_au_codec_list_store));

        for (a = 0; a < xvc_formats[format].num_allowed_au_codecs; a++) {

#ifdef DEBUG
            printf
                ("%s %s: Adding this text (item %i) to mf codec combobox %s\n",
                 DEBUGFILE, DEBUGFUNCTION, a,
                 _(xvc_audio_codecs[xvc_formats[format].allowed_au_codecs[a]].
                   name));
#endif     // DEBUG
            // FIXME: do I need to do the utf8 conversion?
            gtk_combo_box_append_text (GTK_COMBO_BOX (w),
                                       strdup (_
                                               (xvc_audio_codecs
                                                [xvc_formats[format].
                                                 allowed_au_codecs[a]].name)));
        }
    }
#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}
#endif     // HAVE_FFMPEG_AUDIO

/**
 * \brief creates the preferences dialog and sets the widgets according to
 *      current preferences
 *
 * Callbacks are connected at the end
 */
void
xvc_create_pref_dialog (XVC_AppData * lapp)
{
#define DEBUGFUNCTION "xvc_create_pref_dialog()"

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

    XVC_FFormatID sf_t_format = CAP_NONE;
    XVC_CodecID sf_t_codec = CODEC_NONE;

#ifdef USE_FFMPEG
    XVC_FFormatID mf_t_format = CAP_NONE;
    XVC_CodecID mf_t_codec = CODEC_NONE;

#ifdef HAVE_FFMPEG_AUDIO
    XVC_AuCodecID mf_t_au_codec = AU_CODEC_NONE;
    XVC_AuCodecID sf_t_au_codec = AU_CODEC_NONE;
#endif     // HAVE_FFMPEG_AUDIO
#endif     // USE_FFMPEG

    // load the interface
    xml = glade_xml_new (XVC_GLADE_FILE, "xvc_pref_main_window", NULL);
    g_assert (xml);

    xvc_pref_main_window = glade_xml_get_widget (xml, "xvc_pref_main_window");
    g_assert (xvc_pref_main_window);

    // first copy the pass parameters to a temporary structure
    xvc_appdata_copy (&pref_app, lapp);

    sf_t_format = pref_app.single_frame.target;
    if (!sf_t_format)
        sf_t_format =
            xvc_codec_get_target_from_filename (pref_app.single_frame.file);
    sf_t_codec = pref_app.single_frame.targetCodec;
    if (!sf_t_codec)
        sf_t_codec = xvc_formats[sf_t_format].def_vid_codec;
#ifdef HAVE_FFMPEG_AUDIO
    sf_t_au_codec = pref_app.single_frame.au_targetCodec;
    if (!sf_t_au_codec)
        sf_t_au_codec = xvc_formats[sf_t_format].def_au_codec;
#endif     // HAVE_FFMPEG_AUDIO

#ifdef USE_FFMPEG
    mf_t_format = pref_app.multi_frame.target;
    if (!mf_t_format)
        mf_t_format =
            xvc_codec_get_target_from_filename (pref_app.multi_frame.file);
    mf_t_codec = pref_app.multi_frame.targetCodec;
    if (!mf_t_codec)
        mf_t_codec = xvc_formats[mf_t_format].def_vid_codec;
#ifdef HAVE_FFMPEG_AUDIO
    mf_t_au_codec = pref_app.multi_frame.au_targetCodec;
    if (!mf_t_au_codec)
        mf_t_au_codec = xvc_formats[mf_t_format].def_au_codec;
#endif     // HAVE_FFMPEG_AUDIO
#endif     // USE_FFMPEG

#ifdef DEBUG
    printf
        ("%s %s: Determined Formats sf/mf %i/%i and Codecs sf/mf %i/%i Audio Codecs sf/mf %i/%i\n",
         DEBUGFILE, DEBUGFUNCTION, sf_t_format,
#ifdef USE_FFMPEG
         mf_t_format,
#else
         0,
#endif     // USE_FFMPEG
         sf_t_codec,
#ifdef USE_FFMPEG
         mf_t_codec,
#else
         0,
#endif     // USE_FFMPEG
         sf_t_au_codec
#ifdef HAVE_FFMPEG_AUDIO
         , mf_t_au_codec
#else
         , 0
#endif     // HAVE_FFMPEG_AUDIO
        );
#endif     // DEBUG

    // prepare the arrays with the entries for the dynamic comboboxes
    {
        int a;
        char buf[256];
        gchar *utf8text;
        GError *error = NULL;

        for (a = 1; a < NUMCAPS; a++) {
            sprintf (buf, "%s (%s)", _(xvc_formats[a].longname),
                     xvc_formats[a].extensions[0]);
            utf8text =
                g_convert ((gchar *) buf, strlen (buf), "UTF-8",
                           "ISO-8859-1", NULL, NULL, &error);

            if (error != NULL)
                printf
                    ("%s %s: couldn't convert format name to utf8\n",
                     DEBUGFILE, DEBUGFUNCTION);

            format_combo_entries[a - 1] = strdup ((char *) utf8text);
#ifdef DEBUG
            printf ("%s %s: format_combo_entries %i : %s\n", DEBUGFILE,
                    DEBUGFUNCTION, (a - 1), format_combo_entries[a - 1]);
#endif
        }
    }

#ifdef DEBUG
    printf ("%s %s: Constructed strings for format comboboxes\n", DEBUGFILE,
            DEBUGFUNCTION);
#endif     // DEBUG

    // default capture mode radio buttons
#ifdef USE_FFMPEG
    if (pref_app.default_mode == 0) {
        w = NULL;
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_default_capture_mode_sf_radiobutton");
        if (w != NULL)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
    } else {
        w = NULL;
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_default_capture_mode_mf_radiobutton");
        if (w != NULL)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
    }
#else      // USE_FFMPEG
    w = NULL;
    w = glade_xml_get_widget (xml,
                              "xvc_pref_default_capture_mode_sf_radiobutton");
    if (w != NULL) {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);
    }

    w = NULL;
    w = glade_xml_get_widget (xml,
                              "xvc_pref_default_capture_mode_mf_radiobutton");
    if (w != NULL)
        gtk_widget_set_sensitive (GTK_WIDGET (w), FALSE);
#endif     // USE_FFMPEG

    // mouse wanted radio buttons
    if (!(pref_app.flags & FLG_USE_XFIXES)) {
        w = NULL;
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_capture_mouse_white_radiobutton");

        if (pref_app.mouseWanted == 1) {
            if (w != NULL)
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
        } else if (pref_app.mouseWanted == 2) {
            if (w != NULL)
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
        }
    } else {
        w = NULL;
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_capture_mouse_white_radiobutton");
        if (w != NULL)
            gtk_widget_hide (w);
        w = NULL;
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_capture_mouse_black_radiobutton");
        if (w != NULL)
            gtk_widget_hide (w);
    }
    if (pref_app.mouseWanted == 0) {
        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_capture_mouse_checkbutton");
        if (w != NULL)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);

        w = NULL;
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_capture_mouse_black_radiobutton");
        if (w != NULL)
            gtk_widget_set_sensitive (w, FALSE);
        w = NULL;
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_capture_mouse_white_radiobutton");
        if (w != NULL)
            gtk_widget_set_sensitive (w, FALSE);
    } else {
        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_capture_mouse_checkbutton");
        if (w != NULL)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
    }

    // save geometry
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_save_geometry_checkbutton");
    if (w != NULL) {
        if ((pref_app.flags & FLG_SAVE_GEOMETRY) != 0) {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
        } else {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
        }
    }
    // capture area follows mouse pointer
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_follow_mouse_checkbutton");
    if (w != NULL) {
        if ((pref_app.flags & FLG_LOCK_FOLLOWS_MOUSE) != 0) {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
        } else {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
        }
    }
    // hide frame around capture area
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_hide_frame_checkbutton");
    if (w != NULL) {
        if ((pref_app.flags & FLG_NOFRAME) != 0) {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
        } else {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
        }
    }
    // shared memory
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_shared_mem_checkbutton");
#ifdef HAVE_SHMAT
    if ((pref_app.flags & FLG_USE_SHM) != 0) {
        if (w != NULL)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
    } else {
        if (w != NULL)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
    }
#else      // HAVE_SHMAT
    if (w != NULL) {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
        gtk_widget_set_sensitive (w, FALSE);
    }
#endif     // HAVE_SHMAT

    // XDamage use
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_use_xdamage_auto_checkbutton");
#ifdef USE_XDAMAGE
    if (pref_app.use_xdamage == -1) {
        if (w != NULL)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_use_xdamage_yes_radiobutton");
        if (w != NULL)
            gtk_widget_set_sensitive (w, FALSE);
        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_use_xdamage_no_radiobutton");
        if (w != NULL)
            gtk_widget_set_sensitive (w, FALSE);
    } else {
        if (w != NULL) {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
        }
        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_use_xdamage_no_radiobutton");

        if (pref_app.use_xdamage == 0) {
            if (w != NULL)
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
        } else {
            if (w != NULL)
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
        }
    }
# else     // USE_XDAMAGE
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_use_xdamage_auto_checkbutton");
    if (w != NULL)
        gtk_widget_hide (w);

    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_use_xdamage_yes_radiobutton");
    if (w != NULL)
        gtk_widget_hide (w);

    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_use_xdamage_no_radiobutton");
    if (w != NULL)
        gtk_widget_hide (w);
#endif     // USE_XDAMAGE

    // autocontinue
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_autocontinue_checkbutton");
    if ((pref_app.flags & FLG_AUTO_CONTINUE) != 0) {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
    } else {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
    }

    // always show result dialog
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_always_show_results_checkbutton");
    if ((pref_app.flags & FLG_ALWAYS_SHOW_RESULTS) != 0) {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
    } else {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
    }

    // minimize to system tray
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_minimize_to_tray");
    if ((pref_app.flags & FLG_TO_TRAY) != 0) {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
    } else {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
    }

    // rescale
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_rescale_hscale");
    if (w != NULL)
        gtk_range_set_value (GTK_RANGE (w), pref_app.rescale);

#ifdef DEBUG
    printf ("%s %s: Set widgets for general tab\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    //
    // single frame

    // file
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_sf_filename_entry");

    if (w != NULL) {
        gtk_entry_set_text (GTK_ENTRY (w), strdup (pref_app.single_frame.file));
    }
    // format
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_sf_format_combobox");

    if (w != NULL) {
        int a;
        gboolean auto_is_active;
        GtkWidget *check = NULL;
        GtkListStore *sf_format_list_store =
            GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (w)));
        // NOTE: for this to work ther must ALWAYS be a dummy entry
        // present in glade otherwise glade will not create a model at all
        // (let alone a list store) ... on the other hand, this requires
        // deleting the list first
        g_assert (sf_format_list_store);
        gtk_list_store_clear (GTK_LIST_STORE (sf_format_list_store));

#ifdef USE_FFMPEG
        for (a = 0; a < (CAP_MF - 1); a++)
#else
        for (a = 0; a < (NUMCAPS - 1); a++)
#endif     // USE_FFMPEG
        {
#ifdef DEBUG
            printf
                ("%s %s: Adding this text (item %i) to sf format combobox: %s\n",
                 DEBUGFILE, DEBUGFUNCTION, a, format_combo_entries[a]);
#endif     // DEBUG
            gtk_combo_box_append_text (GTK_COMBO_BOX (w),
                                       format_combo_entries[a]);
        }

        auto_is_active = (pref_app.single_frame.target == 0);
        check =
            glade_xml_get_widget (xml, "xvc_pref_sf_format_auto_checkbutton");
        if (check != NULL) {
            int found = (auto_is_active ?
                         xvc_codec_get_target_from_filename (pref_app.
                                                             single_frame.
                                                             file) : pref_app.
                         single_frame.target) - 1;
            // if we specified an invalid target on the command line
            if (found > a)
                found =
                    xvc_codec_get_target_from_filename (pref_app.single_frame.
                                                        file);

            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                                          auto_is_active);
            gtk_widget_set_sensitive (w, !auto_is_active);
            gtk_combo_box_set_active (GTK_COMBO_BOX (w), found);
        }
    }
    // fps
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_sf_fps_hscale");
    if (w != NULL)
        gtk_range_set_value (GTK_RANGE (w),
                             ((float) pref_app.single_frame.fps.num /
                              (float) pref_app.single_frame.fps.den));

    // quality
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_sf_quality_hscale");
    if (w != NULL)
        gtk_range_set_value (GTK_RANGE (w), pref_app.single_frame.quality);

    // max time
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_sf_max_time_spinbutton");
    if (w != NULL)
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (w),
                                   pref_app.single_frame.time);

    // max frames
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_sf_max_frames_spinbutton");
    if (w != NULL)
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (w),
                                   pref_app.single_frame.frames);

    // start number
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_sf_start_no_spinbutton");
    if (w != NULL)
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (w),
                                   pref_app.single_frame.start_no);

    // frame increment
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_sf_frame_increment_spinbutton");
    if (w != NULL)
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (w),
                                   pref_app.single_frame.step);

    //
    // multi_frame

#ifdef USE_FFMPEG
    // file
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_filename_entry");

    if (w != NULL) {
        gtk_entry_set_text (GTK_ENTRY (w), strdup (pref_app.multi_frame.file));
    }
    // format
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_format_combobox");

    if (w != NULL) {
        int a;
        gboolean auto_is_active = FALSE;
        GtkWidget *check = NULL;
        GtkListStore *mf_format_list_store =
            GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (w)));
        // NOTE: for this to work ther must ALWAYS be a dummy entry
        // present in glade otherwise glade will not create a model at all
        // (let alone a list store) ... on the other hand, this requires
        // deleting the list first
        g_assert (mf_format_list_store);
        gtk_list_store_clear (GTK_LIST_STORE (mf_format_list_store));

        for (a = (CAP_MF - 1); a < (NUMCAPS - 1); a++) {
#ifdef DEBUG
            printf
                ("%s %s: Adding this text (item %i) to mf format combobox %s\n",
                 DEBUGFILE, DEBUGFUNCTION, a, format_combo_entries[a]);
#endif     // DEBUG
            gtk_combo_box_append_text (GTK_COMBO_BOX (w),
                                       format_combo_entries[a]);
        }

        auto_is_active = (pref_app.multi_frame.target == 0);
        check =
            glade_xml_get_widget (xml, "xvc_pref_mf_format_auto_checkbutton");
        if (check != NULL) {
            int found = (auto_is_active ?
                         xvc_codec_get_target_from_filename (pref_app.
                                                             multi_frame.
                                                             file) : pref_app.
                         multi_frame.target) - CAP_MF;
            // if we specified an invalid target on the command line
            if (found > a)
                found =
                    xvc_codec_get_target_from_filename (pref_app.multi_frame.
                                                        file) - CAP_MF;

            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                                          auto_is_active);
            gtk_widget_set_sensitive (w, !auto_is_active);
            gtk_combo_box_set_active (GTK_COMBO_BOX (w), found);
        }
    }
    // codec
    mf_codec_combo_set_contents_from_format (mf_t_format);
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_codec_auto_checkbutton");
    if (w != NULL) {
        GtkWidget *codec_combob = NULL;
        gboolean auto_is_active = (pref_app.multi_frame.targetCodec == 0);

        codec_combob = glade_xml_get_widget (xml, "xvc_pref_mf_codec_combobox");

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), auto_is_active);
        if (codec_combob != NULL) {
            gtk_widget_set_sensitive (codec_combob, !auto_is_active);
            int found =
                get_index_of_array_element (xvc_formats[mf_t_format].
                                            num_allowed_vid_codecs,
                                            (int *) xvc_formats[mf_t_format].
                                            allowed_vid_codecs,
                                            (auto_is_active
/*                                             || mf_t_codec >=
                                             xvc_formats[mf_t_format].
                                             num_allowed_vid_codecs */
                                             ?
                                             xvc_formats[mf_t_format].
                                             def_vid_codec : mf_t_codec));
            if (found >= 0)
                gtk_combo_box_set_active (GTK_COMBO_BOX (codec_combob), found);
        }
    }
    // fps
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_fps_hscale");
    if (w != NULL)
        gtk_range_set_value (GTK_RANGE (w),
                             ((float) pref_app.multi_frame.fps.num /
                              (float) pref_app.multi_frame.fps.den));

    // max time
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_max_time_spinbutton");
    if (w != NULL)
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (w),
                                   pref_app.multi_frame.time);

    // max frames
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_max_frames_spinbutton");
    if (w != NULL)
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (w),
                                   pref_app.multi_frame.frames);

    // quality
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_quality_hscale");
    if (w != NULL)
        gtk_range_set_value (GTK_RANGE (w), pref_app.multi_frame.quality);

#ifdef HAVE_FFMPEG_AUDIO
    // audio check
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_checkbutton");
    if (w != NULL) {
        if (pref_app.multi_frame.audioWanted > 0)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
        else
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
    }
    // audio codec
    mf_audio_codec_combo_set_contents_from_format (mf_t_format);
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_codec_auto_checkbutton");
    if (w != NULL) {
        GtkWidget *au_codec_combob = NULL;
        int found = -1;
        gboolean auto_is_active =
            (pref_app.multi_frame.au_targetCodec == 0 ? 1 : 0);

        au_codec_combob =
            glade_xml_get_widget (xml, "xvc_pref_mf_audio_codec_combobox");

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), auto_is_active);
        if (au_codec_combob != NULL) {
            gtk_widget_set_sensitive (au_codec_combob, !auto_is_active);
            found =
                get_index_of_array_element (xvc_formats[mf_t_format].
                                            num_allowed_au_codecs,
                                            (int *) xvc_formats[mf_t_format].
                                            allowed_au_codecs, (auto_is_active
/*                                                                || mf_t_au_codec
                                                                >=
                                                                xvc_formats
                                                                [mf_t_format].
                                                                num_allowed_au_codecs */
                                                                ?
                                                                xvc_formats
                                                                [mf_t_format].
                                                                def_au_codec :
                                                                mf_t_au_codec));

            if (found >= 0)
                gtk_combo_box_set_active (GTK_COMBO_BOX (au_codec_combob),
                                          found);
        }
    }
    // audio sample rate
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_sample_rate_spinbutton");
    if (w != NULL)
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (w),
                                   pref_app.multi_frame.sndrate);

    // audio bit rate
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_bit_rate_spinbutton");
    if (w != NULL)
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (w),
                                   pref_app.multi_frame.sndsize);

    // audio capture device
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_input_device_entry");

    if (w != NULL) {
        gtk_entry_set_text (GTK_ENTRY (w), strdup (pref_app.snddev));
    }
    // audio channels
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_channels_hscale");
    if (w != NULL)
        gtk_range_set_value (GTK_RANGE (w), pref_app.multi_frame.sndchannels);
#else      // HAVE_FFMPEG_AUDIO
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_nb_tab_multi_frame_audio_frame");

    if (w != NULL) {
        gint f, e;
        guint p;
        GtkPackType pt;
        GtkWidget *container = NULL;

        gtk_widget_hide (GTK_WIDGET (w));

        container =
            glade_xml_get_widget (xml, "xvc_pref_nb_tab_multi_frame_vbox");
        if (container != NULL)
            gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET (w));

        w = NULL;
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_nb_tab_multi_frame_video_frame");
        g_assert (w);

        if (container != NULL && w != NULL) {
            gtk_box_query_child_packing (GTK_BOX (container),
                                         GTK_WIDGET (w), &e, &f, &p, &pt);
            gtk_box_set_child_packing (GTK_BOX (container),
                                       GTK_WIDGET (w), (gboolean) TRUE,
                                       (gboolean) TRUE, p, pt);
        }
    }
#endif     // HAVE_FFMPEG_AUDIO

#else      // no USE_FFMPEG
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_nb_tab_multi_frame_vbox");
    g_assert (w);
    gtk_widget_hide (GTK_WIDGET (w));

    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_nb_tab_single_frame_label");
    g_assert (w);
    gtk_label_set_markup_with_mnemonic (GTK_LABEL (w), _("C_apture"));
#endif     // USE_FFMPEG

    // sf play command
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_commands_sf_play_entry");

    if (w != NULL) {
        gtk_entry_set_text (GTK_ENTRY (w),
                            strdup (pref_app.single_frame.play_cmd));
    }
    // sf encoding command
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_commands_sf_encode_entry");

    if (w != NULL) {
        gtk_entry_set_text (GTK_ENTRY (w),
                            strdup (pref_app.single_frame.video_cmd));
    }
    // sf edit command
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_commands_sf_edit_entry");

    if (w != NULL) {
        gtk_entry_set_text (GTK_ENTRY (w),
                            strdup (pref_app.single_frame.edit_cmd));
    }
#ifdef USE_FFMPEG
    // mf play command
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_commands_mf_play_entry");

    if (w != NULL) {
        gtk_entry_set_text (GTK_ENTRY (w),
                            strdup (pref_app.multi_frame.play_cmd));
    }
    // mf encoding command
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_commands_mf_encode_entry");

    if (w != NULL) {
        gtk_entry_set_text (GTK_ENTRY (w),
                            strdup (pref_app.multi_frame.video_cmd));
    }
    // mf edit command
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_commands_mf_edit_entry");

    if (w != NULL) {
        gtk_entry_set_text (GTK_ENTRY (w),
                            strdup (pref_app.multi_frame.edit_cmd));
    }
#else      // USE_FFMPEG
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_nb_tab_commands_mf_frame");
    g_assert (w);
    gtk_widget_hide (GTK_WIDGET (w));
#endif     // USE_FFMPEG

    // connect the signals in the interface
    glade_xml_signal_autoconnect (xml);

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#undef DEBUGFUNCTION
}

/*
 * callbacks
 */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
/**
 * \brief callback for key press events to have shortcuts for displaying
 *      the help pages.
 */
gint
on_xvc_pref_main_window_key_press_event (GtkWidget * widget, GdkEvent * event)
{
#define DEBUGFUNCTION "on_xvc_pref_main_window_key_press_event()"
    GdkEventKey *kevent = NULL;

    g_assert (widget);
    g_assert (event);
    kevent = (GdkEventKey *) event;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    if (kevent->keyval == 65470) {
        doHelp ();
    }
#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    // Tell calling code that we have not handled this event; pass it on.
    return FALSE;
#undef DEBUGFUNCTION
}

void
on_xvc_pref_main_window_response (GtkDialog * dialog, gint response_id,
                                  gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_pref_main_window_response()"

    Job *job;

    // printf("response id : %i \n", response_id);
    switch (response_id) {
    case GTK_RESPONSE_OK:
#ifdef DEBUG
        printf ("%s %s: Entering with:\n", DEBUGFILE, DEBUGFUNCTION);
        printf
            ("filename: %s - target: %i - targetCodec: %i - audioCodec: %i \n",
             pref_app.multi_frame.file, pref_app.multi_frame.target,
             pref_app.multi_frame.targetCodec,
#ifdef HAVE_FFMPEG_AUDIO
             pref_app.multi_frame.au_targetCodec
#else
             0
#endif     // HAVE_FFMPEG_AUDIO
            );
#endif     // DEBUG

        // need to read pref_data from gui
        read_app_data_from_pref_gui (&pref_app);

#ifdef DEBUG
        printf ("%s %s: Leaving with:\n", DEBUGFILE, DEBUGFUNCTION);
        printf
            ("filename: %s - target: %i - targetCodec: %i - audioCodec: %i \n",
             pref_app.multi_frame.file, pref_app.multi_frame.target,
             pref_app.multi_frame.targetCodec,
#ifdef HAVE_FFMPEG_AUDIO
             pref_app.multi_frame.au_targetCodec
#else
             0
#endif     // HAVE_FFMPEG_AUDIO
            );
#endif     // DEBUG

        xvc_pref_do_OK ();

        job = xvc_job_ptr ();
        if (job)
            xvc_change_filename_display (job->pic_no);
        break;

    case GTK_RESPONSE_CANCEL:
        gtk_widget_destroy (xvc_pref_main_window);
        break;

    case GTK_RESPONSE_HELP:
        doHelp ();

    default:
        break;
    }

#undef DEBUGFUNCTION
}

gboolean
on_xvc_pref_main_window_delete_event (GtkWidget * widget, GdkEvent * event,
                                      gpointer user_data)
{
    GladeXML *xml = NULL;
    GtkWidget *cancel_button = NULL;

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);
    cancel_button = glade_xml_get_widget (xml, "xvc_pref_cancel_button");
    g_assert (cancel_button);

    if (GTK_WIDGET_IS_SENSITIVE (GTK_WIDGET (cancel_button)))
        return FALSE;
    else
        return TRUE;
}

void
on_xvc_pref_main_window_close (GtkDialog * dialog, gpointer user_data)
{
    // empty as yet ...
}

void
on_xvc_pref_main_window_destroy (GtkButton * button, gpointer user_data)
{
    // empty as yet ...
}

void
on_xvc_pref_capture_mouse_checkbutton_toggled (GtkToggleButton *
                                               togglebutton, gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_pref_capture_mouse_checkbutton_toggled()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);

    if (gtk_toggle_button_get_active (togglebutton)) {
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_capture_mouse_black_radiobutton");
        g_assert (w);
        gtk_widget_set_sensitive (w, TRUE);
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_capture_mouse_white_radiobutton");
        g_assert (w);
        gtk_widget_set_sensitive (w, TRUE);
    } else {
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_capture_mouse_black_radiobutton");
        g_assert (w);
        gtk_widget_set_sensitive (w, FALSE);
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_capture_mouse_white_radiobutton");
        g_assert (w);
        gtk_widget_set_sensitive (w, FALSE);
    }

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

void
on_xvc_pref_use_xdamage_auto_checkbutton_toggled (GtkToggleButton *
                                                  togglebutton,
                                                  gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_pref_use_xdamage_auto_checkbutton_toggled()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);

    if (gtk_toggle_button_get_active (togglebutton)) {
        w = glade_xml_get_widget (xml, "xvc_pref_use_xdamage_no_radiobutton");
        g_assert (w);
        gtk_widget_set_sensitive (w, TRUE);
        w = glade_xml_get_widget (xml, "xvc_pref_use_xdamage_yes_radiobutton");
        g_assert (w);
        gtk_widget_set_sensitive (w, TRUE);
    } else {
        w = glade_xml_get_widget (xml, "xvc_pref_use_xdamage_no_radiobutton");
        g_assert (w);
        gtk_widget_set_sensitive (w, FALSE);
        w = glade_xml_get_widget (xml, "xvc_pref_use_xdamage_yes_radiobutton");
        g_assert (w);
        gtk_widget_set_sensitive (w, FALSE);
    }

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

/**
 * \brief callback sets the file format selected on change of filename
 *      if automatic selection is active.
 */
void
on_xvc_pref_sf_filename_entry_changed (GtkEntry * entry, gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_pref_sf_filename_entry_changed()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    int i;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_pref_sf_format_auto_checkbutton");
    g_assert (w);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
        i = xvc_codec_get_target_from_filename ((char *)
                                                gtk_entry_get_text (GTK_ENTRY
                                                                    (entry)));

#ifdef USE_FFMPEG
        if (i > 0 && i < CAP_MF) {
#else      // USE_FFMPEG
        if (i > 0 && i < NUMCAPS) {
#endif     // USE_FFMPEG
            w = glade_xml_get_widget (xml, "xvc_pref_sf_format_combobox");
            g_assert (w);
            gtk_combo_box_set_active (GTK_COMBO_BOX (w), (i - 1));
        }
    }
#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

void
on_xvc_pref_sf_format_auto_checkbutton_toggled (GtkToggleButton *
                                                togglebutton,
                                                gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_pref_sf_format_auto_checkbutton_toggled()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);

    if (gtk_toggle_button_get_active (togglebutton)) {
        w = glade_xml_get_widget (xml, "xvc_pref_sf_format_combobox");
        g_assert (w);
        gtk_widget_set_sensitive (w, FALSE);
    } else {
        w = glade_xml_get_widget (xml, "xvc_pref_sf_format_combobox");
        g_assert (w);
        gtk_widget_set_sensitive (w, TRUE);
    }

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

/**
 * \brief sets the selected format if autodetection is active and either
 *      a change has been made to the filename or the autodection is just
 *      being turned on and the currently selected format is not the default.
 *
 * In other words: this callback is also hooked up to the changed event of
 * the auto checkbutton
 */
void
on_xvc_pref_mf_filename_entry_changed (GtkEntry * entry, gpointer user_data)
{
#ifdef USE_FFMPEG
#define DEBUGFUNCTION "on_xvc_pref_mf_filename_entry_changed()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    int i;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);
    // we may be calling this after a click on format auto-select rather
    // than a change of filename
    w = glade_xml_get_widget (xml, "xvc_pref_mf_filename_entry");
    g_assert (w);
    if (w != GTK_WIDGET (entry)) {
        entry = GTK_ENTRY (w);
    }
    w = NULL;
    w = glade_xml_get_widget (xml, "xvc_pref_mf_format_auto_checkbutton");
    g_assert (w);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
        i = xvc_codec_get_target_from_filename ((char *)
                                                gtk_entry_get_text (GTK_ENTRY
                                                                    (entry)));

        if (i >= CAP_MF && i < NUMCAPS) {
            w = glade_xml_get_widget (xml, "xvc_pref_mf_format_combobox");
            g_assert (w);
            gtk_combo_box_set_active (GTK_COMBO_BOX (w), (i - CAP_MF));
        }
    }
#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
#endif     // USE_FFMPEG
}

void
on_xvc_pref_mf_format_auto_checkbutton_toggled (GtkToggleButton *
                                                togglebutton,
                                                gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_pref_mf_format_auto_checkbutton_toggled()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_pref_mf_format_combobox");
    g_assert (w);

    if (gtk_toggle_button_get_active (togglebutton)) {
        gtk_widget_set_sensitive (w, FALSE);
    } else {
        gtk_widget_set_sensitive (w, TRUE);
    }

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

void
on_xvc_pref_mf_codec_auto_checkbutton_toggled (GtkToggleButton *
                                               togglebutton, gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_pref_mf_codec_auto_checkbutton_toggled()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_pref_mf_codec_combobox");
    g_assert (w);

    if (gtk_toggle_button_get_active (togglebutton)) {
        gtk_widget_set_sensitive (w, FALSE);
    } else {
        gtk_widget_set_sensitive (w, TRUE);
    }

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

void
on_xvc_pref_mf_audio_codec_auto_checkbutton_toggled (GtkToggleButton *
                                                     togglebutton,
                                                     gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_pref_mf_audio_codec_auto_checkbutton_toggled()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_codec_combobox");
    g_assert (w);

    if (gtk_toggle_button_get_active (togglebutton)) {
        gtk_widget_set_sensitive (w, FALSE);
    } else {
        gtk_widget_set_sensitive (w, TRUE);
    }

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

/**
 * \brief implement the change of a mf format selection and call all the
 *      resulting actions that may be required
 *
 * This resets the valid codecs and audio codecs, keeps old selections if
 * possible or tries to select default values.
 * @note For audio codecs it is actually possible to have a default audio
 *      codec of AU_CODEC_NONE even if audio support is present. Therefore
 *      one must not assume that the comboboxes will always have a valid
 *      selection
 * @note dont't need to check for format_selected != old_selected_format
 *      because that is implicit in GTK
 */
void
on_xvc_pref_mf_format_combobox_changed (GtkComboBox * combobox,
                                        gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_pref_mf_format_combobox_changed()"

#ifdef USE_FFMPEG

    GladeXML *xml = NULL;
    GtkWidget *codec_combobox = NULL, *w = NULL;
    XVC_FFormatID format_selected = CAP_NONE;

#ifdef HAVE_FFMPEG_AUDIO
    GtkWidget *au_combobox = NULL;
#endif     // HAVE_FFMPEG_AUDIO

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_pref_mf_format_combobox");
    g_assert (w);
    if (w != GTK_WIDGET (combobox)) {
#ifdef DEBUG
        printf ("%s %s: change combobox %p %p\n", DEBUGFILE, DEBUGFUNCTION,
                w, combobox);
#endif     // DEBUG
        combobox = GTK_COMBO_BOX (w);
    }

    format_selected = gtk_combo_box_get_active (GTK_COMBO_BOX (combobox)) +
        CAP_MF;
#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
    printf ("%s %s: reading %i '%s' from format combo box ... \n", DEBUGFILE,
            DEBUGFUNCTION, format_selected,
            xvc_formats[format_selected].longname);
#endif     // DEBUG
    g_assert (format_selected > CAP_NONE);

    codec_combobox = glade_xml_get_widget (xml, "xvc_pref_mf_codec_combobox");
    g_assert (codec_combobox);
#ifdef HAVE_FFMPEG_AUDIO
    au_combobox =
        glade_xml_get_widget (xml, "xvc_pref_mf_audio_codec_combobox");
    g_assert (au_combobox);
#endif     // HAVE_FFMPEG_AUDIO

    if (format_selected > 0) {
        XVC_CodecID old_selected_codec = CODEC_NONE;
        GtkWidget *codec_auto = NULL;

#ifdef HAVE_FFMPEG_AUDIO
        XVC_AuCodecID old_selected_aucodec = AU_CODEC_NONE;
        GtkWidget *au_codec_auto = NULL;

        if (xvc_formats[old_selected_format].num_allowed_au_codecs != 0 &&
            gtk_combo_box_get_active (GTK_COMBO_BOX (au_combobox)) >= 0) {
            old_selected_aucodec =
                xvc_formats[old_selected_format].
                allowed_au_codecs[gtk_combo_box_get_active
                                  (GTK_COMBO_BOX (au_combobox))];
        }
#endif     // HAVE_FFMPEG_AUDIO

        if (xvc_formats[old_selected_format].num_allowed_vid_codecs != 0 &&
            gtk_combo_box_get_active (GTK_COMBO_BOX (codec_combobox)))
            old_selected_codec =
                xvc_formats[old_selected_format].
                allowed_vid_codecs[gtk_combo_box_get_active
                                   (GTK_COMBO_BOX (codec_combobox))];

        mf_codec_combo_set_contents_from_format (format_selected);

        codec_auto =
            glade_xml_get_widget (xml, "xvc_pref_mf_codec_auto_checkbutton");
        g_assert (codec_auto);

        if (xvc_formats[format_selected].num_allowed_vid_codecs != 0) {
            int found;

            if (old_selected_codec != 0 &&
                !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (codec_auto)))
            {
                found =
                    get_index_of_array_element (xvc_formats[format_selected].
                                                num_allowed_vid_codecs, (int *)
                                                xvc_formats[format_selected].
                                                allowed_vid_codecs,
                                                old_selected_codec);
                if (found < 0)
                    found =
                        get_index_of_array_element (xvc_formats
                                                    [format_selected].
                                                    num_allowed_vid_codecs,
                                                    (int *)
                                                    xvc_formats
                                                    [format_selected].
                                                    allowed_vid_codecs,
                                                    xvc_formats
                                                    [format_selected].
                                                    def_vid_codec);
            } else {
                found =
                    get_index_of_array_element (xvc_formats[format_selected].
                                                num_allowed_vid_codecs, (int *)
                                                xvc_formats[format_selected].
                                                allowed_vid_codecs,
                                                xvc_formats[format_selected].
                                                def_vid_codec);
                if (found < 0)
                    found = 0;
            }
            if (found >= 0)
                gtk_combo_box_set_active (GTK_COMBO_BOX (codec_combobox),
                                          found);
        }
#ifdef HAVE_FFMPEG_AUDIO
        mf_audio_codec_combo_set_contents_from_format (format_selected);

        au_codec_auto =
            glade_xml_get_widget (xml,
                                  "xvc_pref_mf_audio_codec_auto_checkbutton");
        g_assert (au_codec_auto);

        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_checkbutton");
        g_assert (w);

        if (xvc_formats[format_selected].num_allowed_au_codecs != 0) {
            int found;

            if (old_selected_aucodec != 0 &&
                !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                               (au_codec_auto))) {
                found =
                    get_index_of_array_element (xvc_formats[format_selected].
                                                num_allowed_au_codecs, (int *)
                                                xvc_formats[format_selected].
                                                allowed_au_codecs,
                                                old_selected_aucodec);

                if (found < 0)
                    found =
                        get_index_of_array_element (xvc_formats
                                                    [format_selected].
                                                    num_allowed_au_codecs,
                                                    (int *)
                                                    xvc_formats
                                                    [format_selected].
                                                    allowed_au_codecs,
                                                    xvc_formats
                                                    [format_selected].
                                                    def_au_codec);
            } else {
                found =
                    get_index_of_array_element (xvc_formats[format_selected].
                                                num_allowed_au_codecs, (int *)
                                                xvc_formats[format_selected].
                                                allowed_au_codecs,
                                                xvc_formats[format_selected].
                                                def_au_codec);
            }
            if (found >= 0)
                gtk_combo_box_set_active (GTK_COMBO_BOX (au_combobox), found);
        }
#endif     // HAVE_FFMPEG_AUDIO
    }

    old_selected_format = format_selected;
#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#endif     // USE_FFMPEG

#undef DEBUGFUNCTION
}

void
on_xvc_pref_mf_audio_checkbutton_toggled (GtkToggleButton * togglebutton,
                                          gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_pref_mf_audio_checkbutton_toggled()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);

    if (!(gtk_toggle_button_get_active (togglebutton))) {
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_sample_rate_label");
        g_assert (w);
        gtk_widget_set_sensitive (w, FALSE);

        w = NULL;
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_mf_audio_sample_rate_spinbutton");
        g_assert (w);
        gtk_widget_set_sensitive (w, FALSE);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_bit_rate_label");
        g_assert (w);
        gtk_widget_set_sensitive (w, FALSE);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_bit_rate_spinbutton");
        g_assert (w);
        gtk_widget_set_sensitive (w, FALSE);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_input_device_label");
        g_assert (w);
        gtk_widget_set_sensitive (w, FALSE);

        w = NULL;
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_mf_audio_input_device_select_button");
        g_assert (w);
        gtk_widget_set_sensitive (w, FALSE);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_input_device_entry");
        g_assert (w);
        gtk_widget_set_sensitive (w, FALSE);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_channels_label");
        g_assert (w);
        gtk_widget_set_sensitive (w, FALSE);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_channels_hscale");
        g_assert (w);
        gtk_widget_set_sensitive (w, FALSE);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_codec_label");
        g_assert (w);
        gtk_widget_set_sensitive (w, FALSE);

        w = NULL;
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_mf_audio_codec_auto_checkbutton");
        g_assert (w);
        gtk_widget_set_sensitive (w, FALSE);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_codec_combobox");
        g_assert (w);
        gtk_widget_set_sensitive (w, FALSE);
    } else {
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_sample_rate_label");
        g_assert (w);
        gtk_widget_set_sensitive (w, TRUE);

        w = NULL;
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_mf_audio_sample_rate_spinbutton");
        g_assert (w);
        gtk_widget_set_sensitive (w, TRUE);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_bit_rate_label");
        g_assert (w);
        gtk_widget_set_sensitive (w, TRUE);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_bit_rate_spinbutton");
        g_assert (w);
        gtk_widget_set_sensitive (w, TRUE);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_input_device_label");
        g_assert (w);
        gtk_widget_set_sensitive (w, TRUE);

        w = NULL;
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_mf_audio_input_device_select_button");
        g_assert (w);
        gtk_widget_set_sensitive (w, TRUE);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_input_device_entry");
        g_assert (w);
        gtk_widget_set_sensitive (w, TRUE);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_channels_label");
        g_assert (w);
        gtk_widget_set_sensitive (w, TRUE);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_channels_hscale");
        g_assert (w);
        gtk_widget_set_sensitive (w, TRUE);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_codec_label");
        g_assert (w);
        gtk_widget_set_sensitive (w, TRUE);

        w = NULL;
        w = glade_xml_get_widget (xml,
                                  "xvc_pref_mf_audio_codec_auto_checkbutton");
        g_assert (w);
        gtk_widget_set_sensitive (w, TRUE);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_codec_combobox");
        g_assert (w);
        gtk_widget_set_sensitive (w, TRUE);
    }

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

/**
 * \brief sets the widget to select mf fps values according to the codec
 *      selected.
 *
 * Some codecs allow a range of fps values (like from 1 - 5 fps) while others
 * only support certain fixed values. The first uses a horizontal scale, the
 * second a combobox. In theory there could be codecs which combine the two
 * kinds of specifying valid fps, but to date we have none.<br>
 * Both widgets are actually present all the time with only one visible at
 * any given point in time.
 */
static void
set_mf_fps_widget_from_codec (XVC_AuCodecID codec)
{
#ifdef USE_FFMPEG

#define DEBUGFUNCTION "set_mf_fps_widget_from_codec()"
    GladeXML *xml = NULL;
    GtkWidget *combobox = NULL, *hscale = NULL;
    gboolean combobox_visible = FALSE, hscale_visible = FALSE;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    XVC_Fps curr_fps_val = (XVC_Fps) { 0, 0 };

    if (codec == mf_fps_widget_save_old_codec)
        return;

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);

    combobox = glade_xml_get_widget (xml, "xvc_pref_mf_fps_combobox");
    g_assert (combobox);
    gtk_object_get (GTK_OBJECT (combobox), "visible", &combobox_visible, NULL);

    hscale = glade_xml_get_widget (xml, "xvc_pref_mf_fps_hscale");
    g_assert (hscale);
    gtk_object_get (GTK_OBJECT (hscale), "visible", &hscale_visible, NULL);

    if (combobox_visible) {
        curr_fps_val =
            xvc_codecs[mf_fps_widget_save_old_codec].
            allowed_fps[gtk_combo_box_get_active (GTK_COMBO_BOX (combobox))];
    } else if (hscale_visible) {
        curr_fps_val = (XVC_Fps) {
        gtk_range_get_value (GTK_RANGE (hscale)), 1};
    }
    if (((!combobox_visible) && (!hscale_visible)) ||
        (combobox_visible && hscale_visible)) {
        fprintf (stderr,
                 "%s %s: unable to determine type of mf_fps_widget, please file a bug!\n",
                 DEBUGFILE, DEBUGFUNCTION);
        exit (1);
    }

    if (xvc_codecs[codec].num_allowed_fps != 0 &&
        xvc_codecs[codec].num_allowed_fps_ranges == 0) {
        // we only have a number of non-continuous fps allowed for this
        // codec

        GtkListStore *fps_list_store =
            GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (combobox)));
        // NOTE: for this to work ther must ALWAYS be a dummy entry
        // present in glade otherwise glade will not create a model at all
        // (let alone a list store)
        int active_item = -1, default_item = -1, a = 0;

        g_assert (fps_list_store);
        gtk_list_store_clear (GTK_LIST_STORE (fps_list_store));

        gtk_widget_show (GTK_WIDGET (combobox));
        gtk_widget_hide (GTK_WIDGET (hscale));

        for (a = 0; a < xvc_codecs[codec].num_allowed_fps; a++) {
            // the following is needed for correct handling of locale's
            // decimal point
            char lelement[256];

            sprintf (lelement, "%.2f",
                     ((float) xvc_codecs[codec].allowed_fps[a].num
                      / (float) xvc_codecs[codec].allowed_fps[a].den));
            gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), lelement);

            if (XVC_FPS_EQUAL (xvc_codecs[codec].allowed_fps[a], curr_fps_val))
                active_item = a;
            if (XVC_FPS_EQUAL
                (xvc_codecs[codec].allowed_fps[a], xvc_codecs[codec].def_fps))
                default_item = a;
        }

        if (active_item >= 0)
            gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), active_item);
        else if (default_item >= 0)
            gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), default_item);
        else
            gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);

    } else if (xvc_codecs[codec].num_allowed_fps_ranges != 0 &&
               xvc_codecs[codec].num_allowed_fps_ranges == 1) {
        // we only have exactly one range
        XVC_FpsRange range = xvc_codecs[codec].allowed_fps_ranges[0];
        XVC_Fps start = range.start;
        XVC_Fps end = range.end;

        gtk_widget_show (GTK_WIDGET (hscale));
        gtk_widget_hide (GTK_WIDGET (combobox));

        // check current fps value
        if (((float) curr_fps_val.num / (float) curr_fps_val.den) <
            ((float) start.num / (float) start.den) ||
            ((float) curr_fps_val.num / (float) curr_fps_val.den) >
            ((float) end.num / (float) end.den)) {
            curr_fps_val = xvc_codecs[codec].def_fps;
        }
        // set new start and end
        gtk_range_set_range (GTK_RANGE (hscale),
                             (gdouble) ((float) start.num / (float) start.den),
                             ((float) end.num / (float) end.den));
        gtk_range_set_value (GTK_RANGE (hscale),
                             (gdouble) ((float) curr_fps_val.num /
                                        (float) curr_fps_val.den));
    } else {
        // we have both
        XVC_Fps start = (XVC_Fps) { INT_MAX, 1 }
        , end = (XVC_Fps) {
        0, 1};
        int a;

        for (a = 0; a < xvc_codecs[codec].num_allowed_fps_ranges; a++) {
            XVC_Fps sRange = xvc_codecs[codec].allowed_fps_ranges[a].start;
            XVC_Fps eRange = xvc_codecs[codec].allowed_fps_ranges[a].end;

            if (((float) start.num / (float) start.den) >
                ((float) sRange.num / (float) sRange.den))
                start = sRange;
            if (((float) end.num / (float) end.den) <
                ((float) eRange.num / (float) eRange.den))
                end = eRange;
        }

        for (a = 0; a < xvc_codecs[codec].num_allowed_fps; a++) {
            XVC_Fps fixed_start = xvc_codecs[codec].allowed_fps[0];
            XVC_Fps fixed_end =
                xvc_codecs[codec].allowed_fps[xvc_codecs[codec].
                                              num_allowed_fps - 1];
            if (((float) start.num / (float) start.den) >
                ((float) fixed_start.num / (float) fixed_start.den))
                start = fixed_start;
            if (((float) end.num / (float) end.den) <
                ((float) fixed_end.num / (float) fixed_end.den))
                end = fixed_end;
        }

        gtk_widget_show (GTK_WIDGET (hscale));
        gtk_widget_hide (GTK_WIDGET (combobox));

        // check current fps value
        if (((float) curr_fps_val.num / (float) curr_fps_val.den) <
            ((float) start.num / (float) start.den) ||
            ((float) curr_fps_val.num / (float) curr_fps_val.den) >
            ((float) end.num / (float) end.den)) {
            curr_fps_val = xvc_codecs[codec].def_fps;
        }
        // set new start and end
        gtk_range_set_range (GTK_RANGE (hscale),
                             (gdouble) ((float) start.num / (float) start.den),
                             (gdouble) ((float) end.num / (float) end.den));
        gtk_range_set_value (GTK_RANGE (hscale), (gdouble)
                             ((float) curr_fps_val.num /
                              (float) curr_fps_val.den));

    }

    mf_fps_widget_save_old_codec = codec;

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#undef DEBUGFUNCTION
#endif     // USE_FFMPEG
}

void
on_xvc_pref_mf_codec_combobox_changed (GtkComboBox * cb, gpointer user_data)
{
#ifdef USE_FFMPEG
#define DEBUGFUNCTION "on_xvc_pref_mf_codec_combobox_changed()"

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    XVC_CodecID codec = CODEC_NONE;
    XVC_FFormatID format_selected;
    int a = gtk_combo_box_get_active (cb);
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_pref_mf_format_combobox");
    g_assert (w);

    format_selected = gtk_combo_box_get_active (GTK_COMBO_BOX (w)) + CAP_MF;
    if (xvc_formats[format_selected].num_allowed_vid_codecs != 0 && a >= 0)
        codec = xvc_formats[format_selected].allowed_vid_codecs[a];

    if (codec > 0)
        set_mf_fps_widget_from_codec (codec);

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
#endif     // USE_FFMPEG
}

void
on_xvc_pref_commands_help_try_button_clicked (GtkButton * button,
                                              gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_pref_commands_help_try_button_clicked()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_pref_commands_help_entry");
    g_assert (w);

    char *curr_help_cmd = (char *) gtk_entry_get_text (GTK_ENTRY (w));

    if (curr_help_cmd != NULL)
        system ((char *) curr_help_cmd);

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

void
on_xvc_pref_commands_sf_play_try_button_clicked (GtkButton * button,
                                                 gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_pref_commands_sf_play_try_button_clicked()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_pref_commands_sf_play_entry");
    g_assert (w);

    char *curr_play_cmd = (char *) gtk_entry_get_text (GTK_ENTRY (w));

    xvc_command_execute (curr_play_cmd, 1, 0,
                         pref_app.single_frame.file,
                         pref_app.single_frame.start_no,
                         (pref_app.single_frame.start_no + 1),
                         pref_app.area->width, pref_app.area->height,
                         pref_app.single_frame.fps);

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

void
on_xvc_pref_commands_sf_encode_try_button_clicked (GtkButton * button,
                                                   gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_pref_commands_sf_encode_try_button_clicked()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_pref_commands_sf_encode_entry");
    g_assert (w);

    char *curr_encode_cmd = (char *) gtk_entry_get_text (GTK_ENTRY (w));

    xvc_command_execute (curr_encode_cmd, 0, 0,
                         pref_app.single_frame.file,
                         pref_app.single_frame.start_no,
                         (pref_app.single_frame.start_no + 1),
                         pref_app.area->width, pref_app.area->height,
                         pref_app.single_frame.fps);

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

void
on_xvc_pref_commands_sf_edit_try_button_clicked (GtkButton * button,
                                                 gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_pref_commands_sf_edit_try_button_clicked()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_pref_commands_sf_edit_entry");
    g_assert (w);

    char *curr_edit_cmd = (char *) gtk_entry_get_text (GTK_ENTRY (w));

    xvc_command_execute (curr_edit_cmd, 2,
                         pref_app.single_frame.start_no,
                         pref_app.single_frame.file,
                         pref_app.single_frame.start_no,
                         (pref_app.single_frame.start_no + 1),
                         pref_app.area->width, pref_app.area->height,
                         pref_app.single_frame.fps);

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

void
on_xvc_pref_commands_mf_play_try_button_clicked (GtkButton * button,
                                                 gpointer user_data)
{
#ifdef USE_FFMPEG
#define DEBUGFUNCTION "on_xvc_pref_commands_mf_play_try_button_clicked()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_pref_commands_mf_play_entry");
    g_assert (w);

    char *curr_play_cmd = (char *) gtk_entry_get_text (GTK_ENTRY (w));

    xvc_command_execute (curr_play_cmd, 2, 0,
                         pref_app.multi_frame.file,
                         pref_app.multi_frame.start_no,
                         (pref_app.multi_frame.start_no + 1),
                         pref_app.area->width, pref_app.area->height,
                         pref_app.multi_frame.fps);

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
#endif     // USE_FFMPEG
}

void
on_xvc_pref_commands_mf_encode_try_button_clicked (GtkButton * button,
                                                   gpointer user_data)
{
#ifdef USE_FFMPEG
#define DEBUGFUNCTION "on_xvc_pref_commands_mf_encode_try_button_clicked()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_pref_commands_mf_encode_entry");
    g_assert (w);

    char *curr_encode_cmd = (char *) gtk_entry_get_text (GTK_ENTRY (w));

    xvc_command_execute (curr_encode_cmd, 2, 0,
                         pref_app.multi_frame.file,
                         pref_app.multi_frame.start_no,
                         (pref_app.multi_frame.start_no + 1),
                         pref_app.area->width, pref_app.area->height,
                         pref_app.multi_frame.fps);

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
#endif     // USE_FFMPEG
}

void
on_xvc_pref_commands_mf_edit_try_button_clicked (GtkButton * button,
                                                 gpointer user_data)
{
#ifdef USE_FFMPEG
#define DEBUGFUNCTION "on_xvc_pref_commands_mf_edit_try_button_clicked()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
    g_assert (xml);
    w = glade_xml_get_widget (xml, "xvc_pref_commands_mf_edit_entry");
    g_assert (w);

    char *curr_edit_cmd = (char *) gtk_entry_get_text (GTK_ENTRY (w));

    xvc_command_execute (curr_edit_cmd, 2, 0,
                         pref_app.multi_frame.file,
                         pref_app.multi_frame.start_no,
                         (pref_app.multi_frame.start_no + 1),
                         pref_app.area->width, pref_app.area->height,
                         pref_app.multi_frame.fps);

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
#endif     // USE_FFMPEG
}

void
on_xvc_pref_sf_filename_select_button_clicked (GtkButton * button,
                                               gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_pref_sf_filename_select_button_clicked()"
    int result = 0;
    char *got_file_name = NULL;

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
    gtk_window_set_title (GTK_WINDOW (dialog), "Save Frames As:");

    result = gtk_dialog_run (GTK_DIALOG (dialog));

    if (result == GTK_RESPONSE_OK) {
        got_file_name =
            gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

        xml = NULL;
        xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
        g_assert (xml);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_sf_filename_entry");
        g_assert (w);

        gtk_entry_set_text (GTK_ENTRY (w), strdup (got_file_name));
    }

    gtk_widget_destroy (dialog);

#ifdef DEBUG
    printf ("%s %s: Leaving with filename %s\n", DEBUGFILE, DEBUGFUNCTION,
            got_file_name);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

void
on_xvc_pref_mf_filename_select_button_clicked (GtkButton * button,
                                               gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_pref_mf_filename_select_button_clicked()"
    int result = 0;
    char *got_file_name = NULL;

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
        got_file_name =
            gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

        xml = NULL;
        xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
        g_assert (xml);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_filename_entry");
        g_assert (w);

        gtk_entry_set_text (GTK_ENTRY (w), strdup (got_file_name));
    }

    gtk_widget_destroy (dialog);

#ifdef DEBUG
    printf ("%s %s: Leaving with filename %s\n", DEBUGFILE, DEBUGFUNCTION,
            got_file_name);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

void
on_xvc_pref_mf_audio_input_device_select_button_clicked (GtkButton * button,
                                                         gpointer user_data)
{
#define DEBUGFUNCTION "on_xvc_pref_mf_audio_input_device_select_button_clicked()"
    int result = 0;
    char *got_file_name = NULL;

    GladeXML *xml = NULL;
    GtkWidget *w = NULL, *dialog = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    // load the interface
    xml = glade_xml_new (XVC_GLADE_FILE, "xvc_open_filechooserdialog", NULL);
    g_assert (xml);
    // connect the signals in the interface
    glade_xml_signal_autoconnect (xml);

    dialog = glade_xml_get_widget (xml, "xvc_open_filechooserdialog");
    g_assert (dialog);
    gtk_window_set_title (GTK_WINDOW (dialog), "Capture Audio From:");

    result = gtk_dialog_run (GTK_DIALOG (dialog));

    if (result == GTK_RESPONSE_OK) {
        got_file_name =
            gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

        xml = NULL;
        xml = glade_get_widget_tree (GTK_WIDGET (xvc_pref_main_window));
        g_assert (xml);

        w = NULL;
        w = glade_xml_get_widget (xml, "xvc_pref_mf_audio_input_device_entry");
        g_assert (w);

        gtk_entry_set_text (GTK_ENTRY (w), strdup (got_file_name));
    }

    gtk_widget_destroy (dialog);

#ifdef DEBUG
    printf ("%s %s: Leaving with filename %s\n", DEBUGFILE, DEBUGFUNCTION,
            got_file_name);
#endif     // DEBUG
#undef DEBUGFUNCTION
}
#endif     // DOXYGEN_SHOULD_SKIP_THIS
