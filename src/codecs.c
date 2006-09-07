/* 
 * codecs.c,
 *
 * Copyright (C) 2004-06 Karl, Frankfurt
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
#include <string.h>
#include <locale.h>
#include <ctype.h>
#include <libintl.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "app_data.h"
#include "codecs.h"
#include "xvidcap-intl.h"

#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>

#define DEBUGFILE "codecs.c"

xvCodec tCodecs[NUMCODECS];
xvFFormat tFFormats[NUMCAPS];
xvAuCodec tAuCodecs[NUMAUCODECS];


const xvCodec none = {
    "NONE",
    N_("NONE"),
    CODEC_NONE,
    CODEC_ID_NONE,
    0,
    0,
    NULL,
    NULL
};

#ifdef USE_FFMPEG
const xvCodec pgm = {
    "PGM",
    N_("Portable Graymap"),
    CODEC_PGM,
    CODEC_ID_PGM,
    300000,
    24,
    "0.1-100",
    NULL
};

const xvCodec ppm = {
    "PPM",
    N_("Portable Pixmap"),
    CODEC_PPM,
    CODEC_ID_PPM,
    300000,
    24,
    "0.1-100",
    NULL
};

const xvCodec png = {
    "PNG",
    N_("Portable Network Graphics"),
    CODEC_PNG,
    CODEC_ID_PNG,
    300000,
    24,
    "0.1-100",
    NULL
};

const xvCodec jpeg = {
    "JPEG",
    N_("Joint Picture Expert Group"),
    CODEC_JPEG,
    CODEC_ID_JPEGLS,
    300000,
    24,
    "0.1-100",
    NULL
};

const xvCodec mpeg1 = {
    "MPEG1",
    N_("MPEG 1"),
    CODEC_MPEG1,
    CODEC_ID_MPEG1VIDEO,
    300000,
    24,
    NULL,
    "24|25|30|50|60"
    // FIXME: adapt to ffmpeg's new fractional fps handling
    //    "23.976|24|25|29.97|30|50|59.94|60"
};

const xvCodec mjpeg = {
    "MJPEG",
    N_("MJPEG"),
    CODEC_MJPEG,
    CODEC_ID_MJPEG,
    300000,
    24,
    "7.5-30",                   // this is actually MPEG4 ... no idea if
    // this is the same here
    NULL
};

const xvCodec mpeg4 = {
    "MPEG4",
    N_("MPEG 4 (DIVX)"),
    CODEC_MPEG4,
    CODEC_ID_MPEG4,
    300000,
    24,
    "7.5-30",
    NULL
};

const xvCodec ms_div2 = {
    "MS_DIV2",
    N_("Microsoft DIVX 2"),
    CODEC_MSDIV2,
    CODEC_ID_MSMPEG4V2,
    300000,
    24,
    "7.5-30",                   // this is actually MPEG4 ... no idea if
    // this is the same here
    NULL
};

const xvCodec ms_div3 = {
    "MS_DIV3",
    N_("Microsoft DIVX 3"),
    CODEC_MSDIV3,
    CODEC_ID_MSMPEG4V3,
    300000,
    24,
    "7.5-30",                   // this is actually MPEG4 ... no idea if
    // this is the same here
    NULL
};

const xvCodec flv1 = {
    "FLASH_VIDEO",
    N_("Flash Video"),
    CODEC_FLV,
    CODEC_ID_FLV1,
    300000,
    24,
    "7.5-30",                   // this is actually MPEG4 ... no idea if
    // this is the same here
    NULL
};

const xvCodec dv = {
    "DV",
    N_("DV Video"),
    CODEC_DV,
    CODEC_ID_DVVIDEO,
    300000,
    25,
    NULL,
    "25"
    // FIXME: adapt to ffmpeg's new fractional fps handling
    //    "25|29.97"
    
};

const xvCodec mpeg2 = {
    "MPEG2",
    N_("MPEG2 Video"),
    CODEC_MPEG2,
    CODEC_ID_MPEG2VIDEO,
    300000,
    24,
    NULL,
    "24|25|30|50|60"
    // FIXME: adapt to ffmpeg's new fractional fps handling
    //    "23.976|24|25|29.97|30|50|59.94|60"
};

const xvCodec svq1 = {
    "SVQ1",
    N_("Soerensen VQ 1"),
    CODEC_SVQ1,
    CODEC_ID_SVQ1,
    300000,
    24,
    "7.5-30",                   // this is actually MPEG4 ... no idea if
    // this is the same here
    NULL
};
#endif                          // USE_FFMPEG


// audio codecs

const xvAuCodec none_aucodec = {
    "NONE",
    N_("NONE"),
    AU_CODEC_NONE,
    CODEC_ID_NONE
};

#ifdef USE_FFMPEG
#ifdef HAVE_FFMPEG_AUDIO
const xvAuCodec mp2 = {
    "MP2",
    N_("MPEG2"),
    AU_CODEC_MP2,
    CODEC_ID_MP2
};

#ifdef HAVE_LIBMP3LAME
const xvAuCodec mp3 = {
    "MP3",
    N_("MPEG2 Layer 3"),
    AU_CODEC_MP3,
    CODEC_ID_MP3
};
#endif                          // HAVE_LIBMP3LAME

const xvAuCodec pcm16 = {
    "PCM16",
    N_("PCM"),
    AU_CODEC_PCM16,
    CODEC_ID_PCM_S16LE
};
#endif                          // HAVE_FFMPEG_AUDIO
#endif                          // USE_FFMPEG


// file formats

const xvFFormat none_format = {
    "NONE",
    N_("NONE"),
    "NONE",
    NULL,
    CODEC_NONE,
    NULL,
    AU_CODEC_NONE,
    NULL /*,
    NULL */
};

