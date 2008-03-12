/**
 * \file frame.h
 */

/*
 * Copyright (C) 1997 Rasca Gmelch, Berlin
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
 */

#ifndef _xvc_XVC_FRAME_H__
#define _xvc_XVC_FRAME_H__

XRectangle *xvc_get_capture_area (void);
int xvc_is_frame_locked (void);
void xvc_set_frame_locked (int);
void xvc_get_window_attributes (Display * dpy, Window win,
                                XWindowAttributes * wa);

// the following are implementation dependent and actually implemented
// in gnome_frame.c
void xvc_frame_drop_capture_display ();
Display *xvc_frame_get_capture_display ();

#endif     // _xvc_XVC_FRAME_H__
