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
#include "ddk/hidtypes.h"

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

#include "psh_hid_macros.h"

const BYTE xinput_report_desc[] =
{
    USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
    USAGE(1, HID_USAGE_GENERIC_GAMEPAD),
    COLLECTION(1, Application),
        USAGE(1, 0),
        COLLECTION(1, Physical),
            USAGE(1, HID_USAGE_GENERIC_X),
            USAGE(1, HID_USAGE_GENERIC_Y),
            LOGICAL_MAXIMUM(2, 0xffff),
            PHYSICAL_MAXIMUM(2, 0xffff),
            REPORT_SIZE(1, 16),
            REPORT_COUNT(1, 2),
            INPUT(1, Data|Var|Abs),
        END_COLLECTION,

        COLLECTION(1, Physical),
            USAGE(1, HID_USAGE_GENERIC_RX),
            USAGE(1, HID_USAGE_GENERIC_RY),
            REPORT_COUNT(1, 2),
            INPUT(1, Data|Var|Abs),
        END_COLLECTION,

        COLLECTION(1, Physical),
            USAGE(1, HID_USAGE_GENERIC_Z),
            REPORT_COUNT(1, 1),
            INPUT(1, Data|Var|Abs),
        END_COLLECTION,

        USAGE_PAGE(1, HID_USAGE_PAGE_BUTTON),
        USAGE_MINIMUM(1, 1),
        USAGE_MAXIMUM(1, 10),
        LOGICAL_MAXIMUM(1, 1),
        PHYSICAL_MAXIMUM(1, 1),
        REPORT_COUNT(1, 10),
        REPORT_SIZE(1, 1),
        INPUT(1, Data|Var|Abs),

        USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
        USAGE(1, HID_USAGE_GENERIC_HATSWITCH),
        LOGICAL_MINIMUM(1, 1),
        LOGICAL_MAXIMUM(1, 8),
        PHYSICAL_MAXIMUM(2, 0x103b),
        REPORT_SIZE(1, 4),
        REPORT_COUNT(4, 1),
        UNIT(1, 0x0e /* none */),
        INPUT(1, Data|Var|Abs|Null),

        REPORT_COUNT(1, 18),
        REPORT_SIZE(1, 1),
        INPUT(1, Cnst|Var|Abs),
    END_COLLECTION,
};

#include "pop_hid_macros.h"

struct xinput_state
{
    WORD lx_axis;
    WORD ly_axis;
    WORD rx_axis;
    WORD ry_axis;
    WORD trigger;
    WORD buttons;
    WORD padding;
};

struct device_state
{
    DEVICE_OBJECT *bus_pdo;
    DEVICE_OBJECT *xinput_pdo;
    DEVICE_OBJECT *internal_pdo;

    WCHAR instance_id[MAX_PATH];

    HIDP_CAPS caps;
    PHIDP_PREPARSED_DATA preparsed;

    HIDP_VALUE_CAPS lx_caps;
    HIDP_VALUE_CAPS ly_caps;
    HIDP_VALUE_CAPS lt_caps;
    HIDP_VALUE_CAPS rx_caps;
    HIDP_VALUE_CAPS ry_caps;
    HIDP_VALUE_CAPS rt_caps;

    CRITICAL_SECTION cs;
    IRP *pending_read;
    ULONG report_len;
    char *report_buf;

    struct xinput_state xinput_state;
};

