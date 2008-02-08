/**
 * \file app_data.c
 *
 * This file contains functions for manipulating the general data model used
 * by xvidcap. All user preferences are first stored in an XVC_AppData struct
 * which in turn contains two XVC_CapTypeOptions structs for storing information
 * related to the two modes of operation (single-frame capture and multi-frame
 * capture).<br>
 * On start of a record session xvidcap creates a Job from this and keeps the
 * volatile state there. Also, some settings can be set to autodetect in
 * XVC_AppData and are only determined on creation of the Job to keep the
 * autodetection settings in the XVC_AppData.<br>
 * This file also contains all the data types and functions related to error
 * checking.
 *
 * \todo error_exit_action should cleanup before exiting
 * \todo add validation for use_xdamage
 *
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
#define DEBUGFILE "app_data.c"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif     // HAVE_CONFIG_H
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>                    // PATH_MAX
#include <ctype.h>
#include <pthread.h>
#include <signal.h>

#ifdef HAVE_LIBXFIXES
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xfixes.h>
#endif     // HAVE_LIBXFIXES
#ifdef USE_XDAMAGE
#include "gnome_ui.h"
#include <X11/extensions/Xdamage.h>
#include <X11/Xatom.h>
#endif     // USE_XDAMAGE
#ifdef HAVE_SHMAT
#include <X11/extensions/XShm.h>
#endif     // HAVE_SHMAT

#include "app_data.h"
#include "codecs.h"
#include "frame.h"
#include "xvidcap-intl.h"

#ifndef DOXYGEN_SHOULD_SKIP_THIS
/*
 * functions purely as forward declarations
 */
static XVC_ErrorListItem *errorlist_append (int code, XVC_ErrorListItem * err,
                                            XVC_AppData * app);
static void error_write_action_msg (int code);
static void appdata_set_display (XVC_AppData * app);
#endif     // DOXYGEN_SHOULD_SKIP_THIS

/**
 * \brief pointer to the application's main XVC_AppData structure
 */
static XVC_AppData *app = NULL;

/**
 * \brief allocates a new XVC_AppData structure. It needs to be freed when
 *      no longer used.
 *
 * @return a pointer to an allocated XVC_AppData struct
 */
static XVC_AppData *
appdata_new ()
{
#define DEBUGFUNCTION "appdata_new()"
    app = (XVC_AppData *) malloc (sizeof (XVC_AppData));
    if (!app) {
        perror ("%s %s: malloc failed?!?");
        exit (1);
    }
    return app;
#undef DEBUGFUNCTION
}

/**
 * \brief frees the global XVC_AppData struct
 */
void
xvc_appdata_free (XVC_AppData * lapp)
{
#define DEBUGFUNCTION "xvc_appdata_free()"
    if (app != NULL) {
        pthread_mutex_destroy (&(lapp->recording_paused_mutex));
        pthread_cond_destroy (&(lapp->recording_condition_unpaused));
        pthread_mutex_destroy (&(lapp->capturing_mutex));
#ifdef USE_XDAMAGE
        pthread_mutex_destroy (&(lapp->damage_regions_mutex));
#endif     // USE_XDAMAGE

        free (app);
        app = NULL;
    }
#undef DEBUGFUNCTION
}

/**
 * \brief get a pointer to the current XVC_AppData struct
 *
 * @return a pointer to the current XVC_AppData struct. If a XVC_AppData had
 *      not been allocated before, it will be now.
 */
XVC_AppData *
xvc_appdata_ptr (void)
{
#define DEBUGFUNCTION "xvc_appdata_ptr()"
    if (app == NULL) {
        appdata_new ();
        xvc_appdata_init (app);
    }
    return (app);
#undef DEBUGFUNCTION
}

/**
 * \brief initializes an empty XVC_CapTypeOptions structure.
 *
 * @param cto a pointer to a pre-existing XVC_CapTypeOptions struct to
 *      initialize.
 * @see XVC_CapTypeOptions
 */
void
xvc_captypeoptions_init (XVC_CapTypeOptions * cto)
{
    cto->file = NULL;
    cto->target = -1;
    cto->targetCodec = -1;
    cto->fps = (XVC_Fps) {
    -1, 1};
    cto->time = -1;
    cto->frames = -1;
    cto->start_no = -1;
    cto->step = -1;
    cto->quality = -1;
    cto->play_cmd = NULL;
    cto->video_cmd = NULL;
    cto->edit_cmd = NULL;
#ifdef HAVE_FFMPEG_AUDIO
    cto->au_targetCodec = -1;
    cto->audioWanted = -1;
    cto->sndrate = -1;
    cto->sndsize = -1;
    cto->sndchannels = -1;
#endif     // HAVE_FFMPEG_AUDIO
}

/**
 * \brief initializes an empty XVC_AppData structure.
 *
 * @param lapp a pointer to a pre-existing XVC_AppData struct to initialize.
 * @see XVC_AppData
 */
void
xvc_appdata_init (XVC_AppData * lapp)
{
    lapp->verbose = 0;
    lapp->flags = 0;
    lapp->rescale = 0;
    lapp->mouseWanted = 0;
    lapp->source = NULL;
    lapp->use_xdamage = -1;
#ifdef HAVE_FFMPEG_AUDIO
    lapp->snddev = NULL;
#endif     // HAVE_FFMPEG_AUDIO
#ifdef HasVideo4Linux
    lapp->device = NULL;
#endif     // HasVideo4Linux
    lapp->default_mode = 0;
    lapp->current_mode = -1;
    lapp->dpy = NULL;
    lapp->area = NULL;
#ifdef USE_XDAMAGE
    lapp->dmg_event_base = 0;
#endif     // USE_XDAMAGE

    pthread_mutex_init (&(lapp->recording_paused_mutex), NULL);
    pthread_cond_init (&(lapp->recording_condition_unpaused), NULL);
    pthread_mutex_init (&(lapp->capturing_mutex), NULL);
    lapp->recording_thread_running = FALSE;
#ifdef USE_XDAMAGE
    pthread_mutex_init (&(lapp->damage_regions_mutex), NULL);
#endif     // USE_XDAMAGE

#ifdef USE_DBUS
    lapp->xso = NULL;
#endif     // USE_DBUS

    xvc_captypeoptions_init (&(lapp->single_frame));
#ifdef USE_FFMPEG
    xvc_captypeoptions_init (&(lapp->multi_frame));
#endif     // USE_FFMPEG
}

/**
 * \brief sets default values for XVC_AppData structure.
 *
 * @param lapp a pointer to a pre-existing XVC_AppData struct to set.
 * @see XVC_AppData
 */
