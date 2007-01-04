/** 
 * \file job.c
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
#include <limits.h>                    // PATH_MAX
#include <X11/Intrinsic.h>

#include "job.h"
#include "capture.h"
#include "xtoxwd.h"
#include "frame.h"
#include "colors.h"
#include "codecs.h"
#include "control.h"
#include "app_data.h"
#include "xvidcap-intl.h"
#ifdef USE_FFMPEG
# include "xtoffmpeg.h"
#endif     // USE_FFMPEG

#define DEBUGFILE "job.c"

static Job *job;

extern pthread_mutex_t recording_mutex;
extern pthread_cond_t recording_condition_unpaused;

static int job_set_sound_dev (char *, int, int, int);
static void job_set_capture (void);

/* 
 * HELPER FUNCTIONS
 */
Job *
xvc_job_new ()
{
#define DEBUGFUNCTION "xvc_job_new()"

    job = (Job *) malloc (sizeof (Job));
    if (!job) {
        perror ("%s %s: malloc failed?!?");
        exit (1);
    }

    job->file = NULL;
    job->flags = 0;
    job->state = 0;
    job->pic_no = 0;
    job->movie_no = 0;

    job->time_per_frame = 0;
#ifdef HAVE_FFMPEG_AUDIO
    job->snd_device = NULL;
#endif     // HAVE_FFMPEG_AUDIO

    job->get_colors = (void *(*)(XColor *, int)) NULL;
    job->save = (void (*)(FILE *, XImage *)) NULL;
    job->clean = (void (*)(void)) NULL;
    job->capture = (long (*)(void)) NULL;

    job->target = 0;
    job->targetCodec = 0;
    job->au_targetCodec = 0;
    job->ncolors = 0;

    job->color_table = NULL;
    job->colors = NULL;

    return (job);
#undef DEBUGFUNCTION
}

void
xvc_job_free ()
{
#define DEBUGFUNCTION "xvc_job_free()"
    if (job) {
        if (job->color_table)
            free (job->color_table);
        free (job);
    }
#undef DEBUGFUNCTION
}

void
xvc_job_set_colors ()
{
#define DEBUGFUNCTION "xvc_job_set_colors()"
    job->ncolors = xvc_get_colors (app->dpy, &(app->win_attr), &(job->colors));
    if (job->get_colors) {
        if (job->color_table)
            free (job->color_table);
        job->color_table = (*job->get_colors) (job->colors, job->ncolors);
    }
#undef DEBUGFUNCTION
}