struct device_extension
{
    BOOL is_fdo;
    BOOL is_xinput;
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

static BOOL set_pending_read(struct device_state *state, IRP *irp)
{
    IRP *previous;

    RtlEnterCriticalSection(&state->cs);
    if (!(previous = state->pending_read))
    {
        state->pending_read = irp;
        IoMarkIrpPending(irp);
    }
    RtlLeaveCriticalSection(&state->cs);

    return previous == NULL;
}

static IRP *pop_pending_read(struct device_state *state)
{
    IRP *pending;

    RtlEnterCriticalSection(&state->cs);
    pending = state->pending_read;
    state->pending_read = NULL;
    RtlLeaveCriticalSection(&state->cs);

    return pending;
}

struct device_strings
{
    const WCHAR* id;
    const WCHAR* product;
};

static const struct device_strings device_strings[] =
{
    { .id = L"VID_045E&PID_028E&IG_00", .product = L"Controller (XBOX 360 For Windows)" },
    { .id = L"VID_045E&PID_028F&IG_00", .product = L"Controller (XBOX 360 For Windows)" },
    { .id = L"VID_045E&PID_02D1&IG_00", .product = L"Controller (XBOX One For Windows)" },
    { .id = L"VID_045E&PID_02DD&IG_00", .product = L"Controller (XBOX One For Windows)" },
    { .id = L"VID_045E&PID_02E3&IG_00", .product = L"Controller (XBOX One For Windows)" },
    { .id = L"VID_045E&PID_02EA&IG_00", .product = L"Controller (XBOX One For Windows)" },
    { .id = L"VID_045E&PID_02FD&IG_00", .product = L"Controller (XBOX One For Windows)" },
    { .id = L"VID_045E&PID_0719&IG_00", .product = L"Controller (XBOX 360 For Windows)" },
};

static NTSTATUS WINAPI xinput_ioctl(DEVICE_OBJECT *device, IRP *irp)
{
    struct device_extension *ext = ext_from_DEVICE_OBJECT(device);
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    ULONG output_len = stack->Parameters.DeviceIoControl.OutputBufferLength;
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    struct device_state *state = ext->state;
    const WCHAR *str = NULL, *match_id;
    DWORD i;

    TRACE("device %p, irp %p, code %#x, bus_pdo %p.\n", device, irp, code, state->bus_pdo);

    switch (code)
    {
    case IOCTL_HID_GET_STRING:
        switch ((ULONG_PTR)stack->Parameters.DeviceIoControl.Type3InputBuffer)
        {
        case HID_STRING_ID_IPRODUCT:
            match_id = ext->device_id + wcslen(ext->bus_id) + 1;
            for (i = 0; i < ARRAY_SIZE(device_strings); ++i)
                if (!wcsicmp(device_strings[i].id, match_id))
                    break;
            if (i < ARRAY_SIZE(device_strings)) str = device_strings[i].product;
            break;
        }

        if (!str)
        {
            IoSkipCurrentIrpStackLocation(irp);
            return IoCallDriver(state->bus_pdo, irp);
        }

        irp->IoStatus.Information = (wcslen(str) + 1) * sizeof(WCHAR);
        if (output_len < irp->IoStatus.Information)
        {
            irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
            IoCompleteRequest(irp, IO_NO_INCREMENT);
            return STATUS_BUFFER_TOO_SMALL;
        }

        wcscpy(irp->UserBuffer, str);
        irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;

    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
    {
        HID_DESCRIPTOR *descriptor = (HID_DESCRIPTOR *)irp->UserBuffer;

        irp->IoStatus.Information = sizeof(*descriptor);
        if (output_len < sizeof(*descriptor))
        {
            irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
            IoCompleteRequest(irp, IO_NO_INCREMENT);
            return STATUS_BUFFER_TOO_SMALL;
        }

        memset(descriptor, 0, sizeof(*descriptor));
        descriptor->bLength = sizeof(*descriptor);
        descriptor->bDescriptorType = HID_HID_DESCRIPTOR_TYPE;
        descriptor->bcdHID = HID_REVISION;
        descriptor->bCountry = 0;
        descriptor->bNumDescriptors = 1;
        descriptor->DescriptorList[0].bReportType = HID_REPORT_DESCRIPTOR_TYPE;
        descriptor->DescriptorList[0].wReportLength = sizeof(xinput_report_desc);

        irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    case IOCTL_HID_GET_REPORT_DESCRIPTOR:
        irp->IoStatus.Information = sizeof(xinput_report_desc);
        if (output_len < sizeof(xinput_report_desc))
        {
            irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
            IoCompleteRequest(irp, IO_NO_INCREMENT);
            return STATUS_BUFFER_TOO_SMALL;
        }

        memcpy(irp->UserBuffer, xinput_report_desc, sizeof(xinput_report_desc));
        irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;

    case IOCTL_HID_GET_INPUT_REPORT:
    case IOCTL_HID_SET_OUTPUT_REPORT:
    case IOCTL_HID_GET_FEATURE:
    case IOCTL_HID_SET_FEATURE:
        irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;

    case IOCTL_HID_READ_REPORT:
        irp->IoStatus.Information = sizeof(state->xinput_state);
        if (output_len < sizeof(state->xinput_state))
        {
            irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
            IoCompleteRequest(irp, IO_NO_INCREMENT);
            return STATUS_BUFFER_TOO_SMALL;
        }

        if (!set_pending_read(state, irp))
        {
            ERR("another read IRP was already pending!\n");
            irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
            IoCompleteRequest(irp, IO_NO_INCREMENT);
            return STATUS_UNSUCCESSFUL;
        }

        return STATUS_PENDING;

    default:
        IoSkipCurrentIrpStackLocation(irp);
        return IoCallDriver(state->bus_pdo, irp);
    }

    return STATUS_SUCCESS;
}

static LONG sign_extend(ULONG value, const HIDP_VALUE_CAPS *caps)
{
    UINT sign = 1 << (caps->BitSize - 1);
    if (sign <= 1 || caps->LogicalMin >= 0) return value;
    return value - ((value & sign) << 1);
}

static LONG scale_value(ULONG value, const HIDP_VALUE_CAPS *caps, LONG min, LONG max)
{
    LONG tmp = sign_extend(value, caps);
    if (caps->LogicalMin > caps->LogicalMax) return 0;
    if (caps->LogicalMin > tmp || caps->LogicalMax < tmp) return 0;
    return min + MulDiv(tmp - caps->LogicalMin, max - min, caps->LogicalMax - caps->LogicalMin);
}

static NTSTATUS WINAPI hid_read_report_completion(DEVICE_OBJECT *device, IRP *irp, void *context)
{
    struct device_extension *ext = ext_from_DEVICE_OBJECT(device);
    ULONG lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0, hat = 0;
    ULONG i, off, count, report_len = irp->IoStatus.Information;
    struct device_state *state = ext->state;
    char *report_buf = irp->UserBuffer;
    USAGE usages[10];
    NTSTATUS status;

    TRACE("device %p, irp %p, bus_pdo %p.\n", device, irp, state->bus_pdo);

    RtlEnterCriticalSection(&state->cs);
    if (!state->report_buf) WARN("report buffer not created yet.\n");
    else
    {
        off = (state->report_buf[0] ? 0 : 1);
        if (state->report_len - off >= report_len) memcpy(state->report_buf + off, report_buf, report_len);
        else ERR("report length mismatch %u, expected %u\n", report_len, state->report_len - off);

        count = ARRAY_SIZE(usages);
        status = HidP_GetUsages(HidP_Input, HID_USAGE_PAGE_BUTTON, 0, usages, &count, state->preparsed, state->report_buf, state->report_len);
        if (status != HIDP_STATUS_SUCCESS) WARN("HidP_GetUsages HID_USAGE_PAGE_BUTTON returned %#x\n", status);
        status = HidP_GetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_HATSWITCH, &hat, state->preparsed, state->report_buf, state->report_len);
        if (status != HIDP_STATUS_SUCCESS) WARN("HidP_GetUsageValue HID_USAGE_PAGE_GENERIC / HID_USAGE_GENERIC_HATSWITCH returned %#x\n", status);
        status = HidP_GetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_X, &lx, state->preparsed, state->report_buf, state->report_len);
        if (status != HIDP_STATUS_SUCCESS) WARN("HidP_GetUsageValue HID_USAGE_PAGE_GENERIC / HID_USAGE_GENERIC_X returned %#x\n", status);
        status = HidP_GetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Y, &ly, state->preparsed, state->report_buf, state->report_len);
        if (status != HIDP_STATUS_SUCCESS) WARN("HidP_GetUsageValue HID_USAGE_PAGE_GENERIC / HID_USAGE_GENERIC_Y returned %#x\n", status);
        status = HidP_GetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z, &lt, state->preparsed, state->report_buf, state->report_len);
        if (status != HIDP_STATUS_SUCCESS) WARN("HidP_GetUsageValue HID_USAGE_PAGE_GENERIC / HID_USAGE_GENERIC_Z returned %#x\n", status);
        status = HidP_GetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_RX, &rx, state->preparsed, state->report_buf, state->report_len);
        if (status != HIDP_STATUS_SUCCESS) WARN("HidP_GetUsageValue HID_USAGE_PAGE_GENERIC / HID_USAGE_GENERIC_RX returned %#x\n", status);
        status = HidP_GetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_RY, &ry, state->preparsed, state->report_buf, state->report_len);
        if (status != HIDP_STATUS_SUCCESS) WARN("HidP_GetUsageValue HID_USAGE_PAGE_GENERIC / HID_USAGE_GENERIC_RY returned %#x\n", status);
        status = HidP_GetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_RZ, &rt, state->preparsed, state->report_buf, state->report_len);
        if (status != HIDP_STATUS_SUCCESS) WARN("HidP_GetUsageValue HID_USAGE_PAGE_GENERIC / HID_USAGE_GENERIC_RZ returned %#x\n", status);

        if (hat < 1 || hat > 8) state->xinput_state.buttons = 0;
        else state->xinput_state.buttons = hat << 10;
        for (i = 0; i < count; i++)
        {
            if (usages[i] < 1 || usages[i] > 10) continue;
            state->xinput_state.buttons |= (1 << (usages[i] - 1));
        }
        state->xinput_state.lx_axis = scale_value(lx, &state->lx_caps, 0, 65535);
        state->xinput_state.ly_axis = scale_value(ly, &state->ly_caps, 0, 65535);
        state->xinput_state.rx_axis = scale_value(rx, &state->rx_caps, 0, 65535);
        state->xinput_state.ry_axis = scale_value(ry, &state->ry_caps, 0, 65535);
        rt = scale_value(rt, &state->rt_caps, 0, 255);
        lt = scale_value(lt, &state->lt_caps, 0, 255);
        state->xinput_state.trigger = 0x8000 + (lt - rt) * 128;
    }
    RtlLeaveCriticalSection(&state->cs);

    if (irp->PendingReturned) IoMarkIrpPending(irp);
    return STATUS_SUCCESS;
}

