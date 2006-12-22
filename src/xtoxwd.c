/* 
 * xtoxwd.c
 *
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif     // HAVE_CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/XWDFile.h>

#include "job.h"
#include "app_data.h"

extern AppData *app;


/* 
 * image size for ZPixmap 
 */
#define ZImageSize(i) (i->bytes_per_line * i->height)

// a couple of byte swap functions originally from util.c

/* 
 * swap the byte order of a 16 bit word;
 */
void
swap_2byte (unsigned short *i)
{
    unsigned char t;
    unsigned char *p = (unsigned char *) i;

    t = p[0];
    p[0] = p[1];
    p[1] = t;
}

/* 
 * swap the byte order of a long integer (32 byte)
 */
void
swap_4byte (unsigned long *i)
{
    unsigned char t;
    unsigned char *p = (unsigned char *) i;

    t = p[0];
    p[0] = p[3];
    p[3] = t;
    t = p[1];
    p[1] = p[2];
    p[2] = t;
}

/* 
 * swap the byte order of a 32 bit word;
 */
void
swap_n_4byte (unsigned char *p, unsigned long n)
{
    register unsigned char t;
    register unsigned long i;

    for (i = 0; i < n; i++) {
        t = p[0];
        p[0] = p[3];
        p[3] = t;
        t = p[1];
        p[1] = p[2];
        p[2] = t;
        p += 4;
    }
}

/* 
 * swap n bytes in a char array
 */
void
swap_n_bytes (unsigned char *p, unsigned long n)
{
    unsigned char t;
    unsigned int i, h;

    h = n-- / 2;
    for (i = 0; i < h; i++) {
        t = *(p + i);
        *(p + i) = *(p + (n - i));
        *(p + (n - i)) = t;
    }
}

/* 
 * global, we need this to check if we must swap some bytes
 */
static unsigned long little_endian = 1;

/* 
 * prepare the color table for the xwd file
 */
void *
XWDcolorTable (XColor * colors, int ncolors)
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
            swap_4byte (&color_table[i].pixel);
            swap_2byte (&color_table[i].red);
            swap_2byte (&color_table[i].green);
            swap_2byte (&color_table[i].blue);
        }
    }
    return (color_table);
}

/* 
 * write ximage as xwd file to 'fp'
 */
void
XImageToXWD (FILE * fp, XImage * image)
{
    static XWDFileHeader head;
    static int file_name_len;
    Job *job = xvc_job_ptr();
    char *file = job->file;

    /* 
     * header must be prepared only once .. 
     */
    if (job->state & VC_START /* it's the first call */ ) {
        XWindowAttributes win_attr = app->win_attr;

#ifdef DEBUG
        printf ("Preparing XWD header ... win_attr.x = %i\n", job->win_attr.x);
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
                          sizeof (head) / sizeof (long));
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
            sizeof (XWDFileHeader), job->win_attr.visual->class);
#endif
}

/* 
 * XwdClean() is not needed 
 */
