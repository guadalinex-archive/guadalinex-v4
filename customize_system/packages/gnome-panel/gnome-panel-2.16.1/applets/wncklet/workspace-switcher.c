/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * libwnck based pager applet.
 * (C) 2001 Alexander Larsson 
 * (C) 2001 Red Hat, Inc
 *
 * Authors: Alexander Larsson
 *
 */

#define WNCK_I_KNOW_THIS_IS_UNSTABLE 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <panel-applet.h>
#include <panel-applet-gconf.h>

#include <stdlib.h>

#include <gtk/gtkdialog.h>
#include <glade/glade-xml.h>
#include <libwnck/libwnck.h>
#include <gconf/gconf-client.h>
#include <libgnomeui/gnome-help.h>

#include "workspace-switcher.h"

#include "wncklet.h"

/* even 16 is pretty darn dubious. */
#define MAX_REASONABLE_ROWS 16
#define DEFAULT_ROWS 1

#define NEVER_SENSITIVE "never_sensitive"
#define NUM_WORKSPACES "/apps/metacity/general/num_workspaces"
#define WORKSPACE_NAME "/apps/metacity/workspace_names/name_1"
#include <lpint-bonobo.h>

typedef struct {
	GtkWidget *applet;

	GtkWidget *pager;
	
	WnckScreen *screen;

	/* Properties: */
	GtkWidget *properties_dialog;
	GtkWidget *display_workspaces_toggle;
	GtkWidget *all_workspaces_radio;
	GtkWidget *current_only_radio;
	GtkWidget *num_rows_spin;	       /* for vertical layout this is cols */
	GtkWidget *label_row_col;
	GtkWidget *num_workspaces_spin;
	GtkWidget *workspaces_tree;
	GtkWidget *about;

	GtkListStore *workspaces_store;
	
	GtkOrientation orientation;
	int n_rows;				/* for vertical layout this is cols */
	WnckPagerDisplayMode display_mode;
	gboolean display_all;

	/* gconf listeners id */
	guint listeners [3];
} PagerData;

static void display_properties_dialog (BonoboUIComponent *uic,
				       PagerData         *pager,
				       const gchar       *verbname);
static void display_help_dialog       (BonoboUIComponent *uic,
				       PagerData         *pager,
				       const gchar       *verbname);
static void display_about_dialog      (BonoboUIComponent *uic,
				       PagerData         *pager,
				       const gchar       *verbname);

static void
pager_update (PagerData *pager)
{
	wnck_pager_set_orientation (WNCK_PAGER (pager->pager),
				    pager->orientation);
	wnck_pager_set_n_rows (WNCK_PAGER (pager->pager),
			       pager->n_rows);
	wnck_pager_set_display_mode (WNCK_PAGER (pager->pager),
				     pager->display_mode);
	wnck_pager_set_show_all (WNCK_PAGER (pager->pager),
				 pager->display_all);
}

static void
applet_realized (PanelApplet *applet,
		 PagerData   *pager)
{
	WnckScreen *screen;

	screen = wncklet_get_screen (GTK_WIDGET (applet));

	wnck_pager_set_screen (WNCK_PAGER (pager->pager), screen);
}

static void
applet_change_orient (PanelApplet       *applet,
		      PanelAppletOrient  orient,
		      PagerData         *pager)
{
	GtkOrientation new_orient;
  
	switch (orient)	{
	case PANEL_APPLET_ORIENT_LEFT:
	case PANEL_APPLET_ORIENT_RIGHT:
		new_orient = GTK_ORIENTATION_VERTICAL;
		break;
	case PANEL_APPLET_ORIENT_UP:
	case PANEL_APPLET_ORIENT_DOWN:
	default:
		new_orient = GTK_ORIENTATION_HORIZONTAL;
		break;
	}

	if (new_orient == pager->orientation)
		return;
  
	pager->orientation = new_orient;
	pager_update (pager);
	if (pager->label_row_col) 
		gtk_label_set_text (GTK_LABEL (pager->label_row_col), pager->orientation == GTK_ORIENTATION_HORIZONTAL ? _("rows") : _("columns"));	
}

