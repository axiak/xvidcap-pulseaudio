/* 
 * job.h
 *
 * Copyright (C) 1997-98 Rasca, Berlin
 * Copyright (C) 2003-06 Karl, Frankfurt
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

#ifndef __JOB_H__
#define __JOB_H__

#include <X11/Intrinsic.h>
#include <stdio.h>
#include "app_data.h"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

typedef struct _Job
{
    char *file; /* filename */
    int flags;  /* different flags .. see app_data.h */
    int state;  /* state flags */
    int pic_no; /* current pic number */
    int movie_no;   /* current movie number */
    int time_per_frame; /* time per frame in milli secs */
#ifdef HAVE_FFMPEG_AUDIO
    char *snd_device;   /* sound device */
#endif     // HAVE_FFMPEG_AUDIO

    /* 
     * some function pointers 
     */
    void *(*get_colors) (XColor *, int);
    void (*save) (FILE *, XImage *);
    void (*clean) ();
    long (*capture) ();

    /* 
     * target format, e.g. CAP_XWD 
     */
    int target;
    int targetCodec;
    int au_targetCodec;
    int ncolors;

    /* 
     * color table for the output file 
     */
    void *color_table;
    XColor *colors;
} Job;

/* 
 * state flags
 */
#define VC_STOP 1
#define VC_START 2
#define VC_REC 4
#define VC_PAUSE 8
#define VC_STEP 16
#define VC_READY 32
#define VC_CONTINUE 64

Job *xvc_job_new ();
void xvc_job_free ();
void xvc_job_set_from_app_data (XVC_AppData * app);
Job *xvc_job_ptr (void);
void xvc_job_dump ();
void xvc_job_set_save_function (Visual * vis, int type);
void xvc_job_set_colors ();

void job_set_state (int state);
void job_merge_state (int state);
void job_remove_state (int state);
void job_merge_and_remove_state (int merge_state, int remove_state);
void job_keep_state (int state);
void job_keep_and_merge_state (int merge_state, int remove_state);

#endif     // __JOB_H__
