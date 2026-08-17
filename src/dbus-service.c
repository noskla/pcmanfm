/*
 *      dbus-service.c: org.freedesktop.FileManager1 DBus interface
 *
 *      Copyright 2025
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "dbus-service.h"

#include <gio/gio.h>
#include <libfm/fm-gtk.h>

#include "pcmanfm.h"
#include "main-win.h"
#include "tab-page.h"

#define FM1_BUS_NAME       "org.freedesktop.FileManager1"
#define FM1_OBJECT_PATH    "/org/freedesktop/FileManager1"
#define FM1_INTERFACE_NAME "org.freedesktop.FileManager1"

static const gchar introspection_xml[] =
"<node>"
"  <interface name='org.freedesktop.FileManager1'>"
"    <method name='ShowItems'>"
"      <arg type='as' name='URIs' direction='in'/>"
"      <arg type='s' name='StartupId' direction='in'/>"
"    </method>"
"    <method name='ShowFolders'>"
"      <arg type='as' name='URIs' direction='in'/>"
"      <arg type='s' name='StartupId' direction='in'/>"
"    </method>"
"    <method name='ShowItemProperties'>"
"      <arg type='as' name='URIs' direction='in'/>"
"      <arg type='s' name='StartupId' direction='in'/>"
"    </method>"
"  </interface>"
"</node>";

static GDBusNodeInfo *introspection_data = NULL;
static guint owner_id = 0;
static guint reg_id = 0;
static GDBusConnection *dbus_connection = NULL;

static void
handle_method_call(GDBusConnection *connection,
                    const gchar *sender,
                    const gchar *object_path,
                    const gchar *interface_name,
                    const gchar *method_name,
                    GVariant *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer user_data)
{
    g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                          G_DBUS_ERROR_UNKNOWN_METHOD,
                                          "Unknown method %s", method_name);
}

static const GDBusInterfaceVTable interface_vtable =
{
    handle_method_call,
    NULL,
    NULL
};

static void
on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    GError *error = NULL;

    dbus_connection = connection;
    reg_id = g_dbus_connection_register_object(connection,
                                               FM1_OBJECT_PATH,
                                               introspection_data->interfaces[0],
                                               &interface_vtable,
                                               NULL, NULL, &error);
    if (reg_id == 0)
    {
        g_warning("Failed to register DBus object: %s", error->message);
        g_error_free(error);
    }
}

static void
on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    /* g_debug("acquired DBus name %s", name); */
}

static void
on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    /* another instance may already own it, or bus is unavailable; ignore */
}

void pcmanfm_dbus_service_init(void)
{
    if (owner_id != 0)
        return; /* already initialized */

    introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, NULL);
    if (introspection_data == NULL)
        return;

    owner_id = g_bus_own_name(G_BUS_TYPE_SESSION,
                              FM1_BUS_NAME,
                              G_BUS_NAME_OWNER_FLAGS_NONE,
                              on_bus_acquired,
                              on_name_acquired,
                              on_name_lost,
                              NULL, NULL);
}

void pcmanfm_dbus_service_finalize(void)
{
    if (reg_id != 0 && dbus_connection != NULL)
    {
        g_dbus_connection_unregister_object(dbus_connection, reg_id);
        reg_id = 0;
    }
    if (owner_id != 0)
    {
        g_bus_unown_name(owner_id);
        owner_id = 0;
    }
    if (introspection_data != NULL)
    {
        g_dbus_node_info_unref(introspection_data);
        introspection_data = NULL;
    }
    dbus_connection = NULL;
}
