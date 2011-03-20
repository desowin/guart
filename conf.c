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
#include "conf.h"

static gchar *port_labels[] = {
    "/dev/ttyS0",
    "/dev/ttyS1",
    "/dev/ttyS2",
    "/dev/ttyS3",
};

/**
 * Baudrate labels, order must match BaudRate enum
 **/
static gchar *baud_labels[] = {
    "150",
    "300",
    "600",
    "1200",
    "1800",
    "2400",
    "4800",
    "9600",
    "19200",
    "38400",
    "57600",
    "115200",
};

/**
 * Databits labels, order must match DataBits enum
 **/
static gchar *databits_labels[] = {
    "5",
    "6",
    "7",
    "8",
};

/**
 * Parity labels, order must match Parity enum
 **/
static gchar *parity_labels[] = {
    "Even",
    "Odd",
    "None",
};

/**
 * Stopbits labels, order must match StopBits enum
 **/
static gchar *stopbits_labels[] = {
    "1",
    "2",
};

static gchar *terminator_labels[] = {
    "None",
    "LF",
    "CR",
    "CR LF",
    "Custom",
};

/**
 * Flow control labels, order must match FlowControl enum
 **/
static gchar *flow_labels[] = {
    "None",
    "RTS/CTS",
    "DTR/DSR",
    "XON/XOFF",
};

static void fill_combo_box(GtkWidget *cbox, gchar** strings, gint n)
{
    int i;
    for (i=0; i<n; i++)
    {
        gtk_combo_box_append_text(GTK_COMBO_BOX(cbox), strings[i]);
    }
}

static void add_to_table(GtkWidget *table, gint i, gchar *label, GtkWidget *widget)
{
    GtkWidget *lbl = gtk_label_new(label);
    gtk_table_attach_defaults(GTK_TABLE(table), lbl, 0, 1, i, i+1);
    gtk_table_attach_defaults(GTK_TABLE(table), widget, 1, 2, i, i+1);
}

static void add_to_box(GtkWidget *box, gchar *label, GtkWidget *widget)
{
    GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
    GtkWidget *lbl = gtk_label_new(label);

    gtk_box_pack_start(GTK_BOX(hbox), lbl, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);
}

void combo_box_changed_cb(GtkComboBox *widget, gint *data)
{
    *data = gtk_combo_box_get_active(widget);
}

void terminator_changed_cb(GtkComboBox *widget, Configuration *cfg)
{
    gint n = gtk_combo_box_get_active(widget);

    if (cfg->terminator)
    {
        g_free(cfg->terminator);
        cfg->terminator = NULL;
        cfg->n_terminator_chars = 0;
    }

    switch (n)
    {
        /* see terminator_labels */
        case 0: /* None */
            /* already cleared terminator and n_terminator_chars */
            break;
        case 1: /* LF */
            cfg->terminator = g_strdup_printf("%c", 0x0A);
            cfg->n_terminator_chars = 1;
            break;
        case 2: /* CR */
            cfg->terminator = g_strdup_printf("%c", 0x0D);
            cfg->n_terminator_chars = 1;
            break;
        case 3: /* CR LF */
            cfg->terminator = g_strdup_printf("%c%c", 0x0D, 0x0A);
            cfg->n_terminator_chars = 2;
            break;
        case 4: /* Custom */
            /* TODO */
            break;
        default:
            break;
    }
}

static void set_terminator_combo_box(GtkWidget *cbox_terminator, Configuration *cfg)
{
    switch (cfg->n_terminator_chars)
    {
        case 0:
            /* None */
            gtk_combo_box_set_active(GTK_COMBO_BOX(cbox_terminator), 0);
            break;
        case 1:
            if (cfg->terminator[0] == 0x0A) /* LF */
            {
                gtk_combo_box_set_active(GTK_COMBO_BOX(cbox_terminator), 1);
            }
            else if (cfg->terminator[0] == 0x0D) /* CR */
            {
                gtk_combo_box_set_active(GTK_COMBO_BOX(cbox_terminator), 2);
            }
            else
            {
                /* custom terminator */
                gtk_combo_box_set_active(GTK_COMBO_BOX(cbox_terminator), 4);
            }
            break;
        case 2:
            if (cfg->terminator[0] == 0x0D && cfg->terminator[1] == 0x0A)
            {
                /* CR LF */
                gtk_combo_box_set_active(GTK_COMBO_BOX(cbox_terminator), 3);
            }
            else
            {
                /* custom terminator */
                gtk_combo_box_set_active(GTK_COMBO_BOX(cbox_terminator), 4);
            }
            break;
        default:
            /* custom terminator */
            gtk_combo_box_set_active(GTK_COMBO_BOX(cbox_terminator), 4);
            break;
    }
}

