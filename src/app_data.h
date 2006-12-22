/** 
 * \file app_data.h,
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
#include <X11/Intrinsic.h>

/* 
 * some flags to toggle on/off options
 */
#define FLG_NONE                0
/** \brief run xvidcap with verbose output */
#define FLG_RUN_VERBOSE         1
#ifdef HAVE_SHMAT
/** \brief use shared memory access to X11 */
#define FLG_USE_SHM             2
#endif     // HAVE_SHMAT
/** \brief use dga for capturing 
 * 
 * @note this is not at all used atm.
 */
#define FLG_USE_DGA             4
/** \brief use video for linux for capturing 
 * 
 * @note this is not at all used atm.
 */
#define FLG_USE_V4L             8

#ifdef HAVE_SHMAT
/** \brief shorthand for the sum of source flags */
#define FLG_SOURCE (FLG_USE_DGA | FLG_USE_SHM | FLG_USE_V4L)
#else
/** \brief shorthand for the sum of source flags */
#define FLG_SOURCE (FLG_USE_DGA | FLG_USE_V4L)
#endif     // HAVE_SHMAT
/** \brief should we record sound
 * 
 * @note this is used inside the job to be able to unset audio capture if
 *      the user wants it in the AppData struct, but audio capture is not
 *      possible
 */
#define FLG_REC_SOUND           16
/** \brief run without GUI */
#define FLG_NOGUI               32
/** \brief enable auto-continue feature */
#define FLG_AUTO_CONTINUE       64
/** \brief save the capture geometry to the preferences file */
#define FLG_SAVE_GEOMETRY       128
/** \brief use video for linux for capturing 
 * 
 * @note this is not at all used atm.
 */
#define FLG_SHOW_TIME           256
/** \brief always show the results dialog */
#define FLG_ALWAYS_SHOW_RESULTS 512
/** \brief use the xfixes extension, i. e. capture the real mouse pointer */
#define FLG_USE_XFIXES          1024

/** \brief This structure contains the settings for one of the two capture
 *      modes (single-frame vs. multi-frame).
 */
typedef struct
{
    /** \brief file name pattern */
    char *file;
    /** \brief target file format, 0 means autodetect */
    int target; // 
    /** \brief target codec, 0 means autodetect */
    int targetCodec;
    /** \brief frames per second */
    int fps;
    /** \brief maximum time in seconds to record */
    int time;
    /** \brief maximum frames to record */
    int frames;
    /** \brief frame number to start with */
    int start_no;
    /** \brief frame increment */
    int step;
    /** \brief quality setting */
    int quality;
#ifdef HAVE_FFMPEG_AUDIO
    /** \brief target codec for audio encoding, 0 means autodetect */
    int au_targetCodec;
    /** \brief audio wanted */
    int audioWanted;
    /** \brief sound sample rate */
    int sndrate;
    /** \brief bits to sample for audio capture */
    int sndsize;
    /** \brief number of channels to record */
    int sndchannels;
#endif     // HAVE_FFMPEG_AUDIO
    /** \brief command to use for animate function */
    char *play_cmd;
    /** \brief command to use for make video function */
    char *video_cmd;
    /** \brief command to use for edit function */
    char *edit_cmd;
} CapTypeOptions;

/** \brief This structure has all the user options passed to the application.
 *      It contains two CapTypeOptions structs to keep the settings for the two
 *      capture modes (single-frame vs. multi-frame) in parallel.
 */
