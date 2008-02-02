/**
 * \file job.c
 *
 * This file contains functions for setting up and manipulating Job structs.
 * One esp. important thing here is the setter functions for the recording
 * state machine. Because state changes should be atomic one MUST NOT set
 * job->state manually.
 */
/*
 *
 * Copyright (C) 1997,98 Rasca, Berlin
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
 *
 * This file defines the job data structure and some functions to
 * set/get info about a job (kinda tries to be OO ;S )
 *
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define DEBUGFILE "job.c"
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <limits.h>                    // PATH_MAX
#include <X11/Intrinsic.h>
#include <errno.h>

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

static Job *job = NULL;

static void job_set_capture (void);

#ifdef HAVE_FFMPEG_AUDIO
/**
 * \brief set and check some parameters for the sound device
 *
 * @param snd the name of the audio input device or "-" for stdin
 * @param rate the sample rate
 * @param size the sample size
 * @param channels the number of channels to record
 */
static void
job_set_sound_dev (char *snd, int rate, int size, int channels)
{
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

    return;
#undef DEBUGFUNCTION
}
#endif     // HAVE_FFMPEG_AUDIO

/**
 * \brief create a new job
 *
 * Since this does a malloc, the job needs to be freed
 * @return a new Job struct with variables set to 0|NULL where possible
 */
