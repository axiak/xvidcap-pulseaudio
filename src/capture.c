/*
 * Copyright (C) 1997-98 Rasca, Berlin
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
 * This file contains routines used for capturing individual frames.
 * Theese routines are called from the record button callback in
 * the GUI and call themselves again till they are stopped by a stop
 * button event handler, a timeout, exceeding the maximum number of
 * frames (see the various VC_... states)
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif                          // HAVE_STDINT_H

#include <stdio.h>
#include <limits.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/XWDFile.h>
#include <X11/Xproto.h>
#include <X11/Xlibint.h>
#include <X11/cursorfont.h>

#ifdef SOLARIS
#include <X11/X.h>
#include <X11/Xlib.h>
#endif                          // SOLARIS

#ifdef HAVE_SHMAT
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif                          // HAVE_SHMAT

#ifdef HasDGA
#include <X11/extensions/xf86dga.h>
#endif                          // HasDGA

#ifdef HasVideo4Linux
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "video.h"
#endif                          // HasVideo4Linux

#include "capture.h"
#include "job.h"
#include "control.h"


#define DEBUGFILE "capture.c"




/* 
 * the following function finds out where the mouse pointer is
 */
static void getCurrentPointer(int *x, int *y, Job * mjob)
{
    #define DEBUGFUNCTION "getCurrentPointer()"
    Window mrootwindow, childwindow;
    int dummy;
    Display *dpy;
    Screen *scr;

    if (!(mjob->flags & FLG_NOGUI)) {
        scr = mjob->win_attr.screen;
        dpy = DisplayOfScreen(scr);
    } else {
        dpy = XOpenDisplay(NULL);
    }
    mrootwindow = DefaultRootWindow(dpy);

    if (XQueryPointer(dpy, mrootwindow, &mrootwindow, &childwindow,
                      x, y, &dummy, &dummy, &dummy)) {
        // if the XQueryPointer was successfull, we have everything we
        // need in the variables passed as result pointers
    } else {
        fprintf(stderr,
                "%s %s: couldn't find mouse pointer for disp: %p , rootwindow: %p\n",
                DEBUGFILE, DEBUGFUNCTION, dpy,
                &(RootWindow(dpy, DefaultScreen(dpy))));
        *x = -1;
        *y = -1;
    }

    if (mjob->flags & FLG_NOGUI) {
        XCloseDisplay(dpy);
    }
    #undef DEBUGFUNCTION
}


/* 
 * paint the dummy mouse pointer into a given frame
 */
static void paintMousePointer(int *x, int *y, XImage * image)
{
    #define DEBUGFUNCTION "paintMousePointer()"
    uint16_t mousePointerBlack[] =
        { 0, 49152, 40960, 36864, 34816, 33792, 33280, 33024, 32896, 32832,
        33728, 37376, 43264, 51456, 1152, 1152, 576, 576, 448, 0
    };
    uint16_t mousePointerWhite[] =
        { 0, 0, 16384, 24576, 28672, 30720, 31744, 32256, 32512, 32640,
        31744,
        27648, 17920, 1536, 768, 768, 384, 384, 0, 0
    };
    Job *mjob;

    mjob = xvc_job_ptr();
    // only paint a mouse pointer into the dummy frame if the position of
    // the mouse is within the rectangle defined by the capture frame

    if ((*x - mjob->area->x) >= 0
        && *x < (mjob->area->width + mjob->area->x)
        && (*y - mjob->area->y) >= 0
        && *y < (mjob->area->height + mjob->area->y)) {
        int line;
        uint8_t *im_data = image->data;

        // move the cursor to the right starting position
        // first: shift to right line
        im_data += (image->bytes_per_line * (*y - mjob->area->y));
        // then: shift to right pixel
        im_data += (image->bits_per_pixel / 8 * (*x - mjob->area->x));

        switch (image->bits_per_pixel) {
        case 32:
            {
                uint32_t *cursor;
                int width;
                uint16_t bm_b, bm_w, mask;

                // the dummy mouse pointer is 20 pixels high ...
                for (line = 0; line < 20; line++) {
                    if (mjob->mouseWanted == 1) {
                        bm_b = mousePointerBlack[line];
                        bm_w = mousePointerWhite[line];
                    } else {
                        bm_b = mousePointerWhite[line];
                        bm_w = mousePointerBlack[line];
                    }
                    mask = (0x0001 << 15);

                    // make sure we don't run out of the screenshot
                    if ((*y + line) < (mjob->area->height + mjob->area->y)) {
                        // ... and 16 pixels wide
                        for (cursor = (uint32_t *) im_data, width = 0;
                             ((width + *x) <
                              (mjob->area->width + mjob->area->x)
                              && width < 16); cursor++, width++) {
                            if ((bm_b & mask) > 0) {
                                *cursor &=
                                    ((~image->red_mask) & (~image->
                                                           green_mask) &
                                     (~image->blue_mask));
                            } else if ((bm_w & mask) > 0) {
                                *cursor |=
                                    (image->red_mask | image->
                                     green_mask | image->blue_mask);
                            }
                            mask >>= 1;
                        }
                        im_data += image->bytes_per_line;
                    }
                }
            }
            break;
        case 24:
            // not sure this can occur at all ..........
            fprintf(stderr,
                    "%s %s: input image bits_per_pixel %i not implemented with mouse pointer capture ... \naborting!\n",
                    DEBUGFILE, DEBUGFUNCTION, image->bits_per_pixel);
            fprintf(stderr,
                    "Please file a bug at http://www.sourceforge.net/projects/xvidcap/\n");
            exit(1);
            break;
        case 16:
            {
                uint16_t *cursor;
                int width;
                uint16_t bm_b, bm_w, mask;

                // the dummy mouse pointer is 20 pixels high ....
                for (line = 0; line < 20; line++) {
                    if (mjob->mouseWanted == 1) {
                        bm_b = mousePointerBlack[line];
                        bm_w = mousePointerWhite[line];
                    } else {
                        bm_b = mousePointerWhite[line];
                        bm_w = mousePointerBlack[line];
                    }
                    mask = (0x0001 << 15);

                    // make sure we don't run out of the screenshot
                    if ((*y + line) < (mjob->area->height + mjob->area->y)) {
                        // ... and 16 pixels wide
                        for (cursor = (uint16_t *) im_data, width = 0;
                             ((width + *x) <
                              (mjob->area->width + mjob->area->x)
                              && width < 16); cursor++, width++) {
                            if ((bm_b & mask) > 0) {
                                *cursor &=
                                    ((~image->red_mask) & (~image->
                                                           green_mask) &
                                     (~image->blue_mask));
                            } else if ((bm_w & mask) > 0) {
                                *cursor |=
                                    (image->red_mask | image->
                                     green_mask | image->blue_mask);
                            }
                            mask >>= 1;
                        }
                        im_data += image->bytes_per_line;
                    }
                }
            }
            break;
        case 8:
            {
                uint8_t *cursor;
                int width;
                uint16_t bm_b, bm_w, mask;

                // the dummy mouse pointer is 20 pixels high ...
                for (line = 0; line < 20; line++) {
                    if (mjob->mouseWanted == 1) {
                        bm_b = mousePointerBlack[line];
                        bm_w = mousePointerWhite[line];
                    } else {
                        bm_b = mousePointerWhite[line];
                        bm_w = mousePointerBlack[line];
                    }
                    mask = (0x0001 << 15);

                    // make sure we don't run out of the screenshot
                    if ((*y + line) < (mjob->area->height + mjob->area->y)) {
                        // ... and 16 pixels wide
                        for (cursor = im_data, width = 0;
                             ((width + *x) <
                              (mjob->area->width + mjob->area->x)
                              && width < 16); cursor++, width++) {
                            if ((bm_b & mask) > 0) {
                                *cursor = 0;
                            } else if ((bm_w & mask) > 0) {
                                *cursor = 1;
                            }
                            mask >>= 1;
                        }
                        im_data += image->bytes_per_line;
                    }
                }
            }
            break;
        default:
            fprintf(stderr,
                    "%s %s: input image bits_per_pixel %i not supported with mouse pointer capture ... \naborting!\n",
                    DEBUGFILE, DEBUGFUNCTION, image->bits_per_pixel);
            exit(1);
        }
    }
    #undef DEBUGFUNCTION
}