void
xvc_appdata_set_defaults (XVC_AppData * lapp)
{
#define DEBUGFUNCTION "xvc_appdata_set_defaults"
    // initialize general options
    // we need the display first
    appdata_set_display (lapp);

    // flags related settings
    lapp->flags = FLG_ALWAYS_SHOW_RESULTS;

#ifdef HAVE_LIBXFIXES
    {
        int a, b;

        if (XFixesQueryExtension (lapp->dpy, &a, &b))
            lapp->flags |= FLG_USE_XFIXES;

#ifdef USE_XDAMAGE
        a = 2;
        b = 0;
        if (lapp->use_xdamage != 0 &&
            XFixesQueryVersion (lapp->dpy, &a, &b) && a >= 2) {
            if (XDamageQueryExtension (lapp->dpy, &(lapp->dmg_event_base), &b)) {
                Window *wm_child = NULL;
                Atom nwm_atom, utf8_string, wm_name_atom, rt;
                unsigned long nbytes, nitems;
                char *wm_name_str = NULL;
                int fmt;

                utf8_string = XInternAtom (lapp->dpy, "UTF8_STRING", False);

                nwm_atom =
                    XInternAtom (lapp->dpy, "_NET_SUPPORTING_WM_CHECK", True);
                wm_name_atom = XInternAtom (lapp->dpy, "_NET_WM_NAME", True);

                if (nwm_atom != None && wm_name_atom != None) {
                    if (XGetWindowProperty (lapp->dpy, lapp->root_window,
                                            nwm_atom, 0, 100,
                                            False, XA_WINDOW,
                                            &rt, &fmt, &nitems, &nbytes,
                                            (unsigned char **) ((void *)
                                                                &wm_child))
                        != Success) {
                        fprintf (stderr,
                                 "%s %s: Error while trying to get a window to identify the window manager.\n",
                                 DEBUGFILE, DEBUGFUNCTION);
                    }
                    if ((wm_child == NULL) ||
                        (XGetWindowProperty
                         (lapp->dpy, *wm_child, wm_name_atom, 0, 100, False,
                          utf8_string, &rt, &fmt, &nitems, &nbytes,
                          (unsigned char **) ((void *)
                                              &wm_name_str))
                         != Success)) {
                        fprintf (stderr,
                                 "%s %s: Warning!!!\nYour window manager appears to be non-compliant!\n",
                                 DEBUGFILE, DEBUGFUNCTION);
                    }
                }
                // Right now only wm's that I know of performing 3d compositing
                // are beryl and compiz. names can be compiz for compiz and
                // beryl/beryl-co/beryl-core for beryl (so it's strncmp )
                if (lapp->use_xdamage != 1 && (!wm_name_str ||
                                               !strcmp (wm_name_str, "compiz")
                                               || !strncmp (wm_name_str,
                                                            "beryl", 5))) {
                    lapp->flags &= ~(FLG_USE_XDAMAGE);
//                    lapp->dmg_event_base = 0;
                } else {
                    lapp->flags |= FLG_USE_XDAMAGE;
                }
            } else {
                lapp->dmg_event_base = 0;
            }
        }
#endif     // USE_XDAMAGE
    }
#endif     // HAVE_LIBXFIXES
#ifdef HasDGA
    {
        int dgy_evb, dga_errb;

        if (!XF86DGAQueryExtension (lapp->dpy, &dga_evb, &dga_errb))
            app->flags &= ~FLG_USE_DGA;
        else {
            int flag = 0;

            XF86DGAQueryDirectVideo (lapp->dpy, lapp->default_screen, &flag);
            if ((flag & XF86DGADirectPresent) == 0) {
                app->flags &= ~FLG_USE_DGA;
                if (app->verbose) {
                    printf ("%s %s: no xfdga direct present\n", DEBUGFILE,
                            DEBUGFUNCTION);
                }
            }
        }
    }
#endif     // HasDGA
#ifdef HAVE_SHMAT
    if (!XShmQueryExtension (lapp->dpy))
        app->flags &= ~FLG_USE_SHM;
#endif     // HAVE_SHMAT

    // capture source related stuff
#ifdef HAVE_SHMAT
    lapp->source = "shm";
#else
    lapp->source = "x11";
#endif     // HAVE_SHMAT
#ifdef HasVideo4Linux
    lapp->device = "/dev/video0";
#endif     // HasVideo4Linux
#ifdef HAVE_FFMPEG_AUDIO
    lapp->snddev = "/dev/dsp";
#endif     // HAVE_FFMPEG_AUDIO

    lapp->mouseWanted = 1;
    lapp->rescale = 100;

    // properties of the area to capture
    lapp->area = xvc_get_capture_area ();
    lapp->area->width = 192;
    lapp->area->height = 144;
    lapp->area->x = lapp->area->y = -1;
    xvc_get_window_attributes (lapp->dpy, lapp->root_window, &(lapp->win_attr));

    // default mode of capture
#ifdef USE_FFMPEG
    lapp->default_mode = 1;
#else
    lapp->default_mode = 0;
#endif     // USE_FFMPEG

#ifdef USE_DBUS
    lapp->xso = xvc_server_object_new ();
#endif     // USE_DBUS

    // initialzie options specific to either single- or multi-frame capture
    lapp->single_frame.quality = lapp->multi_frame.quality = 90;
    lapp->single_frame.step = lapp->multi_frame.step = 1;
    lapp->single_frame.start_no = lapp->multi_frame.start_no = 0;
    lapp->single_frame.file = "test-%04d.xwd";
    lapp->single_frame.fps = lapp->multi_frame.fps = (XVC_Fps) {
    10, 1};
    lapp->single_frame.time = lapp->multi_frame.time = 0;
    lapp->single_frame.frames = lapp->multi_frame.frames = 0;

    // the following two mean autodetect ...
    lapp->single_frame.target = lapp->multi_frame.target = CAP_NONE;
    lapp->single_frame.targetCodec = lapp->multi_frame.targetCodec = CODEC_NONE;

#ifdef HAVE_FFMPEG_AUDIO
    lapp->single_frame.au_targetCodec = lapp->multi_frame.au_targetCodec =
        CODEC_NONE;
    lapp->single_frame.audioWanted = 0;
#endif     // HAVE_FFMPEG_AUDIO

#ifdef USE_FFMPEG
#ifdef HAVE_FFMPEG_AUDIO
    lapp->multi_frame.audioWanted = 1;
    lapp->multi_frame.sndrate = 44100;
    lapp->multi_frame.sndsize = 64000;
    lapp->multi_frame.sndchannels = 2;
#endif     // HAVE_FFMPEG_AUDIO
    lapp->multi_frame.file = "test-%04d.mpeg";

    lapp->multi_frame.edit_cmd =
        _("xterm -e \"echo none specified; echo hit enter to dismiss; read\"");
    lapp->multi_frame.video_cmd =
        _
        ("xterm -e \"echo not needed for multi-frame capture; echo hit enter to dismiss; read\"");
    lapp->multi_frame.play_cmd = "mplayer \"${XVFILE}\" &";
#endif     // USE_FFMPEG
    lapp->single_frame.play_cmd =
        "animate -delay \"${XVTIME%,*}x1000\" \"${XVFILE}\" &";
    lapp->single_frame.video_cmd =
        "ppm2mpeg.sh \"${XVFILE}\" ${XVFFRAME} ${XVLFRAME} ${XVWIDTH} ${XVHEIGHT} ${XVFPS} ${XVTIME} &";
    lapp->single_frame.edit_cmd = "gimp \"${XVFILE}\" &";

#undef DEBUGFUNCTION
}

/**
 * \brief copy a XVC_CapTypeOptions struct
 *
 * @param topts a pointer to a pre-existing XVC_CapTypeOptions struct to
 *      copy to
 * @param sopts a pointer to a XVC_CapTypeOptions struct to copy from
 * @see XVC_CapTypeOptions
 */
void
xvc_captypeoptions_copy (XVC_CapTypeOptions * topts,
                         const XVC_CapTypeOptions * sopts)
{
    topts->file = strdup (sopts->file);
    topts->target = sopts->target;
    topts->targetCodec = sopts->targetCodec;
    topts->fps = sopts->fps;
    topts->time = sopts->time;
    topts->frames = sopts->frames;
    topts->start_no = sopts->start_no;
    topts->step = sopts->step;
    topts->quality = sopts->quality;

    topts->play_cmd = strdup (sopts->play_cmd);
    topts->video_cmd = strdup (sopts->video_cmd);
    topts->edit_cmd = strdup (sopts->edit_cmd);

#ifdef HAVE_FFMPEG_AUDIO
    topts->au_targetCodec = sopts->au_targetCodec;
    topts->audioWanted = sopts->audioWanted;
    topts->sndrate = sopts->sndrate;
    topts->sndsize = sopts->sndsize;
    topts->sndchannels = sopts->sndchannels;
#endif     // HAVE_FFMPEG_AUDIO
}

/**
 * \brief do a deep copy of an XVC_AppData struct
 *
 * @param tapp a pointer to a pre-existing XVC_AppData struc to copy to
 * @param sapp a pointer to an XVC_AppData struct to copy from
 * @see XVC_AppData
 */
void
xvc_appdata_copy (XVC_AppData * tapp, const XVC_AppData * sapp)
{
    tapp->use_xdamage = sapp->use_xdamage;
    tapp->verbose = sapp->verbose;
    tapp->flags = sapp->flags;
    tapp->rescale = sapp->rescale;
    tapp->mouseWanted = sapp->mouseWanted;
    tapp->dpy = sapp->dpy;
    tapp->root_window = sapp->root_window;
    tapp->default_screen = sapp->default_screen;
    tapp->default_screen_num = sapp->default_screen_num;
    tapp->max_width = sapp->max_width;
    tapp->max_height = sapp->max_height;

    tapp->win_attr = sapp->win_attr;
    tapp->area = sapp->area;
#ifdef USE_XDAMAGE
    tapp->dmg_event_base = sapp->dmg_event_base;
#endif     // USE_XDAMAGE

    tapp->source = strdup (sapp->source);
#ifdef HAVE_FFMPEG_AUDIO
    tapp->snddev = strdup (sapp->snddev);
#endif     // HAVE_FFMPEG_AUDIO
#ifdef HasVideo4Linux
    tapp->device = strdup (sapp->device);
#endif     // HasVideo4Linux

#ifdef USE_DBUS
    tapp->xso = sapp->xso;
#endif     // USE_DBUS

    tapp->recording_paused_mutex = sapp->recording_paused_mutex;
    tapp->recording_condition_unpaused = sapp->recording_condition_unpaused;
    tapp->capturing_mutex = sapp->capturing_mutex;
    tapp->recording_thread_running = sapp->recording_thread_running;
#ifdef USE_XDAMAGE
    tapp->damage_regions_mutex = sapp->damage_regions_mutex;
#endif     // USE_XDAMAGE

    tapp->default_mode = sapp->default_mode;
    tapp->current_mode = sapp->current_mode;
    xvc_captypeoptions_copy (&(tapp->single_frame), &(sapp->single_frame));
#ifdef USE_FFMPEG
    xvc_captypeoptions_copy (&(tapp->multi_frame), &(sapp->multi_frame));
#endif     // USE_FFMPEG
}

/**
 * \brief checks an XVC_AppData struct for consistency
 *
 * @param lapp a pointer to the XVC_AppData struct to validate
 * @param mode toggles check for XVC_CapTypeOptions not currently active. mode = 0
 *      does a full check, mode = 1 ignores XVC_CapTypeOptions not currently active
 * @param rc a pointer to an int where a return code will be set. They can be 0 for
 *      no error found, 1 an error was found, or -1 for an internal error
 *      occurred during the check.
 * @return a double-linked list of errors wrapped in XVC_ErrorListItem structs.
 *      This will be NULL if no errors were found.
 */
