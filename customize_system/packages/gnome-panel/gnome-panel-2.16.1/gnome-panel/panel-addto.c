/*
 * panel-addto.c:
 *
 * Copyright (C) 2004 Vincent Untz
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
 * Authors:
 *	Vincent Untz <vincent@vuntz.net>
 */

#include <config.h>
#include <string.h>

#include <libbonobo.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include "panel-addto.h"
#include "panel-addto-canvas.h"

#define PANEL_ADDTO_CANVAS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_ADDTO_TYPE_CANVAS, PanelAddtoCanvasPrivate))
	
static GQuark panel_addto_dialog_quark = 0;

typedef struct EntryInfo {
       PanelAddtoCanvas *canvas;

       GnomeCanvasGroup     *group;
       GnomeCanvasItem      *text;
       GnomeCanvasItem      *pixbuf;
       GnomeCanvasItem      *highlight_pixbuf;
       GnomeCanvasItem      *cover;
       GnomeCanvasItem      *selection;

       double icon_height;
       double icon_width;
       double text_height;
       guint  launching : 1;
       guint  selected : 1;
       guint  highlighted : 1;

       gint n_category;
       gint n_entry;
       gint index;
} EntryInfo;

typedef struct CategoryInfo {
       GnomeCanvasGroup *group;
       GnomeCanvasItem  *title;
       GnomeCanvasItem  *line;
} CategoryInfo;

static PanelAddtoItemInfo internal_addto_items [] = {

        { PANEL_ADDTO_MENU,
	  PANEL_ACTION_NONE,
          N_("Utilities"),
          "Utilities",
	  N_("Main Menu"),
	  N_("The main GNOME menu"),
	  PANEL_MAIN_MENU_ICON,
	  NULL,
	  NULL,
	  NULL,
	  "MENU:MAIN",
	  TRUE },

	{ PANEL_ADDTO_MENUBAR,
	  PANEL_ACTION_NONE,
	  N_("Utilities"),
	  "Utilities",
	  N_("Menu Bar"),
	  N_("A custom menu bar"),
	  PANEL_GNOME_LOGO_ICON,
	  NULL,
	  NULL,
	  NULL,
	  "MENUBAR:NEW",
	  TRUE },

	{ PANEL_ADDTO_SEPARATOR,
	  PANEL_ACTION_NONE,
	  N_("Utilities"),
	  "Utilities",
	  N_("Separator"),
	  N_("A separator to organize the panel items"),
	  PANEL_SEPARATOR_ICON,
	  NULL,
	  NULL,
	  NULL,
	  "SEPARATOR:NEW",
	  TRUE },

	{ PANEL_ADDTO_DRAWER,
	  PANEL_ACTION_NONE,
	  N_("Desktop &amp; Windows"),
	  "Desktop &amp; Windows",
	  N_("Drawer"),
	  N_("A pop out drawer to store other items in"),
	  PANEL_DRAWER_ICON,
	  NULL,
	  NULL,
	  NULL,
	  "DRAWER:NEW",
	  TRUE }
};

static const char applet_requirements [] =
	"has_all (repo_ids, ['IDL:Bonobo/Control:1.0',"
	"		     'IDL:GNOME/Vertigo/PanelAppletShell:1.0']) && "
	"defined (panel:icon)";

static char *applet_sort_criteria [] = {
	"name",
	NULL
	};

enum {
	COLUMN_ICON,
	COLUMN_TEXT,
	COLUMN_DATA,
	COLUMN_SEARCH,
	NUMBER_COLUMNS
};

enum {
	PANEL_ADDTO_RESPONSE_BACK,
	PANEL_ADDTO_RESPONSE_ADD
};

static void panel_addto_present_applications (PanelAddtoDialog *dialog);
static void panel_addto_present_applets      (PanelAddtoDialog *dialog);
static void panel_addto_init_categories      (PanelAddtoDialog *dialog);

static int
panel_addto_applet_info_sort_func (PanelAddtoItemInfo *a,
				   PanelAddtoItemInfo *b)
{
	return g_utf8_collate (a->name, b->name);
}

static GSList *
panel_addto_prepend_internal_applets (GSList *list)
{
	static gboolean translated = FALSE;
	int             i;

	for (i = 0; i < G_N_ELEMENTS (internal_addto_items); i++) {
		if (!translated) {
			internal_addto_items [i].name        = _(internal_addto_items [i].name);
			internal_addto_items [i].description = _(internal_addto_items [i].description);
			internal_addto_items [i].category    = _(internal_addto_items [i].category);
		}

		if (internal_addto_items [i].type == PANEL_ADDTO_MENU ||
		    internal_addto_items [i].type == PANEL_ADDTO_MENUBAR) {
			const char *logo;

			logo = panel_get_distributor_logo ();
			if (logo != NULL)
				internal_addto_items [i].icon = logo;
		}

                list = g_slist_prepend (list, &internal_addto_items [i]);
        }

	translated = TRUE;

	for (i = PANEL_ACTION_LOCK; i < PANEL_ACTION_LAST; i++) {
		PanelAddtoItemInfo *info;

		if (panel_action_get_is_disabled (i))
			continue;

		info                   = g_new0 (PanelAddtoItemInfo, 1);
		info->type             = PANEL_ADDTO_ACTION;
		info->action_type      = i;
		info->name             = g_strdup (panel_action_get_text (i));
		info->description      = g_strdup (panel_action_get_tooltip (i));
		info->category         = g_strdup (panel_action_get_category (i));
		info->english_category = g_strdup (panel_action_get_english_category (i));
		info->icon             = g_strdup (panel_action_get_icon_name (i));
		info->iid              = g_strdup (panel_action_get_drag_id (i));
		info->static_data      = FALSE;

		list = g_slist_prepend (list, info);
	}

        return list;
}

static char *
panel_addto_make_text (const char *name,
		       const char *desc)
{
	const char *real_name;
	char       *result; 

	real_name = name ? name : _("(empty)");

	if (!string_empty (desc)) {
		result = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>\n%s",
						  real_name, desc);
	} else {
		result = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>",
						  real_name);
	}

	return result;
}

