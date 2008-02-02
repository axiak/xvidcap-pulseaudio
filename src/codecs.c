/**
 * \file codecs.c
 *
 * This file contains all data types and functions related to video and audio
 * codecs and file formats.
 */
/*
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

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define DEBUGFILE "codecs.c"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#include <stdio.h>
#include <stdlib.h>
#ifdef SOLARIS
#include <strings.h>
#else      // SOLARIS
#include <string.h>
#endif     // SOLARIS
#include <ctype.h>
#include <libintl.h>
#include <math.h>
#include <locale.h>

#ifdef USE_FFMPEG
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>
#endif     // USE_FFMPEG

#include "app_data.h"
#include "codecs.h"
#include "xvidcap-intl.h"

/*
 * fps and fps_rage arrays for use in the codecs array
 */
static const XVC_FpsRange one_to_hundred_range[] = {
    {
     {1, 10},
     {100, 1}
     }
};

#define len_one_to_hundred_range (sizeof(one_to_hundred_range) / \
    sizeof(XVC_FpsRange))

static const XVC_FpsRange mpeg4_range[] = {
    {
     {75, 10},
     {30, 1}
     }
};

#define len_mpeg4_range (sizeof(mpeg4_range) / sizeof(XVC_FpsRange))

static const XVC_Fps mpeg1_fps[] = {
    {24000, 1001},
    {24, 1},
    {25, 1},
    {30000, 1001},
    {30, 1},
    {50, 1},
    {60000, 1001},
    {60, 1},
};

#define len_mpeg1_fps (sizeof(mpeg1_fps) / sizeof(XVC_Fps))

static const XVC_Fps dv_fps[] = {
    {25, 1},
    {30000, 1001},
};

#define len_dv_fps (sizeof(dv_fps) / sizeof(XVC_Fps))

/** \brief global array storing all available codecs */
const XVC_Codec xvc_codecs[NUMCODECS] = {
    {
     "NONE",
     N_("NONE"),
     CODEC_ID_NONE,
     {0, 1},
     NULL,
     0,
     NULL,
     0},
#ifdef USE_FFMPEG
    {
     "PGM",
     N_("Portable Graymap"),
     CODEC_ID_PGM,
     {24, 1},
     one_to_hundred_range,
     len_one_to_hundred_range,
     NULL,
     0},
    {
     "PPM",
     N_("Portable Pixmap"),
     CODEC_ID_PPM,
     {24, 1},
     one_to_hundred_range,
     len_one_to_hundred_range,
     NULL,
     0},
    {
     "PNG",
     N_("Portable Network Graphics"),
     CODEC_ID_PNG,
     {24, 1},
     one_to_hundred_range,
     len_one_to_hundred_range,
     NULL,
     0},
    {
     "JPEG",
     N_("Joint Picture Expert Group"),
     CODEC_ID_MJPEG,
     {24, 1},
     one_to_hundred_range,
     len_one_to_hundred_range,
     NULL,
     0},
    {
     "MPEG1",
     N_("MPEG 1"),
     CODEC_ID_MPEG1VIDEO,
     {24, 1},
     NULL,
     0,
     mpeg1_fps,
     len_mpeg1_fps},
    {
     "MJPEG",
     N_("MJPEG"),
     CODEC_ID_MJPEG,
     {24, 1},
     mpeg4_range,                      /* this is actually MPEG4 ... dunno if
                                        * this is the same here */
     len_mpeg4_range,
     NULL,
     0},
    {
     "MPEG4",
     N_("MPEG 4 (DIVX)"),
     CODEC_ID_MPEG4,
     {24, 1},
     mpeg4_range,
     len_mpeg4_range,
     NULL,
     0},
    {
     "MS_DIV2",
     N_("Microsoft DIVX 2"),
     CODEC_ID_MSMPEG4V2,
     {24, 1},
     mpeg4_range,                      /* this is actually MPEG4 ... dunno if
                                        * this is the same here */
     len_mpeg4_range,
     NULL,
     0},
    {
     "MS_DIV3",
     N_("Microsoft DIVX 3"),
     CODEC_ID_MSMPEG4V3,
     {24, 1},
     mpeg4_range,                      /* this is actually MPEG4 ... dunno if
                                        * this is the same here */
     len_mpeg4_range,
     NULL,
     0},
    {
     "FFV1",
     N_("FFmpeg Video 1"),
     CODEC_ID_FFV1,
     {24, 1},
     one_to_hundred_range,
     len_one_to_hundred_range,
     NULL,
     0},
    {
     "FLASH_VIDEO",
     N_("Flash Video"),
     CODEC_ID_FLV1,
     {24, 1},
     mpeg4_range,                      /* this is actually MPEG4 ... dunno if
                                        * this is the same here */
     len_mpeg4_range,
     NULL,
     0},
    {
     "FLASH_SV",
     N_("Flash Screen Video"),
     CODEC_ID_FLASHSV,
     {24, 1},
     one_to_hundred_range,
     len_one_to_hundred_range,
     NULL,
     0},
    {
     "DV",
     N_("DV Video"),
     CODEC_ID_DVVIDEO,
     {25, 1},
     NULL,
     0,
     dv_fps,
     len_dv_fps},
    {
     "MPEG2",
     N_("MPEG2 Video"),
     CODEC_ID_MPEG2VIDEO,
     {24, 1},
     NULL,
     0,
     mpeg1_fps,
     len_mpeg1_fps},
#ifdef HAVE_LIBTHEORA
    {
     "THEORA",
     N_("Ogg Theora"),
     CODEC_ID_THEORA,
     {24, 1},
     mpeg4_range,                      /* this is actually MPEG4 ... dunno if
                                        * this is the same here */
     len_mpeg4_range,
     NULL,
     0},
#endif     // HAVE_LIBTHEORA
    {
     "SVQ1",
     N_("Soerensen VQ 1"),
     CODEC_ID_SVQ1,
     {24, 1},
     mpeg4_range,                      /* this is actually MPEG4 ... dunno if
                                        * this is the same here */
     len_mpeg4_range,
     NULL,
     0}
#endif     // USE_FFMPEG
};

