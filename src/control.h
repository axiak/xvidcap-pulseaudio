/* 
 * control.h
 *
 * Copyright (C) 2003,04,05,06 Karl H. Beckers, Frankfurt
 * EMail: khb@jarre-de-the.net
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

#ifndef __XVC_CONTROL_H__
#define __XVC_CONTROL_H__



#include <X11/Intrinsic.h>
#include "xv_error_item.h"



// the following defines the xvc interface for UIs
// they need to be implemented by any GUI anybody wants to add in the
// future. Look at gnome_ui.[c|h] for how to do that

Boolean xvc_init_pre(int argc, char **argv);
Boolean xvc_ui_create();
Boolean xvc_frame_create();
Boolean xvc_ui_init(xvErrorListItem * errors);
int xvc_ui_run(void);


void xvc_idle_add(void *, void *);
Boolean xvc_change_filename_display(int pic_no);
void xvc_frame_change(int x, int y, int width, int height,
                      Boolean reposition_control);

Boolean xvc_capture_stop();
void xvc_capture_start();


Boolean xvc_read_options_file();
Boolean xvc_write_options_file();
void xvc_check_start_options();


Boolean xvc_frame_monitor(int measured_time);




#endif                          // __XVC_CONTROL_H__
