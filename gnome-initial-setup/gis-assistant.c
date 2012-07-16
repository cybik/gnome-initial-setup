/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */


#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gis-assistant.h"
#include "cc-notebook.h"

G_DEFINE_TYPE (GisAssistant, gis_assistant, GTK_TYPE_BOX)

#define GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GIS_TYPE_ASSISTANT, GisAssistantPrivate))

enum {
  PREPARE,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL];

struct _GisAssistantPrivate
{
  GtkWidget *notebook;
  GtkWidget *forward;
  GtkWidget *back;
  GtkWidget *main_layout;
  GtkWidget *action_area;

  GList *pages;
  GList *current_page;
};

typedef struct _PageData PageData;
struct _PageData
{
  GtkWidget *widget;
  gboolean page_complete;
  GList *link;
};

static PageData *
get_page_data_for_page (GtkWidget *page)
{
  return (PageData *) g_object_get_data (G_OBJECT (page), "gis-assistant-page-data");
}

static void
free_page_data (PageData *page_data)
{
  g_object_unref (page_data->widget);
  g_slice_free (PageData, page_data);
}

static PageData *
create_page_data_for_page (GtkWidget *page)
{
  PageData *page_data = g_slice_new0 (PageData);
  page_data->link = NULL;
  page_data->page_complete = FALSE;
  page_data->widget = g_object_ref (page);

  g_object_set_data_full (G_OBJECT (page), "gis-assistant-page-data",
                          page_data, (GDestroyNotify) free_page_data);

  return page_data;
}

void
gis_assistant_next_page (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = assistant->priv;
  cc_notebook_select_page (CC_NOTEBOOK (priv->notebook),
                           priv->current_page->next->data,
                           TRUE);
}

void
gis_assistant_previous_page (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = assistant->priv;
  cc_notebook_select_page (CC_NOTEBOOK (priv->notebook),
                           priv->current_page->prev->data,
                           TRUE);
}

static void
update_buttons_state (GisAssistant *assistant,
                      GtkWidget    *page)
{
  GisAssistantPrivate *priv = assistant->priv;
  gboolean can_go_backward, can_go_forward;

  can_go_backward = (priv->current_page->prev != NULL);
  gtk_widget_set_sensitive (priv->back, can_go_backward);

  can_go_forward = (priv->current_page->next != NULL) && gis_assistant_get_page_complete (assistant, page);
  gtk_widget_set_sensitive (priv->forward, can_go_forward);
}

static void
gis_assistant_prepare (GisAssistant *assistant,
                       GtkWidget    *page)
{
  update_buttons_state (assistant, page);
}

void
gis_assistant_add_page (GisAssistant *assistant,
                        GtkWidget    *page)
{
  GisAssistantPrivate *priv = assistant->priv;
  PageData *page_data = create_page_data_for_page (page);

  priv->pages = g_list_append (priv->pages, page);
  page_data->link = g_list_last (priv->pages);

  cc_notebook_add_page (CC_NOTEBOOK (priv->notebook), page);

  if (page_data->link->prev == priv->current_page)
    update_buttons_state (assistant, priv->current_page->data);
}

static void
go_forward (GtkWidget    *button,
            GisAssistant *assistant)
{
  gis_assistant_next_page (assistant);
}

static void
go_backward (GtkWidget    *button,
             GisAssistant *assistant)
{
  gis_assistant_previous_page (assistant);
}

void
gis_assistant_set_page_complete (GisAssistant *assistant,
                                 GtkWidget    *page,
                                 gboolean      complete)
{
  GisAssistantPrivate *priv = assistant->priv;
  PageData *page_data = get_page_data_for_page (page);

  page_data->page_complete = complete;

  if (page == priv->current_page->data)
    update_buttons_state (assistant, page);
}

gboolean
gis_assistant_get_page_complete (GisAssistant *assistant,
                                 GtkWidget    *page)
{
  PageData *page_data = get_page_data_for_page (page);
  return page_data->page_complete;
}

static void
current_page_changed (CcNotebook   *notebook,
                      GParamSpec   *pspec,
                      GisAssistant *assistant)
{
  GisAssistantPrivate *priv = assistant->priv;
  GtkWidget *page = cc_notebook_get_selected_page (notebook);
  PageData *page_data = get_page_data_for_page (page);
  GList *link = page_data->link;

  if (link == NULL) {
    g_warning ("%s: has no associated link", gtk_widget_get_name (page));
    return;
  }

  if (priv->current_page != link) {
    priv->current_page = link;
    g_signal_emit (assistant, signals[PREPARE], 0, priv->current_page->data);
  }
}

static void
gis_assistant_init (GisAssistant *assistant)
{
  GisAssistantPrivate *priv = GET_PRIVATE (assistant);
  assistant->priv = priv;

  priv->main_layout = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_box_pack_start (GTK_BOX (assistant), priv->main_layout, TRUE, TRUE, 0);

  priv->notebook = cc_notebook_new ();
  gtk_box_pack_start (GTK_BOX (priv->main_layout), priv->notebook, TRUE, TRUE, 0);

  priv->action_area = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (priv->main_layout), priv->action_area, FALSE, TRUE, 0);
  gtk_widget_set_halign (priv->action_area, GTK_ALIGN_END);

  priv->forward = gtk_button_new_with_mnemonic (_("_Next"));
  gtk_button_set_image (GTK_BUTTON (priv->forward),
                        gtk_image_new_from_stock (GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_can_default (priv->forward, TRUE);

  priv->back = gtk_button_new_with_mnemonic (_("_Back"));
  gtk_button_set_image (GTK_BUTTON (priv->back),
                        gtk_image_new_from_stock (GTK_STOCK_GO_BACK, GTK_ICON_SIZE_BUTTON));

  gtk_box_pack_start (GTK_BOX (priv->action_area), priv->back, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (priv->action_area), priv->forward, FALSE, FALSE, 0);

  g_signal_connect (priv->notebook, "notify::current-page",
                    G_CALLBACK (current_page_changed), assistant);

  g_signal_connect (priv->forward, "clicked",
                    G_CALLBACK (go_forward), assistant);

  g_signal_connect (priv->back, "clicked",
                    G_CALLBACK (go_backward), assistant);

  gtk_widget_show_all (GTK_WIDGET (assistant));
}

static void
free_page (GtkWidget *page)
{
  g_object_set_data (G_OBJECT (page), "gis-assistant-page-data", NULL);
}

static void
gis_assistant_finalize (GObject *gobject)
{
  GisAssistant *assistant = GIS_ASSISTANT (gobject);
  GisAssistantPrivate *priv = assistant->priv;

  priv->current_page = NULL;

  g_list_free_full (priv->pages, (GDestroyNotify) free_page);

  G_OBJECT_CLASS (gis_assistant_parent_class)->finalize (gobject);
}

static void
gis_assistant_class_init (GisAssistantClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GisAssistantPrivate));

  gobject_class->finalize = gis_assistant_finalize;

  klass->prepare = gis_assistant_prepare;

  /**
   * GisAssistant::prepare:
   * @assistant: the #GisAssistant
   * @page: the current page
   *
   * The ::prepare signal is emitted when a new page is set as the
   * assistant's current page, before making the new page visible.
   *
   * A handler for this signal can do any preparations which are
   * necessary before showing @page.
   */
  signals[PREPARE] =
    g_signal_new ("prepare",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GisAssistantClass, prepare),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_TYPE_WIDGET);
}