const xvFFormat xwd = {
    "xwd",
    N_("X11 Window Dump"),
    NULL,
    "xwd",
    CODEC_NONE,
    NULL,
    AU_CODEC_NONE,
    NULL /*,
    NULL */
};

#ifdef USE_FFMPEG
const xvFFormat pgm_format = {
    "pgm",
    N_("Portable Graymap File"),
    NULL,
    "pgm",
    CODEC_PGM,
    "PGM",
    AU_CODEC_NONE,
    NULL /*,
    NULL */
};

const xvFFormat ppm_format = {
    "ppm",
    N_("Portable Anymap File"),
    NULL,
    "ppm",
    CODEC_PPM,
    "PPM",
    AU_CODEC_NONE,
    NULL /*,
    NULL */
};

const xvFFormat png_format = {
    "png",
    N_("Portable Network Graphics File"),
    NULL,
    "png",
    CODEC_PNG,
    "PNG",
    AU_CODEC_NONE,
    NULL /*,
    NULL */
};

const xvFFormat jpeg_format = {
    "jpg",
    N_("Joint Picture Expert Group"),
    NULL,
    "jpg|jpeg",
    CODEC_JPEG,
    "JPEG",
    AU_CODEC_NONE,
    NULL /*,
    jpeg_init */
};

const xvFFormat avi = {
    "avi",
    N_("Microsoft Audio Video Interleaved File"),
    "avi",
    "avi",
    CODEC_MSDIV2,
    "MPEG1|MJPEG|MPEG4|MS_DIV2|MPEG2|DV",
#ifdef HAVE_FFMPEG_AUDIO
    AU_CODEC_MP2,
#ifdef HAVE_LIBMP3LAME
    "MP2|MP3|PCM16" //,
#else
    "MP2|PCM16" //,
#endif                          // HAVE_LIBMP3LAME
#else
    AU_CODEC_NONE,
    NULL //,
#endif                          // HAVE_FFMPEG_AUDIO
//    avienc_init
};

const xvFFormat divx = {
    "divx",
    N_("General AVI file (DIVX default)"),
    "avi",
    "mpeg|mpg",
    CODEC_MPEG4,
    "MPEG1|MJPEG|MPEG4|MS_DIV2|MPEG2|DV",
#ifdef HAVE_FFMPEG_AUDIO
    AU_CODEC_MP2,
#ifdef HAVE_LIBMP3LAME
    "MP2|MP3|PCM16" //,
#else
    "MP2|PCM16" //,
#endif                          // HAVE_LIBMP3LAME
#else
    AU_CODEC_NONE,
    NULL //,
#endif                          // HAVE_FFMPEG_AUDIO
//    avienc_init
};

const xvFFormat asf = {
    "asf",
    N_("Microsoft Advanced Streaming Format"),
    "asf",
    "asf",
    CODEC_MSDIV3,
    "MS_DIV3",
#ifdef HAVE_FFMPEG_AUDIO
    AU_CODEC_NONE,
#ifdef HAVE_LIBMP3LAME
    "MP2|MP3" //,
#else
    "MP2" //,
#endif                          // HAVE_LIBMP3LAME
#else
    AU_CODEC_NONE,
    NULL //,
#endif                          // HAVE_FFMPEG_AUDIO
//    asf_init
};

const xvFFormat ff_flv1 = {
    "flv1",
    N_("Macromedia Flash Video Stream"),
    "flv",
    "flv|flv1",
    CODEC_FLV,
    "FLASH_VIDEO",
#ifdef HAVE_FFMPEG_AUDIO
#ifdef HAVE_LIBMP3LAME
    AU_CODEC_MP3,
    "MP3"
#else
    AU_CODEC_NONE,
    NULL
#endif // HAVE_LIBMP3LAME
#else
    AU_CODEC_NONE,
    NULL
#endif // HAVE_FFMPEG_AUDIO
    /*, flvenc_init */
};

