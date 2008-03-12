/**
 * \file gnome_warning.h
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

#ifndef _xvc_GNOME_WARNING_H__
#define _xvc_GNOME_WARNING_H__

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#include "app_data.h"
#endif     // DOXYGEN_SHOULD_SKIP_THIS

GtkWidget *xvc_create_warning_with_errors (XVC_ErrorListItem * elist,
                                           int from_where);

#endif     // _xvc_GNOME_WARNING_H__