XVC_ErrorListItem *
xvc_appdata_validate (XVC_AppData * lapp, int mode, int *rc)
{
#undef DEBUGFUNCTION
#define DEBUGFUNCTION "xvc_appdata_validate()"
    XVC_CapTypeOptions *target = NULL;
    XVC_ErrorListItem *errors = NULL;
    int t_codec, t_format;

#ifdef USE_FFMPEG
    XVC_CapTypeOptions *non_target = NULL;

#ifdef HAVE_FFMPEG_AUDIO
    int t_au_codec;
#endif     // HAVE_FFMPEG_AUDIO
#endif     // USE_FFMPEG

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#ifdef USE_FFMPEG
    // we need current_mode determined by now
    if (lapp->current_mode < 0) {
        *rc = -1;
        return NULL;
    }
    if (lapp->current_mode == 0) {
        target = &(lapp->single_frame);
        non_target = &(lapp->multi_frame);
    } else {
        target = &(lapp->multi_frame);
        non_target = &(lapp->single_frame);
    }
#else      // USE_FFMPEG
    // we only have single frame capture
    if (lapp->current_mode != 0)
        printf (_
                ("%s %s: Capture mode not single_frame (%i) but we don't have ffmpeg ... correcting, but smth's wrong\n"),
                DEBUGFILE, DEBUGFUNCTION, lapp->current_mode);

    lapp->current_mode = 0;
    target = &(lapp->single_frame);
#endif     // USE_FFMPEG

    // flags related
    // 1. verbose
    // the verbose member of the appData struct is just here because in the
    // future we might have various levels of verbosity. Atm we just turn it on
    if (lapp->verbose)
        lapp->flags |= FLG_RUN_VERBOSE;

    // start: capture size
    if ((lapp->area->width + lapp->area->x) > lapp->max_width ||
        (lapp->area->height + lapp->area->y) > lapp->max_height) {
        errors = errorlist_append (6, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: capture size

    // start: mouseWanted
    if (lapp->mouseWanted < 0 || lapp->mouseWanted > 2) {
        errors = errorlist_append (7, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: mouseWanted

    // start: source
    if (strcasecmp (app->source, "x11") == 0) {
        // empty
    }
#ifdef HAVE_SHMAT
    else if (strcasecmp (app->source, "shm") == 0)
        lapp->flags |= FLG_USE_SHM;
#endif     // HAVE_SHMAT
    else if (strcasecmp (app->source, "dga") == 0)
        lapp->flags |= FLG_USE_DGA;
    else if (strstr (app->source, "v4l") != NULL)
        lapp->flags |= FLG_USE_V4L;
    else {
        errors = errorlist_append (8, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }

    if (lapp->flags & FLG_USE_V4L) {
#ifndef HasVideo4Linux
        errors = errorlist_append (2, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
#else      // HasVideo4Linux
        VIDEO *video;

        if (strchr (lapp->source, ':') != NULL) {
            lapp->device = strchr (lapp->source, ':') + 1;
        }

        video = video_open (lapp->device, O_RDWR);
        if (!video) {
            errors = errorlist_append (3, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
            *rc = 1;
            return errors;
        }

        if (lapp->flags & FLG_RUN_VERBOSE) {
            printf ("%s:\n cctm=%d channels=%d audios=%d maxwidth=%d "
                    "maxheight=%d minwidth=%d minheight=%d\n",
                    video->name, video->type & VID_TYPE_CAPTURE,
                    video->channels, video->audios, video->maxwidth,
                    video->maxheight, video->minwidth, video->minheight);
        }
        if (!(video->type & VID_TYPE_CAPTURE)) {
            errors = errorlist_append (4, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
            *rc = 1;
            return errors;
        }
        if (lapp->area->width > video->maxwidth
            || lapp->area->width < video->minwidth
            || lapp->area->height > video->maxheight
            || lapp->area->height < video->minheight) {
            errors = errorlist_append (5, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
        video_close (video);
#endif     // HasVideo4Linux
    }
    // end: source

    // snddev needs to be checked from cap_type_options checks
    // e.g. if target has audio enabled and snddev not accessible
    // we throw an error ... nothing to check here, because we
    // wouldn't know what to do, because we don't know if this is
    // relevant for capturing

    // help_cmd related ...
    // no checks atm

    // device related
    // is V4L device which is checked for further up

    // start: default_mode
    if (lapp->default_mode < 0 || lapp->default_mode > 1) {
        errors = errorlist_append (9, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: current_mode

    // current_mode must be selected by now

    // start: rescale
    if (lapp->rescale < 1 || lapp->rescale > 100) {
        errors = errorlist_append (37, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: rescale

    /*
     * Now check target capture type options
     */

    // start: file
    if (!target->file) {
        errors =
            errorlist_append ((lapp->current_mode == 0) ? 10 : 11, errors,
                              lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
        *rc = 1;
        return errors;
    }

    if (strlen (target->file) < 1) {
        if (lapp->current_mode == 0) {
            // single-frame capture does not know "ask-user"
            errors = errorlist_append (12, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
            *rc = 1;
            return errors;
        } else {
            if (target->target == CAP_NONE) {
                // this is an empty filename string which signifies "ask
                // the user" however we do not support "ask-user" and target
                // auto-detect combined at this time ... so we change to
                // default filename
                errors = errorlist_append (13, errors, lapp);
                if (!errors) {
                    *rc = -1;
                    return NULL;
                }
                *rc = 1;
                return errors;
            } else {
                // this is "ask-user"
                if ((lapp->flags & FLG_AUTO_CONTINUE) > 0)
                    errors = errorlist_append (14, errors, lapp);
                else
                    errors = errorlist_append (15, errors, lapp);
                if (!errors) {
                    *rc = -1;
                    return NULL;
                }
            }

        }
    }
    // end: file

    // start: target
    // first find auto-detection results if auto-detection is turned on
    t_format = target->target;
    t_codec = target->targetCodec;
    if (!t_format)
        t_format = xvc_codec_get_target_from_filename (target->file);
    if (!t_codec)
        t_codec = xvc_formats[t_format].def_vid_codec;
#ifdef HAVE_FFMPEG_AUDIO
    t_au_codec = target->au_targetCodec;
    if (!t_au_codec)
        t_au_codec = xvc_formats[t_format].def_au_codec;
#endif     // HAVE_FFMPEG_AUDIO

    // is target valid and does it match the current_mode?
    // the next includes t_format == 0 ... which is at this stage an
    // unresolved auto-detection
    if (t_format <= CAP_NONE || t_format >= NUMCAPS) {
        errors =
            errorlist_append ((lapp->current_mode == 0) ? 16 : 17, errors,
                              lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
        *rc = 1;
        return errors;
    }
#ifdef USE_FFMPEG
    else if (lapp->current_mode == 0 && t_format >= CAP_MF) {
        errors = errorlist_append (18, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
        *rc = 1;
        return errors;
    } else if (lapp->current_mode == 1 && t_format < CAP_MF) {
        errors = errorlist_append (19, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
        *rc = 1;
        return errors;
    }
    // does targetCodec match the target
    if (t_format < CAP_FFM && target->targetCodec != CODEC_NONE) {
        errors = errorlist_append (20, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
#endif     // USE_FFMPEG

    if (target->target == CAP_NONE && target->targetCodec != CODEC_NONE) {
        // if target is auto-detected we also want to auto-detect targetCodec
        errors =
            errorlist_append ((lapp->current_mode == 0) ? 21 : 22, errors,
                              lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
#ifdef USE_FFMPEG
    else if (xvc_formats[t_format].num_allowed_vid_codecs > 0 &&
             xvc_is_valid_video_codec (t_format, t_codec) == 0) {
#ifdef DEBUG
        printf ("app_data: format %i %s - codec %i %s\n", t_format,
                xvc_formats[t_format].name, t_codec, xvc_codecs[t_codec].name);
#endif     // DEBUG

        errors = errorlist_append (23, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
#ifdef HAVE_FFMPEG_AUDIO
    // only check audio settings if audio capture is enabled
    if (target->audioWanted == 1) {
        if (t_format < CAP_FFM && target->au_targetCodec != CODEC_NONE) {
            errors = errorlist_append (24, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        } else
            if (target->target == CODEC_NONE
                && target->au_targetCodec != CODEC_NONE) {
            // if target is auto-detected we also want to auto-detect
            // au_targetCodec
            errors = errorlist_append (25, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        } else if (xvc_formats[t_format].allowed_au_codecs &&
                   xvc_is_valid_audio_codec (t_format, t_au_codec) == 0) {
            errors = errorlist_append (26, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        } else if (xvc_formats[t_format].allowed_au_codecs == NULL
                   && target->audioWanted) {
            errors =
                errorlist_append ((lapp->current_mode == 0) ? 42 : 43,
                                  errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
    }
#endif     // HAVE_FFMPEG_AUDIO
#endif     // USE_FFMPEG
    // end: target

    // targetCodec ... check for suitable format is above with target

    // au_targetCodec ... check for suitable format is above with target

    // start: fps
    if (lapp->current_mode == 1) {
        // if the fps is "almost" valid, find the valid fps and select it
        if (xvc_codec_is_valid_fps (target->fps, t_codec, 1) == 0 &&
            xvc_codec_is_valid_fps (target->fps, t_codec, 0) != 0) {
            int fps_index =
                xvc_get_index_of_fps_array_element (xvc_codecs[t_codec].
                                                    num_allowed_fps,
                                                    xvc_codecs[t_codec].
                                                    allowed_fps,
                                                    target->fps,
                                                    0);

            target->fps = xvc_codecs[t_codec].allowed_fps[fps_index];
        } else if (xvc_codec_is_valid_fps (target->fps, t_codec, 1) == 0) {
            errors = errorlist_append (27, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }

        }
    } else if (lapp->current_mode == 0 && !XVC_FPS_GT_ZERO (target->fps)) {
        errors = errorlist_append (28, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: fps

    // start: time
    if (target->time < 0) {
        errors =
            errorlist_append ((lapp->current_mode == 0) ? 29 : 30, errors,
                              lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: time

    // start: frames
    if (target->time < 0) {
        errors =
            errorlist_append ((lapp->current_mode == 0) ? 31 : 32, errors,
                              lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: frames

    // start: start_no
    if (target->start_no < 0) {
        errors =
            errorlist_append ((lapp->current_mode == 0) ? 33 : 34, errors,
                              lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: start_no

    // start: step
    if (target->step <= 0) {
        errors =
            errorlist_append ((lapp->current_mode == 0) ? 35 : 36, errors,
                              lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    } else if (lapp->current_mode == 1 && target->step != 1) {
        errors = errorlist_append (36, errors, lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: step

    // start: quality
    if ((target->quality < 0) || (target->quality > 100)) {
        errors =
            errorlist_append ((lapp->current_mode == 0) ? 40 : 41, errors,
                              lapp);
        if (!errors) {
            *rc = -1;
            return NULL;
        }
    }
    // end: quality

    // audioWanted is checked with target

    /**
     * \todo implement some sanity checks if possible<br>
     * - int sndrate;                      // sound sample rate<br>
     * - int sndsize;                      // bits to sample for audio capture<br>
     */

    // start: sndchannels
#ifdef USE_FFMPEG
#ifdef HAVE_FFMPEG_AUDIO
    // only check audio settings if audio capture is enabled
    if (target->audioWanted == 1) {
        if (target->au_targetCodec == AU_CODEC_VORBIS
            && target->sndchannels != 2) {
            errors =
                errorlist_append ((lapp->current_mode == 0) ? 38 : 39, errors,
                                  lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
    }
#endif     // HAVE_FFMPEG_AUDIO
#endif     // USE_FFMPEG
    // end: sndchannels

    /**
     * \todo can there be a meaningful check to validate the following?<br>
     * - char *play_cmd;                   // command for animate function<br>
     * - char *video_cmd;                  // command for make video function<br>
     * - char *edit_cmd;                   // command for edit function<br>
     */

    // if we don't have FFMPEG, we can only have one capture type, i.e.
    // the target
#ifdef USE_FFMPEG
    if (mode == 0) {
        /*
         * Now check non-target capture type options
         */

        // start: file
        if (!non_target->file) {
            errors =
                errorlist_append ((lapp->current_mode == 1) ? 10 : 11,
                                  errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
            *rc = 1;
            return errors;
        }

        if (strlen (non_target->file) < 1) {
            if (lapp->current_mode == 1) {
                // single-frame capture does not know "ask-user"
                errors = errorlist_append (12, errors, lapp);
                if (!errors) {
                    *rc = -1;
                    return NULL;
                }
                *rc = 1;
                return errors;
            } else {
                if (non_target->target == CAP_NONE) {
                    // this is an empty filename string which signifies
                    // "ask the user" however we do not support "ask-user" and
                    // target auto-detect combined at this time ... so we
                    // change to default filename
                    errors = errorlist_append (13, errors, lapp);
                    if (!errors) {
                        *rc = -1;
                        return NULL;
                    }
                    *rc = 1;
                    return errors;
                } else {
                    // this is "ask-user"
                    if ((lapp->flags & FLG_AUTO_CONTINUE) > 0)
                        errors = errorlist_append (14, errors, lapp);
                    else
                        errors = errorlist_append (15, errors, lapp);
                    if (!errors) {
                        *rc = -1;
                        return NULL;
                    }
                }

            }
        }
        // end: file

        // start: target
        // first find auto-detection results if auto-detection is turned on
        t_format = non_target->target;
        t_codec = non_target->targetCodec;
        if (!t_format)
            t_format = xvc_codec_get_target_from_filename (non_target->file);
        if (!t_codec)
            t_codec = xvc_formats[t_format].def_vid_codec;

#ifdef HAVE_FFMPEG_AUDIO
        t_au_codec = non_target->au_targetCodec;
        if (!t_au_codec)
            t_au_codec = xvc_formats[t_format].def_au_codec;
#endif     // HAVE_FFMPEG_AUDIO

        // is target valid and does it match the current_mode?
        // the next includes t_format == 0 ... which is at this stage an
        // unresolved auto-detection
        if (t_format <= CAP_NONE || t_format >= NUMCAPS) {
            errors =
                errorlist_append ((lapp->current_mode == 1) ? 16 : 17,
                                  errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
            *rc = 1;
            return errors;
        } else if (lapp->current_mode == 1 && t_format >= CAP_FFM) {
            errors = errorlist_append (18, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
            *rc = 1;
            return errors;
        } else if (lapp->current_mode == 0 && t_format < CAP_FFM) {
            errors = errorlist_append (19, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
            *rc = 1;
            return errors;
        }
        // does targetCodec match the target
        if (t_format < CAP_FFM && non_target->targetCodec != CODEC_NONE) {
            errors = errorlist_append (20, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
        if (non_target->target == CAP_NONE
            && non_target->targetCodec != CODEC_NONE) {
            // if target is auto-detected we also want to auto-detect
            // targetCodec
            errors =
                errorlist_append ((lapp->current_mode == 1) ? 21 : 22,
                                  errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        } else if (xvc_formats[t_format].allowed_vid_codecs &&
                   xvc_is_valid_video_codec (t_format, t_codec) == 0) {
#ifdef DEBUG
            printf ("app_data: format %i %s - codec %i %s\n", t_format,
                    xvc_formats[t_format].name, t_codec,
                    xvc_codecs[t_codec].name);
#endif     // DEBUG

            errors = errorlist_append (23, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
#ifdef HAVE_FFMPEG_AUDIO
        // only check audio settings if audio capture is enabled
        if (non_target->audioWanted == 1) {
            // does au_targetCodec match target
            if (t_format < CAP_FFM && non_target->au_targetCodec != CODEC_NONE) {
                errors = errorlist_append (24, errors, lapp);
                if (!errors) {
                    *rc = -1;
                    return NULL;
                }
            } else
                if (non_target->target == CODEC_NONE
                    && non_target->au_targetCodec != CODEC_NONE) {
                // if target is auto-detected we also want to auto-detect
                // au_targetCodec
                errors = errorlist_append (25, errors, lapp);
                if (!errors) {
                    *rc = -1;
                    return NULL;
                }
            } else if (xvc_formats[t_format].allowed_au_codecs &&
                       xvc_is_valid_audio_codec (t_format, t_au_codec) == 0) {
                errors = errorlist_append (26, errors, lapp);
                if (!errors) {
                    *rc = -1;
                    return NULL;
                }
            } else if (xvc_formats[t_format].allowed_au_codecs == NULL
                       && non_target->audioWanted) {
                errors =
                    errorlist_append ((lapp->current_mode == 1) ? 42 : 43,
                                      errors, lapp);
                if (!errors) {
                    *rc = -1;
                    return NULL;
                }
            }
        }
#endif     // HAVE_FFMPEG_AUDIO
        // end: target

        // targetCodec ... check for suitable format is above with target

        // au_targetCodec ... check for suitable format is above with target

        // start: fps
        if (lapp->current_mode == 0) {
            // if the fps is "almost" valid, find the valid fps and select it
            if (xvc_codec_is_valid_fps (non_target->fps, t_codec, 1) == 0 &&
                xvc_codec_is_valid_fps (non_target->fps, t_codec, 0) != 0) {
                int fps_index =
                    xvc_get_index_of_fps_array_element (xvc_codecs[t_codec].
                                                        num_allowed_fps,
                                                        xvc_codecs[t_codec].
                                                        allowed_fps,
                                                        non_target->fps,
                                                        0);

                non_target->fps = xvc_codecs[t_codec].allowed_fps[fps_index];
            } else if (xvc_codec_is_valid_fps (non_target->fps, t_codec, 1) ==
                       0) {
                errors = errorlist_append (27, errors, lapp);
                if (!errors) {
                    *rc = -1;
                    return NULL;
                }
            }
        } else if (lapp->current_mode == 1 &&
                   !XVC_FPS_GT_ZERO (non_target->fps)) {
            errors = errorlist_append (28, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
        // end: fps

        // start: time
        if (non_target->time < 0) {
            errors =
                errorlist_append ((lapp->current_mode == 1) ? 29 : 30,
                                  errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
        // end: time

        // start: frames
        if (non_target->time < 0) {
            errors =
                errorlist_append ((lapp->current_mode == 1) ? 31 : 32,
                                  errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
        // end: frames

        // start: start_no
        if (non_target->start_no < 0) {
            errors =
                errorlist_append ((lapp->current_mode == 1) ? 33 : 34,
                                  errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
        // end: start_no

        // start: step
        if (non_target->step <= 0) {
            errors =
                errorlist_append ((lapp->current_mode == 1) ? 35 : 36,
                                  errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        } else if (lapp->current_mode == 0 && non_target->step != 1) {
            errors = errorlist_append (36, errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
        // end: step

        // start: quality
        if ((non_target->quality < 0) || (non_target->quality > 100)) {
            errors =
                errorlist_append ((lapp->current_mode == 1) ? 40 : 41,
                                  errors, lapp);
            if (!errors) {
                *rc = -1;
                return NULL;
            }
        }
        // end: quality

        // audioWanted is checked with target

        /**
         * \todo implement some sanity checks if possible<br>
         * - int sndrate;                      // sound sample rate<br>
         * - int sndsize;                      // bits to sample for audio capture<br>
         */
        // start: sndchannels
#ifdef USE_FFMPEG
#ifdef HAVE_FFMPEG_AUDIO
        // only check audio settings if audio capture is enabled
        if (non_target->audioWanted == 1) {
            if (non_target->au_targetCodec == AU_CODEC_VORBIS
                && non_target->sndchannels != 2) {
                errors =
                    errorlist_append ((lapp->current_mode == 1) ? 38 : 39,
                                      errors, lapp);
                if (!errors) {
                    *rc = -1;
                    return NULL;
                }
            }
        }
#endif     // HAVE_FFMPEG_AUDIO
#endif     // USE_FFMPEG
        // end: sndchannels

        /**
         * \todo can there be a meaningful check to validate the following?<br>
         * - char *play_cmd;                   // command for animate function<br>
         * - char *video_cmd;                  // command for make video function<br>
         * - char *edit_cmd;                   // command for edit function<br>
         */

    }
#endif     // USE_FFMPEG

    if (errors)
        *rc = 1;
    else
        *rc = 0;
#ifdef DEBUG
    printf ("%s %s: Leaving %s errors\n", DEBUGFILE, DEBUGFUNCTION,
            ((errors) ? "with" : "without"));
#endif     // DEBUG

    return errors;
#undef DEBUGFUNCTION
}

/**
 * \brief merges a XVC_CapTypeOptions structure into an XVC_AppData as current
 *      target
 *
 * This is mainly used for assimilating command line parameters into the
 * XVC_AppData. Command line parameters are always regarded to manipulate
 * global settings or settings for the currently active capture mode.
 *
 * @param cto a pointer to the XVC_CapTypeOptions struct to assimilate into an
 *      XVC_AppData struct
 * @param lapp a pointer to a pre-existing XVC_AppData struct to assimilate the
 *      XVC_CapTypeOptions into.
 */
void
xvc_appdata_merge_captypeoptions (XVC_CapTypeOptions * cto, XVC_AppData * lapp)
{
#define DEBUGFUNCTION "xvc_appdata_merge_captypeoptions()"
    XVC_CapTypeOptions *target = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#ifdef USE_FFMPEG
    // we need current_mode determined by now
    if (lapp->current_mode < 0)
        return;
    if (lapp->current_mode == 0)
        target = &(lapp->single_frame);
    else
        target = &(lapp->multi_frame);
#else      // USE_FFMPEG
    // we only have single frame capture
    if (lapp->current_mode != 0)
        printf (_
                ("%s %s: Capture mode not single_frame (%i) but we don't have ffmpeg ... correcting, but smth's wrong\n"),
                DEBUGFILE, DEBUGFUNCTION, lapp->current_mode);

    lapp->current_mode = 0;
    target = &(lapp->single_frame);
#endif     // USE_FFMPEG

    // set the capture options of the right mode to the specified values
    if (cto->file != NULL)
        target->file = strdup (cto->file);
    if (cto->target > -1)
        target->target = cto->target;
    if (cto->targetCodec > -1)
        target->targetCodec = cto->targetCodec;
    if (XVC_FPS_GTE_ZERO (cto->fps))
        target->fps = cto->fps;
    if (cto->time > -1)
        target->time = cto->time;
    if (cto->frames > -1)
        target->frames = cto->frames;
    if (cto->start_no > -1)
        target->start_no = cto->start_no;
    if (cto->step > -1)
        target->step = cto->step;
    if (cto->quality > -1)
        target->quality = cto->quality;
#ifdef HAVE_FFMPEG_AUDIO
    if (cto->au_targetCodec > -1)
        target->au_targetCodec = cto->au_targetCodec;
    if (cto->audioWanted > -1)
        target->audioWanted = cto->audioWanted;
    if (cto->sndrate > -1)
        target->sndrate = cto->sndrate;
    if (cto->sndsize > -1)
        target->sndsize = cto->sndsize;
    if (cto->sndchannels > -1)
        target->sndchannels = cto->sndchannels;
#endif     // HAVE_FFMPEG_AUDIO
    if (cto->play_cmd != NULL)
        target->play_cmd = cto->play_cmd;
    if (cto->video_cmd != NULL)
        target->video_cmd = cto->video_cmd;
    if (cto->edit_cmd != NULL)
        target->edit_cmd = cto->edit_cmd;

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#undef DEBUGFUNCTION
}

/*
 * error handling
 * each error has at least one XVC_Error struct and possibly a function
 * for use as a default action
 */

/**
 * \brief default action shared by all fatal errors
 *
 * @param err a pointer to the actual error raising this.
 */
static void
error_exit_action (XVC_ErrorListItem * err)
{
    raise (SIGINT);
}

/**
 * \brief default action shared by all informational error messages
 *
 * @param err a pointer to the actual error raising this.
 */
static void
error_null_action (XVC_ErrorListItem * err)
{
    // empty
}

/**
 * \brief dummy error action which just prints debug info
 *
 * @param err a pointer to the actual error raising this.
 */
static void
error_1_action (XVC_ErrorListItem * err)
{
    fprintf (stderr, "%s\n", err->err->short_msg);
    fprintf (stderr, "%s\n", err->err->long_msg);
}

static void
error_2_action (XVC_ErrorListItem * err)
{
    err->app->flags &= ~FLG_SOURCE;
    err->app->source = "x11";
}

static void
error_5_action (XVC_ErrorListItem * err)
{
#ifdef HasVideo4Linux
    VIDEO *video;

    if (strchr (lapp->source, ':') != NULL) {
        lapp->device = strchr (lapp->source, ':') + 1;
    }

    video = video_open (lapp->device, O_RDWR);
    if (!video) {
        xvc_error_write_msg (3);
        error_write_action_msg (3);
        (*xvc_errors[3].action) (NULL);
    }

    err->app->area->width = XVC_MIN (err->app->area->width, video->maxwidth);
    err->app->area->height = XVC_MIN (err->app->area->height, video->maxheight);
    err->app->area->width = XVC_MAX (err->app->area->width, video->minwidth);
    err->app->area->height = XVC_MAX (err->app->area->height, video->minheight);

    video_close (video);
#endif     // HasVideo4Linux
}

static void
error_6_action (XVC_ErrorListItem * err)
{
    Display *dpy = app->dpy;
    int max_width, max_height;

    max_width = WidthOfScreen (DefaultScreenOfDisplay (dpy));
    max_height = HeightOfScreen (DefaultScreenOfDisplay (dpy));

    err->app->area->width = max_width - err->app->area->x;
    err->app->area->height = max_height - err->app->area->y;
}

static void
error_7_action (XVC_ErrorListItem * err)
{
    err->app->mouseWanted = 1;
}

static void
error_8_action (XVC_ErrorListItem * err)
{
    err->app->flags &= ~FLG_SOURCE;
    err->app->source = "x11";
}

static void
error_9_action (XVC_ErrorListItem * err)
{
    err->app->default_mode = 0;
}

// error twelve is for zero length filenames with target cap_type_options
// only ... for non target we don't make any change and we can't make the
// distinction from within the error this is only an error for single-frame
// ... multi-frame uses empty filenames to signify ask-user
static void
error_12_action (XVC_ErrorListItem * err)
{
#define DEBUGFUNCTION "error_12_action()"
    XVC_CapTypeOptions *target = NULL;
    char *fn = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#ifdef USE_FFMPEG
    if (err->app->current_mode == 0)
        target = &(err->app->single_frame);
    else
        target = &(err->app->multi_frame);
#else      // USE_FFMPEG
    err->app->current_mode = 0;
    target = &(err->app->single_frame);
#endif     // USE_FFMPEG

    if (target->target > 0 && target->target < NUMCAPS) {
        char tmp_fn[512];
        const char *extension = xvc_formats[target->target].extensions[0];

        sprintf (tmp_fn, "test-%s.%s", "%04d", extension);
        fn = tmp_fn;
    } else {
#ifdef USE_FFMPEG
        if (err->app->current_mode == 0)
#endif     // USE_FFMPEG
            fn = "test-%04d.xwd";
#ifdef USE_FFMPEG
        else
            fn = "test-%04d.mpeg";
#endif     // USE_FFMPEG
        target->target = 0;
    }

    target->file = strdup (fn);

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#undef DEBUGFUNCTION
}

static void
error_14_action (XVC_ErrorListItem * err)
{
    // the temporary filename needs to be set when the job is created
    // because we don't want to store the temporary filename in preferences
    // this just disables autocontinue
    err->app->flags &= ~FLG_AUTO_CONTINUE;
}

static void
error_16_action (XVC_ErrorListItem * err)
{
#define DEBUGFUNCTION "error_16_action()"
#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#ifdef USE_FFMPEG
    if (err->app->current_mode == 0) {
        err->app->single_frame.target = CAP_XWD;
    } else {
        err->app->multi_frame.target = CAP_DIVX;
    }
#else      // USE_FFMPEG
    err->app->current_mode = 0;
    err->app->single_frame.target = CAP_XWD;
#endif     // USE_FFMPEG

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

static void
error_18_action (XVC_ErrorListItem * err)
{
    err->app->single_frame.target = CAP_XWD;
}

#ifdef USE_FFMPEG
static void
error_19_action (XVC_ErrorListItem * err)
{
    err->app->multi_frame.target = CAP_MPG;
}

static void
error_23_action (XVC_ErrorListItem * err)
{
    err->app->multi_frame.targetCodec =
        xvc_formats[err->app->multi_frame.target].def_vid_codec;
}
#endif     // USE_FFMPEG

#ifdef HAVE_FFMPEG_AUDIO
static void
error_26_action (XVC_ErrorListItem * err)
{
    err->app->multi_frame.au_targetCodec =
        xvc_formats[err->app->multi_frame.target].def_au_codec;
}
#endif     // HAVE_FFMPEG_AUDIO

static void
error_28_action (XVC_ErrorListItem * err)
{
    err->app->single_frame.fps = (XVC_Fps) {
    1, 1};
}

static void
error_29_action (XVC_ErrorListItem * err)
{
#define DEBUGFUNCTION "error_29_action()"
    XVC_CapTypeOptions *target = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#ifdef USE_FFMPEG
    if (err->app->current_mode == 0)
        target = &(err->app->single_frame);
    else
        target = &(err->app->multi_frame);
#else      // USE_FFMPEG
    err->app->current_mode = 0;
    target = &(err->app->single_frame);
#endif     // USE_FFMPEG

    target->time = 0;

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

static void
error_31_action (XVC_ErrorListItem * err)
{
#define DEBUGFUNCTION "error_31_action()"
    XVC_CapTypeOptions *target = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#ifdef USE_FFMPEG
    if (err->app->current_mode == 0)
        target = &(err->app->single_frame);
    else
        target = &(err->app->multi_frame);
#else      // USE_FFMPEG
    err->app->current_mode = 0;
    target = &(err->app->single_frame);
#endif     // USE_FFMPEG

    target->frames = 0;

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

static void
error_33_action (XVC_ErrorListItem * err)
{
#define DEBUGFUNCTION "error_33_action()"
    XVC_CapTypeOptions *target = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#ifdef USE_FFMPEG
    if (err->app->current_mode == 0)
        target = &(err->app->single_frame);
    else
        target = &(err->app->multi_frame);
#else      // USE_FFMPEG
    err->app->current_mode = 0;
    target = &(err->app->single_frame);
#endif     // USE_FFMPEG

    target->start_no = 0;

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

static void
error_35_action (XVC_ErrorListItem * err)
{
#define DEBUGFUNCTION "error_35_action()"
    XVC_CapTypeOptions *target = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#ifdef USE_FFMPEG
    if (err->app->current_mode == 0)
        target = &(err->app->single_frame);
    else
        target = &(err->app->multi_frame);
#else      // USE_FFMPEG
    err->app->current_mode = 0;
    target = &(err->app->single_frame);
#endif     // USE_FFMPEG

    target->step = 1;

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

static void
error_37_action (XVC_ErrorListItem * err)
{
#define DEBUGFUNCTION "error_37_action()"

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    err->app->rescale = 100;

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

#ifdef USE_FFMPEG
#ifdef HAVE_FFMPEG_AUDIO
static void
error_38_action (XVC_ErrorListItem * err)
{
#define DEBUGFUNCTION "error_38_action()"
    XVC_CapTypeOptions *target = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    if (err->app->current_mode == 0)
        target = &(err->app->single_frame);
    else
        target = &(err->app->multi_frame);

    target->sndchannels = 2;

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}
#endif     // HAVE_FFMPEG_AUDIO
#endif     // USE_FFMPEG

static void
error_40_action (XVC_ErrorListItem * err)
{
#define DEBUGFUNCTION "error_40_action()"
    XVC_CapTypeOptions *target = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

#ifdef USE_FFMPEG
    if (err->app->current_mode == 0)
        target = &(err->app->single_frame);
    else
        target = &(err->app->multi_frame);
#else      // USE_FFMPEG
    err->app->current_mode = 0;
    target = &(err->app->single_frame);
#endif     // USE_FFMPEG

    target->quality = 1;

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}

#ifdef HAVE_FFMPEG_AUDIO
static void
error_42_action (XVC_ErrorListItem * err)
{
#define DEBUGFUNCTION "error_42_action()"
    XVC_CapTypeOptions *target = NULL;

#ifdef DEBUG
    printf ("%s %s: Entering\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG

    if (err->app->current_mode == 0)
        target = &(err->app->single_frame);
    else
        target = &(err->app->multi_frame);

    target->audioWanted = 0;

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef DEBUGFUNCTION
}
#endif     // HAVE_FFMPEG_AUDIO

/**
 * \brief an array of all preferences related errors known to xvidcap.
 *
 * The array consists of XVC_ErrorListItem elements.
 */
const XVC_Error xvc_errors[NUMERRORS] = {
    {
     1,
     XVC_ERR_ERROR,
     "error 1",
     "this is my first error",
     error_1_action,
     "Display the error messages (smth. that should normally be done outside the default action)"},
    {
     2,
     XVC_ERR_ERROR,
     N_("No V4L available"),
     N_
     ("Video for Linux is selected as the capture source but V4L is not available with this binary."),
     error_2_action,
     N_("Set capture source to 'x11'")
     },
    {
     3,
     XVC_ERR_FATAL,
     N_("V4L device inaccessible"),
     N_("Can't open the specified Video for Linux device."),
     error_exit_action,
     N_("Quit")
     },
    {
     4,
     XVC_ERR_FATAL,
     N_("V4L can't capture"),
     N_("The specified Video for Linux device can't capture to memory."),
     error_exit_action,
     N_("Quit")
     },
    {
     5,
     XVC_ERR_ERROR,
     N_("Capture size exceeds V4L limits"),
     N_
     ("The specified capture size exceeds the limits for Video for Linux capture."),
     error_5_action,
     N_("Increase or decrease the capture area to conform to V4L's limits")
     },
    {
     6,
     XVC_ERR_WARN,
     N_("Capture area outside screen"),
     N_("The specified capture area reaches beyond the visible screen."),
     error_6_action,
     N_("Clip capture size to visible screen")
     },
    {
     7,
     XVC_ERR_WARN,
     N_("Invalid Mouse Capture Option"),
     N_
     ("The specified value for the mouse capture option is not one of the valid values (0 = no capture, 1 = white mouse pointer, 2 = black mouse pointer)."),
     error_7_action,
     N_("Set Mouse Capture Option to '1' for a white mouse pointer")
     },
    {
     8,
     XVC_ERR_WARN,
     N_("Invalid Capture Source"),
     N_("An unsupported capture source was selected."),
     error_8_action,
     N_("Reset capture source to fail-safe 'x11'")
     },
    {
     9,
     XVC_ERR_WARN,
     N_("Invalid default capture mode"),
     N_
     ("The default capture mode specified is neither \"single-frame\" nor \"mult-frame\"."),
     error_9_action,
     N_("Reset default capture mode to \"single-frame\"")
     },
    {
     10,
     XVC_ERR_FATAL,
     "File name is NULL",
     "The filename string variable used for single-frame capture is a null pointer. This should never be possible at this stage.",
     error_exit_action,
     N_("Quit")
     },
    {
     11,
     XVC_ERR_FATAL,
     N_("File name is NULL"),
     N_
     ("The filename string variable used for multi-frame capture is a null pointer. This should never be possible at this stage."),
     error_exit_action,
     N_("Quit")
     },
    {
     12,
     XVC_ERR_ERROR,
     N_("File name empty"),
     N_("You selected single-frame capture without specifying a filename."),
     error_12_action,
     N_
     ("Set the filename used for single-frame capture to a default value based on the current capture mode and target")
     },
    {
     13,
     XVC_ERR_ERROR,
     N_("File format Auto-detection with empty file name"),
     N_
     ("You want to be asked for a filename to save your video to after recording and also want to auto-detect the file type based on filename. That won't work."),
     error_12_action,
     N_
     ("Set the filename used for multi-frame capture to a default value based on the current capture mode and target")
     },
    {
     14,
     XVC_ERR_WARN,
     N_("File name empty, ask user"),
     N_
     ("You specified an empty filename for multi-frame capture. Since you have configured a fixed output file-format xvidcap can ask you for a filename to use after recording. We'll need to disable the autocontinue function you selected, though."),
     error_14_action,
     N_
     ("Enable \"ask user for filename\" mode for multi-frame capture and disable autocontinue")
     },
    {
     15,
     XVC_ERR_INFO,
     N_("File name empty, ask user"),
     N_
     ("You specified an empty filename for multi-frame capture. Since you have configured a fixed output file-format xvidcap can ask you for a filename to use after each recording."),
     error_null_action,
     N_("Ignore, everything is set up alright")
     },
    {
     16,
     XVC_ERR_ERROR,
     N_("Invalid file format"),
     N_
     ("You selected an unknown or invalid file format for single-frame capture. Check the --format-help option."),
     error_16_action,
     N_("Reset to default format for single-frame capture (XWD)")
     },
    {
     17,
     XVC_ERR_ERROR,
     N_("Invalid file format"),
     N_
     ("You selected an unknown or invalid file format for multi-frame capture. Check the --format-help option."),
     error_16_action,
     N_("Reset to default format for multi-frame caputre (MPEG4)")
     },
    {
     18,
     XVC_ERR_ERROR,
     N_("Single-Frame Capture with Multi-Frame File Format"),
     N_
     ("You selected single-frame capture mode but specified a target file format which is used for multi-frame capture."),
     error_18_action,
     N_("Reset file format to default for single-frame capture")
     },
#ifdef USE_FFMPEG
    {
     19,
     XVC_ERR_ERROR,
     N_("Multi-Frame Capture with Single-Frame File Format"),
     N_
     ("You selected multi-frame capture mode but specified a target file format which is used for single-frame capture."),
     error_19_action,
     N_("Reset file format to default for multi-frame capture")
     },
#endif     // USE_FFMPEG
    {
     20,
     XVC_ERR_INFO,
     N_("Single-Frame Capture with invalid target codec"),
     N_
     ("You selected single-frame capture mode and specified a target codec not valid with the file format."),
     error_null_action,
     N_
     ("Ignore because single-frame capture types always have a well-defined codec")
     },
    {
     21,
     XVC_ERR_INFO,
     N_("File Format auto-detection may conflict with explicit codec"),
     N_
     ("For single-frame capture you requested auto-detection of the output file format and explicitly specified a target codec. Your codec selection may not be valid for the file format eventually detected, in which case it will be overridden by the format's default codec."),
     error_null_action,
     N_("Ignore")
     },
    {
     22,
     XVC_ERR_INFO,
     N_("File Format auto-detection may conflict with explicit codec"),
     N_
     ("For multi-frame capture you requested auto-detection of the output file format and explicitly specified a target codec. Your codec selection may not be valid for the file format eventually detected, in which case it will be overridden by the format's default codec."),
     error_null_action,
     N_("Ignore")
     },
#ifdef USE_FFMPEG
    {
     23,
     XVC_ERR_ERROR,
     N_("Multi-Frame Capture with invalid codec"),
     N_
     ("You selected multi-frame capture mode and specified a target codec not valid with the file format."),
     error_23_action,
     N_("Reset multi-frame target codec to default for file format")
     },
#ifdef HAVE_FFMPEG_AUDIO
    {
     24,
     XVC_ERR_INFO,
     "Single-Frame Capture with invalid target audio codec",
     "You selected single-frame capture mode and specified a target audio codec not valid with the file format.",
     error_null_action,
     "Ignore because single-frame capture types don't support audio capture anyway"},
    {
     25,
     XVC_ERR_INFO,
     N_("File Format auto-detection may conflict with explicit audio codec"),
     N_
     ("For multi-frame capture you requested auto-detection of the output file format and explicitly specified a target audio codec. Your audio codec selection may not be valid for the file format eventually detected, in which case it will be overridden by the format's default audio codec."),
     error_null_action,
     N_("Ignore")
     },
    {
     26,
     XVC_ERR_ERROR,
     N_("Multi-Frame Capture with invalid audio codec"),
     N_
     ("You selected multi-frame capture mode and specified a target audio codec not valid with the file format."),
     error_26_action,
     N_("Reset multi-frame target audio codec to default for file format")
     },
#endif     // HAVE_FFMPEG_AUDIO
#endif     // USE_FFMPEG
    {
     27,
     XVC_ERR_FATAL,
     N_("Invalid frame-rate for selected codec"),
     N_
     ("You selected multi-frame capture mode but the requested frame-rate is not valid for the selected codec. This would result in the video playing back too slowly or quickly."),
     error_exit_action,
     N_("Quit")
     },
    {
     28,
     XVC_ERR_WARN,
     N_("Requested Frame Rate <= zero"),
     N_
     ("You selected single-frame capture but the frame rate you requested is not greater than zero."),
     error_28_action,
     N_("Set frame rate to '1'")
     },
    {
     29,
     XVC_ERR_WARN,
     N_("Requested Maximum Capture Time < zero"),
     N_
     ("For single-frame capture you specified a maximum capture time which is less than zero seconds."),
     error_29_action,
     N_("Set the maximum capture time for single-frame capture to unlimited")
     },
    {
     30,
     XVC_ERR_WARN,
     N_("Requested Maximum Capture Time < zero"),
     N_
     ("For multi-frame capture you specified a maximum capture time which is less than zero seconds."),
     error_29_action,
     N_("Set the maximum capture time for multi-frame capture to unlimited")
     },
    {
     31,
     XVC_ERR_WARN,
     N_("Requested Maximum Frames < zero"),
     N_
     ("For single-frame capture you specified a maximum number of frames which is less than zero."),
     error_31_action,
     N_("Set maximum number of frames to unlimited for single-frame capture")
     },
    {
     32,
     XVC_ERR_WARN,
     N_("Requested Maximum Frames < zero"),
     N_
     ("For multi-frame capture you specified a maximum number of frames which is less than zero."),
     error_31_action,
     N_("Set maximum number of frames to unlimited for multi-frame capture")
     },
    {
     33,
     XVC_ERR_WARN,
     N_("Requested Start Number < zero"),
     N_
     ("For single-frame capture you specified a start number for frame numbering which is less than zero."),
     error_33_action,
     N_("Set start number for frame numbering of single-frame capture to '0'")
     },
    {
     34,
     XVC_ERR_WARN,
     N_("Requested Start Number < zero"),
     N_
     ("For multi-frame capture you specified a start number for frame numbering which is less than zero."),
     error_33_action,
     N_("Set start number for frame numbering of multi-frame capture to '0'")
     },
    {
     35,
     XVC_ERR_WARN,
     N_("Requested Frame Increment <= zero"),
     N_
     ("For single-frame capture you specified an increment for frame numbering which is not greater than zero."),
     error_35_action,
     N_("Set increment for frame numbering of single-frame capture to '1'")
     },
    {
     36,
     XVC_ERR_WARN,
     N_("Requested Frame Increment <> one"),
     N_
     ("For multi-frame capture you specified an increment for frame numbering which is not exactly one."),
     error_35_action,
     N_("Set increment for frame numbering of multi-frame capture to '1'")
     },
    {
     37,
     XVC_ERR_WARN,
     N_("Requested invalid Rescale Percentage"),
     N_
     ("You requested rescaling of the input area to a relative size but did not specify a valid percentage. Valid values are between 1 and 100 "),
     error_37_action,
     N_("Set rescale percentage to '100'")
     },
#ifdef USE_FFMPEG
#ifdef HAVE_FFMPEG_AUDIO
    {
     38,
     XVC_ERR_WARN,
     N_("Vorbis needs stereo audio"),
     N_
     ("You selected mono vorbis audio output for single-frame capture, but this version of xvidcap can only encode stereo vorbis audio streams."),
     error_38_action,
     N_("Set number of audio channels to '2'")
     },
    {
     39,
     XVC_ERR_WARN,
     N_("Vorbis needs stereo audio"),
     N_
     ("You selected mono vorbis audio output for multi-frame capture, but this version of xvidcap can only encode stereo vorbis audio streams."),
     error_38_action,
     N_("Set number of audio channels to '2'")
     },
#endif     // HAVE_FFMPEG_AUDIO
#endif     // USE_FFMPEG
    {
     40,
     XVC_ERR_WARN,
     N_("Quality value not a valid percentage"),
     N_
     ("For single-frame capture the value for recording quality is not a percentage between 1 and 100."),
     error_40_action,
     N_("Set recording quality to '75'")
     },
    {
     41,
     XVC_ERR_WARN,
     N_("Quality value not a valid percentage"),
     N_
     ("For multi-frame capture the value for recording quality is not a percentage between 1 and 100."),
     error_40_action,
     N_("Set recording quality to '75'")
     },
#ifdef HAVE_FFMPEG_AUDIO
    {
     42,
     XVC_ERR_WARN,
     N_("Audio Capture not supported by File Format"),
     N_
     ("You selected audio capture but single-frame captures do not support it."),
     error_42_action,
     N_("Disable audio capture for single-frame capture")
     },
    {
     43,
     XVC_ERR_WARN,
     N_("Audio Capture not supported by File Format"),
     N_
     ("For multi-frame capture you selected audio support but picked a file format without support for audio."),
     error_42_action,
     N_("Disable audio capture for multi-frame capture")
     }
#endif     // HAVE_FFMPEG_AUDIO
};

/**
 * \brief adds a new error to an error list
 *
 * An XVC_Error of the specified code is wrapped in an XVC_ErrorListItem and
 * appended to the given list. If err == NULL, a new list is created.
 * err does not need to point to the last element of the list. The new
 * error, however, is always appended at the end and err is returned as
 * it was passed in or pointing to a new error list if it was NULL.
 *
 * @param code an integer error code
 * @param err an element in a list of XVC_ErrorListItems, can be NULL.
 * @param app a pointer to the XVC_AppData struct to refer to from the
 *      new XVC_ErrorListItem.
 * @return the original list with the new element appended or a new
 *      list with just the new element.
 */
static XVC_ErrorListItem *
errorlist_append (int code, XVC_ErrorListItem * err, XVC_AppData * app)
{
    XVC_ErrorListItem *new_err, *iterator = NULL, *last = NULL;
    int i, a = -1;

    for (i = 0; /*xvc_errors[i].code > 0 */ i < NUMERRORS; i++) {
        if (xvc_errors[i].code == code) {
            a = i;
            break;
        }
    }

    // could not find error to add to list
    if (a < 0)
        return NULL;
    new_err = malloc (sizeof (struct _xvc_ErrorListItem));
    if (!new_err)
        return NULL;

    new_err->err = &(xvc_errors[a]);
    new_err->app = app;
    new_err->next = new_err->previous = NULL;

    if (err == NULL) {
        // new list
        new_err->previous = NULL;
        err = new_err;
    } else {
        last = err;
        iterator = err->next;
        while (iterator) {
            last = iterator;
            iterator = iterator->next;
        }
        last->next = new_err;
        new_err->previous = last;
    }

    return err;
}

/**
 * \brief recursively free error list
 *
 * @param err a pointer to the list of XVC_ErrorElements to free
 * @return a pointer to the resulting list of XVC_ErrorElements, i. e. NULL
 *      if the deletion was successful
 */
XVC_ErrorListItem *
xvc_errorlist_delete (XVC_ErrorListItem * err)
{
    XVC_ErrorListItem *iterator = NULL, *last = NULL;

    if (!err)
        return NULL;

    last = err;
    iterator = err->next;
    while (iterator) {
        last = iterator;
        iterator = iterator->next;
    }

    iterator = last;
    while (iterator) {
        last = iterator->previous;
        free (iterator);
        iterator = last;
    }

    return iterator;
}

/*
 * fucntions for printing error messages
 */
static int
find_line_length (char *txt, char delim, int len)
{
    char buf[200];
    char *cont;
    int i = 0;

    if (!txt)
        return 0;
    if (strlen (txt) < len)
        return strlen (txt);

    strncpy (buf, txt, len);
    cont = rindex (buf, (int) delim);
    i = ((int) cont - (int) buf);
    i++;

    return i;
}

static void
lineprint (char *txt, int out_or_err, int pre, int len)
{
    int i = 0, n, inc;
    char buf[200];

    if (!txt)
        return;

    while (strlen (txt) > (i + len)) {

        for (n = 0; n < pre; n++) {
            if (out_or_err)
                fprintf (stderr, " ");
            else
                fprintf (stdout, " ");
        }

        inc = find_line_length ((txt + i), ' ', len);
        strncpy (buf, (txt + i), inc);
        buf[inc] = '\0';
        if (out_or_err)
            fprintf (stderr, "%s\n", buf);
        else
            fprintf (stdout, "%s\n", buf);

        i += inc;
    }

    for (n = 0; n < pre; n++) {
        if (out_or_err)
            fprintf (stderr, " ");
        else
            fprintf (stdout, " ");
    }

    inc = find_line_length ((txt + i), ' ', len);
    strncpy (buf, (txt + i), inc);
    buf[inc] = '\0';
    if (out_or_err)
        fprintf (stderr, "%s\n", buf);
    else
        fprintf (stdout, "%s\n", buf);

}

/**
 * \brief prints an error message to stdout
 *
 * @param code the integer code of the error to print
 * @param print_action_or_not toggle printing of the action (0 = off / 1 = on)
 */
void
xvc_error_write_msg (int code, int print_action_or_not)
{
    char *type = NULL;
    char scratch[50], buf[5000];
    const XVC_Error *err;
    int a = 0, i, inc, pre = 10, len = 80;

    // first we must find the right array element for the error
    for (i = 0; /*xvc_errors[i].code > 0 */ i < NUMERRORS; i++) {
        if (xvc_errors[i].code == code) {
            a = i;
            break;
        }
    }
    if (a == 0)
        return;
    err = &(xvc_errors[a]);

    switch (err->type) {
    case 1:
        type = _("FATAL ERROR");
        break;
    case 2:
        type = _("ERROR");
        break;
    case 3:
        type = _("WARNING");
        break;
    case 4:
        type = _("INFO");
        break;
    default:
        type = _("UNKNOWN");
    }
    /** \todo pretty print later ... */
    sprintf (scratch, "== %s \0", type);
    fprintf (stderr, "%s", scratch);
    for (i = strlen (scratch); i < len; i++) {
        fprintf (stderr, "=");
    }
    fprintf (stderr, "\n");

    sprintf (scratch, _("CODE %i:\0"), err->code);
    fprintf (stderr, "%s", scratch);
    for (i = strlen (scratch); i < pre; i++) {
        fprintf (stderr, " ");
    }

    if (strlen (err->short_msg) > 52)
        inc = find_line_length (_(err->short_msg), ' ', (len - pre));
    else
        inc = strlen (_(err->short_msg));
    strncpy (buf, _(err->short_msg), inc);
    buf[inc] = '\0';
    fprintf (stderr, "%s\n", buf);
    if (strlen (_(err->short_msg)) > inc) {
        strncpy (buf, (_(err->short_msg) + inc), 5000);
        lineprint (buf, 1, pre, (len - pre));
    }

    if (app->flags & FLG_RUN_VERBOSE) {
        sprintf (scratch, _("DESC:\0"));
        fprintf (stderr, "%s", scratch);
        for (i = strlen (scratch); i < pre; i++) {
            fprintf (stderr, " ");
        }

        if (strlen (_(err->long_msg)) > (len - pre))
            inc = find_line_length (_(err->long_msg), ' ', (len - pre));
        else
            inc = strlen (_(err->long_msg));
        strncpy (buf, _(err->long_msg), inc);
        buf[inc] = '\0';
        fprintf (stderr, "%s\n", buf);
        if (strlen (_(err->long_msg)) > inc) {
            strncpy (buf, (_(err->long_msg) + inc), 5000);
            lineprint (buf, 1, pre, (len - pre));
        }
    }

    if (print_action_or_not) {
        error_write_action_msg (code);
    }

    for (i = 0; i < len; i++) {
        fprintf (stderr, "-");
    }
    fprintf (stderr, "\n");

}

/**
 * \brief print action msg
 *
 * @param code the integer code of the error in question
 */
static void
error_write_action_msg (int code)
{
    const XVC_Error *err = &(xvc_errors[code]);
    int i, inc, pre = 10, len = 80;
    char scratch[50], buf[5000];

    sprintf (scratch, _("ACTION:\0"));
    fprintf (stderr, "%s", scratch);
    for (i = strlen (scratch); i < pre; i++) {
        fprintf (stderr, " ");
    }

    if (strlen (_(err->action_msg)) > (len - pre))
        inc = find_line_length (_(err->action_msg), ' ', (len - pre));
    else
        inc = strlen (_(err->action_msg));
    strncpy (buf, _(err->action_msg), inc);
    buf[inc] = '\0';
    fprintf (stderr, "%s\n", buf);
    if (strlen (_(err->action_msg)) > inc) {
        strncpy (buf, (_(err->action_msg) + inc), 5000);
        lineprint (buf, 1, pre, (len - pre));
    }

}

/**
 * \brief executes one of the helper commands
 *
 * This executes on of the helper commands that xvidcap uses for animating
 * individual images to video, transcoding, or playing a video. Depending
 * on the context, the filename may have to be treated differently. If the
 * user has a SHELL environment variable set, that shell will be used to
 * execute the commands.
 *
 * @param command the command string to execute
 * @param number a number to put use with the mutable filename, typically the
 *      current frame number for single-frame capture or the current movie
 *      number for multi-frame capture.
 * @param file file-name pattern
 * @param fframe first frame of a recording
 * @param lframe last frame of a recording
 * @param width width of the area captured
 * @param height height of the area captured
 * @param fps frame rate used
 * @param flag this governs how the function treats the filename passed.
 *      Possible options are:
 *      - 0 : send filename pattern literally
 *      - 1 : replace number pattern by wildcards
 *      - 2 : replace number pattern by the number passed as number
 */
void
xvc_command_execute (const char *command, int flag, int number,
                     const char *file, int fframe, int lframe, int width,
                     int height, XVC_Fps fps)
{
#define DEBUGFUNCTION "xvc_command_execute()"
#define BUFLENGTH ((PATH_MAX * 5) + 1)
    char buf[BUFLENGTH];
    const char *myfile = NULL;
    int k;

#ifdef DEBUG
    printf ("%s %s: Entering with flag '%i'\n", DEBUGFILE, DEBUGFUNCTION, flag);
#endif     // DEBUG

    switch (flag) {
    case 0:
        myfile = file;
        break;
    case 1:
        {
            char file_buf[PATH_MAX + 1];
            const char *p = file;
            int i = 0, n = 0;

            while (*p && (p - file) <= PATH_MAX && (p - file) <= strlen (file)) {
                if (*p == '%') {
                    p++;
                    if (*p == '.')
                        p++;
                    sscanf (p, "%dd", &n);
                    if ((p - file + n) <= PATH_MAX
                        && (p - file) <= strlen (file)) {
                        while (n) {
                            file_buf[i++] = '?';
                            n--;
                        }
                    }
                    while (isdigit (*p))
                        p++;
                    p++;
                } else {
                    file_buf[i] = *p;
                    p++;
                    i++;
                }
            }
            file_buf[i] = '\0';

            myfile = strdup (file_buf);
        }
        break;
    case 2:
        if (xvc_is_filename_mutable (file)) {
            char lfile[PATH_MAX + 1];

            snprintf (lfile, (PATH_MAX + 1), file, number);
            myfile = strdup (lfile);
        } else {
            myfile = file;
        }
        break;
    default:
        break;
    }

    // add the required environment variables to xvidcap's environment
    // so the shell spawned inherits them
    for (k = 0; k < 7; k++) {
        switch (k) {
        case 0:
            snprintf (buf, BUFLENGTH, "%i", fframe);
            setenv ("XVFFRAME", buf, 1);
            break;
        case 1:
            snprintf (buf, BUFLENGTH, "%i", lframe);
            setenv ("XVLFRAME", buf, 1);
            break;
        case 2:
            snprintf (buf, BUFLENGTH, "%i", width);
            setenv ("XVWIDTH", buf, 1);
            break;
        case 3:
            snprintf (buf, BUFLENGTH, "%i", height);
            setenv ("XVHEIGHT", buf, 1);
            break;
        case 4:
            snprintf (buf, BUFLENGTH,
                      "%f", ((float) fps.num / (float) fps.den));
            setenv ("XVFPS", buf, 1);
            break;
        case 5:
            snprintf (buf, BUFLENGTH,
                      "%f",
                      ((float) 1000 / ((float) fps.num / (float) fps.den)));
            setenv ("XVTIME", buf, 1);
            break;
        case 6:
            snprintf (buf, BUFLENGTH, "%s", myfile);
            setenv ("XVFILE", buf, 1);
            break;
        }
    }

    snprintf (buf, BUFLENGTH, "%s", command);

#ifdef DEBUG
    printf ("%s %s: calling: '%s'\n", DEBUGFILE, DEBUGFUNCTION, buf);
#endif     // DEBUG
    system (buf);

    // sf-animate: flag 1
    // cmd: animate "${XVFILE}" -delay $((XVTIME/10)) &
    // sf-encode: flag 0
    // cmd: ppm2mpeg "${XVFILE}" ${XVFFRAME} ${XVLFRAME} ${XVWIDTH}
    // ${XVHEIGHT} ${XVFPS} ${XVTIME} &
    // sf-edit: flag 2
    // gimp "${XVFILE}" &

    // in case we constructed a filename pattern, we need to free it again
    if (myfile != file)
        free ((void *) myfile);

#ifdef DEBUG
    printf ("%s %s: Leaving\n", DEBUGFILE, DEBUGFUNCTION);
#endif     // DEBUG
#undef BUFLENGTH
#undef DEBUGFUNCTION
}

/**
 * \brief checks if filename has pattern to include frame - or movie number
 *
 * @param filename the filename to check
 * @return has pattern TRUE or FALSE
 */
Boolean
xvc_is_filename_mutable (const char *filename)
{
    char file[PATH_MAX + 1];

    snprintf (file, (PATH_MAX + 1), filename, 1);
    if (strcmp (file, filename) == 0) {
        return FALSE;
    } else {
        return TRUE;
    }
}

/**
 * \brief extract number of digits after the decimal point from a string
 *      containing a float number.
 *
 * The function returns e.g. 2 for 29.97 or 3 for 29.969. It takes a user's
 * locale into account, i. e. if a user has ',' as the decimal point, that
 * will be used.
 *
 * @param input string containing a string representation of a floating
 *      point number
 * @return an integer build of the float parsed to the second digit after
 *      the comma times 100
 */
int
xvc_get_number_of_fraction_digits_from_float_string (const char *input)
{
    int pre = 0, post = 0;
    char *ptr;

    pre = (int) strtol (input, &ptr, 10);

    if (*ptr != '\0')
        ptr++;
    while (*ptr != '\0') {
        post++;
        ptr++;
    }

    return post;
}

/**
 * \brief gets the current display and store it in the XVC_AppData struct
 */
static void
appdata_set_display (XVC_AppData * lapp)
{
#define DEBUGFUNCTION "appdata_set_display()"
#ifdef DEBUG
    printf ("%s %s: Entering with dpy %p\n", DEBUGFILE, DEBUGFUNCTION,
            lapp->dpy);
#endif     // DEBUG

    lapp->dpy = NULL;
    lapp->dpy = xvc_frame_get_capture_display ();
    if (!lapp->dpy) {
        char msg[256];

        snprintf (msg, 256, "%s %s: Can't get display to capture from!\n",
                  DEBUGFILE, DEBUGFUNCTION);
        perror (msg);
        return;
    }
    lapp->root_window = DefaultRootWindow (lapp->dpy);
    lapp->default_screen = XDefaultScreenOfDisplay (lapp->dpy);
    lapp->default_screen_num = XDefaultScreen (lapp->dpy);
    lapp->max_width = WidthOfScreen (lapp->default_screen);
    lapp->max_height = HeightOfScreen (lapp->default_screen);

#ifdef DEBUG
    printf ("%s %s: Leaving with dpy %p (%s)\n", DEBUGFILE, DEBUGFUNCTION,
            lapp->dpy, DisplayString (lapp->dpy));
#endif     // DEBUG
#undef DEBUGFUNCTION
}

/**
 * \brief retrieves the XWindowAttributes for the specified Window and stores
 *      them in the XVC_AppData struct.
 *
 * @param win a Window
 */
void
xvc_appdata_set_window_attributes (Window win)
{
#define DEBUGFUNCTION "xvc_appdata_set_window_attributes()"
    xvc_get_window_attributes (app->dpy, win, &(app->win_attr));
#undef DEBUGFUNCTION
}
