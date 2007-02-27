/* panel-power-manager.c - functions for powering down, restarting, and
 *                         suspending the computer
 *
 * Copyright (C) 2006 Ray Strode <rstrode@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "panel-power-manager.h"

#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>

#include <dbus/dbus-glib.h>

struct _PanelPowerManagerPrivate {
	DBusGConnection *dbus_connection;
	DBusGProxy      *bus_proxy;
	DBusGProxy      *gpm_proxy;
	guint32          is_connected : 1;
};


static void panel_power_manager_finalize (GObject *object);
static void panel_power_manager_class_install_signals (PanelPowerManagerClass *manager_class);
static void panel_power_manager_class_install_properties (PanelPowerManagerClass *manager_class);

static void panel_power_manager_get_property (GObject    *object,
					      guint       prop_id,
					      GValue     *value,
					      GParamSpec *pspec);

static gboolean panel_power_manager_ensure_gpm_connection (PanelPowerManager  *manager,
	                                                   GError            **error);

enum {
  PROP_0 = 0,
  PROP_IS_CONNECTED
};

enum {
  REQUEST_FAILED = 0,
  NUMBER_OF_SIGNALS
};

static guint panel_power_manager_signals[NUMBER_OF_SIGNALS];

G_DEFINE_TYPE (PanelPowerManager, panel_power_manager, G_TYPE_OBJECT);

static void
panel_power_manager_class_init (PanelPowerManagerClass *manager_class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (manager_class);

	object_class->finalize = panel_power_manager_finalize;

	panel_power_manager_class_install_properties (manager_class);
	panel_power_manager_class_install_signals (manager_class);

	g_type_class_add_private (manager_class,
				  sizeof (PanelPowerManagerPrivate));
}

static void
panel_power_manager_class_install_signals (PanelPowerManagerClass *manager_class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (manager_class);

	panel_power_manager_signals[REQUEST_FAILED] =
		g_signal_new ("request-failed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (PanelPowerManagerClass, request_failed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	manager_class->request_failed = NULL;
}

static void
panel_power_manager_class_install_properties (PanelPowerManagerClass *manager_class)
{
	GObjectClass *object_class;
	GParamSpec *param_spec;

	object_class = G_OBJECT_CLASS (manager_class);
	object_class->get_property = panel_power_manager_get_property;

	param_spec = g_param_spec_boolean ("is-connected",
					   "Is connected",
					   "Whether the panel is connected to "
					   "the power manager",
					   FALSE,
					   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_IS_CONNECTED,
					 param_spec);
}

static void
panel_power_manager_on_name_owner_changed (DBusGProxy        *bus_proxy,
	                                   const char        *name,
	                                   const char        *prev_owner,
	                                   const char        *new_owner,
	                                   PanelPowerManager *manager)
{
	if (strcmp (name, "org.gnome.PowerManager") != 0)
		return;

	if (manager->priv->gpm_proxy != NULL) {
		g_object_unref (manager->priv->gpm_proxy);
		manager->priv->gpm_proxy = NULL;
	}

	panel_power_manager_ensure_gpm_connection (manager, NULL);
}

static gboolean
panel_power_manager_ensure_gpm_connection (PanelPowerManager  *manager,
					   GError            **error)
{
	GError   *connection_error;
	gboolean  is_connected;

	connection_error = NULL;
	if (manager->priv->dbus_connection == NULL) {
		manager->priv->dbus_connection = dbus_g_bus_get (DBUS_BUS_SESSION,
								 &connection_error);

		if (manager->priv->dbus_connection == NULL) {
			g_propagate_error (error, connection_error);
			is_connected = FALSE;
			goto out;
		}
	}

	if (manager->priv->bus_proxy == NULL) {
		manager->priv->bus_proxy =
			dbus_g_proxy_new_for_name_owner (manager->priv->dbus_connection,
							 DBUS_SERVICE_DBUS,
							 DBUS_PATH_DBUS,
							 DBUS_INTERFACE_DBUS,
							 &connection_error);

		if (manager->priv->bus_proxy == NULL) {
			g_propagate_error (error, connection_error);
			is_connected = FALSE;
			goto out;
		}

		dbus_g_proxy_add_signal (manager->priv->bus_proxy,
					 "NameOwnerChanged",
					 G_TYPE_STRING,
					 G_TYPE_STRING,
					 G_TYPE_STRING,
					 G_TYPE_INVALID);
		dbus_g_proxy_connect_signal (manager->priv->bus_proxy,
					     "NameOwnerChanged",
					     G_CALLBACK (panel_power_manager_on_name_owner_changed),
					     manager, NULL);
	}

	if (manager->priv->gpm_proxy == NULL) {
		manager->priv->gpm_proxy =
			dbus_g_proxy_new_for_name_owner (
					manager->priv->dbus_connection,
					"org.gnome.PowerManager",
					"/org/gnome/PowerManager",
					"org.gnome.PowerManager",
					&connection_error);

		if (manager->priv->gpm_proxy == NULL) {
			g_propagate_error (error, connection_error);
			is_connected = FALSE;
			goto out;
		}
	}
	is_connected = TRUE;

out:
	if (manager->priv->is_connected != is_connected) {
		manager->priv->is_connected = is_connected;
		g_object_notify (G_OBJECT (manager), "is-connected");
	}

	if (!is_connected) {
		if (manager->priv->dbus_connection == NULL) {
			if (manager->priv->bus_proxy != NULL) {
				g_object_unref (manager->priv->bus_proxy);
				manager->priv->bus_proxy = NULL;
			}

			if (manager->priv->gpm_proxy != NULL) {
				g_object_unref (manager->priv->gpm_proxy);
				manager->priv->gpm_proxy = NULL;
			}
		} else if (manager->priv->bus_proxy == NULL) {
			if (manager->priv->gpm_proxy != NULL) {
				g_object_unref (manager->priv->gpm_proxy);
				manager->priv->gpm_proxy = NULL;
			}
		}
	}

	return is_connected;
}

static void
panel_power_manager_init (PanelPowerManager *manager)
{
	GError *error;

	manager->priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
						     PANEL_TYPE_POWER_MANAGER,
						     PanelPowerManagerPrivate);

	manager->priv->dbus_connection = NULL;
	manager->priv->bus_proxy = NULL;
	manager->priv->gpm_proxy = NULL;
	manager->priv->is_connected = FALSE;

	error = NULL;
	if (!panel_power_manager_ensure_gpm_connection (manager, &error)) {
		g_message ("Could not connect to power manager: %s",
			   error->message);
		g_error_free (error);
	}
}

static void
panel_power_manager_finalize (GObject *object)
{
	PanelPowerManager *manager;
	GObjectClass *parent_class;

	manager = PANEL_POWER_MANAGER (object);

	parent_class = G_OBJECT_CLASS (panel_power_manager_parent_class);

	if (parent_class->finalize != NULL)
		parent_class->finalize (object);
}

static void
panel_power_manager_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
	PanelPowerManager *manager = PANEL_POWER_MANAGER (object);

	switch (prop_id) {
	case PROP_IS_CONNECTED:
		g_value_set_boolean (value,
				     manager->priv->is_connected);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
						   prop_id,
						   pspec);
	}
}

GQuark
panel_power_manager_error_quark (void)
{
	static GQuark error_quark = 0;

	if (error_quark == 0)
		error_quark = g_quark_from_static_string ("panel-power-manager-error");

	return error_quark;
}

PanelPowerManager *
panel_power_manager_new (void)
{
	PanelPowerManager *manager;

	manager = g_object_new (PANEL_TYPE_POWER_MANAGER, NULL);

	return manager;
}

gboolean
panel_power_manager_can_suspend (PanelPowerManager *manager)
{
	GError *error;
	gboolean can_suspend;

	error = NULL;

	if (!panel_power_manager_ensure_gpm_connection (manager, &error)) {
		g_message ("Could not connect to power manager: %s",
			   error->message);
		g_error_free (error);
		return FALSE;
	}

	can_suspend = FALSE;
	if (!dbus_g_proxy_call (manager->priv->gpm_proxy, "AllowedSuspend",
				&error,
				G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &can_suspend, G_TYPE_INVALID)) {
		if (error != NULL) {
			g_message ("Could not ask power manager if user can suspend: %s",
				   error->message);
			g_error_free (error);
		}
		can_suspend = FALSE;
	}

	return can_suspend;
}

gboolean
panel_power_manager_can_hibernate (PanelPowerManager *manager)
{
	GError *error;
	gboolean can_hibernate;

	error = NULL;

	if (!panel_power_manager_ensure_gpm_connection (manager, &error)) {
		g_message ("Could not connect to power manager: %s",
			   error->message);
		g_error_free (error);
		return FALSE;
	}

	can_hibernate = FALSE;
	if (!dbus_g_proxy_call (manager->priv->gpm_proxy, "AllowedHibernate",
				&error,
				G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &can_hibernate, G_TYPE_INVALID)) {
		if (error != NULL) {
			g_message ("Could not ask power manager if user can suspend: %s",
				   error->message);
			g_error_free (error);
		}
		can_hibernate = FALSE;
	}

	return can_hibernate;
}

void
panel_power_manager_attempt_suspend (PanelPowerManager *manager)
{
	GError *error;

	error = NULL;

	if (!panel_power_manager_ensure_gpm_connection (manager, &error)) {
		g_warning ("Could not connect to power manager: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	if (!dbus_g_proxy_call (manager->priv->gpm_proxy, "Suspend",
				&error,
				G_TYPE_INVALID, G_TYPE_INVALID) &&
	    error != NULL) {
		GError *call_error;

		g_warning ("Could not ask power manager to suspend: %s",
			   error->message);

		call_error = g_error_new_literal (PANEL_POWER_MANAGER_ERROR,
						  PANEL_POWER_MANAGER_ERROR_SUSPENDING,
						  error->message);
		g_error_free (error);

		g_signal_emit (G_OBJECT (manager),
			       panel_power_manager_signals[REQUEST_FAILED],
			       0, call_error);
		g_error_free (call_error);
	}
}


void
panel_power_manager_attempt_hibernate (PanelPowerManager *manager)
{
	GError *error;

	error = NULL;

	if (!panel_power_manager_ensure_gpm_connection (manager, &error)) {
		g_warning ("Could not connect to power manager: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	if (!dbus_g_proxy_call (manager->priv->gpm_proxy, "Hibernate",
				&error,
				G_TYPE_INVALID, G_TYPE_INVALID) &&
	    error != NULL) {
		GError *call_error;

		g_warning ("Could not ask power manager to hibernate: %s",
			   error->message);

		call_error = g_error_new_literal (PANEL_POWER_MANAGER_ERROR,
						  PANEL_POWER_MANAGER_ERROR_HIBERNATING,
						  error->message);
		g_error_free (error);

		g_signal_emit (G_OBJECT (manager),
			       panel_power_manager_signals[REQUEST_FAILED],
			       0, call_error);
		g_error_free (call_error);
	}
}


PanelPowerManager *
panel_get_power_manager (void)
{
	static PanelPowerManager *manager = NULL;

	if (manager == NULL)
		manager = panel_power_manager_new ();

	return g_object_ref (manager);
}