static void
applet_change_background (PanelApplet               *applet,
			  PanelAppletBackgroundType  type,
			  GdkColor                  *color,
			  GdkPixmap                 *pixmap,
			  PagerData                 *pager)
{
	switch (type) {
	case PANEL_NO_BACKGROUND:
		wnck_pager_set_shadow_type (WNCK_PAGER (pager->pager),
					    GTK_SHADOW_IN);
		break;
	case PANEL_COLOR_BACKGROUND:
		wnck_pager_set_shadow_type (WNCK_PAGER (pager->pager),
					    GTK_SHADOW_NONE);
		break;
	case PANEL_PIXMAP_BACKGROUND:
		wnck_pager_set_shadow_type (WNCK_PAGER (pager->pager),
					    GTK_SHADOW_NONE);
		break;
	}
}

static gboolean
applet_scroll (PanelApplet    *applet,
               GdkEventScroll *event,
               PagerData      *pager)
{
	WnckScreen *screen;
	int         index;
	int         n_workspaces;
	int         n_columns;
	int         in_last_row;
	
	if (event->type != GDK_SCROLL)
		return FALSE;

	screen         = wncklet_get_screen (GTK_WIDGET (applet));
	index          = wnck_workspace_get_number (wnck_screen_get_active_workspace (screen));
	n_workspaces   = wnck_screen_get_workspace_count (screen);
	n_columns      = n_workspaces / pager->n_rows;
	if (n_workspaces % pager->n_rows != 0)
		n_columns++;
	in_last_row    = n_workspaces % n_columns;
	
	switch (event->direction) {
	case GDK_SCROLL_DOWN:
		if (index + n_columns < n_workspaces)
			index += n_columns;
		else if ((index < n_workspaces - 1
			  && index + in_last_row != n_workspaces - 1) ||
			 (index == n_workspaces - 1
			  && in_last_row != 0))
			index = (index % n_columns) + 1;
		break;
		
	case GDK_SCROLL_RIGHT:
		if (index < n_workspaces - 1)
			index++;
		break;
		
	case GDK_SCROLL_UP:
		if (index - n_columns >= 0)
			index -= n_columns;
		else if (index > 0)
			index = ((pager->n_rows - 1) * n_columns) + (index % n_columns) - 1;
			if (index >= n_workspaces)
				index -= n_columns;
		break;

	case GDK_SCROLL_LEFT:
		if (index > 0)
			index--;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	wnck_workspace_activate (wnck_screen_get_workspace (screen, index),
				 event->time);
	
	return TRUE;
}

static void 
response_cb (GtkWidget *widget,
	     int        id,
	     PagerData *pager)
{
	if (id == GTK_RESPONSE_HELP) {
		GError *error = NULL;

		gnome_help_display_desktop_on_screen (
			NULL, "workspace-switcher", "workspace-switcher", "workspacelist-prefs",
			gtk_widget_get_screen (pager->applet),
			&error);

		if (error) {
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (GTK_WINDOW (widget),
							 GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_OK,
							  _("There was an error displaying help: %s"),
							 error->message);

			g_signal_connect (dialog, "response",
					  G_CALLBACK (gtk_widget_destroy),
					  NULL);

			gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
			gtk_window_set_screen (GTK_WINDOW (dialog),
					       gtk_widget_get_screen (pager->applet));
			gtk_widget_show (dialog);
			g_error_free (error);
		}
	} else
		gtk_widget_hide (widget);
}

static void
destroy_pager(GtkWidget * widget, PagerData *pager)
{
	GConfClient *client = gconf_client_get_default ();

	gconf_client_notify_remove (client, pager->listeners[0]);
	gconf_client_notify_remove (client, pager->listeners[1]);
	gconf_client_notify_remove (client, pager->listeners[2]);

	g_object_unref (G_OBJECT (client));

	pager->listeners[0] = 0;
	pager->listeners[1] = 0;
	pager->listeners[2] = 0;

	if (pager->properties_dialog)
		gtk_widget_destroy (pager->properties_dialog);

	if (pager->about)
		gtk_widget_destroy (pager->about);

	g_free (pager);
}

static const BonoboUIVerb pager_menu_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("PagerPreferences", display_properties_dialog),
	BONOBO_UI_UNSAFE_VERB ("PagerHelp",        display_help_dialog),
	BONOBO_UI_UNSAFE_VERB ("PagerAbout",       display_about_dialog),
        BONOBO_UI_VERB_END
};

