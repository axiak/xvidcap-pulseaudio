/* 
 * app_data.c,
 *
 * Copyright (C) 2004-06 Karl, Frankfurt
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

#ifndef max
#define max(a,b) (a>b? a:b)
#endif
#ifndef min
#define min(a,b) (a<b? a:b)
#endif

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> // PATH_MAX
#include <ctype.h>

/* FIXME: still necessary ... and here?
#ifdef SOLARIS 
#include <X11/X.h> 
#include <X11/Xlib.h> 
#endif //SOLARIS
*/ 

#include "app_data.h"
#include "codecs.h"
#include "xvidcap-intl.h"

#define DEBUGFILE "app_data.c"

xvError xvErrors[NUMERRORS];
extern AppData *app;
extern xvCodec tCodecs[NUMCODECS];
extern xvFFormat tFFormats[NUMCAPS];
extern xvAuCodec tAuCodecs[NUMAUCODECS];

/* 
 * initialize empty CapTypeOptions structure
 */
void xvc_cap_type_options_init(CapTypeOptions * cto)
{
    cto->file = NULL;           // file pattern
    cto->target = -1;           // target file format
    cto->targetCodec = -1;      // for video encoding
    cto->fps = -1;              // frames per second
    cto->time = -1;             // time in seconds to record
    cto->frames = -1;           // max frames to record
    cto->start_no = -1;         // frame number to start with
    cto->step = -1;             // frame increment
    cto->quality = -1;          // quality setting
    cto->bpp = -1;              // bits per pixel
#ifdef HAVE_FFMPEG_AUDIO
    cto->au_targetCodec = -1;   // for audio encoding
    cto->audioWanted = -1;      // want audio
    cto->sndrate = -1;          // sound sample rate
    cto->sndsize = -1;          // bits to sample for audio capture
    cto->sndchannels = -1;      // number of channels to record audio to
#endif // HAVE_FFMPEG_AUDIO
    cto->play_cmd = NULL;       // command to use for animate function
    cto->video_cmd = NULL;      // command to use for make video function
    cto->edit_cmd = NULL;       // command to use for edit function
}


/* 
 * initialize empty AppData structure
 */
void xvc_app_data_init(AppData * lapp)
{
    lapp->verbose = 0;          // verbose level
    lapp->flags = 0;            // flags used ... see above
    lapp->cap_width = 0;        // width
    lapp->cap_height = 0;       // height
    lapp->cap_pos_x = 0;        // x position of the capture frame
    lapp->cap_pos_y = 0;        // y position of the capture frame
    lapp->rescale = 0;
    lapp->mouseWanted = 0;      // capture mouse pointer: 0 none , 1 white,
                                // 2 black
    lapp->source = NULL;        // video capture source
#ifdef HAVE_FFMPEG_AUDIO
    lapp->snddev = NULL;        // audio capture source
#endif // HAVE_FFMPEG_AUDIO
    // information
    lapp->device = NULL;        // v4l device to capture from
    lapp->default_mode = 0;     // 0 = single_frame, 1 = multi_frame
    lapp->current_mode = -1;    // dto. ... this is set after evaluation
    // of cli options
    xvc_cap_type_options_init(&(lapp->single_frame));
#ifdef USE_FFMPEG
    xvc_cap_type_options_init(&(lapp->multi_frame));
#endif // USE_FFMPEG
}


/* 
 * set default values for AppData structure
 */
void xvc_app_data_set_defaults(AppData * lapp)
{
    // initialize general options
    lapp->cap_pos_x = lapp->cap_pos_y = -1;
    lapp->flags = FLG_ALWAYS_SHOW_RESULTS;
    lapp->device = "/dev/video0";
#ifdef HAVE_SHMAT
    lapp->source = "shm";
#else
    lapp->source = "x11";
#endif
    lapp->mouseWanted = 1;
#ifdef HAVE_FFMPEG_AUDIO    
    lapp->snddev = "/dev/dsp";
#endif // HAVE_FFMPEG_AUDIO
    lapp->cap_width = 192;
    lapp->cap_height = 144;
    lapp->rescale = 100;
#ifdef USE_FFMPEG
    lapp->default_mode = 1;
#else 
    lapp->default_mode = 0;
#endif // USE_FFMPEG

    // initialzie options specific to either single- or multi-frame
    // capture
    lapp->single_frame.quality = lapp->multi_frame.quality = 75;
    lapp->single_frame.step = lapp->multi_frame.step = 1;
    lapp->single_frame.bpp = lapp->multi_frame.bpp = 0;
    // the following two mean autodetect ...
    lapp->single_frame.target = lapp->multi_frame.target = CAP_NONE;
    lapp->single_frame.targetCodec = lapp->multi_frame.targetCodec =
        CODEC_NONE;
#ifdef HAVE_FFMPEG_AUDIO
    lapp->single_frame.au_targetCodec = lapp->multi_frame.au_targetCodec =
        CODEC_NONE;
    lapp->single_frame.audioWanted = 0;
#endif // HAVE_FFMPEG_AUDIO
    lapp->single_frame.start_no = lapp->multi_frame.start_no = 0;
    lapp->single_frame.file = "test-%04d.xwd";
    lapp->single_frame.fps = lapp->multi_frame.fps = 1000;
    lapp->single_frame.time = lapp->multi_frame.time = 0;
    lapp->single_frame.frames = lapp->multi_frame.frames = 0;

    lapp->single_frame.play_cmd =
        "animate \"${XVFILE}\" -delay $((XVTIME/10)) &";
    lapp->single_frame.video_cmd =
        "ppm2mpeg.sh \"${XVFILE}\" ${XVFFRAME} ${XVLFRAME} ${XVWIDTH} ${XVHEIGHT} ${XVFPS} ${XVTIME} &";
    lapp->single_frame.edit_cmd = "gimp \"${XVFILE}\" &";

#ifdef USE_FFMPEG
#ifdef HAVE_FFMPEG_AUDIO
    lapp->multi_frame.audioWanted = 1;
    lapp->multi_frame.sndrate = 22050;
    lapp->multi_frame.sndsize = 32000;
    lapp->multi_frame.sndchannels = 1;
#endif // HAVE_FFMPEG_AUDIO
    lapp->multi_frame.file = "test-%04d.mpeg";

    lapp->multi_frame.edit_cmd =
        _("xterm -e 'echo \"none specified\" ; sleep 20'");
    lapp->multi_frame.video_cmd =
        _("xterm -e 'echo \"not needed for multi-frame capture\" ; sleep 20'");
    lapp->multi_frame.play_cmd = "mplayer \"${XVFILE}\" &";
#endif // USE_FFMPEG

}

/* 
 * copy a CapTypeOptions structure
 */
void
xvc_cap_type_options_copy(CapTypeOptions * topts, CapTypeOptions * sopts)
{

    topts->file = strdup(sopts->file);  // file pattern
    topts->target = sopts->target;  // target file format
    topts->targetCodec = sopts->targetCodec;    // for video encoding
    topts->fps = sopts->fps;    // frames per second
    topts->time = sopts->time;  // time in seconds to record
    topts->frames = sopts->frames;  // max frames to record
    topts->start_no = sopts->start_no;  // frame number to start with
    topts->step = sopts->step;  // frame increment
    topts->quality = sopts->quality;    // quality setting
    topts->bpp = sopts->bpp;    // bits per pixel
#ifdef HAVE_FFMPEG_AUDIO
    topts->au_targetCodec = sopts->au_targetCodec;  // for audio
    // encoding
    topts->audioWanted = sopts->audioWanted;    // audio wanted
    topts->sndrate = sopts->sndrate;    // sound sample rate
    topts->sndsize = sopts->sndsize;    // bits to sample for audio
    // capture
    topts->sndchannels = sopts->sndchannels;    // number of channels to
    // record audio to 
#endif // HAVE_FFMPEG_AUDIO
    topts->play_cmd = strdup(sopts->play_cmd);  // command to use for
    // animate function
    topts->video_cmd = strdup(sopts->video_cmd);    // command to use
    // for make video
    // function
    topts->edit_cmd = strdup(sopts->edit_cmd);  // command to use for edit 
                                                // 
    // function

}


/* 
 * copy an AppData structure
 */
