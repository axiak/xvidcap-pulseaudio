/* 
 * gnome_options.c
 *
 * this file contains the options dialog for the GTK2 control
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Intrinsic.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <bonobo.h>
#include <gnome.h>
#include <glade/glade.h>

#include "job.h"
#include "app_data.h"
#include "codecs.h"
#include "gnome_warning.h"
#include "control.h"
#include "gnome_ui.h"

#define GLADE_FILE PACKAGE_DATA_DIR"/xvidcap/glade/gnome-xvidcap.glade"
#define DEBUGFILE "gnome_options.c"


/* 
 * these are global 
 */
GtkWidget *xvc_pref_main_window = NULL;

extern xvCodec tCodecs[NUMCODECS];
extern xvFFormat tFFormats[NUMCAPS];
extern xvAuCodec tAuCodecs[NUMAUCODECS];

extern AppData *app;
extern GtkWidget *xvc_warn_main_window;
extern GtkWidget *xvc_ctrl_main_window;


static AppData pref_app;

static char* format_combo_entries[NUMCAPS]; 
static int sf_t_format = 0;
static int sf_t_codec = 0;
static int sf_t_au_codec = 0;

#ifdef HAVE_LIBAVCODEC
static int mf_fps_widget_save_old_codec = -1; 
static int mf_t_format = 0;
static int mf_t_codec = 0;
static int mf_t_au_codec = 0;
#endif // HAVE_LIBAVCODEC

static int OK_attempts = 0;
static xvErrorListItem *errors_after_cli = NULL;


// first callbacks here ...
// 
// 

static void read_app_data_from_pref_gui(AppData * lapp)
{
    #define DEBUGFUNCTION "read_app_data_from_pref_gui()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);

    // general tab 
    // default capture mode 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_default_capture_mode_sf_radiobutton");
    g_assert(w);
    
    lapp->default_mode = ((gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(w) ))?0:1); 
    
    // capture mouse pointer 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_capture_mouse_checkbutton");
    g_assert(w);
    
    if ( gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON( w ) ) ) { 
        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_capture_mouse_white_radiobutton");
        g_assert(w);    
        
        if ( gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON( w ) ) ) { 
            lapp->mouseWanted = 1; 
        } else {
            lapp->mouseWanted = 2; 
        } 
    } else { 
        lapp->mouseWanted = 0; 
    } 
    
    // save geometry 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_save_geometry_checkbutton");
    g_assert(w);

    if ( gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON( w ) ) ) { 
        lapp->flags |= FLG_SAVE_GEOMETRY; 
    } else { 
        lapp->flags &= ~FLG_SAVE_GEOMETRY; 
    } 

#ifdef HAVE_SHMAT
    // use shared-memory 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_shared_mem_checkbutton");
    g_assert(w);

    if ( gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON( w ) ) ) { 
        lapp->flags |= FLG_USE_SHM; 
    } else { 
        lapp->flags &= ~FLG_USE_SHM; 
    } 
#endif // HAVE_SHMAT

    // autocontinue 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_autocontinue_checkbutton");
    g_assert(w);

    if ( gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON( w ) ) ) { 
        lapp->flags |= FLG_AUTO_CONTINUE; 
    } else { 
        lapp->flags &= ~FLG_AUTO_CONTINUE; 
    }


    // sf
    // file name 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_sf_filename_entry");
    g_assert(w);

    lapp->single_frame.file = strdup((char*) gtk_entry_get_text(GTK_ENTRY(w))); 
    
    // file format
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_sf_format_auto_checkbutton");
    g_assert(w);

    if ( gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON( w ) ) ) { 
        lapp->single_frame.target = 0; 
    } else { 
        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_sf_format_combobox");
        g_assert(w);
    
        char *selected_format = (char*) gtk_combo_box_get_active_text(GTK_COMBO_BOX(w));
        int n, m = -1;
    
        for ( n = 0; n < NUMCAPS && m < 0; n++) {
            char *format = format_combo_entries[n]; 
            if ( strcasecmp(format, selected_format)==0 ) m = n; 
#ifdef DEBUG
            printf("%s %s: %s = %s ?\n", DEBUGFILE, DEBUGFUNCTION, format, selected_format);
#endif // DEBUG
        }
    
        lapp->single_frame.target = n; 
#ifdef DEBUG
            printf("%s %s: found new target: %i\n", DEBUGFILE, DEBUGFUNCTION, n);
#endif // DEBUG
    } 
    
    // codec is always 0 for sf
    lapp->single_frame.targetCodec = 0; 
#ifdef HAVE_FFMPEG_AUDIO
    // au_targetCodec is always 0 for sf 
    lapp->single_frame.au_targetCodec = 0; 
#endif // HAVE_FFMPEG_AUDIO

    // fps
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_sf_fps_hscale");
    g_assert(w);

    lapp->single_frame.fps = (int) (gtk_range_get_value (GTK_RANGE (w)) * 100); 
               
    // quality 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_sf_quality_hscale");
    g_assert(w);

    lapp->single_frame.quality = gtk_range_get_value (GTK_RANGE (w)); 
    
    // max time 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_sf_max_time_spinbutton");
    g_assert(w);

    lapp->single_frame.time = (int) gtk_spin_button_get_value_as_int (
                GTK_SPIN_BUTTON (w)); 
                
    // max frames 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_sf_max_frames_spinbutton");
    g_assert(w);

    lapp->single_frame.frames = (int) gtk_spin_button_get_value_as_int (
                GTK_SPIN_BUTTON (w)); 
                
    // start_no 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_sf_start_no_spinbutton");
    g_assert(w);
    
    lapp->single_frame.start_no = (int) gtk_spin_button_get_value_as_int (
                GTK_SPIN_BUTTON (w)); 
                
    // step 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_sf_frame_increment_spinbutton");
    g_assert(w);
    
    lapp->single_frame.step = (int) gtk_spin_button_get_value_as_int (
                GTK_SPIN_BUTTON (w));


#ifdef HAVE_LIBAVCODEC

    // mf
    
    // file name 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_filename_entry");
    g_assert(w);

    lapp->multi_frame.file = strdup((char*) gtk_entry_get_text(GTK_ENTRY(w))); 
    
    // format 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_format_auto_checkbutton");
    g_assert(w);

    if ( gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON( w ) ) ) { 
        lapp->multi_frame.target = 0; 
    } else {
        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_format_combobox");
        g_assert(w);

        char *selected_format = (char*) gtk_combo_box_get_active_text( GTK_COMBO_BOX(w)); 
        int i, n = -1;
    
        for ( i = 0; i < NUMCAPS && n < 0; i++ ) { 
            if ( strcasecmp(format_combo_entries[i],selected_format)==0 ) 
                n = i; 
        }
        lapp->multi_frame.target = n; 
    }
    
    // codec 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_codec_auto_checkbutton");
    g_assert(w);

    if ( gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON( w ) ) ) { 
        lapp->multi_frame.targetCodec = 0; 
    } else { 
        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_codec_combobox");
        g_assert(w);
    
        char *selected_codec = (char*) gtk_combo_box_get_active_text( GTK_COMBO_BOX(w)); 
        int a, codec = -1;
    
        for (a = 0; a < NUMCODECS && codec < 0; a++ ) { 
            if ( strcasecmp(selected_codec, tCodecs[a].longname )==0) codec = a; 
        }
        lapp->multi_frame.targetCodec = codec; 
    } 
    
    // fps 
    { 
        gboolean combobox_visible = FALSE, hscale_visible = FALSE;
        GtkWidget *combobox = NULL, *hscale = NULL;
        
        combobox = glade_xml_get_widget(xml, "xvc_pref_mf_fps_combobox");
        g_assert(combobox);
        gtk_object_get(GTK_OBJECT(combobox), "visible", &combobox_visible, NULL);

        hscale = glade_xml_get_widget(xml, "xvc_pref_mf_fps_hscale");
        g_assert(hscale);
        gtk_object_get(GTK_OBJECT(hscale), "visible", &hscale_visible, NULL);

        if ( combobox_visible ) {
            lapp->multi_frame.fps = xvc_get_int_from_float_string(
                            (char*) gtk_combo_box_get_active_text( 
                                        GTK_COMBO_BOX(combobox))); 
        } else if ( hscale_visible ) {
            lapp->multi_frame.fps = (int) (gtk_range_get_value ( GTK_RANGE(hscale) ) * 100); 
        }
        if ( ((!combobox_visible) && (!hscale_visible)) || 
                (combobox_visible && hscale_visible) ) {
            fprintf(stderr, 
                "%s %s: unable to determine type of mf_fps_widget, please file a bug!\n",
                DEBUGFILE, DEBUGFUNCTION); 
            exit (1); 
        }
#ifdef DEBUG
        printf("%s %s: read fps: %i\n", DEBUGFILE, DEBUGFUNCTION, lapp->multi_frame.fps);
#endif // DEBUG
    } 
    
    // max time 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_max_time_spinbutton");
    g_assert(w);

    lapp->multi_frame.time = (int) gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (w)); 
    
    // max frames 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_max_frames_spinbutton");
    g_assert(w);

    lapp->multi_frame.frames = (int) gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (w)); 

    // quality
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_quality_hscale");
    g_assert(w);

    lapp->multi_frame.quality = gtk_range_get_value(GTK_RANGE (w));


