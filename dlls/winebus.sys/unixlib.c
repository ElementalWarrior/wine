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

#if 0
#pragma makedep unix
#endif

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

#include "unix_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(plugplay);

static struct hid_descriptor mouse_desc;
static struct hid_descriptor keyboard_desc;

static void mouse_remove(struct unix_device *iface)
{
}

static NTSTATUS mouse_start(struct unix_device *iface)
{
    if (!hid_descriptor_begin(&mouse_desc, HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_MOUSE))
        return STATUS_NO_MEMORY;
    if (!hid_descriptor_add_buttons(&mouse_desc, HID_USAGE_PAGE_BUTTON, 1, 3))
        return STATUS_NO_MEMORY;
    if (!hid_descriptor_end(&mouse_desc))
        return STATUS_NO_MEMORY;

    return STATUS_SUCCESS;
}

static void mouse_stop(struct unix_device *iface)
{
}

static NTSTATUS mouse_get_report_descriptor(struct unix_device *iface, BYTE *buffer, DWORD length, DWORD *ret_length)
{
    TRACE("buffer %p, length %u.\n", buffer, length);

    *ret_length = mouse_desc.size;
    if (length < mouse_desc.size) return STATUS_BUFFER_TOO_SMALL;

    memcpy(buffer, mouse_desc.data, mouse_desc.size);
    return STATUS_SUCCESS;
}

static void mouse_set_output_report(struct unix_device *iface, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    FIXME("id %u, stub!\n", packet->reportId);
    io->Information = 0;
    io->Status = STATUS_NOT_IMPLEMENTED;
}

static void mouse_get_feature_report(struct unix_device *iface, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    FIXME("id %u, stub!\n", packet->reportId);
    io->Information = 0;
    io->Status = STATUS_NOT_IMPLEMENTED;
}

static void mouse_set_feature_report(struct unix_device *iface, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    FIXME("id %u, stub!\n", packet->reportId);
    io->Information = 0;
    io->Status = STATUS_NOT_IMPLEMENTED;
}

static const struct unix_device_vtbl mouse_vtbl =
{
    mouse_remove,
    mouse_start,
    mouse_stop,
    mouse_get_report_descriptor,
    mouse_set_output_report,
    mouse_get_feature_report,
    mouse_set_feature_report,
};

static const struct device_desc mouse_device_desc =
{
    .vendor_id = 0,
    .product_id = 0,
    .version = 0,
    .interface = -1,
    .location_id = 0,
    .is_gamepad = FALSE,
    .manufacturer = {"The Wine Project"},
    .product = {"Wine HID mouse"},
    .serialnumber = {"0000"},
};
static struct unix_device mouse_device = {.vtbl = &mouse_vtbl};

static NTSTATUS WINAPI mouse_device_create(struct unix_device **device, struct device_desc *desc)
{
    *device = &mouse_device;
    *desc = mouse_device_desc;
    return STATUS_SUCCESS;
}

static void keyboard_remove(struct unix_device *device)
{
}

static NTSTATUS keyboard_start(struct unix_device *iface)
{
    if (!hid_descriptor_begin(&keyboard_desc, HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYBOARD))
        return STATUS_NO_MEMORY;
    if (!hid_descriptor_add_buttons(&keyboard_desc, HID_USAGE_PAGE_KEYBOARD, 0, 101))
        return STATUS_NO_MEMORY;
    if (!hid_descriptor_end(&keyboard_desc))
        return STATUS_NO_MEMORY;

    return STATUS_SUCCESS;
}

static void keyboard_stop(struct unix_device *device)
{
}

static NTSTATUS keyboard_get_report_descriptor(struct unix_device *iface, BYTE *buffer, DWORD length, DWORD *ret_length)
{
    TRACE("buffer %p, length %u.\n", buffer, length);

    *ret_length = keyboard_desc.size;
    if (length < keyboard_desc.size) return STATUS_BUFFER_TOO_SMALL;

    memcpy(buffer, keyboard_desc.data, keyboard_desc.size);
    return STATUS_SUCCESS;
}

static void keyboard_set_output_report(struct unix_device *iface, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    FIXME("id %u, stub!\n", packet->reportId);
    io->Information = 0;
    io->Status = STATUS_NOT_IMPLEMENTED;
}

static void keyboard_get_feature_report(struct unix_device *iface, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    FIXME("id %u, stub!\n", packet->reportId);
    io->Information = 0;
    io->Status = STATUS_NOT_IMPLEMENTED;
}

static void keyboard_set_feature_report(struct unix_device *iface, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    FIXME("id %u, stub!\n", packet->reportId);
    io->Information = 0;
    io->Status = STATUS_NOT_IMPLEMENTED;
}

