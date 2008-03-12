/**
 * \file codecs.h
 *
 * includes and defines for video -, audio codecs, and file formats
 */

/*
 * Copyright (C) 2003-2007 Karl H. Beckers, Frankfurt
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

#ifndef _xvc_CODECS_H__
#define _xvc_CODECS_H__

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#endif     // DOXYGEN_SHOULD_SKIP_THIS

/**
 * \brief ffmpeg does fractional fps handling through AVRational structs
 *      which look exactly like this. Because we cannot otherwise hope to
 *      exactly match ffmpeg framerates with a float of our own, we mimic
 *      the AVRational struct ... and copy it because we might build without
 *      ffmpeg
 */
typedef struct _xvc_Fps
{
    int num;
    int den;
} XVC_Fps;

/**
 * \brief some codecs support ranges of framerates. The GUI needs to know
 *      about those, so we wrap the XVC_Fps struct in XVC_FpsRange structs
 *      with a defined start and end
 */
typedef struct _xvc_FpsRange
{
    XVC_Fps start;
    XVC_Fps end;
} XVC_FpsRange;

/**
 * \brief codec ID's used by xvidcap. Because these are in the same order as
 *      the elements of the global codecs array, the elements of the array
 *      can be referenced using these telling names.
 */
typedef enum
{
    CODEC_NONE,
#ifdef USE_FFMPEG
    CODEC_PGM,
    CODEC_PPM,
    CODEC_PNG,
    CODEC_JPEG,
    CODEC_MPEG1,
    CODEC_MJPEG,
    CODEC_MPEG4,
    CODEC_MSDIV2,
    CODEC_MSDIV3,
    CODEC_FFV1,
    CODEC_FLV,
    CODEC_FLASHSV,
    CODEC_DV,
    CODEC_MPEG2,
#ifdef HAVE_LIBTHEORA
    CODEC_THEORA,
#endif     // HAVE_LIBTHEORA
    CODEC_SVQ1,
#endif     // USE_FFMPEG
    NUMCODECS
} XVC_CodecID;

#ifdef USE_FFMPEG
/** \brief CODEC_MF is the first codec for multi-frame capture */
#define CODEC_MF CODEC_MPEG1
#else      // USE_FFMPEG
#define CODEC_ID_NONE CODEC_NONE
#endif     // USE_FFMPEG

/** \brief struct containing codec properties */
typedef struct _xvc_Codec
{
    const char *name;
    const char *longname;
    const int ffmpeg_id;
    const XVC_Fps def_fps;
    const XVC_FpsRange *allowed_fps_ranges;
    const int num_allowed_fps_ranges;
    const XVC_Fps *allowed_fps;
    const int num_allowed_fps;
} XVC_Codec;

extern const XVC_Codec xvc_codecs[NUMCODECS];

/**
 * \brief audio codec ID's used by xvidcap. Because these are in the same
 *      order as the elements of the global xvc_audio_codecs array, the
 *      elements of the array can be referenced using these telling names.
 */
typedef enum
{
    AU_CODEC_NONE,
#ifdef HAVE_FFMPEG_AUDIO
    AU_CODEC_MP2,
#ifdef HAVE_LIBMP3LAME
    AU_CODEC_MP3,
#endif     // HAVE_LIBMP3LAME
    AU_CODEC_VORBIS,
    AU_CODEC_AC3,
    AU_CODEC_PCM16,
#endif     // HAVE_FFMPEG_AUDIO
    NUMAUCODECS
} XVC_AuCodecID;

/** \brief struct containing audio codec properties */
typedef struct _xvc_AuCodec
{
    const char *name;
    const char *longname;
    const int ffmpeg_id;
} XVC_AuCodec;

extern const XVC_AuCodec xvc_audio_codecs[NUMAUCODECS];

/**
 * \brief file format ID's used by xvidcap. Because these are in the same
 *      order as the elements of the global xvc_formats array, the elements of
 *      the array can be referenced using these telling names.
 */
