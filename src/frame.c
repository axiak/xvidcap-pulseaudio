/* 
 * frame.c
 *
 * Copyright (C) 1997 Rasca Gmelch, Berlin
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

#include <X11/Intrinsic.h>
#include "frame.h"

/* 
 * return a pointer to the current area enclosed by the frame
 */
XRectangle *
xvc_get_capture_area (void)
{
    extern XRectangle xvc_frame_rectangle;

    return (&xvc_frame_rectangle);
}

/* 
 * this is a convenience function 
 */
int
xvc_is_frame_locked ()
{
    extern int xvc_frame_lock;

    return (xvc_frame_lock);
}
