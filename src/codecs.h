/* 
 * target codecs for ffmpeg
 */

#ifndef __CODECS_H__
#define __CODECS_H__

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* 
 * codecs used by xvidcap 
 */
enum tCodecIDs {
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
    CODEC_FLV,
    CODEC_DV,
    CODEC_MPEG2,
    CODEC_SVQ1,
#endif                          // USE_FFMPEG
    NUMCODECS
};
#ifdef USE_FFMPEG
#define CODEC_MF CODEC_MPEG1
#endif                          // USE_FFMPEG

typedef struct _xvCodec {
    char *name;
    char *longname;
    int id;
    int ffmpeg_id;
    int def_br;
    int def_fps;
    char *allowed_fps_ranges;
    char *allowed_fps;
} xvCodec;

/* 
 * audio codecs
 */
enum tAuCodecIDs {
    AU_CODEC_NONE,
#ifdef HAVE_FFMPEG_AUDIO
    AU_CODEC_MP2,
#ifdef HAVE_LIBMP3LAME
    AU_CODEC_MP3,
#endif                          // HAVE_LIBMP3LAME
    AU_CODEC_PCM16,
#endif                          // HAVE_FFMPEG_AUDIO
    NUMAUCODECS
};

typedef struct _xvAuCodec {
    char *name;
    char *longname;
    int id;
    int ffmpeg_id;
} xvAuCodec;

/* 
 * file formats
 */
/* 
 * capture output formats
 *
 */
enum tcap_formats {
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
    CAP_MOV,
#endif                          // USE_FFMPEG
    NUMCAPS
};

#ifdef USE_FFMPEG
#define CAP_FFM CAP_PGM
#define CAP_MF CAP_AVI
#endif                          // USE_FFMPEG



typedef struct _xvFFormat {
    char *name;
    char *longname;
    char *ffmpeg_name;
    char *extensions;
    int def_vid_codec;
    char *allowed_vid_codecs;
    int def_au_codec;
    char *allowed_au_codecs;
//    int (*init) (void);
} xvFFormat;



void xvc_codecs_init();
int xvc_trans_codec(int);
int xvc_is_element(char *xvList, char *xvElement);
char *xvc_next_element(char *list);
int xvc_codec_get_target_from_filename(char *file);
int xvc_codec_is_valid_fps(int fps, int codec);
int xvc_num_elements(char *list);


#endif                          // __CODECS_H__
