/*  Bus like function for mac HID devices
 *
 * Copyright 2016 CodeWeavers, Aric Stewart
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
#include "wine/port.h"

#include <stdarg.h>

#if defined(HAVE_IOKIT_HID_IOHIDLIB_H)
#define DWORD UInt32
#define LPDWORD UInt32*
#define LONG SInt32
#define LPLONG SInt32*
#define E_PENDING __carbon_E_PENDING
#define ULONG __carbon_ULONG
#define E_INVALIDARG __carbon_E_INVALIDARG
#define E_OUTOFMEMORY __carbon_E_OUTOFMEMORY
#define E_HANDLE __carbon_E_HANDLE
#define E_ACCESSDENIED __carbon_E_ACCESSDENIED
#define E_UNEXPECTED __carbon_E_UNEXPECTED
#define E_FAIL __carbon_E_FAIL
#define E_ABORT __carbon_E_ABORT
#define E_POINTER __carbon_E_POINTER
#define E_NOINTERFACE __carbon_E_NOINTERFACE
#define E_NOTIMPL __carbon_E_NOTIMPL
#define S_FALSE __carbon_S_FALSE
#define S_OK __carbon_S_OK
#define HRESULT_FACILITY __carbon_HRESULT_FACILITY
#define IS_ERROR __carbon_IS_ERROR
#define FAILED __carbon_FAILED
#define SUCCEEDED __carbon_SUCCEEDED
#define MAKE_HRESULT __carbon_MAKE_HRESULT
#define HRESULT __carbon_HRESULT
#define STDMETHODCALLTYPE __carbon_STDMETHODCALLTYPE
#define PAGE_SHIFT __carbon_PAGE_SHIFT
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDLib.h>
#undef ULONG
#undef E_INVALIDARG
#undef E_OUTOFMEMORY
#undef E_HANDLE
#undef E_ACCESSDENIED
#undef E_UNEXPECTED
#undef E_FAIL
#undef E_ABORT
#undef E_POINTER
#undef E_NOINTERFACE
#undef E_NOTIMPL
#undef S_FALSE
#undef S_OK
#undef HRESULT_FACILITY
#undef IS_ERROR
#undef FAILED
#undef SUCCEEDED
#undef MAKE_HRESULT
#undef HRESULT
#undef STDMETHODCALLTYPE
#undef DWORD
#undef LPDWORD
#undef LONG
#undef LPLONG
#undef E_PENDING
#undef PAGE_SHIFT
#endif /* HAVE_IOKIT_HID_IOHIDLIB_H */

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "winioctl.h"
#include "ddk/wdm.h"
#include "ddk/hidtypes.h"
#include "wine/debug.h"

#include "bus.h"
#include "unix_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(plugplay);

#ifdef HAVE_IOHIDMANAGERCREATE

static struct iohid_bus_options options;

static CRITICAL_SECTION iohid_cs;
static CRITICAL_SECTION_DEBUG iohid_cs_debug =
{
    0, 0, &iohid_cs,
    { &iohid_cs_debug.ProcessLocksList, &iohid_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": iohid_cs") }
};
static CRITICAL_SECTION iohid_cs = { &iohid_cs_debug, -1, 0, 0, 0, 0 };

static IOHIDManagerRef hid_manager;
static CFRunLoopRef run_loop;
static struct list event_queue = LIST_INIT(event_queue);
static struct list device_list = LIST_INIT(device_list);

static const WCHAR busidW[] = {'I','O','H','I','D',0};

struct platform_private
{
    struct unix_device unix_device;
    IOHIDDeviceRef device;
    uint8_t *buffer;
};

static inline struct platform_private *impl_from_unix_device(struct unix_device *iface)
{
    return CONTAINING_RECORD(iface, struct platform_private, unix_device);
}

static void CFStringToWSTR(CFStringRef cstr, LPWSTR wstr, int length)
{
    int len = min(CFStringGetLength(cstr), length-1);
    CFStringGetCharacters(cstr, CFRangeMake(0, len), (UniChar*)wstr);
    wstr[len] = 0;
}