#ifdef HAVE_FFMPEG_AUDIO
    // mf audio settings 
    // au_targetCodec 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_codec_auto_checkbutton");
    g_assert(w);
    
    if ( gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON( w ) ) ) { 
        lapp->multi_frame.au_targetCodec = 0; 
    } else { 
        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_codec_combobox");
        g_assert(w);
    
        char *selected_aucodec = (char*) gtk_combo_box_get_active_text( GTK_COMBO_BOX(w)); 
        int a, aucodec = -1;
    
        for (a = 0; a < NUMAUCODECS && aucodec < 0; a++ ) { 
#ifdef DEBUG
            printf("%s %s: selected_aucodec: %s - a: %i - tAuCodec[a].name: %s \n", 
                    DEBUGFILE, DEBUGFUNCTION, selected_aucodec, a, tAuCodecs[a].name );
#endif // DEBUG 
            if ( strcasecmp(selected_aucodec, tAuCodecs[a].name)==0) 
                aucodec = a; 
        } 
        lapp->multi_frame.au_targetCodec = aucodec;
#ifdef DEBUG 
        printf("%s %s: saving audio codec: %i \n", 
                    DEBUGFILE, DEBUGFUNCTION, lapp->multi_frame.au_targetCodec); 
#endif // DEBUG 
    }
    
    // audio wanted 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_checkbutton");
    g_assert(w);

    if ( gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON( w ) ) ) { 
        lapp->multi_frame.audioWanted = 1; 
    } else {
        lapp->multi_frame.audioWanted = 0; 
    } 
    
    // sample rate 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_sample_rate_spinbutton");
    g_assert(w);

    lapp->multi_frame.sndrate = (int) gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (w)); 
    
    // bit rate
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_bit_rate_spinbutton");
    g_assert(w);

    lapp->multi_frame.sndsize = (int) gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (w)); 
    
    // audio in
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_input_device_entry");
    g_assert(w);

    lapp->snddev = strdup((char*) gtk_entry_get_text(GTK_ENTRY(w))); 
    
    // audio channels 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_channels_hscale");
    g_assert(w);

    lapp->multi_frame.sndchannels = gtk_range_get_value (GTK_RANGE (w));
#endif // HAVE_FFMPEG_AUDIO

#endif // HAVE_LIBAVCODEC
     
    // commands 
    // help cmd 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_commands_help_entry");
    g_assert(w);

    lapp->help_cmd = strdup((char*) gtk_entry_get_text(GTK_ENTRY(w))); 
    
    // sf commands 
    // sf playback command 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_commands_sf_play_entry");
    g_assert(w);

    lapp->single_frame.play_cmd = strdup((char*) gtk_entry_get_text(GTK_ENTRY(w))); 
    
    // sf encoding command 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_commands_sf_encode_entry");
    g_assert(w);

    lapp->single_frame.video_cmd = strdup((char*) gtk_entry_get_text(GTK_ENTRY(w))); 

    // sf edit command 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_commands_sf_edit_entry");
    g_assert(w);

    lapp->single_frame.edit_cmd = strdup((char*) gtk_entry_get_text(GTK_ENTRY(w))); 

#ifdef HAVE_LIBAVCODEC
    // mf commands 
    // mf playback command 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_commands_mf_play_entry");
    g_assert(w);

    lapp->multi_frame.play_cmd = strdup((char*) gtk_entry_get_text(GTK_ENTRY(w))); 
    
    // mf encoding command 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_commands_mf_encode_entry");
    g_assert(w);

    lapp->multi_frame.video_cmd = strdup((char*) gtk_entry_get_text(GTK_ENTRY(w))); 

    // mf edit command 
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_commands_mf_edit_entry");
    g_assert(w);

    lapp->multi_frame.edit_cmd = strdup((char*) gtk_entry_get_text(GTK_ENTRY(w))); 
#endif // HAVE_LIBAVCODEC
 
    #undef DEBUGFUNCTION
}


void preferences_submit()
{
    Display *mydisplay = NULL;
    Window root;
    XWindowAttributes win_attr;

    xvc_app_data_copy(app, &pref_app);

    // Get X display and Window Attributes
    // notice, here we're still talking app->flags
    if (!(app->flags & FLG_NOGUI)) {
        mydisplay =
            GDK_DRAWABLE_XDISPLAY(GTK_WIDGET(xvc_ctrl_main_window)->
                                  window);
    } else {
        mydisplay = XOpenDisplay(NULL);
    }
    g_assert(mydisplay);

    root = RootWindow(mydisplay, DefaultScreen(mydisplay));
    g_assert(root);

    if (!XGetWindowAttributes(mydisplay, root, &win_attr))
        perror("Can't get window attributes!\n");
    xvc_job_set_from_app_data(app, mydisplay, win_attr);
    // validate the job parameters
    xvc_job_validate();

    // set controls active/inactive/sensitive/insensitive according to
    // current options
    xvc_reset_ctrl_main_window_according_to_current_prefs();

    if (xvc_errors_delete_list(errors_after_cli)) {
        fprintf(stderr,
                "gtk2_options: Unrecoverable error while freeing error list, please contact the xvidcap project.\n");
        exit(1);
    }
    // reset attempts so warnings will be shown again next time ...
    OK_attempts = 0;

    gtk_widget_destroy(xvc_pref_main_window);

}


void xvc_pref_reset_OK_attempts()
{
    OK_attempts = 0;
}


void xvc_pref_do_OK()
{
    int count_non_info_messages = 0, rc = 0;

    errors_after_cli = xvc_app_data_validate(&pref_app, 0, &rc);
    if (rc == -1) {
        fprintf(stderr,
                "gtk2_options: Unrecoverable error while validating options, please contact the xvidcap project.\n");
        exit(1);
    }
    // warning dialog
    if (errors_after_cli != NULL) {
        xvErrorListItem *err;

        err = errors_after_cli;
        for (; err != NULL; err = err->next) {
            if (err->err->type != XV_ERR_INFO)
                count_non_info_messages++;
        }

        if (count_non_info_messages > 0
            || (app->flags & FLG_RUN_VERBOSE && OK_attempts == 0)) {
            xvc_warn_main_window =
                xvc_create_warning_with_errors(errors_after_cli, 0);
            // printf("gtk2_options: pointer to errors_after_cli: %p - rc: 
            // 
            // 
            // %i\n", errors_after_cli, rc);
            OK_attempts++;

        } else {
            preferences_submit();
        }
    } else {
        preferences_submit();
    }

    // if ( count_non_info_messages > 0 ) {
    // fprintf (stderr, "gtk2_options: Can't resolve all conflicts in
    // input data. Please double-check your input.\n");
    // exit (1);
    // } 

}


static void doHelp()
{
    if ( pref_app.help_cmd != NULL ) system((char*) pref_app.help_cmd);
}


void
on_xvc_pref_main_window_response(GtkDialog * dialog, gint response_id,
                    gpointer user_data)
{
    #define DEBUGFUNCTION "on_xvc_pref_main_window_response()"

    Job *job = xvc_job_ptr ();
    
    //printf("response id : %i \n", response_id); 
    switch (response_id) { 
        case GTK_RESPONSE_OK: 
#ifdef DEBUG
            printf("%s %s: Entering with:\n", DEBUGFILE, DEBUGFUNCTION);
            printf("filename: %s - target: %i - targetCodec: %i - audioCodec: %i \n",
                pref_app.multi_frame.file, pref_app.multi_frame.target,
                pref_app.multi_frame.targetCodec, 
#ifdef HAVE_FFMPEG_AUDIO
                pref_app.multi_frame.au_targetCodec
#else
                0
#endif // HAVE_FFMPEG_AUDIO
                );     
#endif // DEBUG

            // need to read pref_data from gui 
            read_app_data_from_pref_gui ( &pref_app ); 
    
#ifdef DEBUG 
            printf("%s %s: Leaving with:\n", DEBUGFILE, DEBUGFUNCTION);
            printf("filename: %s - target: %i - targetCodec: %i - audioCodec: %i \n",
                pref_app.multi_frame.file, pref_app.multi_frame.target,
                pref_app.multi_frame.targetCodec, 
#ifdef HAVE_FFMPEG_AUDIO
                pref_app.multi_frame.au_targetCodec
#else
                0
#endif // HAVE_FFMPEG_AUDIO
                );     
#endif // DEBUG

            xvc_pref_do_OK (); 
            xvc_change_filename_display (job->pic_no); 
            break;
            
        case GTK_RESPONSE_CANCEL: 
            gtk_widget_destroy(xvc_pref_main_window); 
            break; 
            
        case GTK_RESPONSE_HELP: 
            doHelp(); 
            
        default: 
            break; 
    } 
    
    #undef DEBUGFUNCTION
}


gboolean
on_xvc_pref_main_window_delete_event(GtkWidget * widget, GdkEvent * event,
                        gpointer user_data)
{
    return FALSE;
}

void 
on_xvc_pref_main_window_close(GtkDialog * dialog, gpointer user_data)
{
    // empty as yet ...
}

void 
on_xvc_pref_main_window_destroy(GtkButton * button, gpointer user_data)
{
    // empty as yet ...
}