const xvFFormat swf = {
    "swf",
    N_("Macromedia Shockwave Flash File"),
    "swf",
    "swf",
    CODEC_FLV,
    "FLASH_VIDEO|MJPEG",
#ifdef HAVE_FFMPEG_AUDIO
#ifdef HAVE_LIBMP3LAME
    AU_CODEC_MP3,
    "MP2|MP3" //,
#else
    AU_CODEC_MP2,
    "MP2" //,
#endif                          // HAVE_LIBMP3LAME
#else
    AU_CODEC_NONE,
    NULL //,
#endif                          // HAVE_FFMPEG_AUDIO
//    swf_init
};

const xvFFormat dv_format = {
    "dv",
    N_("DV Video Format"),
    "dv",
    "dv",
    CODEC_DV,
    "DV",
#ifdef HAVE_FFMPEG_AUDIO
    AU_CODEC_MP2,
    "PCM16" //,
#else
    AU_CODEC_NONE,
    NULL //,
#endif                          // HAVE_FFMPEG_AUDIO
//    ff_dv_init
};

const xvFFormat mpeg = {
    "mpeg",
    N_("MPEG1 System Format"),
    "mpeg",
    "m1v|vcd",
    CODEC_MPEG1,
    "MPEG1",
#ifdef HAVE_FFMPEG_AUDIO
    AU_CODEC_MP2,
    "MP2" //,
#else
    AU_CODEC_NONE,
    NULL //,
#endif                          // HAVE_FFMPEG_AUDIO
//    mpegps_init
};

const xvFFormat svcd = {
    "mpeg2",
    N_("MPEG2 PS Format"),
    "svcd",
    "m2v|svcd",
    CODEC_MPEG2,
    "MPEG2",
#ifdef HAVE_FFMPEG_AUDIO
    AU_CODEC_MP2,
    "MP2" //,
#else
    AU_CODEC_NONE,
    NULL //,
#endif                          // HAVE_FFMPEG_AUDIO
//    mpegps_init
};

const xvFFormat mov = {
    "mov",
    N_("Quicktime Format"),
    "mov",
    "mov|qt",
    CODEC_MPEG4,
    "MPEG4|SVQ1|DV",
#ifdef HAVE_FFMPEG_AUDIO
#ifdef HAVE_LIBMP3LAME
    AU_CODEC_MP3,
    "MP2|MP3|PCM16" //,
#else
    AU_CODEC_PCM16,
    "MP2|PCM16" //,
#endif                          // HAVE_LIBMP3LAME
#else
    AU_CODEC_NONE,
    NULL //,
#endif                          // HAVE_FFMPEG_AUDIO
//    mpegps_init
};
#endif                          // USE_FFMPEG


/* 
 * must be called by main() to initialize data structures
 */
void xvc_codecs_init()
{
    int i = 0;

    // these are absolutely required to be in the same order as the
    // enumeration of CAP_* types in codecs.h
    tCodecs[i++] = none;
#ifdef USE_FFMPEG
    tCodecs[i++] = pgm;
    tCodecs[i++] = ppm;
    tCodecs[i++] = png;
    tCodecs[i++] = jpeg;
    tCodecs[i++] = mpeg1;
    tCodecs[i++] = mjpeg;
    tCodecs[i++] = mpeg4;
    tCodecs[i++] = ms_div2;
    tCodecs[i++] = ms_div3;
    tCodecs[i++] = flv1;
    tCodecs[i++] = dv;
    tCodecs[i++] = mpeg2;
    tCodecs[i++] = svq1;
#endif                          // USE_FFMPEG

    // dto.
    // the target item of the array will be found by CAP_XXX - CAP_FFM
    i = 0;
    tFFormats[i++] = none_format;
    tFFormats[i++] = xwd;
#ifdef USE_FFMPEG
    tFFormats[i++] = pgm_format;
    tFFormats[i++] = ppm_format;
    tFFormats[i++] = png_format;
    tFFormats[i++] = jpeg_format;

    tFFormats[i++] = avi;
    tFFormats[i++] = divx;
    tFFormats[i++] = asf;
    tFFormats[i++] = ff_flv1;
    tFFormats[i++] = swf;
    tFFormats[i++] = dv_format;
    tFFormats[i++] = mpeg;
    tFFormats[i++] = svcd;
    tFFormats[i++] = mov;
#endif                          // USE_FFMPEG

    // audio codecs
    i = 0;
    tAuCodecs[i++] = none_aucodec;
#ifdef USE_FFMPEG
#ifdef HAVE_FFMPEG_AUDIO
    tAuCodecs[i++] = mp2;
#ifdef HAVE_LIBMP3LAME

    tAuCodecs[i++] = mp3;
#endif                          // HAVE_LIBMP3LAME
    tAuCodecs[i++] = pcm16;
#endif                          // HAVE_FFMPEG_AUDIO
#endif // USE_FFMPEG
}

