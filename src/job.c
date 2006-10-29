/* 
 * job.c
 *
 * Copyright (C) 1997,98 Rasca, Berlin
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
 *
 * This file defines the job data structure and some functions to
 * set/get info about a job (kinda tries to be OO ;S )
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <limits.h>             // PATH_MAX
#include <X11/Intrinsic.h>

#include "job.h"
#include "capture.h"
#include "xtoxwd.h"
#include "frame.h"
#include "colors.h"
#include "codecs.h"
#include "control.h"
#include "xvidcap-intl.h"
#ifdef USE_FFMPEG
# include "xtoffmpeg.h"
#endif                          // USE_FFMPEG


#define DEBUGFILE "job.c"

static Job *job;

extern xvCodec tCodecs[NUMCODECS];
extern xvFFormat tFFormats[NUMCAPS];

extern pthread_mutex_t recording_mutex;
extern pthread_cond_t recording_condition_unpaused;


int job_set_sound_dev(char *, int, int, int);
void job_set_fps(int fps);
int job_fps(void);
void job_set_file(char *file);
char *job_file(void);
void job_set_compression(int comp);
int job_compression(void);
void job_set_capture(void);
void job_set_quality(int quality);
int job_quality(void);


/* 
 * HELPER FUNCTIONS
 */
Job *xvc_job_new()
{
#define DEBUGFUNCTION "xvc_job_new()"

    job = (Job *) malloc(sizeof(Job));
    if (!job) {
        perror("%s %s: malloc failed?!?");
        exit(1);
    }
    return (job);
#undef DEBUGFUNCTION
}


void
xvc_job_set_from_app_data(AppData * app, Display * disp,
                          XWindowAttributes wa)
{
#undef DEBUGFUNCTION
#define DEBUGFUNCTION "xvc_job_set_from_app_data()"
    CapTypeOptions *cto;
    char file[PATH_MAX + 1];

    // make sure we do have a job
    if (job == NULL) {
        fprintf
            (stderr,
             "%s %s: job is still NULL ... this should never happen!",
             DEBUGFILE, DEBUGFUNCTION);
        exit(1);
    }
    // switch sf or mf
#ifdef USE_FFMPEG
    if (app->current_mode != 0)
        cto = &(app->multi_frame);
    else
#endif                          // USE_FFMPEG
        cto = &(app->single_frame);
    // various manual settings
    // need to have the flags set to smth. before other functions try to
    // do
    // flags |= or smth.
    job->flags = app->flags;

    // get area to capture
    job->area = xvc_get_capture_area();

    job_set_fps(cto->fps);

    job->state = VC_STOP;       // FIXME: better move this outta here?
    job->start_no = cto->start_no;
    job->pic_no = job->start_no;
    job->movie_no = 0;          // FIXME: make this configurable
    job->step = cto->step;
    job->max_frames = cto->frames;
    job->max_time = cto->time;
    job->quality = cto->quality;
    job->rescale = app->rescale;
    job->bpp = cto->bpp;
#ifdef HAVE_FFMPEG_AUDIO
    if (cto->audioWanted != 0)
        job->flags |= FLG_REC_SOUND;
    else
#endif                          // HAVE_FFMPEG_AUDIO
        job->flags &= ~FLG_REC_SOUND;
#ifdef HAVE_FFMPEG_AUDIO
    job_set_sound_dev(app->snddev, cto->sndrate, cto->sndsize,
                      cto->sndchannels);
#endif                          // HAVE_FFMPEG_AUDIO
    job->mouseWanted = app->mouseWanted;
    job->video_dev = app->device;

    job->get_colors = (void *(*)(XColor *, int)) NULL;
    job->clean = (void (*)(Job *)) NULL;

#ifdef DEBUG
    printf("%s %s: target is set to %i \n", DEBUGFILE, DEBUGFUNCTION,
           job->target);
#endif                          // DEBUG

    job->target = cto->target;
    if (job->target <= 0) {
        if (job->target == 0) {
            // we should be able to safely assume cto->filename is longer
            // smaller than 0 for the next bit because
            // with target == 0 it would have been set to a default value
            // otherwise
            job->target = xvc_codec_get_target_from_filename(cto->file);
            // we assume job->target can never be == 0 now, because a
            // sanity checking function
            // should have checked before if we have a valid specification 
            // 
            // 
            // for a target either
            // through target itself or the filename extension
            if (job->target <= 0) {
                fprintf(stderr,
                        "%s %s: Unrecoverable error while initializing job from app_data.\n",
                        DEBUGFILE, DEBUGFUNCTION);
                fprintf(stderr,
                        "targetCodec is still 0. This should never happen.\n");
                fprintf(stderr,
                        "Please contact the xvidcap project team.\n");
#ifdef DEBUG
                xvc_job_dump();
#endif                          // DEBUG
                exit(1);
            }
        }
    }

    job->targetCodec = cto->targetCodec;
    if (job->targetCodec <= 0) {
        if (job->targetCodec == 0)
            job->targetCodec = tFFormats[job->target].def_vid_codec;
        if (job->targetCodec < 0) {
            fprintf(stderr,
                    "%s %s: Unrecoverable error while initializing job from app_data.\n",
                    DEBUGFILE, DEBUGFUNCTION);
            fprintf(stderr,
                    "targetCodec is still < 0. This should never happen.\n");
            fprintf(stderr, "Please contact the xvidcap project team.\n");
#ifdef DEBUG
            xvc_job_dump();
#endif                          // DEBUG
            exit(1);
        }
    }
#ifdef HAVE_FFMPEG_AUDIO
    job->au_targetCodec = cto->au_targetCodec;
    if (job->au_targetCodec <= 0) {
        if (job->au_targetCodec == 0)
            job->au_targetCodec = tFFormats[job->target].def_au_codec;
        // if 0 the format has no default audio codec. This should only be 
        // 
        // 
        // 
        // the case if the format does not support audio or recording
        // without
        // audio is encouraged
        if (job->au_targetCodec < 0) {
            fprintf(stderr,
                    "%s %s: Unrecoverable error while initializing job from app_data.\n",
                    DEBUGFILE, DEBUGFUNCTION);
            fprintf(stderr,
                    "au_targetCodec is still < 0. This should never happen.\n");
            fprintf(stderr, "Please contact the xvidcap project team.\n");
#ifdef DEBUG
            xvc_job_dump();
#endif                          // DEBUG
            exit(1);
        }
    }
#endif                          // HAVE_FFMPEG_AUDIO

    job->color_table = NULL;
    job->colors = NULL;
    job->win_attr = wa;

    // the order of the following actions is key!
    // the default is to use the colormap of the root window
    job->ncolors = xvc_get_colors(disp, &job->win_attr, &job->colors);
    job_set_capture();

    // set temporary filename if set to "ask-user"
    if (strlen(cto->file) < 1) {
        char *home = NULL;
        int pid;

        home = getenv("HOME");
        pid = (int) getpid();

        snprintf(file, (PATH_MAX + 1), "%s/.xvidcap-tmp.%i.%s", home, pid,
                 xvc_next_element(tFFormats[job->target].extensions));
    } else {
        snprintf(file, (PATH_MAX + 1), "%s", cto->file);
    }

    job_set_file(file);
    xvc_job_set_save_function(job->win_attr.visual, job->target);

    // get color table
    if (job->get_colors)
        job->color_table = (*job->get_colors) (job->colors, job->ncolors);

#ifdef DEBUG
    printf("%s %s: Leaving function with this job:\n", DEBUGFILE,
           DEBUGFUNCTION);
    xvc_job_dump();
#endif                          // DEBUG
}


