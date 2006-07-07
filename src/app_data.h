/* 
 * app_data.h,
 *
 * Copyright (C) 1997 Rasca Gmelch, Berlin
 * Copyright (C) 2003-2006 Karl H. Beckers, Frankfurt
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

#ifndef __APP_DATA_H__
#define __APP_DATA_H__

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* 
 * some flags to toggle on/off options
 *
 */
#define FLG_NONE 0
#define FLG_RUN_VERBOSE 1
#define FLG_MULTI_IMAGE 2

#ifdef HAVE_SHMAT
#define FLG_USE_SHM 4
#endif // HAVE_SHMAT

#define FLG_USE_DGA 8
#define FLG_USE_V4L 16

#ifdef HAVE_SHMAT
#define FLG_SOURCE (FLG_USE_DGA | FLG_USE_SHM | FLG_USE_V4L)
#else
#define FLG_SOURCE (FLG_USE_DGA | FLG_USE_V4L)
#endif // HAVE_SHMAT

#define FLG_REC_SOUND 32
// FIXME: do we need the following?
#define FLG_SYNC 64

#define FLG_NOGUI 128
#define FLG_USE_DEFAULT_CODECS 256
#define FLG_AUTO_CONTINUE 512
#define FLG_SAVE_GEOMETRY 1024
#define FLG_SHOW_TIME 2048


/* 
 * moving AppData definition here to make it available to options.c, control.c etc.
 *
 * this structure is used to pass options passed to the application to the job
 * controller
 *
 * now, we want options specific to single-frame capture and those to multi-frame
 * capture (a.k.a on-the-fly encoding) separate
 */
typedef struct {
    char *file;                 // file pattern
    int target;                 // target file format
    int targetCodec;            // for video encoding
    int fps;                    // frames per second
    int time;                   // time in seconds to record
    int frames;                 // max frames to record
    int start_no;               // frame number to start with
    int step;                   // frame increment
    int quality;                // quality setting
    int bpp;                    // bits per pixel
#ifdef HAVE_FFMPEG_AUDIO
    int au_targetCodec;         // for audio encoding
    int audioWanted;            // audio wanted
    int sndrate;                // sound sample rate
    int sndsize;                // bits to sample for audio capture
    int sndchannels;            // number of channels to record
#endif // HAVE_FFMPEG_AUDIO
    // audio to 
    char *play_cmd;             // command to use for animate function
    char *video_cmd;            // command to use for make video function
    char *edit_cmd;             // command to use for edit function
} CapTypeOptions;

typedef struct {
    int verbose;                // verbose level
    int flags;                  // flags used ... see above
    int cap_width;              // width
    int cap_height;             // height
    int cap_pos_x;              // x position of the capture frame
    int cap_pos_y;              // y position of the capture frame
    int mouseWanted;            // capture mouse pointer: 0 none , 
    // 1 white , 2 black
    char *source;               // video capture source
#ifdef HAVE_FFMPEG_AUDIO
    char *snddev;               // audio capture source
#endif // HAVE_FFMPEG_AUDIO
    char *help_cmd;             // command to use for displaying help
    // information
    char *device;               // v4l device to capture from
    int default_mode;           // 0 = single_frame, 1 =
    // multi_frame
    int current_mode;           // dto.
    CapTypeOptions single_frame;
    CapTypeOptions multi_frame;
    // int use_default_codecs; // override targetCodec values from
    // preferences file
    // with defaults for target file type
} AppData;


// error related

#define NUMERRORS 50            // max number of errors to
// initialize error array
#define XV_ERR_FATAL 1          // fatal error (default
// action calls exit)
#define XV_ERR_ERROR 2          // error - default action
// is a major change to
// input
#define XV_ERR_WARN 3           // warning - default
// action is a minor
// change to input
#define XV_ERR_INFO 4           // info - smth's strange
// but does NOT require a
// change
// in other words: an INFO 
// MUST NOT NEED TO CHANGE 
// ANYTHING
// the app will continue
// when there are only
// INFO messages left


typedef struct _xvError {
    int code;                   // error code
    int type;                   // one of the error types XV_ERR_*
    char *short_msg;            // short description (title?)
    char *long_msg;             // long description in full sentences
    // ending in a '.' (or smth.)
    void (*action) (void *);    // default action 
    char *action_msg;           // describes what the default action does
    // formulate like you're completing the sentence: The default action
    // is to .....
} xvError;

typedef struct _xvErrorListItem {
    xvError *err;               // pointer to actual error
    AppData *app;               // pointer to app_data that threw error
    struct _xvErrorListItem *previous;  // previous in dbl-linked-list
    struct _xvErrorListItem *next;  // next in dbl-linked-list
} xvErrorListItem;


#include <X11/Intrinsic.h>

// Functions from app_data.c
int xvc_merge_cap_type_and_app_data(CapTypeOptions * cto, AppData * lapp);

xvErrorListItem *xvc_app_data_validate(AppData * lapp, int mode, int *rc);
void xvc_app_data_init(AppData * lapp);
void xvc_app_data_set_defaults(AppData * lapp);
void xvc_app_data_copy(AppData * tapp, AppData * sapp);

void xvc_cap_type_options_copy(CapTypeOptions * topts,
                               CapTypeOptions * sopts);
void xvc_cap_type_options_init(CapTypeOptions * cto);


void xvc_errors_init();
xvErrorListItem *xvc_errors_append(int code, xvErrorListItem * err,
                                   AppData * app);
void xvc_errors_write_action_msg(int code);
xvErrorListItem *xvc_errors_delete_list(xvErrorListItem * err);
void xvc_errors_write_error_msg(int code, int print_action_or_not);


Boolean xvc_is_filename_mutable(char *);
int xvc_get_int_from_float_string(char *input);
void xvc_command_execute(char *command, int flag, int number,
                         char *file, int fframe, int lframe,
                         int width, int height, int fps, int time);


#endif                          // __APP_DATA_H__
