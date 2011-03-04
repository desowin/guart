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

#include <stdio.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <string.h>
#include <unistd.h>
#include <hex-document.h>
#include "guart.h"
#include "conf.h"
#include "serial.h"

static GtkWidget *window = NULL;
static GtkWidget *lbl_cfg;
static GtkWidget *btn_cfg;
static GtkWidget *btn_connect;
static GtkTextBuffer *databuffer;
#ifdef HAVE_LIBGTKHEX
static HexDocument *hexdocument;
#endif

static GtkWidget *txt_dtr, *txt_dsr, *txt_rts, *txt_cts;

static GIOChannel *serial_channel = NULL;
static guint serial_channel_source;
static int serial_fd;

void destroy(void)
{
    if (serial_channel != NULL)
    {
        g_source_remove(serial_channel_source);
        serial_channel = NULL;
    }

    gtk_main_quit();
}

static void cfg_button_cb(GtkButton *btn, gpointer data)
{
    configure(GTK_WIDGET(data), g_object_get_data(G_OBJECT(data), "cfg"));
    gchar *conf = get_configuration_string(g_object_get_data(G_OBJECT(data), "cfg"));
    g_message(conf);
    gtk_label_set_text(GTK_LABEL(lbl_cfg), conf);
    g_free(conf);
}

#define BUFF_SIZE 256

gboolean serial_read_cb(GIOChannel *source, GIOCondition condition, gpointer data)
{
    if (condition == G_IO_IN || condition == G_IO_PRI)
    {
        GtkTextIter iter;
        gchar c[BUFF_SIZE];
        gint bytes_read = read(serial_fd, c, BUFF_SIZE);

        gtk_text_buffer_get_end_iter(databuffer, &iter);
        gtk_text_buffer_insert(databuffer, &iter, c, bytes_read);

#ifdef HAVE_LIBGTKHEX
        hex_document_set_data(hexdocument, hexdocument->file_size,
                              bytes_read, 0 /* rep_len? */, (guchar*)c, FALSE);
#endif
    }
    
    return TRUE;
}

static void serial_detach_notify(gpointer data)
{

}

static inline void check_line_change(gchar current, gchar previous, GtkWidget *w)
{
    if (current != previous)
    {
        previous = current;
        gtk_label_set_text(GTK_LABEL(w), current ? "High" : "Low");
    }
}

/**
 *  Checks DTR, DSR, RTS and CTS lines for change.
 *  Updates appropriate labels (prev_dtr, prev_dsr, prev_rts, prev_cts) if needed.
 *  create_control_line_widgets() *must* be called prior to this function.
 *  Supposed to be called as idle source.
 *
 *  \return FALSE if not connected to any
 **/
static gboolean update_control_lines_cb(gpointer data)
{
    static gchar prev_dtr = -1, prev_dsr = -1, prev_rts = -1, prev_cts = -1;
    gchar dtr, dsr, rts, cts;

    if (serial_channel == NULL)
        return FALSE;

    if (get_control_lines(serial_fd, &dtr, &dsr, &rts, &cts) == FALSE)
        return FALSE;

    check_line_change(dtr, prev_dtr, txt_dtr);
    check_line_change(dsr, prev_dsr, txt_dsr);
    check_line_change(rts, prev_rts, txt_rts);
    check_line_change(cts, prev_cts, txt_cts);

    return TRUE;
}

static void connect_button_cb(GtkButton *btn, gpointer data)
{
    Configuration *cfg = g_object_get_data(G_OBJECT(data), "cfg");

    if (gtk_widget_is_sensitive(btn_cfg) == TRUE)
    {
        /* Connect to serial port */
        if (serial_channel)
        {
            g_io_channel_unref(serial_channel);
        }
        serial_channel = serial_connect(cfg, &serial_fd);

        if (serial_channel == NULL)
        {
            g_message("Unable to connect");
            return;
        }

        serial_channel_source =
            g_io_add_watch_full(serial_channel, G_PRIORITY_DEFAULT, G_IO_IN | G_IO_PRI,
                                serial_read_cb, NULL, serial_detach_notify);
        g_io_channel_unref(serial_channel);

        g_idle_add(update_control_lines_cb, NULL);
        
        gtk_widget_set_sensitive(btn_cfg, FALSE);
        gtk_button_set_label(btn, "Disconnect");
    }
    else
    {
        /* Disconnect from serial port */
        if (serial_channel != NULL)
        {
            g_source_remove(serial_channel_source);
            serial_channel = NULL;
        }
        gtk_widget_set_sensitive(btn_cfg, TRUE);
        gtk_button_set_label(btn, "Connect");
    }
}