/**
 * \brief global array storing all available audio codecs
 */
const XVC_AuCodec xvc_audio_codecs[NUMAUCODECS] = {
    {
     "NONE",
     N_("NONE"),
     CODEC_ID_NONE}
#ifdef USE_FFMPEG
#ifdef HAVE_FFMPEG_AUDIO
    , {
       "MP2",
       N_("MPEG2"),
       CODEC_ID_MP2}
#ifdef HAVE_LIBMP3LAME
    , {
       "MP3",
       N_("MPEG2 Layer 3"),
       CODEC_ID_MP3}
#endif     // HAVE_LIBMP3LAME
    , {
       "VORBIS",
       N_("Ogg Vorbis"),
       CODEC_ID_VORBIS}
    , {
       "AC3",
       N_("Dolby Digital AC-3"),
       CODEC_ID_AC3}
    , {
       "PCM16",
       N_("PCM"),
       CODEC_ID_PCM_S16LE}
#endif     // HAVE_FFMPEG_AUDIO
#endif     // USE_FFMPEG
};

/*
 * arrays with extensions, allowed video and audio codecs for use in
 * the global xvc_formats array
 */
static const char *extension_xwd[] = { "xwd" };

#define len_extension_xwd (sizeof(extension_xwd) / sizeof(char*))

#ifdef USE_FFMPEG
static const char *extension_pgm[] = { "pgm" };

#define len_extension_pgm (sizeof(extension_pgm) / sizeof(char*))

static const char *extension_ppm[] = { "ppm" };

#define len_extension_ppm (sizeof(extension_ppm) / sizeof(char*))

static const char *extension_png[] = { "png" };

#define len_extension_png (sizeof(extension_png) / sizeof(char*))

static const char *extension_jpg[] = { "jpg", "jpeg" };

#define len_extension_jpg (sizeof(extension_jpg) / sizeof(char*))

static const char *extension_avi[] = { "avi" };

#define len_extension_avi (sizeof(extension_avi) / sizeof(char*))

static const char *extension_mpg[] = { "mpeg", "mpg" };

#define len_extension_mpg (sizeof(extension_mpg) / sizeof(char*))

static const char *extension_asf[] = { "asf" };

#define len_extension_asf (sizeof(extension_asf) / sizeof(char*))

static const char *extension_flv[] = { "flv", "flv1" };

#define len_extension_flv (sizeof(extension_flv) / sizeof(char*))

static const char *extension_swf[] = { "swf" };

#define len_extension_swf (sizeof(extension_swf) / sizeof(char*))

