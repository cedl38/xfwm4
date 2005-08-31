/*      $Id$
 
        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2, or (at your option)
        any later version.
 
        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.
 
        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 
        xfwm4    - (c) 2002-2005 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define WIN_ICON_SIZE 48

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h> 
#include <libxfcegui4/libxfcegui4.h>
#include "inline-tabwin-icon.h"
#include "icons.h"
#include "focus.h"
#include "tabwin.h"

        
static gboolean
paint_selected(GtkWidget *w,  GdkEventExpose *event, gpointer data)
{
    gtk_paint_box(w->style, w->window,
                  GTK_STATE_SELECTED,
                  GTK_SHADOW_ETCHED_IN,
                  NULL, w, NULL, 
                  w->allocation.x-3,
                  w->allocation.y-3,
                  w->allocation.width+3,
                  w->allocation.height+3);
    return FALSE;
}

static void
tabwinSetLabel(Tabwin *t, gchar *label)
{
    gchar *markup;
    gchar *str;
    
    markup = g_strconcat ("<span size=\"larger\" weight=\"bold\">", label, "</span>", NULL);
    gtk_label_set_markup(GTK_LABEL(t->label), markup);
    gtk_widget_modify_fg (t->label, GTK_STATE_NORMAL, &t->label->style->fg[GTK_STATE_SELECTED]);
    
    g_free(markup);
}

static void
tabwinSetSelected(Tabwin *t, GtkWidget *w)
{
    Client *c;
    if (t->selected_callback)
        g_signal_handler_disconnect(t->current->data, t->selected_callback);
    t->selected_callback=g_signal_connect(G_OBJECT(w), "expose-event", G_CALLBACK(paint_selected), NULL);
    c = g_object_get_data(G_OBJECT(w), "client-ptr-val");
    
    tabwinSetLabel(t, c->name);
}

static GtkWidget *
createWindowIcon(Client *c)
{
    GdkPixbuf *icon_pixbuf;
    GtkWidget  *icon;

    icon_pixbuf = getAppIcon(c->screen_info->display_info, c->window, WIN_ICON_SIZE, WIN_ICON_SIZE);
    icon = gtk_image_new();
    g_object_set_data(G_OBJECT(icon), "client-ptr-val", c);
    if (icon_pixbuf)
        gtk_image_set_from_pixbuf(GTK_IMAGE(icon), icon_pixbuf);
    else
        gtk_image_set_from_stock(GTK_IMAGE(icon), "gtk-missing-image", GTK_ICON_SIZE_DIALOG);
    return icon;
}

static GtkWidget *
createWindowlist(GdkScreen *scr, Client *c, unsigned int cycle_range, Tabwin *t)
{
    GdkRectangle monitor_sz;
    gint monitor;
    GtkWidget *windowlist;
    GtkWidget  *icon;
    Client *c2 = NULL;
    ScreenInfo *screen_info;
    unsigned int grid_cols;
    unsigned int n_clients;
    unsigned int grid_rows;
    int i = 0, packpos = 0;
    
    /* calculate the wrapping */
    screen_info = c->screen_info;
    xfce_gdk_display_locate_monitor_with_pointer(screen_info->display_info->gdisplay, &monitor);
    gdk_screen_get_monitor_geometry (scr, monitor, &monitor_sz);
    /* add the width of the 7px border on each side */
    grid_cols   = (monitor_sz.width/(WIN_ICON_SIZE+14))*0.75;
    n_clients   = screen_info->client_count;
    grid_rows   = n_clients/grid_cols + 1;
    windowlist  = gtk_table_new(grid_rows, grid_cols, FALSE);

    t->grid_cols = grid_cols;
    t->grid_rows = grid_rows;
    /* pack the client icons */
    for (c2=c,i=0; c2 && i< n_clients; i++, c2=c2->next)
    {
        if (!clientSelectMask (c2, cycle_range, WINDOW_NORMAL | WINDOW_DIALOG | WINDOW_MODAL_DIALOG))
            continue;
        icon = createWindowIcon(c2);
        gtk_table_attach(GTK_TABLE(windowlist), GTK_WIDGET(icon), 
                         packpos%grid_cols, packpos%grid_cols+1,
                         packpos/grid_cols, packpos/grid_cols + 1,
                         GTK_FILL, GTK_FILL,
                         7,7);
        packpos++;        
        t->head = g_list_append(t->head, icon);
    }
    /*circlify the list for easier cycling*/
    t->head->prev = g_list_last(t->head);
    t->head->prev->next = t->head;

    t->current = t->head->next;
    tabwinSetSelected(t, t->current->data);
    return windowlist;
}