static void send_button_cb(GtkButton *btn, GtkWidget *window)
{
    GtkWidget *entry = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "entry"));
    Configuration *cfg = (Configuration*)g_object_get_data(G_OBJECT(window), "cfg");

    if (serial_channel != NULL)
    {
        const gchar *entry_text = gtk_entry_get_text(GTK_ENTRY(entry));
        gint entry_text_length = strlen(entry_text);
        gint i;
        
        if (entry_text_length == 0)
        {
            return;
        }

        /* create send data buffer (includes terminator characters) */
        gchar *data = g_malloc(entry_text_length + cfg->n_terminator_chars);
        memcpy((void*)data, (void*)entry_text, entry_text_length);
        for (i = 0; i<cfg->n_terminator_chars; i++)
        {
            data[entry_text_length+i] = cfg->terminator[i];
        }

        i = write(serial_fd, data, entry_text_length + cfg->n_terminator_chars);
#ifdef DEBUG
        for (i=0;i<entry_text_length+cfg->n_terminator_chars;i++)
            printf("%02x ", data[i]);
        printf("\n");
        g_message("Wrote %d bytes", i);
#endif
        g_free(data);

        gtk_entry_set_text(GTK_ENTRY(entry), "");
    }
}

static gboolean
show_menu_cb(GtkWidget *widget, GdkEvent *event)
{
    GtkMenu *menu;

    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GTK_IS_MENU(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    if (serial_channel == NULL)
    {
        /* there's no point in showing menu if we're not connected to any tty */
        return FALSE;
    }
    
    menu = GTK_MENU (widget);
    if (event->type == GDK_BUTTON_PRESS)
    {
        GdkEventButton *event_button = (GdkEventButton *) event;
        if (event_button->button == 3)
        {
            gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
			               event_button->button, event_button->time);
	        return TRUE;
	    }
    }
    return FALSE;
}

static void status_line_menu_item_cb(GtkMenuItem *item, gpointer data)
{
    gint state = GPOINTER_TO_INT(data);
    void (*callback)(int fd, gchar state) = g_object_get_data(G_OBJECT(item), "callback");

    if (serial_channel == NULL)
    {
        /* not connected to any tty */
        return;
    }
    if (callback != NULL)
    {
        callback(serial_fd, (gchar)state);
    }
}

static void create_control_line_widget(GtkWidget *box, gchar *lbl,
                                       GtkWidget **widget, gchar *menu_lbl,
                                       void (*callback)(int fd, gchar state))
{
    GtkWidget *label;
    
    if (callback != NULL)
    {
        /* pack label inside GtkEventBox, so we can get button press event */
        label = gtk_event_box_new();
        GtkWidget *text = gtk_label_new(lbl);
        gtk_container_add(GTK_CONTAINER(label), text);
    }
    else
    {
        label = gtk_label_new(lbl);
    }
    *widget = gtk_label_new(NULL);

    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), *widget, FALSE, FALSE, 0);

    if (callback != NULL && menu_lbl != NULL)
    {
        /* prepare right click menu */
        GtkWidget *menu = gtk_menu_new();
        gchar *high = g_strdup_printf("Set %s high", menu_lbl);
        gchar *low = g_strdup_printf("Set %s low", menu_lbl);

        GtkWidget *i_high = gtk_menu_item_new_with_label(high);
        GtkWidget *i_low = gtk_menu_item_new_with_label(low);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), i_high);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), i_low);

        g_object_set_data(G_OBJECT(i_high), "callback", callback);
        g_signal_connect(i_high, "activate",
                         G_CALLBACK(status_line_menu_item_cb),
                         GINT_TO_POINTER(1));

        g_object_set_data(G_OBJECT(i_low), "callback", callback);
        g_signal_connect(i_low, "activate",
                         G_CALLBACK(status_line_menu_item_cb),
                         GINT_TO_POINTER(0));

        g_signal_connect_swapped(label, "button-press-event",
	                             G_CALLBACK(show_menu_cb), menu);
        
        g_free(high);
        g_free(low);

        /*
           show menu widgets now, so later just gtk_menu_popup() will suffice
        */
        gtk_widget_show_all(menu);
    }
}