/* 
 * just read new data in the image structure, the image
 * structure inclusive the data area must be allocated before
 */
static Boolean
XGetZPixmap(Display * dpy, Drawable d, XImage * image, int x, int y)
{
    #define DEBUGFUNCTION "XGetZPixmap()"
    register xGetImageReq *req = NULL;
    xGetImageReply rep;
    long nbytes;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    if (!image)
        return (False);
    LockDisplay(dpy);
    GetReq(GetImage, req);
    /* 
     * first set up the standard stuff in the request
     */
    req->drawable = d;
    req->x = x;
    req->y = y;
    req->width = image->width;
    req->height = image->height;
    req->planeMask = AllPlanes;
    req->format = ZPixmap;

    if (_XReply(dpy, (xReply *) & rep, 0, xFalse) == 0 || rep.length == 0) {
        UnlockDisplay(dpy);
        SyncHandle();
        return (False);
    }

    nbytes = (long) rep.length << 2;
#ifdef DEBUG
    printf("%s %s: read %i bytes\n", DEBUGFILE, DEBUGFUNCTION, nbytes);
#endif // DEBUG

    _XReadPad(dpy, image->data, nbytes);

    UnlockDisplay(dpy);
    SyncHandle();

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    return (True);
    #undef DEBUGFUNCTION
}


FILE *getOutputFile(Job * job)
{
    #define DEBUGFUNCTION "getOutputFile()"
    char file[PATH_MAX + 1];
    FILE *fp = NULL;
    
    if ((job->flags & FLG_MULTI_IMAGE) != 0) {
        sprintf(file, job->file, job->movie_no);
    } else {
        sprintf(file, job->file, job->pic_no);
    }

    fp = fopen(file, job->open_flags);
    if (!fp) {
        perror(file);
        job->state = VC_STOP;
    }
    
    return fp;                  // possibly NULL
    #undef DEBUGFUNCTION
}


Display *getCaptureDisplay(Job * job)
{
    #define DEBUGFUNCTION "getCaptureDisplay()"
    Screen *scr;
    Display *dpy;
    
    if (!(job->flags & FLG_NOGUI)) {
        scr = job->win_attr.screen;
        dpy = DisplayOfScreen(scr);
    } else {
        dpy = XOpenDisplay(NULL);
    }
    
    return dpy;                 // possibly NULL
    #undef DEBUGFUNCTION
}


