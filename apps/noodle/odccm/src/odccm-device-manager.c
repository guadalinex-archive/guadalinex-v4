/*
 * Copyright (C) 2006 Ole André Vadla Ravnås <oleavr@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <gnet.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <libhal.h>

#include "odccm-device-manager.h"

#include "odccm-device-manager-glue.h"

#include "odccm-device.h"
#include "util.h"

/* FIXME: make these configurable */
#define DEVICE_IP_ADDRESS "169.254.2.1"
#define LOCAL_IP_ADDRESS "169.254.2.2"
#define LOCAL_NETMASK    "255.255.255.0"
#define LOCAL_BROADCAST  "169.254.2.255"

G_DEFINE_TYPE (OdccmDeviceManager, odccm_device_manager, G_TYPE_OBJECT)

/* signals */
enum
{
  DEVICE_ATTACHED,
  DEVICE_DETACHED,
  DEVICE_CONNECTED,
  DEVICE_DISCONNECTED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

/* private stuff */
typedef struct _OdccmDeviceManagerPrivate OdccmDeviceManagerPrivate;

struct _OdccmDeviceManagerPrivate
{
  gboolean dispose_has_run;

  GServer *server;

  GSList *devices;

  LibHalContext *hal_ctx;
  gchar *udi;
  gchar *ifname;
};

#define ODCCM_DEVICE_MANAGER_GET_PRIVATE(o) \
    (G_TYPE_INSTANCE_GET_PRIVATE((o), ODCCM_TYPE_DEVICE_MANAGER, OdccmDeviceManagerPrivate))

static void
odccm_device_manager_init (OdccmDeviceManager *self)
{
}

static void client_connected_cb (GServer *server, GConn *conn, gpointer user_data);
static void init_hal (OdccmDeviceManager *self);

static GObject *
odccm_device_manager_constructor (GType type, guint n_props,
                                  GObjectConstructParam *props)
{
  GObject *obj;
  OdccmDeviceManagerPrivate *priv;

  obj = G_OBJECT_CLASS (odccm_device_manager_parent_class)->
    constructor (type, n_props, props);

  priv = ODCCM_DEVICE_MANAGER_GET_PRIVATE (obj);

  priv->server = gnet_server_new (NULL, 990, client_connected_cb, obj);

  dbus_g_connection_register_g_object (_odccm_get_dbus_conn (),
                                       DEVICE_MANAGER_OBJECT_PATH, obj);

  init_hal (ODCCM_DEVICE_MANAGER (obj));

  return obj;
}

static void
odccm_device_manager_dispose (GObject *obj)
{
  OdccmDeviceManager *self = ODCCM_DEVICE_MANAGER (obj);
  OdccmDeviceManagerPrivate *priv = ODCCM_DEVICE_MANAGER_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (G_OBJECT_CLASS (odccm_device_manager_parent_class)->dispose)
    G_OBJECT_CLASS (odccm_device_manager_parent_class)->dispose (obj);
}

static void
odccm_device_manager_finalize (GObject *obj)
{
  OdccmDeviceManager *self = ODCCM_DEVICE_MANAGER (obj);
  OdccmDeviceManagerPrivate *priv = ODCCM_DEVICE_MANAGER_GET_PRIVATE (self);

  g_free (priv->udi);
  g_free (priv->ifname);

  G_OBJECT_CLASS (odccm_device_manager_parent_class)->finalize (obj);
}

static void
odccm_device_manager_class_init (OdccmDeviceManagerClass *dev_mgr_class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (dev_mgr_class);

  g_type_class_add_private (dev_mgr_class,
                            sizeof (OdccmDeviceManagerPrivate));

  obj_class->constructor = odccm_device_manager_constructor;

  obj_class->dispose = odccm_device_manager_dispose;
  obj_class->finalize = odccm_device_manager_finalize;

  signals[DEVICE_ATTACHED] =
    g_signal_new ("device-attached",
                  G_OBJECT_CLASS_TYPE (dev_mgr_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  signals[DEVICE_DETACHED] =
    g_signal_new ("device-detached",
                  G_OBJECT_CLASS_TYPE (dev_mgr_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  signals[DEVICE_CONNECTED] =
    g_signal_new ("device-connected",
                  G_OBJECT_CLASS_TYPE (dev_mgr_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);

  signals[DEVICE_DISCONNECTED] =
    g_signal_new ("device-disconnected",
                  G_OBJECT_CLASS_TYPE (dev_mgr_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);

  dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (dev_mgr_class),
                                   &dbus_glib_odccm_device_manager_object_info);
}

static void device_obj_path_changed_cb (GObject *obj, GParamSpec *param, gpointer user_data);

static void
client_connected_cb (GServer *server,
                     GConn *conn,
                     gpointer user_data)
{
  OdccmDeviceManager *self = ODCCM_DEVICE_MANAGER (user_data);
  OdccmDeviceManagerPrivate *priv = ODCCM_DEVICE_MANAGER_GET_PRIVATE (self);
  struct in_addr addr;
  OdccmDevice *dev = NULL;
  GSList *cur;

  if (conn == NULL)
    {
      g_warning ("%s: an error occurred", G_STRFUNC);
      return;
    }

  gnet_inetaddr_get_bytes (gnet_tcp_socket_get_remote_inetaddr (conn->socket),
                           (gchar *) &(addr.s_addr));

  for (cur = priv->devices; cur != NULL && dev == NULL; cur = cur->next)
    {
      guint32 cur_addr;

      g_object_get (cur->data, "ip-address", &cur_addr, NULL);

      if (cur_addr == addr.s_addr)
        {
          dev = cur->data;
        }
    }

  if (dev != NULL)
    {
      _odccm_device_client_connected (dev, conn);
    }
  else
    {
      dev = g_object_new (ODCCM_TYPE_DEVICE, "connection", conn, NULL);
      priv->devices = g_slist_append (priv->devices, dev);

      g_signal_connect (dev, "notify::object-path",
                        (GCallback) device_obj_path_changed_cb,
                        self);

      gnet_conn_unref (conn);
    }
}

static void
device_obj_path_changed_cb (GObject    *obj,
                            GParamSpec *param,
                            gpointer    user_data)
{
  OdccmDeviceManager *self = ODCCM_DEVICE_MANAGER (user_data);
  OdccmDevice *dev = ODCCM_DEVICE (obj);

  if (_odccm_device_is_identified (dev))
    {
      gchar *obj_path;

      g_object_get (dev, "object-path", &obj_path, NULL);

      g_signal_emit (self, signals[DEVICE_CONNECTED], 0, obj_path);

      g_free (obj_path);
    }
}

gboolean
odccm_device_manager_get_connected_devices (OdccmDeviceManager *self,
                                            GPtrArray **ret,
                                            GError **error)
{
  OdccmDeviceManagerPrivate *priv = ODCCM_DEVICE_MANAGER_GET_PRIVATE (self);
  GSList *cur;

  *ret = g_ptr_array_new ();
  for (cur = priv->devices; cur != NULL; cur = cur->next)
    {
      OdccmDevice *dev = cur->data;

      if (_odccm_device_is_identified (dev))
        {
          gchar *obj_path;

          g_object_get (dev, "object-path", &obj_path, NULL);
          g_ptr_array_add (*ret, obj_path);
        }
    }

  return TRUE;
}

static gboolean
interface_timed_cb (gpointer data)
{
  OdccmDeviceManager *self = ODCCM_DEVICE_MANAGER (data);
  OdccmDeviceManagerPrivate *priv = ODCCM_DEVICE_MANAGER_GET_PRIVATE (self);

  /* Has the device disappeared for some reason? */
  if (priv->udi == NULL)
    return FALSE;

  /* Has it connected? */
  if (priv->devices != NULL)
    return FALSE;

  /* Has the device been re-configured? */
  if (!_odccm_interface_is_configured (priv->ifname, LOCAL_IP_ADDRESS))
    {
      /* It has, reconfigure and try again on the next tick. */
      _odccm_configure_interface (priv->ifname, LOCAL_IP_ADDRESS, LOCAL_NETMASK,
                                  LOCAL_BROADCAST);

      return TRUE;
    }

  /* Send a trigger-packet to make the device connect. */
  _odccm_trigger_connection (self);

  return TRUE;
}

static void
hal_device_added_cb (LibHalContext *ctx, const gchar *udi)
{
  OdccmDeviceManager *self = ODCCM_DEVICE_MANAGER (
      libhal_ctx_get_user_data (ctx));
  OdccmDeviceManagerPrivate *priv = ODCCM_DEVICE_MANAGER_GET_PRIVATE (self);
  DBusError error;
  gchar *ifname;

  dbus_error_init (&error);

  ifname = libhal_device_get_property_string (ctx, udi, "net.interface", &error);
  if (ifname != NULL)
    {
      if (strncmp (ifname, "rndis", 5) == 0)
        {
          g_debug ("PDA network interface discovered! udi='%s', device='%s'",
                   udi, ifname);

          if (priv->udi != NULL)
            {
              g_warning ("Only the first device discovered will be auto-"
                         "configured for now");
              return;
            }

          priv->udi = g_strdup (udi);
          priv->ifname = g_strdup (ifname);

          _odccm_configure_interface (ifname, LOCAL_IP_ADDRESS, LOCAL_NETMASK,
                                      LOCAL_BROADCAST);

          g_timeout_add (50, interface_timed_cb, self);
        }

      libhal_free_string (ifname);
    }

  dbus_error_free (&error);
}

static void
hal_device_removed_cb (LibHalContext *ctx, const gchar *udi)
{
  OdccmDeviceManager *self = ODCCM_DEVICE_MANAGER (
      libhal_ctx_get_user_data (ctx));
  OdccmDeviceManagerPrivate *priv = ODCCM_DEVICE_MANAGER_GET_PRIVATE (self);
  GSList *cur;

  if (priv->udi == NULL)
    return;

  if (strcmp (udi, priv->udi) != 0)
    return;

  g_debug ("PDA disconnected! udi='%s', device='%s'", priv->udi, priv->ifname);

  /* FIXME: Hack.  We should have a hashtable mapping UDIs to IP-addresses and
   *        use that instead of this ugly approach... */
  for (cur = priv->devices; cur != NULL; cur = cur->next)
    {
      OdccmDevice *dev = cur->data;
      guint32 addr;

      g_object_get (dev, "ip-address", &addr, NULL);

      if (addr == inet_addr (DEVICE_IP_ADDRESS))
        {
          gchar *obj_path;

          g_object_get (dev, "object-path", &obj_path, NULL);
          g_signal_emit (self, signals[DEVICE_DISCONNECTED], 0, obj_path);
          g_free (obj_path);

          priv->devices = g_slist_delete_link (priv->devices, cur);
          g_object_unref (dev);

          break;
        }
    }

  g_free (priv->udi);
  priv->udi = NULL;
  g_free (priv->ifname);
  priv->ifname = NULL;
}

static void
init_hal (OdccmDeviceManager *self)
{
  OdccmDeviceManagerPrivate *priv = ODCCM_DEVICE_MANAGER_GET_PRIVATE (self);
  gboolean success = FALSE, initialized = FALSE;
  DBusConnection *conn;
  const gchar *func_name = "";
  DBusError error;

  dbus_error_init (&error);

  if ((conn = dbus_bus_get (DBUS_BUS_SYSTEM, &error)) == NULL)
    {
      func_name = "dbus_bus_get";
      goto DBUS_ERROR;
    }

  dbus_connection_setup_with_g_main (conn, NULL);

  priv->hal_ctx = libhal_ctx_new ();
  if (priv->hal_ctx == NULL)
    goto OUT;

  if (!libhal_ctx_set_dbus_connection (priv->hal_ctx, conn))
    goto OUT;

  if (!libhal_ctx_init (priv->hal_ctx, &error))
    {
      func_name = "libhal_ctx_init";
      goto DBUS_ERROR;
    }

  initialized = TRUE;

  if (!libhal_ctx_set_user_data (priv->hal_ctx, self))
    goto OUT;

  if (!libhal_ctx_set_device_added (priv->hal_ctx, hal_device_added_cb))
    goto OUT;

  if (!libhal_ctx_set_device_removed (priv->hal_ctx, hal_device_removed_cb))
    goto OUT;

  success = TRUE;
  goto OUT;

DBUS_ERROR:
  g_warning ("%s failed with D-Bus error %s: %s\n",
             func_name, error.name, error.message);

  dbus_error_free (&error);

OUT:
  if (!success)
    {
      if (!initialized)
        {
          libhal_ctx_free (priv->hal_ctx);
          priv->hal_ctx = NULL;
        }

      g_warning ("%s: failed", G_STRFUNC);
    }
}

