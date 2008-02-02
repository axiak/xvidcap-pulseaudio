/**
 * \file main.c
 */
/*
 * Copyright (C) 1997-98 Rasca, Berlin
 * Copyright (C) 2003-07 Karl H. Beckers, Frankfurt
 *
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2, or (at your option) any later version.
 *
 * main.c is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with main.c. See the file "COPYING". If not,
 * write to: The Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define DEBUGFILE "main.c"
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <libintl.h>
#include <ctype.h>
#include <pthread.h>
#include <signal.h>
#include <math.h>
#include <locale.h>

#ifdef USE_FFMPEG
#include <ffmpeg/avcodec.h>
#endif     // USE_FFMPEG

#include "control.h"
#include "codecs.h"
#include "job.h"
#include "frame.h"
#include "xvidcap-intl.h"

typedef void (*sighandler_t) (int);

/*
 * GLOBAL VARIABLES
 */
static XVC_AppData *app;
static Window capture_window = None;

/*
 * HELPER FUNCTIONS
 */
/**
 * \brief displays usage information
 *
 * @param prog string containing the program as it was called
 */
void
usage (char *prog)
{
    int n, m;

    printf
        (_
         ("Usage: %s, ver %s, (c) rasca, berlin 1997,98,99, khb (c) 2003-07\n"),
         prog, VERSION);
    printf (_
            ("[--fps #.#]      frames per second (float) or a fraction string like \"30000/1001\"\n"));
    printf (_("[--verbose #]    verbose level, '-v' is --verbose 1\n"));
    printf (_("[--time #.#]     time to record in seconds (float)\n"));
    printf (_("[--frames #]     frames to record, don't use it with --time\n"));
    printf (_
            ("[--continue [yes|no]] autocontinue after maximum frames/time\n"));
    printf (_
            ("[--cap_geometry #x#[+#+#]] size of the capture window (WIDTHxHEIGHT+X+Y)\n"));
    printf (_
            ("[--window <hex-window-id>] a hexadecimal window id (ref. xwininfo)\n"));
    printf (_
            ("[--rescale #]    relative output size in percent compared to input (1-100)\n"));
    printf (_("[--quality #]    recording quality (1-100)\n"));
    printf (_("[--start_no #]   start number for the file names\n"));
#ifdef HAVE_SHMAT
    printf (_("[--source <src>] select input source: x11, shm\n"));
#endif     // HAVE_SHMAT
    printf (_("[--file <file>]  file pattern, e.g. out%%03d.xwd\n"));
    printf (_("[--gui [yes|no]] turn on/off gui\n"));
#ifdef USE_FFMPEG
    printf (_
            ("[--sf|--mf]      request single-frame or multi-frame capture mode\n"));
#endif     // USE_FFMPEG
    printf
        (_
         ("[--auto]         cause auto-detection of output format, video-, and audio codec\n"));
    printf (_
            ("[--codec <codec>] specify codec to use for multi-frame capture\n"));
    printf (_
            ("[--codec-help]   list available codecs for multi-frame capture\n"));
    printf (_
            ("[--format <format>] specify file format to override the extension in the filename\n"));
    printf (_("[--format-help]  list available file formats\n"));
#ifdef HAVE_FFMPEG_AUDIO
    printf
        (_
         ("[--aucodec <codec>] specify audio codec to use for multi-frame capture\n"));
    printf (_
            ("[--aucodec-help] list available audio codecs for multi-frame capture\n"));
    printf (_("[--audio [yes|no]] turn on/off audio capture\n"));
    printf
        (_
         ("[--audio_in <src>] specify audio input device or '-' for pipe input\n"));
    printf (_("[--audio_rate #] sample rate for audio capture\n"));
    printf (_("[--audio_bits #] bit rate for audio capture\n"));
    printf (_("[--audio_channels #] number of audio channels\n"));
#endif     // HAVE_FFMPEG_AUDIO

    printf (_("Supported output formats:\n"));
    for (n = CAP_NONE; n < NUMCAPS; n++) {
        if (xvc_formats[n].extensions) {
            printf (" %s", _(xvc_formats[n].longname));
            for (m = strlen (_(xvc_formats[n].longname)); m < 40; m++)
                printf (" ");
            printf ("(");
            for (m = 0; m < xvc_formats[n].num_extensions; m++) {
                printf (".%s", xvc_formats[n].extensions[m]);
                if (xvc_formats[n].num_extensions > (m + 1))
                    printf (", ");
            }
            printf (")\n");
        }
    }
    exit (1);
}