/* 
 * find ffmpeg codec id from xvidcap's
 * 0 is a valid id ... therefore we return -1 on failure
 */
int xvc_trans_codec(int xv_codec)
{
    int i, ret = -1;

    for (i = 0; i < NUMCODECS; i++) {
        if (tCodecs[i].id == xv_codec)
            ret = tCodecs[i].ffmpeg_id;
    }
    return ret;
}

/* 
 * check if string is element in list (e.g. allowed_vid_codecs)
 */
int xvc_is_element(char *xvList, char *xvElement)
{
    char llist[512];
    char lelement[512];
    char *found = NULL;
    int ret, y;

    sprintf(llist, "|%s|", xvList);
    for (y = 0; y < strlen(llist); y++) {
        llist[y] = toupper(llist[y]);
    }

    sprintf(lelement, "|%s|", xvElement);
    for (y = 0; y < strlen(lelement); y++) {
        lelement[y] = toupper(lelement[y]);
    }

    found = strstr(llist, lelement);
    if (found != NULL)
        ret = 1;
    else
        ret = 0;
    return ret;
}

/* 
 * enumerate list (e.g. allowed_vid_codecs)
 */
char *xvc_next_element(char *list)
{
    static char *p_list;
    static char llist[512];
    char *ret;

    if (list != NULL) {
        sprintf(llist, "|%s|", list);
        ret = strtok_r(llist, "|", &p_list);
    } else {
        ret = strtok_r(NULL, "|", &p_list);
    }
    return ret;
}

/* 
 * find target based on filename
 * returns CAP_* or 0 if none found
 */
int xvc_codec_get_target_from_filename(char *file)
{
    char *ext = NULL, *element = NULL;
    int ret = 0, n;

    ext = rindex(file, '.');
    if (ext == NULL) {
        return ret;
    }
    ext++;

    for (n = CAP_NONE; n < NUMCAPS; n++) {
        if (tFFormats[n].extensions) {
            element = xvc_next_element(tFFormats[n].extensions);
            while (element != NULL) {
                if (strcasecmp(ext, element) == 0) {
                    // then we have found the right extension in target n
                    ret = n;
                    return ret;
                }
                element = xvc_next_element(NULL);
            }
        }
    }

    return ret;
}

/* 
 * check if fps rate is valid for given codec
 * returns 0 for false or 1 for true
 */
int xvc_codec_is_valid_fps(int fps, int codec)
{
    #define DEBUGFUNCTION "xvc_codec_is_valid_fps()"
    char *element = NULL;

    if (tCodecs[codec].allowed_fps) {
        element = xvc_next_element(tCodecs[codec].allowed_fps);
        while (element) {
            int f = xvc_get_int_from_float_string(element);
            if (f < 0)
                fprintf(stderr,
                        "%s %s: Non-fatal error in the definition of valid fps for codec %s\n",
                        DEBUGFILE, DEBUGFUNCTION,
                        tCodecs[codec].name);
            else if (f == fps)
                return 1;
            element = xvc_next_element(NULL);
        }
    }

    if (tCodecs[codec].allowed_fps_ranges) {
        element = xvc_next_element(tCodecs[codec].allowed_fps_ranges);
        while (element) {
            int start = 0, end = 0;
            char *start_str = strdup(element);
            char *ptr = index(start_str, '-');
            *ptr = '\0';
            ptr++;

            start = xvc_get_int_from_float_string(start_str);
            end = xvc_get_int_from_float_string(ptr);

            if (start < 0 || end < 0) {
                fprintf(stderr,
                        "%s %s: Non-fatal error in the definition of valid fps ranges for codec %s\n",
                        DEBUGFILE, DEBUGFUNCTION,
                        tCodecs[codec].name);
            } else if (start <= fps && fps <= end)
                return 1;
            element = xvc_next_element(NULL);
        }
    }
    // FIXME: what kind of error do I want to report with the return code?
    return 0;
    #undef DEBUGFUNCTION
}


/* 
 * count elements in a list (e.g. allowed_vid_codecs)
 */
int xvc_num_elements(char *list)
{
    static char *p_list;
    static char llist[512];
    char *elem;
    int elem_count = 0;

    if (list != NULL) {
        sprintf(llist, "|%s|", list);
        elem = strtok_r(llist, "|", &p_list);
        do {
            elem_count++;
            elem = strtok_r(NULL, "|", &p_list);
        }
        while (elem != NULL);
    }
    return elem_count;
}