void xvc_app_data_copy(AppData * tapp, AppData * sapp)
{

    tapp->verbose = sapp->verbose;  // verbose level
    tapp->flags = sapp->flags;  // flags used ... see above
    tapp->cap_width = sapp->cap_width;  // width
    tapp->cap_height = sapp->cap_height;    // height
    tapp->cap_pos_x = sapp->cap_pos_x;  // x position of the capture frame
    tapp->cap_pos_y = sapp->cap_pos_y;  // y position of the capture frame
    tapp->rescale = sapp->rescale;
    tapp->mouseWanted = sapp->mouseWanted;  // capture mouse pointer:
    // 0 none , 1 white , 2
    // black
    tapp->source = strdup(sapp->source);    // video capture source
#ifdef HAVE_FFMPEG_AUDIO
    tapp->snddev = strdup(sapp->snddev);    // audio capture source
#endif // HAVE_FFMPEG_AUDIO
    tapp->device = strdup(sapp->device);    // v4l device to capture
    // from
    tapp->default_mode = sapp->default_mode;    // 0 = single_frame, 1 =
    // multi_frame
    tapp->current_mode = sapp->current_mode;    // dto.
    xvc_cap_type_options_copy(&(tapp->single_frame),
                              &(sapp->single_frame));
#ifdef USE_FFMPEG
    xvc_cap_type_options_copy(&(tapp->multi_frame), &(sapp->multi_frame));
#endif // USE_FFMPEG

}



/* 
 * validates app_data parameters before job is set
 * mode = 0 : full check
 * mode = 1 : ignore non-current mode data
 *
 * returns 0 on no error
 * 1 on errors
 * -1 on internal errors (i.e. errors while checking for errors)
 *
 * passing the errors return pointer always clears it
 */