static GtkWidget *create_control_line_widgets()
{
    GtkWidget *hbox = gtk_hbox_new(TRUE, 0);

    create_control_line_widget(hbox, "DTR:", &txt_dtr, "DTR", set_dtr);
    create_control_line_widget(hbox, "DSR:", &txt_dsr, NULL, NULL);
    create_control_line_widget(hbox, "RTS:", &txt_rts, "RTS", set_rts);
    create_control_line_widget(hbox, "CTS:", &txt_cts, NULL, NULL);

    return hbox;
}

int main(int argc, char *argv[]) {
    GtkWidget *vbox;
    GtkWidget *hbox_conf;
    GtkWidget *scrolled_window;
    GtkWidget *view;
#ifdef HAVE_LIBGTKHEX
    GtkWidget *hexview;
    GtkWidget *notebook;
#endif
    GtkWidget *hbox_input;
    GtkWidget *entry;
    GtkWidget *btn_send;
    GtkWidget *control_lines;
    gchar *cfg_text;
    
    Configuration *cfg = configuration_new();
    /* TODO: save last used settings */
    cfg->port = g_strdup("/dev/ttyUSB0");
    cfg->terminator = g_strdup_printf("%c", 0x0A); /* LF */
    cfg->n_terminator_chars = 1;

    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_size_request(window, 650, 500);
    gtk_signal_connect(GTK_OBJECT(window), "destroy",
                       GTK_SIGNAL_FUNC (destroy), NULL);

    g_object_set_data_full(G_OBJECT(window), "cfg", cfg, (GDestroyNotify)configuration_free);
    
    vbox = gtk_vbox_new(FALSE, 5);

    hbox_conf = gtk_hbox_new(FALSE, 5);
    
    cfg_text = get_configuration_string(cfg);
    lbl_cfg = gtk_label_new(cfg_text);
    g_free(cfg_text);
    
    btn_cfg = gtk_button_new_with_label("Configure");
    btn_connect = gtk_button_new_with_label("Connect");

    gtk_signal_connect(GTK_OBJECT(btn_cfg), "clicked",
		               GTK_SIGNAL_FUNC(cfg_button_cb), window);
    gtk_signal_connect(GTK_OBJECT(btn_connect), "clicked",
		               GTK_SIGNAL_FUNC(connect_button_cb), window);

    gtk_box_pack_start(GTK_BOX(hbox_conf), lbl_cfg, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_conf), btn_cfg, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_conf), btn_connect, FALSE, FALSE, 0);
    
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    databuffer = gtk_text_buffer_new(NULL);
    view = gtk_text_view_new_with_buffer(databuffer);
    /* TODO: make this configurable */
    PangoFontDescription *font_desc = pango_font_description_from_string("Monospace 10");
    gtk_widget_modify_font(GTK_WIDGET(view), font_desc);
    gtk_container_add(GTK_CONTAINER(scrolled_window), view);
    
#ifdef HAVE_LIBGTKHEX
    notebook = gtk_notebook_new();
    hexdocument = hex_document_new();
    hexview = hex_document_add_view(hexdocument);
#endif
    
    hbox_input = gtk_hbox_new(FALSE, 0);
    entry = gtk_entry_new();
    btn_send = gtk_button_new_with_label("Send");
    gtk_box_pack_start(GTK_BOX(hbox_input), entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_input), btn_send, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(window), "entry", entry);
    gtk_signal_connect(GTK_OBJECT(btn_send), "clicked",
                       GTK_SIGNAL_FUNC(send_button_cb), window);

    control_lines = create_control_line_widgets();

    gtk_box_pack_start(GTK_BOX(vbox), hbox_conf, FALSE, FALSE, 0);
#ifdef HAVE_LIBGTKHEX
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scrolled_window,
                             gtk_label_new("Text View"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), hexview,
                             gtk_label_new("Hex View"));
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);
#else
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
#endif
    gtk_box_pack_start(GTK_BOX(vbox), hbox_input, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), control_lines, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    gtk_widget_show_all(window);

    gtk_main();
    
    return 0;
}