#define ICON_SIZE 32
#define LAUNCHER_ICON_SIZE 20

static GdkPixbuf *
panel_addto_make_pixbuf (const char *filename)
{
	//FIXME: size shouldn't be fixed but should depend on the font size
	return panel_load_icon (gtk_icon_theme_get_default (),
				filename,
				ICON_SIZE, ICON_SIZE, ICON_SIZE,
				NULL);
}

static GdkPixbuf *
panel_addto_make_pixbuf_for_launcher (const char *filename)
{
	//FIXME: size shouldn't be fixed but should depend on the font size
	return panel_load_icon (gtk_icon_theme_get_default (),
				filename,
				LAUNCHER_ICON_SIZE, LAUNCHER_ICON_SIZE, LAUNCHER_ICON_SIZE,
				NULL);
}

static void  
panel_addto_applications_drag_data_get_cb (GtkWidget        *widget,
			      GdkDragContext   *context,
			      GtkSelectionData *selection_data,
			      guint             info,
			      guint             time,
			      const char       *string)
{
	gtk_selection_data_set (selection_data,
				selection_data->target, 8, (guchar *) string,
				strlen (string));
}

static void
panel_addto_applications_drag_begin_cb (GtkWidget      *widget,
			   GdkDragContext *context,
			   gpointer        data)
{
	GtkTreeModel *model;
	GtkTreePath  *path;
	GtkTreeIter   iter;
	GdkPixbuf    *pixbuf;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	   
	gtk_tree_view_get_cursor (GTK_TREE_VIEW (widget), &path, NULL);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	gtk_tree_model_get (model, &iter,
	                    COLUMN_ICON, &pixbuf,
	                    -1);

	gtk_drag_set_icon_pixbuf (context, pixbuf, 0, 0);
	g_object_unref (pixbuf);
}

static void
panel_addto_applications_setup_drag (GtkTreeView          *tree_view,
			const GtkTargetEntry *target,
			const char           *text)
{
	if (!text || panel_lockdown_get_locked_down ())
		return;
	
	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (tree_view),
						GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
						target, 1, GDK_ACTION_COPY);
	
	g_signal_connect_data (G_OBJECT (tree_view), "drag_data_get",
			       G_CALLBACK (panel_addto_applications_drag_data_get_cb),
			       g_strdup (text),
			       (GClosureNotify) g_free,
			       0 /* connect_flags */);
	g_signal_connect_after (G_OBJECT (tree_view), "drag-begin",
	                        G_CALLBACK (panel_addto_applications_drag_begin_cb),
	                        NULL);
}

static void
panel_addto_applications_setup_launcher_drag (GtkTreeView *tree_view,
				 const char  *uri)
{
        static GtkTargetEntry target[] = {
		{ "text/uri-list", 0, 0 }
	};
	char *uri_list;

	uri_list = g_strconcat (uri, "\r\n", NULL);
	panel_addto_applications_setup_drag (tree_view, target, uri_list);
	g_free (uri_list);
}

static void
panel_addto_applications_setup_internal_applet_drag (GtkTreeView *tree_view,
					const char  *applet_type)
{
	static GtkTargetEntry target[] = {
		{ "application/x-panel-applet-internal", 0, 0 }
	};

	panel_addto_applications_setup_drag (tree_view, target, applet_type);
}

static GSList *
panel_addto_query_applets (GSList *list)
{
	Bonobo_ServerInfoList *applet_list;
	CORBA_Environment   env;
	const char * const *langs;
	GSList             *langs_gslist;
	int                 i;

	CORBA_exception_init (&env);

	applet_list = bonobo_activation_query (applet_requirements,
					       applet_sort_criteria,
					       &env);
	if (BONOBO_EX (&env)) {
		g_warning (_("query returned exception %s\n"),
			   BONOBO_EX_REPOID (&env));

		CORBA_exception_free (&env);
		CORBA_free (applet_list);

		return NULL;
	}

	CORBA_exception_free (&env);

	langs = g_get_language_names ();

	langs_gslist = NULL;
	for (i = 0; langs[i]; i++)
		langs_gslist = g_slist_prepend (langs_gslist, (char *) langs[i]);

	langs_gslist = g_slist_reverse (langs_gslist);

	for (i = 0; i < applet_list->_length; i++) {
		Bonobo_ServerInfo *info;
		const char *name, *description, *category, *english_category, *icon;
		PanelAddtoItemInfo *applet;

		info = &applet_list->_buffer[i];

		name = bonobo_server_info_prop_lookup             (info,
								   "name",
								   langs_gslist);
		description = bonobo_server_info_prop_lookup      (info,
								   "description",
								   langs_gslist);
		category = bonobo_server_info_prop_lookup         (info,
								   "panel:category",
								   langs_gslist);
		english_category = bonobo_server_info_prop_lookup (info,
								   "panel:category",
								   NULL);
		icon = bonobo_server_info_prop_lookup             (info,
								   "panel:icon",
								   NULL);

		if (!name ||
		    panel_lockdown_is_applet_disabled (info->iid)) {
			continue;
		}

		applet = g_new0 (PanelAddtoItemInfo, 1);
		applet->type = PANEL_ADDTO_APPLET;
		applet->name = g_strdup (name);
		applet->description = g_strdup (description);
		applet->icon = g_strdup (icon);
		applet->iid = g_strdup (info->iid);
		applet->static_data = FALSE;
		if (!category) {
			applet->category = g_strdup_printf (_("Miscellaneous"));			
			applet->english_category = g_strdup_printf ("Miscellaneous");			
		} else {
			applet->category = g_markup_escape_text (category, -1);
			applet->english_category = g_markup_escape_text (english_category, -1);
		}
		list = g_slist_prepend (list, applet);
	}

	g_slist_free (langs_gslist);
	CORBA_free (applet_list);

	return list;
}