static NTSTATUS WINAPI internal_ioctl(DEVICE_OBJECT *device, IRP *irp)
{
    struct device_extension *ext = ext_from_DEVICE_OBJECT(device);
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    struct device_state *state = ext->state;
    IRP *pending;

    if (InterlockedOr(&ext->removed, FALSE))
    {
        irp->IoStatus.Status = STATUS_DELETE_PENDING;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
    }

    if (ext->is_xinput) return xinput_ioctl(device, irp);

    TRACE("device %p, irp %p, code %#x, bus_pdo %p.\n", device, irp, code, state->bus_pdo);

    if (code == IOCTL_HID_READ_REPORT)
    {
        /* we completed the previous internal read, send the fixed up report to xinput pdo */
        if ((pending = pop_pending_read(state)))
        {
            RtlEnterCriticalSection(&state->cs);
            memcpy(pending->UserBuffer, &state->xinput_state, sizeof(state->xinput_state));
            pending->IoStatus.Information = sizeof(state->xinput_state);
            RtlLeaveCriticalSection(&state->cs);
            pending->IoStatus.Status = irp->IoStatus.Status;
            IoCompleteRequest(pending, IO_NO_INCREMENT);
        }

        IoCopyCurrentIrpStackLocationToNext(irp);
        IoSetCompletionRoutine(irp, hid_read_report_completion, NULL, TRUE, FALSE, FALSE);
    }
    else IoSkipCurrentIrpStackLocation(irp);
    return IoCallDriver(state->bus_pdo, irp);
}