static const char *extension_dv[] = { "dv" };

#define len_extension_dv (sizeof(extension_dv) / sizeof(char*))

static const char *extension_m1v[] = { "m1v", "vcd" };

#define len_extension_m1v (sizeof(extension_m1v) / sizeof(char*))

static const char *extension_m2v[] = { "m2v", "svcd" };

#define len_extension_m2v (sizeof(extension_m2v) / sizeof(char*))

static const char *extension_dvd[] = { "vob", "dvd" };

#define len_extension_dvd (sizeof(extension_dvd) / sizeof(char*))

static const char *extension_mov[] = { "mov", "qt" };

#define len_extension_mov (sizeof(extension_mov) / sizeof(char*))

static const XVC_CodecID allowed_vid_codecs_pgm[] = { CODEC_PGM };

#define len_allowed_vid_codecs_pgm (sizeof(allowed_vid_codecs_pgm) / \
    sizeof(XVC_CodecID))

static const XVC_CodecID allowed_vid_codecs_ppm[] = { CODEC_PPM };

#define len_allowed_vid_codecs_ppm (sizeof(allowed_vid_codecs_ppm) / \
    sizeof(XVC_CodecID))

static const XVC_CodecID allowed_vid_codecs_png[] = { CODEC_PNG };

#define len_allowed_vid_codecs_png (sizeof(allowed_vid_codecs_png) / \
    sizeof(XVC_CodecID))

static const XVC_CodecID allowed_vid_codecs_jpg[] = { CODEC_JPEG };

#define len_allowed_vid_codecs_jpg (sizeof(allowed_vid_codecs_jpg) / \
    sizeof(XVC_CodecID))

static const XVC_CodecID allowed_vid_codecs_avi[] = {
    CODEC_MPEG1,
    CODEC_MJPEG,
    CODEC_MPEG4,
    CODEC_MSDIV2,
    CODEC_MPEG2,
#ifdef HAVE_LIBTHEORA
    CODEC_THEORA,
#endif     // HAVE_LIBTHEORA
    CODEC_DV,
    CODEC_FFV1
};

#define len_allowed_vid_codecs_avi (sizeof(allowed_vid_codecs_avi) / \
    sizeof(XVC_CodecID))

static const XVC_CodecID allowed_vid_codecs_asf[] = { CODEC_MSDIV3 };

#define len_allowed_vid_codecs_asf (sizeof(allowed_vid_codecs_asf) / \
    sizeof(XVC_CodecID))

static const XVC_CodecID allowed_vid_codecs_flv[] =
    { CODEC_FLV, CODEC_FLASHSV };

#define len_allowed_vid_codecs_flv (sizeof(allowed_vid_codecs_flv) / \
    sizeof(XVC_CodecID))

static const XVC_CodecID allowed_vid_codecs_swf[] = { CODEC_FLV, CODEC_MJPEG };

#define len_allowed_vid_codecs_swf (sizeof(allowed_vid_codecs_swf) / \
    sizeof(XVC_CodecID))

static const XVC_CodecID allowed_vid_codecs_dv[] = { CODEC_DV, CODEC_MJPEG };

#define len_allowed_vid_codecs_dv (sizeof(allowed_vid_codecs_dv) / \
    sizeof(XVC_CodecID))

static const XVC_CodecID allowed_vid_codecs_mpeg1[] = { CODEC_MPEG1 };

#define len_allowed_vid_codecs_mpeg1 (sizeof(allowed_vid_codecs_mpeg1) / \
    sizeof(XVC_CodecID))

static const XVC_CodecID allowed_vid_codecs_mpeg2[] = { CODEC_MPEG2 };

#define len_allowed_vid_codecs_mpeg2 (sizeof(allowed_vid_codecs_mpeg2) / \
    sizeof(XVC_CodecID))

static const XVC_CodecID allowed_vid_codecs_mov[] = {
    CODEC_MPEG4,
    CODEC_SVQ1,
    CODEC_DV
};

#define len_allowed_vid_codecs_mov (sizeof(allowed_vid_codecs_mov) / \
    sizeof(XVC_CodecID))

#ifdef HAVE_FFMPEG_AUDIO
static const XVC_AuCodecID allowed_au_codecs_avi[] = {
    AU_CODEC_MP2,
#ifdef HAVE_LIBMP3LAME
    AU_CODEC_MP3,
#endif     // HAVE_LIBMP3LAME
    AU_CODEC_VORBIS,
    AU_CODEC_PCM16
};