Tabwin *
tabwinCreate(GdkScreen *scr, Client *c, unsigned int cycle_range)
{
    static GdkPixbuf *logo = NULL;
    Tabwin *tabwin;
    GtkWidget *header;
    GtkWidget *frame;
    GtkWidget *vbox, *header_hbox;
    GtkWidget *windowlist;

    tabwin = g_new (Tabwin, 1);
    memset(tabwin, 0, sizeof(Tabwin));
    if (!logo)
    {
        logo = xfce_inline_icon_at_size (tabwin_icon_data, 32, 32);
        g_object_ref (G_OBJECT (logo));
    }

    tabwin->window = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_screen(GTK_WINDOW(tabwin->window), scr);
    gtk_container_set_border_width(GTK_CONTAINER(tabwin->window), 0);
    gtk_window_set_position (GTK_WINDOW (tabwin->window),
        GTK_WIN_POS_CENTER_ALWAYS);
    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 0);
    gtk_container_add (GTK_CONTAINER (tabwin->window), frame);

    vbox = gtk_vbox_new (FALSE, 5);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
    gtk_container_add (GTK_CONTAINER (frame), vbox);

    header = xfce_create_header (logo, _("Switch to:"));
    gtk_box_pack_start (GTK_BOX (vbox), header, FALSE, TRUE, 0);
    header_hbox = gtk_bin_get_child(GTK_BIN(header));
    tabwin->label = gtk_label_new(" ");
    gtk_widget_set_size_request (GTK_WIDGET(tabwin->label), 240, -1);
    if ((gtk_major_version == 2 && gtk_minor_version >= 6) || gtk_major_version > 2)
    {
#ifdef PANGO_ELLIPSIZE_END
	g_object_set (G_OBJECT (tabwin->label), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
#else
	g_object_set (G_OBJECT (tabwin->label), "ellipsize", 3, NULL);
#endif
    }
    
    gtk_box_pack_start(GTK_BOX(header_hbox), tabwin->label, TRUE, TRUE, 0);

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 0);
    gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

    windowlist = createWindowlist(scr, c, cycle_range, tabwin);
    tabwin->container = windowlist;
    gtk_container_add(GTK_CONTAINER(frame), windowlist);

    gtk_widget_show_all(tabwin->window);
    return tabwin;
}

void
tabwinRemoveClient(Tabwin *t, Client *c)
{
    GList *tmp;
    
    tmp = t->head;
    do {
        if (((Client *)g_object_get_data(tmp->data, "client-ptr-val")) == c)
        {
            if (tmp == t->current)
                tabwinSelectNext(t);    
            gtk_container_remove(GTK_CONTAINER(t->container), tmp->data);
            t->head = g_list_delete_link(t->head, tmp);
            TRACE("Removed Client\n");
        }
    /* since this is a circular list, hitting head signals end, not hitting NULL */
    } while ((tmp=tmp->next) && (tmp!=t->head));
    
}

void
tabwinSelectNext (Tabwin * t)
{
    tabwinSetSelected(t, t->current->next->data);
    t->current = t->current->next;
    gtk_widget_queue_draw(t->window);
}

void
tabwinSelectPrev (Tabwin *t)
{
    tabwinSetSelected(t, t->current->prev->data);
    t->current = t->current->prev;
    gtk_widget_queue_draw(t->window);    
}

void
tabwinDestroy (Tabwin * tabwin)
{
    g_return_if_fail (tabwin != NULL);

    /* un-circlify the list so we don't infinitely loop while freeing */
    tabwin->head->prev->next = NULL;
    tabwin->head->prev = NULL;
    g_list_free(tabwin->head);
    gtk_widget_destroy (tabwin->window);
}