static void check_value_caps(struct device_state *state, USHORT usage, HIDP_VALUE_CAPS *caps)
{
    switch (usage)
    {
    case HID_USAGE_GENERIC_X: state->lx_caps = *caps; break;
    case HID_USAGE_GENERIC_Y: state->ly_caps = *caps; break;
    case HID_USAGE_GENERIC_Z: state->lt_caps = *caps; break;
    case HID_USAGE_GENERIC_RX: state->rx_caps = *caps; break;
    case HID_USAGE_GENERIC_RY: state->ry_caps = *caps; break;
    case HID_USAGE_GENERIC_RZ: state->rt_caps = *caps; break;
    }
}

static NTSTATUS xinput_pdo_start_device(DEVICE_OBJECT *device)
{
    struct device_extension *ext = ext_from_DEVICE_OBJECT(device);
    struct device_state *state = ext->state;
    PHIDP_REPORT_DESCRIPTOR report_desc;
    PHIDP_PREPARSED_DATA preparsed;
    HIDP_BUTTON_CAPS *button_caps;
    HIDP_VALUE_CAPS *value_caps;
    ULONG i, u, button_count;
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

    preparsed = ext->device_desc.CollectionDesc->PreparsedData;
    status = HidP_GetCaps(preparsed, &state->caps);
    if (status != HIDP_STATUS_SUCCESS) WARN("HidP_GetCaps returned %#x\n", status);

    button_count = 0;
    if (!(button_caps = malloc(sizeof(*button_caps) * state->caps.NumberInputButtonCaps))) return STATUS_NO_MEMORY;
    status = HidP_GetButtonCaps(HidP_Input, button_caps, &state->caps.NumberInputButtonCaps, preparsed);
    if (status != HIDP_STATUS_SUCCESS) WARN("HidP_GetButtonCaps returned %#x\n", status);
    else for (i = 0; i < state->caps.NumberInputButtonCaps; i++)
    {
        if (button_caps[i].UsagePage != HID_USAGE_PAGE_BUTTON) continue;
        if (button_caps[i].IsRange) button_count = max(button_count, button_caps[i].Range.UsageMax);
        else button_count = max(button_count, button_caps[i].NotRange.Usage);
    }
    free(button_caps);
    if (button_count < 10) WARN("only %u buttons found\n", button_count);

    if (!(value_caps = malloc(sizeof(*value_caps) * state->caps.NumberInputValueCaps))) return STATUS_NO_MEMORY;
    status = HidP_GetValueCaps(HidP_Input, value_caps, &state->caps.NumberInputValueCaps, preparsed);
    if (status != HIDP_STATUS_SUCCESS) WARN("HidP_GetValueCaps returned %#x\n", status);
    else for (i = 0; i < state->caps.NumberInputValueCaps; i++)
    {
        HIDP_VALUE_CAPS *caps = value_caps + i;
        if (caps->UsagePage != HID_USAGE_PAGE_GENERIC) continue;
        if (!caps->IsRange) check_value_caps(state, caps->NotRange.Usage, caps);
        else for (u = caps->Range.UsageMin; u <=caps->Range.UsageMax; u++) check_value_caps(state, u, value_caps + i);
    }
    free(value_caps);

    if (!state->lt_caps.UsagePage) WARN("missing lt axis\n");
    if (!state->rt_caps.UsagePage) WARN("missing rt axis\n");
    if (!state->lx_caps.UsagePage) WARN("missing lx axis\n");
    if (!state->ly_caps.UsagePage) WARN("missing ly axis\n");
    if (!state->rx_caps.UsagePage) WARN("missing rx axis\n");
    if (!state->ry_caps.UsagePage) WARN("missing ry axis\n");

    status = STATUS_SUCCESS;
    RtlEnterCriticalSection(&state->cs);
    state->report_len = state->caps.InputReportByteLength;
    if (!(state->report_buf = malloc(state->report_len))) status = STATUS_NO_MEMORY;
    else state->report_buf[0] = ext->device_desc.ReportIDs[0].ReportID;
    state->preparsed = preparsed;
    RtlLeaveCriticalSection(&state->cs);
    if (status) return status;

    return STATUS_SUCCESS;
}