static DWORD CFNumberToDWORD(CFNumberRef num)
{
    int dwNum = 0;
    if (num)
        CFNumberGetValue(num, kCFNumberIntType, &dwNum);
    return dwNum;
}

static void handle_IOHIDDeviceIOHIDReportCallback(void *context,
        IOReturn result, void *sender, IOHIDReportType type,
        uint32_t reportID, uint8_t *report, CFIndex report_length)
{
    struct unix_device *iface = (struct unix_device *)context;
    bus_event_queue_input_report(&event_queue, iface, report, report_length);
}

static void iohid_device_destroy(struct unix_device *iface)
{
    unix_device_destroy(iface);
}

static int iohid_device_compare(struct unix_device *iface, void *context)
{
    struct platform_private *private = impl_from_unix_device(iface);
    IOHIDDeviceRef dev2 = (IOHIDDeviceRef)context;
    if (private->device != dev2)
        return 1;
    else
        return 0;
}

static NTSTATUS iohid_device_start(struct unix_device *iface)
{
    DWORD length;
    struct platform_private *private = impl_from_unix_device(iface);
    CFNumberRef num;

    num = IOHIDDeviceGetProperty(private->device, CFSTR(kIOHIDMaxInputReportSizeKey));
    length = CFNumberToDWORD(num);
    private->buffer = HeapAlloc(GetProcessHeap(), 0, length);

    IOHIDDeviceRegisterInputReportCallback(private->device, private->buffer, length, handle_IOHIDDeviceIOHIDReportCallback, iface);
    return STATUS_SUCCESS;
}

static void iohid_device_stop(struct unix_device *iface)
{
    struct platform_private *private = impl_from_unix_device(iface);

    IOHIDDeviceRegisterInputReportCallback(private->device, NULL, 0, NULL, NULL);

    EnterCriticalSection(&iohid_cs);
    list_remove(&private->unix_device.entry);
    LeaveCriticalSection(&iohid_cs);
}

static NTSTATUS iohid_device_get_report_descriptor(struct unix_device *iface, BYTE *buffer, DWORD length, DWORD *out_length)
{
    struct platform_private *private = impl_from_unix_device(iface);
    CFDataRef data = IOHIDDeviceGetProperty(private->device, CFSTR(kIOHIDReportDescriptorKey));
    int data_length = CFDataGetLength(data);
    const UInt8 *ptr;

    *out_length = data_length;
    if (length < data_length)
        return STATUS_BUFFER_TOO_SMALL;

    ptr = CFDataGetBytePtr(data);
    memcpy(buffer, ptr, data_length);
    return STATUS_SUCCESS;
}

static void iohid_device_set_output_report(struct unix_device *iface, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    IOReturn result;
    struct platform_private *private = impl_from_unix_device(iface);
    result = IOHIDDeviceSetReport(private->device, kIOHIDReportTypeOutput, packet->reportId, packet->reportBuffer, packet->reportBufferLen);
    if (result == kIOReturnSuccess)
    {
        io->Information = packet->reportBufferLen;
        io->Status = STATUS_SUCCESS;
    }
    else
    {
        io->Information = 0;
        io->Status = STATUS_UNSUCCESSFUL;
    }
}

static void iohid_device_get_feature_report(struct unix_device *iface, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    IOReturn ret;
    CFIndex report_length = packet->reportBufferLen;
    struct platform_private *private = impl_from_unix_device(iface);

    ret = IOHIDDeviceGetReport(private->device, kIOHIDReportTypeFeature, packet->reportId, packet->reportBuffer, &report_length);
    if (ret == kIOReturnSuccess)
    {
        io->Information = report_length;
        io->Status = STATUS_SUCCESS;
    }
    else
    {
        io->Information = 0;
        io->Status = STATUS_UNSUCCESSFUL;
    }
}

