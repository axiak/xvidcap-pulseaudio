/* 
 * main.c
 * Copyright (C) 1997-98 Rasca, Berlin
 * Copyright (C) 2003-06 Karl H. Beckers, Frankfurt
 * 
 * main.c is free software.
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

#ifndef max
#define max(a,b) (a>b? a:b)
#endif
#ifndef min
#define min(a,b) (a<b? a:b)
#endif

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

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
#include "control.h"
#include "codecs.h"
#include "job.h"

#define DEBUGFILE "main.c"


/* 
 * GLOBAL VARIABLES 
 */
AppData sapp, *app;
extern xvCodec tCodecs[NUMCODECS];
extern xvFFormat tFFormats[NUMCAPS];
extern xvAuCodec tAuCodecs[NUMAUCODECS];


/*
 * STATIC VARIABLES
 */



/* 
 * HELPER FUNCTIONS 
 */
void usage(char *prog)
{
    int n, m;
    char *element;

    printf
        ("Usage: %s, ver %s, (c) rasca, berlin 1997,98,99, khb (c) 2003,04,05,06\n",
         prog, VERSION);
    printf("[--fps #.#] frames per second (float)\n");
    printf("[--verbose #] verbose level, '-v' is --verbose 1\n");
    printf("[--time #.#] time to record in seconds (float)\n");
    printf("[--frames #] frames to record, don't use it with --time\n");
    printf
        ("[--continue [yes|no]] autocontinue after maximum frames/time\n");
    printf
        ("[--cap_geometry #x#[+#+#]] size of the capture window (WIDTHxHEIGHT+X+Y)\n");
    printf("[--start_no #] start number for the file names\n");
#ifdef HAVE_SHMAT
    printf("[--source <src>] select input source: x11, shm\n");
#endif // HAVE_SHMAT
    printf("[--file <file>] file pattern, e.g. out%%03d.xwd\n");
    printf("[--gui [yes|no]] turn on/off gui\n");
#ifdef HAVE_LIBAVCODEC
    printf
        ("[--sf|--mf] request single-frame or multi-frame capture mode\n");
#endif                          // HAVE_LIBAVCODEC
    printf
        ("[--auto] cause auto-detection of output format, video-, and audio codec\n");
    printf
        ("[--codec <codec>] specify codec to use for multi-frame capture\n");
    printf
        ("[--codec-help] list available codecs for multi-frame capture\n");
    printf
        ("[--format <format>] specify file format to override the extension in the filename\n");
    printf("[--format-help] list available file formats\n");
#ifdef HAVE_FFMPEG_AUDIO
    printf
        ("[--aucodec <codec>] specify audio codec to use for multi-frame capture\n");
    printf
        ("[--aucodec-help] list available audio codecs for multi-frame capture\n");
    printf("[--audio [yes|no]] turn on/off audio capture\n");
    printf
        ("[--audio_in <src>] specify audio input device or '-' for pipe input\n");
    printf("[--audio_rate #] sample rate for audio capture\n");
    printf("[--audio_bits #] bit rate for audio capture\n");
    printf("[--audio_channels #] number of audio channels\n");
#endif                          // HAVE_FFMPEG_AUDIO

    printf("Supported output formats:\n");
    for (n = CAP_NONE; n < NUMCAPS; n++) {
        if (tFFormats[n].extensions) {
            printf(" %s", tFFormats[n].longname);
            for (m = strlen(tFFormats[n].longname); m < 40; m++)
                printf(" ");
            printf("(");
            element = xvc_next_element(tFFormats[n].extensions);
            while (element != NULL) {
                printf(".%s", element);
                element = xvc_next_element(NULL);
                if (element != NULL)
                    printf(", ");
            }
            printf(")\n");
        }
    }
    exit(1);
}


void xvc_init(CapTypeOptions * ctos, int argc, char *argv[])
{
    xvc_codecs_init();
    xvc_errors_init();

    // 
    // some variable initialization
    // 
    app = &sapp;
    xvc_app_data_init(app);
    xvc_app_data_set_defaults(app);
    xvc_cap_type_options_init(ctos);
}