void
on_xvc_pref_capture_mouse_checkbutton_toggled(GtkToggleButton * togglebutton,
                             gpointer user_data)
{
    #define DEBUGFUNCTION "on_xvc_pref_capture_mouse_checkbutton_toggled()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);

    if ( gtk_toggle_button_get_active ( togglebutton ) ) {
        w = glade_xml_get_widget(xml, "xvc_pref_capture_mouse_black_radiobutton");
        g_assert(w);
        gtk_widget_set_sensitive ( w, TRUE );
        w = glade_xml_get_widget(xml, "xvc_pref_capture_mouse_white_radiobutton");
        g_assert(w);
        gtk_widget_set_sensitive ( w, TRUE );
    } else {
        w = glade_xml_get_widget(xml, "xvc_pref_capture_mouse_black_radiobutton");
        g_assert(w);
        gtk_widget_set_sensitive ( w, FALSE );
        w = glade_xml_get_widget(xml, "xvc_pref_capture_mouse_white_radiobutton");
        g_assert(w);
        gtk_widget_set_sensitive ( w, FALSE);
    }

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
}

void
on_xvc_pref_sf_format_auto_checkbutton_toggled(GtkToggleButton * togglebutton,
                                gpointer user_data)
{
    #define DEBUGFUNCTION "on_xvc_pref_sf_format_auto_checkbutton_toggled()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);

    if ( gtk_toggle_button_get_active ( togglebutton ) ) {
        w = glade_xml_get_widget(xml, "xvc_pref_sf_format_combobox");
        g_assert(w);
        gtk_widget_set_sensitive ( w, FALSE ); 
    } else {
        w = glade_xml_get_widget(xml, "xvc_pref_sf_format_combobox");
        g_assert(w);
        gtk_widget_set_sensitive ( w, TRUE ); 
    }
    
#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
}

void
on_xvc_pref_sf_filename_entry_changed(GtkEntry * entry, gpointer user_data)
{
    #define DEBUGFUNCTION "on_xvc_pref_sf_filename_entry_changed()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    int i, a = 0;
    GtkListStore *sf_format_list_store = NULL;
    gboolean valid;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);
    w = glade_xml_get_widget(xml, "xvc_pref_sf_format_auto_checkbutton");
    g_assert(w);

    if ( gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(w) ) ) { 
        GtkTreeIter iter;

        i = xvc_codec_get_target_from_filename ((char*) gtk_entry_get_text (GTK_ENTRY (entry) )); 
        
        w = glade_xml_get_widget(xml, "xvc_pref_sf_format_combobox");
        g_assert(w);
        sf_format_list_store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(w)));
        g_assert(sf_format_list_store);

        valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL(sf_format_list_store), &iter);

#ifdef HAVE_LIBAVCODEC
        if ( i > 0 && i < CAP_MF ) {
#else // HAVE_LIBAVCODEC
        if ( i > 0 && i < NUMCAPS ) {
#endif // HAVE_LIBAVCODEC
            while (valid) {
                gchar *str_data;

                // Make sure you terminate calls to gtk_tree_model_get()
                // with a '-1' value
                gtk_tree_model_get (GTK_TREE_MODEL(sf_format_list_store), &iter, 
                            0, &str_data,
                            -1);

#ifdef DEBUG
                printf("%s %s: format_combo_entries %i %s\n", DEBUGFILE, DEBUGFUNCTION,
                        (i-1), format_combo_entries[i-1]);
#endif // DEBUG
                if (strcasecmp(str_data, format_combo_entries[i - 1])==0) {
#ifdef DEBUG
                    printf("%s %s: found %s\n", DEBUGFILE, DEBUGFUNCTION,
                            str_data);
#endif // DEBUG
                    gtk_combo_box_set_active(GTK_COMBO_BOX(w), a);
                }
                g_free (str_data);

                valid = gtk_tree_model_iter_next (GTK_TREE_MODEL(sf_format_list_store), &iter);
                a++;
            }
        }
    } 
    
#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
}

void
on_xvc_pref_mf_format_auto_checkbutton_toggled(GtkToggleButton * togglebutton,
                                gpointer user_data)
{
    #define DEBUGFUNCTION "on_xvc_pref_mf_format_auto_checkbutton_toggled()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);
    w = glade_xml_get_widget(xml, "xvc_pref_mf_format_combobox");
    g_assert(w);

    if ( gtk_toggle_button_get_active ( togglebutton ) ) {
        gtk_widget_set_sensitive ( w, FALSE ); 
    } else {
        gtk_widget_set_sensitive ( w, TRUE ); 
    } 

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
}

void
on_xvc_pref_mf_filename_entry_changed(GtkEntry * entry, gpointer user_data)
{
#ifdef HAVE_LIBAVCODEC
    #define DEBUGFUNCTION "on_xvc_pref_mf_filename_entry_changed()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;
    int i, a = 0;
    GtkListStore *mf_format_list_store = NULL;
    gboolean valid;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);
    w = glade_xml_get_widget(xml, "xvc_pref_mf_format_auto_checkbutton");
    g_assert(w);

    if ( gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(w) ) ) { 
        GtkTreeIter iter;

        i = xvc_codec_get_target_from_filename ((char*) gtk_entry_get_text (GTK_ENTRY (entry) )); 
        
        w = glade_xml_get_widget(xml, "xvc_pref_mf_format_combobox");
        g_assert(w);
        mf_format_list_store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(w)));
        g_assert(mf_format_list_store);

        valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL(mf_format_list_store), &iter);

        if ( i >= CAP_MF && i < NUMCAPS ) {
            while (valid) {
                gchar *str_data;

                // Make sure you terminate calls to gtk_tree_model_get()
                // with a '-1' value
                gtk_tree_model_get (GTK_TREE_MODEL(mf_format_list_store), &iter, 
                            0, &str_data,
                            -1);

#ifdef DEBUG
                printf("%s %s: format_combo_entries %i %s\n", DEBUGFILE, DEBUGFUNCTION,
                        (i-1), format_combo_entries[i-1]);
#endif // DEBUG
                if (strcasecmp(str_data, format_combo_entries[i - 1])==0) {
#ifdef DEBUG
                    printf("%s %s: found %s\n", DEBUGFILE, DEBUGFUNCTION,
                            str_data);
#endif // DEBUG
                    gtk_combo_box_set_active(GTK_COMBO_BOX(w), a);
                }
                g_free (str_data);

                valid = gtk_tree_model_iter_next (GTK_TREE_MODEL(mf_format_list_store), &iter);
                a++;
            }
        }
    } 

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
#endif // HAVE_LIBAVCODEC
}

void
on_xvc_pref_mf_codec_auto_checkbutton_toggled(GtkToggleButton * togglebutton,
                               gpointer user_data)
{
    #define DEBUGFUNCTION "on_xvc_pref_mf_codec_auto_checkbutton_toggled()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);
    w = glade_xml_get_widget(xml, "xvc_pref_mf_codec_combobox");
    g_assert(w);

    if ( gtk_toggle_button_get_active ( togglebutton ) ) {
        gtk_widget_set_sensitive ( w, FALSE ); 
    } else {
        gtk_widget_set_sensitive ( w, TRUE ); 
    } 

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
}

void
on_xvc_pref_mf_audio_codec_auto_checkbutton_toggled(GtkToggleButton * togglebutton,
                                     gpointer user_data)
{
    #define DEBUGFUNCTION "on_xvc_pref_mf_audio_codec_auto_checkbutton_toggled()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);
    w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_codec_combobox");
    g_assert(w);

    if ( gtk_toggle_button_get_active ( togglebutton ) ) {
        gtk_widget_set_sensitive ( w, FALSE ); 
    } else {
        gtk_widget_set_sensitive ( w, TRUE ); 
    } 

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
}

static void mf_codec_combo_set_contents_from_format(int format)
{
    #define DEBUGFUNCTION "mf_codec_combo_set_contents_from_format()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf("%s %s: Entering with format %i\n", DEBUGFILE, DEBUGFUNCTION, format);
#endif // DEBUG

    if (format <= 0) return;
    
    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_codec_combobox");

    if (w != NULL) {
        int a;
        char *codec_string = NULL;

        GtkListStore *mf_codec_list_store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(w)));
        // NOTE: for this to work ther must ALWAYS be a dummy entry present in glade
        // otherwise glade will not create a model at all (let alone a list store)

        g_assert(mf_codec_list_store);
        gtk_list_store_clear(GTK_LIST_STORE(mf_codec_list_store));

        codec_string = strdup(xvc_next_element(tFFormats[format].allowed_vid_codecs));
        for (a = 0; codec_string; a++) {
#ifdef DEBUG
            printf("%s %s: Adding this text (item %i) to mf codec combobox %s\n", DEBUGFILE, DEBUGFUNCTION,
            a, codec_string);
#endif // DEBUG
            // FIXME: do I need to do the utf8 conversion?
            gtk_combo_box_append_text(GTK_COMBO_BOX(w), codec_string);
            codec_string = xvc_next_element(NULL);
            if (codec_string) codec_string = strdup(codec_string);
        }
    }
    
#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
}

static void mf_audio_codec_combo_set_contents_from_format(int format)
{
    #define DEBUGFUNCTION "mf_audio_codec_combo_set_contents_from_format()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf("%s %s: Entering with format %i\n", DEBUGFILE, DEBUGFUNCTION, format);
#endif // DEBUG

    if (format <= 0) return;
    
    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_codec_combobox");

    if (w != NULL) {
        int a;
        char *au_codec_string = NULL;
#ifdef HAVE_FFMPEG_AUDIO
        GtkWidget *au_check = NULL;
#endif // HAVE_FFMPEG_AUDIO

        GtkListStore *mf_au_codec_list_store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(w)));
        // NOTE: for this to work ther must ALWAYS be a dummy entry present in glade
        // otherwise glade will not create a model at all (let alone a list store)

        g_assert(mf_au_codec_list_store);
        gtk_list_store_clear(GTK_LIST_STORE(mf_au_codec_list_store));

        au_codec_string = strdup(xvc_next_element(tFFormats[format].allowed_au_codecs));
        for (a = 0; au_codec_string; a++) {
#ifdef DEBUG
            printf("%s %s: Adding this text (item %i) to mf audio codec combobox %s\n", DEBUGFILE, DEBUGFUNCTION,
            a, au_codec_string);
#endif // DEBUG

            // FIXME: do I need to do the utf8 conversion?
            gtk_combo_box_append_text(GTK_COMBO_BOX(w), au_codec_string);
            au_codec_string = xvc_next_element(NULL);
            if (au_codec_string) au_codec_string = strdup(au_codec_string);
        }
    }

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
}