void
xvc_job_set_from_app_data (XVC_AppData * app)
{
#undef DEBUGFUNCTION
#define DEBUGFUNCTION "xvc_job_set_from_app_data()"
    XVC_CapTypeOptions *cto;
    char file[PATH_MAX + 1];

    // make sure we do have a job
    if (!job) {
        xvc_job_new ();
    }
    // switch sf or mf
#ifdef USE_FFMPEG
    if (app->current_mode != 0)
        cto = &(app->multi_frame);
    else
#endif     // USE_FFMPEG
        cto = &(app->single_frame);
    // various manual settings
    // need to have the flags set to smth. before other functions try to
    // do: flags |= or smth.
    job->flags = app->flags;

    job->time_per_frame = (int) (1000 / 
        ((float) cto->fps.num / (float) cto->fps.den));

    job->state = VC_STOP;              // FIXME: better move this outta here?
    job->pic_no = cto->start_no;
    job->movie_no = 0;                 // FIXME: make this configurable
#ifdef HAVE_FFMPEG_AUDIO
    if (cto->audioWanted != 0)
        job->flags |= FLG_REC_SOUND;
    else
#endif     // HAVE_FFMPEG_AUDIO
        job->flags &= ~FLG_REC_SOUND;
#ifdef HAVE_FFMPEG_AUDIO
    job_set_sound_dev (app->snddev, cto->sndrate, cto->sndsize,
                       cto->sndchannels);
#endif     // HAVE_FFMPEG_AUDIO

    job->target = cto->target;
    if (job->target <= 0) {
        if (job->target == 0) {
            // we should be able to safely assume cto->filename is longer
            // smaller than 0 for the next bit because with target == 0 
            // it would have been set to a default value otherwise
            job->target = xvc_codec_get_target_from_filename (cto->file);
            // we assume job->target can never be == 0 now, because a
            // sanity checking function should have checked before if 
            // we have a valid specification  for a target either
            // through target itself or the filename extension
            if (job->target <= 0) {
                fprintf (stderr,
                         "%s %s: Unrecoverable error while initializing job from app_data.\n",
                         DEBUGFILE, DEBUGFUNCTION);
                fprintf (stderr,
                         "targetCodec is still 0. This should never happen.\n");
                fprintf (stderr, "Please contact the xvidcap project team.\n");
#ifdef DEBUG
                xvc_job_dump ();
#endif     // DEBUG
                exit (1);
            }
        }
    }

    job->targetCodec = cto->targetCodec;
    if (job->targetCodec <= 0) {
        if (job->targetCodec == 0)
            job->targetCodec = xvc_formats[job->target].def_vid_codec;
        if (job->targetCodec < 0) {
            fprintf (stderr,
                     "%s %s: Unrecoverable error while initializing job from app_data.\n",
                     DEBUGFILE, DEBUGFUNCTION);
            fprintf (stderr,
                     "targetCodec is still < 0. This should never happen.\n");
            fprintf (stderr, "Please contact the xvidcap project team.\n");
#ifdef DEBUG
            xvc_job_dump ();
#endif     // DEBUG
            exit (1);
        }
    }
#ifdef HAVE_FFMPEG_AUDIO
    job->au_targetCodec = cto->au_targetCodec;
    if (job->au_targetCodec <= 0) {
        if (job->au_targetCodec == 0)
            job->au_targetCodec = xvc_formats[job->target].def_au_codec;
        // if 0 the format has no default audio codec. This should only be 
        // the case if the format does not support audio or recording
        // without audio is encouraged
        if (job->au_targetCodec < 0) {
            fprintf (stderr,
                     "%s %s: Unrecoverable error while initializing job from app_data.\n",
                     DEBUGFILE, DEBUGFUNCTION);
            fprintf (stderr,
                     "au_targetCodec is still < 0. This should never happen.\n");
            fprintf (stderr, "Please contact the xvidcap project team.\n");
#ifdef DEBUG
            xvc_job_dump ();
#endif     // DEBUG
            exit (1);
        }
    }
#endif     // HAVE_FFMPEG_AUDIO

    // set temporary filename if set to "ask-user"
    if (strlen (cto->file) < 1) {
        char *home = NULL;
        int pid;

        home = getenv ("HOME");
        pid = (int) getpid ();

        snprintf (file, (PATH_MAX + 1), "%s/.xvidcap-tmp.%i.%s", home, pid,
                  xvc_formats[job->target].extensions[0]);
    } else {
        snprintf (file, (PATH_MAX + 1), "%s", cto->file);
    }

    /** \todo double I need a strdup here? */
    job->file = strdup (file);

    job_set_capture ();

    // the order of the following actions is key!
    // the default is to use the colormap of the root window
    xvc_job_set_save_function (app->win_attr.visual, job->target);
    xvc_job_set_colors ();

#ifdef DEBUG
    printf ("%s %s: Leaving function with this job:\n", DEBUGFILE,
            DEBUGFUNCTION);
    xvc_job_dump ();
#endif     // DEBUG
#undef DEBUGFUNCTION
}

/* 
 * get a pointer to the static Job
 *
 */
Job *
xvc_job_ptr (void)
{
#define DEBUGFUNCTION "xvc_job_ptr()"
    if (!job)
        xvc_job_new ();
    return (job);
#undef DEBUGFUNCTION
}

/* 
 * Select Function to use for processing the captured frame
 */
void
xvc_job_set_save_function (Visual * vis, int type)
{
#define DEBUGFUNCTION "xvc_job_set_save_function()"

#ifdef DEBUG2
    printf ("%s %s: entering with type: %i\n", DEBUGFILE, DEBUGFUNCTION, type);
#endif     // DEBUG2

#ifdef USE_FFMPEG
    if (type >= CAP_MF) {
        job->clean = FFMPEGClean;
        if (job->targetCodec == CODEC_NONE) {
            job->targetCodec = CODEC_MPEG1;
        }
        job->get_colors = FFMPEGcolorTable;
        job->save = XImageToFFMPEG;
    } else if (type >= CAP_FFM) {
        job->clean = FFMPEGClean;
        if (job->targetCodec == CODEC_NONE) {
            job->targetCodec = CODEC_PGM;
        }
        job->get_colors = FFMPEGcolorTable;
        job->save = XImageToFFMPEG;
    } else
#endif     // USE_FFMPEG
    {
        job->save = XImageToXWD;
        job->get_colors = XWDcolorTable;
        job->clean = NULL;
    }

}

#ifdef HAVE_FFMPEG_AUDIO
/* 
 * set and check some parameters for the sound device
 */