void
parse_cli_options(CapTypeOptions * tmp_capture_options, int argc,
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
        {NULL, 0, NULL, 0},
    };
    int opt_index = 0, c;

    while ((c = getopt_long(argc, _argv, "v", options, &opt_index)) != -1) {
        switch (c) {
        case 0:                // it's a long option
            switch (opt_index) {
            case 0:            // fps
                tmp_capture_options->fps =
                    xvc_get_int_from_float_string(optarg);
                break;
            case 1:            // file
                tmp_capture_options->file = strdup(optarg);
                break;
            case 2:
                app->verbose = atoi(optarg);
                break;
            case 3:            // max_time
                tmp_capture_options->time = atoi(optarg);
                break;
            case 4:            // max_frames
                tmp_capture_options->frames = atoi(optarg);
                break;
            case 5:            // cap_geometry
                sscanf(optarg, "%dx%d+%d+%d", &(app->cap_width),
                       &(app->cap_height), &(app->cap_pos_x),
                       &(app->cap_pos_y));
                break;
            case 6:            // start_no
                tmp_capture_options->start_no = atoi(optarg);
                break;
            case 7:            // quality
                tmp_capture_options->quality = atoi(optarg);
                break;
            case 8:            // source
#ifdef HAVE_SHMAT
                app->source = strdup(optarg);
                /* 
                 * FIXME: do we need this any longer? if (strstr(optarg,
                 * "v4l") != NULL) { app->flags &= ~FLG_SOURCE; app->flags 
                 * |= FLG_USE_V4L; if (strchr(optarg, ':') != NULL) {
                 * app->device = strchr(optarg, ':')+1; } } else if
                 * (strstr(optarg, "dga") != NULL) { app->flags &=
                 * ~FLG_SOURCE; app->flags |= FLG_USE_DGA; } else if
                 * (strstr(optarg, "shm") != NULL) { app->flags &=
                 * ~FLG_SOURCE; app->flags |= FLG_USE_SHM; } else (
                 * app->flags &= ~FLG_SOURCE; printf("using normal x
                 * server access ..\n"); } 
                 */
#else
                fprintf(stderr,
                        "Only 'x11' is supported as a capture source with this binary.\n");
                usage(_argv[0]);
#endif // HAVE_SHMAT
                break;
            case 9:            // step_no
                tmp_capture_options->step = atoi(optarg);
                break;
            case 10:           // gui
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
                        tmp = strdup(optarg);
                    }
                    if (strstr(tmp, "no") != NULL) {
                        app->flags |= FLG_NOGUI;
                    } else {
                        app->flags &= ~FLG_NOGUI;
                    }
                }
                break;
            case 11:           // audio
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
                        tmp = strdup(optarg);
                    }
                    if (strstr(tmp, "no") != NULL) {
                        tmp_capture_options->audioWanted = 0;
                    } else {
                        tmp_capture_options->audioWanted = 1;
                    }
                }
#else
                fprintf(stderr,
                        "Audio support not present in this binary.\n");
                usage(_argv[0]);
#endif                          // HAVE_FFMPEG_AUDIO
                break;
            case 12:           // audio_in
#ifdef HAVE_FFMPEG_AUDIO
                tmp_capture_options->audioWanted = 1;
                app->snddev = strdup(optarg);
#else
                fprintf(stderr,
                        "Audio support not present in this binary.\n");
                usage(_argv[0]);
#endif                          // HAVE_FFMPEG_AUDIO
                break;
            case 13:           // audio_rate
#ifdef HAVE_FFMPEG_AUDIO
                tmp_capture_options->audioWanted = 1;
                tmp_capture_options->sndrate = atoi(optarg);
#else
                fprintf(stderr,
                        "Audio support not present in this binary.\n");
                usage(_argv[0]);