static const struct unix_device_vtbl keyboard_vtbl =
{
    keyboard_remove,
    keyboard_start,
    keyboard_stop,
    keyboard_get_report_descriptor,
    keyboard_set_output_report,
    keyboard_get_feature_report,
    keyboard_set_feature_report,
};

static const struct device_desc keyboard_device_desc =
{
    .vendor_id = 0,
    .product_id = 0,
    .version = 0,
    .interface = -1,
    .location_id = 0,
    .is_gamepad = FALSE,
    .manufacturer = {"The Wine Project"},
    .product = {"Wine HID keyboard"},
    .serialnumber = {"0000"},
};
static struct unix_device keyboard_device = {.vtbl = &keyboard_vtbl};

static NTSTATUS WINAPI keyboard_device_create(struct unix_device **device, struct device_desc *desc)
{
    *device = &keyboard_device;
    *desc = keyboard_device_desc;
    return STATUS_SUCCESS;
}

void *unix_device_create(const struct unix_device_vtbl *vtbl, SIZE_T size)
{
    struct unix_device *iface;

    if (!(iface = RtlAllocateHeap(GetProcessHeap(), HEAP_ZERO_MEMORY, size))) return NULL;
    iface->vtbl = vtbl;
    iface->ref = 1;

    return iface;
}

void unix_device_destroy(struct unix_device *iface)
{
    RtlFreeHeap(GetProcessHeap(), 0, iface);
}

static void unix_device_decref(struct unix_device *iface)
{
    if (!InterlockedDecrement(&iface->ref)) iface->vtbl->destroy(iface);
}

static ULONG unix_device_incref(struct unix_device *iface)
{
    return InterlockedIncrement(&iface->ref);
}

static void WINAPI unix_device_remove(struct unix_device *iface)
{
    iface->vtbl->stop(iface);
    unix_device_decref(iface);
}

static NTSTATUS WINAPI unix_device_start(struct unix_device *iface)
{
    return iface->vtbl->start(iface);
}

static NTSTATUS WINAPI unix_device_get_report_descriptor(struct unix_device *iface, BYTE *buffer,
                                                         DWORD length, DWORD *out_length)
{
    return iface->vtbl->get_report_descriptor(iface, buffer, length, out_length);
}

static void WINAPI unix_device_set_output_report(struct unix_device *iface, HID_XFER_PACKET *packet,
                                                 IO_STATUS_BLOCK *io)
{
    return iface->vtbl->set_output_report(iface, packet, io);
}

static void WINAPI unix_device_get_feature_report(struct unix_device *iface, HID_XFER_PACKET *packet,
                                                  IO_STATUS_BLOCK *io)
{
    return iface->vtbl->get_feature_report(iface, packet, io);
}

static void WINAPI unix_device_set_feature_report(struct unix_device *iface, HID_XFER_PACKET *packet,
                                                  IO_STATUS_BLOCK *io)
{
    return iface->vtbl->set_feature_report(iface, packet, io);
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
    unix_device_remove,
    unix_device_start,
    unix_device_get_report_descriptor,
    unix_device_set_output_report,
    unix_device_get_feature_report,
    unix_device_set_feature_report,
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
    unix_device_decref(event->device);
    RtlFreeHeap(GetProcessHeap(), 0, event);
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

BOOL bus_event_queue_device_removed(struct list *queue, struct unix_device *device)
{
    struct bus_event *event = RtlAllocateHeap(GetProcessHeap(), 0, sizeof(*event));
    if (!event) return FALSE;

    if (unix_device_incref(device) == 1) return FALSE; /* being destroyed */

    event->type = BUS_EVENT_TYPE_DEVICE_REMOVED;
    event->device = device;
    list_add_tail(queue, &event->entry);

    return TRUE;
}

BOOL bus_event_queue_device_created(struct list *queue, struct unix_device *device, struct device_desc *desc)
{
    struct bus_event *event = RtlAllocateHeap(GetProcessHeap(), 0, sizeof(*event));
    if (!event) return FALSE;

    if (unix_device_incref(device) == 1) return FALSE; /* being destroyed */

    event->type = BUS_EVENT_TYPE_DEVICE_CREATED;
    event->device = device;
    event->device_created.desc = *desc;
    list_add_tail(queue, &event->entry);

    return TRUE;
}

BOOL bus_event_queue_input_report(struct list *queue, struct unix_device *device, BYTE *report, DWORD length)
{
    struct bus_event *event = RtlAllocateHeap(GetProcessHeap(), 0, offsetof(struct bus_event, input_report.buffer[length]));
    if (!event) return FALSE;

    if (unix_device_incref(device) == 1) return FALSE; /* being destroyed */

    event->type = BUS_EVENT_TYPE_INPUT_REPORT;
    event->device = device;
    event->input_report.length = length;
    memcpy(event->input_report.buffer, report, length);
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
