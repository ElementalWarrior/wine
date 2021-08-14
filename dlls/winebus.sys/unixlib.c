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

#include "config.h"

#include <stdarg.h>
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "ddk/hidtypes.h"

#include "wine/debug.h"
#include "wine/list.h"

#include "bus.h"
#include "unix_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(plugplay);

static struct hid_descriptor mouse_desc;
static struct hid_descriptor keyboard_desc;

static void mouse_free_device(DEVICE_OBJECT *device)
{
}

static NTSTATUS mouse_start_device(DEVICE_OBJECT *device)
{
    if (!hid_descriptor_begin(&mouse_desc, HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_MOUSE))
        return STATUS_NO_MEMORY;
    if (!hid_descriptor_add_buttons(&mouse_desc, HID_USAGE_PAGE_BUTTON, 1, 3))
        return STATUS_NO_MEMORY;
    if (!hid_descriptor_end(&mouse_desc))
        return STATUS_NO_MEMORY;

    return STATUS_SUCCESS;
}

static NTSTATUS mouse_get_reportdescriptor(DEVICE_OBJECT *device, BYTE *buffer, DWORD length, DWORD *ret_length)
{
    TRACE("buffer %p, length %u.\n", buffer, length);

    *ret_length = mouse_desc.size;
    if (length < mouse_desc.size) return STATUS_BUFFER_TOO_SMALL;

    memcpy(buffer, mouse_desc.data, mouse_desc.size);
    return STATUS_SUCCESS;
}

static NTSTATUS mouse_get_string(DEVICE_OBJECT *device, DWORD index, WCHAR *buffer, DWORD length)
{
    static const WCHAR nameW[] = {'W','i','n','e',' ','H','I','D',' ','m','o','u','s','e',0};
    if (index != HID_STRING_ID_IPRODUCT)
        return STATUS_NOT_IMPLEMENTED;
    if (length < ARRAY_SIZE(nameW))
        return STATUS_BUFFER_TOO_SMALL;
    lstrcpyW(buffer, nameW);
    return STATUS_SUCCESS;
}

static void mouse_set_output_report(DEVICE_OBJECT *device, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    FIXME("id %u, stub!\n", packet->reportId);
    io->Information = 0;
    io->Status = STATUS_NOT_IMPLEMENTED;
}

static void mouse_get_feature_report(DEVICE_OBJECT *device, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    FIXME("id %u, stub!\n", packet->reportId);
    io->Information = 0;
    io->Status = STATUS_NOT_IMPLEMENTED;
}

static void mouse_set_feature_report(DEVICE_OBJECT *device, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    FIXME("id %u, stub!\n", packet->reportId);
    io->Information = 0;
    io->Status = STATUS_NOT_IMPLEMENTED;
}

const platform_vtbl mouse_vtbl =
{
    .free_device = mouse_free_device,
    .start_device = mouse_start_device,
    .get_reportdescriptor = mouse_get_reportdescriptor,
    .get_string = mouse_get_string,
    .set_output_report = mouse_set_output_report,
    .get_feature_report = mouse_get_feature_report,
    .set_feature_report = mouse_set_feature_report,
};

static struct unix_device mouse_device;

static NTSTATUS WINAPI mouse_device_create(struct unix_device **device)
{
    *device = &mouse_device;
    return STATUS_SUCCESS;
}

static void keyboard_free_device(DEVICE_OBJECT *device)
{
}

static NTSTATUS keyboard_start_device(DEVICE_OBJECT *device)
{
    if (!hid_descriptor_begin(&keyboard_desc, HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYBOARD))
        return STATUS_NO_MEMORY;
    if (!hid_descriptor_add_buttons(&keyboard_desc, HID_USAGE_PAGE_KEYBOARD, 0, 101))
        return STATUS_NO_MEMORY;
    if (!hid_descriptor_end(&keyboard_desc))
        return STATUS_NO_MEMORY;

    return STATUS_SUCCESS;
}