static void
num_rows_changed (GConfClient *client,
		  guint        cnxn_id,
		  GConfEntry  *entry,
		  PagerData   *pager)
{
	int n_rows = DEFAULT_ROWS;
	
	if (entry->value != NULL &&
	    entry->value->type == GCONF_VALUE_INT) {
		n_rows = gconf_value_get_int (entry->value);
	}

        n_rows = CLAMP (n_rows, 1, MAX_REASONABLE_ROWS);
        
	pager->n_rows = n_rows;
	pager_update (pager);

	if (pager->num_rows_spin &&
	    gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (pager->num_rows_spin)) != n_rows)
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (pager->num_rows_spin), pager->n_rows);
}

static void
display_workspace_names_changed (GConfClient *client,
				 guint        cnxn_id,
				 GConfEntry  *entry,
				 PagerData   *pager)
{
	gboolean value = FALSE; /* Default value */
	
	if (entry->value != NULL &&
	    entry->value->type == GCONF_VALUE_BOOL) {
		value = gconf_value_get_bool (entry->value);
	}

	if (value) {
		pager->display_mode = WNCK_PAGER_DISPLAY_NAME;
	} else {
		pager->display_mode = WNCK_PAGER_DISPLAY_CONTENT;
	}
	pager_update (pager);
	
	if (pager->display_workspaces_toggle &&
	    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pager->display_workspaces_toggle)) != value) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pager->display_workspaces_toggle),
					      value);
	}
}


static void
all_workspaces_changed (GConfClient *client,
			guint        cnxn_id,
			GConfEntry  *entry,
			PagerData   *pager)
{
	gboolean value = TRUE; /* Default value */
	
	if (entry->value != NULL &&
	    entry->value->type == GCONF_VALUE_BOOL) {
		value = gconf_value_get_bool (entry->value);
	}

	pager->display_all = value;
	pager_update (pager);

	if (pager->all_workspaces_radio){
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pager->all_workspaces_radio)) != value) {
			if (value) {
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pager->all_workspaces_radio), TRUE);
			} else {
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pager->current_only_radio), TRUE);
			}
		}
		if ( ! g_object_get_data (G_OBJECT (pager->num_rows_spin), NEVER_SENSITIVE))
			gtk_widget_set_sensitive (pager->num_rows_spin, value);
	}
}

static void
setup_gconf (PagerData *pager)
{
	GConfClient *client;
	char *key;

	client = gconf_client_get_default ();

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (pager->applet),
					       "num_rows");
	pager->listeners[0] = gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)num_rows_changed,
				pager,
				NULL, NULL);
		
	g_free (key);


	key = panel_applet_gconf_get_full_key (PANEL_APPLET (pager->applet),
					       "display_workspace_names");
	pager->listeners[1] = gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)display_workspace_names_changed,
				pager,
				NULL, NULL);
		
	g_free (key);

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (pager->applet),
					       "display_all_workspaces");
	pager->listeners[2] = gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)all_workspaces_changed,
				pager,
				NULL, NULL);
		
	g_free (key);

	g_object_unref (G_OBJECT (client));

}