static Job *
job_new ()
{
#define DEBUGFUNCTION "job_new()"
    job = (Job *) malloc (sizeof (Job));
    if (!job) {
        fprintf (stderr, "%s %s: malloc failed?!?", DEBUGFILE, DEBUGFUNCTION);
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
    job->c_info = NULL;

#ifdef USE_XDAMAGE
    job->dmg_region = XCreateRegion ();
#endif     // USE_XDAMAGE

    job->capture_returned_errno = 0;
    job->frame_moved_x = 0;
    job->frame_moved_y = 0;

    return (job);
#undef DEBUGFUNCTION
}

/**
 * \brief frees a Job
 */
void
xvc_job_free ()
{
#define DEBUGFUNCTION "xvc_job_free()"
    if (job != NULL) {
        if (job->color_table)
            free (job->color_table);

#ifdef USE_XDAMAGE
        XDestroyRegion (job->dmg_region);
#endif     // USE_XDAMAGE

        if (job->c_info)
            free (job->c_info);

        free (job);
        job = NULL;
    }
#undef DEBUGFUNCTION
}

/**
 * \brief set the Job's color information
 */
void
xvc_job_set_colors ()
{
#define DEBUGFUNCTION "xvc_job_set_colors()"
    XVC_AppData *app = xvc_appdata_ptr ();

    job->ncolors = xvc_get_colors (app->dpy, &(app->win_attr), &(job->colors));
    if (job->get_colors) {
        if (job->color_table)
            free (job->color_table);
        job->color_table = (*job->get_colors) (job->colors, job->ncolors);
    }
#undef DEBUGFUNCTION
}

/**
 * \brief set the Job's contents from the current preferences
 *
 * @param app the preferences struct to set the Job from
 */
void
xvc_job_set_from_app_data (XVC_AppData * app)
{
#define DEBUGFUNCTION "xvc_job_set_from_app_data()"
    XVC_CapTypeOptions *cto;
    char file[PATH_MAX + 1];

    // make sure we do have a job
    if (!job) {
        job_new ();
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
    if (app->current_mode == 0 || xvc_is_filename_mutable (cto->file))
        job->flags &= ~(FLG_AUTO_CONTINUE);

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
    xvc_job_set_save_function (job->target);
    xvc_job_set_colors ();

#ifdef DEBUG
    printf ("%s %s: Leaving function with this job:\n", DEBUGFILE,
            DEBUGFUNCTION);
    xvc_job_dump ();
#endif     // DEBUG
#undef DEBUGFUNCTION
}

/**
 * \brief get a pointer to the current Job struct
 *
 * @return a pointer to the current job struct. If a Job had not been
 *      initialized before, it will be initialized now.
 */
Job *
xvc_job_ptr (void)
{
#define DEBUGFUNCTION "xvc_job_ptr()"
    if (job == NULL)
        job_new ();
    return (job);
#undef DEBUGFUNCTION
}

/**
 * \brief selects functions to use for processing the captured frame
 *
 * @param type the id of the target file format used
 */
void
xvc_job_set_save_function (XVC_FFormatID type)
{
#define DEBUGFUNCTION "xvc_job_set_save_function()"

#ifdef DEBUG2
    printf ("%s %s: entering with type: %i\n", DEBUGFILE, DEBUGFUNCTION,
            (int) type);
#endif     // DEBUG2

#ifdef USE_FFMPEG
    if (type >= CAP_MF) {
        job->clean = xvc_ffmpeg_clean;
        if (job->targetCodec == CODEC_NONE) {
            job->targetCodec = CODEC_MPEG1;
        }
        job->get_colors = xvc_ffmpeg_get_color_table;
        job->save = xvc_ffmpeg_save_frame;
    } else if (type >= CAP_FFM) {
        job->clean = xvc_ffmpeg_clean;
        if (job->targetCodec == CODEC_NONE) {
            job->targetCodec = CODEC_PGM;
        }
        job->get_colors = xvc_ffmpeg_get_color_table;
        job->save = xvc_ffmpeg_save_frame;
    } else
#endif     // USE_FFMPEG
    {
        job->save = xvc_xwd_save_frame;
        job->get_colors = xvc_xwd_get_color_table;
        job->clean = NULL;
    }
#undef DEBUGFUNCTION
}

/**
 * \brief find the correct capture function
 */
static void
job_set_capture (void)
{
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
#undef DEBUGFUNCTION
}

/**
 * \brief dump current job's settings to stdout for debugging
 */
void
xvc_job_dump ()
{
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

    printf ("capture returned error: %s\n",
            strerror (job->capture_returned_errno));
#undef DEBUGFUNCTION
}

/**
 * \brief signal the recording thread for certain state changes so it will
 *      be unpaused.
 *
 * @param orig_state previous state
 * @param new_state new state
 */
void
job_state_change_signals_thread (int orig_state, int new_state)
{
    XVC_AppData *app = xvc_appdata_ptr ();

    if (((orig_state & VC_PAUSE) > 0 && (new_state & VC_PAUSE) == 0) ||
        ((orig_state & VC_STOP) == 0 && (new_state & VC_STOP) > 0) ||
        ((orig_state & VC_STEP) == 0 && (new_state & VC_STEP) > 0)
        ) {
        // signal potentially paused thread
        pthread_cond_broadcast (&(app->recording_condition_unpaused));
    }
}

/**
 * \brief set the state overwriting any previous state information
 *
 * @param state state to set
 */
void
xvc_job_set_state (int state)
{
#define DEBUGFUNCTION "xvc_job_set_state()"
    XVC_AppData *app = xvc_appdata_ptr ();
    int orig_state = job->state;

#ifdef DEBUG
    printf ("%s %s: setting state %i\n", DEBUGFILE, DEBUGFUNCTION, state);
#endif     // DEBUG
    pthread_mutex_lock (&(app->capturing_mutex));
    job->state = state;
    job_state_change_signals_thread (orig_state, job->state);
    pthread_mutex_unlock (&(app->capturing_mutex));
#undef DEBUGFUNCTION
}

/**
 * \brief merge the state with present state information
 *
 * @param state the state to merge (OR) with the current state
 */
void
xvc_job_merge_state (int state)
{
#define DEBUGFUNCTION "xvc_job_merge_state()"
    XVC_AppData *app = xvc_appdata_ptr ();
    int orig_state = job->state;

#ifdef DEBUG
    printf ("%s %s: merging state %i with present %i\n", DEBUGFILE,
            DEBUGFUNCTION, state, job->state);
#endif     // DEBUG
    pthread_mutex_lock (&(app->capturing_mutex));
    job->state |= state;
    job_state_change_signals_thread (orig_state, job->state);
    pthread_mutex_unlock (&(app->capturing_mutex));
#undef DEBUGFUNCTION
}

/**
 * \brief removes a certain state from the current state information
 *
 * @param state the state to remove from the current state
 */
void
xvc_job_remove_state (int state)
{
#define DEBUGFUNCTION "xvc_job_remove_state()"
    XVC_AppData *app = xvc_appdata_ptr ();
    int orig_state = job->state;

#ifdef DEBUG
    printf ("%s %s: removing state %i\n", DEBUGFILE, DEBUGFUNCTION, state);
#endif     // DEBUG
    pthread_mutex_lock (&(app->capturing_mutex));
    job->state &= ~(state);
    job_state_change_signals_thread (orig_state, job->state);
    pthread_mutex_unlock (&(app->capturing_mutex));
#undef DEBUGFUNCTION
}

/**
 * \brief merge one state with current state information and remove another
 *
 * @param merge_state the state flag to merge with the current state
 * @param remove_state the state flag to remove from the current state
 */
void
xvc_job_merge_and_remove_state (int merge_state, int remove_state)
{
#define DEBUGFUNCTION "xvc_job_merge_and_remove_state()"
    XVC_AppData *app = xvc_appdata_ptr ();
    int orig_state = job->state;

#ifdef DEBUG
    printf ("%s %s: merging state %i with %i removing %i\n", DEBUGFILE,
            DEBUGFUNCTION, job->state, merge_state, remove_state);
#endif     // DEBUG
    pthread_mutex_lock (&(app->capturing_mutex));
    job->state |= merge_state;
    job->state &= ~(remove_state);
    job_state_change_signals_thread (orig_state, job->state);
    pthread_mutex_unlock (&(app->capturing_mutex));
#undef DEBUGFUNCTION
}

/**
 * \brief keeps the specified state flag if set
 *
 * @param state the state flag to keep if set (all others are unset)
 */
void
xvc_job_keep_state (int state)
{
#define DEBUGFUNCTION "xvc_job_keep_state()"
    XVC_AppData *app = xvc_appdata_ptr ();
    int orig_state = job->state;

#ifdef DEBUG
    printf ("%s %s: keeping %i of state %i\n", DEBUGFILE, DEBUGFUNCTION, state,
            job->state);
#endif     // DEBUG
    pthread_mutex_lock (&(app->capturing_mutex));
    job->state &= state;
    job_state_change_signals_thread (orig_state, job->state);
    pthread_mutex_unlock (&(app->capturing_mutex));
#undef DEBUGFUNCTION
}

/**
 * \brief keeps one state and merges with another
 *
 * @param keep_state the state flag to keep
 * @param merge_state the state to merge with the kept
 * @see xvc_job_keep_state
 */
void
xvc_job_keep_and_merge_state (int keep_state, int merge_state)
{
#define DEBUGFUNCTION "xvc_job_keep_and_merge_state()"
    XVC_AppData *app = xvc_appdata_ptr ();
    int orig_state = job->state;

#ifdef DEBUG
    printf ("%s %s: keeping %i of state %i and merge with %i\n", DEBUGFILE,
            DEBUGFUNCTION, keep_state, job->state, merge_state);
#endif     // DEBUG
    pthread_mutex_lock (&(app->capturing_mutex));
    job->state &= keep_state;
    job->state |= merge_state;
    job_state_change_signals_thread (orig_state, job->state);
    pthread_mutex_unlock (&(app->capturing_mutex));
#undef DEBUGFUNCTION
}

#ifdef USE_XDAMAGE
//XserverRegion
Region
xvc_get_damage_region ()
{
    //XserverRegion region, dmg_region;
    Region region, dmg_region;
    XVC_AppData *app = xvc_appdata_ptr ();

    pthread_mutex_lock (&(app->damage_regions_mutex));
    //region = XFixesCreateRegion (app->dpy, 0, 0);
    region = XCreateRegion ();
    dmg_region = job->dmg_region;
    job->dmg_region = region;
    pthread_mutex_unlock (&(app->damage_regions_mutex));
    return dmg_region;
}
#endif     // USE_XDAMAGE
