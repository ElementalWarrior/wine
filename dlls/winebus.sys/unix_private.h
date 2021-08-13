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

#ifndef __WINEBUS_UNIX_PRIVATE_H
#define __WINEBUS_UNIX_PRIVATE_H

#include <stdarg.h>

#include <windef.h>
#include <winbase.h>
#include <winternl.h>

#include "unixlib.h"

#include "wine/list.h"

struct unix_device_vtbl
{
    void (*destroy)(struct unix_device *iface);
    NTSTATUS (*start)(struct unix_device *iface);
    void (*stop)(struct unix_device *iface);
    NTSTATUS (*get_report_descriptor)(struct unix_device *iface, BYTE *buffer, DWORD length, DWORD *out_length);
    void (*set_output_report)(struct unix_device *iface, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io);
    void (*get_feature_report)(struct unix_device *iface, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io);
    void (*set_feature_report)(struct unix_device *iface, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io);
};

struct unix_device
{
    const struct unix_device_vtbl *vtbl;
    struct list entry;
    LONG ref;
};

extern void *unix_device_create(const struct unix_device_vtbl *vtbl, SIZE_T size) DECLSPEC_HIDDEN;
extern void unix_device_destroy(struct unix_device *iface) DECLSPEC_HIDDEN;

extern NTSTATUS WINAPI sdl_bus_init(void *args) DECLSPEC_HIDDEN;
extern NTSTATUS WINAPI sdl_bus_wait(void *args) DECLSPEC_HIDDEN;
extern NTSTATUS WINAPI sdl_bus_stop(void) DECLSPEC_HIDDEN;

extern NTSTATUS WINAPI udev_bus_init(void *args) DECLSPEC_HIDDEN;
extern NTSTATUS WINAPI udev_bus_wait(void *args) DECLSPEC_HIDDEN;
extern NTSTATUS WINAPI udev_bus_stop(void) DECLSPEC_HIDDEN;

extern NTSTATUS WINAPI iohid_bus_init(void *args) DECLSPEC_HIDDEN;
extern NTSTATUS WINAPI iohid_bus_wait(void *args) DECLSPEC_HIDDEN;
extern NTSTATUS WINAPI iohid_bus_stop(void) DECLSPEC_HIDDEN;

extern void bus_event_destroy(struct bus_event *event) DECLSPEC_HIDDEN;
extern void bus_event_queue_destroy(struct list *queue) DECLSPEC_HIDDEN;
extern BOOL bus_event_queue_device_removed(struct list *queue, struct unix_device *device) DECLSPEC_HIDDEN;
extern BOOL bus_event_queue_device_created(struct list *queue, struct unix_device *device, struct device_desc *desc) DECLSPEC_HIDDEN;
extern BOOL bus_event_queue_input_report(struct list *queue, struct unix_device *device,
                                         BYTE *report, DWORD length) DECLSPEC_HIDDEN;
extern BOOL bus_event_queue_pop(struct list *queue, struct bus_event **event) DECLSPEC_HIDDEN;

struct hid_descriptor
{
    BYTE *data;
    SIZE_T size;
    SIZE_T max_size;

    BYTE haptics_feature_report;
    BYTE haptics_waveform_report;

    BYTE next_report_id[3];
};

enum haptics_waveform_index
{
    HAPTICS_WAVEFORM_STOP_INDEX = 1,
    HAPTICS_WAVEFORM_NULL_INDEX = 2,
    HAPTICS_WAVEFORM_RUMBLE_INDEX = 3,
    HAPTICS_WAVEFORM_BUZZ_INDEX = 4,
    HAPTICS_WAVEFORM_LAST_INDEX = HAPTICS_WAVEFORM_BUZZ_INDEX,
};

struct haptics_features
{
    WORD  waveform_list[HAPTICS_WAVEFORM_LAST_INDEX - HAPTICS_WAVEFORM_NULL_INDEX];
    WORD  duration_list[HAPTICS_WAVEFORM_LAST_INDEX - HAPTICS_WAVEFORM_NULL_INDEX];
    DWORD waveform_cutoff_time;
};

struct haptics_waveform
{
    WORD manual_trigger;
    WORD intensity;
};

struct haptics
{
    struct haptics_features features;
    struct haptics_waveform waveforms[HAPTICS_WAVEFORM_LAST_INDEX + 1];
};

extern BOOL hid_descriptor_append(struct hid_descriptor *desc, const BYTE *buffer, SIZE_T size) DECLSPEC_HIDDEN;
extern BOOL hid_descriptor_begin(struct hid_descriptor *desc, USAGE usage_page, USAGE usage) DECLSPEC_HIDDEN;
extern BOOL hid_descriptor_end(struct hid_descriptor *desc) DECLSPEC_HIDDEN;
extern void hid_descriptor_free(struct hid_descriptor *desc) DECLSPEC_HIDDEN;

extern BOOL hid_descriptor_begin_report(struct hid_descriptor *desc, BYTE type, BYTE *id) DECLSPEC_HIDDEN;
extern BOOL hid_descriptor_end_report(struct hid_descriptor *desc) DECLSPEC_HIDDEN;

extern BOOL hid_descriptor_add_buttons(struct hid_descriptor *desc, USAGE usage_page,
                                       USAGE usage_min, USAGE usage_max) DECLSPEC_HIDDEN;
extern BOOL hid_descriptor_add_padding(struct hid_descriptor *desc, BYTE bitcount) DECLSPEC_HIDDEN;
extern BOOL hid_descriptor_add_hatswitch(struct hid_descriptor *desc, INT count) DECLSPEC_HIDDEN;
extern BOOL hid_descriptor_add_axes(struct hid_descriptor *desc, BYTE count, USAGE usage_page,
                                    const USAGE *usages, BOOL rel, INT size, LONG min, LONG max) DECLSPEC_HIDDEN;

extern BOOL hid_descriptor_add_haptics(struct hid_descriptor *desc, BYTE *id, struct haptics *haptics) DECLSPEC_HIDDEN;

extern void handle_haptics_set_output_report(struct hid_descriptor *desc, struct haptics *haptics,
                                             HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io) DECLSPEC_HIDDEN;
extern void handle_haptics_set_feature_report(struct hid_descriptor *desc, struct haptics *haptics,
                                             HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io) DECLSPEC_HIDDEN;
extern void handle_haptics_get_feature_report(struct hid_descriptor *desc, struct haptics *haptics,
                                             HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io) DECLSPEC_HIDDEN;

#endif /* __WINEBUS_UNIX_PRIVATE_H */