#endif                          // HAVE_FFMPEG_AUDIO
                break;
            case 14:           // audio_bits
#ifdef HAVE_FFMPEG_AUDIO
                tmp_capture_options->audioWanted = 1;
                tmp_capture_options->sndsize = atoi(optarg);
#else
                fprintf(stderr,
                        "Audio support not present in this binary.\n");
                usage(_argv[0]);
#endif                          // HAVE_FFMPEG_AUDIO
                break;
            case 15:           // audio_channels
#ifdef HAVE_FFMPEG_AUDIO
                tmp_capture_options->audioWanted = 1;
                tmp_capture_options->sndchannels = atoi(optarg);
#else
                fprintf(stderr,
                        "Audio support not present in this binary.\n");
                usage(_argv[0]);
#endif                          // HAVE_FFMPEG_AUDIO
                break;
            case 16:           // continue
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
                        tmp = strdup(optarg);
                    }
                    if (strstr(tmp, "no") != NULL) {
                        app->flags &= ~FLG_AUTO_CONTINUE;
                    } else {
                        app->flags |= FLG_AUTO_CONTINUE;
                    }
                }
                break;
#ifdef HAVE_LIBAVCODEC
            case 17:           // single-frame
                app->current_mode = 0;
                break;
            case 18:           // multi-frame
                app->current_mode = 1;
                break;
#endif // HAVE_LIBAVCODEC
            case 19:           // codec
                {
                    int n, m = 0;
                    Boolean found = FALSE;

                    for (n = 1; n < NUMCODECS; n++) {
                        if (strcasecmp(optarg, tCodecs[n].name) == 0) {
                            found = TRUE;
                            m = n;
                        }
                    }
                    if (strcasecmp(optarg, "auto") == 0) {
                        found = TRUE;
                        m = 0;  // set to CODEC_NONE which will get
                        // overridden later
                    }

                    if (found) {
                        tmp_capture_options->targetCodec = m;
                    } else {
                        usage(_argv[0]);
                    }
                }
                break;
            case 20:           // codec-help
                {
                    int n, m;
                    char tmp_codec[512];

                    printf
                        ("%s, ver %s, (c) rasca, berlin 1997,98,99, khb (c) 2003,04,05,06\n",
                         _argv[0], VERSION);

#ifdef HAVE_LIBAVCODEC
                    printf("Available codecs for single-frame capture: ");

                    for (n = 1; n < CAP_MF; n++) {
                        for (m = 0; m < strlen(tCodecs[n].name); m++) {
                            tmp_codec[m] = tolower(tCodecs[n].name[m]);
                        }
                        tmp_codec[strlen(tCodecs[n].name)] = '\0';
                        printf("%s", tmp_codec);
                        if (n < (CAP_MF - 1))
                            printf(", ");
                    }
                    printf("\n");

                    printf("Available codecs for multi-frame capture: ");

                    for (n = CAP_MF; n < NUMCODECS; n++) {
                        for (m = 0; m < strlen(tCodecs[n].name); m++) {
                            tmp_codec[m] = tolower(tCodecs[n].name[m]);
                        }
                        tmp_codec[strlen(tCodecs[n].name)] = '\0';
                        printf("%s", tmp_codec);
                        if (n < (NUMCODECS - 1))
                            printf(", ");
                    }
                    printf("\n");
#else
                    printf("Available codecs for single-frame capture: ");

                    for (n = 1; n < NUMCODECS; n++) {
                        for (m = 0; m < strlen(tCodecs[n].name); m++) {
                            tmp_codec[m] = tolower(tCodecs[n].name[m]);
                        }
                        tmp_codec[strlen(tCodecs[n].name)] = '\0';
                        printf("%s", tmp_codec);
                        if (n < (NUMCODECS - 1))
                            printf(", ");
                    }
                    printf("\n");
#endif                          // HAVE_LIBAVCODEC

                    printf
                        ("Specify 'auto' to use the file format's default codec.\n");
                    exit(1);
                }
                break;
            case 21:           // format
                {
                    int n, m = -1;
                    Boolean found = FALSE;

                    for (n = CAP_NONE; n < NUMCAPS; n++) {
                        if (strcasecmp(optarg, tFFormats[n].name) == 0) {
                            found = TRUE;
                            m = n;
                        }
                    }
                    if (strcasecmp(optarg, "auto") == 0) {
                        found = TRUE;
                        m = 0;
                    }

                    if (found) {
                        if (m > -1) {
                            tmp_capture_options->target = m;
                        } else {
                            fprintf(stderr,
                                    "Specified file format '%s' not supported with this binary.",
                                    optarg);
                            fprintf(stderr,
                                    "Resetting to file format auto-detection.\n");
                            tmp_capture_options->target = 0;
                        }
                    } else {
                        fprintf(stderr,
                                "Unknown file format '%s' specified.\n",
                                optarg);
                        usage(_argv[0]);
                    }
                }
                break;
            case 22:           // format-help
                {
                    int n, m;
                    char tmp_format[512];

                    printf
                        ("%s, ver %s, (c) rasca, berlin 1997,98,99, khb (c) 2003,04,05,06\n",
                         _argv[0], VERSION);
                    printf("Available file formats: ");

                    for (n = CAP_NONE; n < NUMCAPS; n++) {
                        if (tFFormats[n].extensions) {
                            for (m = 0; m < strlen(tFFormats[n].name); m++) {
                                tmp_format[m] =
                                    tolower(tFFormats[n].name[m]);
                            }
                            tmp_format[strlen(tFFormats[n].name)] = '\0';
                            printf("%s", tmp_format);
                            if (n < (NUMCAPS - 1))
                                printf(", ");
                        }
                    }
                    printf("\n");
                    printf
                        ("Specify 'auto' to force file format auto-detection.\n");
                    exit(1);
                }
                break;
            case 23:           // aucodec