#define len_allowed_au_codecs_avi (sizeof(allowed_au_codecs_avi) / \
    sizeof(XVC_AuCodecID))

static const XVC_AuCodecID au_codecs_mp2_and_mp3[] = {
#ifdef HAVE_LIBMP3LAME
    AU_CODEC_MP3,
#endif     // HAVE_LIBMP3LAME
    AU_CODEC_MP2
};

#define len_au_codecs_mp2_and_mp3 (sizeof(au_codecs_mp2_and_mp3) / \
    sizeof(XVC_AuCodecID))

#ifdef HAVE_LIBMP3LAME
static const XVC_AuCodecID au_codecs_mp3[] = { AU_CODEC_MP3 };

#define len_au_codecs_mp3 (sizeof(au_codecs_mp3) / \
    sizeof(XVC_AuCodecID))
#endif     // HAVE_LIBMP3LAME

static const XVC_AuCodecID au_codecs_mp2_and_pcm[] = {
    AU_CODEC_MP2,
    AU_CODEC_PCM16
};

#define len_au_codecs_mp2_and_pcm (sizeof(au_codecs_mp2_and_pcm) / \
    sizeof(XVC_AuCodecID))

static const XVC_AuCodecID au_codecs_mp2[] = { AU_CODEC_MP2 };

#define len_au_codecs_mp2 (sizeof(au_codecs_mp2) / \
    sizeof(XVC_AuCodecID))

static const XVC_AuCodecID au_codecs_ac3[] = { AU_CODEC_AC3 };

#define len_au_codecs_ac3 (sizeof(au_codecs_ac3) / \
    sizeof(XVC_AuCodecID))

#endif     // HAVE_FFMPEG_AUDIO
#endif     // USE_FFMPEG

/**
 * \brief global array storing all available file formats
 */
