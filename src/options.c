/* 
 * options.c
 *
 * Copyright (C) 1997,98 Rasca, Berlin
 * Copyright (C) 2003-06 Karl H. Beckers, Frankfurt
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
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

#include "job.h"
#include "app_data.h"
#include "codecs.h"
#include "xvidcap-intl.h"

#define OPS_FILE ".xvidcaprc"

extern AppData *app;
extern xvCodec tCodecs[NUMCODECS];
extern xvFFormat tFFormats[NUMCAPS];
extern xvAuCodec tAuCodecs[NUMAUCODECS];

/* 
 * save the values
 */
Boolean xvc_write_options_file()
{
    int n;
    char *home, *element = NULL;
    char file[PATH_MAX + 1];
    FILE *fp;
    Job *job = xvc_job_ptr();

    // save it to $HOME/.xvidcaprc
    home = getenv("HOME");
    sprintf(file, "%s/%s", home, OPS_FILE);
    fp = fopen(file, "w");
    if (!fp) {
        perror(file);
        return FALSE;
    }

    fprintf(fp, _("#\n# xvidcap configuration file\n\n"));

    // general options
    fprintf(fp, _("#general options ...\n"));
    fprintf(fp,
            _("# default capture mode (0 = single-frame, 1 = multi-frame\ndefault_mode: %i\n"),
#ifdef HAVE_LIBAVCODEC
            app->default_mode);
#else // HAVE_LIBAVCODEC
            0);
#endif // HAVE_LIBAVCODEC
    fprintf(fp, _("# capture source\nsource: %s\n"), app->source);
    fprintf(fp, _("# hide GUI\nnogui: %d\n"),
            ((app->flags & FLG_NOGUI) ? 1 : 0));
#ifdef HAVE_FFMPEG_AUDIO
    fprintf(fp, _("# device to grab audio from\naudio_in: %s\n"),
            ((strcmp(app->snddev, "pipe:") == 0) ? "-" : app->snddev));
#endif // HAVE_FFMPEG_AUDIO
    fprintf(fp, _("# command to display help\nhelp_cmd: %s\n"),
            app->help_cmd);
    fprintf(fp,
            _("# what kind of mouse pointer should be recorded? 0 = none, 1 = white, 2 = black\n"));
    fprintf(fp, "mouse_wanted: %i\n", app->mouseWanted);
    fprintf(fp,
            _("# should cap_geometry be saved to prefs?\nsave_geometry: %d\n"),
            ((app->flags & FLG_SAVE_GEOMETRY) ? 1 : 0));
    if ((app->flags & FLG_SAVE_GEOMETRY) > 0) {
        fprintf(fp, _("# area to capture as in --capture_geometry\n"));
        fprintf(fp, "cap_geometry: %ix%i", job->area->width,
                job->area->height);
        if (job->area->x >= 0 && job->area->y >= 0)
            fprintf(fp, "+%i+%i\n", job->area->x, job->area->y);
        else
            fprintf(fp, "\n");
    }
    fprintf(fp, _("# show time rather than frame count\nshow_time: %d\n"),
            ((app->flags & FLG_SHOW_TIME) ? 1 : 0));
    fprintf(fp, _("# toggle autocontinue (0/1) \nauto_continue: %i\n"),
            ((app->flags & FLG_AUTO_CONTINUE) ? 1 : 0));
    fprintf(fp, _("# always show results dialog (0/1) \nalways_show_results: %i\n"),
            ((app->flags & FLG_ALWAYS_SHOW_RESULTS) ? 1 : 0));

    fprintf(fp, _("\n#options for single-frame capture ...\n"));
    fprintf(fp,
            _("# file pattern\n# this defines the filetype to write via the extension provided\n"));
    fprintf(fp, _("# valid extensions are: "));