#ifdef HAVE_FFMPEG_AUDIO
                {
                    int n, m = 0;
                    Boolean found = FALSE;

                    for (n = 1; n < NUMAUCODECS; n++) {
                        if (strcasecmp(optarg, tAuCodecs[n].name) == 0) {
                            found = TRUE;
                            m = n;
                        }
                    }
                    if (strcasecmp(optarg, "auto") == 0) {
                        found = TRUE;
                        m = 0;  // set to CODEC_NONE which will get
                        // overridden later
                    }

                    if (found) {
                        tmp_capture_options->au_targetCodec = m;
                    } else {
                        usage(_argv[0]);
                    }
                }
#else
                fprintf(stderr,
                        "Audio support not present in this binary.\n");
                usage(_argv[0]);
#endif                          // HAVE_FFMPEG_AUDIO
                break;
            case 24:           // aucodec-help
#ifdef HAVE_FFMPEG_AUDIO
                {
                    int n, m;
                    char tmp_codec[512];

                    printf
                        ("%s, ver %s, (c) rasca, berlin 1997,98,99, khb (c) 2003,04\n",
                         _argv[0], VERSION);
                    printf
                        ("Available audio codecs for multi-frame capture: ");

                    for (n = 1; n < NUMAUCODECS; n++) {
                        for (m = 0; m < strlen(tAuCodecs[n].name); m++) {
                            tmp_codec[m] = tolower(tAuCodecs[n].name[m]);
                        }
                        tmp_codec[strlen(tAuCodecs[n].name)] = '\0';
                        printf("%s", tmp_codec);
                        if (n < (NUMAUCODECS - 1))
                            printf(", ");
                    }
                    printf("\n");
                    printf
                        ("Specify 'auto' to use the file format's default audio codec.\n");
                    exit(1);
                }
#else
                fprintf(stderr,
                        "Audio support not present in this binary.\n");
                usage(_argv[0]);
#endif                          // HAVE_FFMPEG_AUDIO
                break;
            case 25:           // auto
                tmp_capture_options->target = 0;
                tmp_capture_options->targetCodec = 0;