const XVC_FFormat xvc_formats[NUMCAPS] = {
    {
     "NONE",
     N_("NONE"),
     NULL,
     CODEC_NONE,
     NULL,
     0,
     AU_CODEC_NONE,
     NULL,
     0,
     NULL,
     0},
    {
     "xwd",
     N_("X11 Window Dump"),
     NULL,
     CODEC_NONE,
     NULL,
     0,
     AU_CODEC_NONE,
     NULL,
     0,
     extension_xwd,
     len_extension_xwd}
#ifdef USE_FFMPEG
    , {
       "pgm",
       N_("Portable Graymap File"),
       NULL,
       CODEC_PGM,
       allowed_vid_codecs_pgm,
       len_allowed_vid_codecs_pgm,
       AU_CODEC_NONE,
       NULL,
       0,
       extension_pgm,
       len_extension_pgm},
    {
     "ppm",
     N_("Portable Anymap File"),
     NULL,
     CODEC_PPM,
     allowed_vid_codecs_ppm,
     len_allowed_vid_codecs_ppm,
     AU_CODEC_NONE,
     NULL,
     0,
     extension_ppm,
     len_extension_ppm},
    {
     "png",
     N_("Portable Network Graphics File"),
     NULL,
     CODEC_PNG,
     allowed_vid_codecs_png,
     len_allowed_vid_codecs_png,
     AU_CODEC_NONE,
     NULL,
     0,
     extension_png,
     len_extension_png},
    {
     "jpg",
     N_("Joint Picture Expert Group"),
     NULL,
     CODEC_JPEG,
     allowed_vid_codecs_jpg,
     len_allowed_vid_codecs_jpg,
     AU_CODEC_NONE,
     NULL,
     0,
     extension_jpg,
     len_extension_jpg},
    {
     "avi",
     N_("Microsoft Audio Video Interleaved File"),
     "avi",
     CODEC_MSDIV2,
     allowed_vid_codecs_avi,
     len_allowed_vid_codecs_avi,
#ifdef HAVE_FFMPEG_AUDIO
#ifdef HAVE_LIBMP3LAME
     AU_CODEC_MP3,
#else      // HAVE_LIBMP3LAME
     AU_CODEC_MP2,
#endif     // HAVE_LIBMP3LAME
     allowed_au_codecs_avi,
     len_allowed_au_codecs_avi,
#else      // HAVE_FFMPEG_AUDIO
     AU_CODEC_NONE,
     NULL,
     0,
#endif     // HAVE_FFMPEG_AUDIO
     extension_avi,
     len_extension_avi},
    {
     "divx",
     N_("General AVI file (DIVX default)"),
     "avi",
     CODEC_MPEG4,
     allowed_vid_codecs_avi,
     len_allowed_vid_codecs_avi,
#ifdef HAVE_FFMPEG_AUDIO
#ifdef HAVE_LIBMP3LAME
     AU_CODEC_MP3,
#else      // HAVE_LIBMP3LAME
     AU_CODEC_MP2,
#endif     // HAVE_LIBMP3LAME
     allowed_au_codecs_avi,
     len_allowed_au_codecs_avi,
#else      // HAVE_FFMPEG_AUDIO
     AU_CODEC_NONE,
     NULL,
     0,
#endif     // HAVE_FFMPEG_AUDIO
     extension_mpg,
     len_extension_mpg},
    {
     "asf",
     N_("Microsoft Advanced Streaming Format"),
     "asf",
     CODEC_MSDIV3,
     allowed_vid_codecs_asf,
     len_allowed_vid_codecs_asf,
     AU_CODEC_NONE,
#ifdef HAVE_FFMPEG_AUDIO
     au_codecs_mp2_and_mp3,
     len_au_codecs_mp2_and_mp3,
#else      // HAVE_FFMPEG_AUDIO
     NULL,
     0,
#endif     // HAVE_FFMPEG_AUDIO
     extension_asf,
     len_extension_asf},
    {
     "flv1",
     N_("Macromedia Flash Video Stream"),
     "flv",
     CODEC_FLV,
     allowed_vid_codecs_flv,
     len_allowed_vid_codecs_flv,
#ifdef HAVE_FFMPEG_AUDIO
#ifdef HAVE_LIBMP3LAME
     AU_CODEC_MP3,
     au_codecs_mp3,
     len_au_codecs_mp3,
#else      // HAVE_LIBMP3LAME
     AU_CODEC_NONE,
     NULL,
     0,
#endif     // HAVE_LIBMP3LAME
#else      // HAVE_FFMPEG_AUDIO
     AU_CODEC_NONE,
     NULL,
     0,
#endif     // HAVE_FFMPEG_AUDIO
     extension_flv,
     len_extension_flv},
    {
     "swf",
     N_("Macromedia Shockwave Flash File"),
     "swf",
     CODEC_FLV,
     allowed_vid_codecs_swf,
     len_allowed_vid_codecs_swf,
#ifdef HAVE_FFMPEG_AUDIO
#ifdef HAVE_LIBMP3LAME
     AU_CODEC_MP3,
     au_codecs_mp3,
     len_au_codecs_mp3,
#else      // HAVE_LIBMP3LAME
     AU_CODEC_NONE,
     NULL,
     0,
#endif     // HAVE_LIBMP3LAME
#else      // HAVE_FFMPEG_AUDIO
     AU_CODEC_NONE,
     NULL,
     0,
#endif     // HAVE_FFMPEG_AUDIO
     extension_swf,
     len_extension_swf},
    {
     "dv",
     N_("DV Video Format"),
     "dv",
     CODEC_DV,
     allowed_vid_codecs_dv,
     len_allowed_vid_codecs_dv,
#ifdef HAVE_FFMPEG_AUDIO
     AU_CODEC_MP2,
     au_codecs_mp2_and_pcm,
     len_au_codecs_mp2_and_pcm,
#else      // HAVE_FFMPEG_AUDIO
     AU_CODEC_NONE,
     NULL,
     0,
#endif     // HAVE_FFMPEG_AUDIO
     extension_dv,
     len_extension_dv},
    {
     "mpeg",
     N_("MPEG1 System Format (VCD)"),
     "mpeg",
     CODEC_MPEG1,
     allowed_vid_codecs_mpeg1,
     len_allowed_vid_codecs_mpeg1,
#ifdef HAVE_FFMPEG_AUDIO
     AU_CODEC_MP2,
     au_codecs_mp2,
     len_au_codecs_mp2,
#else
     AU_CODEC_NONE,
     NULL,
     0,
#endif     // HAVE_FFMPEG_AUDIO
     extension_m1v,
     len_extension_m1v},
    {
     "mpeg2",
     N_("MPEG2 PS Format (SVCD)"),
     "svcd",
     CODEC_MPEG2,
     allowed_vid_codecs_mpeg2,
     len_allowed_vid_codecs_mpeg2,
#ifdef HAVE_FFMPEG_AUDIO
     AU_CODEC_MP2,
     au_codecs_mp2,
     len_au_codecs_mp2,
#else
     AU_CODEC_NONE,
     NULL,
     0,
#endif     // HAVE_FFMPEG_AUDIO
     extension_m2v,
     len_extension_m2v},
    {
     "vob",
     N_("MPEG2 PS format (DVD VOB)"),
     "dvd",
     CODEC_MPEG2,
     allowed_vid_codecs_mpeg2,
     len_allowed_vid_codecs_mpeg2,
#ifdef HAVE_FFMPEG_AUDIO
     AU_CODEC_AC3,
     au_codecs_ac3,
     len_au_codecs_ac3,
#else
     AU_CODEC_NONE,
     NULL,
     0,
#endif     // HAVE_FFMPEG_AUDIO
     extension_dvd,
     len_extension_dvd},
    {
     "mov",
     N_("Quicktime Format"),
     "mov",
     CODEC_MPEG4,
     allowed_vid_codecs_mov,
     len_allowed_vid_codecs_mov,
#ifdef HAVE_FFMPEG_AUDIO
     AU_CODEC_MP2,
     allowed_au_codecs_avi,
     len_allowed_au_codecs_avi,
#else      // HAVE_FFMPEG_AUDIO
     AU_CODEC_NONE,
     NULL,
     0,
#endif     // HAVE_FFMPEG_AUDIO
     extension_mov,
     len_extension_mov}
#endif     // USE_FFMPEG
};