static int mf_format_combo_get_target_from_text(char *text) {
#ifdef HAVE_LIBAVCODEC
    int a, b = -1;

    for ( a = (CAP_MF-1); a < (NUMCAPS-1) && b < 0; a++ ) { 
        if (strcasecmp(format_combo_entries[a], text)==0) b = a; 
    } 
    
    return (b + 1); 
#endif // HAVE_LIBAVCODEC
    return 0;
}


void
on_xvc_pref_mf_format_combobox_changed(GtkComboBox * combobox, gpointer user_data)
{
    #define DEBUGFUNCTION "on_xvc_pref_mf_format_combobox_changed()"

#ifdef HAVE_FFMPEG_AUDIO
    GladeXML *xml = NULL;
    GtkWidget *codec_combobox = NULL, *au_combobox = NULL;
    char *format_selected = strdup((char*) gtk_combo_box_get_active_text( GTK_COMBO_BOX(combobox))); 
    int a;

    // FIXME: save previously selected format to avoid doing all this if unchanged

    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);
    codec_combobox = glade_xml_get_widget(xml, "xvc_pref_mf_codec_combobox");
    g_assert(codec_combobox);
    au_combobox = glade_xml_get_widget(xml, "xvc_pref_mf_audio_codec_combobox");
    g_assert(au_combobox);
    
#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
    printf("%s %s: reading '%s' from format combo box ... \n", DEBUGFILE, DEBUGFUNCTION, 
                format_selected); 
#endif // DEBUG
    
    a = mf_format_combo_get_target_from_text (format_selected); 
#ifdef DEBUG
    printf("%s %s: format selected: %s - number: %i\n", DEBUGFILE, DEBUGFUNCTION,
            format_selected, a);
#endif // DEBUG

    if (a > 0) {
        char *old_selected_codec = NULL;
        int n = 0; 
        GtkListStore *mf_codec_list_store = NULL;
        GtkTreeIter iter;
        gboolean valid = FALSE;
        GtkWidget *codec_auto = NULL;
#ifdef HAVE_FFMPEG_AUDIO
        char *old_selected_aucodec = NULL;
        GtkWidget *au_codec_auto = NULL;
        GtkListStore *mf_au_codec_list_store;

        old_selected_aucodec = gtk_combo_box_get_active_text( GTK_COMBO_BOX(au_combobox));
        if (old_selected_aucodec) old_selected_aucodec = strdup(old_selected_aucodec);
#endif // HAVE_FFMPEG_AUDIO

        old_selected_codec = gtk_combo_box_get_active_text( GTK_COMBO_BOX(codec_combobox));
        if (old_selected_codec) old_selected_codec = strdup(old_selected_codec);
        
        mf_codec_combo_set_contents_from_format (a);

        mf_codec_list_store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(codec_combobox)));
        g_assert(mf_codec_list_store);
        codec_auto = glade_xml_get_widget(xml, "xvc_pref_mf_codec_auto_checkbutton");
        g_assert(codec_auto);

        valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL(mf_codec_list_store), &iter);

        while (valid) {
            gchar *str_data;

            // Make sure you terminate calls to gtk_tree_model_get()
            // with a '-1' value
            gtk_tree_model_get (GTK_TREE_MODEL(mf_codec_list_store), &iter, 
                        0, &str_data,
                        -1);

            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(codec_auto) ) ) { 
                if ( strcasecmp(str_data, tCodecs[tFFormats[a].def_vid_codec].name)==0) { 
                    gtk_combo_box_set_active(GTK_COMBO_BOX(codec_combobox), n);
                } 
            } else if ( strlen( old_selected_codec ) > 0 ) { 
                if ( strcasecmp(str_data, old_selected_codec)==0) { 
                    gtk_combo_box_set_active(GTK_COMBO_BOX(codec_combobox), n);
                } 
            } else if ( strcasecmp( str_data, tCodecs[pref_app.multi_frame.targetCodec].name)==0 ) { 
                gtk_combo_box_set_active(GTK_COMBO_BOX(codec_combobox), n);
            } 

            g_free (str_data);
            valid = gtk_tree_model_iter_next (GTK_TREE_MODEL(mf_codec_list_store), &iter);
            n++;
        }

#ifdef HAVE_FFMPEG_AUDIO
        mf_audio_codec_combo_set_contents_from_format (a); 
        n = 0;

        mf_au_codec_list_store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(au_combobox)));
        g_assert(mf_au_codec_list_store);
        au_codec_auto = glade_xml_get_widget(xml, "xvc_pref_mf_audio_codec_auto_checkbutton");
        g_assert(au_codec_auto);

        valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL(mf_au_codec_list_store), &iter);
    
        while (valid) {
            gchar *str_data;

            // Make sure you terminate calls to gtk_tree_model_get()
            // with a '-1' value
            gtk_tree_model_get (GTK_TREE_MODEL(mf_au_codec_list_store), &iter, 
                        0, &str_data,
                        -1);
            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(au_codec_auto) ) ) { 
                if ( strcasecmp(str_data, tAuCodecs[tFFormats[a].def_au_codec].name)==0) { 
                    gtk_combo_box_set_active(GTK_COMBO_BOX(au_combobox), n);
                } 
            } else if ( strlen( old_selected_aucodec ) > 0 ) { 
                if ( strcasecmp(str_data, old_selected_aucodec)==0) { 
                    gtk_combo_box_set_active(GTK_COMBO_BOX(au_combobox), n);
                } 
            } else if ( strcasecmp( str_data, tCodecs[pref_app.multi_frame.au_targetCodec].name)==0 ) { 
                gtk_combo_box_set_active(GTK_COMBO_BOX(au_combobox), n);
            } 

            g_free (str_data);
            valid = gtk_tree_model_iter_next (GTK_TREE_MODEL(mf_au_codec_list_store), &iter);
            n++;
        }
#endif // HAVE_FFMPEG_AUDIO
    
    } 
    
#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

#endif // HAVE_FFMPEG_AUDIO
    #undef DEBUGFUNCTION
}


void
on_xvc_pref_mf_audio_checkbutton_toggled(GtkToggleButton * togglebutton,
                          gpointer user_data)
{
    #define DEBUGFUNCTION "on_xvc_pref_mf_audio_checkbutton_toggled()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);

    if ( ! (gtk_toggle_button_get_active ( togglebutton ) )) {
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_sample_rate_label");
        g_assert(w);
        gtk_widget_set_sensitive ( w, FALSE ); 

        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_sample_rate_spinbutton");
        g_assert(w);
        gtk_widget_set_sensitive ( w, FALSE ); 
        
        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_bit_rate_label");
        g_assert(w);
        gtk_widget_set_sensitive ( w, FALSE ); 

        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_bit_rate_spinbutton");
        g_assert(w);
        gtk_widget_set_sensitive ( w, FALSE ); 
        
        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_input_device_label");
        g_assert(w);
        gtk_widget_set_sensitive ( w, FALSE ); 
        
        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_input_device_select_button");
        g_assert(w);
        gtk_widget_set_sensitive ( w, FALSE ); 

        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_input_device_entry");
        g_assert(w);
        gtk_widget_set_sensitive ( w, FALSE ); 

        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_channels_label");
        g_assert(w);
        gtk_widget_set_sensitive ( w, FALSE ); 

        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_channels_hscale");
        g_assert(w);
        gtk_widget_set_sensitive ( w, FALSE ); 

        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_codec_label");
        g_assert(w);
        gtk_widget_set_sensitive ( w, FALSE ); 

        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_codec_auto_checkbutton");
        g_assert(w);
        gtk_widget_set_sensitive ( w, FALSE ); 

        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_codec_combobox");
        g_assert(w);
        gtk_widget_set_sensitive ( w, FALSE ); 
    } else {
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_sample_rate_label");
        g_assert(w);
        gtk_widget_set_sensitive ( w, TRUE ); 

        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_sample_rate_spinbutton");
        g_assert(w);
        gtk_widget_set_sensitive ( w, TRUE ); 
        
        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_bit_rate_label");
        g_assert(w);
        gtk_widget_set_sensitive ( w, TRUE ); 

        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_bit_rate_spinbutton");
        g_assert(w);
        gtk_widget_set_sensitive ( w, TRUE ); 
        
        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_input_device_label");
        g_assert(w);
        gtk_widget_set_sensitive ( w, TRUE ); 
        
        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_input_device_select_button");
        g_assert(w);
        gtk_widget_set_sensitive ( w, TRUE ); 

        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_input_device_entry");
        g_assert(w);
        gtk_widget_set_sensitive ( w, TRUE ); 

        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_channels_label");
        g_assert(w);
        gtk_widget_set_sensitive ( w, TRUE ); 

        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_channels_hscale");
        g_assert(w);
        gtk_widget_set_sensitive ( w, TRUE ); 

        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_codec_label");
        g_assert(w);
        gtk_widget_set_sensitive ( w, TRUE ); 

        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_codec_auto_checkbutton");
        g_assert(w);
        gtk_widget_set_sensitive ( w, TRUE ); 

        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_codec_combobox");
        g_assert(w);
        gtk_widget_set_sensitive ( w, TRUE ); 
    } 
     
#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
}