static GtkWidget *create_configuration_table(Configuration *cfg)
{
    GtkWidget *cfg_table;
    GtkWidget *cbox_port, *cbox_baudrate, *vbox_format, *cbox_terminator, *cbox_flow;
    GtkWidget *cbox_databits, *cbox_parity, *cbox_stopbits;

    cfg_table = gtk_table_new(5, 2, FALSE);

    cbox_port = gtk_combo_box_entry_new_text();
    fill_combo_box(cbox_port, port_labels, G_N_ELEMENTS(port_labels));
    gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(cbox_port))),
                       cfg->port);
    g_object_set_data(G_OBJECT(cfg_table), "port", cbox_port);

    cbox_baudrate = gtk_combo_box_new_text();
    fill_combo_box(cbox_baudrate, baud_labels, G_N_ELEMENTS(baud_labels));

    vbox_format = gtk_vbox_new(FALSE, 0);
    cbox_databits = gtk_combo_box_new_text();
    fill_combo_box(cbox_databits, databits_labels, G_N_ELEMENTS(databits_labels));
    cbox_parity = gtk_combo_box_new_text();
    fill_combo_box(cbox_parity, parity_labels, G_N_ELEMENTS(parity_labels));
    cbox_stopbits = gtk_combo_box_new_text();
    fill_combo_box(cbox_stopbits, stopbits_labels, G_N_ELEMENTS(stopbits_labels));
    add_to_box(vbox_format, "Data bits:", cbox_databits);
    add_to_box(vbox_format, "Parity:", cbox_parity);
    add_to_box(vbox_format, "Stop bits:", cbox_stopbits);

    cbox_terminator = gtk_combo_box_new_text();
    fill_combo_box(cbox_terminator, terminator_labels, G_N_ELEMENTS(terminator_labels));
    set_terminator_combo_box(cbox_terminator, cfg);

    cbox_flow = gtk_combo_box_new_text();
    fill_combo_box(cbox_flow, flow_labels, G_N_ELEMENTS(flow_labels));

    add_to_table(cfg_table, 0, "Port:", cbox_port);
    add_to_table(cfg_table, 1, "Baudrate:", cbox_baudrate);
    add_to_table(cfg_table, 2, "Format:", vbox_format);
    add_to_table(cfg_table, 3, "Terminator:", cbox_terminator);
    add_to_table(cfg_table, 4, "Flow control:", cbox_flow);

    gtk_combo_box_set_active(GTK_COMBO_BOX(cbox_baudrate), cfg->rate);
    gtk_combo_box_set_active(GTK_COMBO_BOX(cbox_databits), cfg->databits);
    gtk_combo_box_set_active(GTK_COMBO_BOX(cbox_parity), cfg->parity);
    gtk_combo_box_set_active(GTK_COMBO_BOX(cbox_stopbits), cfg->stopbits);
    gtk_combo_box_set_active(GTK_COMBO_BOX(cbox_flow), cfg->flow);

    g_signal_connect(G_OBJECT(cbox_baudrate), "changed", G_CALLBACK(combo_box_changed_cb), &cfg->rate);
    g_signal_connect(G_OBJECT(cbox_databits), "changed", G_CALLBACK(combo_box_changed_cb), &cfg->databits);
    g_signal_connect(G_OBJECT(cbox_parity), "changed", G_CALLBACK(combo_box_changed_cb), &cfg->parity);
    g_signal_connect(G_OBJECT(cbox_stopbits), "changed", G_CALLBACK(combo_box_changed_cb), &cfg->stopbits);
    g_signal_connect(G_OBJECT(cbox_flow), "changed", G_CALLBACK(combo_box_changed_cb), &cfg->flow);

    g_signal_connect(G_OBJECT(cbox_terminator), "changed", G_CALLBACK(terminator_changed_cb), cfg);

    gtk_widget_show_all(cfg_table);

    return cfg_table;
}

Configuration default_config = {
    NULL,
    GUART_B115200,
    GUART_BITS8,
    GUART_PARITY_NONE,
    GUART_STOPBITS1,
    GUART_FLOW_NONE,
    NULL,
    0
};

static void configuration_copy(Configuration *dest, Configuration *src)
{
    memcpy((void*)dest, (void*)src, sizeof(Configuration));
    if (dest->port != NULL)
        dest->port = g_strdup(dest->port);

    if (dest->terminator != NULL)
        dest->terminator = g_strndup(dest->terminator, dest->n_terminator_chars);
}

Configuration *configuration_new()
{
    Configuration *conf = g_slice_new(Configuration);
    configuration_copy(conf, &default_config);

    return conf;
}

void configuration_free(Configuration *conf)
{
    if (conf->port != NULL)
        g_free(conf->port);

    if (conf->terminator != NULL)
        g_free(conf->terminator);

    g_slice_free(Configuration, conf);
}

gboolean configure(GtkWidget *parent, Configuration *cfg)
{
    GtkWidget *dialog;
    GtkWidget *cfg_table;
    GtkWidget *cbox_port;
    Configuration *cfg_new;

    cfg_new = configuration_new();
    configuration_copy(cfg_new, cfg);

    dialog = gtk_dialog_new_with_buttons("GUART configuration",
                                         GTK_WINDOW(parent),
                                         GTK_DIALOG_MODAL,
                                         GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                         NULL);

    cfg_table = create_configuration_table(cfg_new);

    gtk_box_pack_start_defaults(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                                cfg_table);

    gint result = gtk_dialog_run(GTK_DIALOG(dialog));

    cbox_port = GTK_WIDGET(g_object_get_data(G_OBJECT(cfg_table), "port"));
    if (cfg_new->port != NULL)
        g_free(cfg_new->port);
    cfg_new->port = gtk_combo_box_get_active_text(GTK_COMBO_BOX(cbox_port));

    gtk_widget_destroy(dialog);

    switch (result)
    {
        case GTK_RESPONSE_ACCEPT:
            g_object_set_data_full(G_OBJECT(parent), "cfg", cfg_new,
                                   (GDestroyNotify)configuration_free);
            return TRUE;
        case GTK_RESPONSE_REJECT:
        default:
            configuration_free(cfg_new);
            return FALSE;
    }
}

gchar *get_configuration_string(Configuration *cfg)
{
    gchar *tmp = g_strdup_printf("%s, %s %s/%c/%s, %s",
                                 cfg->port,
                                 baud_labels[cfg->rate],
                                 databits_labels[cfg->databits],
                                 parity_labels[cfg->parity][0],
                                 stopbits_labels[cfg->stopbits],
                                 flow_labels[cfg->flow]);
    return tmp;
}