static void
panel_addto_append_item (PanelAddtoDialog *dialog,
			 GtkListStore *model,
			 PanelAddtoItemInfo *applet)
{
	char *text;
	GdkPixbuf *pixbuf;
	GtkTreeIter iter;

	if (applet == NULL) {
		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter,
				    COLUMN_ICON, NULL,
				    COLUMN_TEXT, NULL,
				    COLUMN_DATA, NULL,
				    COLUMN_SEARCH, NULL,
				    -1);
	} else {
		pixbuf = NULL;

		if (applet->icon != NULL) {
			pixbuf = panel_addto_make_pixbuf (applet->icon);
		}

		gtk_list_store_append (model, &iter);

		text = panel_addto_make_text (applet->name,
					      applet->description);

		gtk_list_store_set (model, &iter,
				    COLUMN_ICON, pixbuf,
				    COLUMN_TEXT, text,
				    COLUMN_DATA, applet,
				    COLUMN_SEARCH, applet->name,
				    -1);

		if (pixbuf)
			g_object_unref (pixbuf);

		g_free (text);
	}
}

static GtkTreeModel *
panel_addto_make_applet_model (PanelAddtoDialog *dialog)
{
	GtkListStore *model;
	GSList       *l;

	if (panel_profile_id_lists_are_writable ()) {
		dialog->applet_list = panel_addto_query_applets (dialog->applet_list);
		dialog->applet_list = panel_addto_prepend_internal_applets (dialog->applet_list);
	}

	dialog->applet_list = g_slist_sort (dialog->applet_list,
					    (GCompareFunc) panel_addto_applet_info_sort_func);

	model = gtk_list_store_new (NUMBER_COLUMNS,
				    GDK_TYPE_PIXBUF,
				    G_TYPE_STRING,
				    G_TYPE_POINTER,
				    G_TYPE_STRING);

	if (panel_profile_id_lists_are_writable () && dialog->applet_list) {
			panel_addto_append_item (dialog, model, NULL);
	}

	for (l = dialog->applet_list; l; l = l->next)
		panel_addto_append_item (dialog, model, l->data);

	return (GtkTreeModel *) model;
}

static void panel_addto_make_application_list (GSList             **parent_list,
					       GMenuTreeDirectory  *directory,
					       const char          *filename);

static void
panel_addto_prepend_directory (GSList             **parent_list,
			       GMenuTreeDirectory  *directory,
			       const char          *filename)
{
	PanelAddtoAppList *data;

	data = g_new0 (PanelAddtoAppList, 1);

	data->item_info.type          = PANEL_ADDTO_MENU;
	data->item_info.name          = g_strdup (gmenu_tree_directory_get_name (directory));
	data->item_info.description   = g_strdup (gmenu_tree_directory_get_comment (directory));
	data->item_info.icon          = g_strdup (gmenu_tree_directory_get_icon (directory));
	data->item_info.menu_filename = g_strdup (filename);
	data->item_info.menu_path     = gmenu_tree_directory_make_path (directory, NULL);
	data->item_info.static_data   = FALSE;

	/* We should set the iid here to something and do
	 * iid = g_strdup_printf ("MENU:%s", tfr->name)
	 * but this means we'd have to free the iid later
	 * and this would complexify too much the free
	 * function.
	 * So the iid is built when we select the row.
	 */

	*parent_list = g_slist_prepend (*parent_list, data);
			
	panel_addto_make_application_list (&data->children, directory, filename);
}

static void
panel_addto_prepend_entry (GSList         **parent_list,
			   GMenuTreeEntry  *entry,
			   const char      *filename)
{
	PanelAddtoAppList *data;

	data = g_new0 (PanelAddtoAppList, 1);

	data->item_info.type          = PANEL_ADDTO_LAUNCHER;
	data->item_info.name          = g_strdup (gmenu_tree_entry_get_name (entry));
	data->item_info.description   = g_strdup (gmenu_tree_entry_get_comment (entry));
	data->item_info.icon          = g_strdup (gmenu_tree_entry_get_icon (entry));
	data->item_info.launcher_path = g_strdup (gmenu_tree_entry_get_desktop_file_path (entry));
	data->item_info.static_data   = FALSE;

	*parent_list = g_slist_prepend (*parent_list, data);
}

static void
panel_addto_prepend_alias (GSList         **parent_list,
			   GMenuTreeAlias  *alias,
			   const char      *filename)
{
	GMenuTreeItem *aliased_item;

	aliased_item = gmenu_tree_alias_get_item (alias);

	switch (gmenu_tree_item_get_type (aliased_item)) {
	case GMENU_TREE_ITEM_DIRECTORY:
		panel_addto_prepend_directory (parent_list,
					       GMENU_TREE_DIRECTORY (aliased_item),
					       filename);
		break;

	case GMENU_TREE_ITEM_ENTRY:
		panel_addto_prepend_entry (parent_list,
					   GMENU_TREE_ENTRY (aliased_item),
					   filename);
		break;

	default:
		break;
	}

	gmenu_tree_item_unref (aliased_item);
}

static void
panel_addto_make_application_list (GSList             **parent_list,
				   GMenuTreeDirectory  *directory,
				   const char          *filename)
{
	GSList *items;
	GSList *l;

	items = gmenu_tree_directory_get_contents (directory);

	for (l = items; l; l = l->next) {
		switch (gmenu_tree_item_get_type (l->data)) {
		case GMENU_TREE_ITEM_DIRECTORY:
			panel_addto_prepend_directory (parent_list, l->data, filename);
			break;

		case GMENU_TREE_ITEM_ENTRY:
			panel_addto_prepend_entry (parent_list, l->data, filename);
			break;

		case GMENU_TREE_ITEM_ALIAS:
			panel_addto_prepend_alias (parent_list, l->data, filename);
			break;

		default:
			break;
		}

		gmenu_tree_item_unref (l->data);
	}

	g_slist_free (items);

	*parent_list = g_slist_reverse (*parent_list);
}

