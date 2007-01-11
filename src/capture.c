/**
 * \file capture.c
 * 
 * This file contains routines used for capturing individual frames.
 * Theese routines are called from the capture thread spawned on start of
 * a capture session. Once started the capture is completely governed by
 * changes in the VC_* states. It will also be stopped on reaching a 
 * configured maximum number of frames or maximum capture time.
 *
 * \todo add dga and v4l again
 */

/*
 * Copyright (C) 1997-98 Rasca, Berlin
 * Copyright (C) 2003-07 Karl H. Beckers, Frankfurt
 * contains code written 2006 by Eduardo Gomez for the ffmpeg project
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

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define DEBUGFILE "capture.c"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif     // HAVE_STDINT_H
#include <stdio.h>
#include <limits.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#ifdef HAVE_LIBXFIXES
#include <X11/extensions/Xfixes.h>
#endif     // HAVE_LIBXFIXES
#ifdef HAVE_SHMAT
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif     // HAVE_SHMAT
#ifdef HasDGA
#include <X11/extensions/xf86dga.h>
#endif     // HasDGA

#ifdef HasVideo4Linux
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "video.h"
#endif     // HasVideo4Linux

#include "capture.h"
#include "job.h"
#include "app_data.h"
#include "control.h"

#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern int xvc_led_time;
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#ifdef HAVE_LIBXFIXES
#include "colors.h"
/** \brief vectors used for alpha masking of real mouse pointer */
static unsigned char top[65536];

/** \brief vectors used for alpha masking of real mouse pointer */
static unsigned char bottom[65536];

/** \brief color information required for paintig of real mouse pointer */
static ColorInfo c_info;
#endif     // HAVE_LIBXFIXES

/** 
 * \brief function to find out where the mouse pointer is
 *
 * @param x return pointer to write x coordinate to pre-existing int
 * @param y return pointer to write y coordinate to pre-existing int
 */
static void
getCurrentPointer (int *x, int *y)
{
#define DEBUGFUNCTION "getCurrentPointer()"
    Window mrootwindow, childwindow;
    int dummy;

    mrootwindow = DefaultRootWindow (app->dpy);

    if (!XQueryPointer (app->dpy, mrootwindow, &mrootwindow, &childwindow,
                        x, y, &dummy, &dummy, (unsigned int *) &dummy)) {
        fprintf (stderr,
                 "%s %s: couldn't find mouse pointer for disp: %p , rootwindow: %p\n",
                 DEBUGFILE, DEBUGFUNCTION, app->dpy, &mrootwindow);
        *x = -1;
        *y = -1;
    }
#undef DEBUGFUNCTION
}

/**
 * Mouse painting helper function that applies an 'and' and 'or' mask pair to
 * '*dst' pixel. It actually draws a mouse pointer pixel to grabbed frame.
 *
 * @param dst Destination pixel
 * @param and Part of the mask that must be applied using a bitwise 'and'
 *            operator
 * @param or  Part of the mask that must be applied using a bitwise 'or'
 *            operator
 * @param bits_per_pixel Bits per pixel used in the grabbed image
 */
static void inline
apply_masks (uint8_t * dst, uint32_t and, uint32_t or, int bits_per_pixel)
{
    switch (bits_per_pixel) {
    case 32:
        *(uint32_t *) dst = (*(uint32_t *) dst & and) | or;
        break;
    case 16:
        *(uint16_t *) dst = (*(uint16_t *) dst & and) | or;
        break;
    case 8:
        *dst = !!or;
        break;
    }
}

/**
 * Paints a mouse pointer in an X11 image.
 *
 * @param image Image where to paint the mouse pointer
 */