#ifdef HAVE_LIBAVCODEC
    for (n = CAP_NONE; n < CAP_FFM; n++) {
#else
    for (n = CAP_NONE; n < NUMCAPS; n++) {
#endif                          // HAVE_LIBAVCODEC
        if (tFFormats[n].extensions) {
            element = xvc_next_element(tFFormats[n].extensions);
            while (element != NULL) {
                fprintf(fp, "%s", element);
                element = xvc_next_element(NULL);
                if (element != NULL)
                    fprintf(fp, ", ");
            }
#ifdef HAVE_LIBAVCODEC
            if (n < (CAP_FFM - 1))
                fprintf(fp, ", ");
#else
            if (n < (NUMCAPS - 1))
                fprintf(fp, ", ");
#endif                          // HAVE_LIBAVCODEC
        }
    }
    fprintf(fp, "\n");
    fprintf(fp, "sf_file: %s\n", app->single_frame.file);
    fprintf(fp,
            _("# file format - use AUTO to select format through file extension\n"));
    fprintf(fp, _("# Otherwise specify one of the following: "));
#ifdef HAVE_LIBAVCODEC
    for (n = (CAP_NONE + 1); n < CAP_MF; n++) {
#else
    for (n = (CAP_NONE + 1); n < NUMCAPS; n++) {
#endif                          // HAVE_LIBAVCODEC
        if (tFFormats[n].name) {
            element = xvc_next_element(tFFormats[n].name);
            while (element != NULL) {
                fprintf(fp, "%s", element);
                element = xvc_next_element(NULL);
                if (element != NULL)
                    fprintf(fp, ", ");
            }
            if (n < (NUMCAPS - 1))
                fprintf(fp, ", ");
        }
    }
    fprintf(fp, "\n");
    fprintf(fp, "sf_format: %s\n",
            tFFormats[app->single_frame.target].name);
    fprintf(fp,
            _("# video codec used by ffmpeg - use AUTO to auto-detect codec\n"));
    fprintf(fp, _("# Otherwise specify one of the following: "));
#ifdef HAVE_LIBAVCODEC
    for (n = (CODEC_NONE + 1); n < CODEC_MF; n++) {
#else
    for (n = (CODEC_NONE + 1); n < NUMCODECS; n++) {
#endif                          // HAVE_LIBAVCODEC
        if (tCodecs[n].name) {
            element = xvc_next_element(tCodecs[n].name);
            while (element != NULL) {
                fprintf(fp, "%s", element);
                element = xvc_next_element(NULL);
                if (element != NULL)
                    fprintf(fp, ", ");
            }
            if (n < (NUMCODECS - 1))
                fprintf(fp, ", ");
        }
    }
    fprintf(fp, "\n#sf_codec: %s\n",
            tCodecs[app->single_frame.targetCodec].name);
    fprintf(fp, _("# audio codec (for future use)\nsf_au_codec: %s\n"),
            "NONE");
    fprintf(fp, _("# frames per second\nsf_fps: %.2f\n"),
            ((float) app->single_frame.fps / 100));
    fprintf(fp, _("# max time (0 = unlimited)\nsf_max_time: %i\n"),
            app->single_frame.time);
    fprintf(fp, _("# max frames (0 = unlimited)\nsf_max_frames: %d\n"),
            app->single_frame.frames);
    fprintf(fp, _("# number to start counting individual frames from\n"));
    fprintf(fp, "sf_start_no: %i\n", app->single_frame.start_no);
    fprintf(fp, _("# quality (JPEG/MPEG)\nsf_quality: %d\n"),
            app->single_frame.quality);
    fprintf(fp,
            _("# toggle audio capture (0/1) (for future use)\nsf_audio: 0\n"));
    fprintf(fp,
            _("# sample rate for audio capture (for future use)\nsf_audio_rate: 0\n"));
    fprintf(fp,
            _("# bit rate for audio capture (for future use)\nsf_audio_bits: 0\n"));
    fprintf(fp,
            _("# number of channels to use in audio capture (for future use)\n"));
    fprintf(fp, "sf_audio_channels: 0\n");
    fprintf(fp, _("# command to display captured frames as animation\n"));
    fprintf(fp, "sf_animate_cmd: %s\n", app->single_frame.play_cmd);
    fprintf(fp, _("# command to edit current frame\n"));
    fprintf(fp, "sf_edit_cmd: %s\n", app->single_frame.edit_cmd);
    fprintf(fp, _("# command to encode captured frames\n"));
    fprintf(fp, "sf_video_cmd: %s\n", app->single_frame.video_cmd);

#ifdef HAVE_LIBAVCODEC
    fprintf(fp, _("\n#options for multi-frame capture ...\n"));
    fprintf(fp,
            _("# file pattern\n# this defines the filetype to write via the extension provided\n"));
    fprintf(fp, _("# valid extensions are: "));
    for (n = CAP_FFM; n < NUMCAPS; n++) {
        if (tFFormats[n].extensions) {
            element = xvc_next_element(tFFormats[n].extensions);
            while (element != NULL) {
                fprintf(fp, ".%s", element);
                element = xvc_next_element(NULL);
                if (element != NULL)
                    fprintf(fp, ", ");
            }
        }
    }
    fprintf(fp, "\n");
    fprintf(fp, "mf_file: %s\n", app->multi_frame.file);
    fprintf(fp,
            _("# file format - use AUTO to select format through file extension\n"));
    fprintf(fp, _("# Otherwise specify one of the following: "));
    for (n = CAP_MF; n < NUMCAPS; n++) {
        if (tFFormats[n].name) {
            element = xvc_next_element(tFFormats[n].name);
            while (element != NULL) {
                fprintf(fp, "%s", element);
                element = xvc_next_element(NULL);
                if (element != NULL)
                    fprintf(fp, ", ");
            }
            if (n < (NUMCAPS - 1))
                fprintf(fp, ", ");
        }
    }
    fprintf(fp, "\n");
    fprintf(fp, "mf_format: %s\n",
            tFFormats[app->multi_frame.target].name);
    fprintf(fp,
            _("# video codec used by ffmpeg - use AUTO to auto-detect codec\n"));
    fprintf(fp, _("# Otherwise specify one of the following: "));
    for (n = CODEC_MF; n < NUMCODECS; n++) {
        if (tCodecs[n].name) {
            element = xvc_next_element(tCodecs[n].name);
            while (element != NULL) {
                fprintf(fp, "%s", element);
                element = xvc_next_element(NULL);
                if (element != NULL)
                    fprintf(fp, ", ");
            }
            if (n < (NUMCODECS - 1))
                fprintf(fp, ", ");
        }
    }
    fprintf(fp, "\nmf_codec: %s\n",
            tCodecs[app->multi_frame.targetCodec].name);
    fprintf(fp,
            _("# audio codec used by ffmpeg - use AUTO to auto-detect audio codec\n"));
    fprintf(fp, _("# Otherwise specify one of the following: "));
    for (n = (AU_CODEC_NONE + 1); n < NUMAUCODECS; n++) {
        if (tAuCodecs[n].name) {
            element = xvc_next_element(tAuCodecs[n].name);
            while (element != NULL) {
                fprintf(fp, "%s", element);
                element = xvc_next_element(NULL);
                if (element != NULL)
                    fprintf(fp, ", ");
            }
            if (n < (NUMAUCODECS - 1))
                fprintf(fp, ", ");
        }
    }
#ifdef HAVE_FFMPEG_AUDIO
    fprintf(fp, "\nmf_au_codec: %s\n",
            tAuCodecs[app->multi_frame.au_targetCodec].name);
#endif // HAVE_FFMPEG_AUDIO
    fprintf(fp, _("# frames per second\nmf_fps: %.2f\n"),
            ((float) app->multi_frame.fps / 100));
    fprintf(fp, _("# max time (0 = unlimited)\nmf_max_time: %i\n"),
            app->multi_frame.time);
    fprintf(fp, _("# max frames (0 = unlimited)\nmf_max_frames: %d\n"),
            app->multi_frame.frames);
    fprintf(fp,
            _("# number to start counting individual frames from (for future use)\n"));
    fprintf(fp, "mf_start_no: %i\n", app->multi_frame.start_no);
    fprintf(fp, _("# quality (JPEG/MPEG)\nmf_quality: %d\n"),
            app->multi_frame.quality);
#ifdef HAVE_FFMPEG_AUDIO
    fprintf(fp, _("# toggle audio capture (0/1)\nmf_audio: %i\n"),
            ((app->multi_frame.audioWanted == 1) ? 1 : 0));
    fprintf(fp, _("# sample rate for audio capture\nmf_audio_rate: %i\n"),
            app->multi_frame.sndrate);
    fprintf(fp, _("# bit rate for audio capture\nmf_audio_bits: %i\n"),
            app->multi_frame.sndsize);
    fprintf(fp, _("# number of channels to use in audio capture\n"));
    fprintf(fp, "mf_audio_channels: %i\n", app->multi_frame.sndchannels);
#endif                          // HAVE_FFMPEG_AUDIO
    fprintf(fp, _("# command to display captured frames as animation\n"));
    fprintf(fp, "mf_animate_cmd: %s\n", app->multi_frame.play_cmd);
    fprintf(fp, _("# command to edit current movie\n"));
    fprintf(fp, "mf_edit_cmd: %s\n", app->multi_frame.edit_cmd);
    fprintf(fp,
            _("# command to encode captured frames (subject to change)\n"));
    fprintf(fp, "mf_video_cmd: %s\n", app->multi_frame.video_cmd);
#endif                          // HAVE_LIBAVCODEC

    fclose(fp);

    return TRUE;
}


/* 
 * read options file
 */
Boolean xvc_read_options_file()
{
    char line[260];
    char *home;
    char file[PATH_MAX + 1];
    FILE *fp;

    home = getenv("HOME");
    sprintf(file, "%s/%s", home, OPS_FILE);
    fp = fopen(file, "r");
    if (fp != NULL) {
        while (fgets(line, 255, fp)) {
            char *token, *value;
            char *n_hash;
            char *c_hash = strchr(line, '#');

            // ignore comments
            if (c_hash != NULL)
                sprintf(c_hash, "\0");
            // get rid of newlines
            n_hash = strchr(line, '\n');
            if (n_hash != NULL)
                sprintf(n_hash, "\0");

            // if smth. is left, parse line
            if (strlen(line) > 0) {
                char low_token[260];
                int y;

                token = strtok(line, " :=\"");
                // this has found the first token
                // for the value we need special treatment for the command 
                // 
                // parameters which
                // consist of multiple words
                // all other values are just a single token
                for (y = 0; y < strlen(token); y++) {
                    low_token[y] = tolower(token[y]);
                }
                low_token[strlen(token)] = '\0';
                if (strstr(low_token, "_animate_cmd") != NULL
                    || strstr(low_token, "_edit_cmd") != NULL
                    || strstr(low_token, "_video_cmd") != NULL
                    || strcasecmp(token, "help_cmd") == 0) {
                    int x = 1;

                    while (line[strlen(token) + x] == ' '
                           || line[strlen(token) + x] == ':'
                           || line[strlen(token) + x] == '=')
                        x++;
                    value = strdup(&line[strlen(token) + x]);
                } else {
                    value = strtok(NULL, " :=\"");
                    if (!value)
                        value = "";
                }

                // general options first ...
#ifdef HAVE_LIBAVCODEC
                if (strcasecmp(token, "default_mode") == 0) {
                    app->default_mode = atoi(value);
                } else 
#else // HAVE_LIBAVCODEC
                app->default_mode = 0;
#endif // HAVE_LIBAVCODEC
                if (strcasecmp(token, "source") == 0) {
                    app->source = strdup(value);
                } else if (strcasecmp(token, "nogui") == 0) {
                    if (atoi(value) == 1)
                        app->flags |= FLG_NOGUI;
                    else if (atoi(value) == 0)
                        app->flags &= ~FLG_NOGUI;
                    else {
                        app->flags &= ~FLG_NOGUI;
                        fprintf(stderr,
                                _("reading unsupported GUI preferences from options file\nresetting to GUI not hidden\n"));
                    }
                }
#ifdef HAVE_FFMPEG_AUDIO
                else if (strcasecmp(token, "audio_in") == 0) {
                    app->snddev = strdup(value);
                }
#endif // HAVE_FFMPEG_AUDIO
                else if (strcasecmp(token, "help_cmd") == 0) {
                    app->help_cmd = strdup(value);
                } else if (strcasecmp(token, "mouse_wanted") == 0) {
                    app->mouseWanted = atoi(value);
                } else if (strcasecmp(token, "save_geometry") == 0) {
                    if (atoi(value) == 1)
                        app->flags |= FLG_SAVE_GEOMETRY;
                    else if (atoi(value) == 0)
                        app->flags &= ~FLG_SAVE_GEOMETRY;
                    else {
                        app->flags &= ~FLG_SAVE_GEOMETRY;
                        fprintf(stderr,
                                _("reading unsupported save_geometry preferences from options file\nresetting to capture geometry not saved\n"));
                    }
                } else if (strcasecmp(token, "cap_geometry") == 0) {
                    sscanf(value, "%dx%d+%d+%d", &(app->cap_width),
                           &(app->cap_height), &(app->cap_pos_x),
                           &(app->cap_pos_y));
                } else if (strcasecmp(token, "show_time") == 0) {
                    // FIXME: this is not at all implemented, yet
                    if (atoi(value) == 1)
                        app->flags |= FLG_SHOW_TIME;
                    else if (atoi(value) == 0)
                        app->flags &= ~FLG_SHOW_TIME;
                    else {
                        app->flags &= ~FLG_SHOW_TIME;
                        fprintf(stderr,
                                _("reading unsupported show_time value from options file\nresetting to display frame counter\n"));
                    }
                } else if (strcasecmp(token, "auto_continue") == 0) {
                    if (value)
                        app->flags |= FLG_AUTO_CONTINUE;
                } else if (strcasecmp(token, "always_show_results") == 0) {
                    if (value)
                        app->flags |= FLG_ALWAYS_SHOW_RESULTS;

                    // now single-frame capture options
                } else if (strcasecmp(token, "sf_file") == 0) {
                    app->single_frame.file = strdup(value);
                } else if (strcasecmp(token, "sf_format") == 0) {
                    int cap_index = 0, found_target = 0;
                    for (cap_index = CAP_NONE; cap_index < NUMCAPS;
                         cap_index++) {
                        if (strcasecmp(tFFormats[cap_index].name, token) ==
                            0)
                            found_target = cap_index;
                    }
                    app->single_frame.target = found_target;
                } else if (strcasecmp(token, "sf_codec") == 0) {
                    int codec_index = 0, found_codec = 0;
                    for (codec_index = CODEC_NONE; codec_index < NUMCODECS;
                         codec_index++) {
                        if (strcasecmp(tCodecs[codec_index].name, token) ==
                            0)
                            found_codec = codec_index;
                    }
                    app->single_frame.targetCodec = found_codec;
                } 
#ifdef HAVE_FFMPEG_AUDIO
                else if (strcasecmp(token, "sf_au_codec") == 0) {
                    int auCodec_index = 0, found_auCodec = 0;
                    for (auCodec_index = AU_CODEC_NONE;
                         auCodec_index < NUMAUCODECS; auCodec_index++) {
                        if (strcasecmp
                            (tAuCodecs[auCodec_index].name, token) == 0)
                            found_auCodec = auCodec_index;
                    }
                    app->single_frame.au_targetCodec = found_auCodec;
                } 
#endif // HAVE_FFMPEG_AUDIO
                else if (strcasecmp(token, "sf_fps") == 0) {
                    app->single_frame.fps =
                        xvc_get_int_from_float_string(value);
                } else if (strcasecmp(token, "sf_max_time") == 0) {
                    app->single_frame.time = atoi(value);
                    if (atof(value) > 0)
                        app->single_frame.frames = 0;
                } else if (strcasecmp(token, "sf_max_frames") == 0) {
                    app->single_frame.frames = atoi(value);
                    if (atoi(value) > 0)
                        app->single_frame.time = 0;
                } else if (strcasecmp(token, "sf_start_no") == 0) {
                    app->single_frame.start_no = atof(value);
                } else if (strcasecmp(token, "sf_quality") == 0) {
                    app->single_frame.quality = atoi(value);
                    if ((app->single_frame.quality < 0)
                        || (app->single_frame.quality > 100))
                        app->single_frame.quality = 75; /* reset to
                                                         * default */
                } else if (strcasecmp(token, "sf_audio_rate") == 0) {

                    // for future use
                } else if (strcasecmp(token, "sf_ audio_bits") == 0) {
                    // for future use
                } else if (strcasecmp(token, "sf_audio_channels") == 0) {
                    // for future use
                } else if (strcasecmp(token, "sf_animate_cmd") == 0) {
                    app->single_frame.play_cmd = strdup(value);
                } else if (strcasecmp(token, "sf_edit_cmd") == 0) {
                    app->single_frame.edit_cmd = strdup(value);
                } else if (strcasecmp(token, "sf_video_cmd") == 0) {
                    app->single_frame.video_cmd = strdup(value);
                }
#ifdef HAVE_LIBAVCODEC
                // now multi-frame capture options
                else if (strcasecmp(token, "mf_file") == 0) {
                    app->multi_frame.file = strdup(value);
                } else if (strcasecmp(token, "mf_format") == 0) {
                    int n, a = -1;

                    for (n = CAP_FFM; n < NUMCAPS; n++) {
                        if (strcasecmp(value, tFFormats[n].name) == 0)
                            a = n;
                    }
                    if (strcasecmp(value, "NONE") == 0
                        || strcasecmp(value, "AUTO") == 0) {
                        // flag set target by filename through setting
                        // target to 0;
                        a = CAP_NONE;
                    }
                    if (a < CAP_NONE) {
                        fprintf(stderr,
                                _("reading unsupported target from options file\nresetting to target auto-detection\n"));
                        a = CAP_NONE;
                    }
                    if (a >= CAP_NONE)
                        app->multi_frame.target = a;
                } else if (strcasecmp(token, "mf_codec") == 0) {
                    int n, a = -1;

                    for (n = CODEC_NONE; n < NUMCODECS; n++) {
                        if (strcasecmp(value, tCodecs[n].name) == 0)
                            a = n;
                    }
                    if (strcasecmp(value, "AUTO") == 0)
                        a = CODEC_NONE;
                    if (a < CODEC_NONE) {
                        fprintf(stderr,
                                _("reading unsupported target codec (%i) from options file\nresetting to codec auto-detection\n"),
                                a);
                        a = CODEC_NONE;
                    }
                    app->multi_frame.targetCodec = a;
                } 
#ifdef HAVE_FFMPEG_AUDIO
                else if (strcasecmp(token, "mf_au_codec") == 0) {
                    int n, a = -1;

                    for (n = AU_CODEC_NONE; n < NUMAUCODECS; n++) {
                        if (strcasecmp(value, tAuCodecs[n].name) == 0)
                            a = n;
                    }
                    if (strcasecmp(value, "AUTO") == 0)
                        a = AU_CODEC_NONE;
                    if (a < AU_CODEC_NONE) {
                        fprintf(stderr,
                                _("reading unsupported target audio codec (%i) from options file\nresetting to audio codec auto-detection\n"),
                                a);
                        a = AU_CODEC_NONE;
                    }
                    app->multi_frame.au_targetCodec = a;
                } 
#endif // HAVE_FFMPEG_AUDIO
                else if (strcasecmp(token, "mf_fps") == 0) {
                    app->multi_frame.fps =
                        xvc_get_int_from_float_string(value);
                } else if (strcasecmp(token, "mf_max_time") == 0) {
                    app->multi_frame.time = atoi(value);
                    if (atof(value) > 0)
                        app->multi_frame.frames = 0;
                } else if (strcasecmp(token, "mf_max_frames") == 0) {
                    app->multi_frame.frames = atoi(value);
                    if (atoi(value) > 0)
                        app->multi_frame.time = 0;
                } else if (strcasecmp(token, "mf_start_no") == 0) {
                    app->multi_frame.start_no = atof(value);
                } else if (strcasecmp(token, "mf_quality") == 0) {
                    app->multi_frame.quality = atoi(value);
                    if ((app->multi_frame.quality < 0)
                        || (app->multi_frame.quality > 100))
                        app->multi_frame.quality = 75;  /* reset to
                                                         * default */
                }
#ifdef HAVE_FFMPEG_AUDIO
                else if (strcasecmp(token, "mf_audio") == 0) {
                    if (strcasecmp(value, "0") == 0) {
                        app->multi_frame.audioWanted = 0;
                    } else if (strcasecmp(value, "1") == 0) {
                        app->multi_frame.audioWanted = 1;
                    } else {
                        fprintf(stderr,
                                _("Invalid parameter 'audio: %s' in preferences file\n"),
                                value);
                        fprintf(stderr,
                                _("Disabling audio capture unless overridden by command line\n"));
                        app->multi_frame.audioWanted = 0;
                    }
                } else if (strcasecmp(token, "mf_audio_rate") == 0) {
                    if (value)
                        app->multi_frame.sndrate = atoi(value);
                } else if (strcasecmp(token, "mf_audio_bits") == 0) {
                    if (value)
                        app->multi_frame.sndsize = atoi(value);
                } else if (strcasecmp(token, "mf_audio_channels") == 0) {
                    if (value)
                        app->multi_frame.sndchannels = atoi(value);
                }
#endif                          // HAVE_FFMPEG_AUDIO
                else if (strcasecmp(token, "mf_animate_cmd") == 0) {
                    app->multi_frame.play_cmd = strdup(value);
                } else if (strcasecmp(token, "mf_edit_cmd") == 0) {
                    app->multi_frame.edit_cmd = strdup(value);
                } else if (strcasecmp(token, "mf_video_cmd") == 0) {
                    app->multi_frame.video_cmd = strdup(value);
                }
#endif                          // HAVE_LIBAVCODEC
            }
        }
    } else {                    // !fp
        return FALSE;
    }
    return TRUE;
}