static void
panel_addto_populate_application_model (GtkTreeStore *store,
					GtkTreeIter  *parent,
					GSList       *app_list)
{
	PanelAddtoAppList *data;
	GtkTreeIter        iter;
	char              *text;
	GdkPixbuf         *pixbuf;
	GSList            *app;

	for (app = app_list; app != NULL; app = app->next) {
		data = app->data;
		gtk_tree_store_append (store, &iter, parent);

		text = panel_addto_make_text (data->item_info.name,
					      data->item_info.description);
		pixbuf = panel_addto_make_pixbuf (data->item_info.icon);
		gtk_tree_store_set (store, &iter,
				    COLUMN_ICON, pixbuf,
				    COLUMN_TEXT, text,
				    COLUMN_DATA, &(data->item_info),
				    COLUMN_SEARCH, data->item_info.name,
				    -1);

		if (pixbuf)
			g_object_unref (pixbuf);

		g_free (text);

		if (data->children != NULL) 
			panel_addto_populate_application_model (store,
								&iter,
								data->children);
	}
}

static GtkTreeModel *
panel_addto_make_application_model (PanelAddtoDialog *dialog)
{
	GtkTreeStore      *store;
	GMenuTree          *tree;
	GMenuTreeDirectory *root;

	store = gtk_tree_store_new (NUMBER_COLUMNS,
				    GDK_TYPE_PIXBUF,
				    G_TYPE_STRING,
				    G_TYPE_POINTER,
				    G_TYPE_STRING);

	tree = gmenu_tree_lookup ("applications.menu", GMENU_TREE_FLAGS_NONE);

	if ((root = gmenu_tree_get_root_directory (tree))) {
		panel_addto_make_application_list (&dialog->application_list,
						   root, "applications.menu");
		panel_addto_populate_application_model (store, NULL, dialog->application_list);

		gmenu_tree_item_unref (root);
	}

	gmenu_tree_unref (tree);

	tree = gmenu_tree_lookup ("settings.menu", GMENU_TREE_FLAGS_NONE);

	if ((root = gmenu_tree_get_root_directory (tree))) {
		GtkTreeIter iter;

		gtk_tree_store_append (store, &iter, NULL);
		gtk_tree_store_set (store, &iter,
				    COLUMN_ICON, NULL,
				    COLUMN_TEXT, NULL,
				    COLUMN_DATA, NULL,
				    COLUMN_SEARCH, NULL,
				    -1);

		panel_addto_make_application_list (&dialog->settings_list,
						   root, "settings.menu");
		panel_addto_populate_application_model (store, NULL,
							dialog->settings_list);

		gmenu_tree_item_unref (root);
	}

	gmenu_tree_unref (tree);

	return GTK_TREE_MODEL (store);
}

gboolean
panel_addto_add_item (PanelAddtoDialog   *dialog,
	 	      PanelAddtoItemInfo *item_info)
{
	gboolean destroy;
	
	g_assert (item_info != NULL);

	destroy = TRUE;

	switch (item_info->type) {
	case PANEL_ADDTO_APPLET:
		panel_applet_frame_create (dialog->panel_widget->toplevel,
					   dialog->insertion_position,
					   item_info->iid);
		break;
	case PANEL_ADDTO_ACTION:
		panel_action_button_create (dialog->panel_widget->toplevel,
					    dialog->insertion_position,
					    item_info->action_type);
		break;
	case PANEL_ADDTO_LAUNCHER_MENU:
		panel_addto_present_applications (dialog);
		destroy = FALSE;
		break;
	case PANEL_ADDTO_LAUNCHER:
		panel_launcher_create (dialog->panel_widget->toplevel,
				       dialog->insertion_position,
				       item_info->launcher_path);
		break;
	case PANEL_ADDTO_LAUNCHER_NEW:
		ask_about_launcher (NULL, dialog->panel_widget,
				    dialog->insertion_position, FALSE);
		break;
	case PANEL_ADDTO_MENU:
		panel_menu_button_create (dialog->panel_widget->toplevel,
					  dialog->insertion_position,
					  item_info->menu_filename,
					  item_info->menu_path,
					  item_info->menu_path != NULL,
					  item_info->name);
		break;
	case PANEL_ADDTO_MENUBAR:
		panel_menu_bar_create (dialog->panel_widget->toplevel,
				       dialog->insertion_position);
		break;
	case PANEL_ADDTO_SEPARATOR:
		panel_separator_create (dialog->panel_widget->toplevel,
					dialog->insertion_position);
		break;
	case PANEL_ADDTO_DRAWER:
		panel_drawer_create (dialog->panel_widget->toplevel,
				     dialog->insertion_position,
				     NULL, FALSE, NULL);
		break;
	}

	return destroy;
}

void
panel_addto_dialog_response (GtkWidget *widget_dialog,
			     guint response_id,
			     PanelAddtoDialog *dialog)
{
	PanelAddtoItemInfo      *data;
	PanelAddtoCanvasPrivate *priv;
	PanelAddtoEntry         *selected_applet;
        GtkTreeSelection        *selected_application;
	GtkTreeModel            *model;
	GtkTreeIter              iter;

	switch (response_id) {
	case GTK_RESPONSE_HELP:
		panel_show_help (gtk_window_get_screen (GTK_WINDOW (dialog->addto_dialog)),
				 "user-guide.xml", "gospanel-15");
		break;

	case PANEL_ADDTO_RESPONSE_ADD:

                if (dialog->status == APPLETS) {
                        priv = PANEL_ADDTO_CANVAS_GET_PRIVATE (dialog->canvas);
                        selected_applet = priv->selected;
                        data = selected_applet->item_info;
                        if (data) {
				panel_addto_add_item (dialog, data);
			}
                } else if (dialog->status == APPLICATIONS) {
                        selected_application = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
                        if (gtk_tree_selection_get_selected (selected_application,
                                                             &model, &iter)) {
                                gtk_tree_model_get (model, &iter,
                                                    COLUMN_DATA, &data, -1);

                                if (data != NULL) {
                                        panel_addto_add_item (dialog, data);
                                }
                        }
                }

		break;


	case PANEL_ADDTO_RESPONSE_BACK:
		/* This response only happens when we're showing the
		 * application list and the user wants to go back to the
		 * applet list. */
		gtk_widget_hide (dialog->applications_sw);
                dialog->status = APPLETS;
                panel_addto_present_applets (dialog);
                gtk_widget_show_all  (dialog->applets_sw);
                gtk_widget_show (dialog->statuslabel);
                gtk_widget_show (dialog->search_entry);
                gtk_widget_show (dialog->search_label);
		gtk_widget_set_sensitive (dialog->launcher_button, TRUE);
		if (!panel_lockdown_get_disable_command_line ())
		  gtk_widget_set_sensitive (dialog->custom_launcher_button, TRUE);
		break;

	case GTK_RESPONSE_CLOSE:
		gtk_widget_destroy (widget_dialog);
		break;

	default:
		break;
	}
}