static NTSTATUS xinput_pdo_remove_device(DEVICE_OBJECT *device)
{
    struct device_extension *ext = ext_from_DEVICE_OBJECT(device);
    struct device_state *state = ext->state;

    RtlEnterCriticalSection(&state->cs);
    free(state->report_buf);
    state->report_buf = NULL;
    state->preparsed = NULL;
    RtlLeaveCriticalSection(&state->cs);

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
    IRP *pending;

    TRACE("device %p, irp %p, code %#x, bus_pdo %p.\n", device, irp, code, state->bus_pdo);

    switch (code)
    {
    case IRP_MN_START_DEVICE:
        if (ext->is_xinput) status = xinput_pdo_start_device(device);
        else status = STATUS_SUCCESS;
        break;

    case IRP_MN_SURPRISE_REMOVAL:
        status = STATUS_SUCCESS;
        if (InterlockedExchange(&ext->removed, TRUE)) break;

        if (!ext->is_xinput) break;

        if ((pending = pop_pending_read(state)))
        {
            pending->IoStatus.Status = STATUS_DELETE_PENDING;
            pending->IoStatus.Information = 0;
            IoCompleteRequest(pending, IO_NO_INCREMENT);
        }
        break;

    case IRP_MN_REMOVE_DEVICE:
        if (ext->is_xinput) xinput_pdo_remove_device(device);
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
    DEVICE_OBJECT *internal_pdo, *xinput_pdo;
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

    swprintf(pdo_name, ARRAY_SIZE(pdo_name), L"\\Device\\WINEXINPUT#%p&%p", fdo->DriverObject, state->bus_pdo);
    RtlInitUnicodeString(&string, pdo_name);
    if ((status = IoCreateDevice(fdo->DriverObject, sizeof(*pdo_ext), &string, 0, 0, FALSE, &internal_pdo)))
    {
        ERR( "failed to create internal PDO, status %#x.\n", status );
        IoDeleteDevice(xinput_pdo);
        return;
    }

    pdo_ext = xinput_pdo->DeviceExtension;
    pdo_ext->is_fdo = FALSE;
    pdo_ext->is_xinput = TRUE;
    pdo_ext->state = state;
    wcscpy(pdo_ext->bus_id, L"XINPUT");
    swprintf(pdo_ext->device_id, MAX_PATH, L"XINPUT\\%s", fdo_ext->device_id);
    if ((tmp = wcsstr(pdo_ext->device_id, L"&MI_"))) memcpy(tmp, L"&IG", 6);
    else wcscat(pdo_ext->device_id, L"&IG_00");

    pdo_ext = internal_pdo->DeviceExtension;
    pdo_ext->is_fdo = FALSE;
    pdo_ext->is_xinput = FALSE;
    pdo_ext->state = state;
    wcscpy(pdo_ext->bus_id, L"WINEXINPUT");
    swprintf(pdo_ext->device_id, MAX_PATH, L"WINEXINPUT\\%s", fdo_ext->device_id);
    if ((tmp = wcsstr(pdo_ext->device_id, L"&MI_"))) memcpy(tmp, L"&XI", 6);
    else wcscat(pdo_ext->device_id, L"&XI_00");

    state->xinput_pdo = xinput_pdo;
    state->internal_pdo = internal_pdo;

    TRACE("fdo %p, internal PDO %p, xinput PDO %p.\n", fdo, internal_pdo, xinput_pdo);

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
            if ((child = state->internal_pdo))
            {
                devices->Objects[devices->Count] = child;
                call_fastcall_func1(ObfReferenceObject, child);
                devices->Count++;
            }
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
    RtlInitializeCriticalSection(&state->cs);

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