static void set_mf_fps_widget_from_codec(int codec)
{
#ifdef HAVE_LIBAVCODEC

    #define DEBUGFUNCTION "set_mf_fps_widget_from_codec()"
    GladeXML *xml = NULL;
    GtkWidget *combobox = NULL, *hscale = NULL;
    gboolean combobox_visible = FALSE, hscale_visible = FALSE;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    int curr_fps_val = 0;

    if ( codec == mf_fps_widget_save_old_codec ) return; 
    
    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);
    
    combobox = glade_xml_get_widget(xml, "xvc_pref_mf_fps_combobox");
    g_assert(combobox);
    gtk_object_get(GTK_OBJECT(combobox), "visible", &combobox_visible, NULL);
    
    hscale = glade_xml_get_widget(xml, "xvc_pref_mf_fps_hscale");
    g_assert(hscale);
    gtk_object_get(GTK_OBJECT(hscale), "visible", &hscale_visible, NULL);


    if ( combobox_visible ) {
        curr_fps_val = xvc_get_int_from_float_string(
                            (char*) gtk_combo_box_get_active_text( 
                                        GTK_COMBO_BOX(combobox))); 
    } else if ( hscale_visible ) {
        curr_fps_val = (int) (gtk_range_get_value ( GTK_RANGE(hscale) ) * 100); 
    }
    if ( ((!combobox_visible) && (!hscale_visible)) || 
                (combobox_visible && hscale_visible) ) {
        fprintf(stderr, 
                "%s %s: unable to determine type of mf_fps_widget, please file a bug!\n",
                DEBUGFILE, DEBUGFUNCTION); 
        exit (1); 
    }



    if ( tCodecs[codec].allowed_fps != NULL &&
            tCodecs[codec].allowed_fps_ranges == NULL ) { 
        // we only have a number of non-continuous fps allowed for this codec 

        GtkListStore *fps_list_store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(combobox)));
        // NOTE: for this to work ther must ALWAYS be a dummy entry present in glade
        // otherwise glade will not create a model at all (let alone a list store)
        char * element = NULL;
        int active_item = -1, default_item = -1, i = 0;

        g_assert(fps_list_store);
        gtk_list_store_clear(GTK_LIST_STORE(fps_list_store));

        gtk_widget_show(GTK_WIDGET(combobox));
        gtk_widget_hide(GTK_WIDGET(hscale));

        element = xvc_next_element ( tCodecs[codec].allowed_fps ); 
        do { 
            // the following is needed for correct handling of locale's decimal point 
            char lelement[256]; 
            int fps = xvc_get_int_from_float_string(element); 
            sprintf(lelement, "%.2f", ((float) (fps) / 100) );
            gtk_combo_box_append_text(GTK_COMBO_BOX(combobox), lelement);

            if ( fps == curr_fps_val ) active_item = i;
            if ( fps == tCodecs[codec].def_fps ) default_item = i;
            
            element = xvc_next_element ( NULL );
            i++;
        } while ( element != NULL );

        if ( active_item >= 0) gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), active_item);
        else if ( default_item >= 0 ) gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), default_item);
        else gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), 0);
        
    } else if ( tCodecs[codec].allowed_fps_ranges != NULL &&
                tCodecs[codec].allowed_fps == NULL &&
                xvc_num_elements(tCodecs[codec].allowed_fps_ranges) == 1 ) { 
        // we only have exactly one range 
        char* range = strdup( tCodecs[codec].allowed_fps_ranges ); 
        char* start = strtok( range, "-" ); 
        char* end = strtok( NULL, "-" );
    
        if (start == NULL || end == NULL ) { 
            fprintf(stderr, 
                    "%s %s: this codec supposedly only has one valid range of fps's, but we can't determin start and end.\n",
                    DEBUGFILE, DEBUGFUNCTION); 
            exit (2); 
        }

        gtk_widget_show(GTK_WIDGET(hscale));
        gtk_widget_hide(GTK_WIDGET(combobox));

        // check current fps value 
        if ( curr_fps_val < xvc_get_int_from_float_string(start) || 
                    curr_fps_val > xvc_get_int_from_float_string(end) ) { 
            curr_fps_val = ((int) tCodecs[codec].def_fps * 100); 
        } 
        // set new start and end
        gtk_range_set_range(GTK_RANGE(hscale), 
                (gdouble) ((float)xvc_get_int_from_float_string(start)/100),
                (gdouble) ((float)xvc_get_int_from_float_string(end)/100));
        gtk_range_set_value(GTK_RANGE(hscale), 
                (gdouble) ((float)curr_fps_val/100));

    } else { 
        // we have both 
        char *fixed_start = NULL, *fixed_end = NULL, *fixed_buf = NULL;
        char *start =  NULL, *end = NULL; 
        char *range = xvc_next_element ( tCodecs[codec].allowed_fps_ranges );
    
        if ( range != NULL ) { 
            start = strdup( strtok( range, "-" ) ); 
            end = strdup( strtok( NULL, "-" ) ); 
        } 
        range = xvc_next_element ( NULL );
        
        while (range != NULL) { 
            end = strtok( range, "-" ); 
            end = strdup( strtok( NULL, "-" ) ); 
            range = xvc_next_element ( NULL ); 
        }
        
        fixed_start = xvc_next_element( tCodecs[codec].allowed_fps ); 
        if ( strtod(fixed_start,NULL) < strtod(start,NULL) ) 
            start = fixed_start;
    
        fixed_buf = xvc_next_element( NULL ); 
        while ( fixed_buf != NULL ) {
            fixed_end = strdup ( fixed_buf ); 
            fixed_buf = xvc_next_element( NULL ); 
        } 
        if ( strtod(fixed_end,NULL) > strtod(end,NULL) ) 
            end = fixed_end;

        gtk_widget_show(GTK_WIDGET(hscale));
        gtk_widget_hide(GTK_WIDGET(combobox));

        // check current fps value 
        if ( curr_fps_val < xvc_get_int_from_float_string(start) || 
                    curr_fps_val > xvc_get_int_from_float_string(end) ) { 
            curr_fps_val = ((int) tCodecs[codec].def_fps * 100); 
        } 
        // set new start and end
        gtk_range_set_range(GTK_RANGE(hscale), 
                (gdouble) ((float)xvc_get_int_from_float_string(start)/100),
                (gdouble) ((float)xvc_get_int_from_float_string(end)/100));
        gtk_range_set_value(GTK_RANGE(hscale), 
                (gdouble) ((float)curr_fps_val/100));

    }
    
    mf_fps_widget_save_old_codec = codec; 
     
#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    #undef DEBUGFUNCTION
#endif // HAVE_LIBAVCODEC
}


void 
on_xvc_pref_mf_codec_combobox_changed(GtkComboBox * cb, gpointer user_data)
{
#ifdef HAVE_LIBAVCODEC
    #define DEBUGFUNCTION "on_xvc_pref_mf_codec_combobox_changed()"

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    int a, codec = -1; 
    char* codec_selected = (char*) gtk_combo_box_get_active_text(cb);

    if (codec_selected) {
        for (a = 0; a < NUMCODECS && codec < 0; a++ ) { 
            if (strcasecmp(codec_selected, tCodecs[a].longname )==0) codec = a; 
        }
    }
    
    if (codec > 0) set_mf_fps_widget_from_codec(codec); 
    
#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
#endif // HAVE_LIBAVCODEC
}


void
on_xvc_pref_commands_help_try_button_clicked(GtkButton * button, gpointer user_data)
{
    #define DEBUGFUNCTION "on_xvc_pref_commands_help_try_button_clicked()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);
    w = glade_xml_get_widget(xml, "xvc_pref_commands_help_entry");
    g_assert(w);

    char *curr_help_cmd = (char*) gtk_entry_get_text ( GTK_ENTRY(w) ); 
    if ( curr_help_cmd != NULL ) system((char*) curr_help_cmd ); 

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
}


void
on_xvc_pref_commands_sf_play_try_button_clicked(GtkButton * button, gpointer user_data)
{
    #define DEBUGFUNCTION "on_xvc_pref_commands_sf_play_try_button_clicked()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    w = glade_xml_get_widget(xml, "xvc_pref_commands_sf_play_entry");
    g_assert(w);

    char *curr_play_cmd = (char*) gtk_entry_get_text ( GTK_ENTRY( w ) ); 
    xvc_command_execute (curr_play_cmd, 1, 0, 
            pref_app.single_frame.file, pref_app.single_frame.start_no,
            (pref_app.single_frame.start_no+1), pref_app.cap_width,
            pref_app.cap_height, pref_app.single_frame.fps, 
            (int) (1000 / (pref_app.single_frame.fps/100)) ); 
    
#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
}


