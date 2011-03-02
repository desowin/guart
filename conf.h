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

#ifndef CONF_H
#define CONF_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

typedef enum {
    GUART_B150 = 0,
    GUART_B300,
    GUART_B600,
    GUART_B1200,
    GUART_B1800,
    GUART_B2400,
    GUART_B4800,
    GUART_B9600,
    GUART_B19200,
    GUART_B38400,
    GUART_B57600,
    GUART_B115200,
} BaudRate;

typedef enum {
    GUART_BITS5 = 0,
    GUART_BITS6,
    GUART_BITS7,
    GUART_BITS8,
} DataBits;

typedef enum {
    GUART_PARITY_EVEN = 0,
    GUART_PARITY_ODD,
    GUART_PARITY_NONE,
} Parity;

typedef enum {
    GUART_STOPBITS1 = 0,
    GUART_STOPBITS2,
} StopBits;

typedef enum {
    GUART_FLOW_NONE = 0,
    GUART_FLOW_RTSCTS,
    GUART_FLOW_DTRDSR,
    GUART_FLOW_XONXOFF,
} FlowControl;

typedef struct {
    gchar *port;
    BaudRate rate;
    DataBits databits;
    Parity parity;
    StopBits stopbits;
    FlowControl flow;
    gchar *terminator;
    gint n_terminator_chars;
} Configuration;

Configuration *configuration_new();
void configuration_free(Configuration *conf);
gboolean configure(GtkWidget *parent, Configuration *cfg);
gchar *get_configuration_string(Configuration *cfg);

#endif /* CONF_H */