typedef struct
{
    /** \brief level of verbosity */
    int verbose;
    /** \brief general flags 
     *
     * @see FLG_NONE
     */
    int flags;
    /** \brief rescale to percentage */
    int rescale;
    /** \brief capture mouse pointer: 0 none, 1 white , 2 black. When Xfixes
     *      is used to capture the real mouse pointer only 0 or != 0 is
     *      relevant.
     */
    int mouseWanted;
    /** \brief video capture source */
    char *source;
#ifdef HAVE_FFMPEG_AUDIO
    /** \brief audio capture source */
    char *snddev;
#endif     // HAVE_FFMPEG_AUDIO
#ifdef HasVideo4Linux
    /** \brief v4l device to capture from */
    char *device;
#endif     // HasVideo4Linux
    /** \brief window attributes for area to capture. 
     *
     * This is mostly relevant for captures on 8-bit pseudo-color displays with
     * potential for colormap flashing. In such cases selecting a single window for
     * capture (through the select toggle and a single-click on a window or the
     * --window parameter) will retrieve the correct colormap for that window.
     * Also the geometry will be retrieved in that way.
     */
    XWindowAttributes win_attr;
    /** \brief store the display here, so we don't open and close it everywhere
     *      we need it */
    Display *dpy;
    /** \brief the area to capture */
    XRectangle *area;

    /** \brief the default capture mode (if detection fails). 0 = single-frame
     *      1 = multi-frame */
    int default_mode;
    /** \brief the current capture mode, values as with default_mode */
    int current_mode;
    /** \brief options for single-frame capture mode */
    CapTypeOptions single_frame;
    /** \brief options for multi-frame capture mode */
    CapTypeOptions multi_frame;
} AppData;

/*
 * error related constants
 */
/** \brief max number of errors to initialize error array */
#define NUMERRORS               50
/** \brief error severity fatal (default action calls exit) */
#define XV_ERR_FATAL            1
/** \brief error severity error (default action is a major change to input) */
#define XV_ERR_ERROR            2
/** \brief error severity warning (default action is a minor change to input)*/
#define XV_ERR_WARN             3
/** \brief error severity info (smth's strange but does NOT require a change) 
 *
 * In other words: an INFO MUST NOT NEED TO CHANGE ANYTHING! xvidcap will 
 * continue when there are only INFO messages left.
 */
#define XV_ERR_INFO             4

/** \brief all information about a given error related to an inconsistency
 *      regarding user preferences is contained here.
 */
struct _xvErrorListItem;
typedef struct _xvError
{
    /** \brief error code */
    int code;
    /** \brief one of the error types 
     *
     * @see XV_ERR_FATAL
     */
    int type;
    /** \brief short description (title?) */
    char *short_msg;
    /** \brief long description in full sentences */
    char *long_msg;
    /** \brief default action */
    void (*action) (struct _xvErrorListItem *);
    /** \brief describes what the default action does 
     *
     * formulate like you're completing the sentence: to resolve this I will ...
     */
    char *action_msg;
} xvError;

/** \brief wraps an xv_Error in an xvErrorListItem to create a double-linked
 *      list of errors.
 */
typedef struct _xvErrorListItem
{
    /** \brief pointer to actual error */
    xvError *err;
    /** \brief pointer to app_data that threw error */
    AppData *app;
    /** \brief previous element in the double-linked list */
    struct _xvErrorListItem *previous;  // 
    /** \brief next element in the double-linked list */
    struct _xvErrorListItem *next;
} xvErrorListItem;

/*
 * Functions from app_data.c
 */
void xvc_app_data_init (AppData * lapp);
void xvc_app_data_set_defaults (AppData * lapp);
void xvc_app_data_copy (AppData * tapp, AppData * sapp);
void xvc_merge_cap_type_and_app_data (CapTypeOptions * cto, AppData * lapp);
xvErrorListItem *xvc_app_data_validate (AppData * lapp, int mode, int *rc);
void xvc_app_data_set_window_attributes (Window win);

void xvc_cap_type_options_copy (CapTypeOptions * topts, CapTypeOptions * sopts);
void xvc_cap_type_options_init (CapTypeOptions * cto);

void xvc_errors_init ();
xvErrorListItem *xvc_errors_delete_list (xvErrorListItem * err);
void xvc_errors_write_error_msg (int code, int print_action_or_not);

Boolean xvc_is_filename_mutable (char *);
int xvc_get_int_from_float_string (char *input);
void xvc_command_execute (char *command, int flag, int number,
                          char *file, int fframe, int lframe,
                          int width, int height, int fps);

#endif     // __APP_DATA_H__