static void
panel_addto_add_launcher (PanelAddtoDialog *dialog)
{
  PanelAddtoEntry         *launcher_applet;
  PanelAddtoItemInfo      *data;

  launcher_applet = g_new (PanelAddtoEntry, 1);
  data = launcher_applet->item_info;
  data = g_new (PanelAddtoItemInfo, 1);
  data->type = PANEL_ADDTO_LAUNCHER_MENU;

  panel_addto_add_item (dialog, data);

  g_free (launcher_applet);
  g_free (data);
}

static void
panel_addto_add_custom_launcher (PanelAddtoDialog *dialog)
{
  PanelAddtoEntry         *launcher_applet;
  PanelAddtoItemInfo      *data;

  launcher_applet = g_new (PanelAddtoEntry, 1);
  data = launcher_applet->item_info;
  data = g_new (PanelAddtoItemInfo, 1);
  data->type = PANEL_ADDTO_LAUNCHER_NEW;

  panel_addto_add_item (dialog, data);

  g_free (launcher_applet);
  g_free (data);
}

static void
panel_addto_dialog_destroy (GtkWidget *widget_dialog,
			    PanelAddtoDialog *dialog)
{
	panel_toplevel_pop_autohide_disabler (PANEL_TOPLEVEL (dialog->panel_widget->toplevel));
	g_object_set_qdata (G_OBJECT (dialog->panel_widget->toplevel),
			    panel_addto_dialog_quark,
			    NULL);
}


static PanelAddtoCategory *
panel_addto_category_new (const gchar *title,
                         const gchar *english_title,
                          gboolean     real_category)
{
       PanelAddtoCategory *retval;
       retval = g_new0 (PanelAddtoCategory, 1);

       retval->title = g_strdup (title);
       retval->english_title = g_strdup (english_title);
       retval->real_category = real_category != FALSE;
       retval->entries = NULL;  /* We will populate it in init_categories */

       return retval;
}

static int
compare_categories (PanelAddtoCategory *a,
                   PanelAddtoCategory *b)
{
       return g_utf8_collate (a->title, b->title);
}

static void
panel_addto_init_categories (PanelAddtoDialog *dialog)
{
       PanelAddtoInformation *info;
       PanelAddtoItemInfo    *applet;
       PanelAddtoEntry       *entry;
       PanelAddtoCategory    *current_category;
       GSList                *categories = NULL;
       GSList                *l, *m;
       gchar                 *category_title, *category_english_title;
       gboolean               found;

       info = g_new0 (PanelAddtoInformation, 1);

       for (l = dialog->applet_list; l; l = l->next) {
                applet = l->data;
               category_title = g_strdup (applet->category);
               category_english_title = g_strdup (applet->english_category);
               found = FALSE;
               for (m = categories; m; m = m->next) {
                       current_category = m->data;
                       if (!g_utf8_collate (current_category->english_title, category_english_title)) {
                               found = TRUE;
                               current_category->n_entries++;
                               if (!current_category->translated && strcmp (category_title, category_english_title)) {
                                       current_category->title = category_title;
                                       current_category->translated = TRUE;
                               }
                               break;
                       }
		}
	        if (!found) {
                       categories = g_slist_prepend (categories,
                                                     panel_addto_category_new (category_title, category_english_title, TRUE));
                       current_category = categories->data;
                       current_category->translated = strcmp (category_title, category_english_title) ? TRUE : FALSE;
                       current_category->n_entries = 1;
		}

               entry              = g_new0 (PanelAddtoEntry, 1);
               entry->title       = g_strdup (applet->name);
               entry->comment     = g_strdup (applet->description);
               entry->item_info   = applet;
               entry->category    = current_category;
               entry->dialog      = dialog;
               entry->icon_pixbuf = panel_addto_make_pixbuf (applet->icon);
               current_category->entries = g_slist_append (current_category->entries, entry);
       }

       categories = g_slist_sort (categories,
                                  (GCompareFunc) compare_categories);

       info->n_categories = g_slist_length (categories);
       info->categories   = categories;

       dialog->info = info;
}

static gboolean
panel_addto_separator_func (GtkTreeModel *model,
			    GtkTreeIter *iter,
			    gpointer data)
{
	int column = GPOINTER_TO_INT (data);
	char *text;
	
	gtk_tree_model_get (model, iter, column, &text, -1);
	
	if (!text)
		return TRUE;
	
	g_free(text);
	return FALSE;
}

static void
panel_addto_applications_selection_changed (GtkTreeSelection *selection,
					    PanelAddtoDialog *dialog)
{
	GtkTreeModel       *model;
	GtkTreeIter         iter;
	PanelAddtoItemInfo *data;
	char               *iid;


	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_widget_set_sensitive (GTK_WIDGET (dialog->add_button), FALSE);
		return;
	}

	gtk_tree_model_get (model, &iter, COLUMN_DATA, &data, -1);

	if (!data) {
		gtk_widget_set_sensitive (GTK_WIDGET (dialog->add_button), FALSE);
		return;
	}

	gtk_widget_set_sensitive (GTK_WIDGET (dialog->add_button), TRUE);

	gtk_button_set_label (GTK_BUTTON (dialog->add_button),
			      GTK_STOCK_ADD);
	gtk_button_set_use_stock (GTK_BUTTON (dialog->add_button),
				  TRUE);

	/* only allow dragging applets if we can add applets */

	if (panel_profile_id_lists_are_writable ()) {
		switch (data->type) {
		case PANEL_ADDTO_LAUNCHER:
		  	panel_addto_applications_setup_launcher_drag (GTK_TREE_VIEW (dialog->tree_view),
								      data->launcher_path);
			break;
		case PANEL_ADDTO_MENU:

			/* build the iid for menus other than the main menu */

			if (data->iid == NULL) {
				iid = g_strdup_printf ("MENU:%s/%s",
						       data->menu_filename,
						       data->menu_path);
			} else {
				iid = g_strdup (data->iid);
			}
				panel_addto_applications_setup_internal_applet_drag (GTK_TREE_VIEW (dialog->tree_view),
										     iid);
			g_free (iid);
			break;
		default:
			break;
		}
	}
}