/**
 * \brief does initialization
 *
 * This does the gtk_init stuff, initializes the global preferences variables,
 * and removes any argv values gtk_init might have NULLed
 * @param ctos pointer to the XVC_CapTypeOptions struct for the current type
 *      of capture (sf vs. mf)
 * @param argc number of cli args
 * @param argv the array of command line options
 */
static void
init (XVC_CapTypeOptions * ctos, int *argc, char *argv[])
{
#define DEBUGFUNCTION "init()"
    int i;

    app = xvc_appdata_ptr ();

    // Xlib threading initialization
    // this is here instead of with the gtk/glib stuff in gnome_ui.c|h
    // because this is UI independant and would need to be here even with Qt
    XInitThreads ();

    // this is a hook for a GUI to do some pre-init functions ...
    // possibly to set some fallback options read from a rc file or
    // Xdefaults
    if (!xvc_init_pre (*argc, argv)) {
        fprintf (stderr,
                 _("%s %s: can't do GUI pre-initialization ... aborting\n"),
                 DEBUGFILE, DEBUGFUNCTION);
        exit (2);
    }
    //
    // some variable initialization
    //
    xvc_appdata_set_defaults (app);
    xvc_captypeoptions_init (ctos);

    // gtk_init may replace argv values with NULL ... that's bad for
    // getopt_long
    for (i = *argc; i > 0; i--) {
        int j;

        //printf("arg %i = %s\n", i-1, argv[i-1]);
        if (argv[i - 1] == NULL) {
            if (i < *argc) {
                for (j = i; j < *argc; j++) {
                    argv[j - 1] = argv[j];
                }
            }
            *argc = *argc - 1;
        }
    }
#undef DEBUGFUNCTION
}

/**
 * \brief cleans up after the program
 */
static void
cleanup ()
{
    Job *job = xvc_job_ptr ();

    if (job)
        xvc_job_free ();

    xvc_frame_drop_capture_display ();

    if (app)
        xvc_appdata_free (app);
#ifdef USE_FFMPEG
    av_free_static ();
#endif
}

/**
 * \brief sets an XVC_CapTypeOptions struct according to the cli arguments
 *
 * @param tmp_capture_options pointer to a pre-existing XVC_CapTypeOptions
 *      struct to be set
 * @param argc number of cli arguments
 * @param _argv array of cli arguments
 */
