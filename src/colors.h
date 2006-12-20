/* 
 * colors.h
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

#ifndef __COLORS_H__
#define __COLORS_H__

typedef struct
{
    unsigned long red_shift;
    unsigned long green_shift;
    unsigned long blue_shift;
    unsigned long alpha_shift;

    unsigned long max_val;
    unsigned long bit_depth;

    unsigned long red_max_val;
    unsigned long green_max_val;
    unsigned long blue_max_val;
    unsigned long alpha_max_val;

    unsigned long red_bit_depth;
    unsigned long green_bit_depth;
    unsigned long blue_bit_depth;
    unsigned long alpha_bit_depth;

    u_int32_t alpha_mask;
} ColorInfo;

void xvc_get_color_info (XImage * image, ColorInfo * return_struct);
int xvc_get_colors (Display * dpy, XWindowAttributes * winfo, XColor ** colors);

#endif     // __COLORS_H__