static void
panel_addto_applications_selection_activated (GtkTreeView       *view,
					      GtkTreePath       *path,
					      GtkTreeViewColumn *column,
					      PanelAddtoDialog  *dialog)
{
	gtk_dialog_response (GTK_DIALOG (dialog->addto_dialog),
			     PANEL_ADDTO_RESPONSE_ADD);
}

static void
panel_addto_present_applications (PanelAddtoDialog *dialog)
{
        GtkTreeSelection  *selection;
	GtkTreeViewColumn *column;

        if (dialog->application_model == NULL) {
        	dialog->application_model = panel_addto_make_application_model (dialog);
	}
	gtk_widget_hide (dialog->applets_sw);
	gtk_widget_hide (dialog->statuslabel);
	gtk_widget_hide (dialog->search_entry);
	gtk_widget_hide (dialog->search_label);

	gtk_widget_set_sensitive (dialog->launcher_button, FALSE);
	gtk_widget_set_sensitive (dialog->custom_launcher_button, FALSE);

	dialog->status = APPLICATIONS;

	dialog->applications_sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (dialog->applications_sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (dialog->applications_sw),
					     GTK_SHADOW_IN);
	dialog->tree_view = gtk_tree_view_new ();
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (dialog->tree_view),
					   FALSE);
	gtk_tree_view_expand_all (GTK_TREE_VIEW (dialog->tree_view));

	dialog->renderer = g_object_new (GTK_TYPE_CELL_RENDERER_PIXBUF,
					 "xpad", 4,
					 "ypad", 4,
					 NULL);

	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (dialog->tree_view),
						     -1, NULL,
						     dialog->renderer,
						     "pixbuf", COLUMN_ICON,
						     NULL);

	dialog->renderer = gtk_cell_renderer_text_new ();
	g_object_set (dialog->renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (dialog->tree_view),
						     -1, NULL,
						     dialog->renderer,
						     "markup", COLUMN_TEXT,
						     NULL);
	gtk_tree_view_set_search_column (GTK_TREE_VIEW (dialog->tree_view),
					 COLUMN_SEARCH);

	gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (dialog->tree_view),
					      panel_addto_separator_func,
					      GINT_TO_POINTER (COLUMN_TEXT),
					      NULL);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	column = gtk_tree_view_get_column (GTK_TREE_VIEW (dialog->tree_view),
					   COLUMN_TEXT);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);

	g_signal_connect (selection, "changed",
			  G_CALLBACK (panel_addto_applications_selection_changed),
			  dialog);

	g_signal_connect (dialog->tree_view, "row-activated",
			  G_CALLBACK (panel_addto_applications_selection_activated),
			  dialog);

        gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->tree_view),
				 dialog->application_model);
        gtk_container_add    (GTK_CONTAINER (dialog->applications_sw),
			      GTK_WIDGET (dialog->tree_view));
	gtk_box_pack_start (GTK_BOX (dialog->inner_vbox), 
			    GTK_WIDGET (dialog->applications_sw), TRUE, TRUE, 0);


	gtk_window_set_focus (GTK_WINDOW (dialog->addto_dialog),
			      dialog->tree_view);
	gtk_widget_set_sensitive (dialog->back_button, TRUE);
	gtk_widget_show_all (GTK_WIDGET (dialog->applications_sw));
}

static void
panel_addto_present_applets (PanelAddtoDialog *dialog)
{

	if (dialog->applet_model == NULL)
		dialog->applet_model = panel_addto_make_applet_model (dialog);

	gtk_widget_set_sensitive (dialog->back_button, FALSE);
	
}

static void
panel_addto_dialog_free_item_info (PanelAddtoItemInfo *item_info)
{
	if (item_info == NULL || item_info->static_data)
		return;

	if (item_info->name != NULL)
		g_free (item_info->name);
	item_info->name = NULL;

	if (item_info->description != NULL)
		g_free (item_info->description);
	item_info->description = NULL;

	if (item_info->icon != NULL)
		g_free (item_info->icon);
	item_info->icon = NULL;

	if (item_info->iid != NULL)
		g_free (item_info->iid);
	item_info->iid = NULL;

	if (item_info->launcher_path != NULL)
		g_free (item_info->launcher_path);
	item_info->launcher_path = NULL;

	if (item_info->menu_filename != NULL)
		g_free (item_info->menu_filename);
	item_info->menu_filename = NULL;

	if (item_info->menu_path != NULL)
		g_free (item_info->menu_path);
	item_info->menu_path = NULL;
}

static void
panel_addto_dialog_free_application_list (GSList *application_list)
{
	PanelAddtoAppList *data;
	GSList            *app;

	if (application_list == NULL)
		return;

	for (app = application_list; app != NULL; app = app->next) {
		data = app->data;
		if (data->children) {
			panel_addto_dialog_free_application_list (data->children);
		}
		panel_addto_dialog_free_item_info (&data->item_info);
		g_free (data);
	}
	g_slist_free (application_list);
}