static void
parse_cli_options (XVC_CapTypeOptions * tmp_capture_options, int argc,
                   char *_argv[])
{
#undef DEBUGFUNCTION
#define DEBUGFUNCTION "parse_cli_options()"
    struct option options[] = {
        {"fps", required_argument, NULL, 0},
        {"file", required_argument, NULL, 0},
        {"verbose", required_argument, NULL, 0},
        {"time", required_argument, NULL, 0},
        {"frames", required_argument, NULL, 0},
        {"cap_geometry", required_argument, NULL, 0},
        {"start_no", required_argument, NULL, 0},
        {"quality", required_argument, NULL, 0},
        {"source", required_argument, NULL, 0},
        {"step", required_argument, NULL, 0},
        {"gui", optional_argument, NULL, 0},
        {"audio", optional_argument, NULL, 0},
        {"audio_in", required_argument, NULL, 0},
        {"audio_rate", required_argument, NULL, 0},
        {"audio_bits", required_argument, NULL, 0},
        {"audio_channels", required_argument, NULL, 0},
        {"continue", optional_argument, NULL, 0},
        {"sf", no_argument, NULL, 0},
        {"mf", no_argument, NULL, 0},
        {"codec", required_argument, NULL, 0},
        {"codec-help", no_argument, NULL, 0},
        {"format", required_argument, NULL, 0},
        {"format-help", no_argument, NULL, 0},
        {"aucodec", required_argument, NULL, 0},
        {"aucodec-help", no_argument, NULL, 0},
        {"auto", no_argument, NULL, 0},
        {"rescale", required_argument, NULL, 0},
        {"window", required_argument, NULL, 0},
        {NULL, 0, NULL, 0},
    };
    int opt_index = 0, c;

    while ((c = getopt_long (argc, _argv, "v", options, &opt_index)) != -1) {
        switch (c) {
        case 0:                       // it's a long option
            switch (opt_index) {
            case 0:                   // fps
                tmp_capture_options->fps = xvc_read_fps_from_string (optarg);
                break;
            case 1:                   // file
                tmp_capture_options->file = strdup (optarg);
                break;
            case 2:
                app->verbose = atoi (optarg);
                break;
            case 3:                   // max_time
                tmp_capture_options->time = atoi (optarg);
                break;
            case 4:                   // max_frames
                tmp_capture_options->frames = atoi (optarg);
                break;
            case 5:                   // cap_geometry
                sscanf (optarg, "%hux%hu+%hi+%hi", &(app->area->width),
                        &(app->area->height), &(app->area->x), &(app->area->y));
                break;
            case 6:                   // start_no
                tmp_capture_options->start_no = atoi (optarg);
                break;
            case 7:                   // quality
                tmp_capture_options->quality = atoi (optarg);
                break;
            case 8:                   // source
#ifdef HAVE_SHMAT
                app->source = strdup (optarg);
#else
                fprintf (stderr,
                         _
                         ("Only 'x11' is supported as a capture source with this binary.\n"));
                usage (_argv[0]);
#endif     // HAVE_SHMAT
                break;
            case 9:                   // step_no
                tmp_capture_options->step = atoi (optarg);
                break;
            case 10:                  // gui
                {
                    char *tmp;

                    if (!optarg) {
                        if (optind < argc) {
                            tmp =
                                (_argv[optind][0] ==
                                 '-') ? "yes" : _argv[optind++];
                        } else {
                            tmp = "yes";
                        }
                    } else {
                        tmp = strdup (optarg);
                    }
                    if (strstr (tmp, "no") != NULL) {
                        app->flags |= FLG_NOGUI;
                    } else {
                        app->flags &= ~FLG_NOGUI;
                    }
                }
                break;
            case 11:                  // audio
#ifdef HAVE_FFMPEG_AUDIO
                {
                    char *tmp;

                    if (!optarg) {
                        if (optind < argc) {
                            tmp =
                                (_argv[optind][0] ==
                                 '-') ? "yes" : _argv[optind++];
                        } else {
                            tmp = "yes";
                        }
                    } else {
                        tmp = strdup (optarg);
                    }
                    if (strstr (tmp, "no") != NULL) {
                        tmp_capture_options->audioWanted = 0;
                    } else {
                        tmp_capture_options->audioWanted = 1;
                    }
                }
#else
                fprintf (stderr,
                         _("Audio support not present in this binary.\n"));
                usage (_argv[0]);
#endif     // HAVE_FFMPEG_AUDIO
                break;
            case 12:                  // audio_in
#ifdef HAVE_FFMPEG_AUDIO
                tmp_capture_options->audioWanted = 1;
                app->snddev = strdup (optarg);
#else
                fprintf (stderr,
                         _("Audio support not present in this binary.\n"));
                usage (_argv[0]);
#endif     // HAVE_FFMPEG_AUDIO
                break;
            case 13:                  // audio_rate
#ifdef HAVE_FFMPEG_AUDIO
                tmp_capture_options->audioWanted = 1;
                tmp_capture_options->sndrate = atoi (optarg);
#else
                fprintf (stderr,
                         _("Audio support not present in this binary.\n"));
                usage (_argv[0]);
#endif     // HAVE_FFMPEG_AUDIO
                break;
            case 14:                  // audio_bits
#ifdef HAVE_FFMPEG_AUDIO
                tmp_capture_options->audioWanted = 1;
                tmp_capture_options->sndsize = atoi (optarg);
#else
                fprintf (stderr,
                         _("Audio support not present in this binary.\n"));
                usage (_argv[0]);
#endif     // HAVE_FFMPEG_AUDIO
                break;
            case 15:                  // audio_channels
#ifdef HAVE_FFMPEG_AUDIO
                tmp_capture_options->audioWanted = 1;
                tmp_capture_options->sndchannels = atoi (optarg);
#else
                fprintf (stderr,
                         _("Audio support not present in this binary.\n"));
                usage (_argv[0]);
#endif     // HAVE_FFMPEG_AUDIO
                break;
            case 16:                  // continue
                {
                    char *tmp;

                    if (!optarg) {
                        if (optind < argc) {
                            tmp =
                                (_argv[optind][0] ==
                                 '-') ? "yes" : _argv[optind++];
                        } else {
                            tmp = "yes";
                        }
                    } else {
                        tmp = strdup (optarg);
                    }
                    if (strstr (tmp, "no") != NULL) {
                        app->flags &= ~FLG_AUTO_CONTINUE;
                    } else {
                        app->flags |= FLG_AUTO_CONTINUE;
                    }
                }
                break;
#ifdef USE_FFMPEG
            case 17:                  // single-frame
                app->current_mode = 0;
                break;
            case 18:                  // multi-frame
                app->current_mode = 1;
                break;
#endif     // USE_FFMPEG
            case 19:                  // codec
                {
                    int n, m = 0;
                    Boolean found = FALSE;

                    for (n = 1; n < NUMCODECS; n++) {
                        if (strcasecmp (optarg, xvc_codecs[n].name) == 0) {
                            found = TRUE;
                            m = n;
                        }
                    }
                    if (strcasecmp (optarg, "auto") == 0) {
                        found = TRUE;
                        m = 0;         // set to CODEC_NONE which will get
                        // overridden later
                    }

                    if (found) {
                        tmp_capture_options->targetCodec = m;
                    } else {
                        usage (_argv[0]);
                    }
                }
                break;
            case 20:                  // codec-help
                {
                    int n, m;
                    char tmp_codec[512];

                    printf
                        (_
                         ("%s, ver %s, (c) rasca, berlin 1997,98,99, khb (c) 2003-07\n"),
                         _argv[0], VERSION);

#ifdef USE_FFMPEG
                    printf (_("Available codecs for single-frame capture: "));

                    for (n = 1; n < CAP_MF; n++) {
                        for (m = 0; m < strlen (xvc_codecs[n].name); m++) {
                            tmp_codec[m] = tolower (xvc_codecs[n].name[m]);
                        }
                        tmp_codec[strlen (xvc_codecs[n].name)] = '\0';
                        printf ("%s", tmp_codec);
                        if (n < (CAP_MF - 1))
                            printf (", ");
                    }
                    printf ("\n");

                    printf (_("Available codecs for multi-frame capture: "));

                    for (n = CAP_MF; n < NUMCODECS; n++) {
                        for (m = 0; m < strlen (xvc_codecs[n].name); m++) {
                            tmp_codec[m] = tolower (xvc_codecs[n].name[m]);
                        }
                        tmp_codec[strlen (xvc_codecs[n].name)] = '\0';
                        printf ("%s", tmp_codec);
                        if (n < (NUMCODECS - 1))
                            printf (", ");
                    }
                    printf ("\n");
#else
                    printf (_("Available codecs for single-frame capture: "));

                    for (n = 1; n < NUMCODECS; n++) {
                        for (m = 0; m < strlen (xvc_codecs[n].name); m++) {
                            tmp_codec[m] = tolower (xvc_codecs[n].name[m]);
                        }
                        tmp_codec[strlen (xvc_codecs[n].name)] = '\0';
                        printf ("%s", tmp_codec);
                        if (n < (NUMCODECS - 1))
                            printf (", ");
                    }
                    printf ("\n");
#endif     // USE_FFMPEG

                    printf
                        (_
                         ("Specify 'auto' to use the file format's default codec.\n"));
                    exit (1);
                }
                break;
            case 21:                  // format
                {
                    int n, m = -1;
                    Boolean found = FALSE;

                    for (n = CAP_NONE; n < NUMCAPS; n++) {
                        if (strcasecmp (optarg, xvc_formats[n].name) == 0) {
                            found = TRUE;
                            m = n;
                        }
                    }
                    if (strcasecmp (optarg, "auto") == 0) {
                        found = TRUE;
                        m = 0;
                    }

                    if (found) {
                        if (m > -1) {
                            tmp_capture_options->target = m;
                        } else {
                            fprintf (stderr,
                                     _
                                     ("Specified file format '%s' not supported with this binary."),
                                     optarg);
                            fprintf (stderr,
                                     _
                                     ("Resetting to file format auto-detection.\n"));
                            tmp_capture_options->target = 0;
                        }
                    } else {
                        fprintf (stderr,
                                 _("Unknown file format '%s' specified.\n"),
                                 optarg);
                        usage (_argv[0]);
                    }
                }
                break;
            case 22:                  // format-help
                {
                    int n, m;
                    char tmp_format[512];

                    printf
                        (_
                         ("%s, ver %s, (c) rasca, berlin 1997,98,99, khb (c) 2003-07\n"),
                         _argv[0], VERSION);
                    printf (_("Available file formats: "));

                    for (n = CAP_NONE; n < NUMCAPS; n++) {
                        if (xvc_formats[n].extensions) {
                            for (m = 0; m < strlen (xvc_formats[n].name); m++) {
                                tmp_format[m] =
                                    tolower (xvc_formats[n].name[m]);
                            }
                            tmp_format[strlen (xvc_formats[n].name)] = '\0';
                            printf ("%s", tmp_format);
                            if (n < (NUMCAPS - 1))
                                printf (", ");
                        }
                    }
                    printf ("\n");
                    printf
                        (_
                         ("Specify 'auto' to force file format auto-detection.\n"));
                    exit (1);
                }
                break;
            case 23:                  // aucodec
#ifdef HAVE_FFMPEG_AUDIO
                {
                    int n, m = 0;
                    Boolean found = FALSE;

                    for (n = 1; n < NUMAUCODECS; n++) {
                        if (strcasecmp (optarg, xvc_audio_codecs[n].name) == 0) {
                            found = TRUE;
                            m = n;
                        }
                    }
                    if (strcasecmp (optarg, "auto") == 0) {
                        found = TRUE;
                        m = 0;         // set to CODEC_NONE which will get
                        // overridden later
                    }

                    if (found) {
                        tmp_capture_options->au_targetCodec = m;
                    } else {
                        usage (_argv[0]);
                    }
                }
#else
                fprintf (stderr,
                         _("Audio support not present in this binary.\n"));
                usage (_argv[0]);
#endif     // HAVE_FFMPEG_AUDIO
                break;
            case 24:                  // aucodec-help
#ifdef HAVE_FFMPEG_AUDIO
                {
                    int n, m;
                    char tmp_codec[512];

                    printf
                        (_
                         ("%s, ver %s, (c) rasca, berlin 1997,98,99, khb (c) 2003-07\n"),
                         _argv[0], VERSION);
                    printf (_
                            ("Available audio codecs for multi-frame capture: "));

                    for (n = 1; n < NUMAUCODECS; n++) {
                        for (m = 0; m < strlen (xvc_audio_codecs[n].name); m++) {
                            tmp_codec[m] =
                                tolower (xvc_audio_codecs[n].name[m]);
                        }
                        tmp_codec[strlen (xvc_audio_codecs[n].name)] = '\0';
                        printf ("%s", tmp_codec);
                        if (n < (NUMAUCODECS - 1))
                            printf (", ");
                    }
                    printf ("\n");
                    printf
                        (_
                         ("Specify 'auto' to use the file format's default audio codec.\n"));
                    exit (1);
                }
#else
                fprintf (stderr,
                         _("Audio support not present in this binary.\n"));
                usage (_argv[0]);
#endif     // HAVE_FFMPEG_AUDIO
                break;
            case 25:                  // auto
                tmp_capture_options->target = 0;
                tmp_capture_options->targetCodec = 0;
#ifdef HAVE_FFMPEG_AUDIO
                tmp_capture_options->au_targetCodec = 0;
#endif     // HAVE_FFMPEG_AUDIO
                break;
            case 26:                  // rescale
                app->rescale = atoi (optarg);
                break;
            case 27:
                {
                    unsigned int win_id = 0;

                    sscanf (optarg, "%X", &win_id);
                    capture_window = (Window) win_id;
                    break;
                }
            default:
                usage (_argv[0]);
                break;
            }
            break;
            // now it's a short option
        case 'v':
            app->verbose++;
            break;

        default:
            usage (_argv[0]);
            break;
        }
    }
    if (argc != optind)
        usage (_argv[0]);

#undef DEBUGFUNCTION
}