/* 
 * get a pointer to the static Job
 *
 */
Job *xvc_job_ptr(void)
{
#undef DEBUGFUNCTION
#define DEBUGFUNCTION "xvc_job_ptr()"

    return (job);
}


/* 
 * Select Function to use for processing the captured frame
 */
void xvc_job_set_save_function(Visual * vis, int type)
{
#undef DEBUGFUNCTION
#define DEBUGFUNCTION "xvc_job_set_save_function()"

#ifdef DEBUG2
    printf("%s %s: entering with type: %i\n", DEBUGFILE, DEBUGFUNCTION,
           type);
#endif                          // DEBUG2

#ifdef USE_FFMPEG
    if (type >= CAP_MF) {
        job->clean = FFMPEGClean;
        if (job->targetCodec == CODEC_NONE) {
            job->targetCodec = CODEC_MPEG1;
        }
        job->flags |= FLG_MULTI_IMAGE;
        job->get_colors = FFMPEGcolorTable;
        job->save = XImageToFFMPEG;
    } else if (type >= CAP_FFM) {
        job->clean = FFMPEGClean;
        if (job->targetCodec == CODEC_NONE) {
            job->targetCodec = CODEC_PGM;
        }
        job->flags &= ~FLG_MULTI_IMAGE;
        job->get_colors = FFMPEGcolorTable;
        job->save = XImageToFFMPEG;
    } else
#endif                          // USE_FFMPEG
    {
        job->save = XImageToXWD;
        job->get_colors = XWDcolorTable;
        job->flags &= ~FLG_MULTI_IMAGE;
        job->clean = NULL;
    }

}


/* 
 */