gboolean
workspace_switcher_applet_fill (PanelApplet *applet)
{
	PagerData *pager;
	GError *error;
	gboolean display_names;
        BonoboUIComponent* popup_component;
	
	panel_applet_add_preferences (applet, "/schemas/apps/workspace_switcher_applet/prefs", NULL);
	
	pager = g_new0 (PagerData, 1);

	pager->applet = GTK_WIDGET (applet);

	panel_applet_set_flags (PANEL_APPLET (pager->applet), PANEL_APPLET_EXPAND_MINOR);

	setup_gconf (pager);
	
	error = NULL;
	pager->n_rows = panel_applet_gconf_get_int (applet, "num_rows", &error);
	if (error) {
                g_printerr (_("Error loading num_rows value for Workspace Switcher: %s\n"),
                            error->message);
		g_error_free (error);
                /* leave current value */
	}

        pager->n_rows = CLAMP (pager->n_rows, 1, MAX_REASONABLE_ROWS);

	error = NULL;
	display_names = panel_applet_gconf_get_bool (applet, "display_workspace_names", &error);
	if (error) {
                g_printerr (_("Error loading display_workspace_names value for Workspace Switcher: %s\n"),
                            error->message);
		g_error_free (error);
                /* leave current value */
	}

	if (display_names) {
		pager->display_mode = WNCK_PAGER_DISPLAY_NAME;
	} else {
		pager->display_mode = WNCK_PAGER_DISPLAY_CONTENT;
	}

	error = NULL;
	pager->display_all = panel_applet_gconf_get_bool (applet, "display_all_workspaces", &error);
	if (error) {
                g_printerr (_("Error loading display_all_workspaces value for Workspace Switcher: %s\n"),
                            error->message);
		g_error_free (error);
                /* leave current value */
	}
	
	switch (panel_applet_get_orient (applet)) {
	case PANEL_APPLET_ORIENT_LEFT:
	case PANEL_APPLET_ORIENT_RIGHT:
		pager->orientation = GTK_ORIENTATION_VERTICAL;
		break;
	case PANEL_APPLET_ORIENT_UP:
	case PANEL_APPLET_ORIENT_DOWN:
	default:
		pager->orientation = GTK_ORIENTATION_HORIZONTAL;
		break;
	}

	pager->screen = wncklet_get_screen (pager->applet);

	/* because the pager doesn't respond to signals at the moment */
	wnck_screen_force_update (pager->screen);

	pager->pager = wnck_pager_new (pager->screen);
	wnck_pager_set_shadow_type (WNCK_PAGER (pager->pager), GTK_SHADOW_IN);

	g_signal_connect (G_OBJECT (pager->pager), "destroy",
			  G_CALLBACK (destroy_pager),
			  pager);

	pager_update (pager);
	
	gtk_container_add (GTK_CONTAINER (pager->applet), pager->pager);
	gtk_widget_show (pager->pager);

	gtk_widget_show (pager->applet);

	g_signal_connect (G_OBJECT (pager->applet),
			  "realize",
			  G_CALLBACK (applet_realized),
			  pager);
	g_signal_connect (G_OBJECT (pager->applet),
			  "change_orient",
			  G_CALLBACK (applet_change_orient),
			  pager);
	g_signal_connect (G_OBJECT (pager->applet),
			  "scroll-event",
			  G_CALLBACK (applet_scroll),
			  pager);
	g_signal_connect (G_OBJECT (pager->applet),
			  "change_background",
			  G_CALLBACK (applet_change_background),
			  pager);

	panel_applet_set_background_widget (PANEL_APPLET (pager->applet),
					    GTK_WIDGET (pager->applet));
	
	panel_applet_setup_menu_from_file (PANEL_APPLET (pager->applet),
					   NULL,
					   "GNOME_WorkspaceSwitcherApplet.xml",
					   NULL,
					   pager_menu_verbs,
					   pager);

	if (panel_applet_get_locked_down (PANEL_APPLET (pager->applet))) {
		BonoboUIComponent *popup_component;

		popup_component = panel_applet_get_popup_component (PANEL_APPLET (pager->applet));

		bonobo_ui_component_set_prop (popup_component,
					      "/commands/PagerPreferences",
					      "hidden", "1",
					      NULL);
	}

        popup_component = panel_applet_get_popup_component (PANEL_APPLET (pager->applet));

        launchpad_integration_add_bonobo_ui(popup_component, "/popups/button3/LaunchpadItems");        


	return TRUE;
}


static void
display_help_dialog (BonoboUIComponent *uic,
		     PagerData         *pager,
		     const gchar       *verbname)
{
	wncklet_display_help (pager->applet, "workspace-switcher",
			      "workspace-switcher", NULL);
}

