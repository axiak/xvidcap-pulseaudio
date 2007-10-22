/**
 * \file xtoxwd.c
 *
 * This file contains the functions for saving captured frames to individual
 * xwd files.
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

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif     // HAVE_CONFIG_H
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
#include <inttypes.h>
#endif     // HAVE_STDINT_H

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <netinet/in.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/XWDFile.h>

#include "job.h"
#include "app_data.h"

/** \brief image size for ZPixmap */
#define ZImageSize(i) (i->bytes_per_line * i->height)

/**
 * \brief swap the byte order of a long integer (32 byte)
 *
 * @param i the 32-bit word to swap
 */
static void
swap_4byte (uint32_t * i)
{
    if (sizeof (long) != sizeof (uint32_t)) {
        *i = htonl (*i);
    } else {
        long *cursor = (long *) i;

        *cursor = htonl (*cursor);
    }
}

/**
 * \brief swap the byte order of a number of 32 bit word
 *
 * @param p pointer to a number of 32-bit words
 * @param n number of 32-bit words to swap
 */
static void
swap_n_4byte (unsigned char *p, unsigned long n)
{
    register unsigned long i;

    for (i = 0; i < n; i++) {
        if (sizeof (long) != sizeof (uint32_t)) {
            uint32_t *cursor = (uint32_t *) p;

            *cursor = htonl (*cursor);
        } else {
            long *cursor = (long *) p;

            *cursor = htonl (*cursor);
        }
        p += 4;
    }
}

/** \brief we need this to check if we must swap some bytes */
static unsigned long little_endian = 1;

/**
 * \brief prepare the color table for the xwd file
 *
 * @param colors colors as represented in the XImage
 * @param ncolors number of colors
 * @return a pointer to the converted color table
 */
void *
xvc_xwd_get_color_table (XColor * colors, int ncolors)
{
    XWDColor *color_table;
    int i;

    color_table = (XWDColor *) malloc (sizeof (XWDColor) * ncolors);
    if (!color_table)
        return (NULL);
    for (i = 0; i < ncolors; i++) {
        color_table[i].pixel = colors[i].pixel;
        color_table[i].red = colors[i].red;
        color_table[i].green = colors[i].green;
        color_table[i].blue = colors[i].blue;
        color_table[i].flags = colors[i].flags;
    }
    if (*(char *) &little_endian) {
        for (i = 0; i < ncolors; i++) {
            swap_4byte ((uint32_t *) (void *) &(color_table[i].pixel));
            color_table[i].red = htons (color_table[i].red);
            color_table[i].green = htons (color_table[i].green);
            color_table[i].blue = htons (color_table[i].blue);
        }
    }
    return (color_table);
}

/**
 * \brief main function to write ximage as individual frame to 'fp'
 *
 * @param fp file handle, this, however, is only used here
 * @param image the captured XImage to save
 * \todo remove fp from outside the save function. It is only needed here
 *      and should be handled here.
 */
void
xvc_xwd_save_frame (FILE * fp, XImage * image)
{
    static XWDFileHeader head;
    static int file_name_len;
    Job *job = xvc_job_ptr ();
    char *file = job->file;
    XVC_AppData *app = xvc_appdata_ptr ();

    /*
     * header must be prepared only once ..
     */
    if (job->state & VC_START /* it's the first call */ ) {
        XWindowAttributes win_attr = app->win_attr;

#ifdef DEBUG
        printf ("Preparing XWD header ... win_attr.x = %i\n", app->win_attr.x);
#endif
        file_name_len = strlen (file) + 1;
        head.header_size = (CARD32) (sizeof (head) + file_name_len);
        head.file_version = (CARD32) XWD_FILE_VERSION;
        head.pixmap_format = (CARD32) ZPixmap;
        head.pixmap_depth = (CARD32) image->depth;
        head.pixmap_width = (CARD32) image->width;
        head.pixmap_height = (CARD32) image->height;
        head.xoffset = (CARD32) image->xoffset;
        head.byte_order = (CARD32) image->byte_order;
        head.bitmap_unit = (CARD32) image->bitmap_unit;
        head.bitmap_bit_order = (CARD32) image->bitmap_bit_order;
        head.bitmap_pad = (CARD32) image->bitmap_pad;
        head.bits_per_pixel = (CARD32) image->bits_per_pixel;
        head.bytes_per_line = (CARD32) image->bytes_per_line;
        head.visual_class = (CARD32) win_attr.visual->class;
        head.red_mask = (CARD32) win_attr.visual->red_mask;
        head.green_mask = (CARD32) win_attr.visual->green_mask;
        head.blue_mask = (CARD32) win_attr.visual->blue_mask;
        head.bits_per_rgb = (CARD32) win_attr.visual->bits_per_rgb;
        head.colormap_entries = (CARD32) win_attr.visual->map_entries;
        head.ncolors = (CARD32) job->ncolors;
        head.window_width = (CARD32) win_attr.width;
        head.window_height = (CARD32) win_attr.height;
        head.window_x = (long) win_attr.x;
        head.window_y = (long) win_attr.y;
        head.window_bdrwidth = (CARD32) win_attr.border_width;

        if (*(char *) &little_endian)
            swap_n_4byte ((unsigned char *) &head,
                          sizeof (head) / sizeof (uint32_t));
    }
    if (fwrite ((char *) &head, sizeof (head), 1, fp) < 1)
        perror (file);
    if (fwrite (file, file_name_len, 1, fp) < 1)
        perror (file);
    if (fwrite (job->color_table, sizeof (XWDColor), job->ncolors, fp) < 1)
        perror (file);
    if (fwrite (image->data, ZImageSize (image), 1, fp) < 1)
        perror (file);

#ifdef DEBUG
    printf ("XImageToXWD() header size = %d visual=%d\n",
            sizeof (XWDFileHeader), app->win_attr.visual->class);
#endif
}