static void
paintMousePointer (XImage * image)
{
#define DEBUGFUNCTION "paintMousePointer()"
    /* 16x20x1bpp bitmap for the black channel of the mouse pointer */
    static const uint16_t const mousePointerBlack[] = {
        0x0000, 0x0003, 0x0005, 0x0009, 0x0011,
        0x0021, 0x0041, 0x0081, 0x0101, 0x0201,
        0x03C1, 0x0049, 0x0095, 0x0093, 0x0120,
        0x0120, 0x0240, 0x0240, 0x0380, 0x0000
    };

    /* 16x20x1bpp bitmap for the white channel of the mouse pointer */
    static const uint16_t const mousePointerWhite[] = {
        0x0000, 0x0000, 0x0002, 0x0006, 0x000E,
        0x001E, 0x003E, 0x007E, 0x00FE, 0x01FE,
        0x003E, 0x0036, 0x0062, 0x0060, 0x00C0,
        0x00C0, 0x0180, 0x0180, 0x0000, 0x0000
    };

    int x, y, cursor_width = 16, cursor_height = 20;

#ifdef HAVE_LIBXFIXES
    unsigned char topp, botp;
    unsigned long applied;
    XFixesCursorImage *x_cursor = NULL;
#endif     // HAVE_LIBXFIXES

    // only paint a mouse pointer into the dummy frame if the position of
    // the mouse is within the rectangle defined by the capture frame

#ifdef HAVE_LIBXFIXES
    if (app->flags & FLG_USE_XFIXES) {
        x_cursor = XFixesGetCursorImage (app->dpy);
        cursor_width = x_cursor->width;
        cursor_height = x_cursor->height;
        y = x_cursor->y - x_cursor->yhot;
        x = x_cursor->x - x_cursor->xhot;
    } else {
        getCurrentPointer (&x, &y);
    }
#else
    getCurrentPointer (&x, &y);
#endif     // HAVE_LIBXFIXES

    if ((x - app->area->x + cursor_width) >= 0 &&
        x < (app->area->width + app->area->x) &&
        (y - app->area->y + cursor_height) >= 0 &&
        y < (app->area->height + app->area->y)
        ) {
        uint8_t *im_data = (uint8_t *) image->data;
        int bytes_per_pixel = image->bits_per_pixel >> 3;
        int line;
        uint32_t masks = 0;
        int yoff = app->area->y - y;

        /* Select correct masks and pixel size */
        if (!app->flags & FLG_USE_XFIXES) {
            if (image->bits_per_pixel == 8) {
                masks = 1;
            } else {
                masks =
                    (image->red_mask | image->green_mask | image->blue_mask);
            }

            // first: shift to right line
            im_data +=
                (image->bytes_per_line * XVC_MAX (0, (y - app->area->y)));

            // then: shift to right pixel
            im_data += (bytes_per_pixel * XVC_MAX (0, (x - app->area->x)));
        }

        /* Draw the cursor - proper loop */
        for (line = XVC_MAX (0, yoff);
             line < XVC_MIN (cursor_height,
                             (app->area->y + image->height) - y); line++) {
            uint8_t *cursor = im_data;
            int column;
            uint16_t bm_b;
            uint16_t bm_w;
            int xoff = app->area->x - x;
            unsigned long *pix_pointer = NULL;

#ifdef HAVE_LIBXFIXES
            if (app->flags & FLG_USE_XFIXES)
                pix_pointer = (line * x_cursor->width) + x_cursor->pixels;
#endif     // HAVE_LIBXFIXES

            if (!app->flags & FLG_USE_XFIXES) {
                if (app->mouseWanted == 1) {
                    bm_b = mousePointerBlack[line];
                    bm_w = mousePointerWhite[line];
                } else {
                    bm_b = mousePointerWhite[line];
                    bm_w = mousePointerBlack[line];
                }

                if (xoff > 0) {
                    bm_b >>= xoff;
                    bm_w >>= xoff;
                }
            }

            for (column = XVC_MAX (0, xoff);
                 column < XVC_MIN (cursor_width,
                                   (app->area->x + app->area->width) - x);
                 column++) {
#ifdef HAVE_LIBXFIXES
                if (app->flags & FLG_USE_XFIXES) {
                    int count;
                    int rel_x = (x - app->area->x + column);
                    int rel_y = (y - app->area->y + line);
                    int mask = (*pix_pointer & 0xFF000000) >> 24;
                    int shift, src_shift, src_mask;

                    applied = 0;

                    for (count = 2; count >= 0; count--) {
                        shift = count * 8;

                        switch (count) {
                        case 2:
                            src_shift = c_info.red_shift;
                            src_mask = image->red_mask;
                            break;
                        case 1:
                            src_shift = c_info.green_shift;
                            src_mask = image->green_mask;
                            break;
                        default:
                            src_shift = c_info.blue_shift;
                            src_mask = image->blue_mask;
                        }

                        topp =
                            top[(mask << 8) + ((*pix_pointer >> shift) & 0xFF)];
                        botp =
                            bottom[(mask << 8) +
                                   (((XGetPixel (image, rel_x, rel_y) &
                                      src_mask) >> src_shift) & 0xFF)];

                        applied |= (topp + botp) << shift;
                    }

                    XPutPixel (image, rel_x, rel_y, applied);

                    pix_pointer++;
                } else {
#endif     // HAVE_LIBXFIXES

                    apply_masks (cursor, ~(masks * (bm_b & 1)),
                                 masks * (bm_w & 1), image->bits_per_pixel);
                    cursor += bytes_per_pixel;
                    bm_b >>= 1;
                    bm_w >>= 1;

#ifdef HAVE_LIBXFIXES
                }
#endif     // HAVE_LIBXFIXES
            }

            im_data += image->bytes_per_line;
        }
    }
#undef DEBUGFUNCTION
}

