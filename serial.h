/*
 *  Copyright (c) 2011 Tomasz Moń <desowin@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 */

#ifndef SERIAL_H
#define SERIAL_H

#include <gio/gio.h>
#include "conf.h"

GIOChannel *serial_connect(Configuration *cfg, int *serial_fd);
gboolean get_control_lines(int fd, gchar *dtr, gchar *dsr, gchar *rts, gchar *cts);
void set_rts(int fd, gchar state);
void set_dtr(int fd, gchar state);

#endif /* SERIAL_H */
