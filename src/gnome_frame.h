/* 
 * gnome_frame.h
 *
 * Copyright (C) 1997 Rasca Gmelch, Berlin
 * Copyright (C) 2003,04,05,06 Karl H. Beckers, Frankfurt
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


#ifndef __XVC_GNOME_FRAME_H__
#define __XVC_GNOME_FRAME_H__

#define FRAME_WIDTH 3
#define FRAME_OFFSET (4*FRAME_WIDTH)

#include "gtk/gtk.h"


void xvc_change_gtk_frame(int x, int y, int width, int height, 
                          Boolean reposition_control);
void xvc_create_gtk_frame(GtkWidget * w, int width, int height, int x,
                          int y);
void xvc_destroy_gtk_frame(void);


#endif                          // __XVC_GNOME_FRAME_H__