/** 
 * \brief reads new data in a pre-existing image structure.
 *
 * @param dpy a pointer to the display to read from
 * @param d the drawable to use
 * @param image a pointer to the image to return data to
 * @param x origin x coordinate
 * @param y origin y coordinate
 * @return we were successfull TRUE or FALSE
 */
static Boolean
XGetZPixmap (Display * dpy, Drawable d, XImage * image, int x, int y)
{
#define DEBUGFUNCTION "XGetZPixmap()"
    register xGetImageReq *req = NULL;
    xGetImageReply rep;
    long nbytes;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    if (!image)
        return (False);
    LockDisplay (dpy);
    GetReq (GetImage, req);
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

    if (_XReply (dpy, (xReply *) & rep, 0, xFalse) == 0 || rep.length == 0) {
        UnlockDisplay (dpy);
        SyncHandle ();
        return (False);
    }

    nbytes = (long) rep.length << 2;
#ifdef DEBUG
    printf ("%s %s: read %li bytes\n", DEBUGFILE, DEBUGFUNCTION, nbytes);
#endif     // DEBUG

    _XReadPad (dpy, image->data, nbytes);

    UnlockDisplay (dpy);
    SyncHandle ();

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    return (True);
#undef DEBUGFUNCTION
}

/** 
 * \brief compute the output filename depending on current capture mode and
 *      frame or movie number. Then open that file for writing.
 *
 * @return a pointer to a file handle or NULL
 */
static FILE *
getOutputFile ()
{
#define DEBUGFUNCTION "getOutputFile()"
    char file[PATH_MAX + 1];
    FILE *fp = NULL;
    Job *job = xvc_job_ptr ();

    if (app->current_mode > 0) {
        sprintf (file, job->file, job->movie_no);
    } else {
        sprintf (file, job->file, job->pic_no);
    }

    fp = fopen (file, "wb");
    if (!fp) {
        perror (file);
        job->state = VC_STOP;
    }

    return fp;
#undef DEBUGFUNCTION
}

/** 
 * \brief this captures without an existing XImage and creates one along the
 *      way. This is the plain X11 version.
 *
 * @param dpy a pointer to the display to read from
 * @return a pointer to an XImage or NULL
 */
static XImage *
captureFrameCreatingImage (Display * dpy)
{
#define DEBUGFUNCTION "captureFrameCreatingImage()"
    XImage *image = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    // get the image here
    image = XGetImage (dpy, RootWindow (dpy, DefaultScreen (dpy)),
                       app->area->x, app->area->y,
                       app->area->width, app->area->height, AllPlanes, ZPixmap);
    if (!image) {
        printf ("%s %s: Can't get image: %dx%d+%d+%d\n",
                DEBUGFILE, DEBUGFUNCTION, app->area->width,
                app->area->height, app->area->x, app->area->y);
        xvc_job_set_state (VC_STOP);
    } else {
        // paint the mouse pointer into the captured image if necessary
        if (app->mouseWanted > 0) {
            paintMousePointer (image);
        }
    }

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    return image;
#undef DEBUGFUNCTION
}

/** 
 * \brief this captures into an existing XImage. This is the plain X11 
 *      version.
 *
 * @param dpy a pointer to the display to read from
 * @param image a pointer to the image to write to
 * @return 1 on success, 0 on failure
 */
static int
captureFrameToImage (Display * dpy, XImage * image)
{
#define DEBUGFUNCTION "captureFrameToImage()"
    int ret = 0;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    // get the image here
    if (XGetZPixmap (dpy,
                     RootWindow (dpy, DefaultScreen (dpy)),
                     image, app->area->x, app->area->y)) {
        // paint the mouse pointer into the captured image if necessary
        if (app->mouseWanted > 0) {
            paintMousePointer (image);
        }
        ret = 1;
    }
#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    return ret;
#undef DEBUGFUNCTION
}

#ifdef HAVE_SHMAT
/** 
 * \brief this captures into an existing XImage. This is the SHM version.
 *
 * @param dpy a pointer to the display to read from
 * @param image a pointer to the image to write to
 * @return 1 on success, 0 on failure
 */
