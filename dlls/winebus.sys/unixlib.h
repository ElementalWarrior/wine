/*
 * Copyright 2021 Rémi Bernon for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __WINEBUS_UNIXLIB_H
#define __WINEBUS_UNIXLIB_H

#include <stdarg.h>

#include <windef.h>
#include <winbase.h>
#include <winternl.h>
#include <ddk/wdm.h>
#include <hidusage.h>

#include "wine/list.h"
#include "wine/debug.h"

struct device_desc
{
    const WCHAR *bus_id;
    DWORD vendor_id;
    DWORD product_id;
    DWORD version;
    DWORD interface;
    DWORD location_id;
    WCHAR serial[256];
    BOOL is_gamepad;
};

struct sdl_bus_options
{
    BOOL map_controllers;
};

struct udev_bus_options
{
    BOOL disable_hidraw;
    BOOL disable_input;
};

struct iohid_bus_options
{
};

struct unix_device;

enum bus_event_type
{
    BUS_EVENT_TYPE_NONE,
    BUS_EVENT_TYPE_DEVICE_REMOVED,
};

struct bus_event
{
    struct list entry;
    enum bus_event_type type;

    union
    {
        struct
        {
            const WCHAR *bus_id;
            void *context;
        } device_removed;
    };
};

struct unix_funcs
{
    NTSTATUS (WINAPI *sdl_bus_init)(void *);
    NTSTATUS (WINAPI *sdl_bus_wait)(void *);
    NTSTATUS (WINAPI *sdl_bus_stop)(void);

    NTSTATUS (WINAPI *udev_bus_init)(void *);
    NTSTATUS (WINAPI *udev_bus_wait)(void *);
    NTSTATUS (WINAPI *udev_bus_stop)(void);

    NTSTATUS (WINAPI *iohid_bus_init)(void *);
    NTSTATUS (WINAPI *iohid_bus_wait)(void *);
    NTSTATUS (WINAPI *iohid_bus_stop)(void);

    NTSTATUS (WINAPI *mouse_device_create)(struct unix_device **, struct device_desc *);
    NTSTATUS (WINAPI *keyboard_device_create)(struct unix_device **, struct device_desc *);
};

static inline const char *debugstr_device_desc(struct device_desc *desc)
{
    if (!desc) return "(null)";
    return wine_dbg_sprintf("{bus %s, vid %04x, pid %04x, rev %04x, interface %d, location_id %08x, serial %s, is_gamepad %u}",
                            debugstr_w(desc->bus_id), desc->vendor_id, desc->product_id, desc->version,
                            desc->interface, desc->location_id, debugstr_w(desc->serial), desc->is_gamepad);
}

#endif /* __WINEBUS_UNIXLIB_H */
