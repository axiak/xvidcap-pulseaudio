/**
 * \file app_data.h
 */

/*
 * Copyright (C) 1997 Rasca Gmelch, Berlin
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

#ifndef _xvc_APP_DATA_H__
#define _xvc_APP_DATA_H__

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/Intrinsic.h>
#include "codecs.h"
#ifdef USE_DBUS
#include "dbus-server-object.h"
#endif     // USE_DBUS
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#define XVC_MAX(a,b) ((a)>(b)? (a):(b))
#define XVC_MIN(a,b) ((a)<(b)? (a):(b))
#define XVC_GLADE_FILE PACKAGE_DATA_DIR"/xvidcap/glade/gnome-xvidcap.glade"

/*
 * some flags to toggle on/off options
 */
enum XVC_Flags
{
    FLG_NONE = 0,
/** \brief run xvidcap with verbose output */
    FLG_RUN_VERBOSE = 1,
#ifdef HAVE_SHMAT
/** \brief use shared memory access to X11 */
    FLG_USE_SHM = 2,
#endif     // HAVE_SHMAT
/**
 * \brief use dga for capturing
 *
 * @note this is not at all used atm.
 */
    FLG_USE_DGA = 4,
/**
 * \brief use video for linux for capturing
 *
 * @note this is not at all used atm.
 */
    FLG_USE_V4L = 8,

/**
 * \brief should we record sound
 *
 * @note this is used inside the job to be able to unset audio capture if
 *      the user wants it in the XVC_AppData struct, but audio capture is not
 *      possible
 */
    FLG_REC_SOUND = 16,
/** \brief run without GUI */
    FLG_NOGUI = 32,
/** \brief enable auto-continue feature */
    FLG_AUTO_CONTINUE = 64,
/** \brief save the capture geometry to the preferences file */
    FLG_SAVE_GEOMETRY = 128,
/**
 * \brief use video for linux for capturing
 *
 * @note this is not at all used atm.
 */
    FLG_SHOW_TIME = 256,
/** \brief always show the results dialog */
    FLG_ALWAYS_SHOW_RESULTS = 512,
/** \brief use the xfixes extension, i. e. capture the real mouse pointer */
    FLG_USE_XFIXES = 1024,
/** \brief should we capture screen updates as deltas using Xdamage
 * extension */
    FLG_USE_XDAMAGE = 2048,
/** \brief minimize xvidcap to the system tray while recording */
    FLG_TO_TRAY = 4096,
/**
 * \brief this will make the frame follow the mouse when the lock is locked
 *      rather than have it follow the main control window
 */
    FLG_LOCK_FOLLOWS_MOUSE = 8192,
/** \brief run without frame around the capture area */
    FLG_NOFRAME = 16384
};

#ifdef HAVE_SHMAT
/** \brief shorthand for the sum of source flags */
#define FLG_SOURCE (FLG_USE_DGA | FLG_USE_SHM | FLG_USE_V4L)
#else      // HAVE_SHMAT
/** \brief shorthand for the sum of source flags */
#define FLG_SOURCE (FLG_USE_DGA | FLG_USE_V4L)
#endif     // HAVE_SHMAT

/**
 * \brief This structure contains the settings for one of the two capture
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
    XVC_Fps fps;
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
} XVC_CapTypeOptions;

/**
 * \brief This structure has all the user options passed to the application.
 *
 * It contains two XVC_CapTypeOptions structs to keep the settings for the two
 * capture modes (single-frame vs. multi-frame) in parallel.
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
    /**
     * \brief capture mouse pointer: 0 none, 1 white , 2 black.
     *
     * When Xfixes is used to capture the real mouse pointer only 0 or != 0 is
     * relevant.
     */
    int mouseWanted;
    /** \brief video capture source */
    char *source;
    /** \brief controls the use of the XDamage extension for screen capture
     * -1 == auto, 0 == off, 1 == on */
    int use_xdamage;
#ifdef HAVE_FFMPEG_AUDIO
    /** \brief audio capture source */
    char *snddev;
#endif     // HAVE_FFMPEG_AUDIO
#ifdef HasVideo4Linux
    /** \brief v4l device to capture from */
    char *device;
#endif     // HasVideo4Linux
    /**
     * \brief window attributes for area to capture.
     *
     * This is mostly relevant for captures on 8-bit pseudo-color displays with
     * potential for colormap flashing. In such cases selecting a single window for
     * capture (through the select toggle and a single-click on a window or the
     * --window parameter) will retrieve the correct colormap for that window.
     * Also the geometry will be retrieved in that way.
     */
    XWindowAttributes win_attr;
    /**
     * \brief store the display here, so we don't open and close it everywhere
     *      we need it
     */
    Display *dpy;
    /** \brief the default root window */
    Window root_window;
    /** \brief pointer to the default screen */
    Screen *default_screen;
    /** \brief default screen number */
    int default_screen_num;
    /** \brief maximum width of the default screen */
    int max_width;
    /** \brief maximum height of the default screen */
    int max_height;

    /** \brief the area to capture */
    XRectangle *area;
#ifdef USE_XDAMAGE
    /** \brief event base for damage events to be used for delta screenshot
     *      capture */
    int dmg_event_base;