static int
captureFrameToImageSHM (Display * dpy, XImage * image)
{
#define DEBUGFUNCTION "captureFrameToImageSHM()"
    int ret = 0;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    // get the image here
    if (XShmGetImage (dpy,
                      RootWindow (dpy, DefaultScreen (dpy)),
                      image, app->area->x, app->area->y, AllPlanes)) {
        // paint the mouse pointer into the captured image if necessary
        if (app->mouseWanted > 0) {
            paintMousePointer (image);
        }
        ret = 1;
    }
#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    return ret;
#undef DEBUGFUNCTION
}

/** 
 * \brief this captures without an existing XImage and creates one along the
 *      way. This is the SHM version.
 *
 * @param dpy a pointer to the display to read from
 * @param shminfo shared memory information
 * @return a pointer to an XImage or NULL
 */
static XImage *
captureFrameCreatingImageSHM (Display * dpy, XShmSegmentInfo * shminfo)
{
#define DEBUGFUNCTION "captureFrameCreatingImageSHM()"
    XImage *image = NULL;
    Visual *visual = app->win_attr.visual;
    unsigned int depth = app->win_attr.depth;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    // get the image here
    image = XShmCreateImage (dpy, visual, depth, ZPixmap, NULL,
                             shminfo, app->area->width, app->area->height);
    if (!image) {
        printf ("%s %s: Can't get image: %dx%d+%d+%d\n",
                DEBUGFILE, DEBUGFUNCTION, app->area->width,
                app->area->height, app->area->x, app->area->y);
        xvc_job_set_state (VC_STOP);
    } else {
        shminfo->shmid = shmget (IPC_PRIVATE,
                                 image->bytes_per_line * image->height,
                                 IPC_CREAT | 0777);
        if (shminfo->shmid == -1) {
            printf ("%s %s: Fatal: Can't get shared memory!\n",
                    DEBUGFILE, DEBUGFUNCTION);
            exit (1);
        }
        shminfo->shmaddr = image->data = shmat (shminfo->shmid, 0, 0);
        shminfo->readOnly = False;

        if (XShmAttach (dpy, shminfo) == 0) {
            printf ("%s %s: Fatal: Failed to attach shared memory!\n",
                    DEBUGFILE, DEBUGFUNCTION);
            exit (1);
        }

        captureFrameToImageSHM (dpy, image);
    }

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    return image;
#undef DEBUGFUNCTION
}
#endif     // HAVE_SHMAT

/** 
 * \brief calculates in how many msecs the next capture is due based on fps
 *      and the duration of the previous capture.
 *
 * @param time the time in msecs when the last capture started
 * @param time1 the time in msecs when the last capture ended
 * @return the number of msecs in which the next capture is due
 */
static long
checkCaptureDuration (long time, long time1)
{
#define DEBUGFUNCTION "checkCaptureDuration()"
    Job *job = xvc_job_ptr ();
    struct timeval curr_time;   /* for measuring the duration of a frame
                                 * capture */

    // update monitor widget, here time is the time the capture took
    // time == 0 resets led_meter
    if (time1 < 1)
        time1 = 1;
    // this sets the frame monitor widget
    xvc_led_time = time1;

    // calculate the remaining time we have till capture of next frame
    time1 = job->time_per_frame - time1;

    if (time1 > 0) {
        // get time again because updating frame drop meter took some time
        // we're only doing this if wait time for next frame hasn't been
        // negative already before
        gettimeofday (&curr_time, NULL);
        time = (curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000) - time;
        time = job->time_per_frame - time;
    } else {
        time = time1;
    }
    if (time < 0) {
        if (app->flags & FLG_RUN_VERBOSE)
            printf
                ("missing %ld milli secs (%d needed per frame), pic no %d\n",
                 time, job->time_per_frame, job->pic_no);
    }

    return time;
#undef DEBUGFUNCTION
}

/** 
 * \brief this is the merged capture function that handles all sources.
 *
 * @param capfunc passes the source as specified in captureFunctions
 * @return the number of msecs in which the next capture is due
 * @see captureFunctions
 */
