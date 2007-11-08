/**
 * \file frame.c
 *
 * This file contains some state variables for the capture area frame
 * and generic functions like setter/getter for state etc.
 */

/*
 * Copyright (C) 1997 Rasca Gmelch, Berlin
 * Copyright (C) 2004-07 Karl, Frankfurt
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

#include <stdio.h>
#include <X11/Intrinsic.h>
#include "frame.h"
#include "app_data.h"

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define DEBUGFILE "frame.c"
#endif     // DOXYGEN_SHOULD_SKIP_THIS

/** \brief stores the state of the frame lock */
static int xvc_frame_lock;

/** \brief stores the area enclosed by the frame as an XRectangle */
static XRectangle xvc_frame_rectangle;

/**
 * \brief return a pointer to the current area enclosed by the capture frame
 *
 * @return a pointer to an XRectangle struct
 */
XRectangle *
xvc_get_capture_area (void)
{
    return (&xvc_frame_rectangle);
}

/**
 * \brief get the state of the frame lock
 *
 * @return integer state of the frame lock: 1 = locked, 0 = unlocked
 */
int
xvc_is_frame_locked ()
{
    return (xvc_frame_lock);
}

/**
 * \brief set the state of the frame lock
 *
 * @param lock integer state of the frame lock: 0 = unlocked, >0 = locked
 */
void
xvc_set_frame_locked (int lock)
{
    if (lock == 0)
        xvc_frame_lock = lock;
    else
        xvc_frame_lock = 1;
}

/**
 * \brief get the window attributes for the given window
 *
 * @param win a Window to retrieve the attributes for. Can be a single window,
 *      the root window (if manually selected) or None (in which case we will
 *      select the root window ourselves
 * @param wa return pointer to the XWindowAttributes struct to write to
 */
void
xvc_get_window_attributes (Display * dpy, Window win, XWindowAttributes * wa)
{
#define DEBUGFUNCTION "xvc_get_window_attributes()"
    if (win == None) {
        win = DefaultRootWindow (dpy);
    }

    if (!XGetWindowAttributes (dpy, win, wa)) {
        char msg[256];

        snprintf (msg, 256, "%s %s: Can't get window attributes!\n",
                  DEBUGFILE, DEBUGFUNCTION);
        perror (msg);
    }
#undef DEBUGFUNCTTION
}