void job_set_fps(int fps)
{
#undef DEBUGFUNCTION
#define DEBUGFUNCTION "job_set_fps()"

#ifdef DEBUG2
    printf("%s %s: entering with fps: %i\n", DEBUGFILE, DEBUGFUNCTION,
           fps);
#endif                          // DEBUG2

    job->time_per_frame = (int) (1000 / (fps / (float) 100));
    job->fps = fps;
}


#ifdef HAVE_FFMPEG_AUDIO
/* 
 * set and check some parameters for the sound device
 */

int job_set_sound_dev(char *snd, int rate, int size, int channels)
{
#undef DEBUGFUNCTION
#define DEBUGFUNCTION "job_set_sound_dev()"
    extern int errno;
    struct stat statbuf;
    int stat_ret;

    job->snd_rate = rate;
    job->snd_smplsize = size;
    job->snd_channels = channels;
    job->snd_device = snd;

    if (job->flags & FLG_REC_SOUND) {
        if (strcmp(snd, "-") != 0) {
            stat_ret = stat(snd, &statbuf);

            if (stat_ret != 0) {
                switch (errno) {
                case EACCES:
                    fprintf(stderr,
                            _
                            ("Insufficient permission to access sound input from %s\n"),
                            snd);
                    fprintf(stderr, _("Sound disabled!\n"));
                    job->flags &= ~FLG_REC_SOUND;
                    break;
                default:
                    fprintf(stderr,
                            _("Error accessing sound input from %s\n"),
                            snd);
                    fprintf(stderr, _("Sound disabled!\n"));
                    job->flags &= ~FLG_REC_SOUND;
                    break;
                }
            }
        }
    }

    return 0;
}
#endif                          // HAVE_FFMPEG_AUDIO



/* 
 */
void job_set_file(char *file)
{
#undef DEBUGFUNCTION
#define DEBUGFUNCTION "job_set_file()"

    if (!file)
        return;

    job->file = strdup(file);

    sprintf(job->open_flags, "wb");

#ifdef DEBUG2
    printf("%s %s: leaving function with file = %s\n", DEBUGFILE,
           DEBUGFUNCTION, job->file);
#endif                          // DEBUG2
}


/* 
 * find the correct capture function
 */
void job_set_capture(void)
{
#undef DEBUGFUNCTION
#define DEBUGFUNCTION "job_set_capture()"
    int input = job->flags & FLG_SOURCE;

    switch (input) {
#ifdef HAVE_SHMAT
    case FLG_USE_SHM:
        job->capture = TCbCaptureSHM;
        break;
#endif                          // HAVE_SHMAT
    case FLG_USE_DGA:
        job->capture = TCbCaptureDGA;
        break;
#ifdef HasBTTV
    case FLG_USE_V4L:
        job->capture = TCbCaptureV4L;
        break;
#endif
    default:
        job->capture = TCbCaptureX11;
        break;
    }

}


/* 
 */
void job_set_quality(int quality)
{
#undef DEBUGFUNCTION
#define DEBUGFUNCTION "job_set_quality()"

    if (quality < 1)
        quality = 1;
    else if (quality > 100)
        quality = 100;
    job->quality = quality;
}


/* 
 * dump job settings
 */
void xvc_job_dump()
{
#undef DEBUGFUNCTION
#define DEBUGFUNCTION "xvc_job_dump()"

    printf("fps = %.2f\n", (float) (job->fps / 100.00));
    printf("file = %s\n", job->file);
    printf("flags = %i\n", job->flags);
    printf("state = %i\n", job->state);
    printf("start_no = %i\n", job->start_no);
    printf("pic_no = %i\n", job->pic_no);
    printf("movie_no = %i\n", job->movie_no);
    printf("step = %i\n", job->step);
    printf("time_per_frame = %i\n", job->time_per_frame);
    printf("max_frames = %i\n", job->max_frames);
    printf("max_time = %i\n", job->max_time);
    printf("quality = %i\n", job->quality);
    printf("open_flags = %s\n", strdup(job->open_flags));
    printf("bpp = %i\n", job->bpp);
    printf("vid_dev = %i\n", job->vid_dev);
#ifdef HAVE_FFMPEG_AUDIO
    printf("snd_dev = %i\n", job->snd_dev);
    printf("snd_rate = %i\n", job->snd_rate);
    printf("snd_smplsize = %i\n", job->snd_smplsize);
    printf("snd_channels = %i\n", job->snd_channels);
#endif                          // HAVE_FFMPEG_AUDIO
    printf("mouseWanted = %i\n", job->mouseWanted);
    printf("video_dev = %s\n", job->video_dev);
#ifdef HAVE_FFMPEG_AUDIO
    printf("snd_device = %s\n", job->snd_device);
#endif                          // HAVE_FFMPEG_AUDIO

    printf("get_colors = %p\n", job->get_colors);
    printf("save = %p\n", job->save);
    printf("clean = %p\n", job->clean);
    printf("capture = %p\n", job->capture);

    printf("target = %i\n", job->target);
    printf("targetCodec = %i\n", job->targetCodec);
    printf("ncolors = %i\n", job->ncolors);

    printf("color_table = %p\n", job->color_table);
    printf("colors = %p\n", job->colors);
    printf("win_attr (w/h/x/y) = %i/%i/%i/%i\n", job->win_attr.width,
           job->win_attr.height, job->win_attr.x, job->win_attr.y);
    printf("area (w/h/x/y) = %i/%i/%i/%i\n", job->area->width,
           job->area->height, job->area->x, job->area->y);

}