static long
commonCapture (enum captureFunctions capfunc)
{
#define DEBUGFUNCTION "commonCapture()"
    static XImage *image = NULL;
    static FILE *fp = NULL; // file handle to write the frame to
    long time, time1;   /* for measuring the duration of a frame capture */
    struct timeval curr_time;   /* for measuring the duration of a frame 
                                 * capture */
#ifdef HAVE_SHMAT
    static XShmSegmentInfo shminfo;
#endif     // HAVE_SHMAT
    int ret = 0;
    XVC_CapTypeOptions *target;
    Job *job = xvc_job_ptr ();

#ifdef DEBUG
    printf ("%s %s: Entering pic_no=%d - state=%i\n", DEBUGFILE, DEBUGFUNCTION,
            job->pic_no, job->state);
#endif     // DEBUG

#ifdef USE_FFMPEG
    if (app->current_mode != 0)
        target = &(app->multi_frame);
    else
#endif     // USE_FFMPEG
        target = &(app->single_frame);

    // wait for next iteration if pausing
    if (job->state & VC_REC) {         // if recording ...

#ifdef DEBUG
        printf ("%s %s: we're recording\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

        // the next bit is true if we have configured max_frames and we're
        // reaching the limit with this captured frame
        if (target->frames && ((job->pic_no - target->start_no) >
                               target->frames - 1)) {

#ifdef DEBUG
            printf ("%s %s: this is the final frame\n", DEBUGFILE,
                    DEBUGFUNCTION);
#endif     // DEBUG

            if (app->flags & FLG_RUN_VERBOSE)
                printf ("%s %s: Stopped! pic_no=%d max_frames=%d\n",
                        DEBUGFILE, DEBUGFUNCTION, job->pic_no, target->frames);

            // we need to stop the capture to go through the necessary
            // cleanup routines for writing a correct file. If we have
            // autocontinue on we're setting a flag to let the cleanup
            // code know we need
            // to restart again afterwards
            if (app->flags & FLG_AUTO_CONTINUE) {
                xvc_job_merge_state (VC_CONTINUE);
            }

            goto CLEAN_CAPTURE;
        }
        // take the time before starting the capture
        gettimeofday (&curr_time, NULL);
        time = curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000;

        // open the output file we need to do this for every frame for
        // individual frame 
        // capture and only once for capture to movie
        if ((app->current_mode == 0) ||
            (app->current_mode > 0 && job->state & VC_START)) {

#ifdef DEBUG
            printf ("%s %s: opening file for captured frame(s)\n",
                    DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

            fp = getOutputFile ();
            if (!fp)
                return FALSE;
        }
        // if this is the first frame for the current job ...
        if (job->state & VC_START) {

#ifdef DEBUG
            printf ("%s %s: create data structure for image\n", DEBUGFILE,
                    DEBUGFUNCTION);
#endif     // DEBUG

#ifdef HAVE_LIBXFIXES
            if (app->flags & FLG_USE_XFIXES) {
                unsigned int mask, color;

                for (mask = 0; mask <= 255; mask++) {
                    for (color = 0; color <= 255; color++) {
                        top[(mask << 8) + color] = (color * (mask + 1)) >> 8;
                        bottom[(mask << 8) + color] =
                            (color * (256 - mask)) >> 8;
                    }
                }
            }
#endif     // HAVE_LIBXFIXES

            switch (capfunc) {
#ifdef HAVE_SHMAT
            case SHM:
                image = captureFrameCreatingImageSHM (app->dpy, &shminfo);
                break;
#endif     // HAVE_SHMAT
            case X11:
            default:
                image = captureFrameCreatingImage (app->dpy);
            }
            if (image) {
                // call the necessary XtoXYZ function to process the image
                (*job->save) (fp, image);
                xvc_job_remove_state (VC_START);
            } else
                printf ("%s %s: could not acquire pixmap!\n", DEBUGFILE,
                        DEBUGFUNCTION);

#ifdef HAVE_LIBXFIXES
            if (app->flags & FLG_USE_XFIXES)
                xvc_get_color_info (image, &c_info);
#endif     // HAVE_LIBXFIXES

        } else {
            // we're recording and not in the first frame ....
            // so we just read new data into the present image structure 
#ifdef DEBUG
            printf ("%s %s: reading an image in a data sturctur present\n",
                    DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

            switch (capfunc) {
#ifdef HAVE_SHMAT
            case SHM:
                ret = captureFrameToImage (app->dpy, image);
                break;
#endif     // HAVE_SHMAT
            case X11:
            default:
                ret = captureFrameToImage (app->dpy, image);
            }
            if (ret > 0)
                // call the necessary XtoXYZ function to process the image
                (*job->save) (fp, image);
            else
                printf
                    ("%s %s: attempt to acquire pixmap returned 'False'!\n",
                     DEBUGFILE, DEBUGFUNCTION);
        }

        // this again is for recording, no matter if first frame or any
        // other close the image file if single frame capture mode
        if (app->current_mode == 0) {

#ifdef DEBUG
            printf ("%s %s: done saving image, closing file descriptor\n",
                    DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

            fclose (fp);
        }
        // substract the time we needed for creating and saving the frame
        // to the file 
        gettimeofday (&curr_time, NULL);
        time1 = (curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000) - time;

        time = checkCaptureDuration (time, time1);
        // time the next capture

#ifdef DEBUG
        printf
            ("%s %s: submitting capture of next frame in %ld milliseconds\n",
             DEBUGFILE, DEBUGFUNCTION, time);
#endif     // DEBUG

        job->pic_no += target->step;

        // this might be a single step. If so, remove the state flag so we 
        // don't keep single stepping
        if (job->state & VC_STEP) {
            xvc_job_remove_state (VC_STEP);
            // the time is the pause between this and the next snapshot
            // for step mode this makes no sense and could be 0. Setting
            // it to 50 is just to give the led meter the chance to flash
            // before being set blank again
            time = 50;
        }
        // the following means VC_STATE != VC_REC
        // this can only happen in capture.c if we were capturing and are
        // just stopping
    } else {
        int orig_state;

        // clean up 
      CLEAN_CAPTURE:

#ifdef DEBUG
        printf ("%s %s: cleaning up\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

        time = 0;
        orig_state = job->state;       // store state here, esp. VC_CONTINUE

        if (image) {
            XDestroyImage (image);
            image = NULL;
        }

        xvc_job_set_state (VC_STOP);

        // set the sensitive stuff for the control panel if we don't
        // autocontinue 
        if ((orig_state & VC_CONTINUE) == 0)
            xvc_idle_add (xvc_capture_stop, job, TRUE);

        // clean up the save routines in xtoXXX.c 
        if (job->clean)
            (*job->clean) ();
        // if we're recording to a movie, we must close the file now
        if (app->current_mode > 0)
            if (fp)
                fclose (fp);
        fp = NULL;

        if ((orig_state & VC_CONTINUE) == 0) {
            // after this we're ready to start recording again 
            xvc_job_merge_state (VC_READY);
        } else {

#ifdef DEBUG
            printf
                ("%s %s: autocontinue selected, preparing autocontinue\n",
                 DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

            // prepare autocontinue
            job->movie_no += 1;
            job->pic_no = target->start_no;
            xvc_job_merge_and_remove_state ((VC_START | VC_REC), VC_STOP);

            return time;
        }
    }

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    return time;

#undef DEBUGFUNCTION
}

/** 
 * \brief function used for capturing. This one is used with source = x11,
 *      i. e. when capturing from X11 display w/o SHM
 *
 * @return the number of msecs in which the next capture is due
 */
long
xvc_capture_x11 ()
{
#define DEBUGFUNCTION "xvc_capture_x11()"
    return commonCapture (X11);
#undef DEBUGFUNCTION
}

#ifdef HAVE_SHMAT
/** 
 * \brief function used for capturing. This one is used with source = shm,
 *      i. e. when capturing from X11 with SHM support
 *
 * @return the number of msecs in which the next capture is due
 */
long
xvc_capture_shm ()
{
#define DEBUGFUNCTION "xvc_capture_shm()"
    return commonCapture (SHM);
#undef DEBUGFUNCTION
}

#endif     /* HAVE_SHMAT */

/*
 *
 * 
 *
 */
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

Boolean
TCbCaptureV4L (XtPointer xtp, XtIntervalId id *)
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
        dpy = DisplayOfScreen (scr);
    } else {
        dpy = XOpenDisplay (NULL);
    }

#ifdef DEBUG2
    printf ("TCbCaptureV4L() pic_no=%d\n", job->pic_no);
#endif
    if ((job->state & VC_PAUSE) && !(job->state & VC_STEP)) {
        xvc_add_timeout (job->time_per_frame, job->capture, job);

    } else if (job->state & VC_REC) {
        if (job->max_frames && ((job->pic_no - job->start_no) >
                                job->max_frames - 1)) {
            if (job->flags & FLG_RUN_VERBOSE)
                printf ("Stopped! pic_no=%d max_frames=%d\n",
                        job->pic_no, job->max_frames);
            if ((!(job->flags & FLG_RUN_VERBOSE)) & (!(job->flags & FLG_NOGUI)))
                xvc_change_filename_display (job->pic_no);

            if (job->flags & FLG_AUTO_CONTINUE)
                job->state |= VC_CONTINUE;
            goto CLEAN_V4L;
        }
        job->state &= ~VC_STEP;
        gettimeofday (&curr_time, NULL);
        time = curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000;

        if ((!(job->flags & FLG_MULTI_IMAGE)) ||
            ((job->flags & FLG_MULTI_IMAGE) && (job->state & VC_START))) {
            if (job->flags & FLG_MULTI_IMAGE)
                sprintf (file, job->file, job->movie_no);
            else
                sprintf (file, job->file, job->pic_no);
            fp = (*job->open) (file, job->open_flags);
            if (!fp) {
                perror (file);
                job->state = VC_STOP;
                return;
            }
        }
        if (job->state & VC_START) {
            /* 
             * the first time this procedure is started
             * we must prepare some stuff ..
             */
            if (!job->bpp)             /* use depth of the default window */
                job->bpp = job->win_attr.depth;

            sync ();                   /* remove if your bttv driver runs stable
                                        * .. */
            video = video_open (job->video_dev, O_RDWR);
            if (!video) {
                perror (job->video_dev);
                goto CLEAN_V4L;
            }
            vi_pict.depth = 0;
            /* 
             * read default values for hue, etc.. 
             */
            ioctl (video->fd, VIDIOCGPICT, &vi_pict);

            printf ("%d->%d %d\n", job->bpp, vi_pict.depth, vi_pict.palette);
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
                printf ("Fatal: unsupported bpp (%d)\n", job->bpp);
                exit (3);
                break;
            }
            ioctl (video->fd, VIDIOCSPICT, &vi_pict);
            printf ("%d->%d %d\n", job->bpp, vi_pict.depth, vi_pict.palette);

            vi_memb.size = 0;
            ioctl (video->fd, VIDIOCGMBUF, &vi_memb);
            printf ("%d %d %d\n", vi_memb.size, vi_memb.frames,
                    vi_memb.offsets);

            image = (XImage *) XtMalloc (sizeof (XImage));
            if (!image) {
                printf ("Can't get image: %dx%d+%d+%d\n", job->area->width,
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
                printf ("Fatal: unsupported bpp (%d)\n", job->bpp);
                exit (3);
                break;
            }
            image->width = job->area->width;
            image->height = job->area->height;
            image->bits_per_pixel = job->bpp;
            image->bytes_per_line = job->bpp / 8 * image->width;
            image->byte_order = MSBFirst;
            size = image->width * image->height * job->bpp;
            video->size = vi_memb.size;
            video_mmap (video, 1);
            if (video->mmap == NULL) {
                perror ("mmap()");
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
        if (ioctl (video->fd, VIDIOCMCAPTURE, &vi_mmap) < 0) {
            perror ("ioctl(capture)");
            /* 
             * if (vb.frame) vb.frame = 0; else vb.frame = 1; 
             */
            goto CLEAN_V4L;
        }
        printf ("syncing ..\n");
        if (ioctl (video->fd, VIDIOCSYNC, vi_mmap.frame) < 0) {
            perror ("ioctl(sync)");
        }
        printf ("synced()\n");

        (*job->save) (fp, image, job);
        job->state &= ~VC_START;

        if (!(job->flags & FLG_MULTI_IMAGE))
            (*job->close) (fp);

        /* 
         * substract the time we needed for creating and saving the frame
         * to the file 
         */
        gettimeofday (&curr_time, NULL);
        time1 = (curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000) - time;
        // update monitor widget, here time is the time the capture took
        // time == 0 resets led_meter
        if (!time1)
            time1 = 1;
        if (!(job->flags & FLG_NOGUI))
            xvc_frame_monitor (time1);
        // printf("time: %i time_per_frame: %i\n", time1,
        // job->time_per_frame);
        // calculate the remaining time we have till capture of next frame
        time1 = job->time_per_frame - time1;

        if (time1 > 0) {
            // get time again because updating frame drop meter took some
            // time
            gettimeofday (&curr_time, NULL);
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
        if (time < 2)
            time = 2;

        xvc_add_timeout (time, job->capture, job);

        job->pic_no += job->step;
        if (time >= 2 && (!(job->flags & FLG_NOGUI)))
            xvc_change_filename_display (job->pic_no);

    } else {
        int orig_state;

        /* 
         * clean up 
         */
      CLEAN_V4L:
        orig_state = job->state;       // store state here

        if (!(job->flags & FLG_NOGUI)) {
            // maybe the last update didn't succeed
            xvc_change_filename_display (job->pic_no);
            xvc_frame_monitor (0);
        }

        job->state = VC_STOP;
        if (image) {
            XtFree ((char *) image);
            image = NULL;
        }
        if (video) {
            video_mmap (video, 0);
            video_close (video);
            video = NULL;
        }

        /* 
         * set the sensitive stuff for the control panel if we don't
         * autocontinue 
         */
        if ((orig_state & VC_CONTINUE) == 0)
            xvc_capture_stop (job);

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
            xvc_capture_start (job);
            return;
        }

    }

    /* 
     * after this we're ready to start recording again 
     */
    job->state |= VC_READY;

    if (job->flags & FLG_NOGUI && (!is_filename_mutable (job->file))) {
        XCloseDisplay (dpy);
    }

    return FALSE;
}
#endif     /* HasVideo4Linux */

#ifdef HasDGA
/* 
 * direct graphic access
 * this doesn't work until now and may be removed in future..!?
 *
 * IT HAS ALSO NOT BEEN REWRITTEN FOR GTK GUI SUPPORT
 */
Boolean
TCbCaptureDGA (XtPointer xtp, XtIntervalId id *)
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
    printf ("TCbCaptureDGA() pic_no=%d state=%d\n", job->pic_no, job->state);
#endif
    if ((job->state & VC_PAUSE) && !(job->state & VC_STEP)) {
        XtAppAddTimeOut (XtWidgetToApplicationContext (job->toplevel),
                         job->time_per_frame,
                         (XtTimerCallbackProc) job->capture, job);

    } else if (job->state & VC_REC) {
        if (job->max_frames && ((job->pic_no - job->start_no) >
                                job->max_frames - 1)) {
            if (job->flags & FLG_RUN_VERBOSE)
                printf ("Stopped! pic_no=%d max_frames=%d\n",
                        job->pic_no, job->max_frames);
            if (!(job->flags & FLG_RUN_VERBOSE))
                ChangeLabel (job->pic_no);
            goto CLEAN_DGA;
        }
        job->state &= ~VC_STEP;
        gettimeofday (&curr_time, NULL);
        time = curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000;

        if ((!(job->flags & FLG_MULTI_IMAGE)) ||
            ((job->flags & FLG_MULTI_IMAGE) && (job->state & VC_START))) {
            if (job->flags & FLG_MULTI_IMAGE)
                sprintf (file, job->file, job->movie_no);
            else
                sprintf (file, job->file, job->pic_no);
            fp = (*job->open) (file, job->open_flags);
            if (!fp) {
                perror (file);
                job->state = VC_STOP;
                return;
            }
        }
        if (job->state & VC_START) {
            /* 
             * the first time this procedure is started
             * we must create a new image structure ..
             */
            dpy = XtDisplay (job->toplevel);
            image = (XImage *) XtMalloc (sizeof (XImage));
            if (!image) {
                printf ("Can't get image: %dx%d+%d+%d\n", job->area->width,
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

                XF86DGAGetVideo (dpy, XDefaultScreen (dpy), (char **) &base,
                                 &width, &bank, &ram);
                image->data = base;
            }
            XF86DGADirectVideo (dpy, XDefaultScreen (dpy),
                                XF86DGADirectGraphics);
            (*job->save) (fp, image, job);
            XF86DGADirectVideo (dpy, XDefaultScreen (dpy), 0);
            job->state &= ~VC_START;
        } else {
            /* 
             * just read new data in the image structure 
             */
            XF86DGADirectVideo (dpy, XDefaultScreen (dpy),
                                XF86DGADirectGraphics);
            (*job->save) (fp, image, job);
            XF86DGADirectVideo (dpy, XDefaultScreen (dpy), 0);
        }
        if (!(job->flags & FLG_MULTI_IMAGE))
            (*job->close) (fp);

        /* 
         * substract the time we needed for creating and saving the frame
         * to the file 
         */
        gettimeofday (&curr_time, NULL);
        time = (curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000) - time;
        time = job->time_per_frame - time;
        if (time < 0) {
            if (job->flags & FLG_RUN_VERBOSE)
                printf
                    ("missing %ld milli secs (%d needed per frame), pic no %d\n",
                     time, job->time_per_frame, job->pic_no);
            time = 0;
        }
        XtAppAddTimeOut (XtWidgetToApplicationContext (job->toplevel),
                         time, (XtTimerCallbackProc) job->capture, job);
        job->pic_no += job->step;
        /* 
         * update the label if we have time to do this 
         */
        if (time)
            ChangeLabel (job->pic_no);
    } else {
        /* 
         * clean up 
         */
      CLEAN_DGA:
        /* 
         * may be the last update failed .. so do it here before stop 
         */
        ChangeLabel (job->pic_no);
        job->state = VC_STOP;
        if (image) {
            XtFree ((char *) image);
            image = NULL;
        }
        XF86DGADirectVideo (dpy, XDefaultScreen (dpy), 0);
        /* 
         * set the sensitive stuff for the control panel 
         */
        CbStop (NULL, NULL, NULL);

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
#endif     /* HasDGA */
