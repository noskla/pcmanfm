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
show_folder(FmPath *folder_path)
{
    FmMainWin *win = fm_main_win_get_last_active();

    if (win == NULL)
    {
        win = fm_main_win_add_win(NULL, folder_path);
        pcmanfm_ref();
    }
    else
    {
        fm_main_win_add_tab(win, folder_path);
    }
    gtk_window_present(GTK_WINDOW(win));
}

static gboolean
uri_strv_to_paths(const gchar * const *uris, GList **paths_out, GError **error)
{
    GList *paths = NULL;
    const gchar * const *p;

    for (p = uris; p && *p; ++p)
    {
        FmPath *path = fm_path_new_for_uri(*p);
        if (path == NULL)
        {
            g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                        "Invalid URI: %s", *p);
            g_list_free_full(paths, (GDestroyNotify)fm_path_unref);
            return FALSE;
        }
        paths = g_list_append(paths, path);
    }
    *paths_out = paths;
    return TRUE;
}

static void
handle_show_folders(const gchar * const *uris)
{
    GList *paths = NULL, *l;
    GError *error = NULL;

    if (!uri_strv_to_paths(uris, &paths, &error))
    {
        g_warning("ShowFolders: %s", error->message);
        g_error_free(error);
        return;
    }

    for (l = paths; l; l = l->next)
        show_folder((FmPath*)l->data);

    g_list_free_full(paths, (GDestroyNotify)fm_path_unref);
}

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
    gchar **uris = NULL;
    const gchar *startup_id = NULL;

    if (g_strcmp0(method_name, "ShowFolders") == 0)
    {
        g_variant_get(parameters, "(^ass)", &uris, &startup_id);
        handle_show_folders((const gchar * const *)uris);
        g_strfreev(uris);
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else
    {
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                              G_DBUS_ERROR_UNKNOWN_METHOD,
                                              "Unknown method %s", method_name);
    }
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