static void
display_about_dialog (BonoboUIComponent *uic,
		      PagerData         *pager,
		      const gchar       *verbname)
{
	static const gchar *authors[] =
	{
		"Alexander Larsson <alla@lysator.liu.se>",
		NULL
	};
	const char *documenters [] = {
                "John Fleck <jfleck@inkstain.net>",
                "Sun GNOME Documentation Team <gdocteam@sun.com>",
                NULL
	};
	const char *translator_credits = _("translator-credits");

	wncklet_display_about (pager->applet, &pager->about,
			       _("Workspace Switcher"),
			       "Copyright \xc2\xa9 2001-2002 Red Hat, Inc.",
			       _("The Workspace Switcher shows you a small version of your workspaces that lets you manage your windows."),
			       authors,
			       documenters,
			       translator_credits,
			       "gnome-panel-workspace-switcher",
			       "pager",
			       "Pager");
}


static void
display_workspace_names_toggled (GtkToggleButton *button,
				 PagerData       *pager)
{
	panel_applet_gconf_set_bool (PANEL_APPLET (pager->applet),
				     "display_workspace_names",
				     gtk_toggle_button_get_active (button),
				     NULL);
}

static void
all_workspaces_toggled (GtkToggleButton *button,
			PagerData       *pager)
{
	panel_applet_gconf_set_bool (PANEL_APPLET (pager->applet),
				     "display_all_workspaces",
				     gtk_toggle_button_get_active (button),
				     NULL);
}

static void
num_rows_value_changed (GtkSpinButton *button,
			PagerData       *pager)
{
	panel_applet_gconf_set_int (PANEL_APPLET (pager->applet),
				    "num_rows",
				    gtk_spin_button_get_value_as_int (button),
				    NULL);
}

static void
update_workspaces_model (PagerData *pager)
{
	int nr_ws, i;
	WnckWorkspace *workspace;
	GtkTreeIter iter;

	nr_ws = wnck_screen_get_workspace_count (pager->screen);
        
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pager->num_workspaces_spin), nr_ws);

	gtk_list_store_clear (pager->workspaces_store);
	for (i = 0; i < nr_ws; i++) {
		workspace = wnck_screen_get_workspace (pager->screen, i);
		gtk_list_store_append (pager->workspaces_store, &iter);
		gtk_list_store_set (pager->workspaces_store,
				    &iter,
				    0, wnck_workspace_get_name (workspace),
				    -1);
	}
}

static void
workspace_renamed (WnckWorkspace *space,
		   PagerData     *pager)
{
	update_workspaces_model (pager);
}

static void
workspace_created (WnckScreen    *screen,
		   WnckWorkspace *space,
		   PagerData     *pager)
{
        g_return_if_fail (WNCK_IS_SCREEN (screen));
        
	update_workspaces_model (pager);

	wncklet_connect_while_alive (space, "name_changed",
				     G_CALLBACK(workspace_renamed),
				     pager,
				     pager->applet);

}

static void
workspace_destroyed (WnckScreen    *screen,
		     WnckWorkspace *space,
		     PagerData     *pager)
{
        g_return_if_fail (WNCK_IS_SCREEN (screen));
	update_workspaces_model (pager);
}

static void
num_workspaces_value_changed (GtkSpinButton *button,
			      PagerData       *pager)
{
        wnck_screen_change_workspace_count (pager->screen,
                                            gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (pager->num_workspaces_spin)));
}

static gboolean
workspaces_tree_focused_out (GtkTreeView   *treeview,
			     GdkEventFocus *event,
			     PagerData     *pager)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (treeview);
	gtk_tree_selection_unselect_all (selection);
	return TRUE;
}

static void 
workspace_name_edited (GtkCellRendererText *cell_renderer_text,
		       const gchar         *path,
		       const gchar         *new_text,
		       PagerData           *pager)
{
        const gint *indices;
        WnckWorkspace *workspace;
        GtkTreePath *p;

        p = gtk_tree_path_new_from_string (path);
        indices = gtk_tree_path_get_indices (p);
        workspace = wnck_screen_get_workspace (pager->screen,
                                               indices[0]);
        if (workspace != NULL) {
                gchar* temp_name = g_strdup(new_text);

                wnck_workspace_change_name (workspace,
                                            g_strstrip(temp_name));
                g_free (temp_name);
        }
        else
                g_warning ("Edited name of workspace %d which no longer exists",
                           indices[0]);

        gtk_tree_path_free (p);
}

static gboolean
delete_event (GtkWidget *widget, gpointer data)
{
	gtk_widget_hide (widget);
	return TRUE;
}