static void
panel_addto_dialog_free (PanelAddtoDialog *dialog)
{
	GConfClient             *client;
	GSList                  *item, *l, *m;
	PanelAddtoCanvasPrivate *priv;
	PanelAddtoCategory      *current_category;

	client = panel_gconf_get_client ();

	if (dialog->name_notify)
		gconf_client_notify_remove (client, dialog->name_notify);
	dialog->name_notify = 0;

	if (dialog->addto_dialog)
	        gtk_widget_destroy (dialog->addto_dialog);
	dialog->addto_dialog = NULL;
	
	if (dialog->label) {
	  dialog->label = NULL;
	}
	
	for (item = dialog->applet_list; item != NULL; item = item->next) {
		PanelAddtoItemInfo *applet;

		applet = (PanelAddtoItemInfo *) item->data;
		if (!applet->static_data) {
			panel_addto_dialog_free_item_info (applet);
			g_free (applet);
		}
	}
	g_slist_free (dialog->applet_list);

	panel_addto_dialog_free_application_list (dialog->application_list);
	panel_addto_dialog_free_application_list (dialog->settings_list);
		
	if (dialog->canvas) {
	        priv = PANEL_ADDTO_CANVAS_GET_PRIVATE (dialog->canvas);
	        if (priv) {
		        if (priv && priv->info && priv->info->categories) {
			  for (l = priv->info->categories; l; l = l->next) {
			          current_category = l->data;
				  for (m = current_category->entries; m; m = m->next) {
				          g_free (m->data);
				  }
				  if (current_category->user_data)
				          g_free (current_category->user_data);
				  g_slist_free (current_category->entries);
				  g_free (current_category);
			  }
			  g_slist_free (priv->info->categories);
			}
			g_free (priv->search_text);
		}
	}

	

	if (dialog->applet_model)
	  g_object_unref (dialog->applet_model);
	dialog->applet_model = NULL;

	if (dialog->application_model)
		g_object_unref (dialog->application_model);
	dialog->application_model = NULL;

	if (dialog->menu_tree)
		gmenu_tree_unref (dialog->menu_tree);
	dialog->menu_tree = NULL;

	g_free (dialog);
}

static void
panel_addto_name_change (PanelAddtoDialog *dialog,
			 const char       *name)
{
	char *title;
	char *label;

	label = NULL;

	if (!string_empty (name))
		label = g_strdup_printf (_("Select an item to add to \"%s\":"),
					 name);

	if (panel_toplevel_get_is_attached (dialog->panel_widget->toplevel)) {
		title = g_strdup_printf (_("Add to Drawer"));
		if (label == NULL)
			label = g_strdup (_("Select an item to add to the drawer:"));
	} else {
		title = g_strdup_printf (_("Add to Panel"));
		if (label == NULL)
			label = g_strdup (_("Select an item to add to the panel \n\
(you can also directly drag and drop items onto the panel):"));
	}

	gtk_window_set_title (GTK_WINDOW (dialog->addto_dialog), title);
	g_free (title);

	gtk_label_set_text_with_mnemonic (GTK_LABEL (dialog->label), label);
	g_free (label);
}

static void
panel_addto_name_notify (GConfClient      *client,
			 guint             cnxn_id,
			 GConfEntry       *entry,
			 PanelAddtoDialog *dialog)
{
	GConfValue *value;
	const char *key;
	const char *text = NULL;

	key = panel_gconf_basename (gconf_entry_get_key (entry));

	if (strcmp (key, "name"))
		return;

	value = gconf_entry_get_value (entry);

	if (value && value->type == GCONF_VALUE_STRING)
		text = gconf_value_get_string (value);

	if (text)
		panel_addto_name_change (dialog, text);
}