/**
 * \brief merges an XVC_CapTypeOptions struct into the current preferences
 *
 * @param tmp_capture_options the XVC_CapTypeOptions to assimilate
 * @return a pointer to the XVC_CapTypeOptions within the current preferences,
 *      i.e. the global XVC_AppData struct representing the currently
 *      active capture mode
 */
static XVC_CapTypeOptions *
merge_cli_options (XVC_CapTypeOptions * tmp_capture_options)
{
#define DEBUGFUNCTION "merge_cli_options()"
    int current_mode_by_filename = -1, current_mode_by_target = -1;
    XVC_CapTypeOptions *target;

    // evaluate tmp_capture_options and set real ones accordingly

    // determine capture mode
    // either explicitly set in cli options or default_mode in options
    // (prefs/xdefaults), the latter does not need special action because
    // it will have been set by now or fall back to the
    // default set in init_app_data.

    // app->current_mode is either -1 (not explicitly specified), 0
    // (single_frame), or 1 (multi_frame) ...
    // setting current_mode_by_* accordingly

    // by filename
    if (tmp_capture_options->file == NULL)
        current_mode_by_filename = -1;
    else {
        current_mode_by_filename =
            xvc_codec_get_target_from_filename (tmp_capture_options->file);
#ifdef USE_FFMPEG
        if (current_mode_by_filename >= CAP_MF)
            current_mode_by_filename = 1;
        else
#endif     // USE_FFMPEG
        if (current_mode_by_filename > 0)
            current_mode_by_filename = 0;
        // if we can't determine capture type by filename treat as not
        // set for all practical purposes and leave error handling to
        // error handling
        else
            current_mode_by_filename = -1;
    }

    // by target
    if (tmp_capture_options->target < 0)
        current_mode_by_target = -1;
    else {
#ifdef USE_FFMPEG
        if (tmp_capture_options->target >= CAP_MF)
            current_mode_by_target = 1;
        else
#endif     // USE_FFMPEG
        if (tmp_capture_options->target > 0)
            current_mode_by_target = 0;
        // if we have an invalide target treat as not
        // set for all practical purposes and leave error handling to
        // error handling
        else
            current_mode_by_target = -1;
    }

    if (app->current_mode >= 0) {
        if (app->current_mode == 0)
            target = &(app->single_frame);
        else
            target = &(app->multi_frame);
    } else {
        app->current_mode = app->default_mode;
        // in case default_mode is not set correctly (will trigger error
        // later)
        if (app->current_mode < 0 || app->current_mode > 1)
            app->current_mode = 0;
        // preferences defaults are overridden by filename
        if (current_mode_by_filename >= 0)
            app->current_mode = current_mode_by_filename;
        // filename is overridden by format
        if (current_mode_by_target >= 0)
            app->current_mode = current_mode_by_target;

        if (app->current_mode == 0)
            target = &(app->single_frame);
        else
            target = &(app->multi_frame);
    }

    // merge cli options with app
    xvc_appdata_merge_captypeoptions (tmp_capture_options, app);

    return target;
#undef DEBUGFUNCTION
}