#ifdef HAVE_FFMPEG_AUDIO
                tmp_capture_options->au_targetCodec = 0;
#endif // HAVE_FFMPEG_AUDIO
                break;
            default:
                usage(_argv[0]);
                break;
            }
            break;
            // now it's a short option
        case 'v':
            app->verbose++;
            break;

        default:
            usage(_argv[0]);
            break;
        }
    }
    if (argc != optind)
        usage(_argv[0]);

}


CapTypeOptions *merge_cli_options(CapTypeOptions * tmp_capture_options)
{
    #undef DEBUGFUNCTION
    #define DEBUGFUNCTION "merge_cli_options()"
    int current_mode_by_filename = -1, current_mode_by_target = -1;
    CapTypeOptions *target;

    // evaluate tmp_capture_options and set real ones accordingly

    // determine capture mode
    // either explicitly set in cli options
    // or default_mode in options (prefs/xdefaults), the latter does not
    // need
    // special action because it will have been set by now or fall back to 
    // 
    // the
    // default set in init_app_data.

    // app->current_mode is either -1 (not explicitly specified), 0
    // (single_frame),
    // or 1 (multi_frame) ... setting current_mode_by_* accordingly

    // by filename
    if (tmp_capture_options->file == NULL)
        current_mode_by_filename = -1;
    else {
        current_mode_by_filename =
            xvc_codec_get_target_from_filename(tmp_capture_options->file);
#ifdef HAVE_LIBAVCODEC
        if (current_mode_by_filename >= CAP_MF)
            current_mode_by_filename = 1;
        else
#endif                          // HAVE_LIBAVCODEC
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
#ifdef HAVE_LIBAVCODEC
        if (tmp_capture_options->target >= CAP_MF)
            current_mode_by_target = 1;
        else
#endif                          // HAVE_LIBAVCODEC
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
    // printf("Doing %s capture.\n",
    // ((app->current_mode == 0) ? "single-frame" : "multi-frame"));

    // merge cli options with app
    if (xvc_merge_cap_type_and_app_data(tmp_capture_options, app) < 1) {
        fprintf(stderr,
                "%s %s: Unrecoverable error while merging options, please contact the xvidcap project.\n", DEBUGFILE, DEBUGFUNCTION);
        exit(1);
    }

    return target;

}


void cleanup_when_interrupted(int signal)
{
    Job *job = xvc_job_ptr();
    
    if (job) xvc_capture_stop(job);
}


void print_current_settings(CapTypeOptions * target)
{
    char *mp;
    switch (app->mouseWanted) {
    case 2:
        mp = "black";
        break;
    case 1:
        mp = "white";
        break;
    default:
        mp = "none";
    }

    // FIXME: make the options output take new app_data structure with
    // parallel capTypeOptions into account

    printf("Current settings:\n");
    printf(" flags = %d\n", app->flags);
#ifdef HAVE_LIBAVCODEC
    printf(" capture mode = %s\n",
           ((app->current_mode == 0) ? "single-frame" : "multi-frame"));
#endif                          // HAVE_LIBAVCODEC
    printf(" position = %ix%i", app->cap_width, app->cap_height);
    if (app->cap_pos_x >= 0)
        printf("+%i+%i", app->cap_pos_x, app->cap_pos_y);
    printf("\n");
    printf(" frames per second = %.2f\n", ((float) target->fps / 100));
    printf(" file pattern = %s\n", target->file);
    printf(" file format = %s\n",
           ((target->target ==
             CAP_NONE) ? "AUTO" : tFFormats[target->target].longname));
    printf(" video encoding = %s\n",
           ((target->targetCodec ==
             CODEC_NONE) ? "AUTO" : tCodecs[target->targetCodec].name));
#ifdef HAVE_FFMPEG_AUDIO
    printf(" audio codec = %s\n",
           ((target->au_targetCodec ==
             CODEC_NONE) ? "AUTO" : tAuCodecs[target->au_targetCodec].
            name));
#endif // HAVE_FFMPEG_AUDIO
    printf(" verbose level = %d\n", app->verbose);
    printf(" frames to store = %d\n", target->frames);
    printf(" time to capture = %i sec\n", target->time);
    printf(" autocontinue = %s\n",
           ((app->flags & FLG_AUTO_CONTINUE) ? "yes" : "no"));
    printf(" input source = %s (%d)\n", app->source,
           app->flags & FLG_SOURCE);
    printf(" capture pointer = %s\n", mp);
#ifdef HAVE_FFMPEG_AUDIO
    printf(" capture audio = %s\n",
           ((target->audioWanted == 1) ? "yes" : "no"));
    printf(" - input = %s\n", app->snddev);
    printf(" - sample rate = %i\n", target->sndrate);
    printf(" - bit rate = %i\n", target->sndsize);
    printf(" - channels = %i\n", target->sndchannels);
#endif                          // HAVE_FFMPEG_AUDIO
    printf(" animate command = %s\n", target->play_cmd);
    printf(" make video command= %s\n", target->video_cmd);
    printf(" edit frame command= %s\n", target->edit_cmd);
    printf(" help command = %s\n", app->help_cmd);
}


/* 
 * main
 */
int main(int argc, char *argv[])
{
    #undef DEBUGFUNCTION
    #define DEBUGFUNCTION "main()"
    xvErrorListItem *errors_after_cli = NULL;
    int resultCode;
    CapTypeOptions s_tmp_capture_options, *target;

#ifdef HasDGA
    int dga_evb, dga_errb;
#endif                          // HasDGA

    // Xlib threading initialization
    // this is here with the gtk/glib stuff in gnome_ui.c|h because this is
    // UI independant and would need to be here even with Qt
    XInitThreads();

#ifdef ENABLE_NLS
    // i18n initialization
    bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);
#endif

    // this is a hook for a GUI to do some pre-init functions ...
    // possibly to set some fallback options read from a rc file or
    // Xdefaults
    if (!xvc_init_pre(argc, argv)) {
        fprintf(stderr, "%s %s: can't do GUI pre-initialization ... aborting\n", DEBUGFILE, DEBUGFUNCTION);
        exit(2);
    }
    // xvc initialization
    xvc_init(&s_tmp_capture_options, argc, argv);

    // read options file now 
    xvc_read_options_file(app);

    // parse cli options and merge with app data
    parse_cli_options(&s_tmp_capture_options, argc, argv);
    target = merge_cli_options(&s_tmp_capture_options);

    // get errors in options to pass to xvc_ui_create
    {
        int rc;
        rc = 0;
        errors_after_cli = xvc_app_data_validate(app, 1, &rc);
        if (rc == -1) {
            fprintf(stderr,
                    "%s %s: Unrecoverable error while validating options, please contact the xvidcap project.\n", DEBUGFILE, DEBUGFUNCTION);
            exit(1);
        }
    }

    // these are the hooks for a GUI to create the GUI,
    // the selection frame, and do some initialization ...
    if (!xvc_ui_create()) {
        fprintf(stderr, "%s %s: can't create GUI ... aborting\n", DEBUGFILE, DEBUGFUNCTION);
        exit(2);
    }
    if (!xvc_frame_create()) {
        fprintf(stderr, "%s %s: can't create selection Frame ... aborting\n", DEBUGFILE, DEBUGFUNCTION);
        exit(2);
    }
    if (!xvc_ui_init(errors_after_cli)) {
        fprintf(stderr, "%s %s: can't initialize GUI ... aborting\n", DEBUGFILE, DEBUGFUNCTION);
        exit(2);
    }

    if (app->verbose) {
        print_current_settings(target);
    }

    signal(SIGINT, cleanup_when_interrupted);

    // this is a hook for the GUI's main loop
    resultCode = xvc_ui_run();

#ifdef HAVE_LIBAVCODEC
    av_free_static();
#endif

    return (resultCode);
}