/* 
 * validate job
 * some things, can only be set after the app_data things have become a job
 * these things include e.g. flags and capture size
 */
void xvc_job_validate()
{
#undef DEBUGFUNCTION
#define DEBUGFUNCTION "xvc_job_validate()"

    // unset autocontinue unless we capture to movie and file is mutable 
    if (job->flags & FLG_AUTO_CONTINUE &&
        ((!xvc_is_filename_mutable(job->file)) ||
         (job->flags & FLG_MULTI_IMAGE) == 0)) {
        job->flags &= ~FLG_AUTO_CONTINUE;
        printf
            ("Output not a video file or no counter in filename\nDisabling autocontinue!\n");
    }
#ifdef USE_FFMPEG
    // make sure we have even width and height for ffmpeg 
    if (job->target >= CAP_MF) {
        Boolean changed = FALSE;
        int orig_width = job->area->width, orig_height = job->area->height;

        if ((job->area->width % 2) > 0) {
            job->area->width--;
            changed = TRUE;
        }
        if ((job->area->height % 2) > 0) {
            job->area->height--;
            changed = TRUE;
        }
        if (job->win_attr.width < 26) {
            job->win_attr.width = 26;
            changed = TRUE;
        }
        if (job->win_attr.height < 26) {
            job->win_attr.height = 26;
            changed = TRUE;
        }

        if (changed) {
            xvc_frame_change(job->area->x, job->area->y, job->area->width,
                             job->area->height, FALSE);
            if (job->flags & FLG_RUN_VERBOSE) {
                fprintf(stdout, _("%s %s: Original dimensions: %i * %i\n"),
                        DEBUGFILE, DEBUGFUNCTION, orig_width, orig_height);
                fprintf(stdout,
                        _("%s %s(): Modified dimensions: %i * %i\n"),
                        DEBUGFILE, DEBUGFUNCTION, job->area->width,
                        job->area->height);
            }
        }

    }
#endif                          // USE_FFMPEG

}


void job_state_change_signals_thread(int orig_state, int new_state)
{
    if (((orig_state & VC_PAUSE) > 0 && (new_state & VC_PAUSE) == 0) ||
        ((orig_state & VC_STOP) == 0 && (new_state & VC_STOP) > 0) ||
        ((orig_state & VC_STEP) == 0 && (new_state & VC_STEP) > 0)
        ) {
        // signal potentially paused thread
        pthread_cond_broadcast(&recording_condition_unpaused);
    }
}

void job_set_state(int state)
{
    int orig_state = job->state;

    pthread_mutex_lock(&recording_mutex);
    job->state = state;
    job_state_change_signals_thread(orig_state, job->state);
    pthread_mutex_unlock(&recording_mutex);
}

void job_merge_state(int state)
{
    int orig_state = job->state;

    pthread_mutex_lock(&recording_mutex);
    job->state |= state;
    job_state_change_signals_thread(orig_state, job->state);
    pthread_mutex_unlock(&recording_mutex);
}

void job_remove_state(int state)
{
    int orig_state = job->state;

    pthread_mutex_lock(&recording_mutex);
    job->state &= ~(state);
    job_state_change_signals_thread(orig_state, job->state);
    pthread_mutex_unlock(&recording_mutex);
}

void job_merge_and_remove_state(int merge_state, int remove_state)
{
    int orig_state = job->state;

    pthread_mutex_lock(&recording_mutex);
    job->state |= merge_state;
    job->state &= ~(remove_state);
    job_state_change_signals_thread(orig_state, job->state);
    pthread_mutex_unlock(&recording_mutex);
}

void job_keep_state(int state)
{
    int orig_state = job->state;

    pthread_mutex_lock(&recording_mutex);
    job->state &= state;
    job_state_change_signals_thread(orig_state, job->state);
    pthread_mutex_unlock(&recording_mutex);
}

void job_keep_and_merge_state(int keep_state, int merge_state)
{
    int orig_state = job->state;

    pthread_mutex_lock(&recording_mutex);
    job->state &= keep_state;
    job->state |= merge_state;
    job_state_change_signals_thread(orig_state, job->state);
    pthread_mutex_unlock(&recording_mutex);
}