static NTSTATUS keyboard_get_reportdescriptor(DEVICE_OBJECT *device, BYTE *buffer, DWORD length, DWORD *ret_length)
{
    TRACE("buffer %p, length %u.\n", buffer, length);

    *ret_length = keyboard_desc.size;
    if (length < keyboard_desc.size) return STATUS_BUFFER_TOO_SMALL;

    memcpy(buffer, keyboard_desc.data, keyboard_desc.size);
    return STATUS_SUCCESS;
}

static NTSTATUS keyboard_get_string(DEVICE_OBJECT *device, DWORD index, WCHAR *buffer, DWORD length)
{
    static const WCHAR nameW[] = {'W','i','n','e',' ','H','I','D',' ','k','e','y','b','o','a','r','d',0};
    if (index != HID_STRING_ID_IPRODUCT)
        return STATUS_NOT_IMPLEMENTED;
    if (length < ARRAY_SIZE(nameW))
        return STATUS_BUFFER_TOO_SMALL;
    lstrcpyW(buffer, nameW);
    return STATUS_SUCCESS;
}

static void keyboard_set_output_report(DEVICE_OBJECT *device, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    FIXME("id %u, stub!\n", packet->reportId);
    io->Information = 0;
    io->Status = STATUS_NOT_IMPLEMENTED;
}

static void keyboard_get_feature_report(DEVICE_OBJECT *device, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    FIXME("id %u, stub!\n", packet->reportId);
    io->Information = 0;
    io->Status = STATUS_NOT_IMPLEMENTED;
}

static void keyboard_set_feature_report(DEVICE_OBJECT *device, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    FIXME("id %u, stub!\n", packet->reportId);
    io->Information = 0;
    io->Status = STATUS_NOT_IMPLEMENTED;
}

const platform_vtbl keyboard_vtbl =
{
    .free_device = keyboard_free_device,
    .start_device = keyboard_start_device,
    .get_reportdescriptor = keyboard_get_reportdescriptor,
    .get_string = keyboard_get_string,
    .set_output_report = keyboard_set_output_report,
    .get_feature_report = keyboard_get_feature_report,
    .set_feature_report = keyboard_set_feature_report,
};

static struct unix_device keyboard_device;

static NTSTATUS WINAPI keyboard_device_create(struct unix_device **device)
{
    *device = &keyboard_device;
    return STATUS_SUCCESS;
}

static const struct unix_funcs unix_funcs =
{
    sdl_bus_init,
    sdl_bus_wait,
    sdl_bus_stop,
    udev_bus_init,
    udev_bus_wait,
    udev_bus_stop,
    iohid_bus_init,
    iohid_bus_wait,
    iohid_bus_stop,
    mouse_device_create,
    keyboard_device_create,
};

NTSTATUS CDECL __wine_init_unix_lib(HMODULE module, DWORD reason, const void *ptr_in, void *ptr_out)
{
    TRACE("module %p, reason %u, ptr_in %p, ptr_out %p\n", module, reason, ptr_in, ptr_out);

    if (reason != DLL_PROCESS_ATTACH) return STATUS_SUCCESS;
    *(const struct unix_funcs **)ptr_out = &unix_funcs;
    return STATUS_SUCCESS;
}

void bus_event_destroy(struct bus_event *event)
{
    HeapFree(GetProcessHeap(), 0, event);
}

void bus_event_queue_destroy(struct list *queue)
{
    struct bus_event *event, *next;

    LIST_FOR_EACH_ENTRY_SAFE(event, next, queue, struct bus_event, entry)
    {
        list_remove(&event->entry);
        bus_event_destroy(event);
    }
}

BOOL bus_event_queue_device_removed(struct list *queue, const WCHAR *bus_id, void *context)
{
    struct bus_event *event = HeapAlloc(GetProcessHeap(), 0, sizeof(*event));
    if (!event) return FALSE;

    event->type = BUS_EVENT_TYPE_DEVICE_REMOVED;
    event->device_removed.bus_id = bus_id;
    event->device_removed.context = context;
    list_add_tail(queue, &event->entry);

    return TRUE;
}

BOOL bus_event_queue_pop(struct list *queue, struct bus_event **event)
{
    struct list *entry = list_head(queue);
    if (!entry) return FALSE;

    *event = LIST_ENTRY(entry, struct bus_event, entry);
    list_remove(entry);

    return TRUE;
}
