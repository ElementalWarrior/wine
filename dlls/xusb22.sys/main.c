/*
 * WINE XUSB22 driver
 *
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

#include <stdarg.h>
#include <stdlib.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "winioctl.h"

#include "ddk/wdm.h"
#include "ddk/hidport.h"
#include "ddk/hidpddi.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(xusb22);

#if defined(__i386__) && !defined(_WIN32)

extern void * WINAPI wrap_fastcall_func1( void *func, const void *a );
__ASM_STDCALL_FUNC( wrap_fastcall_func1, 8,
                   "popl %ecx\n\t"
                   "popl %eax\n\t"
                   "xchgl (%esp),%ecx\n\t"
                   "jmp *%eax" );

#define call_fastcall_func1(func,a) wrap_fastcall_func1(func,a)

#else

#define call_fastcall_func1(func,a) func(a)

#endif

struct device_state
{
    DEVICE_OBJECT *bus_pdo;
    DEVICE_OBJECT *xinput_pdo;

    WCHAR instance_id[MAX_PATH];

    HIDP_CAPS caps;
};

struct device_extension
{
    BOOL is_fdo;
    BOOL removed;

    WCHAR bus_id[MAX_PATH];
    WCHAR device_id[MAX_PATH];
    HIDP_DEVICE_DESC device_desc;

    struct device_state *state;
};

static inline struct device_extension *ext_from_DEVICE_OBJECT(DEVICE_OBJECT *device)
{
    return (struct device_extension *)device->DeviceExtension;
}

static NTSTATUS sync_ioctl(DEVICE_OBJECT *device, DWORD code, void *in_buf, DWORD in_len, void *out_buf, DWORD out_len)
{
    IO_STATUS_BLOCK io;
    KEVENT event;
    IRP *irp;

    KeInitializeEvent(&event, NotificationEvent, FALSE);
    irp = IoBuildDeviceIoControlRequest(code, device, in_buf, in_len, out_buf, out_len, TRUE, &event, &io);
    if (IoCallDriver(device, irp) == STATUS_PENDING) KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    return io.Status;
}

static NTSTATUS WINAPI internal_ioctl(DEVICE_OBJECT *device, IRP *irp)
{
    struct device_extension *ext = ext_from_DEVICE_OBJECT(device);
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    struct device_state *state = ext->state;

    if (InterlockedOr(&ext->removed, FALSE))
    {
        irp->IoStatus.Status = STATUS_DELETE_PENDING;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
    }

    TRACE("device %p, irp %p, code %#x, bus_pdo %p.\n", device, irp, code, state->bus_pdo);

    IoSkipCurrentIrpStackLocation(irp);
    return IoCallDriver(state->bus_pdo, irp);
}

static NTSTATUS xinput_pdo_start_device(DEVICE_OBJECT *device)
{
    struct device_extension *ext = ext_from_DEVICE_OBJECT(device);
    struct device_state *state = ext->state;
    PHIDP_REPORT_DESCRIPTOR report_desc;
    HID_DESCRIPTOR hid_desc;
    ULONG report_desc_len;
    NTSTATUS status;

    if ((status = sync_ioctl(state->bus_pdo, IOCTL_HID_GET_DEVICE_DESCRIPTOR, NULL, 0, &hid_desc, sizeof(hid_desc))))
        return status;

    if (!(report_desc_len = hid_desc.DescriptorList[0].wReportLength)) return STATUS_UNSUCCESSFUL;
    if (!(report_desc = malloc(report_desc_len))) return STATUS_NO_MEMORY;

    status = sync_ioctl(state->bus_pdo, IOCTL_HID_GET_REPORT_DESCRIPTOR, NULL, 0, report_desc, report_desc_len);
    if (!status) status = HidP_GetCollectionDescription(report_desc, report_desc_len, PagedPool, &ext->device_desc);
    free(report_desc);
    if (status != HIDP_STATUS_SUCCESS) return status;

    status = HidP_GetCaps(ext->device_desc.CollectionDesc->PreparsedData, &state->caps);
    if (status != HIDP_STATUS_SUCCESS) WARN("HidP_GetCaps returned %#x\n", status);

    return STATUS_SUCCESS;
}

static NTSTATUS xinput_pdo_remove_device(DEVICE_OBJECT *device)
{
    struct device_extension *ext = ext_from_DEVICE_OBJECT(device);

    HidP_FreeCollectionDescription(&ext->device_desc);

    return STATUS_SUCCESS;
}

static WCHAR *query_instance_id(DEVICE_OBJECT *device)
{
    struct device_extension *ext = ext_from_DEVICE_OBJECT(device);
    struct device_state *state = ext->state;
    DWORD len = wcslen(state->instance_id);
    WCHAR *dst;

    if ((dst = ExAllocatePool(PagedPool, len * sizeof(WCHAR))))
        wcscpy(dst, state->instance_id);

    return dst;
}

static WCHAR *query_device_id(DEVICE_OBJECT *device)
{
    struct device_extension *ext = ext_from_DEVICE_OBJECT(device);
    DWORD len = wcslen(ext->device_id);
    WCHAR *dst;

    if ((dst = ExAllocatePool(PagedPool, len * sizeof(WCHAR))))
        wcscpy(dst, ext->device_id);

    return dst;
}

static WCHAR *query_compatible_ids(DEVICE_OBJECT *device)
{
    struct device_extension *ext = ext_from_DEVICE_OBJECT(device);
    struct device_state *state = ext->state;
    DWORD len = 0, pos = 0, bus_len, device_len, instance_len;
    WCHAR *dst;

    instance_len = wcslen(state->instance_id);
    device_len = wcslen(ext->device_id);
    bus_len = wcslen(ext->bus_id);

    len += device_len + instance_len + 2;
    len += device_len + 2;
    len += bus_len + 2;

    if ((dst = ExAllocatePool(PagedPool, len * sizeof(WCHAR))))
    {
        pos += swprintf(dst + pos, len - pos, L"%s\\%s", ext->device_id, state->instance_id);
        dst[pos++] = 0;
        pos += swprintf(dst + pos, len - pos, L"%s", ext->device_id);
        dst[pos++] = 0;
        pos += swprintf(dst + pos, len - pos, L"%s", ext->bus_id);
        dst[pos++] = 0;
    }

    return dst;
}

static NTSTATUS WINAPI pdo_pnp(DEVICE_OBJECT *device, IRP *irp)
{
    struct device_extension *ext = ext_from_DEVICE_OBJECT(device);
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    struct device_state *state = ext->state;
    ULONG code = stack->MinorFunction;
    NTSTATUS status;

    TRACE("device %p, irp %p, code %#x, bus_pdo %p.\n", device, irp, code, state->bus_pdo);

    switch (code)
    {
    case IRP_MN_START_DEVICE:
        status = xinput_pdo_start_device(device);
        break;

    case IRP_MN_SURPRISE_REMOVAL:
        status = STATUS_SUCCESS;
        if (InterlockedExchange(&ext->removed, TRUE)) break;
        break;

    case IRP_MN_REMOVE_DEVICE:
        xinput_pdo_remove_device(device);
        irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        IoDeleteDevice(device);
        return STATUS_SUCCESS;

    case IRP_MN_QUERY_ID:
        switch (stack->Parameters.QueryId.IdType)
        {
        case BusQueryHardwareIDs:
        case BusQueryCompatibleIDs:
            irp->IoStatus.Information = (ULONG_PTR)query_compatible_ids(device);
            break;
        case BusQueryDeviceID:
            irp->IoStatus.Information = (ULONG_PTR)query_device_id(device);
            break;
        case BusQueryInstanceID:
            irp->IoStatus.Information = (ULONG_PTR)query_instance_id(device);
            break;
        default:
            IoSkipCurrentIrpStackLocation(irp);
            return IoCallDriver(state->bus_pdo, irp);
        }

        if (!irp->IoStatus.Information) status = STATUS_NO_MEMORY;
        else status = STATUS_SUCCESS;
        break;

    default:
        IoSkipCurrentIrpStackLocation(irp);
        return IoCallDriver(state->bus_pdo, irp);
    }

    irp->IoStatus.Status = status;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

static void create_child_pdos(DEVICE_OBJECT *fdo)
{
    struct device_extension *fdo_ext = fdo->DeviceExtension, *pdo_ext;
    struct device_state *state = fdo_ext->state;
    DEVICE_OBJECT *xinput_pdo;
    UNICODE_STRING string;
    WCHAR *tmp, pdo_name[255];
    NTSTATUS status;

    swprintf(pdo_name, ARRAY_SIZE(pdo_name), L"\\Device\\XINPUT#%p&%p", fdo->DriverObject, state->bus_pdo);
    RtlInitUnicodeString(&string, pdo_name);
    if ((status = IoCreateDevice(fdo->DriverObject, sizeof(*pdo_ext), &string, 0, 0, FALSE, &xinput_pdo)))
    {
        ERR( "failed to create xinput PDO, status %#x.\n", status );
        return;
    }

    pdo_ext = xinput_pdo->DeviceExtension;
    pdo_ext->is_fdo = FALSE;
    pdo_ext->state = state;
    wcscpy(pdo_ext->bus_id, L"XINPUT");
    swprintf(pdo_ext->device_id, MAX_PATH, L"XINPUT\\%s", fdo_ext->device_id);
    if ((tmp = wcsstr(pdo_ext->device_id, L"&MI_"))) memcpy(tmp, L"&IG", 6);
    else wcscat(pdo_ext->device_id, L"&IG_00");

    state->xinput_pdo = xinput_pdo;

    TRACE("fdo %p, xinput PDO %p.\n", fdo, xinput_pdo);

    IoInvalidateDeviceRelations(state->bus_pdo, BusRelations);
}

static NTSTATUS WINAPI fdo_pnp(DEVICE_OBJECT *device, IRP *irp)
{
    struct device_extension *ext = ext_from_DEVICE_OBJECT(device);
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    struct device_state *state = ext->state;
    ULONG code = stack->MinorFunction;
    DEVICE_RELATIONS *devices;
    DEVICE_OBJECT *child;
    NTSTATUS status;

    TRACE("device %p, irp %p, code %#x, bus_pdo %p.\n", device, irp, code, state->bus_pdo);

    switch (stack->MinorFunction)
    {
    case IRP_MN_QUERY_DEVICE_RELATIONS:
        if (stack->Parameters.QueryDeviceRelations.Type == BusRelations)
        {
            if (!(devices = ExAllocatePool(PagedPool, offsetof(DEVICE_RELATIONS, Objects[2]))))
            {
                irp->IoStatus.Status = STATUS_NO_MEMORY;
                IoCompleteRequest(irp, IO_NO_INCREMENT);
                return STATUS_NO_MEMORY;
            }

            devices->Count = 0;
            if ((child = state->xinput_pdo))
            {
                devices->Objects[devices->Count] = child;
                call_fastcall_func1(ObfReferenceObject, child);
                devices->Count++;
            }

            irp->IoStatus.Information = (ULONG_PTR)devices;
            irp->IoStatus.Status = STATUS_SUCCESS;
        }

        IoSkipCurrentIrpStackLocation(irp);
        return IoCallDriver(state->bus_pdo, irp);

    case IRP_MN_START_DEVICE:
        IoSkipCurrentIrpStackLocation(irp);
        if (!(status = IoCallDriver(state->bus_pdo, irp)))
            create_child_pdos(device);
        return status;

    case IRP_MN_REMOVE_DEVICE:
        IoSkipCurrentIrpStackLocation(irp);
        status = IoCallDriver(state->bus_pdo, irp);
        IoDetachDevice(state->bus_pdo);
        IoDeleteDevice(device);
        return status;

    default:
        IoSkipCurrentIrpStackLocation(irp);
        return IoCallDriver(state->bus_pdo, irp);
    }

    return STATUS_SUCCESS;
}

static NTSTATUS WINAPI driver_pnp(DEVICE_OBJECT *device, IRP *irp)
{
    struct device_extension *ext = ext_from_DEVICE_OBJECT(device);

    if (ext->is_fdo) return fdo_pnp(device, irp);
    return pdo_pnp(device, irp);
}

static NTSTATUS get_device_id(DEVICE_OBJECT *device, BUS_QUERY_ID_TYPE type, WCHAR *id)
{
    IO_STACK_LOCATION *stack;
    IO_STATUS_BLOCK io;
    KEVENT event;
    IRP *irp;

    KeInitializeEvent(&event, NotificationEvent, FALSE);
    irp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP, device, NULL, 0, NULL, &event, &io);
    if (irp == NULL) return STATUS_NO_MEMORY;

    stack = IoGetNextIrpStackLocation(irp);
    stack->MinorFunction = IRP_MN_QUERY_ID;
    stack->Parameters.QueryId.IdType = type;

    if (IoCallDriver(device, irp) == STATUS_PENDING)
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);

    wcscpy(id, (WCHAR *)io.Information);
    ExFreePool((WCHAR *)io.Information);
    return io.Status;
}

static NTSTATUS WINAPI add_device(DRIVER_OBJECT *driver, DEVICE_OBJECT *bus_pdo)
{
    WCHAR bus_id[MAX_PATH], *device_id, instance_id[MAX_PATH];
    struct device_extension *ext;
    struct device_state *state;
    DEVICE_OBJECT *fdo;
    NTSTATUS status;

    TRACE("driver %p, bus_pdo %p.\n", driver, bus_pdo);

    if ((status = get_device_id(bus_pdo, BusQueryDeviceID, bus_id)))
    {
        ERR("failed to get PDO device id, status %#x.\n", status);
        return status;
    }

    if ((device_id = wcsrchr(bus_id, '\\'))) *device_id++ = 0;
    else
    {
        ERR("unexpected device id %s\n", debugstr_w(bus_id));
        return STATUS_UNSUCCESSFUL;
    }

    if ((status = get_device_id(bus_pdo, BusQueryInstanceID, instance_id)))
    {
        ERR("failed to get PDO instance id, status %#x.\n", status);
        return status;
    }

    if ((status = IoCreateDevice(driver, sizeof(*ext) + sizeof(struct device_state), NULL,
                                 FILE_DEVICE_BUS_EXTENDER, 0, FALSE, &fdo)))
    {
        ERR("failed to create bus FDO, status %#x.\n", status);
        return status;
    }

    ext = fdo->DeviceExtension;
    ext->is_fdo = TRUE;
    ext->state = (struct device_state *)(ext + 1);

    state = ext->state;
    state->bus_pdo = bus_pdo;
    wcscpy(ext->bus_id, bus_id);
    wcscpy(ext->device_id, device_id);
    wcscpy(state->instance_id, instance_id);

    TRACE("fdo %p, bus %s, device %s, instance %s.\n", fdo, debugstr_w(ext->bus_id), debugstr_w(ext->device_id), debugstr_w(state->instance_id));

    IoAttachDeviceToDeviceStack(fdo, bus_pdo);
    fdo->Flags &= ~DO_DEVICE_INITIALIZING;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI DriverEntry(DRIVER_OBJECT *driver, UNICODE_STRING *path)
{
    TRACE("driver %p, path %s.\n", driver, debugstr_w(path->Buffer));

    driver->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = internal_ioctl;
    driver->MajorFunction[IRP_MJ_PNP] = driver_pnp;
    driver->DriverExtension->AddDevice = add_device;

    return STATUS_SUCCESS;
}
