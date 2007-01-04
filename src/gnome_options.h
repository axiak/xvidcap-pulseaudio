/**
 * \file gtk2_options.h
 */

/*
 * Copyright (C) 2003-07 Karl H. Beckers, Frankfurt
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

#ifndef __GNOME_OPTIONS_H__
#define __GNOME_OPTIONS_H__

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#include <gtk/gtk.h>
#include "app_data.h"
#endif // DOXYGEN_SHOULD_SKIP_THIS

void xvc_create_pref_dialog (XVC_AppData * lapp);
void xvc_pref_do_OK ();
void xvc_pref_reset_OK_attempts ();

#endif     // __GNOME_OPTIONS_H__
