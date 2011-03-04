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

#include <gio/gio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include "serial.h"
#include "conf.h"

//typedef unsigned int	tcflag_t; /*FIXME*/


static tcflag_t get_cflag(Configuration *cfg)
{
    tcflag_t cflag;

    switch (cfg->rate)
    {
        case GUART_B150: cflag = B150; break;
        case GUART_B300: cflag = B300; break;
        case GUART_B600: cflag = B600; break;
        case GUART_B1200: cflag = B1200; break;
        case GUART_B1800: cflag = B1800; break;
        case GUART_B2400: cflag = B2400; break;
        case GUART_B4800: cflag = B4800; break;
        case GUART_B9600: cflag = B9600; break;
        case GUART_B19200: cflag = B19200; break;
        case GUART_B38400: cflag = B38400; break;
        case GUART_B57600: cflag = B57600; break;
        default:
            g_message("Invalid BaudRate, assuming 115200");
        case GUART_B115200: cflag = B115200; break;
    }

    switch (cfg->databits)
    {
        case GUART_BITS5: cflag |= CS5; break;
        case GUART_BITS6: cflag |= CS6; break;
        case GUART_BITS7: cflag |= CS7; break;
        default:
            g_message("Invalid DataBits, assuming 8");
        case GUART_BITS8: cflag |= CS8; break;
    }

#ifdef CRTSCTS
    if (cfg->flow == GUART_FLOW_RTSCTS)
    {
        cflag |= CRTSCTS;
    }
#endif
#ifdef CDTRDSR
    /* http://lkml.org/lkml/2008/8/6/478 */
    if (cfg->flow == GUART_FLOW_DTRDSR)
    {
        cflag |= CDTRDSR;
    }
#endif

    switch (cfg->stopbits)
    {
        default:
            g_message("Invalied StopBits, assuming 1");
        case GUART_STOPBITS1: break; /* it's default */
        case GUART_STOPBITS2: cflag |= CSTOPB;
    }

    switch (cfg->parity)
    {
        default:
            g_message("Invalid Parity, assuming None");
        case GUART_PARITY_NONE: break;
        case GUART_PARITY_EVEN: cflag |= PARENB; break;
        case GUART_PARITY_ODD: cflag |= PARENB | PARODD; break;
    }
    
    cflag |= CREAD; /* Enable receiver */

    /* TODO: make this configureable */
    cflag |= CLOCAL; /* Ignore modem control lines */

    return cflag;
}

GIOChannel *serial_connect(Configuration *cfg, int *serial_fd)
{
    GIOChannel *io;
    int fd;
    struct termios config;

    /* O_RDWR - Opens the port for reading and writing
     * O_NOCTTY - The port never becomes the controlling terminal of the process.
     * O_NDELAY - Use non-blocking I/O. On some systems this also means the RS232 DCD signal line is ignored.
     */
    fd = open(cfg->port, O_RDWR | O_NOCTTY);// | O_NDELAY);


    tcgetattr(fd, &config);
    
    if (fd < -1)
    {
        g_message("Unable to connect to %s: %s(%d)!", cfg->port, strerror(errno), errno);
        return NULL;
    }

    config.c_cflag = get_cflag(cfg);
    config.c_iflag = IGNPAR | IGNBRK;
    if (cfg->flow == GUART_FLOW_XONXOFF)
    {
        config.c_iflag |= IXON | IXOFF;
    }

    config.c_oflag = 0;
    config.c_lflag = 0;

    config.c_cc[VTIME] = 0;
 	config.c_cc[VMIN] = 1;


	if (tcsetattr(fd, TCSANOW, &config) < 0) {
		g_message("Can't change serial settings: %s(%d)", strerror(errno), errno);
		close(fd);
		return NULL;
	}

    tcflush(fd, TCOFLUSH);
    tcflush(fd, TCIFLUSH);

    io = g_io_channel_unix_new(fd);
    g_io_channel_set_close_on_unref(io, TRUE);

 	if (g_io_channel_set_flags(io, G_IO_FLAG_NONBLOCK, NULL) != G_IO_STATUS_NORMAL) {
		g_io_channel_unref(io);
		return NULL;
	}

    *serial_fd = fd;
	return io;
}

gboolean get_control_lines(int fd, gchar *dtr, gchar *dsr, gchar *rts, gchar *cts)
{
    int status;

    if (ioctl(fd, TIOCMGET, &status) == -1) {
        g_message("TIOCMGET failed");
        return FALSE;
    }

    if (status & TIOCM_DTR) *dtr = 1; else *dtr = 0;
    if (status & TIOCM_DSR) *dsr = 1; else *dsr = 0;
    if (status & TIOCM_RTS) *rts = 1; else *rts = 0;
    if (status & TIOCM_CTS) *cts = 1; else *cts = 0;

    return TRUE;
}

void set_rts(int fd, gchar state)
{
    int status;

    if (ioctl(fd, TIOCMGET, &status) == -1) {
        g_message("setRTS(): TIOCMGET failed");
        return;
    }

    if (state)
        status |= TIOCM_RTS;
    else
        status &= ~TIOCM_RTS;

    if (ioctl(fd, TIOCMSET, &status) == -1) {
        g_message("setRTS(): TIOCMSET failed");
    }
}

void set_dtr(int fd, gchar state)
{
    int status;

    if (ioctl(fd, TIOCMGET, &status) == -1) {
        g_message("setRTS(): TIOCMGET failed");
        return;
    }

    if (state)
        status |= TIOCM_DTR;
    else
        status &= ~TIOCM_DTR;

    if (ioctl(fd, TIOCMSET, &status) == -1) {
        g_message("setRTS(): TIOCMSET failed");
    }
}