/**
 * \brief function to add a signal handler
 *
 * This is in this function because for multi-platform compatibility we
 * do not rely on special signal flags to automatically reregister a
 * handler after execution but instead add it again if needed within the
 * signal handler.
 */
static sighandler_t
my_signal_add (int sig_nr, sighandler_t sighandler)
{
    struct sigaction neu_sig, alt_sig;

    neu_sig.sa_handler = sighandler;
    sigemptyset (&neu_sig.sa_mask);
    neu_sig.sa_flags = SA_RESETHAND;
    if (sigaction (sig_nr, &neu_sig, &alt_sig) < 0)
        return SIG_ERR;
    return alt_sig.sa_handler;
}

/**
 * \brief implements the signal handler
 *
 * @param signal the signal number to handle
 */
static void
xvc_signal_handler (int signal)
{
#define DEBUGFUNCTION "xvc_signal_handler()"
    Job *job = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering with thread %i sigal %i\n",
            DEBUGFILE, DEBUGFUNCTION, (int) pthread_self (), (int) signal);
#endif     // DEBUG

    switch (signal) {
    case SIGINT:
        job = xvc_job_ptr ();
        if (job)
            xvc_capture_stop_signal (TRUE);
        cleanup ();
        exit (0);
        break;
    case SIGALRM:
        my_signal_add (SIGALRM, xvc_signal_handler);
        break;
    default:
        break;
    }

