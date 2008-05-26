/**
 * \file job.h
 *
 * \todo rename the Job struct to XVC_Job
 */
/*
 * Copyright (C) 1997-98 Rasca, Berlin
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

#ifndef _xvc_JOB_H__
#define _xvc_JOB_H__

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#include <X11/Intrinsic.h>
#include <stdio.h>
#include "app_data.h"
#include "colors.h"

#ifdef USE_XDAMAGE
#include <X11/Xutil.h>
//#include <X11/extensions/Xfixes.h>
//#include <X11/extensions/Xdamage.h>
#endif     // USE_XDAMAGE

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#endif     // DOXYGEN_SHOULD_SKIP_THIS

/**
 * \brief state flags for the recording state machine
 */
enum JobStates
{
    VC_STOP = 1,
    VC_START = 2,
    VC_REC = 4,
    VC_PAUSE = 8,
    VC_STEP = 16,
    VC_READY = 32,
    VC_CONTINUE = 64
};

/**
 * \brief keeps data about the current recording job
 *
 * The stuff here is partly volatile information augmenting the preferences
 * set in an XVC_AppData struct, partly settings which are derived from
 * the preferences. For example, target may be set to autodetect in the
 * preferences, but here we need the final (potentially autodetected)
 * target.
 */
typedef struct  //_job
{
    /** \brief filename */
    char *file;
    /**
     * \brief various flags
     *
     * @see app_data.h
     */
    int flags;
    /** \brief state flags */
    int state;
    /** \brief current pic number */
    int pic_no;
    /** \brief current movie number */
    int movie_no;
    /** \brief time per frame in milli secs */
    int time_per_frame;
#ifdef HAVE_FFMPEG_AUDIO
    /** \brief sound device */
    char *snd_device;
#endif     // HAVE_FFMPEG_AUDIO

    /*
     * some function pointers
     */
    /** \brief function to retrieve color information */
    void *(*get_colors) (XColor *, int);
    /** \brief function used to save a captured frame */
    void (*save) (FILE *, XImage *);
    /** \brief function used to cleanup after a recording session */
    void (*clean) ();
    /** \brief function to capture the frames */
    long (*capture) ();

    /*
     * target format, e.g. CAP_XWD
     */
    /**
     * \brief target format
     *
     * @see app_data.h
     */
    int target;
    /**
     * \brief target video codec
     *
     * @see app_data.h
     */
    int targetCodec;
    /**
     * \brief target audio codec
     *
     * @see app_data.h
     */
    int au_targetCodec;

    /*
     * color information for the output file
     */
    /** \brief the number of colors in the pseudo color color table */
    int ncolors;
    /** \brief the color table as the output API requires it */
    void *color_table;
    /** \brief the colors as X11 sends them with the captured image */
    XColor *colors;
    /** \brief color information retrieved from first XImage */
    ColorInfo *c_info;

#ifdef USE_XDAMAGE
//    XserverRegion dmg_region;
    Region dmg_region;
#endif     // USE_XDAMAGE

    /** \brief the last capture session returned this errno */
    int capture_returned_errno;

    int frame_moved_x;
    int frame_moved_y;
} Job;

void xvc_job_free ();
void xvc_job_set_from_app_data (XVC_AppData * app);
Job *xvc_job_ptr (void);
void xvc_job_dump ();
void xvc_job_set_save_function (XVC_FFormatID type);
void xvc_job_set_colors ();

void xvc_job_set_state (int state);
void xvc_job_merge_state (int state);
void xvc_job_remove_state (int state);
void xvc_job_merge_and_remove_state (int merge_state, int remove_state);
void xvc_job_keep_state (int state);
void xvc_job_keep_and_merge_state (int merge_state, int remove_state);

#ifdef USE_XDAMAGE
//XserverRegion xvc_get_damage_region ();
Region xvc_get_damage_region ();
#endif     // USE_XDAMAGE
#endif     // _xvc_JOB_H__