/**
 * \brief finds libavcodec's codec id from xvidcap's
 *
 * @param xv_codec xvidcap's codec id
 * @return an integer codec id as understood by libavcodec. Since 0 is a valid
 *      codec id we return -1 on failure
 */
int
xvc_trans_codec (XVC_CodecID xv_codec)
{
    int ret = -1;

    if (xv_codec > 0 && xv_codec < NUMCODECS)
        ret = xvc_codecs[xv_codec].ffmpeg_id;

    return ret;
}

/**
 * \brief finds out if a codec is in the an array of valid video codec ids
 *      for a given format
 *
 * @param format the id of the format to check
 * @param codec the codec id to check for
 * @return 0 for not found, otherwise the position in the array where codec
 *      was found started at 1 (i. e. normal index + 1)
 */
int
xvc_is_valid_video_codec (XVC_FFormatID format, XVC_CodecID codec)
{
    int i = 0, found = 0;

    if (format < 0 || format >= NUMCAPS ||
        xvc_formats[format].num_allowed_vid_codecs == 0)
        return found;
    for (i = 0; i < xvc_formats[format].num_allowed_vid_codecs; i++) {
        if (xvc_formats[format].allowed_vid_codecs[i] == codec) {
            found = ++i;
            return found;
        }
    }
    return found;
}

/**
 * \brief finds out if an audio codec is in the an array of valid audio codec
 *      ids for a given format
 *
 * @param format the id of the format to check
 * @param codec the audio codec id to check for
 * @return 0 for not found, otherwise the position in the array where codec
 *      was found started at 1 (i. e. normal index + 1)
 */
int
xvc_is_valid_audio_codec (XVC_FFormatID format, XVC_AuCodecID codec)
{
    int i = 0;
    int found = 0;

    if (format < 0 || format >= NUMCAPS ||
        xvc_formats[format].num_allowed_au_codecs == 0)
        return found;

    for (i = 0; i < xvc_formats[format].num_allowed_au_codecs; i++) {
        if (xvc_formats[format].allowed_au_codecs[i] == codec) {
            found = ++i;
            return found;
        }
    }
    return found;
}

/**
 * \brief find target file format based on filename, i. e. the extension
 *
 * @param file a filename to test
 * @return the index number pointing to the array element for the format found
 *      in the global file xvc_formats array
 */
XVC_FFormatID
xvc_codec_get_target_from_filename (const char *file)
{
    char *ext = NULL;
    int ret = 0, i, i2;

    ext = rindex (file, '.');
    if (ext == NULL) {
        return ret;
    }
    ext++;

    for (i = 0; i < NUMCAPS; i++) {
        if (xvc_formats[i].extensions) {
            for (i2 = 0; i2 < xvc_formats[i].num_extensions; i2++) {
                if (strcasecmp (ext, xvc_formats[i].extensions[i2]) == 0) {
                    // then we have found the right extension in target n
                    ret = i;
                    return ret;
                }
            }
        }
    }

    return ret;
}