void
on_xvc_pref_commands_sf_encode_try_button_clicked(GtkButton * button, gpointer user_data)
{
    #define DEBUGFUNCTION "on_xvc_pref_commands_sf_encode_try_button_clicked()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);
    w = glade_xml_get_widget(xml, "xvc_pref_commands_sf_encode_entry");
    g_assert(w);

    char *curr_encode_cmd = (char*) gtk_entry_get_text ( GTK_ENTRY( w ) ); 
    xvc_command_execute (curr_encode_cmd, 0, 0, 
            pref_app.single_frame.file, pref_app.single_frame.start_no,
            (pref_app.single_frame.start_no+1), pref_app.cap_width,
            pref_app.cap_height, pref_app.single_frame.fps, 
            (int) (1000 / (pref_app.single_frame.fps/100)) ); 
    
#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
}


void
on_xvc_pref_commands_sf_edit_try_button_clicked(GtkButton * button, gpointer user_data)
{
    #define DEBUGFUNCTION "on_xvc_pref_commands_sf_edit_try_button_clicked()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);
    w = glade_xml_get_widget(xml, "xvc_pref_commands_sf_edit_entry");
    g_assert(w);

    char *curr_edit_cmd = (char*) gtk_entry_get_text ( GTK_ENTRY( w ) ); 
    xvc_command_execute (curr_edit_cmd, 2,
            pref_app.single_frame.start_no, pref_app.single_frame.file,
            pref_app.single_frame.start_no, (pref_app.single_frame.start_no+1), 
            pref_app.cap_width, pref_app.cap_height, pref_app.single_frame.fps, 
            (int) (1000 / (pref_app.single_frame.fps/100)) ); 
    
#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
}


void
on_xvc_pref_commands_mf_play_try_button_clicked(GtkButton * button, gpointer user_data)
{
#ifdef HAVE_LIBAVCODEC
    #define DEBUGFUNCTION "on_xvc_pref_commands_mf_play_try_button_clicked()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);
    w = glade_xml_get_widget(xml, "xvc_pref_commands_mf_play_entry");
    g_assert(w);

    char *curr_play_cmd = (char*) gtk_entry_get_text ( GTK_ENTRY( w ) ); 
    xvc_command_execute (curr_play_cmd, 2, 0,
            pref_app.multi_frame.file, pref_app.multi_frame.start_no,
            (pref_app.multi_frame.start_no+1), pref_app.cap_width,
            pref_app.cap_height, pref_app.multi_frame.fps, (int) (1000 /
            (pref_app.multi_frame.fps/100)) ); 
    
#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
#endif // HAVE_LIBAVCODEC
}


void
on_xvc_pref_commands_mf_encode_try_button_clicked(GtkButton * button, gpointer user_data)
{
#ifdef HAVE_LIBAVCODEC
    #define DEBUGFUNCTION "on_xvc_pref_commands_mf_encode_try_button_clicked()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);
    w = glade_xml_get_widget(xml, "xvc_pref_commands_mf_encode_entry");
    g_assert(w);

    char *curr_encode_cmd = (char*) gtk_entry_get_text ( GTK_ENTRY( w ) ); 
    xvc_command_execute (curr_encode_cmd, 2, 0, 
            pref_app.multi_frame.file, pref_app.multi_frame.start_no,
            (pref_app.multi_frame.start_no+1), pref_app.cap_width,
            pref_app.cap_height, pref_app.multi_frame.fps, (int) (1000 /
            (pref_app.multi_frame.fps/100)) ); 
    
#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
#endif // HAVE_LIBAVCODEC
}


void
on_xvc_pref_commands_mf_edit_try_button_clicked(GtkButton * button, gpointer user_data)
{
#ifdef HAVE_LIBAVCODEC
    #define DEBUGFUNCTION "on_xvc_pref_commands_mf_edit_try_button_clicked()"
    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
    g_assert(xml);
    w = glade_xml_get_widget(xml, "xvc_pref_commands_mf_edit_entry");
    g_assert(w);

    char *curr_edit_cmd = (char*) gtk_entry_get_text ( GTK_ENTRY( w ) ); 
    xvc_command_execute (curr_edit_cmd, 2, 0,
            pref_app.multi_frame.file, pref_app.multi_frame.start_no,
            (pref_app.multi_frame.start_no+1), pref_app.cap_width,
            pref_app.cap_height, pref_app.multi_frame.fps, (int) (1000 /
            (pref_app.multi_frame.fps/100)) ); 
    
#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
#endif // HAVE_LIBAVCODEC
}


void
on_xvc_pref_sf_filename_select_button_clicked(GtkButton * button, gpointer user_data)
{
    #define DEBUGFUNCTION "on_xvc_pref_sf_filename_select_button_clicked()"
    int result = 0; 
    char* got_file_name;
    
    GladeXML *xml = NULL;
    GtkWidget *w = NULL, *dialog = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    // load the interface
    xml = glade_xml_new(GLADE_FILE, "xvc_save_filechooserdialog", NULL);
    g_assert(xml);
    // connect the signals in the interface 
    glade_xml_signal_autoconnect(xml);

    dialog = glade_xml_get_widget(xml, "xvc_save_filechooserdialog");
    g_assert(dialog);
    gtk_window_set_title(GTK_WINDOW(dialog), "Save Frames As:");

    result = gtk_dialog_run (GTK_DIALOG (dialog));

    if ( result == GTK_RESPONSE_OK) { 
        guint length;
        gchar *path, *path_rev;
        
        got_file_name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog)); 
        
        xml = NULL;
        xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
        g_assert(xml);
        
        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_sf_filename_entry");
        g_assert(w);
        
        gtk_entry_set_text( GTK_ENTRY(w), strdup(got_file_name) ); 
    }
    
    gtk_widget_destroy (dialog);

#ifdef DEBUG
    printf("%s %s: Leaving with filename %s\n", DEBUGFILE, DEBUGFUNCTION, got_file_name);
#endif // DEBUG
    #undef DEBUGFUNCTION
}


void
on_xvc_pref_mf_filename_select_button_clicked(GtkButton * button, gpointer user_data)
{
    #define DEBUGFUNCTION "on_xvc_pref_mf_filename_select_button_clicked()"
    int result = 0; 
    char* got_file_name;
    
    GladeXML *xml = NULL;
    GtkWidget *w = NULL, *dialog = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    // load the interface
    xml = glade_xml_new(GLADE_FILE, "xvc_save_filechooserdialog", NULL);
    g_assert(xml);
    // connect the signals in the interface 
    glade_xml_signal_autoconnect(xml);

    dialog = glade_xml_get_widget(xml, "xvc_save_filechooserdialog");
    g_assert(dialog);
    gtk_window_set_title(GTK_WINDOW(dialog), "Save Video As:");

    result = gtk_dialog_run (GTK_DIALOG (dialog));

    if ( result == GTK_RESPONSE_OK) { 
        guint length;
        gchar *path, *path_rev;
        
        got_file_name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog)); 
        
        xml = NULL;
        xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
        g_assert(xml);
        
        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_filename_entry");
        g_assert(w);
        
        gtk_entry_set_text( GTK_ENTRY(w), strdup(got_file_name) ); 
    }
    
    gtk_widget_destroy (dialog);

#ifdef DEBUG
    printf("%s %s: Leaving with filename %s\n", DEBUGFILE, DEBUGFUNCTION, got_file_name);
#endif // DEBUG
    #undef DEBUGFUNCTION
}

void
on_xvc_pref_mf_audio_input_device_select_button_clicked(GtkButton * button,
                                     gpointer user_data)
{
    #define DEBUGFUNCTION "on_xvc_pref_mf_audio_input_device_select_button_clicked()"
    int result = 0; 
    char* got_file_name;
    
    GladeXML *xml = NULL;
    GtkWidget *w = NULL, *dialog = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    // load the interface
    xml = glade_xml_new(GLADE_FILE, "xvc_open_filechooserdialog", NULL);
    g_assert(xml);
    // connect the signals in the interface 
    glade_xml_signal_autoconnect(xml);

    dialog = glade_xml_get_widget(xml, "xvc_open_filechooserdialog");
    g_assert(dialog);
    gtk_window_set_title(GTK_WINDOW(dialog), "Capture Audio From:");

    result = gtk_dialog_run (GTK_DIALOG (dialog));

    if ( result == GTK_RESPONSE_OK) { 
        guint length;
        gchar *path, *path_rev;
        
        got_file_name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog)); 
        
        xml = NULL;
        xml = glade_get_widget_tree(GTK_WIDGET(xvc_pref_main_window));
        g_assert(xml);
        
        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_input_device_entry");
        g_assert(w);
        
        gtk_entry_set_text( GTK_ENTRY(w), strdup(got_file_name) ); 
    }
    
    gtk_widget_destroy (dialog);

#ifdef DEBUG
    printf("%s %s: Leaving with filename %s\n", DEBUGFILE, DEBUGFUNCTION, got_file_name);
#endif // DEBUG
    #undef DEBUGFUNCTION
}


