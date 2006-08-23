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


typedef struct _Job {
    int fps;                    /* frames per second */
    char *file;                 /* filename */
    int flags;                  /* different flags .. see app_data.h */
    int state;                  /* state flags */
    int start_no;               /* start number */
    int pic_no;                 /* current pic number */
    int movie_no;               /* current movie number */
    int step;                   /* number to use for increasing pic number 
                                 */
    int time_per_frame;         /* time per frame in milli secs */
    int max_frames;             /* max number of frames to record */
    int max_time;
    int quality;                /* needed for jpeg */
    char open_flags[8];
    int bpp;                    /* for video4linux */
    int vid_dev;                /* file descriptor for video device */
#ifdef HAVE_FFMPEG_AUDIO
    int snd_dev;                /* file descriptor for sound device */
    int snd_rate;
    int snd_smplsize;
    int snd_channels;
#endif                          // HAVE_FFMPEG_AUDIO
    int mouseWanted;            /* 0 none , 1 white , 2 black */
    char *video_dev;            /* video device */
#ifdef HAVE_FFMPEG_AUDIO
    char *snd_device;           /* sound device */
#endif                          // HAVE_FFMPEG_AUDIO

    /* 
     * some function pointers 
     */
    void *(*get_colors) (XColor *, int);
    void (*save) (FILE *, XImage *, struct _Job *);
    void (*clean) (struct _Job *);
    long (*capture) (void *, unsigned long *);

    /* 
     * target format, e.g. CAP_XWD 
     */
    int target;
    int targetCodec;
    int au_targetCodec;
    unsigned long crc;
    int ncolors;

    /* 
     * color table for the output file 
     */
    void *color_table;
    XColor *colors;
    XWindowAttributes win_attr;
    XRectangle *area;
    int rescale;
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


Job *xvc_job_new();
void xvc_job_set_from_app_data(AppData * app, Display * disp,
                               XWindowAttributes wa);
Job *xvc_job_ptr(void);
void xvc_job_dump();
void xvc_job_validate();
void xvc_job_set_save_function(Visual * vis, int type);

void job_set_state(int state);
void job_merge_state(int state);
void job_remove_state(int state);
void job_merge_and_remove_state(int merge_state, int remove_state);
void job_keep_state(int state);
void job_keep_and_merge_state(int merge_state, int remove_state);


#endif                          // __JOB_H__