/**
 * \brief checks if fps rate is valid for given codec
 *
 * @param fps the fps rate to test
 * @param codec the codec to test the fps rate against specified as an index
 *      number pointing to a codec in the global codecs array
 * @param exact if 1 the function does an exact check, if 0 it will accept
 *      an fps value as valid, if there is a valid fps value that is
 *      not more than 0.01 larger or smaller than the fps given
 * @return 1 for fps is valid for given codec or 0 for not valid
 */
int
xvc_codec_is_valid_fps (XVC_Fps fps, XVC_CodecID codec, int exact)
{
#define DEBUGFUNCTION "xvc_codec_is_valid_fps()"
    int found = 0;

    if (xvc_codecs[codec].num_allowed_fps == 0 &&
        xvc_codecs[codec].num_allowed_fps_ranges == 0)
        return found;

    if (xvc_get_index_of_fps_array_element (xvc_codecs[codec].num_allowed_fps,
                                            xvc_codecs[codec].allowed_fps,
                                            fps, exact) >= 0) {
        found = 1;
        return found;
    }

    if (xvc_codecs[codec].num_allowed_fps_ranges != 0) {
        int i = 0;

        for (i = 0; i < xvc_codecs[codec].num_allowed_fps_ranges; i++) {
            XVC_FpsRange curr = xvc_codecs[codec].allowed_fps_ranges[i];

            if (XVC_FPS_GTE (fps, curr.start) && XVC_FPS_LTE (fps, curr.end)) {
                found = 1;
                return found;
            }
        }
    }

    return found;
#undef DEBUGFUNCTION
}

/**
 * \brief gets the index number of a given fps value from an array of fps
 *      values
 *
 * @param size the size of the array of fps values
 * @param haystack array of XVC_Fps values
 * @param needle the fps to search for
 * @param exact if 1 the function looks for an exact match, if 0 it will accept
 *      an fps value as matching, if there is a valid fps value that is
 *      not more than 0.01 larger or smaller than the fps given
 * @return the index number of the fps value in the array or -1 if not found
 */
int
xvc_get_index_of_fps_array_element (int size,
                                    const XVC_Fps * haystack,
                                    XVC_Fps needle, int exact)
{
    int i, found = -1;

    for (i = 0; i < size; i++) {
        if (XVC_FPS_EQUAL (haystack[i], needle)) {
            found = i;
            return found;
        }
    }

    if (!exact) {
        for (i = 0; i < size; i++) {
            double n, h;

            h = (double) haystack[i].num / (double) haystack[i].den;
            n = (double) needle.num / (double) needle.den;
            if (fabs (n - h) < 0.01) {
                found = i;
                return found;
            }
        }
    }
    return found;
}

/**
 * \brief reads a string and returns an fps value
 *
 * This accepts two forms of specifying an fps value: Either as a floating
 * point number with locale specific decimal point, or as a fraction like
 * "int/int"
 *
 * @param fps_string a string representation of an fps value
 * @return the fps value read from the string or { 0, 1}
 */
XVC_Fps
xvc_read_fps_from_string (const char *fps_string)
{
    struct lconv *loc = localeconv ();
    XVC_Fps fps = { 0, 1 };

    if ((index (fps_string, '/') &&
         !strstr (fps_string, loc->decimal_point)) ||
        (index (fps_string, '/') && strstr (fps_string, loc->decimal_point) &&
         index (fps_string, '/') < strstr (fps_string, loc->decimal_point))
        ) {
        char *slash, *endptr;

        fps.num = strtol (fps_string, &endptr, 10);
        slash = index (fps_string, '/');
        slash++;
        fps.den = strtol (slash, &endptr, 10);
    } else {
        char *endptr = NULL;

        fps.num = (int) (strtod (fps_string, &endptr) * pow (10,
                                                             xvc_get_number_of_fraction_digits_from_float_string
                                                             (fps_string)) +
                         0.5);
        fps.den =
            (int) (pow
                   (10,
                    xvc_get_number_of_fraction_digits_from_float_string
                    (fps_string)) + 0.5);
    }
    return fps;
}