static PanelAddtoDialog *
panel_addto_dialog_new (PanelWidget *panel_widget)
{
	PanelAddtoDialog *dialog;
	GtkWidget        *vbox;
	GtkWidget        *top_hbox;
	GtkWidget        *launchers_hbox;
	GdkPixbuf        *launcher_pixbuf;
	GtkWidget        *launcher_image;
	GtkWidget        *bottom_hbox;
	GtkTooltips      *launchers_tooltips;
	GtkImage         *label_image;

	dialog = g_new0 (PanelAddtoDialog, 1);

	dialog->status = APPLETS;

	g_object_set_qdata_full (G_OBJECT (panel_widget->toplevel),
				 panel_addto_dialog_quark,
				 dialog,
				 (GDestroyNotify) panel_addto_dialog_free);

	dialog->panel_widget = panel_widget;
	dialog->name_notify =
	panel_profile_toplevel_notify_add (dialog->panel_widget->toplevel,
	                                   "name",
	                                   (GConfClientNotifyFunc) panel_addto_name_notify,
	                                    dialog);
	dialog->addto_dialog = gtk_dialog_new ();

	dialog->statuslabel = gtk_label_new ("");
	
	gtk_dialog_add_button (GTK_DIALOG (dialog->addto_dialog),
			       GTK_STOCK_HELP, GTK_RESPONSE_HELP);
	dialog->back_button = gtk_dialog_add_button (GTK_DIALOG (dialog->addto_dialog),
						     GTK_STOCK_GO_BACK,
						     PANEL_ADDTO_RESPONSE_BACK);
	dialog->add_button = gtk_dialog_add_button (GTK_DIALOG (dialog->addto_dialog),
						     GTK_STOCK_ADD,
						     PANEL_ADDTO_RESPONSE_ADD);
	gtk_dialog_add_button (GTK_DIALOG (dialog->addto_dialog),
			       GTK_STOCK_CLOSE,
			       GTK_RESPONSE_CLOSE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->add_button), FALSE);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog->addto_dialog),
				      FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog->addto_dialog),
					 PANEL_ADDTO_RESPONSE_ADD);

	gtk_container_set_border_width (GTK_CONTAINER (dialog->addto_dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog->addto_dialog)->vbox), 2);
	g_signal_connect (G_OBJECT (dialog->addto_dialog), "response",
			  G_CALLBACK (panel_addto_dialog_response), dialog);
	g_signal_connect (dialog->addto_dialog, "destroy",
			  G_CALLBACK (panel_addto_dialog_destroy), dialog);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog->addto_dialog)->vbox),
			   GTK_WIDGET (vbox));

	dialog->inner_vbox = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), dialog->inner_vbox, TRUE, TRUE, 0);

	top_hbox       = gtk_hbox_new (FALSE, 6);
	launchers_hbox = gtk_hbox_new (FALSE, 6);
	bottom_hbox    = gtk_hbox_new (FALSE, 0);
	dialog->label = gtk_label_new_with_mnemonic ("");
	gtk_misc_set_alignment (GTK_MISC (dialog->label), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (dialog->label), TRUE);

	label_image = g_object_new (GTK_TYPE_IMAGE,
	                            "stock", GTK_STOCK_DIALOG_INFO,
	                            "icon-size", GTK_ICON_SIZE_DND,
	                            NULL);
	dialog->search_label = gtk_label_new_with_mnemonic (_("_Search: "));
	dialog->search_entry = gtk_entry_new ();
	g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->search_entry)));
	gtk_label_set_mnemonic_widget (GTK_LABEL (dialog->search_label), GTK_WIDGET (dialog->search_entry));

	g_signal_connect (G_OBJECT (dialog->search_entry), "changed",
			  G_CALLBACK (panel_addto_relayout_for_search), dialog);
	g_signal_connect (G_OBJECT (dialog->search_entry), "activate",
			  G_CALLBACK (panel_addto_activate_from_searchbar), dialog);
	
	gtk_box_pack_start (GTK_BOX (top_hbox), GTK_WIDGET(label_image),
			    FALSE, FALSE, 0);
	
	gtk_box_pack_start (GTK_BOX (top_hbox), dialog->label,
			    TRUE, TRUE, 0);
	
	gtk_box_pack_start (GTK_BOX (top_hbox), GTK_WIDGET (dialog->search_label),
                            FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (top_hbox), GTK_WIDGET (dialog->search_entry),
                            TRUE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (dialog->inner_vbox), top_hbox,
			    FALSE, FALSE, 0);

	/* Launcher button */
	launchers_tooltips = gtk_tooltips_new ();
	dialog->launcher_button = gtk_button_new_with_mnemonic (_("A_pplication Launcher..."));
	launcher_pixbuf = panel_addto_make_pixbuf_for_launcher ("launcher-program");
	launcher_image = gtk_image_new_from_pixbuf (launcher_pixbuf);
	gtk_button_set_image (GTK_BUTTON (dialog->launcher_button), launcher_image);
	gtk_tooltips_set_tip (launchers_tooltips, dialog->launcher_button, _("Launch a program that is already in the GNOME menu"), NULL);
	g_signal_connect_swapped (dialog->launcher_button, "clicked",
				  G_CALLBACK (panel_addto_add_launcher), dialog);
	gtk_box_pack_start (GTK_BOX (launchers_hbox), dialog->launcher_button,
			    FALSE, FALSE, 0);

	/* Custom launcher button*/
	dialog->custom_launcher_button = gtk_button_new_with_mnemonic (_("Custom Application _Launcher"));
	launcher_image = gtk_image_new_from_pixbuf (launcher_pixbuf);
	gtk_button_set_image (GTK_BUTTON (dialog->custom_launcher_button), launcher_image);
	gtk_tooltips_set_tip (launchers_tooltips, dialog->custom_launcher_button, _("Create a new launcher"), NULL);
	g_signal_connect_swapped (dialog->custom_launcher_button, "clicked",
				  G_CALLBACK (panel_addto_add_custom_launcher), dialog);
	gtk_box_pack_start (GTK_BOX (launchers_hbox), dialog->custom_launcher_button,
			    FALSE, FALSE, 0);

	if (panel_lockdown_get_disable_command_line ())
	  gtk_widget_set_sensitive (dialog->custom_launcher_button, FALSE);

	gtk_box_pack_start (GTK_BOX (dialog->inner_vbox), launchers_hbox,
			    FALSE, FALSE, 0);

	dialog->applets_sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (dialog->applets_sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (dialog->applets_sw),
   					     GTK_SHADOW_IN);

	gtk_box_pack_start (GTK_BOX (dialog->inner_vbox), dialog->applets_sw, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (bottom_hbox), dialog->statuslabel, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (dialog->inner_vbox), bottom_hbox,
			    FALSE, FALSE, 0);

	gtk_widget_show_all (vbox);

	gtk_label_set_mnemonic_widget (GTK_LABEL (dialog->label),
				       dialog->canvas);

	panel_toplevel_push_autohide_disabler (dialog->panel_widget->toplevel);
	panel_widget_register_open_dialog (panel_widget,
					   dialog->addto_dialog);
	panel_addto_name_change (dialog,
				 panel_toplevel_get_name (dialog->panel_widget->toplevel));

	return dialog;
}

#define MAX_ADDTOPANEL_HEIGHT 540

void
panel_addto_present (GtkMenuItem *item,
		     PanelWidget *panel_widget)
{
	PanelAddtoDialog *dialog;
	PanelToplevel *toplevel;
	PanelData     *pd;
	GtkAdjustment *sw_adjustment = NULL;
	GdkScreen *screen;
	gint screen_height;
	gint height;

	toplevel = panel_widget->toplevel;
	pd = g_object_get_data (G_OBJECT (toplevel), "PanelData");

	if (!panel_addto_dialog_quark)
		panel_addto_dialog_quark =
			g_quark_from_static_string ("panel-addto-dialog");

	dialog = g_object_get_qdata (G_OBJECT (toplevel),
				     panel_addto_dialog_quark);

	screen = gtk_window_get_screen (GTK_WINDOW (toplevel));
	screen_height = gdk_screen_get_height (screen);
	height = MIN (MAX_ADDTOPANEL_HEIGHT, 3 * (screen_height / 4));

	if (!dialog) {
		dialog = panel_addto_dialog_new (panel_widget);

		panel_addto_present_applets (dialog);
		panel_addto_init_categories (dialog);

		dialog->canvas = panel_addto_canvas_new (dialog->info);
       		gtk_container_add (GTK_CONTAINER (dialog->applets_sw), 
				   GTK_WIDGET (dialog->canvas));
	}

	sw_adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (dialog->applets_sw));
	dialog->insertion_position = pd ? pd->insertion_pos : -1;
	gtk_window_set_screen (GTK_WINDOW (dialog->addto_dialog), screen);
	gtk_window_set_default_size (GTK_WINDOW (dialog->addto_dialog),
				     height * 1.35, height);
	gtk_window_present (GTK_WINDOW (dialog->addto_dialog));
	
	sw_adjustment->value = sw_adjustment->lower;
	sw_adjustment->step_increment = 55;
	gtk_adjustment_clamp_page (sw_adjustment, sw_adjustment->lower, sw_adjustment->upper);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (dialog->applets_sw), sw_adjustment);
}