static void iohid_device_set_feature_report(struct unix_device *iface, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    IOReturn result;
    struct platform_private *private = impl_from_unix_device(iface);

    result = IOHIDDeviceSetReport(private->device, kIOHIDReportTypeFeature, packet->reportId, packet->reportBuffer, packet->reportBufferLen);
    if (result == kIOReturnSuccess)
    {
        io->Information = packet->reportBufferLen;
        io->Status = STATUS_SUCCESS;
    }
    else
    {
        io->Information = 0;
        io->Status = STATUS_UNSUCCESSFUL;
    }
}

static const struct unix_device_vtbl iohid_device_vtbl =
{
    iohid_device_destroy,
    iohid_device_compare,
    iohid_device_start,
    iohid_device_stop,
    iohid_device_get_report_descriptor,
    iohid_device_set_output_report,
    iohid_device_get_feature_report,
    iohid_device_set_feature_report,
};

static void handle_DeviceMatchingCallback(void *context, IOReturn result, void *sender, IOHIDDeviceRef IOHIDDevice)
{
    struct device_desc desc =
    {
        .bus_id = busidW,
        .vendor_id = 0,
        .product_id = 0,
        .version = 0,
        .input = -1,
        .uid = 0,
        .is_gamepad = FALSE,
        .manufacturer = {0},
        .product = {0},
        .serialnumber = {"0000"},
    };
    struct platform_private *private;
    CFStringRef str = NULL;

    TRACE("OS/X IOHID Device Added %p\n", IOHIDDevice);

    desc.vendor_id = CFNumberToDWORD(IOHIDDeviceGetProperty(IOHIDDevice, CFSTR(kIOHIDVendorIDKey)));
    desc.product_id = CFNumberToDWORD(IOHIDDeviceGetProperty(IOHIDDevice, CFSTR(kIOHIDProductIDKey)));
    desc.version = CFNumberToDWORD(IOHIDDeviceGetProperty(IOHIDDevice, CFSTR(kIOHIDVersionNumberKey)));
    desc.uid = CFNumberToDWORD(IOHIDDeviceGetProperty(IOHIDDevice, CFSTR(kIOHIDLocationIDKey)));

    if (IOHIDDeviceOpen(IOHIDDevice, 0) != kIOReturnSuccess)
    {
        ERR("Failed to open HID device %p (vid %04x, pid %04x)\n", IOHIDDevice, desc.vendor_id, desc.product_id);
        return;
    }
    IOHIDDeviceScheduleWithRunLoop(IOHIDDevice, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

    str = IOHIDDeviceGetProperty(IOHIDDevice, CFSTR(kIOHIDManufacturerKey));
    if (str) lstrcpynA(desc.manufacturer, str, sizeof(desc.manufacturer));
    str = IOHIDDeviceGetProperty(IOHIDDevice, CFSTR(kIOHIDProductKey));
    if (str) lstrcpynA(desc.product, str, sizeof(desc.product));
    str = IOHIDDeviceGetProperty(IOHIDDevice, CFSTR(kIOHIDSerialNumberKey));
    if (str) lstrcpynA(desc.serialnumber, str, sizeof(desc.serialnumber));

    if (IOHIDDeviceConformsTo(IOHIDDevice, kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad) ||
       IOHIDDeviceConformsTo(IOHIDDevice, kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick))
    {
        if (is_xbox_gamepad(desc.vendor_id, desc.product_id))
            desc.is_gamepad = TRUE;
        else
        {
            int axes=0, buttons=0;
            CFArrayRef element_array = IOHIDDeviceCopyMatchingElements(
                IOHIDDevice, NULL, kIOHIDOptionsTypeNone);

            if (element_array) {
                CFIndex index;
                CFIndex count = CFArrayGetCount(element_array);
                for (index = 0; index < count; index++)
                {
                    IOHIDElementRef element = (IOHIDElementRef)CFArrayGetValueAtIndex(element_array, index);
                    if (element)
                    {
                        int type = IOHIDElementGetType(element);
                        if (type == kIOHIDElementTypeInput_Button) buttons++;
                        if (type == kIOHIDElementTypeInput_Axis) axes++;
                        if (type == kIOHIDElementTypeInput_Misc)
                        {
                            uint32_t usage = IOHIDElementGetUsage(element);
                            switch (usage)
                            {
                                case kHIDUsage_GD_X:
                                case kHIDUsage_GD_Y:
                                case kHIDUsage_GD_Z:
                                case kHIDUsage_GD_Rx:
                                case kHIDUsage_GD_Ry:
                                case kHIDUsage_GD_Rz:
                                case kHIDUsage_GD_Slider:
                                    axes ++;
                            }
                        }
                    }
                }
                CFRelease(element_array);
            }
            desc.is_gamepad = (axes == 6  && buttons >= 14);
        }
    }
    if (desc.is_gamepad)
        desc.input = 0;

    if (!(private = unix_device_create(&iohid_device_vtbl, sizeof(*private)))) return;
    list_add_tail(&device_list, &private->unix_device.entry);
    private->device = IOHIDDevice;
    private->buffer = NULL;

    bus_event_queue_device_created(&event_queue, &private->unix_device, &desc);
}

static void handle_RemovalCallback(void *context, IOReturn result, void *sender, IOHIDDeviceRef IOHIDDevice)
{
    TRACE("OS/X IOHID Device Removed %p\n", IOHIDDevice);
    IOHIDDeviceRegisterInputReportCallback(IOHIDDevice, NULL, 0, NULL, NULL);
    /* Note: Yes, we leak the buffer. But according to research there is no
             safe way to deallocate that buffer. */
    IOHIDDeviceUnscheduleFromRunLoop(IOHIDDevice, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDDeviceClose(IOHIDDevice, 0);
    bus_event_queue_device_removed(&event_queue, busidW, IOHIDDevice);
}

NTSTATUS WINAPI iohid_bus_init(void *args)
{
    TRACE("args %p\n", args);

    options = *(struct iohid_bus_options *)args;

    hid_manager = IOHIDManagerCreate(kCFAllocatorDefault, 0L);
    run_loop = CFRunLoopGetCurrent();

    IOHIDManagerSetDeviceMatching(hid_manager, NULL);
    IOHIDManagerRegisterDeviceMatchingCallback(hid_manager, handle_DeviceMatchingCallback, NULL);
    IOHIDManagerRegisterDeviceRemovalCallback(hid_manager, handle_RemovalCallback, NULL);
    IOHIDManagerScheduleWithRunLoop(hid_manager, run_loop, kCFRunLoopDefaultMode);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI iohid_bus_wait(void *args)
{
    struct bus_event **result = args;
    CFRunLoopRunResult result;

    /* destroy previously returned event */
    if (*result) bus_event_destroy(*result);
    *result = NULL;

    do
    {
        if (bus_event_queue_pop(&event_queue, result)) return STATUS_PENDING;
        EnterCriticalSection(&iohid_cs);
        result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 10, TRUE);
        LeaveCriticalSection(&iohid_cs);
    } while (result != kCFRunLoopRunStopped);

    TRACE("IOHID main loop exiting\n");
    bus_event_queue_destroy(&event_queue);
    IOHIDManagerRegisterDeviceMatchingCallback(hid_manager, NULL, NULL);
    IOHIDManagerRegisterDeviceRemovalCallback(hid_manager, NULL, NULL);
    CFRelease(hid_manager);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI iohid_bus_stop(void)
{
    if (!run_loop) return STATUS_SUCCESS;

    IOHIDManagerUnscheduleFromRunLoop(hid_manager, run_loop, kCFRunLoopDefaultMode);
    CFRunLoopStop(run_loop);
    return STATUS_SUCCESS;
}

#else

NTSTATUS WINAPI iohid_bus_init(void *args)
{
    WARN("IOHID support not compiled in!\n");
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI iohid_bus_wait(void *args)
{
    WARN("IOHID support not compiled in!\n");
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI iohid_bus_stop(void)
{
    WARN("IOHID support not compiled in!\n");
    return STATUS_NOT_IMPLEMENTED;
}

#endif /* HAVE_IOHIDMANAGERCREATE */