void
xvc_create_pref_dialog(AppData * lapp)
{
    #define DEBUGFUNCTION "xvc_create_pref_dialog()"

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    GladeXML *xml = NULL;
    GtkWidget *w = NULL;

    // load the interface
    xml = glade_xml_new(GLADE_FILE, "xvc_pref_main_window", NULL);
    g_assert(xml);
    // connect the signals in the interface 
    glade_xml_signal_autoconnect(xml);

#ifdef DEBUG
    printf("%s %s: Connected signals preferences dialog\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG


    xvc_pref_main_window = glade_xml_get_widget(xml, "xvc_pref_main_window");
    g_assert(xvc_pref_main_window);

    // first copy the pass parameters to a temporary structure
    xvc_app_data_copy(&pref_app, lapp);

    sf_t_format = pref_app.single_frame.target;
    if (!sf_t_format)
        sf_t_format =
            xvc_codec_get_target_from_filename(pref_app.single_frame.file);
    sf_t_codec = pref_app.single_frame.targetCodec;
    if (!sf_t_codec)
        sf_t_codec = tFFormats[sf_t_format].def_vid_codec;
#ifdef HAVE_FFMPEG_AUDIO
    sf_t_au_codec = pref_app.single_frame.au_targetCodec;
    if (!sf_t_au_codec)
        sf_t_au_codec = tFFormats[sf_t_format].def_au_codec;
#endif // HAVE_FFMPEG_AUDIO

#ifdef HAVE_LIBAVCODEC
    mf_t_format = pref_app.multi_frame.target;
    if (!mf_t_format)
        mf_t_format =
            xvc_codec_get_target_from_filename(pref_app.multi_frame.file);
    mf_t_codec = pref_app.multi_frame.targetCodec;
    if (!mf_t_codec)
        mf_t_codec = tFFormats[mf_t_format].def_vid_codec;
#ifdef HAVE_FFMPEG_AUDIO
    mf_t_au_codec = pref_app.multi_frame.au_targetCodec;
    if (!mf_t_au_codec)
        mf_t_au_codec = tFFormats[mf_t_format].def_au_codec;
#endif // HAVE_FFMPEG_AUDIO
#endif // HAVE_LIBAVCODEC

#ifdef DEBUG
    printf("%s %s: Determined Formats sf/mf %i/%i and Codecs sf/mf %i/%i Audio Codecs sf/mf %i/%i\n", DEBUGFILE, DEBUGFUNCTION,
        sf_t_format, 
#ifdef HAVE_LIBAVCODEC
        mf_t_format, 
#else 
        0,
#endif // HAVE_LIBAVCODEC
        sf_t_codec,
#ifdef HAVE_LIBAVCODEC
        mf_t_codec, 
#else 
        0,
#endif // HAVE_LIBAVCODEC
        sf_t_au_codec
#ifdef HAVE_FFMPEG_AUDIO
        , mf_t_au_codec
#else 
        , 0
#endif // HAVE_FFMPEG_AUDIO
        );
#endif // DEBUG

    // prepare the arrays with the entries for the dynamic comboboxes 
    // FIXME: do the others need to go here?
    {
        int a;
        char buf[256];
        gchar *utf8text;
        GError *error = NULL;

        for (a = 1; a < NUMCAPS; a++) {
            sprintf(buf, "%s (%s)", tFFormats[a].longname,
                    xvc_next_element(tFFormats[a].extensions));
            utf8text =
                g_convert((gchar *) buf, strlen(buf), "UTF-8",
                          "ISO-8859-1", NULL, NULL, &error);

            if (error != NULL)
                printf
                    ("%s %s: couldn't convert format name to utf8\n", DEBUGFILE, DEBUGFUNCTION);

            format_combo_entries[a - 1] = strdup((char *) utf8text);
#ifdef DEBUG
            printf("%s %s: format_combo_entries %i : %s\n", DEBUGFILE, DEBUGFUNCTION,
                    (a-1), format_combo_entries[a-1]);
#endif
        }
    } 

#ifdef DEBUG
    printf("%s %s: Constructed strings for format comboboxes\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    // default capture mode radio buttons
#ifdef HAVE_LIBAVCODEC
    if (pref_app.default_mode == 0) {
        w = NULL;
        w = glade_xml_get_widget(xml,
                                 "xvc_pref_default_capture_mode_sf_radiobutton");
        if (w != NULL)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
    } else {
        w = NULL;
        w = glade_xml_get_widget(xml,
                                 "xvc_pref_default_capture_mode_mf_radiobutton");
        if (w != NULL)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
    }
#else // HAVE_LIBAVCODEC
    w = NULL;
    w = glade_xml_get_widget(xml,
                    "xvc_pref_default_capture_mode_sf_radiobutton");
    if (w != NULL) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(w), FALSE);
    }

    w = NULL;
    w = glade_xml_get_widget(xml,
                    "xvc_pref_default_capture_mode_mf_radiobutton");
    if (w != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(w), FALSE);
#endif // HAVE_LIBAVCODEC


    // mouse wanted radio buttons
    if (pref_app.mouseWanted == 1) {
        w = NULL;
        w = glade_xml_get_widget(xml,
                                 "xvc_pref_capture_mouse_white_radiobutton");
        if (w != NULL) 
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
    } else if (pref_app.mouseWanted == 2) {
        w = NULL;
        w = glade_xml_get_widget(xml,
                                 "xvc_pref_capture_mouse_black_radiobutton");
        if (w != NULL) 
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
    }
    if (pref_app.mouseWanted == 0) {
        w = NULL;
        w = glade_xml_get_widget(xml,
                                 "xvc_pref_capture_mouse_checkbutton");
        if (w != NULL)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), FALSE);

        w = NULL;
        w = glade_xml_get_widget(xml,
                                 "xvc_pref_capture_mouse_black_radiobutton");
        if (w != NULL)
            gtk_widget_set_sensitive(w, FALSE);
        w = NULL;
        w = glade_xml_get_widget(xml,
                                 "xvc_pref_capture_mouse_white_radiobutton");
        if (w != NULL)
            gtk_widget_set_sensitive(w, FALSE);
    } else {
        w = NULL;
        w = glade_xml_get_widget(xml,
                                 "xvc_pref_capture_mouse_checkbutton");
        if (w != NULL)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
    }


    // save geometry
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_save_geometry_checkbutton");
    if (w != NULL) {
        if ((pref_app.flags & FLG_SAVE_GEOMETRY) != 0) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
        } else {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), FALSE);
        }
    }

    // shared memory
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_shared_mem_checkbutton");
#ifdef HAVE_SHMAT
    if ((pref_app.flags & FLG_USE_SHM) != 0) {
        if (w != NULL)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
    } else {
        if (w != NULL)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), FALSE);
    }
#else
    if (w != NULL) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), FALSE);
        gtk_widget_set_sensitive(w, FALSE);
    }
#endif                          // HAVE_SHMAT


    // autocontinue
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_autocontinue_checkbutton");
    if ((pref_app.flags & FLG_AUTO_CONTINUE) != 0) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), FALSE);
    }

#ifdef DEBUG
    printf("%s %s: Set widgets for general tab\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG


    // 
    // single frame

    // fps
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_sf_fps_hscale");
    if (w != NULL) gtk_range_set_value(GTK_RANGE(w), (pref_app.single_frame.fps/100.00));

    // quality
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_sf_quality_hscale");
    if (w != NULL) gtk_range_set_value(GTK_RANGE(w), pref_app.single_frame.quality);
    
    // max time
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_sf_max_time_spinbutton");
    if (w != NULL) gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), pref_app.single_frame.time);

    // max frames
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_sf_max_frames_spinbutton");
    if (w != NULL) gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), pref_app.single_frame.frames);

    // start number
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_sf_start_no_spinbutton");
    if (w != NULL) gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), pref_app.single_frame.start_no);
    
    // frame increment
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_sf_frame_increment_spinbutton");
    if (w != NULL) gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), pref_app.single_frame.step);

    // format
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_sf_format_combobox");

    if (w != NULL) {
        int a, n;
        GtkWidget *check = NULL;
        GtkListStore *sf_format_list_store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(w)));
        // NOTE: for this to work ther must ALWAYS be a dummy entry present in glade
        // otherwise glade will not create a model at all (let alone a list store)
        g_assert(sf_format_list_store);
        gtk_list_store_clear(GTK_LIST_STORE(sf_format_list_store));

#ifdef HAVE_LIBAVCODEC
        for (a = 0; a < (CAP_MF - 1); a++)
#else
        for (a = 0; a < (NUMCAPS - 1); a++)
#endif                          // HAVE_LIBAVCODEC
        {
#ifdef DEBUG
            printf("%s %s: Adding this text (item %i) to sf format combobox %s\n", DEBUGFILE, DEBUGFUNCTION,
            a, format_combo_entries[a]);
#endif // DEBUG
        
//            sf_format_items =
//                g_list_append(sf_format_items, format_combo_entries[a]);
            if (strncasecmp(format_combo_entries[a], 
                    tFFormats[pref_app.single_frame.target].longname, 
                    strlen(tFFormats[pref_app.single_frame.target].longname) )==0) {
                n = a;
            }
            gtk_combo_box_append_text(GTK_COMBO_BOX(w), format_combo_entries[a]);
        }

        check =
            glade_xml_get_widget(xml,
                                 "xvc_pref_sf_format_auto_checkbutton");
        if (check != NULL) {
            if (pref_app.single_frame.target == 0) {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), TRUE);
                gtk_widget_set_sensitive(w, FALSE);
            } else {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), FALSE);
                gtk_widget_set_sensitive(w, TRUE);
                gtk_combo_box_set_active(GTK_COMBO_BOX(w), n);
            }
        }
    }

    // file
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_sf_filename_entry");

    if (w != NULL) {
        gtk_entry_set_text ( GTK_ENTRY( w ), strdup(pref_app.single_frame.file) );
    }


    //
    // multi_frame


#ifdef HAVE_LIBAVCODEC
    // fps
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_fps_hscale");
    if (w != NULL) gtk_range_set_value(GTK_RANGE(w), (pref_app.multi_frame.fps/100.00));

    // max time
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_max_time_spinbutton");
    if (w != NULL) gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), pref_app.multi_frame.time);
    
    // max frames
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_max_frames_spinbutton");
    if (w != NULL) gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), pref_app.multi_frame.frames);
    
    // quality
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_quality_hscale");
    if (w != NULL) gtk_range_set_value(GTK_RANGE(w), pref_app.multi_frame.quality);
     
    