#ifdef DEBUG
    printf ("%s %s: Leaving with signal %i\n", DEBUGFILE, DEBUGFUNCTION,
            signal);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

/**
 * \brief prints the current settings, i.e. the stuff in the global
 *      XVC_AppData struct but only stepping into the XVC_CapTypeOptions
 *      for the currently active capture mode (sf vs. mf).
 *
 * Output is to stdout for debbuging purposes
 * @param target pointer to the XVC_CapTypeOptions representing the
 *      currently active capture mode within the global XVC_AppData struct
 * \todo make the options output take new app_data structure with
 *      parallel capTypeOptions into account
 */
static void
print_current_settings (XVC_CapTypeOptions * target)
{
    char *mp;

    switch (app->mouseWanted) {
    case 2:
        mp = _("black");
        break;
    case 1:
        mp = _("white");
        break;
    default:
        mp = _("none");
    }

    printf (_("Current settings:\n"));
    printf (_(" flags = %d\n"), app->flags);
#ifdef USE_FFMPEG
    printf (_(" capture mode = %s\n"),
            ((app->current_mode == 0) ? _("single-frame") : _("multi-frame")));
#endif     // USE_FFMPEG
    printf (_(" position = %ix%i"), app->area->width, app->area->height);
    if (app->area->x >= 0)
        printf ("+%i+%i", app->area->x, app->area->y);
    printf ("\n");
    printf (_(" rescale output to = %i\n"), app->rescale);
    printf (_(" frames per second = %.2f\n"), ((float) target->fps.num /
                                               (float) target->fps.den));
    printf (_(" file pattern = %s\n"), target->file);
    printf (_(" file format = %s\n"),
            ((target->target ==
              CAP_NONE) ? "AUTO" : _(xvc_formats[target->target].longname)));
    printf (_(" video encoding = %s\n"),
            ((target->targetCodec ==
              CODEC_NONE) ? "AUTO" : xvc_codecs[target->targetCodec].name));
#ifdef HAVE_FFMPEG_AUDIO
    printf (_(" audio codec = %s\n"),
            ((target->au_targetCodec ==
              CODEC_NONE) ? "AUTO" : xvc_audio_codecs[target->au_targetCodec].
             name));
#endif     // HAVE_FFMPEG_AUDIO
    printf (_(" verbose level = %d\n"), app->verbose);
    printf (_(" frame start no = %d\n"), target->start_no);
    printf (_(" frames to store = %d\n"), target->frames);
    printf (_(" time to capture = %i sec\n"), target->time);
    printf (_(" autocontinue = %s\n"),
            ((app->flags & FLG_AUTO_CONTINUE) ? "yes" : "no"));
    printf (_(" input source = %s (%d)\n"), app->source,
            app->flags & FLG_SOURCE);
    printf (_(" capture pointer = %s\n"), mp);
#ifdef HAVE_FFMPEG_AUDIO
    printf (_(" capture audio = %s\n"),
            ((target->audioWanted == 1) ? "yes" : "no"));
    printf (_(" - input = %s\n"), app->snddev);
    printf (_(" - sample rate = %i\n"), target->sndrate);
    printf (_(" - bit rate = %i\n"), target->sndsize);
    printf (_(" - channels = %i\n"), target->sndchannels);
#endif     // HAVE_FFMPEG_AUDIO
    printf (_(" animate command = %s\n"), target->play_cmd);
    printf (_(" make video command= %s\n"), target->video_cmd);
    printf (_(" edit frame command= %s\n"), target->edit_cmd);
}