static int
job_set_sound_dev (char *snd, int rate, int size, int channels)
{
#undef DEBUGFUNCTION
#define DEBUGFUNCTION "job_set_sound_dev()"
    extern int errno;
    struct stat statbuf;
    int stat_ret;

    job->snd_device = snd;
    if (job->flags & FLG_REC_SOUND) {
        if (strcmp (snd, "-") != 0) {
            stat_ret = stat (snd, &statbuf);

            if (stat_ret != 0) {
                switch (errno) {
                case EACCES:
                    fprintf (stderr,
                             _
                             ("Insufficient permission to access sound input from %s\n"),
                             snd);
                    fprintf (stderr, _("Sound disabled!\n"));
                    job->flags &= ~FLG_REC_SOUND;
                    break;
                default:
                    fprintf (stderr,
                             _("Error accessing sound input from %s\n"), snd);
                    fprintf (stderr, _("Sound disabled!\n"));
                    job->flags &= ~FLG_REC_SOUND;
                    break;
                }
            }
        }
    }

    return 0;
}
#endif     // HAVE_FFMPEG_AUDIO


/* 
 * find the correct capture function
 */
static void
job_set_capture (void)
{
#undef DEBUGFUNCTION
#define DEBUGFUNCTION "job_set_capture()"
    int input = job->flags & FLG_SOURCE;

    switch (input) {
#ifdef HAVE_SHMAT
    case FLG_USE_SHM:
        job->capture = xvc_capture_shm;
        break;
#endif     // HAVE_SHMAT
    case FLG_USE_DGA:
        job->capture = TCbCaptureDGA;
        break;
#ifdef HasBTTV
    case FLG_USE_V4L:
        job->capture = TCbCaptureV4L;
        break;
#endif
    default:
        job->capture = xvc_capture_x11;
        break;
    }

}


/* 
 * dump job settings
 */
void
xvc_job_dump ()
{
#undef DEBUGFUNCTION
#define DEBUGFUNCTION "xvc_job_dump()"

    printf ("file = %s\n", job->file);
    printf ("flags = %i\n", job->flags);
    printf ("state = %i\n", job->state);
    printf ("pic_no = %i\n", job->pic_no);
    printf ("movie_no = %i\n", job->movie_no);
    printf ("time_per_frame = %i\n", job->time_per_frame);
#ifdef HAVE_FFMPEG_AUDIO
    printf ("snd_device = %s\n", job->snd_device);
#endif     // HAVE_FFMPEG_AUDIO

printf ("get_colors = %p\n", job->get_colors);
    printf ("save = %p\n", job->save);
    printf ("clean = %p\n", job->clean);
    printf ("capture = %p\n", job->capture);

    printf ("ncolors = %i\n", job->ncolors);
    printf ("color_table = %p\n", job->color_table);
    printf ("colors = %p\n", job->colors);

}


void
job_state_change_signals_thread (int orig_state, int new_state)
{
    if (((orig_state & VC_PAUSE) > 0 && (new_state & VC_PAUSE) == 0) ||
        ((orig_state & VC_STOP) == 0 && (new_state & VC_STOP) > 0) ||
        ((orig_state & VC_STEP) == 0 && (new_state & VC_STEP) > 0)
        ) {
        // signal potentially paused thread
        pthread_cond_broadcast (&recording_condition_unpaused);
    }
}

void
job_set_state (int state)
{
    int orig_state = job->state;

    pthread_mutex_lock (&recording_mutex);
    job->state = state;
    job_state_change_signals_thread (orig_state, job->state);
    pthread_mutex_unlock (&recording_mutex);
}

void
job_merge_state (int state)
{
    int orig_state = job->state;

    pthread_mutex_lock (&recording_mutex);
    job->state |= state;
    job_state_change_signals_thread (orig_state, job->state);
    pthread_mutex_unlock (&recording_mutex);
}

void
job_remove_state (int state)
{
    int orig_state = job->state;

    pthread_mutex_lock (&recording_mutex);
    job->state &= ~(state);
    job_state_change_signals_thread (orig_state, job->state);
    pthread_mutex_unlock (&recording_mutex);
}

void
job_merge_and_remove_state (int merge_state, int remove_state)
{
    int orig_state = job->state;

    pthread_mutex_lock (&recording_mutex);
    job->state |= merge_state;
    job->state &= ~(remove_state);
    job_state_change_signals_thread (orig_state, job->state);
    pthread_mutex_unlock (&recording_mutex);
}

void
job_keep_state (int state)
{
    int orig_state = job->state;

    pthread_mutex_lock (&recording_mutex);
    job->state &= state;
    job_state_change_signals_thread (orig_state, job->state);
    pthread_mutex_unlock (&recording_mutex);
}

void
job_keep_and_merge_state (int keep_state, int merge_state)
{
    int orig_state = job->state;

    pthread_mutex_lock (&recording_mutex);
    job->state &= keep_state;
    job->state |= merge_state;
    job_state_change_signals_thread (orig_state, job->state);
    pthread_mutex_unlock (&recording_mutex);
}