#ifdef HAVE_FFMPEG_AUDIO    
    // audio check
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_checkbutton");
    if (w != NULL) {
        if ( pref_app.multi_frame.audioWanted > 0 ) 
            gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(w), TRUE);
        else
            gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(w), FALSE);
    }
    
    // audio codec
    mf_audio_codec_combo_set_contents_from_format(mf_t_format);
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_codec_auto_checkbutton");
    if (w != NULL) {
        if (pref_app.multi_frame.au_targetCodec == 0) {
            GtkWidget *au_codec_combob = NULL;
            au_codec_combob = glade_xml_get_widget(xml, "xvc_pref_mf_audio_codec_combobox");
            
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
            if (au_codec_combob != NULL) gtk_widget_set_sensitive(au_codec_combob, FALSE);
        } else {
            GtkWidget *au_codec_combob = NULL;
            au_codec_combob = glade_xml_get_widget(xml, "xvc_pref_mf_audio_codec_combobox");

            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), FALSE);
            if (au_codec_combob != NULL) {
                GtkListStore *list_store = NULL;
                gboolean valid;
                GtkTreeIter iter;
                int a = 0;
                
                gtk_widget_set_sensitive(au_codec_combob, TRUE);
                
                list_store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(au_codec_combob)));
                g_assert(list_store);

                valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL(list_store), &iter);

                while (valid) {
                    gchar *str_data;

                // Make sure you terminate calls to gtk_tree_model_get()
                // with a '-1' value
                    gtk_tree_model_get (GTK_TREE_MODEL(list_store), &iter, 
                            0, &str_data,
                            -1);

                    if (strcasecmp(str_data, tAuCodecs[pref_app.multi_frame.au_targetCodec].name)==0) {
                        gtk_combo_box_set_active(GTK_COMBO_BOX(au_codec_combob), a);
                    }
                    g_free (str_data);

                    valid = gtk_tree_model_iter_next (GTK_TREE_MODEL(list_store), &iter);
                    a++;
                }
            }
        }
    }

    // audio sample rate
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_sample_rate_spinbutton");
    if (w != NULL) gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), pref_app.multi_frame.sndrate);

    // audio bit rate
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_bit_rate_spinbutton");
    if (w != NULL) gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), pref_app.multi_frame.sndsize);

    // audio capture device
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_input_device_entry");

    if (w != NULL) {
        gtk_entry_set_text ( GTK_ENTRY( w ), strdup(pref_app.snddev) );
    }

    // audio channels
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_audio_channels_hscale");
    if (w != NULL) gtk_range_set_value(GTK_RANGE(w), pref_app.multi_frame.sndchannels);
#else // HAVE_FFMPEG_AUDIO    
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_nb_tab_multi_frame_audio_frame");

    if (w != NULL) 
    {
        gint f, e, p;
        GtkPackType pt;
        GtkWidget *container = NULL;
        
        gtk_widget_hide(GTK_WIDGET(w));

        container = glade_xml_get_widget(xml, "xvc_pref_nb_tab_multi_frame_vbox");
        if ( container != NULL ) gtk_container_remove(GTK_CONTAINER(container), GTK_WIDGET(w));

        w = NULL;
        w = glade_xml_get_widget(xml, "xvc_pref_nb_tab_multi_frame_video_frame");
        g_assert(w);

        if (container != NULL && w != NULL) { 
            gtk_box_query_child_packing (GTK_BOX(container),
                    GTK_WIDGET(w), &e, &f, &p, &pt);
            gtk_box_set_child_packing (GTK_BOX(container), 
                    GTK_WIDGET(w), (gboolean) TRUE, (gboolean) TRUE, p, pt);
        }
    }
#endif // HAVE_FFMPEG_AUDIO

    // codec
    mf_codec_combo_set_contents_from_format(mf_t_format);
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_codec_auto_checkbutton");
    if (w != NULL) {
        if (pref_app.multi_frame.targetCodec == 0) {
            GtkWidget *codec_combob = NULL;
            codec_combob = glade_xml_get_widget(xml, "xvc_pref_mf_codec_combobox");
            
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
            if (codec_combob != NULL) gtk_widget_set_sensitive(codec_combob, FALSE);
        } else {
            GtkWidget *codec_combob = NULL;
            codec_combob = glade_xml_get_widget(xml, "xvc_pref_mf_codec_combobox");

            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), FALSE);
            if (codec_combob != NULL) {
                GtkListStore *list_store = NULL;
                gboolean valid;
                GtkTreeIter iter;
                int a = 0;
                
                gtk_widget_set_sensitive(codec_combob, TRUE);
                
                list_store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(codec_combob)));
                g_assert(list_store);

                valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL(list_store), &iter);

                while (valid) {
                    gchar *str_data;

                // Make sure you terminate calls to gtk_tree_model_get()
                // with a '-1' value
                    gtk_tree_model_get (GTK_TREE_MODEL(list_store), &iter, 
                            0, &str_data,
                            -1);

                    if (strcasecmp(str_data, tCodecs[pref_app.multi_frame.targetCodec].name)==0) {
                        gtk_combo_box_set_active(GTK_COMBO_BOX(codec_combob), a);
                    }
                    g_free (str_data);

                    valid = gtk_tree_model_iter_next (GTK_TREE_MODEL(list_store), &iter);
                    a++;
                }
            }
        }
    }
    
    // format
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_format_combobox");

    if (w != NULL) {
        int a, n;
        GtkWidget *check = NULL;
        GtkListStore *mf_format_list_store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(w)));
        // NOTE: for this to work ther must ALWAYS be a dummy entry present in glade
        // otherwise glade will not create a model at all (let alone a list store)
        g_assert(mf_format_list_store);
        gtk_list_store_clear(GTK_LIST_STORE(mf_format_list_store));

        for (a = (CAP_MF - 1); a < (NUMCAPS - 1); a++) {
#ifdef DEBUG
            printf("%s %s: Adding this text (item %i) to mf format combobox %s\n", DEBUGFILE, DEBUGFUNCTION,
            a, format_combo_entries[a]);
#endif // DEBUG
        
            if (strncasecmp(format_combo_entries[a], 
                    tFFormats[mf_t_format].longname, 
                    strlen(tFFormats[mf_t_format].longname) )==0) {
                n = a - CAP_MF + 1;
            }
            gtk_combo_box_append_text(GTK_COMBO_BOX(w), format_combo_entries[a]);
        }

        check =
            glade_xml_get_widget(xml,
                                 "xvc_pref_mf_format_auto_checkbutton");
        if (check != NULL) {
            if (pref_app.multi_frame.target == 0) {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), TRUE);
                gtk_widget_set_sensitive(w, FALSE);
            } else {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), FALSE);
                gtk_widget_set_sensitive(w, TRUE);
                gtk_combo_box_set_active(GTK_COMBO_BOX(w), n);
            }
        }
    }

    // file
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_mf_filename_entry");

    if (w != NULL) {
        gtk_entry_set_text ( GTK_ENTRY( w ), strdup(pref_app.multi_frame.file) );
    }

#else // no HAVE_LIBAVCODEC
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_nb_tab_multi_frame_vbox");
    g_assert(w);
    gtk_widget_hide(GTK_WIDGET(w));

    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_nb_tab_single_frame_label");
    g_assert(w);
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(w), _("C_apture"));
#endif // HAVE_LIBAVCODEC



    // help command
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_commands_help_entry");

    if (w != NULL) {
        gtk_entry_set_text ( GTK_ENTRY( w ), strdup(pref_app.help_cmd) );
    }
    
    // sf play command
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_commands_sf_play_entry");

    if (w != NULL) {
        gtk_entry_set_text ( GTK_ENTRY( w ), strdup(pref_app.single_frame.play_cmd) );
    }
     
    // sf encoding command
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_commands_sf_encode_entry");

    if (w != NULL) {
        gtk_entry_set_text ( GTK_ENTRY( w ), strdup(pref_app.single_frame.video_cmd) );
    }
    
    // sf edit command
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_commands_sf_edit_entry");

    if (w != NULL) {
        gtk_entry_set_text ( GTK_ENTRY( w ), strdup(pref_app.single_frame.edit_cmd) );
    }
    
#ifdef HAVE_LIBAVCODEC
    // mf play command
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_commands_mf_play_entry");

    if (w != NULL) {
        gtk_entry_set_text ( GTK_ENTRY( w ), strdup(pref_app.multi_frame.play_cmd) );
    }
     
    // mf encoding command
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_commands_mf_encode_entry");

    if (w != NULL) {
        gtk_entry_set_text ( GTK_ENTRY( w ), strdup(pref_app.multi_frame.video_cmd) );
    }
    
    // mf edit command
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_commands_mf_edit_entry");

    if (w != NULL) {
        gtk_entry_set_text ( GTK_ENTRY( w ), strdup(pref_app.multi_frame.edit_cmd) );
    }
#else // HAVE_LIBAVCODEC
    w = NULL;
    w = glade_xml_get_widget(xml, "xvc_pref_nb_tab_commands_mf_frame");
    g_assert(w);
    gtk_widget_hide(GTK_WIDGET(w));
#endif // HAVE_LIBAVCODEC


#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    #undef DEBUGFUNCTION
}