xvErrorListItem *xvc_app_data_validate(AppData * lapp, int mode, int *rc)
{
    #undef DEBUGFUNCTION
    #define DEBUGFUNCTION "xvc_app_data_validate()"
    CapTypeOptions *target = NULL;
#ifdef USE_FFMPEG
    CapTypeOptions *non_target = NULL;
#endif
    xvErrorListItem *errors = NULL;
    int t_codec, t_format;
#ifdef HAVE_FFMPEG_AUDIO
    int t_au_codec;
#endif // HAVE_FFMPEG_AUDIO

    // capture size related
    Display *dpy;
    int max_width, max_height;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

#ifdef USE_FFMPEG
    // we need current_mode determined by now
    if (lapp->current_mode < 0) {
        *rc = -1;
        return NULL;
    }
    if (lapp->current_mode == 0) {
        target = &(lapp->single_frame);
        non_target = &(lapp->multi_frame);
    } else {
        target = &(lapp->multi_frame);
        non_target = &(lapp->single_frame);
    }
#else // USE_FFMPEG
    // we only have single frame capture
    if (lapp->current_mode != 0)
        printf(_("%s %s: Capture mode not single_frame (%i) but we don't have ffmpeg ... correcting, but smth's wrong\n"), 
                DEBUGFILE, DEBUGFUNCTION, lapp->current_mode);
    
    lapp->current_mode = 0;
    target = &(lapp->single_frame);
#endif // USE_FFMPEG


    // flags related
    // 1. verbose
    // the verbose member of the appData struct is just here because in
    // the
    // future we might have various levels of verbosity. Atm we just turn
    // it on
    if (lapp->verbose)
        lapp->flags |= FLG_RUN_VERBOSE;


    // start: capture size
    dpy = XOpenDisplay(NULL);
    // printf("display: %p\n", dpy);
    max_width = WidthOfScreen(DefaultScreenOfDisplay(dpy));
    max_height = HeightOfScreen(DefaultScreenOfDisplay(dpy));
    XCloseDisplay(dpy);

    if ((lapp->cap_width + lapp->cap_pos_x) > max_width ||
        (lapp->cap_height + lapp->cap_pos_y) > max_height) {
        errors = xvc_errors_append(6, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: capture size

    // start: mouseWanted
    if (lapp->mouseWanted < 0 || lapp->mouseWanted > 2) {
        errors = xvc_errors_append(7, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: mouseWanted

    // start: source
//    lapp->flags &= ~FLG_SOURCE;
    if (strcasecmp(app->source, "x11") == 0) {
        // empty
    }
#ifdef HAVE_SHMAT   
    else if (strcasecmp(app->source, "shm") == 0)
        lapp->flags |= FLG_USE_SHM;
#endif // HAVE_SHMAT
    else if (strcasecmp(app->source, "dga") == 0)
        lapp->flags |= FLG_USE_DGA;
    else if (strstr(app->source, "v4l") != NULL)
        lapp->flags |= FLG_USE_V4L;
    else {
        errors = xvc_errors_append(8, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }

    if (lapp->flags & FLG_USE_V4L) {
#ifndef HasVideo4Linux
        errors = xvc_errors_append(2, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
#else                           // HasVideo4Linux
        VIDEO *video;

        if (strchr(lapp->source, ':') != NULL) {
            lapp->device = strchr(lapp->source, ':') + 1;
        }

        video = video_open(lapp->device, O_RDWR);
        if (!video) {
            errors = xvc_errors_append(3, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
            *rc = 1;
            return errors;
        }

        if (lapp->flags & FLG_RUN_VERBOSE) {
            printf("%s:\n cctm=%d channels=%d audios=%d maxwidth=%d "
                   "maxheight=%d minwidth=%d minheight=%d\n",
                   video->name, video->type & VID_TYPE_CAPTURE,
                   video->channels, video->audios, video->maxwidth,
                   video->maxheight, video->minwidth, video->minheight);
        }
        if (!(video->type & VID_TYPE_CAPTURE)) {
            errors = xvc_errors_append(4, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
            *rc = 1;
            return errors;
        }
        if (lapp->cap_width > video->maxwidth
            || lapp->cap_width < video->minwidth
            || lapp->cap_height > video->maxheight
            || lapp->cap_height < video->minheight) {
            errors = xvc_errors_append(5, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
        video_close(video);
#endif                          // HasVideo4Linux
    }
    // end: source

    // snddev needs to be checked from cap_type_options checks
    // e.g. if target has audio enabled and snddev not accessible
    // we throw an error ... nothing to check here, because we
    // wouldn't know what to do, because we don't know if this is
    // relevant for capturing

    // help_cmd related ...
    // no checks atm

    // device related
    // is V4L device which is checked for further up

    // start: default_mode
    if (lapp->default_mode < 0 || lapp->default_mode > 1) {
        errors = xvc_errors_append(9, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: current_mode

    // current_mode must be selected by now

    // start: rescale
    if (lapp->rescale < 1 || lapp->rescale > 100 ) {
        errors = xvc_errors_append(37, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: rescale

    /* 
     * Now check target capture type options
     */

    // start: file
    if (!target->file) {
        errors =
            xvc_errors_append((lapp->current_mode == 0) ? 10 : 11, errors,
                              lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
        *rc = 1;
        return errors;
    }

    if (strlen(target->file) < 1) {
        if (lapp->current_mode == 0) {
            // single-frame capture does not know "ask-user"
            errors = xvc_errors_append(12, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
            *rc = 1;
            return errors;
        } else {
            if (target->target == CAP_NONE) {
                // this is an empty filename string which signifies "ask
                // the user"
                // however we do not support "ask-user" and target
                // auto-detect
                // combined at this time ... so we change to default
                // filename
                errors = xvc_errors_append(13, errors, lapp);
                if (!errors) {
                    *rc = -1;
                    return NULL;
                }
                *rc = 1;
                return errors;
            } else {
                // this is "ask-user"
                if ((lapp->flags & FLG_AUTO_CONTINUE) > 0)
                    errors = xvc_errors_append(14, errors, lapp);
                else
                    errors = xvc_errors_append(15, errors, lapp);
                if (!errors) {
                    *rc = -1;
                    return NULL;
                }
            }

        }
    }
    // end: file

    // start: target
    // first find auto-detection results if auto-detection is turned on
    t_format = target->target;
    t_codec = target->targetCodec;
    if (!t_format)
        t_format = xvc_codec_get_target_from_filename(target->file);
    if (!t_codec)
        t_codec = tFFormats[t_format].def_vid_codec;
#ifdef HAVE_FFMPEG_AUDIO
    t_au_codec = target->au_targetCodec;        
    if (!t_au_codec)
        t_au_codec = tFFormats[t_format].def_au_codec;
#endif // HAVE_FFMPEG_AUDIO

    // is target valid and does it match the current_mode?
    // the next includes t_format == 0 ... which is at this stage an
    // unresolved
    // auto-detection
    if (t_format <= CAP_NONE || t_format >= NUMCAPS) {
        errors =
            xvc_errors_append((lapp->current_mode == 0) ? 16 : 17, errors,
                              lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
        *rc = 1;
        return errors;
    }
#ifdef USE_FFMPEG
    else if (lapp->current_mode == 0 && t_format >= CAP_MF) {
        errors = xvc_errors_append(18, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
        *rc = 1;
        return errors;
    }
    else if (lapp->current_mode == 1 && t_format < CAP_MF) {
        errors = xvc_errors_append(19, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
        *rc = 1;
        return errors;
    }
    // does targetCodec match the target
    if (t_format < CAP_FFM && target->targetCodec != CODEC_NONE) {
        errors = xvc_errors_append(20, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
#endif                          // USE_FFMPEG

    if (target->target == CAP_NONE && target->targetCodec != CODEC_NONE) {
        // if target is auto-detected we also want to auto-detect
        // targetCodec
        errors =
            xvc_errors_append((lapp->current_mode == 0) ? 21 : 22, errors,
                              lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
        // a failure of xvc_is_element (...) is only relevant if
        // allowed_vid_codecs
        // is not NULL
    } 
#ifdef USE_FFMPEG    
    else if (tFFormats[t_format].allowed_vid_codecs &&
               (!xvc_is_element(tFFormats[t_format].allowed_vid_codecs,
                                tCodecs[t_codec].name))) {
#ifdef DEBUG
        printf("app_data: format %i %s - codec %i %s\n", t_format,
               tFFormats[t_format].name, t_codec, tCodecs[t_codec].name);
#endif                          // DEBUG

        errors = xvc_errors_append(23, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
        // FIXME: do I need to return here?
        // *rc = 1;
        // return errors;
    }
#ifdef HAVE_FFMPEG_AUDIO
    // only check audio settings if audio capture is enabled
    if ( target->audioWanted == 1 ) {
        if (t_format < CAP_FFM && target->au_targetCodec != CODEC_NONE) {
            errors = xvc_errors_append(24, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        } else
        if (target->target == CODEC_NONE
                && target->au_targetCodec != CODEC_NONE) {
            // if target is auto-detected we also want to auto-detect
            // au_targetCodec
            errors = xvc_errors_append(25, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        } 
        // a failure of xvc_is_element (...) is only relevant if
        // allowed_au_codecs
        // is not NULL
        else if (tFFormats[t_format].allowed_au_codecs &&
                (!xvc_is_element(tFFormats[t_format].allowed_au_codecs,
                                tAuCodecs[t_au_codec].name))) {
            errors = xvc_errors_append(26, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
            // FIXME: do I need to return here?
            // *rc = 1;
            // return errors;
        } 
        else if (tFFormats[t_format].allowed_au_codecs == NULL
                && target->audioWanted) {
            errors =
                xvc_errors_append((lapp->current_mode == 0) ? 42 : 43, errors,
                                lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
    }
#endif // HAVE_FFMPEG_AUDIO
#endif // USE_FFMPEG
    // end: target

    // targetCodec ... check for suitable format is above with target

    // au_targetCodec ... check for suitable format is above with target

    // start: fps
    if (lapp->current_mode == 1
        && (xvc_codec_is_valid_fps(target->fps, t_codec) == 0)) {
        errors = xvc_errors_append(27, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    } else if (lapp->current_mode == 0 && target->fps <= 0) {
        errors = xvc_errors_append(28, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: fps

    // start: time
    if (target->time < 0) {
        errors =
            xvc_errors_append((lapp->current_mode == 0) ? 29 : 30, errors,
                              lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: time

    // start: frames
    if (target->time < 0) {
        errors =
            xvc_errors_append((lapp->current_mode == 0) ? 31 : 32, errors,
                              lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: frames

    // start: start_no
    if (target->start_no < 0) {
        errors =
            xvc_errors_append((lapp->current_mode == 0) ? 33 : 34, errors,
                              lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: start_no

    // start: step
    if (target->step <= 0) {
        errors =
            xvc_errors_append((lapp->current_mode == 0) ? 35 : 36, errors,
                              lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    } else if (lapp->current_mode == 1 && target->step != 1) {
        errors = xvc_errors_append(36, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: step

    // start: quality
    if ((target->quality < 0) || (target->quality > 100)) {
        errors =
            xvc_errors_append((lapp->current_mode == 0) ? 40 : 41, errors,
                              lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: quality

    // audioWanted is checked with target 

    /* 
     * dunno about valid rates for the following FIXME: implement some
     * sanity checks int sndrate; // sound sample rate int sndsize; //
     * bits to sample for audio capture int sndchannels; // number of
     * channels to record audio to 
     */

    /* 
     * FIXME: can there be a meaningful check to validate the following?
     *
     char *play_cmd; // command to use for animate function
     char *video_cmd; // command to use for make video function
     char *edit_cmd; // command to use for edit function
     */


    // if we don't have FFMPEG, we can only have one capture type, i.e. the target
#ifdef USE_FFMPEG
    if (mode == 0) {
        /* 
         * Now check non-target capture type options
         */

        // start: file
        if (!non_target->file) {
            errors =
                xvc_errors_append((lapp->current_mode == 1) ? 10 : 11,
                                  errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
            *rc = 1;
            return errors;
        }

        if (strlen(non_target->file) < 1) {
            if (lapp->current_mode == 1) {
                // single-frame capture does not know "ask-user"
                errors = xvc_errors_append(12, errors, lapp);
                if (!errors) {
                    *rc = -1;
                    return NULL;
                }
                *rc = 1;
                return errors;
            } else {
                if (non_target->target == CAP_NONE) {
                    // this is an empty filename string which signifies
                    // "ask the user"
                    // however we do not support "ask-user" and target
                    // auto-detect
                    // combined at this time ... so we change to default
                    // filename
                    errors = xvc_errors_append(13, errors, lapp);
                    if (!errors) {
                        *rc = -1;
                        return NULL;
                    }
                    *rc = 1;
                    return errors;
                } else {
                    // this is "ask-user"
                    if ((lapp->flags & FLG_AUTO_CONTINUE) > 0)
                        errors = xvc_errors_append(14, errors, lapp);
                    else
                        errors = xvc_errors_append(15, errors, lapp);
                    if (!errors) {
                        *rc = -1;
                        return NULL;
                    }
                }

            }
        }
        // end: file

        // start: target
        // first find auto-detection results if auto-detection is turned
        // on
        t_format = non_target->target;
        t_codec = non_target->targetCodec;
        if (!t_format)
            t_format =
                xvc_codec_get_target_from_filename(non_target->file);
        if (!t_codec)
            t_codec = tFFormats[t_format].def_vid_codec;

#ifdef HAVE_FFMPEG_AUDIO
        t_au_codec = non_target->au_targetCodec;
        if (!t_au_codec)
            t_au_codec = tFFormats[t_format].def_au_codec;
#endif // HAVE_FFMPEG_AUDIO

        // is target valid and does it match the current_mode?
        // the next includes t_format == 0 ... which is at this stage an
        // unresolved
        // auto-detection
        if (t_format <= CAP_NONE || t_format >= NUMCAPS) {
            errors =
                xvc_errors_append((lapp->current_mode == 1) ? 16 : 17,
                                  errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
            *rc = 1;
            return errors;
        }
        else if (lapp->current_mode == 1 && t_format >= CAP_FFM) {
            errors = xvc_errors_append(18, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
            *rc = 1;
            return errors;
        } else if (lapp->current_mode == 0 && t_format < CAP_FFM) {
            errors = xvc_errors_append(19, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
            *rc = 1;
            return errors;
        }
        // does targetCodec match the target
        if (t_format < CAP_FFM && non_target->targetCodec != CODEC_NONE) {
            errors = xvc_errors_append(20, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
        if (non_target->target == CAP_NONE
            && non_target->targetCodec != CODEC_NONE) {
            // if target is auto-detected we also want to auto-detect
            // targetCodec
            errors =
                xvc_errors_append((lapp->current_mode == 1) ? 21 : 22,
                                  errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
            // a failure of xvc_is_element (...) is only relevant if
            // allowed_vid_codecs
            // is not NULL
        } else if (tFFormats[t_format].allowed_vid_codecs &&
                   (!xvc_is_element(tFFormats[t_format].allowed_vid_codecs,
                                    tCodecs[t_codec].name))) {
#ifdef DEBUG
            printf("app_data: format %i %s - codec %i %s\n", t_format,
                   tFFormats[t_format].name, t_codec,
                   tCodecs[t_codec].name);
#endif                          // DEBUG

            errors = xvc_errors_append(23, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
            // FIXME: do I need to return here?
            // *rc = 1;
            // return errors;
        }
#ifdef HAVE_FFMPEG_AUDIO
        // only check audio settings if audio capture is enabled
        if ( non_target->audioWanted == 1 ) {
            // does au_targetCodec match target
            if (t_format < CAP_FFM && non_target->au_targetCodec != CODEC_NONE) {
                errors = xvc_errors_append(24, errors, lapp);
                if (!errors) {
                    *rc = -1;
                    return NULL;
                }
            } else

            if (non_target->target == CODEC_NONE
                    && non_target->au_targetCodec != CODEC_NONE) {
                // if target is auto-detected we also want to auto-detect
                // au_targetCodec
                errors = xvc_errors_append(25, errors, lapp);
                if (!errors) {
                    *rc = -1;
                    return NULL;
                }
                // a failure of xvc_is_element (...) is only relevant if
                // allowed_au_codecs
                // is not NULL
            } else if (tFFormats[t_format].allowed_au_codecs &&
                    (!xvc_is_element(tFFormats[t_format].allowed_au_codecs,
                                    tAuCodecs[t_au_codec].name))) {
                errors = xvc_errors_append(26, errors, lapp);
                if (!errors) {
                    *rc = -1;
                    return NULL;
                }
                // FIXME: do I need to return here?
                // *rc = 1;
                // return errors;
            } else if (tFFormats[t_format].allowed_au_codecs == NULL
                    && non_target->audioWanted) {
                errors =
                    xvc_errors_append((lapp->current_mode == 1) ? 42 : 43,
                                    errors, lapp);
                if (!errors) {
                    *rc = -1;
                    return NULL;
                }
            }
        }
#endif // HAVE_FFMPEG_AUDIO        
        // end: target

        // targetCodec ... check for suitable format is above with target

        // au_targetCodec ... check for suitable format is above with
        // target

        // start: fps
        if (lapp->current_mode == 0
            && (xvc_codec_is_valid_fps(non_target->fps, t_codec) == 0)) {
            errors = xvc_errors_append(27, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        } else if (lapp->current_mode == 1 && non_target->fps <= 0) {
            errors = xvc_errors_append(28, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
        // end: fps

        // start: time
        if (non_target->time < 0) {
            errors =
                xvc_errors_append((lapp->current_mode == 1) ? 29 : 30,
                                  errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
        // end: time

        // start: frames
        if (non_target->time < 0) {
            errors =
                xvc_errors_append((lapp->current_mode == 1) ? 31 : 32,
                                  errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
        // end: frames

        // start: start_no
        if (non_target->start_no < 0) {
            errors =
                xvc_errors_append((lapp->current_mode == 1) ? 33 : 34,
                                  errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
        // end: start_no

        // start: step
        if (non_target->step <= 0) {
            errors =
                xvc_errors_append((lapp->current_mode == 1) ? 35 : 36,
                                  errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        } else if (lapp->current_mode == 0 && non_target->step != 1) {
            errors = xvc_errors_append(36, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
        // end: step

        // start: quality
        if ((non_target->quality < 0) || (non_target->quality > 100)) {
            errors =
                xvc_errors_append((lapp->current_mode == 1) ? 40 : 41,
                                  errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
        // end: quality

        // audioWanted is checked with target 

        /* 
         * dunno about valid rates for the following 
         * FIXME: implement some 
         * sanity checks int sndrate; // sound sample rate int sndsize;
         * // bits to sample for audio capture int sndchannels; // number
         * of channels to record audio to 
         */

        /* 
         * FIXME: can there be a meaningful check to validate the following?
         *
         char *play_cmd; // command to use for animate function
         char *video_cmd; // command to use for make video function
         char *edit_cmd; // command to use for edit function
         */

    }
#endif // USE_FFMPEG


    if (errors)
        *rc = 1;
    else
        *rc = 0;
#ifdef DEBUG
    printf("%s %s: Leaving %s errors\n", DEBUGFILE, DEBUGFUNCTION, ((errors)?"with":"without"));
#endif // DEBUG

    return errors;
#undef DEBUGFUNCTION
}



/* 
 * merges a CapTypeOptions structure into an AppData as current target
 */
int xvc_merge_cap_type_and_app_data(CapTypeOptions * cto, AppData * lapp)
{
    #define DEBUGFUNCTION "xvc_merge_cap_type_and_app_data()"
    CapTypeOptions *target = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

#ifdef USE_FFMPEG
    // we need current_mode determined by now
    if (lapp->current_mode < 0)
        return 0;
    if (lapp->current_mode == 0)
        target = &(lapp->single_frame);
    else
        target = &(lapp->multi_frame);
#else // USE_FFMPEG
    // we only have single frame capture
    if (lapp->current_mode != 0)
        printf(_("%s %s: Capture mode not single_frame (%i) but we don't have ffmpeg ... correcting, but smth's wrong\n"), 
                DEBUGFILE, DEBUGFUNCTION, lapp->current_mode);
    
    lapp->current_mode = 0;
    target = &(lapp->single_frame);
#endif // USE_FFMPEG

    // set the capture options of the right mode to the specified values
    if (cto->file != NULL)
        target->file = strdup(cto->file);
    if (cto->target > -1)
        target->target = cto->target;
    if (cto->targetCodec > -1)
        target->targetCodec = cto->targetCodec;
    if (cto->fps > -1)
        target->fps = cto->fps;
    if (cto->time > -1)
        target->time = cto->time;
    if (cto->frames > -1)
        target->frames = cto->frames;
    if (cto->start_no > -1)
        target->start_no = cto->start_no;
    if (cto->step > -1)
        target->step = cto->step;
    if (cto->quality > -1)
        target->quality = cto->quality;
    if (cto->bpp > -1)
        target->bpp = cto->bpp;
#ifdef HAVE_FFMPEG_AUDIO        
    if (cto->au_targetCodec > -1)
        target->au_targetCodec = cto->au_targetCodec;
    if (cto->audioWanted > -1)
        target->audioWanted = cto->audioWanted;
    if (cto->sndrate > -1)
        target->sndrate = cto->sndrate;
    if (cto->sndsize > -1)
        target->sndsize = cto->sndsize;
    if (cto->sndchannels > -1)
        target->sndchannels = cto->sndchannels;
#endif // HAVE_FFMPEG_AUDIO
    if (cto->play_cmd != NULL)
        target->play_cmd = cto->play_cmd;
    if (cto->video_cmd != NULL)
        target->video_cmd = cto->video_cmd;
    if (cto->edit_cmd != NULL)
        target->edit_cmd = cto->edit_cmd;

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    return 1;

    #undef DEBUGFUNCTION
}



/* 
 * error handling
 * each error has at least one xvError struct and possibly a function
 * for use as a default action
 */

void xverror_exit_action(void *err)
{
    exit(1);
}

void xverror_null_action(void *err)
{
    // empty
}

void xverror_1_action(void *err)
{
    xvErrorListItem *cerr = (xvErrorListItem *) err;
    fprintf(stderr, "%s\n", cerr->err->short_msg);
    fprintf(stderr, "%s\n", cerr->err->long_msg);
}

const xvError one = {
    1,
    XV_ERR_ERROR,
    "error 1",
    "this is my first error",
    xverror_1_action,
    "Display the error messages (smth. that should normally be done outside the default action)"
};

void xverror_2_action(void *err)
{
    xvErrorListItem *cerr = (xvErrorListItem *) err;
    cerr->app->flags &= ~FLG_SOURCE;
    cerr->app->source = "x11";
}

const xvError two = {
    2,
    XV_ERR_ERROR,
    N_("No V4L available"),
    N_("Video for Linux is selected as the capture source but V4L is not available with this binary."),
    xverror_2_action,
    N_("Set capture source to 'x11'")
};

const xvError three = {
    3,
    XV_ERR_FATAL,
    N_("V4L device inaccessible"),
    N_("Can't open the specified Video for Linux device."),
    xverror_exit_action,
    N_("Quit")
};

const xvError four = {
    4,
    XV_ERR_FATAL,
    N_("V4L can't capture"),
    N_("The specified Video for Linux device can't capture to memory."),
    xverror_exit_action,
    N_("Quit")
};

void xverror_5_action(void *err)
{
#ifdef HasVideo4Linux
    xvErrorListItem *cerr = (xvErrorListItem *) err;
    VIDEO *video;

    if (strchr(lapp->source, ':') != NULL) {
        lapp->device = strchr(lapp->source, ':') + 1;
    }

    video = video_open(lapp->device, O_RDWR);
    if (!video) {
        xvc_errors_write_error_msg(3);
        xvc_errors_write_action_msg(3);
        (*xvErrors[3].action) (NULL);
    }

    cerr->app->cap_width = min(cerr->app->cap_width, video->maxwidth);
    cerr->app->cap_height = min(cerr->app->cap_height, video->maxheight);
    cerr->app->cap_width = max(cerr->app->cap_width, video->minwidth);
    cerr->app->cap_height = max(cerr->app->cap_height, video->minheight);

    video_close(video);
#endif                          // HasVideo4Linux
}

const xvError five = {
    5,
    XV_ERR_ERROR,
    N_("Capture size exceeds V4L limits"),
    N_("The specified capture size exceeds the limits for Video for Linux capture."),
    xverror_5_action,
    N_("Increase or decrease the capture area to conform to V4L's limits")
};

void xverror_6_action(void *err)
{
    Display *dpy;
    int max_width, max_height;
    xvErrorListItem *cerr = (xvErrorListItem *) err;

    dpy = XOpenDisplay(NULL);
    max_width = WidthOfScreen(DefaultScreenOfDisplay(dpy));
    max_height = HeightOfScreen(DefaultScreenOfDisplay(dpy));

    cerr->app->cap_width = max_width - cerr->app->cap_pos_x;
    cerr->app->cap_height = max_height - cerr->app->cap_pos_y;
    XCloseDisplay(dpy);
}

const xvError six = {
    6,
    XV_ERR_WARN,
    N_("Capture area outside screen"),
    N_("The specified capture area reaches beyond the visible screen."),
    xverror_6_action,
    N_("Clip capture size to visible screen")
};

void xverror_7_action(void *err)
{
    xvErrorListItem *cerr = (xvErrorListItem *) err;

    cerr->app->mouseWanted = 1;
}

const xvError seven = {
    7,
    XV_ERR_WARN,
    N_("Invalid Mouse Capture Option"),
    N_("The specified value for the mouse capture option is not one of the valid values (0 = no capture, 1 = white mouse pointer, 2 = black mouse pointer)."),
    xverror_7_action,
    N_("Set Mouse Capture Option to '1' for a white mouse pointer")
};

void xverror_8_action(void *err)
{
    xvErrorListItem *cerr = (xvErrorListItem *) err;

    cerr->app->flags &= ~FLG_SOURCE;
    cerr->app->source = "x11";
}

const xvError eight = {
    8,
    XV_ERR_WARN,
    N_("Invalid Capture Source"),
    N_("An unsupported capture source was selected."),
    xverror_8_action,
    N_("Reset capture source to fail-safe 'x11'")
};

void xverror_9_action(void *err)
{
    xvErrorListItem *cerr = (xvErrorListItem *) err;

    cerr->app->default_mode = 0;
}

const xvError nine = {
    9,
    XV_ERR_WARN,
    N_("Invalid default capture mode"),
    N_("The default capture mode specified is neither \"single-frame\" nor \"mult-frame\"."),
    xverror_9_action,
    N_("Reset default capture mode to \"single-frame\"")
};

const xvError ten = {
    10,
    XV_ERR_FATAL,
    "File name is NULL",
    "The filename string variable used for single-frame capture is a null pointer. This should never be possible at this stage.",
    xverror_exit_action,
    N_("Quit")
};

const xvError eleven = {
    11,
    XV_ERR_FATAL,
    N_("File name is NULL"),
    N_("The filename string variable used for multi-frame capture is a null pointer. This should never be possible at this stage."),
    xverror_exit_action,
    N_("Quit")
};

// error twelve is for zero length filenames with target
// cap_type_options only ... for non target we don't make
// any change and we can't make the distinction from within
// the error
// this is only an error for single-frame ... multi-frame uses
// empty filenames to signify ask-user
void xverror_12_action(void *err)
{
    #define DEBUGFUNCTION "xverror_12_action()"
    xvErrorListItem *cerr = (xvErrorListItem *) err;
    CapTypeOptions *target = NULL;
    char *fn = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

#ifdef USE_FFMPEG
    if (cerr->app->current_mode == 0)
        target = &(cerr->app->single_frame);
    else
        target = &(cerr->app->multi_frame);
#else // USE_FFMPEG
    // we only have single frame capture
    if (cerr->app->current_mode != 0)
        printf(_("%s %s: Capture mode not single_frame (%i) but we don't have ffmpeg ... correcting, but smth's wrong\n"), 
                DEBUGFILE, DEBUGFUNCTION, cerr->app->current_mode);
    
    cerr->app->current_mode = 0;
    target = &(cerr->app->single_frame);
#endif // USE_FFMPEG

    if (target->target > 0 && target->target < NUMCAPS) {
        char tmp_fn[512];
        char *extension;
        int i, n, m;

        sprintf(tmp_fn, "test-%s\0", "%04d.");
        extension = xvc_next_element(tFFormats[target->target].extensions);
        n = strlen(tmp_fn);
        for (i = n, m = 0; i < (n + strlen(extension)); i++, m++) {
            tmp_fn[i] = extension[m];
        }
        i++;
        tmp_fn[i] = '\0';
        fn = strdup(tmp_fn);
    } else {
#ifdef USE_FFMPEG
        if (cerr->app->current_mode == 0)
#endif // USE_FFMPEG
            fn = "test-%04d.xwd";
#ifdef USE_FFMPEG
        else
            fn = "test-%04d.mpeg";
#endif // USE_FFMPEG
        target->target = 0;
    }

    target->file = strdup(fn);

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    #undef DEBUGFUNCTION
}

const xvError twelve = {
    12,
    XV_ERR_ERROR,
    N_("File name empty"),
    N_("You selected single-frame capture without specifying a filename."),
    xverror_12_action,
    N_("Set the filename used for single-frame capture to a default value based on the current capture mode and target")
};

// error twentytwo is for zero length filenames with target
// cap_type_options only ... we're reusing action 11

const xvError thirteen = {
    13,
    XV_ERR_ERROR,
    N_("File format Auto-detection with empty file name"),
    N_("You want to be asked for a filename to save your video to after recording and also want to auto-detect the file type based on filename. That won't work."),
    xverror_12_action,
    N_("Set the filename used for multi-frame capture to a default value based on the current capture mode and target")
};

void xverror_14_action(void *err)
{
    xvErrorListItem *cerr = (xvErrorListItem *) err;

    // the temporary filename needs to be set when the job is created
    // because we don't want to store the temporary filename in
    // preferences
    // this just disables autocontinue
    cerr->app->flags &= ~FLG_AUTO_CONTINUE;
}

const xvError fourteen = {
    14,
    XV_ERR_WARN,
    N_("File name empty, ask user"),
    N_("You specified an empty filename for multi-frame capture. Since you have configured a fixed output file-format xvidcap can ask you for a filename to use after recording. We'll need to disable the autocontinue function you selected, though."),
    xverror_14_action,
    N_("Enable \"ask user for filename\" mode for multi-frame capture and disable autocontinue")
};

const xvError fifteen = {
    15,
    XV_ERR_INFO,
    N_("File name empty, ask user"),
    N_("You specified an empty filename for multi-frame capture. Since you have configured a fixed output file-format xvidcap can ask you for a filename to use after each recording."),
    xverror_null_action,
    N_("Ignore, everything is set up alright")
};


void xverror_16_action(void *err) {
    #define DEBUGFUNCTION "xverror_16_action()"
    xvErrorListItem *cerr = (xvErrorListItem *) err;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

#ifdef USE_FFMPEG
    if (cerr->app->current_mode == 0) {
        cerr->app->single_frame.target = CAP_XWD;
    } else {
        cerr->app->multi_frame.target = CAP_DIVX;        
    }
#else // USE_FFMPEG
    // we only have single frame capture
    if (cerr->app->current_mode != 0)
        printf(_("%s %s: Capture mode not single_frame (%i) but we don't have ffmpeg ... correcting, but smth's wrong\n"), 
                DEBUGFILE, DEBUGFUNCTION, cerr->app->current_mode);
    
    cerr->app->current_mode = 0;
    cerr->app->single_frame.target = CAP_XWD;
#endif // USE_FFMPEG

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    #undef DEBUGFUNCTION
}

const xvError sixteen = {
    16,
    XV_ERR_ERROR,
    N_("Invalid file format"),
    N_("You selected an unknown or invalid file format for single-frame capture. Check the --format-help option."),
    xverror_16_action,
    N_("Reset to default format for single-frame capture (XWD)")
};

const xvError seventeen = {
    17,
    XV_ERR_ERROR,
    N_("Invalid file format"),
    N_("You selected an unknown or invalid file format for multi-frame capture. Check the --format-help option."),
    xverror_16_action,
    N_("Reset to default format for multi-frame caputre (MPEG4)")
};

void xverror_18_action(void *err)
{
    xvErrorListItem *cerr = (xvErrorListItem *) err;

    cerr->app->single_frame.target = CAP_XWD;
}

const xvError eighteen = {
    18,
    XV_ERR_ERROR,
    N_("Single-Frame Capture with Multi-Frame File Format"),
    N_("You selected single-frame capture mode but specified a target file format which is used for multi-frame capture."),
    xverror_18_action,
    N_("Reset file format to default for single-frame capture")
};

#ifdef USE_FFMPEG
void xverror_19_action(void *err)
{
    xvErrorListItem *cerr = (xvErrorListItem *) err;

    cerr->app->multi_frame.target = CAP_MPG;
}

const xvError nineteen = {
    19,
    XV_ERR_ERROR,
    N_("Multi-Frame Capture with Single-Frame File Format"),
    N_("You selected multi-frame capture mode but specified a target file format which is used for single-frame capture."),
    xverror_19_action,
    N_("Reset file format to default for multi-frame capture")
};
#endif                          // USE_FFMPEG

const xvError twenty = {
    20,
    XV_ERR_INFO,
    N_("Single-Frame Capture with invalid target codec"),
    N_("You selected single-frame capture mode and specified a target codec not valid with the file format."),
    xverror_null_action,
    N_("Ignore because single-frame capture types always have a well-defined codec")
};

const xvError twentyone = {
    21,
    XV_ERR_INFO,
    N_("File Format auto-detection may conflict with explicit codec"),
    N_("For single-frame capture you requested auto-detection of the output file format and explicitly specified a target codec. Your codec selection may not be valid for the file format eventually detected, in which case it will be overridden by the format's default codec."),
    xverror_null_action,
    N_("Ignore")
};

const xvError twentytwo = {
    22,
    XV_ERR_INFO,
    N_("File Format auto-detection may conflict with explicit codec"),
    N_("For multi-frame capture you requested auto-detection of the output file format and explicitly specified a target codec. Your codec selection may not be valid for the file format eventually detected, in which case it will be overridden by the format's default codec."),
    xverror_null_action,
    N_("Ignore")
};

#ifdef USE_FFMPEG
void xverror_23_action(void *err)
{
    xvErrorListItem *cerr = (xvErrorListItem *) err;

    cerr->app->multi_frame.targetCodec =
        tFFormats[cerr->app->multi_frame.target].def_vid_codec;
}

const xvError twentythree = {
    23,
    XV_ERR_ERROR,
    N_("Multi-Frame Capture with invalid codec"),
    N_("You selected multi-frame capture mode and specified a target codec not valid with the file format."),
    xverror_23_action,
    N_("Reset multi-frame target codec to default for file format")
};
#endif // USE_FFMPEG

const xvError twentyfour = {
    24,
    XV_ERR_INFO,
    "Single-Frame Capture with invalid target audio codec",
    "You selected single-frame capture mode and specified a target audio codec not valid with the file format.",
    xverror_null_action,
    "Ignore because single-frame capture types don't support audio capture anyway"
};

const xvError twentyfive = {
    25,
    XV_ERR_INFO,
    N_("File Format auto-detection may conflict with explicit audio codec"),
    N_("For multi-frame capture you requested auto-detection of the output file format and explicitly specified a target audio codec. Your audio codec selection may not be valid for the file format eventually detected, in which case it will be overridden by the format's default audio codec."),
    xverror_null_action,
    N_("Ignore")
};

#ifdef HAVE_FFMPEG_AUDIO
void xverror_26_action(void *err)
{
    xvErrorListItem *cerr = (xvErrorListItem *) err;

    cerr->app->multi_frame.au_targetCodec =
        tFFormats[cerr->app->multi_frame.target].def_au_codec;
}

const xvError twentysix = {
    26,
    XV_ERR_ERROR,
    N_("Multi-Frame Capture with invalid audio codec"),
    N_("You selected multi-frame capture mode and specified a target audio codec not valid with the file format."),
    xverror_26_action,
    N_("Reset multi-frame target audio codec to default for file format")
};
#endif // HAVE_FFMPEG_AUDIO

const xvError twentyseven = {
    27,
    XV_ERR_FATAL,
    N_("Invalid frame-rate for selected codec"),
    N_("You selected multi-frame capture mode but the requested frame-rate is not valid for the selected codec. This would result in the video playing back too slowly or quickly."),
    xverror_exit_action,
    N_("Quit")
};

void xverror_28_action(void *err)
{
    xvErrorListItem *cerr = (xvErrorListItem *) err;

    cerr->app->single_frame.fps = 1;
}

const xvError twentyeight = {
    28,
    XV_ERR_WARN,
    N_("Requested Frame Rate <= zero"),
    N_("You selected single-frame capture but the frame rate you requested is not greater than zero."),
    xverror_28_action,
    N_("Set frame rate to '1'")
};

void xverror_29_action(void *err)
{
    #define DEBUGFUNCTION "xverror_29_action()"
    xvErrorListItem *cerr = (xvErrorListItem *) err;
    CapTypeOptions *target = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

#ifdef USE_FFMPEG
    if (cerr->app->current_mode == 0)
        target = &(cerr->app->single_frame);
    else
        target = &(cerr->app->multi_frame);
#else // USE_FFMPEG
    // we only have single frame capture
    if (cerr->app->current_mode != 0)
        printf(_("%s %s: Capture mode not single_frame (%i) but we don't have ffmpeg ... correcting, but smth's wrong\n"), 
                DEBUGFILE, DEBUGFUNCTION, cerr->app->current_mode);
    
    cerr->app->current_mode = 0;
    target = &(cerr->app->single_frame);
#endif // USE_FFMPEG

    target->time = 0;

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    #undef DEBUGFUNCTION
}

const xvError twentynine = {
    29,
    XV_ERR_WARN,
    N_("Requested Maximum Capture Time < zero"),
    N_("For single-frame capture you specified a maximum capture time which is less than zero seconds."),
    xverror_29_action,
    N_("Set the maximum capture time for single-frame capture to unlimited")
};

const xvError thirty = {
    30,
    XV_ERR_WARN,
    N_("Requested Maximum Capture Time < zero"),
    N_("For multi-frame capture you specified a maximum capture time which is less than zero seconds."),
    xverror_29_action,
    N_("Set the maximum capture time for multi-frame capture to unlimited")
};

void xverror_31_action(void *err)
{
    #define DEBUGFUNCTION "xverror_31_action()"
    xvErrorListItem *cerr = (xvErrorListItem *) err;
    CapTypeOptions *target = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

#ifdef USE_FFMPEG
    if (cerr->app->current_mode == 0)
        target = &(cerr->app->single_frame);
    else
        target = &(cerr->app->multi_frame);
#else // USE_FFMPEG
    // we only have single frame capture
    if (cerr->app->current_mode != 0)
        printf(_("%s %s: Capture mode not single_frame (%i) but we don't have ffmpeg ... correcting, but smth's wrong\n"), 
                DEBUGFILE, DEBUGFUNCTION, cerr->app->current_mode);
    
    cerr->app->current_mode = 0;
    target = &(cerr->app->single_frame);
#endif // USE_FFMPEG

    target->frames = 0;

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    #undef DEBUGFUNCTION
}

const xvError thirtyone = {
    31,
    XV_ERR_WARN,
    N_("Requested Maximum Frames < zero"),
    N_("For single-frame capture you specified a maximum number of frames which is less than zero."),
    xverror_31_action,
    N_("Set maximum number of frames to unlimited for single-frame capture")
};

const xvError thirtytwo = {
    32,
    XV_ERR_WARN,
    N_("Requested Maximum Frames < zero"),
    N_("For multi-frame capture you specified a maximum number of frames which is less than zero."),
    xverror_31_action,
    N_("Set maximum number of frames to unlimited for multi-frame capture")
};

void xverror_33_action(void *err)
{
    #define DEBUGFUNCTION "xverror_33_action()"
    xvErrorListItem *cerr = (xvErrorListItem *) err;
    CapTypeOptions *target = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

#ifdef USE_FFMPEG
    if (cerr->app->current_mode == 0)
        target = &(cerr->app->single_frame);
    else
        target = &(cerr->app->multi_frame);
#else // USE_FFMPEG
    // we only have single frame capture
    if (cerr->app->current_mode != 0)
        printf(_("%s %s: Capture mode not single_frame (%i) but we don't have ffmpeg ... correcting, but smth's wrong\n"), 
                DEBUGFILE, DEBUGFUNCTION, cerr->app->current_mode);
    
    cerr->app->current_mode = 0;
    target = &(cerr->app->single_frame);
#endif // USE_FFMPEG

    target->start_no = 0;

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    #undef DEBUGFUNCTION
}

const xvError thirtythree = {
    33,
    XV_ERR_WARN,
    N_("Requested Start Number < zero"),
    N_("For single-frame capture you specified a start number for frame numbering which is less than zero."),
    xverror_33_action,
    N_("Set start number for frame numbering of single-frame capture to '0'")
};

const xvError thirtyfour = {
    34,
    XV_ERR_WARN,
    N_("Requested Start Number < zero"),
    N_("For multi-frame capture you specified a start number for frame numbering which is less than zero."),
    xverror_33_action,
    N_("Set start number for frame numbering of multi-frame capture to '0'")
};

void xverror_35_action(void *err)
{
    #define DEBUGFUNCTION "xverror_35_action()"
    xvErrorListItem *cerr = (xvErrorListItem *) err;
    CapTypeOptions *target = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

#ifdef USE_FFMPEG
    if (cerr->app->current_mode == 0)
        target = &(cerr->app->single_frame);
    else
        target = &(cerr->app->multi_frame);
#else // USE_FFMPEG
    // we only have single frame capture
    if (cerr->app->current_mode != 0)
        printf(_("%s %s: Capture mode not single_frame (%i) but we don't have ffmpeg ... correcting, but smth's wrong\n"), 
                DEBUGFILE, DEBUGFUNCTION, cerr->app->current_mode);
    
    cerr->app->current_mode = 0;
    target = &(cerr->app->single_frame);
#endif // USE_FFMPEG

    target->step = 1;

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    #undef DEBUGFUNCTION
}

const xvError thirtyfive = {
    35,
    XV_ERR_WARN,
    N_("Requested Frame Increment <= zero"),
    N_("For single-frame capture you specified an increment for frame numbering which is not greater than zero."),
    xverror_35_action,
    N_("Set increment for frame numbering of single-frame capture to '1'")
};

const xvError thirtysix = {
    36,
    XV_ERR_WARN,
    N_("Requested Frame Increment <> one"),
    N_("For multi-frame capture you specified an increment for frame numbering which is not exactly one."),
    xverror_35_action,
    N_("Set increment for frame numbering of multi-frame capture to '1'")
};

void xverror_37_action(void *err)
{
    #define DEBUGFUNCTION "xverror_37_action()"
    xvErrorListItem *cerr = (xvErrorListItem *) err;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    cerr->app->rescale = 100;

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    #undef DEBUGFUNCTION
}

const xvError thirtyseven = {
    37,
    XV_ERR_WARN,
    N_("Requested invalid Rescale Percentage"),
    N_("You requested rescaling of the input area to a relative size but did not specify a valid percentage. Valid values are between 1 and 100 "),
    xverror_37_action,
    N_("Set rescale percentage to '100'")
};

void xverror_40_action(void *err)
{
    #define DEBUGFUNCTION "xverror_40_action()"
    xvErrorListItem *cerr = (xvErrorListItem *) err;
    CapTypeOptions *target = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

#ifdef USE_FFMPEG
    if (cerr->app->current_mode == 0)
        target = &(cerr->app->single_frame);
    else
        target = &(cerr->app->multi_frame);
#else // USE_FFMPEG
    // we only have single frame capture
    if (cerr->app->current_mode != 0)
        printf(_("%s %s: Capture mode not single_frame (%i) but we don't have ffmpeg ... correcting, but smth's wrong\n"), 
                DEBUGFILE, DEBUGFUNCTION, cerr->app->current_mode);
    
    cerr->app->current_mode = 0;
    target = &(cerr->app->single_frame);
#endif // USE_FFMPEG

    target->quality = 1;

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    #undef DEBUGFUNCTION
}

const xvError fourty = {
    40,
    XV_ERR_WARN,
    N_("Quality value not a valid percentage"),
    N_("For single-frame capture the value for recording quality is not a percentage between 1 and 100."),
    xverror_40_action,
    N_("Set recording quality to '75'")
};

const xvError fourtyone = {
    41,
    XV_ERR_WARN,
    N_("Quality value not a valid percentage"),
    N_("For multi-frame capture the value for recording quality is not a percentage between 1 and 100."),
    xverror_40_action,
    N_("Set recording quality to '75'")
};

#ifdef HAVE_FFMPEG_AUDIO
void xverror_42_action(void *err)
{
    #define DEBUGFUNCTION "xverror_42_action()"
    xvErrorListItem *cerr = (xvErrorListItem *) err;
    CapTypeOptions *target = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    if (cerr->app->current_mode == 0)
        target = &(cerr->app->single_frame);
    else
        target = &(cerr->app->multi_frame);

    target->audioWanted = 0;

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    #undef DEBUGFUNCTION
}

const xvError fourtytwo = {
    42,
    XV_ERR_WARN,
    N_("Audio Capture not supported by File Format"),
    N_("You selected audio capture but single-frame captures do not support it."),
    xverror_42_action,
    N_("Disable audio capture for single-frame capture")
};

const xvError fourtythree = {
    43,
    XV_ERR_WARN,
    N_("Audio Capture not supported by File Format"),
    N_("For multi-frame capture you selected audio support but picked a file format without support for audio."),
    xverror_42_action,
    N_("Disable audio capture for multi-frame capture")
};
#endif // HAVE_FFMPEG_AUDIO




/* 
 * initializes the error array
 */
void xvc_errors_init()
{
    int i = 0;

    xvErrors[i++] = one;        // this is actually zero
    xvErrors[i++] = one;
    xvErrors[i++] = two;
    xvErrors[i++] = three;
    xvErrors[i++] = four;
    xvErrors[i++] = five;
    xvErrors[i++] = six;
    xvErrors[i++] = seven;
    xvErrors[i++] = eight;
    xvErrors[i++] = nine;
    xvErrors[i++] = ten;
    xvErrors[i++] = eleven;
    xvErrors[i++] = twelve;
    xvErrors[i++] = thirteen;
    xvErrors[i++] = fourteen;
    xvErrors[i++] = fifteen;
    xvErrors[i++] = sixteen;
    xvErrors[i++] = seventeen;
    xvErrors[i++] = eighteen;
#ifdef USE_FFMPEG
    xvErrors[i++] = nineteen;
#endif
    xvErrors[i++] = twenty;
    xvErrors[i++] = twentyone;
    xvErrors[i++] = twentytwo;
#ifdef USE_FFMPEG
    xvErrors[i++] = twentythree;
#endif // USE_FFMPEG
#ifdef HAVE_FFMPEG_AUDIO
    xvErrors[i++] = twentyfour;
    xvErrors[i++] = twentyfive;
    xvErrors[i++] = twentysix;
#endif // HAVE_FFMPEG_AUDIO
    xvErrors[i++] = twentyseven;
    xvErrors[i++] = twentyeight;
    xvErrors[i++] = twentynine;
    xvErrors[i++] = thirty;
    xvErrors[i++] = thirtyone;
    xvErrors[i++] = thirtytwo;
    xvErrors[i++] = thirtythree;
    xvErrors[i++] = thirtyfour;
    xvErrors[i++] = thirtyfive;
    xvErrors[i++] = thirtysix;
    xvErrors[i++] = thirtyseven;
//    xvErrors[i++] = thirtyeight;
//    xvErrors[i++] = thirtynine;
    xvErrors[i++] = fourty;
    xvErrors[i++] = fourtyone;
#ifdef HAVE_FFMPEG_AUDIO
    xvErrors[i++] = fourtytwo;
    xvErrors[i++] = fourtythree;
#endif // HAVE_FFMPEG_AUDIO

    // the next one is used to terminate the list in the array
    // don't depend on NUMERRORS
    xvErrors[i].code = 0;
}


/* 
 * adds a new error to an error list
 */
xvErrorListItem *xvc_errors_append(int code, xvErrorListItem * err,
                                   AppData * app)
{
    xvErrorListItem *new_err, *iterator = NULL, *last = NULL;
    int i, a = -1;

    for (i = 0; xvErrors[i].code > 0; i++) {
        if (xvErrors[i].code == code)
            a = i;
    }

    // could not find error to add to list
    if (a < 0)
        return NULL;
    new_err = malloc(sizeof(struct _xvErrorListItem));
    if (!new_err)
        return NULL;

    new_err->err = &(xvErrors[a]);
    new_err->app = app;
    new_err->next = new_err->previous = NULL;

    if (err == NULL) {
        // new list
        new_err->previous = NULL;
        err = new_err;
    } else {
        last = err;
        iterator = err->next;
        while (iterator) {
            last = iterator;
            iterator = iterator->next;
        }
        last->next = new_err;
        new_err->previous = last;
    }

    return err;
}

/* 
 * delete error list
 */
xvErrorListItem *xvc_errors_delete_list(xvErrorListItem * err)
{
    xvErrorListItem *iterator = NULL, *last = NULL;

    if (!err)
        return NULL;

    last = err;
    iterator = err->next;
    while (iterator) {
        last = iterator;
        iterator = iterator->next;
    }

    iterator = last;
    while (iterator) {
        last = iterator->previous;
        free(iterator);
        iterator = last;
    }

    return iterator;
}


/* 
 * print error msg
 */

int find_line_length(char *txt, char delim, int len)
{
    char buf[200];
    char *cont;
    int i = 0;

    if (!txt)
        return 0;
    if (strlen(txt) < len)
        return strlen(txt);

    strncpy(buf, txt, len);
    cont = rindex(buf, (int) delim);
    i = ((int) cont - (int) buf);
    i++;

    return i;
}

void lineprint(char *txt, int out_or_err, int pre, int len)
{
    int i = 0, n, inc;
    char buf[200];

    if (!txt)
        return;

    while (strlen(txt) > (i + len)) {

        for (n = 0; n < pre; n++) {
            if (out_or_err)
                fprintf(stderr, " ");
            else
                fprintf(stdout, " ");
        }

        inc = find_line_length((txt + i), ' ', len);
        strncpy(buf, (txt + i), inc);
        buf[inc] = '\0';
        if (out_or_err)
            fprintf(stderr, "%s\n", buf);
        else
            fprintf(stdout, "%s\n", buf);

        i += inc;
    }


    for (n = 0; n < pre; n++) {
        if (out_or_err)
            fprintf(stderr, " ");
        else
            fprintf(stdout, " ");
    }

    inc = find_line_length((txt + i), ' ', len);
    strncpy(buf, (txt + i), inc);
    buf[inc] = '\0';
    if (out_or_err)
        fprintf(stderr, "%s\n", buf);
    else
        fprintf(stdout, "%s\n", buf);

}

void xvc_errors_write_error_msg(int code, int print_action_or_not)
{
    char *type = NULL;
    char scratch[50], buf[5000];
    xvError *err = &(xvErrors[code]);
    int i, inc, pre = 10, len = 80;

    switch (err->type) {
    case 1:
        type = _("FATAL ERROR");
        break;
    case 2:
        type = _("ERROR");
        break;
    case 3:
        type = _("WARNING");
        break;
    case 4:
        type = _("INFO");
        break;
    default:
        type = _("UNKNOWN");
    }
    // FIXME: pretty print later ...
    sprintf(scratch, "== %s \0", type);
    fprintf(stderr, "%s", scratch);
    for (i = strlen(scratch); i < len; i++) {
        fprintf(stderr, "=");
    }
    fprintf(stderr, "\n");

    sprintf(scratch, _("CODE %i:\0"), err->code);
    fprintf(stderr, "%s", scratch);
    for (i = strlen(scratch); i < pre; i++) {
        fprintf(stderr, " ");
    }

    if (strlen(err->short_msg) > 52)
        inc = find_line_length(_(err->short_msg), ' ', (len - pre));
    else
        inc = strlen(_(err->short_msg));
    strncpy(buf, _(err->short_msg), inc);
    buf[inc] = '\0';
    fprintf(stderr, "%s\n", buf);
    if (strlen(_(err->short_msg)) > inc) {
        strncpy(buf, (_(err->short_msg) + inc), 5000);
        lineprint(buf, 1, pre, (len - pre));
    }

    if (app->flags & FLG_RUN_VERBOSE) {
        sprintf(scratch, _("DESC:\0"));
        fprintf(stderr, "%s", scratch);
        for (i = strlen(scratch); i < pre; i++) {
            fprintf(stderr, " ");
        }

        if (strlen(_(err->long_msg)) > (len - pre))
            inc = find_line_length(_(err->long_msg), ' ', (len - pre));
        else
            inc = strlen(_(err->long_msg));
        strncpy(buf, _(err->long_msg), inc);
        buf[inc] = '\0';
        fprintf(stderr, "%s\n", buf);
        if (strlen(_(err->long_msg)) > inc) {
            strncpy(buf, (_(err->long_msg) + inc), 5000);
            lineprint(buf, 1, pre, (len - pre));
        }
    }

    if (print_action_or_not) {
        xvc_errors_write_action_msg(code);
    }

    for (i = 0; i < len; i++) {
        fprintf(stderr, "-");
    }
    fprintf(stderr, "\n");

}

/* 
 * print action msg
 */
void xvc_errors_write_action_msg(int code)
{
    xvError *err = &(xvErrors[code]);
    int i, inc, pre = 10, len = 80;
    char scratch[50], buf[5000];

    sprintf(scratch, _("ACTION:\0"));
    fprintf(stderr, "%s", scratch);
    for (i = strlen(scratch); i < pre; i++) {
        fprintf(stderr, " ");
    }

    if (strlen(_(err->action_msg)) > (len - pre))
        inc = find_line_length(_(err->action_msg), ' ', (len - pre));
    else
        inc = strlen(_(err->action_msg));
    strncpy(buf, _(err->action_msg), inc);
    buf[inc] = '\0';
    fprintf(stderr, "%s\n", buf);
    if (strlen(_(err->action_msg)) > inc) {
        strncpy(buf, (_(err->action_msg) + inc), 5000);
        lineprint(buf, 1, pre, (len - pre));
    }

}



/* 
 * execute command 
 * flag = 0 : send filename pattern literally
 * flag = 1 : replace number pattern by wildcards
 * flap = 2 : replace number pattern by the number passed as number
 *
 * file : file-name pattern
 * fframe : first frame
 * lframe : last frame
 * width : width
 * height : height
 * fps : fps
 *
 */
void
xvc_command_execute(char *command, int flag, int number, char *file,
                    int fframe, int lframe, int width, int height,
                    int fps)
{
    #define DEBUGFUNCTION "xvc_command_execute()"
    int buflength = (PATH_MAX * 5) + 1;
    char buf[buflength];
    char *myfile = NULL;

#ifdef DEBUG
    printf("%s %s: Entering with flag '%i'\n", DEBUGFILE, DEBUGFUNCTION, flag);
#endif // DEBUG

    switch (flag) {
    case 0:
        myfile = file;
        break;
    case 1:
        {
            char file_buf[PATH_MAX + 1], *p;
            int i = 0, n = 0;

            p = file;
            while (*p && (p - file) <= PATH_MAX) {
                if (*p == '%') {
                    p++;
                    sscanf(p, "%dd", &n);
                    if ((p - file + n) <= PATH_MAX) {
                        while (n) {
                            file_buf[i++] = '?';
                            n--;
                        }
                    }
                    while (isdigit(*p))
                        p++;
                    p++;
                } else {
                    file_buf[i] = *p;
                    p++;
                    i++;
                }
            }
            file_buf[i] = '\0';

            myfile = strdup(file_buf);
        }
        break;
    case 2:
        if (xvc_is_filename_mutable(file)) {
            char lfile[PATH_MAX + 1];

            snprintf(lfile, (PATH_MAX + 1), file, number);
            myfile = strdup(lfile);
        } else {
            myfile = file;
        }
        break;
    default:
        break;

    }
    
    snprintf(buf, buflength, 
             "XVFFRAME=\"%i\" XVLFRAME=\"%i\" XVWIDTH=\"%i\" XVHEIGHT=\"%i\" XVFPS=\"%f\" XVTIME=\"%f\" XVFILE=\"%s\" ; %s",
             fframe, lframe, width, height, ((float) fps / 100), 
             (float) 1000 / ((float) fps / 100),
             myfile, command);

#ifdef DEBUG
    printf("%s %s: calling: '%s'\n", DEBUGFILE, DEBUGFUNCTION, buf);
#endif // DEBUG
    system(buf);

    // sf-animate: flag 1
    // cmd: animate "${XVFILE}" -delay $((XVTIME/10)) &
    // sf-encode: flag 0
    // cmd: ppm2mpeg "${XVFILE}" ${XVFFRAME} ${XVLFRAME} ${XVWIDTH}
    // ${XVHEIGHT} ${XVFPS} ${XVTIME} &
    // sf-edit: flag 2
    // gimp "${XVFILE}" &
    
#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG    
    #undef DEBUGFUNCTION
}



/* 
 * checks if filename has pattern
 */
Boolean xvc_is_filename_mutable(char *filename)
{
    char file[PATH_MAX + 1];

    snprintf(file, (PATH_MAX + 1), filename, 1);
    if (strcmp(file, filename) == 0) {
        return FALSE;
    } else {
        return TRUE;
    }
}


/* 
 * extract number with two digits behind the comma from string
 * the function returns e.g. 2997 for 29.97 or 29.969
 */
int xvc_get_int_from_float_string(char *input)
{
    int pre = 0, post = 0, tenth = 0, hundreth = 0, thousandth = 0;
    char *ptr, *linput = strdup(input);

    pre = (int) strtol(linput, &ptr, 10);
    if (ptr != '\0')
        ptr++;
    if (ptr != '\0' && isdigit(ptr[0]))
        tenth = (int) ptr[0] - 48;
    if (ptr != '\0' && isdigit(ptr[1]))
        hundreth = (int) ptr[1] - 48;
    if (ptr != '\0' && isdigit(ptr[2]))
        thousandth = (int) ptr[2] - 48;
    if (thousandth >= 5)
        hundreth++;
    post = (tenth * 10) + hundreth;

    return ((pre * 100) + post);
}