#endif     // USE_XDAMAGE

    /**
     * \brief the default capture mode (if detection fails). 0 = single-frame
     *      1 = multi-frame
     */
    int default_mode;
    /** \brief the current capture mode, values as with default_mode */
    int current_mode;

    // various mutexes for thread synchonization
    /** \brief mutex for pausing/unpausing the recording session
     *		by waiting for the recording_condition_unpaused */
    pthread_mutex_t recording_paused_mutex;
    /** \brief condition for pausing/unpausing a recording effectively */
    pthread_cond_t recording_condition_unpaused;
    /** \brief mutex for synchronizing state or frame changes with
     *		capturing of individual frames */
    pthread_mutex_t capturing_mutex;
    /** \brief is the recording thread running?
     *
     * \todo find out if there's a way to tell that from the tread directly
     */
    int recording_thread_running;

#ifdef USE_XDAMAGE
    /** \brief mutex for the Xdamage support. It governs write access to the
     *      damaged region storage */
    pthread_mutex_t damage_regions_mutex;
#endif     // USE_XDAMAGE

#ifdef USE_DBUS
    XvcServerObject *xso;
#endif     // USE_DBUS

    /** \brief options for single-frame capture mode */
    XVC_CapTypeOptions single_frame;
    /** \brief options for multi-frame capture mode */
    XVC_CapTypeOptions multi_frame;
} XVC_AppData;

/*
 * error related constants
 */
enum XVC_ErrorType
{
/** \brief error severity fatal (default action calls exit) */
    XVC_ERR_FATAL,
/** \brief error severity error (default action is a major change to input) */
    XVC_ERR_ERROR,
/** \brief error severity warning (default action is a minor change to input)*/
    XVC_ERR_WARN,
/**
 * \brief error severity info (smth's strange but does NOT require a change)
 *
 * In other words: an INFO MUST NOT NEED TO CHANGE ANYTHING! xvidcap will
 * continue when there are only INFO messages left.
 */
    XVC_ERR_INFO
};

struct _xvc_ErrorListItem;

/**
 * \brief all information about a given error related to an inconsistency
 *      regarding user preferences is contained here.
 */
typedef struct _xvc_Error
{
    /** \brief error code */
    const enum XVC_ErrorType code;
    /**
     * \brief one of the error types
     *
     * @see XVC_ERR_FATAL
     */
    const int type;
    /** \brief short description (title?) */
    const char *short_msg;
    /** \brief long description in full sentences */
    const char *long_msg;
    /** \brief default action */
    void (*action) (struct _xvc_ErrorListItem *);
    /**
     * \brief describes what the default action does
     *
     * formulate like you're completing the sentence: to resolve this I will ...
     */
    const char *action_msg;
} XVC_Error;

#ifdef USE_FFMPEG
#ifdef HAVE_FFMPEG_AUDIO
#define NUMERRORS              43
#else      // HAVE_FFMPEG_AUDIO
#define NUMERRORS              36
#endif     // HAVE_FFMPEG_AUDIO
#else      // USE_FFMPEG
#define NUMERRORS              34
#endif     // USE_FFMPEG

extern const XVC_Error xvc_errors[NUMERRORS];

/**
 * \brief wraps an xv_Error in an XVC_ErrorListItem to create a double-linked
 *      list of errors.
 */
typedef struct _xvc_ErrorListItem
{
    /** \brief pointer to actual error */
    const struct _xvc_Error *err;
    /** \brief pointer to app_data that threw error */
    XVC_AppData *app;
    /** \brief previous element in the double-linked list */
    struct _xvc_ErrorListItem *previous;
    /** \brief next element in the double-linked list */
    struct _xvc_ErrorListItem *next;
} XVC_ErrorListItem;

/*
 * Functions from app_data.c
 */
void xvc_appdata_free (XVC_AppData * lapp);
XVC_AppData *xvc_appdata_ptr (void);
void xvc_appdata_init (XVC_AppData * lapp);
void xvc_appdata_set_defaults (XVC_AppData * lapp);
void xvc_appdata_copy (XVC_AppData * tapp, const XVC_AppData * sapp);
void xvc_appdata_merge_captypeoptions (XVC_CapTypeOptions * cto,
                                       XVC_AppData * lapp);
XVC_ErrorListItem *xvc_appdata_validate (XVC_AppData * lapp, int mode, int *rc);
void xvc_appdata_set_window_attributes (Window win);

void xvc_captypeoptions_copy (XVC_CapTypeOptions * topts,
                              const XVC_CapTypeOptions * sopts);
void xvc_captypeoptions_init (XVC_CapTypeOptions * cto);

XVC_ErrorListItem *xvc_errorlist_delete (XVC_ErrorListItem * err);
void xvc_error_write_msg (int code, int print_action_or_not);

Boolean xvc_is_filename_mutable (const char *);
int xvc_get_number_of_fraction_digits_from_float_string (const char *input);
void xvc_command_execute (const char *command, int flag, int number,
                          const char *file, int fframe, int lframe, int width,
                          int height, XVC_Fps fps);

#endif     // _xvc_APP_DATA_H__