typedef enum
{
    CAP_NONE,
    CAP_XWD,
#ifdef USE_FFMPEG
    CAP_PGM,
    CAP_PPM,
    CAP_PNG,
    CAP_JPG,
    CAP_AVI,
    CAP_DIVX,
    CAP_ASF,
    CAP_FLV,
    CAP_SWF,
    CAP_DV,
    CAP_MPG,
    CAP_SVCD,
    CAP_DVD,
    CAP_MOV,
#endif     // USE_FFMPEG
    NUMCAPS
} XVC_FFormatID;

#ifdef USE_FFMPEG
/** \brief CAP_FFM is the first file format using libav* */
#define CAP_FFM CAP_PGM
/** \brief CAP_MF is the first file format for multi-frame capture */
#define CAP_MF CAP_AVI
#endif     // USE_FFMPEG

/** \brief struct containing file format properties */
typedef struct _xvc_FFormat
{
    const char *name;
    const char *longname;
    const char *ffmpeg_name;
    const XVC_CodecID def_vid_codec;
    const XVC_CodecID *allowed_vid_codecs;
    const int num_allowed_vid_codecs;
    const XVC_AuCodecID def_au_codec;
    const XVC_AuCodecID *allowed_au_codecs;
    const int num_allowed_au_codecs;
    const char **extensions;
    const int num_extensions;
} XVC_FFormat;

extern const XVC_FFormat xvc_formats[NUMCAPS];

/**
 * \brief this is a convenience function for getting an array's length
 *
 * Remember that this uses sizeof and sizeof can only be used on dynamic
 * arrays in the file where the array is defined.
 */
#define XVC_ARRAY_LENGTH(a) ( a == NULL ? 0 : sizeof(a) / sizeof(a[0]) )
/** \brief compare two XVC_Fps values for equality */
#define XVC_FPS_EQUAL(a, b) ((float) a.num / (float) a.den == \
(float) b.num / b.den? 1 : 0 )
/** \brief compare two XVC_Fps values for a < b */
#define XVC_FPS_LT(a, b) ((float) a.num / (float) a.den < \
(float) b.num / b.den? 1 : 0 )
/** \brief compare two XVC_Fps values for a > b */
#define XVC_FPS_GT(a, b) ((float) a.num / (float) a.den > \
(float) b.num / b.den? 1 : 0 )
/** \brief compare two XVC_Fps values for a <= b */
#define XVC_FPS_LTE(a, b) ((float) a.num / (float) a.den <= \
(float) b.num / b.den? 1 : 0 )
/** \brief compare two XVC_Fps values for a >= b */
#define XVC_FPS_GTE(a, b) ((float) a.num / (float) a.den >= \
(float) b.num / b.den? 1 : 0 )
/** \brief compare two XVC_Fps values for a == 0 */
#define XVC_FPS_GT_ZERO(a) ((float) a.num / (float) a.den > 0 ? 1 : 0 )
/** \brief compare two XVC_Fps values for a >= 0 */
#define XVC_FPS_GTE_ZERO(a) ((float) a.num / (float) a.den >= 0 ? 1 : 0 )

int xvc_trans_codec (XVC_CodecID xv_codec);
int xvc_is_valid_video_codec (XVC_FFormatID format, XVC_CodecID codec);
int xvc_is_valid_audio_codec (XVC_FFormatID format, XVC_AuCodecID codec);
XVC_FFormatID xvc_codec_get_target_from_filename (const char *file);
int xvc_codec_is_valid_fps (XVC_Fps fps, XVC_CodecID codec, int exact);
int xvc_get_index_of_fps_array_element (int size,
                                        const XVC_Fps * haystack,
                                        XVC_Fps needle, int exact);
XVC_Fps xvc_read_fps_from_string (const char *fps_string);

#endif     // _xvc_CODECS_H__