#define WID(s) glade_xml_get_widget (xml, s)

static void
close_dialog (GtkWidget *button,
              gpointer data)
{
	PagerData *pager = data;
	GtkTreeViewColumn *col;

	/* This is a hack. The "editable" signal for GtkCellRenderer is emitted
	only on button press or focus cycle. Hence when the user changes the
	name and closes the preferences dialog without a button-press he would
	lose the name changes. So, we call the gtk_cell_editable_editing_done
	to stop the editing. Thanks to Paolo for a better crack than the one I had.
	*/

	col = gtk_tree_view_get_column(GTK_TREE_VIEW (pager->workspaces_tree),0);
	if (col->editable_widget != NULL && GTK_IS_CELL_EDITABLE (col->editable_widget))
	    gtk_cell_editable_editing_done(col->editable_widget);

	gtk_widget_hide (pager->properties_dialog);
}

static void
setup_sensitivity (PagerData *pager,
		   GladeXML *xml,
		   const char *wid1,
		   const char *wid2,
		   const char *wid3,
		   const char *key)
{
	PanelApplet *applet = PANEL_APPLET (pager->applet);
	GConfClient *client = gconf_client_get_default ();
	char *fullkey;
	GtkWidget *w;

	if (key[0] == '/')
		fullkey = g_strdup (key);
	else
		fullkey = panel_applet_gconf_get_full_key (applet, key);

	if (gconf_client_key_is_writable (client, fullkey, NULL)) {
		g_object_unref (G_OBJECT (client));
		g_free (fullkey);
		return;
	}
	g_object_unref (G_OBJECT (client));
	g_free (fullkey);

	w = glade_xml_get_widget (xml, wid1);
	g_assert (w != NULL);
	g_object_set_data (G_OBJECT (w), NEVER_SENSITIVE,
			   GINT_TO_POINTER (1));
	gtk_widget_set_sensitive (w, FALSE);

	if (wid2 != NULL) {
		w = glade_xml_get_widget (xml, wid2);
		g_assert (w != NULL);
		g_object_set_data (G_OBJECT (w), NEVER_SENSITIVE,
				   GINT_TO_POINTER (1));
		gtk_widget_set_sensitive (w, FALSE);
	}
	if (wid3 != NULL) {
		w = glade_xml_get_widget (xml, wid3);
		g_assert (w != NULL);
		g_object_set_data (G_OBJECT (w), NEVER_SENSITIVE,
				   GINT_TO_POINTER (1));
		gtk_widget_set_sensitive (w, FALSE);
	}

}

