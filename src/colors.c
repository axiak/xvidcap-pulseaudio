/**
 * \file colors.c
 *
 * This file contains utility functions for retrieving color information.
 */

/*
 * Copyright (C) 1997,98 Rasca, Berlin
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
#include <stdlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include "colors.h"
#include "app_data.h"

#define DEBUGFILE "colors.c"

/**
 * \brief retrieves XColor array for a given window
 *
 * @param dpy pointer to the display to retrieve the information from
 * @param winfo pointer to window attributes for which to retrieve colors
 *      information
 * @param colors return pointer to an array of XColor elements
 * @return the number of colors retrieved
 */
int
xvc_get_colors (Display * dpy, const XWindowAttributes * winfo,
                XColor ** colors)
{
# define lowbit(x) ((x) & (~(x) + 1))
    int ncolors, i;
    Visual *visual;

    visual = winfo->visual;
    ncolors = visual->map_entries;
    if (*colors)
        free (*colors);
    *colors = (XColor *) malloc (sizeof (XColor) * ncolors);
    if (!*colors) {
        perror ("GetColors()");
        return (0);
    }
    if ((visual->class == DirectColor) || (visual->class == TrueColor)) {
        Pixel red, green, blue, red0, green0, blue0;

#ifdef DEBUG
        printf ("GetColors() Direct- or TrueColor (%d)\n", ncolors);
#endif
        red = green = blue = 0;
        red0 = lowbit (visual->red_mask);
        green0 = lowbit (visual->green_mask);
        blue0 = lowbit (visual->blue_mask);
        for (i = 0; i < ncolors; i++) {
            (*colors)[i].pixel = red | green | blue;
            (*colors)[i].pad = 0;
            red += red0;
            if (red > visual->red_mask)
                red = 0;
            green += green0;
            if (green > visual->green_mask)
                green = 0;
            blue += blue0;
            if (blue > visual->blue_mask)
                blue = 0;
        }
    } else {
        for (i = 0; i < ncolors; i++) {
            (*colors)[i].pixel = i;
            (*colors)[i].pad = 0;
        }
    }
    XQueryColors (dpy, winfo->colormap, *colors, ncolors);
#ifdef DEBUG2
    for (i = 0; i < ncolors; i++) {
        printf ("color[%d] pixel:%ld red:%d green:%d blue:%d\n", i,
                (*colors)[i].pixel,
                (*colors)[i].red, (*colors)[i].green, (*colors)[i].blue);
    }
#endif
    return (ncolors);
}

/**
 * \brief fills the ColorInfo struct with some useful color information, esp.
 *      the masks and shifts are relevant
 *
 * @param image pointer to the XImage to assimilate color information from
 * @param ci pointer to a ColorInfo struct
 */
ColorInfo *
xvc_get_color_info (const XImage * image)
{
#define DEBUGFUNCTION "xvc_get_color_info"

    unsigned long red_mask, green_mask, blue_mask, alpha_mask;
    ColorInfo *ci;

    ci = (ColorInfo *) malloc (sizeof (ColorInfo));
    if (!ci) {
        fprintf (stderr, "%s %s: malloc failed?!?", DEBUGFILE, DEBUGFUNCTION);
        exit (1);
    }
    // the shifts are unsigned longs as well

    if (!ci)
        return NULL;

    // setting shifts and bit_depths to zero
    ci->red_shift = ci->green_shift = ci->blue_shift = ci->alpha_shift = 0;
    ci->red_bit_depth = ci->green_bit_depth = ci->blue_bit_depth =
        ci->alpha_bit_depth = 0;

    red_mask = image->red_mask;
    if (red_mask > 0) {
        // shift red_mask to the right till all empty bits have been
        // shifted out and count how many they were
        while ((red_mask & 0x01) == 0) {
            red_mask >>= 1;
            ci->red_shift++;
        }
        // count how many bits are set in the mask = depth
        while ((red_mask & 0x01) == 1) {
            red_mask >>= 1;
            ci->red_bit_depth++;
        }
    }
    // why don't I just set this above when 0 bits are shifted out of the
    // mask??????
    ci->red_max_val = (1 << ci->red_bit_depth) - 1;

    green_mask = image->green_mask;
    if (green_mask > 0) {
        while ((green_mask & 0x01) == 0) {
            green_mask >>= 1;
            ci->green_shift++;
        }
        while ((green_mask & 0x01) == 1) {
            green_mask >>= 1;
            ci->green_bit_depth++;
        }
    }
    ci->green_max_val = (1 << ci->green_bit_depth) - 1;

    blue_mask = image->blue_mask;
    if (blue_mask > 0) {
        while ((blue_mask & 0x01) == 0) {
            blue_mask >>= 1;
            ci->blue_shift++;
        }
        while ((blue_mask & 0x01) == 1) {
            blue_mask >>= 1;
            ci->blue_bit_depth++;
        }
    }
    ci->blue_max_val = (1 << ci->blue_bit_depth) - 1;

    /*
     * over all max values
     */
    // whatever they are good for
    ci->max_val = XVC_MAX (ci->red_max_val, ci->green_max_val);
    ci->max_val = XVC_MAX (ci->blue_max_val, ci->max_val);
    ci->bit_depth = XVC_MAX (ci->red_bit_depth, ci->green_bit_depth);
    ci->bit_depth = XVC_MAX (ci->blue_bit_depth, ci->bit_depth);
    if (image->bits_per_pixel > image->depth) {
        /*
         * alpha?
         */
        // this seems to not reflect X's ignorance of alpha in its
        // masks
        ci->alpha_mask = ~
            (image->red_mask | image->blue_mask | image->green_mask);
        alpha_mask = ci->alpha_mask;
        if (alpha_mask > 0) {
            while ((alpha_mask & 0x01) == 0) {
                alpha_mask >>= 1;
                ci->alpha_shift++;
            }
            while ((alpha_mask & 0x01) == 1) {
                alpha_mask >>= 1;
                ci->alpha_bit_depth++;
            }
        }
        ci->alpha_max_val = (1 << ci->alpha_bit_depth) - 1;
    }
#ifdef DEBUG2
    printf ("rgb mask %08x bit_depth %lu max_val %lu\n",
            (uint) (image->red_mask | image->green_mask | image->blue_mask),
            ci->bit_depth, ci->max_val);
    printf ("red mask %08x bit_depth %lu shift %lu max_val %lu\n",
            (uint) (image->red_mask), ci->red_bit_depth, ci->red_shift,
            ci->red_max_val);
    printf ("green mask %08x bit_depth %lu shift %lu max_val %lu\n",
            (uint) (image->green_mask), ci->green_bit_depth,
            ci->green_shift, ci->green_max_val);
    printf ("blue mask %08x bit_depth %lu shift %lu max_val %lu\n",
            (uint) (image->blue_mask), ci->blue_bit_depth, ci->blue_shift,
            ci->blue_max_val);
    printf ("alpha mask %08x bit_depth %lu shift %lu max_val %lu\n",
            ci->alpha_mask, ci->alpha_bit_depth, ci->alpha_shift,
            ci->alpha_max_val);
#endif
    return ci;
#undef DEBUGFUNCTION
}