XImage *captureFrameCreatingImage(Display * dpy, Job * job)
{
    #define DEBUGFUNCTION "captureFrameCreatingImage()"
    int x, y;                   // trace mouse pointer
    XImage *image = NULL;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    
    // don't find the current pointer position if we don't need it
    if (job->mouseWanted > 0) {
        getCurrentPointer(&x, &y, job);
    }
#ifdef DEBUG
    printf("%s %s: going to fetch image next\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    // get the image here
    image = XGetImage(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                      job->area->x, job->area->y,
                      job->area->width, job->area->height,
                      AllPlanes, ZPixmap);
    if (!image) {
        printf("%s %s: Can't get image: %dx%d+%d+%d\n",
               DEBUGFILE, DEBUGFUNCTION, job->area->width,
               job->area->height, job->area->x, job->area->y);
        job->state = VC_STOP;
    } else {
        // paint the mouse pointer into the captured image if necessary
        if (job->mouseWanted > 0) {
            paintMousePointer(&x, &y, image);
        }
    }

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    return image;
    #undef DEBUGFUNCTION
}


int captureFrameToImage(Display * dpy, Job * job, XImage * image)
{
    #define DEBUGFUNCTION "captureFrameToImage()"
    int x, y, ret = 0;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    // don't find the current pointer position if we don't need it
    if (job->mouseWanted > 0) {
        getCurrentPointer(&x, &y, job);
    }
#ifdef DEBUG
    printf("%s %s: going to fetch image next\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    // get the image here
    if (XGetZPixmap(dpy,
                    RootWindow(dpy, DefaultScreen(dpy)),
                    image, job->area->x, job->area->y)) {
        // paint the mouse pointer into the captured image if necessary
        if (job->mouseWanted > 0) {
            paintMousePointer(&x, &y, image);
        }
        ret = 1;
    }

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    
    return ret;
    #undef DEBUGFUNCTION
}


int captureFrameToImageSHM(Display * dpy, Job * job, XImage * image)
{
    #define DEBUGFUNCTION "captureFrameToImageSHM()"
    int x, y, ret = 0;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    // don't find the current pointer position if we don't need it
    if (job->mouseWanted > 0) {
        getCurrentPointer(&x, &y, job);
    }
#ifdef DEBUG
    printf("%s %s: going to fetch image next\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    // get the image here
    if (XShmGetImage(dpy,
                    RootWindow(dpy, DefaultScreen(dpy)),
                    image, job->area->x, job->area->y, AllPlanes)) {
        // paint the mouse pointer into the captured image if necessary
        if (job->mouseWanted > 0) {
            paintMousePointer(&x, &y, image);
        }
        ret = 1;
    }

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    
    return ret;
    #undef DEBUGFUNCTION
}


XImage *captureFrameCreatingImageSHM(Display * dpy, Job * job, XShmSegmentInfo *shminfo)
{
    #define DEBUGFUNCTION "captureFrameCreatingImageSHM()"
    int x, y;                   // trace mouse pointer
    XImage *image = NULL;

    Visual *visual = job->win_attr.visual;
    unsigned int depth = job->win_attr.depth;

#ifdef DEBUG
    printf("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    
    // don't find the current pointer position if we don't need it
    if (job->mouseWanted > 0) {
        getCurrentPointer(&x, &y, job);
    }
    
#ifdef DEBUG
    printf("%s %s: going to fetch image next\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG
    // get the image here
    image = XShmCreateImage(dpy, visual, depth, ZPixmap, NULL,
            shminfo, job->area->width, job->area->height);
    if (!image) {
        printf("%s %s: Can't get image: %dx%d+%d+%d\n",
               DEBUGFILE, DEBUGFUNCTION, job->area->width,
               job->area->height, job->area->x, job->area->y);
        job_set_state(VC_STOP);
    } else {
        shminfo->shmid = shmget(IPC_PRIVATE,
                    image->bytes_per_line * image->height,
                    IPC_CREAT | 0777);
        if (shminfo->shmid == -1) {
            printf("%s %s: Fatal: Can't get shared memory!\n",
                    DEBUGFILE, DEBUGFUNCTION);
            exit(1);
        }
        shminfo->shmaddr = image->data = shmat(shminfo->shmid, 0, 0);
        shminfo->readOnly = False;

        if (XShmAttach(dpy, shminfo) == 0) {
            printf("%s %s: Fatal: Failed to attach shared memory!\n",
                    DEBUGFILE, DEBUGFUNCTION);
            exit(1);
        }

        captureFrameToImageSHM(dpy, job, image);
    }

#ifdef DEBUG
    printf("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

    return image;
    #undef DEBUGFUNCTION
}


long checkCaptureDuration(Job * job, long time, long time1)
{
    #define DEBUGFUNCTION "checkCaptureDuration()"
    struct timeval curr_time;   // for measuring the duration of a frame
                                // capture
    
    // update monitor widget, here time is the time the capture took
    // time == 0 resets led_meter
    if (time1 < 1)
        time1 = 1;
    if (!(job->flags & FLG_NOGUI))
        xvc_idle_add( xvc_frame_monitor, (void *) time1, FALSE);
    // calculate the remaining time we have till capture of next frame
    time1 = job->time_per_frame - time1;

    if (time1 > 0) {
        // get time again because updating frame drop meter took some time
        // we're only doing this if wait time for next frame hasn't been
        // negative already before
        gettimeofday(&curr_time, NULL);
        time = (curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000) - time;
        time = job->time_per_frame - time;
    } else {
        time = time1;
    }
    if (time < 0) {
        if (job->flags & FLG_RUN_VERBOSE)
            printf
                ("missing %ld milli secs (%d needed per frame), pic no %d\n",
                 time, job->time_per_frame, job->pic_no);
    }
    // we need a little time to update the GUI (frame_drop_meter)
/*    if (time < 2)
        time = 2; */

    return time;
    #undef DEBUGFUNCTION
}


// timer callback for capturing
// this is used with source = x11 ... capture from X11 display w/o SHM
long TCbCaptureX11(XtPointer xtp, XtIntervalId * id)
{
    #define DEBUGFUNCTION "TCbCaptureX11()"
    Job *job = (Job *) xtp;
    static XImage *image = NULL;
    static FILE *fp = NULL;     // file handle to write the frame to
    long time, time1;           // for measuring the duration of a frame
                                // capture
    struct timeval curr_time;   // for measuring the duration of a frame
                                // capture
    static Display *dpy;

#ifdef DEBUG
    printf("%s %s: pic_no=%d - state=%i\n", DEBUGFILE, DEBUGFUNCTION,
           job->pic_no, job->state);
#endif // DEBUG

    // wait for next iteration if pausing
    if (job->state & VC_REC) { // if recording ...

#ifdef DEBUG
        printf("%s %s: we're recording\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

        // the next bit is true if we have configured max_frames and we're
        // reaching the limit with this captured frame
        if (job->max_frames && ((job->pic_no - job->start_no) >
                                job->max_frames - 1)) {

#ifdef DEBUG
            printf("%s %s: this is the final frame\n", DEBUGFILE,
                   DEBUGFUNCTION);
#endif // DEBUG

            if (job->flags & FLG_RUN_VERBOSE)
                printf("%s %s: Stopped! pic_no=%d max_frames=%d\n",
                       DEBUGFILE, DEBUGFUNCTION, job->pic_no,
                       job->max_frames);
            if (!(job->flags & FLG_NOGUI))
                xvc_idle_add( xvc_change_filename_display, (void *) job->pic_no, FALSE );

            // we need to stop the capture to go through the necessary
            // cleanup routines for writing a correct file. If we have
            // autocontinue on we're setting a flag to let the cleanup
            // code know we need
            // to restart again afterwards
            if (job->flags & FLG_AUTO_CONTINUE) {
                job_merge_state(VC_CONTINUE);
            }

            goto CLEAN_X11;
        }
        // this might be a single step. If so, remove the state flag so we 
        // don't keep single stepping
        job_remove_state(VC_STEP);

        // take the time before starting the capture
        gettimeofday(&curr_time, NULL);
        time = curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000;

        // open the output file we need to do this for every frame for
        // individual frame 
        // capture and only once for capture to movie
        if ((!(job->flags & FLG_MULTI_IMAGE)) ||
            ((job->flags & FLG_MULTI_IMAGE) && (job->state & VC_START))) {

#ifdef DEBUG
            printf("%s %s: opening file for captured frame(s)\n",
                   DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

            fp = getOutputFile(job);
            if (!fp)
                return FALSE;
        }
        // if this is the first frame for the current job ...
        if (job->state & VC_START) {

#ifdef DEBUG
            printf("%s %s: create data structure for image\n", DEBUGFILE,
                   DEBUGFUNCTION);
#endif // DEBUG

            // the first time this procedure is started
            // we must create a new image ..
            dpy = getCaptureDisplay(job);
            if (!dpy)
                return FALSE;

            image = captureFrameCreatingImage(dpy, job);
            if (image) {
                // call the necessary XtoXYZ function to process the image
                (*job->save) (fp, image, job);
                job_remove_state(VC_START);
            }
        } else { // we're recording and not in the first frame ....
             // so we just read new data into the present image structure 

#ifdef DEBUG
            printf("%s %s: reading an image in a data sturctur present\n",
                   DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

            if (captureFrameToImage(dpy, job, image) > 0)
                // call the necessary XtoXYZ function to process the image
                (*job->save) (fp, image, job);
            else
                printf("%s %s: XGetZPixmap returned 'False'!\n", DEBUGFILE,
                       DEBUGFUNCTION);
        }
        // this again is for recording, no matter if first frame or any
        // other close the image file if single frame capture mode
        if (!(job->flags & FLG_MULTI_IMAGE)) {

#ifdef DEBUG
            printf("%s %s: done saving image, closing file descriptor\n",
                   DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

            fclose(fp);
        }

         // substract the time we needed for creating and saving the frame
         // to the file 
        gettimeofday(&curr_time, NULL);
        time1 =
            (curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000) - time;

        time = checkCaptureDuration(job, time, time1);
        // time the next capture

#ifdef DEBUG
        printf
            ("%s %s: submitting capture of next frame in %ld milliseconds\n",
             DEBUGFILE, DEBUGFUNCTION, time);
#endif // DEBUG

        job->pic_no += job->step;
        // if we're not short on time, update frame count
        if (!(job->flags & FLG_NOGUI))
            xvc_idle_add( xvc_change_filename_display, (void * )job->pic_no, FALSE );

        // the following means VC_STATE != VC_REC
        // this can only happen in capture.c if we were capturing and are
        // just stopping
    } else {
        int orig_state;

         // clean up 
      CLEAN_X11:

#ifdef DEBUG
        printf("%s %s: cleaning up\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

        time = 0;
        orig_state = job->state;    // store state here, esp. VC_CONTINUE

        if (image) {
            XDestroyImage(image);
            image = NULL;
        }

        job_set_state(VC_STOP);

        // set the sensitive stuff for the control panel if we don't
        // autocontinue 
        if ((orig_state & VC_CONTINUE) == 0)
            xvc_idle_add( xvc_capture_stop, job, FALSE );

        // clean up the save routines in xtoXXX.c 
        if (job->clean)
            (*job->clean) (job);
        // if we're recording to a movie, we must close the file now
        if (job->flags & FLG_MULTI_IMAGE)
            if (fp)
                fclose(fp);
        fp = NULL;

        if ((orig_state & VC_CONTINUE) == 0) {
            // after this we're ready to start recording again 
            job_merge_state(VC_READY);
        } else {

#ifdef DEBUG
            printf
                ("%s %s: autocontinue selected, preparing autocontinue\n",
                 DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

            // prepare autocontinue
            job->movie_no += 1;
            job->pic_no = job->start_no;
            job_merge_and_remove_state((VC_START|VC_REC), VC_STOP);
            // start new capture session
//            xvc_capture_start(job);
            // and leave this one without further ado ...
            return time;
        }

        if (job->flags & FLG_NOGUI) {
            XCloseDisplay(dpy);
            exit(0);
        }
    }

    return time;

    #undef DEBUGFUNCTION
}


#ifdef HAVE_SHMAT
/* 
 * timer callback for capturing with shared memory
 */
long TCbCaptureSHM(XtPointer xtp, XtIntervalId * id)
{
    #define DEBUGFUNCTION "TCbCaptureSHM()"

    Job *job = (Job *) xtp;
    static XImage *image = NULL;
    static FILE *fp = NULL;     // file handle to write the frame to
    long time, time1;           // for measuring the duration of a frame
                                // capture
    struct timeval curr_time;   // for measuring the duration of a frame
                                // capture
    static Display *dpy;
    static XShmSegmentInfo shminfo;

#ifdef DEBUG
    printf("%s %s: pic_no=%d flags=%d state=%i ", DEBUGFILE, DEBUGFUNCTION,
            job->pic_no, job->flags, job->state);
    printf("VC_REC %i - VC_STOP %i\n", (job->state & VC_REC),
           (job->state & VC_STOP));
    xvc_job_dump();
#endif // DEBUG

    // wait for next iteration if pausing
    if (job->state & VC_REC) { // if recording ...

#ifdef DEBUG
        printf("%s %s: we're recording\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

        // the next bit is true if we have configured max_frames and we're
        // reaching the limit with this captured frame
        if (job->max_frames && ((job->pic_no - job->start_no) >
                                job->max_frames - 1)) {

#ifdef DEBUG
            printf("%s %s: this is the final frame\n", DEBUGFILE,
                   DEBUGFUNCTION);
#endif // DEBUG

            if (job->flags & FLG_RUN_VERBOSE)
                printf("%s %s: Stopped! pic_no=%d max_frames=%d\n",
                       DEBUGFILE, DEBUGFUNCTION, job->pic_no,
                       job->max_frames);
            if (!(job->flags & FLG_NOGUI))
                xvc_idle_add( xvc_change_filename_display, (void *) job->pic_no, FALSE );

            // we need to stop the capture to go through the necessary
            // cleanup routines for writing a correct file. If we have
            // autocontinue on we're setting a flag to let the cleanup
            // code know we need
            // to restart again afterwards
            if (job->flags & FLG_AUTO_CONTINUE) {
                job_merge_state(VC_CONTINUE);
            }

            goto CLEAN_SHM;
        }

        // this might be a single step. If so, remove the state flag so we 
        // don't keep single stepping
        job_remove_state(VC_STEP);
        
        // take the time before starting the capture
        gettimeofday(&curr_time, NULL);
        time = curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000;

        // open the output file we need to do this for every frame for
        // individual frame 
        // capture and only once for capture to movie
        if ((!(job->flags & FLG_MULTI_IMAGE)) ||
            ((job->flags & FLG_MULTI_IMAGE) && (job->state & VC_START))) {

#ifdef DEBUG
            printf("%s %s: opening file for captured frame(s)\n",
                   DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

            fp = getOutputFile(job);
            if (!fp)
                return FALSE;
        }
        // if this is the first frame for the current job ...
        if (job->state & VC_START) {

#ifdef DEBUG
            printf("%s %s: create data structure for image\n", DEBUGFILE,
                   DEBUGFUNCTION);
#endif // DEBUG

            // the first time this procedure is started
            // we must create a new image ..
            dpy = getCaptureDisplay(job);
            if (!dpy)
                return FALSE;

            image = captureFrameCreatingImageSHM(dpy, job, &shminfo);
            if (image) {
                // call the necessary XtoXYZ function to process the image
                (*job->save) (fp, image, job);
                job_remove_state(VC_START);
            }
        } else { // we're recording and not in the first frame ....
             // so we just read new data into the present image structure 

#ifdef DEBUG
            printf("%s %s: reading an image in a data sturctur present\n",
                   DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

            if (captureFrameToImage(dpy, job, image) > 0)
                // call the necessary XtoXYZ function to process the image
                (*job->save) (fp, image, job);
            else
                printf("%s %s: XGetZPixmap returned 'False'!\n", DEBUGFILE,
                       DEBUGFUNCTION);
        }
        // this again is for recording, no matter if first frame or any
        // other close the image file if single frame capture mode
        if (!(job->flags & FLG_MULTI_IMAGE)) {

#ifdef DEBUG
            printf("%s %s: done saving image, closing file descriptor\n",
                   DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

            fclose(fp);
        }

        // substract the time we needed for creating and saving the frame
        // to the file 
        gettimeofday(&curr_time, NULL);
        time1 =
            (curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000) - time;

        time = checkCaptureDuration(job, time, time1);
        // time the next capture

#ifdef DEBUG
        printf
            ("%s %s: submitting capture of next frame in %ld milliseconds\n",
             DEBUGFILE, DEBUGFUNCTION, time);
#endif // DEBUG
//        xvc_add_timeout(time, job->capture);

        job->pic_no += job->step;
        if (!(job->flags & FLG_NOGUI))
            xvc_idle_add( xvc_change_filename_display, (void * )job->pic_no, FALSE );

        // the following means VC_STATE != VC_REC
        // this can only happen in capture.c if we were capturing and are
        // just stopping
    } else {
        int orig_state;

         // clean up 
      CLEAN_SHM:

#ifdef DEBUG
        printf("%s %s: cleaning up\n", DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

        time = 0;
        orig_state = job->state;    // store state here, esp. VC_CONTINUE

        if (image) {
            XShmDetach(dpy, &shminfo);
            image->data = NULL;
            XDestroyImage(image);
            image = NULL;
            shmdt(shminfo.shmaddr);
            shmctl(shminfo.shmid, IPC_RMID, 0);
        } 

        job_set_state(VC_STOP);

        // set the sensitive stuff for the control panel if we don't
        // autocontinue 
        if ((orig_state & VC_CONTINUE) == 0)
            xvc_idle_add( xvc_capture_stop, job, FALSE );

        // clean up the save routines in xtoXXX.c 
        if (job->clean)
            (*job->clean) (job);
        // if we're recording to a movie, we must close the file now
        if (job->flags & FLG_MULTI_IMAGE)
            if (fp)
                fclose(fp);
        fp = NULL;

        if ((orig_state & VC_CONTINUE) == 0) {
            // after this we're ready to start recording again 
            job_merge_state(VC_READY);
        } else {

#ifdef DEBUG
            printf
                ("%s %s: autocontinue selected, preparing autocontinue\n",
                 DEBUGFILE, DEBUGFUNCTION);
#endif // DEBUG

            // prepare autocontinue
            job->movie_no += 1;
            job->pic_no = job->start_no;
            job_merge_and_remove_state((VC_START|VC_REC), VC_STOP);
            // start new capture session
//            xvc_capture_start(job);
            // and leave this one without further ado ...
            return time;
        }

        if (job->flags & FLG_NOGUI) {
            XCloseDisplay(dpy);
            exit(0);
        }
    }

    return time;

    #undef DEBUGFUNCTION
}

#endif                          /* HAVE_SHMAT */



// FIXME: update capture modes after this
#ifdef HasVideo4Linux
/* 
 * timer callback for capturing direct from bttv driver (only linux)
 */
#ifndef linux
#error only for linux
#endif
/* 
 * from bttv.h 
 */

Boolean TCbCaptureV4L(XtPointer xtp, XtIntervalId id *)
{
    Job *job = (Job *) xtp;
    static char file[PATH_MAX + 1];
    static XImage *image = NULL;
    static void *fp = NULL;
    static VIDEO *video = 0;
    static int size;
    static struct video_mmap vi_mmap;
    static struct video_mbuf vi_memb;
    static struct video_picture vi_pict;
    long time, time1;
    struct timeval curr_time;
    Display *dpy;
    Screen *scr;

    if (!(job->flags & FLG_NOGUI)) {
        scr = job->win_attr.screen;
        dpy = DisplayOfScreen(scr);
    } else {
        dpy = XOpenDisplay(NULL);
    }

#ifdef DEBUG2
    printf("TCbCaptureV4L() pic_no=%d\n", job->pic_no);
#endif
    if ((job->state & VC_PAUSE) && !(job->state & VC_STEP)) {
        xvc_add_timeout(job->time_per_frame, job->capture, job);

    } else if (job->state & VC_REC) {
        if (job->max_frames && ((job->pic_no - job->start_no) >
                                job->max_frames - 1)) {
            if (job->flags & FLG_RUN_VERBOSE)
                printf("Stopped! pic_no=%d max_frames=%d\n",
                       job->pic_no, job->max_frames);
            if ((!(job->flags & FLG_RUN_VERBOSE)) &
                (!(job->flags & FLG_NOGUI)))
                xvc_change_filename_display(job->pic_no);

            if (job->flags & FLG_AUTO_CONTINUE)
                job->state |= VC_CONTINUE;
            goto CLEAN_V4L;
        }
        job->state &= ~VC_STEP;
        gettimeofday(&curr_time, NULL);
        time = curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000;

        if ((!(job->flags & FLG_MULTI_IMAGE)) ||
            ((job->flags & FLG_MULTI_IMAGE) && (job->state & VC_START))) {
            if (job->flags & FLG_MULTI_IMAGE)
                sprintf(file, job->file, job->movie_no);
            else
                sprintf(file, job->file, job->pic_no);
            fp = (*job->open) (file, job->open_flags);
            if (!fp) {
                perror(file);
                job->state = VC_STOP;
                return;
            }
        }
        if (job->state & VC_START) {
            /* 
             * the first time this procedure is started
             * we must prepare some stuff ..
             */
            if (!job->bpp)      /* use depth of the default window */
                job->bpp = job->win_attr.depth;

            sync();             /* remove if your bttv driver runs stable
                                 * .. */
            video = video_open(job->video_dev, O_RDWR);
            if (!video) {
                perror(job->video_dev);
                goto CLEAN_V4L;
            }
            vi_pict.depth = 0;
            /* 
             * read default values for hue, etc.. 
             */
            ioctl(video->fd, VIDIOCGPICT, &vi_pict);

            printf("%d->%d %d\n", job->bpp, vi_pict.depth,
                   vi_pict.palette);
            switch (job->bpp) {
            case 24:
                vi_mmap.format = vi_pict.palette = VIDEO_PALETTE_RGB24;
                break;
            case 16:
                vi_mmap.format = vi_pict.palette = VIDEO_PALETTE_RGB565;
                break;
            case 15:
                vi_mmap.format = vi_pict.palette = VIDEO_PALETTE_RGB555;
                break;
            default:
                printf("Fatal: unsupported bpp (%d)\n", job->bpp);
                exit(3);
                break;
            }
            ioctl(video->fd, VIDIOCSPICT, &vi_pict);
            printf("%d->%d %d\n", job->bpp, vi_pict.depth,
                   vi_pict.palette);


            vi_memb.size = 0;
            ioctl(video->fd, VIDIOCGMBUF, &vi_memb);
            printf("%d %d %d\n", vi_memb.size, vi_memb.frames,
                   vi_memb.offsets);

            image = (XImage *) XtMalloc(sizeof(XImage));
            if (!image) {
                printf("Can't get image: %dx%d+%d+%d\n", job->area->width,
                       job->area->height, job->area->x, job->area->y);
                goto CLEAN_V4L;
            }
            switch (job->bpp) {
            case 24:
                image->red_mask = 0xFF0000;
                image->green_mask = 0x00FF00;
                image->blue_mask = 0x0000FF;
                break;
            case 16:
                image->red_mask = 0x00F800;
                image->green_mask = 0x0007E0;
                image->blue_mask = 0x00001F;
                break;
            case 15:
                image->red_mask = 0x00F800;
                image->green_mask = 0x0007E0;
                image->blue_mask = 0x00001F;
                break;
            default:
                printf("Fatal: unsupported bpp (%d)\n", job->bpp);
                exit(3);
                break;
            }
            image->width = job->area->width;
            image->height = job->area->height;
            image->bits_per_pixel = job->bpp;
            image->bytes_per_line = job->bpp / 8 * image->width;
            image->byte_order = MSBFirst;
            size = image->width * image->height * job->bpp;
            video->size = vi_memb.size;
            video_mmap(video, 1);
            if (video->mmap == NULL) {
                perror("mmap()");
                goto CLEAN_V4L;
            }

            vi_mmap.width = image->width;
            vi_mmap.height = image->height;
            vi_mmap.frame = 0;
            image->data = video->mmap;
        }
        /* 
         * just read new data in the image structure 
         */
        if (ioctl(video->fd, VIDIOCMCAPTURE, &vi_mmap) < 0) {
            perror("ioctl(capture)");
            /* 
             * if (vb.frame) vb.frame = 0; else vb.frame = 1; 
             */
            goto CLEAN_V4L;
        }
        printf("syncing ..\n");
        if (ioctl(video->fd, VIDIOCSYNC, vi_mmap.frame) < 0) {
            perror("ioctl(sync)");
        }
        printf("synced()\n");

        (*job->save) (fp, image, job);
        job->state &= ~VC_START;

        if (!(job->flags & FLG_MULTI_IMAGE))
            (*job->close) (fp);

        /* 
         * substract the time we needed for creating and saving the frame
         * to the file 
         */
        gettimeofday(&curr_time, NULL);
        time1 =
            (curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000) - time;
        // update monitor widget, here time is the time the capture took
        // time == 0 resets led_meter
        if (!time1)
            time1 = 1;
        if (!(job->flags & FLG_NOGUI))
            xvc_frame_monitor(time1);
        // printf("time: %i time_per_frame: %i\n", time1,
        // job->time_per_frame);
        // calculate the remaining time we have till capture of next frame
        time1 = job->time_per_frame - time1;

        if (time1 > 0) {
            // get time again because updating frame drop meter took some
            // time
            gettimeofday(&curr_time, NULL);
            time =
                (curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000) -
                time;
            time = job->time_per_frame - time;
        } else {
            time = time1;
        }
        if (time < 0) {
            if (job->flags & FLG_RUN_VERBOSE)
                printf
                    ("missing %ld milli secs (%d needed per frame), pic no %d\n",
                     time, job->time_per_frame, job->pic_no);
        }
        if (time < 2)
            time = 2;

        xvc_add_timeout(time, job->capture, job);

        job->pic_no += job->step;
        if (time >= 2 && (!(job->flags & FLG_NOGUI)))
            xvc_change_filename_display(job->pic_no);

    } else {
        int orig_state;

        /* 
         * clean up 
         */
      CLEAN_V4L:
        orig_state = job->state;    // store state here

        if (!(job->flags & FLG_NOGUI)) {
            // maybe the last update didn't succeed
            xvc_change_filename_display(job->pic_no);
            xvc_frame_monitor(0);
        }

        job->state = VC_STOP;
        if (image) {
            XtFree((char *) image);
            image = NULL;
        }
        if (video) {
            video_mmap(video, 0);
            video_close(video);
            video = NULL;
        }

        /* 
         * set the sensitive stuff for the control panel if we don't
         * autocontinue 
         */
        if ((orig_state & VC_CONTINUE) == 0)
            xvc_capture_stop(job);

        /* 
         * clean up the save routines in xtoXXX.c 
         */
        if (job->clean)
            (*job->clean) (job);
        if (job->flags & FLG_MULTI_IMAGE)
            if (fp)
                (*job->close) (fp);
        fp = NULL;

        if ((orig_state & VC_CONTINUE) == 0) {
            /* 
             * after this we're ready to start recording again 
             */
            job->state |= VC_READY;
        } else {
            job->movie_no += 1;
            job->pic_no = job->start_no;
            job->state &= ~VC_STOP;
            job->state |= VC_START;
            job->state |= VC_REC;
            xvc_capture_start(job);
            return;
        }

    }

    /* 
     * after this we're ready to start recording again 
     */
    job->state |= VC_READY;

    if (job->flags & FLG_NOGUI && (!is_filename_mutable(job->file))) {
        XCloseDisplay(dpy);
    }

    return FALSE;
}
#endif                          /* HasVideo4Linux */

#ifdef HasDGA
/* 
 * direct graphic access
 * this doesn't work until now and may be removed in future..!?
 *
 * IT HAS ALSO NOT BEEN REWRITTEN FOR GTK GUI SUPPORT
 */
Boolean TCbCaptureDGA(XtPointer xtp, XtIntervalId id *)
{
    Job *job = (Job *) xtp;
    static char file[PATH_MAX + 1];
    static XImage *image = NULL;
    static void *fp;
    static int size;
    static Display *dpy;
    long time;
    struct timeval curr_time;

#ifdef DEBUG2
    printf("TCbCaptureDGA() pic_no=%d state=%d\n", job->pic_no,
           job->state);
#endif
    if ((job->state & VC_PAUSE) && !(job->state & VC_STEP)) {
        XtAppAddTimeOut(XtWidgetToApplicationContext(job->toplevel),
                        job->time_per_frame,
                        (XtTimerCallbackProc) job->capture, job);

    } else if (job->state & VC_REC) {
        if (job->max_frames && ((job->pic_no - job->start_no) >
                                job->max_frames - 1)) {
            if (job->flags & FLG_RUN_VERBOSE)
                printf("Stopped! pic_no=%d max_frames=%d\n",
                       job->pic_no, job->max_frames);
            if (!(job->flags & FLG_RUN_VERBOSE))
                ChangeLabel(job->pic_no);
            goto CLEAN_DGA;
        }
        job->state &= ~VC_STEP;
        gettimeofday(&curr_time, NULL);
        time = curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000;

        if ((!(job->flags & FLG_MULTI_IMAGE)) ||
            ((job->flags & FLG_MULTI_IMAGE) && (job->state & VC_START))) {
            if (job->flags & FLG_MULTI_IMAGE)
                sprintf(file, job->file, job->movie_no);
            else
                sprintf(file, job->file, job->pic_no);
            fp = (*job->open) (file, job->open_flags);
            if (!fp) {
                perror(file);
                job->state = VC_STOP;
                return;
            }
        }
        if (job->state & VC_START) {
            /* 
             * the first time this procedure is started
             * we must create a new image structure ..
             */
            dpy = XtDisplay(job->toplevel);
            image = (XImage *) XtMalloc(sizeof(XImage));
            if (!image) {
                printf("Can't get image: %dx%d+%d+%d\n", job->area->width,
                       job->area->height, job->area->x, job->area->y);
                goto CLEAN_DGA;
            }
            image->width = job->area->width;
            image->height = job->area->height;
            image->bits_per_pixel = 3 * 8;
            image->bytes_per_line = 3 * image->width;
            image->byte_order = MSBFirst;
            size = image->width * image->height;
            {
                int width, bank, ram;
                char *base;
                XF86DGAGetVideo(dpy, XDefaultScreen(dpy), (char **) &base,
                                &width, &bank, &ram);
                image->data = base;
            }
            XF86DGADirectVideo(dpy, XDefaultScreen(dpy),
                               XF86DGADirectGraphics);
            (*job->save) (fp, image, job);
            XF86DGADirectVideo(dpy, XDefaultScreen(dpy), 0);
            job->state &= ~VC_START;
        } else {
            /* 
             * just read new data in the image structure 
             */
            XF86DGADirectVideo(dpy, XDefaultScreen(dpy),
                               XF86DGADirectGraphics);
            (*job->save) (fp, image, job);
            XF86DGADirectVideo(dpy, XDefaultScreen(dpy), 0);
        }
        if (!(job->flags & FLG_MULTI_IMAGE))
            (*job->close) (fp);

        /* 
         * substract the time we needed for creating and saving the frame
         * to the file 
         */
        gettimeofday(&curr_time, NULL);
        time = (curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000) - time;
        time = job->time_per_frame - time;
        if (time < 0) {
            if (job->flags & FLG_RUN_VERBOSE)
                printf
                    ("missing %ld milli secs (%d needed per frame), pic no %d\n",
                     time, job->time_per_frame, job->pic_no);
            time = 0;
        }
        XtAppAddTimeOut(XtWidgetToApplicationContext(job->toplevel),
                        time, (XtTimerCallbackProc) job->capture, job);
        job->pic_no += job->step;
        /* 
         * update the label if we have time to do this 
         */
        if (time)
            ChangeLabel(job->pic_no);
    } else {
        /* 
         * clean up 
         */
      CLEAN_DGA:
        /* 
         * may be the last update failed .. so do it here before stop 
         */
        ChangeLabel(job->pic_no);
        job->state = VC_STOP;
        if (image) {
            XtFree((char *) image);
            image = NULL;
        }
        XF86DGADirectVideo(dpy, XDefaultScreen(dpy), 0);
        /* 
         * set the sensitive stuff for the control panel 
         */
        CbStop(NULL, NULL, NULL);

        /* 
         * clean up the save routines in xtoXXX.c 
         */
        if (job->clean)
            (*job->clean) (job);
        if (job->flags & FLG_MULTI_IMAGE)
            if (fp)
                (*job->close) (fp);
        fp = NULL;

        return FALSE;
    }
    return TRUE;
}
#endif                          /* HasDGA */