/*
 * main
 */
int
main (int argc, char *argv[])
{
#undef DEBUGFUNCTION
#define DEBUGFUNCTION "main()"
    XVC_ErrorListItem *errors_after_cli = NULL;
    int resultCode;
    XVC_CapTypeOptions s_tmp_capture_options, *target;

#ifdef HasDGA
    int dga_evb, dga_errb;
#endif     // HasDGA

#ifdef ENABLE_NLS
    // i18n initialization
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    // xvc initialization
    init (&s_tmp_capture_options, &argc, argv);

    // read options file now
    xvc_read_options_file (app);

    // parse cli options and merge with app data
    parse_cli_options (&s_tmp_capture_options, argc, argv);
    target = merge_cli_options (&s_tmp_capture_options);

    // get errors in options to pass to xvc_ui_create
    {
        int rc;

        rc = 0;
        errors_after_cli = xvc_appdata_validate (app, 1, &rc);
        if (rc == -1) {
            fprintf (stderr,
                     _
                     ("%s %s: Unrecoverable error while validating options, please contact the xvidcap project.\n"),
                     DEBUGFILE, DEBUGFUNCTION);
            exit (1);
        }
    }

    // these are the hooks for a GUI to create the GUI,
    // the selection frame, and do some initialization ...
    if (!xvc_ui_create ()) {
        fprintf (stderr, _("%s %s: can't create GUI ... aborting\n"),
                 DEBUGFILE, DEBUGFUNCTION);
        exit (2);
    }
    if (!xvc_frame_create (capture_window)) {
        fprintf (stderr,
                 _("%s %s: can't create selection Frame ... aborting\n"),
                 DEBUGFILE, DEBUGFUNCTION);
        exit (2);
    }
    if (!xvc_ui_init (errors_after_cli)) {
        fprintf (stderr, _("%s %s: can't initialize GUI ... aborting\n"),
                 DEBUGFILE, DEBUGFUNCTION);
        exit (2);
    }

    if (app->verbose) {
        print_current_settings (target);
    }
    // signal handling for --gui no operation (CTRL-C) and
    // unsleeping the recoring thread on stop
    my_signal_add (SIGALRM, xvc_signal_handler);
    my_signal_add (SIGINT, xvc_signal_handler);

    // this is a hook for the GUI's main loop
    resultCode = xvc_ui_run ();

    cleanup ();
    return (resultCode);
}