static void
setup_dialog (GladeXML  *xml,
	      PagerData *pager)
{
	gboolean value;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	int nr_ws, i;
	
	pager->display_workspaces_toggle = WID ("workspace_name_toggle");
	setup_sensitivity (pager, xml,
			   "workspace_name_toggle",
			   NULL,
			   NULL,
			   "display_workspace_names" /* key */);

	pager->all_workspaces_radio = WID ("all_workspaces_radio");
	pager->current_only_radio = WID ("current_only_radio");
	setup_sensitivity (pager, xml,
			   "all_workspaces_radio",
			   "current_only_radio",
			   "label_row_col",
			   "display_all_workspaces" /* key */);

	pager->num_rows_spin = WID ("num_rows_spin");
	pager->label_row_col = WID("label_row_col");
	setup_sensitivity (pager, xml,
			   "num_rows_spin",
			   NULL,
			   NULL,
			   "num_rows" /* key */);

	pager->num_workspaces_spin = WID ("num_workspaces_spin");
	setup_sensitivity (pager, xml,
			   "num_workspaces_spin",
			   NULL,
			   NULL,
			   NUM_WORKSPACES /* key */);

	pager->workspaces_tree = WID ("workspaces_tree_view");
	setup_sensitivity (pager, xml,
			   "workspaces_tree_view",
			   NULL,
			   NULL,
			   WORKSPACE_NAME /* key */);

	/* Display workspace names: */
	
	g_signal_connect (G_OBJECT (pager->display_workspaces_toggle), "toggled",
			  (GCallback) display_workspace_names_toggled, pager);

	if (pager->display_mode == WNCK_PAGER_DISPLAY_NAME) {
		value = TRUE;
	} else {
		value = FALSE;
	}
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pager->display_workspaces_toggle),
				      value);

	/* Display all workspaces: */
	g_signal_connect (G_OBJECT (pager->all_workspaces_radio), "toggled",
			  (GCallback) all_workspaces_toggled, pager);

	if (pager->display_all) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pager->all_workspaces_radio), TRUE);
		if ( ! g_object_get_data (G_OBJECT (pager->num_rows_spin), NEVER_SENSITIVE))
			gtk_widget_set_sensitive (pager->num_rows_spin, TRUE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pager->current_only_radio), TRUE);
		gtk_widget_set_sensitive (pager->num_rows_spin, FALSE);
	}
		
	/* Num rows: */
	g_signal_connect (G_OBJECT (pager->num_rows_spin), "value_changed",
			  (GCallback) num_rows_value_changed, pager);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pager->num_rows_spin), pager->n_rows);
	gtk_label_set_text (GTK_LABEL (pager->label_row_col), pager->orientation == GTK_ORIENTATION_HORIZONTAL ? _("rows") : _("columns"));

	g_signal_connect (pager->properties_dialog, "delete_event",
			  G_CALLBACK (delete_event),
			  pager);
	g_signal_connect (pager->properties_dialog, "response",
			  G_CALLBACK (response_cb),
			  pager);
	
	g_signal_connect (WID ("done_button"), "clicked",
			  (GCallback) close_dialog, pager);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pager->num_workspaces_spin),
				   wnck_screen_get_workspace_count (pager->screen));
	g_signal_connect (G_OBJECT (pager->num_workspaces_spin), "value_changed",
			  (GCallback) num_workspaces_value_changed, pager);
	
	wncklet_connect_while_alive (pager->screen, "workspace_created",
				     G_CALLBACK(workspace_created),
				     pager,
				     pager->applet);

	wncklet_connect_while_alive (pager->screen, "workspace_destroyed",
				     G_CALLBACK(workspace_destroyed),
				     pager,
				     pager->applet);

	g_signal_connect (G_OBJECT (pager->workspaces_tree), "focus_out_event",
			  (GCallback) workspaces_tree_focused_out, pager);

	pager->workspaces_store = gtk_list_store_new (1, G_TYPE_STRING, NULL);
	update_workspaces_model (pager);
	gtk_tree_view_set_model (GTK_TREE_VIEW (pager->workspaces_tree), GTK_TREE_MODEL (pager->workspaces_store));

	g_object_unref (pager->workspaces_store);

	cell = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT, "editable", TRUE, NULL);
	column = gtk_tree_view_column_new_with_attributes ("workspace",
							   cell,
							   "text", 0,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (pager->workspaces_tree), column);
	g_signal_connect (cell, "edited",
			  (GCallback) workspace_name_edited, pager);
	
	nr_ws = wnck_screen_get_workspace_count (pager->screen);
	for (i = 0; i < nr_ws; i++) {
		wncklet_connect_while_alive (
				G_OBJECT (wnck_screen_get_workspace (pager->screen, i)),
			   	"name_changed",
				G_CALLBACK(workspace_renamed),
				pager,
				pager->applet);
	}
}

static void 
display_properties_dialog (BonoboUIComponent *uic,
			   PagerData         *pager,
			   const gchar       *verbname)
{
	if (pager->properties_dialog == NULL) {
		GladeXML  *xml;

		xml = glade_xml_new (PAGER_GLADEDIR "/workspace-switcher.glade", NULL, NULL);
		pager->properties_dialog = glade_xml_get_widget (xml, "pager_properties_dialog");

		g_object_add_weak_pointer (G_OBJECT (pager->properties_dialog), 
					   (gpointer *) &pager->properties_dialog);

		setup_dialog (xml, pager);
		
		g_object_unref (G_OBJECT (xml));
	}

	gtk_window_set_icon_name (GTK_WINDOW (pager->properties_dialog),
	                          "gnome-panel-workspace-switcher");
	gtk_window_set_screen (GTK_WINDOW (pager->properties_dialog),
			       gtk_widget_get_screen (pager->applet));
	gtk_window_present (GTK_WINDOW (pager->properties_dialog));
}
