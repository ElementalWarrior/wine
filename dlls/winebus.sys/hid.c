/*
 * Common HID report descriptor helpers
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

#if 0
#pragma makedep unix
#endif

#include <stdarg.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"
#include "winioctl.h"
#include "hidusage.h"
#include "ddk/wdm.h"

#include "unix_private.h"

#include "ddk/hidsdi.h"

BOOL hid_descriptor_append(struct hid_descriptor *desc, const BYTE *buffer, SIZE_T size)
{
    BYTE *tmp = desc->data;

    if (desc->size + size > desc->max_size)
    {
        desc->max_size = max(desc->max_size * 3 / 2, desc->size + size);
        if (!desc->data) desc->data = RtlAllocateHeap(GetProcessHeap(), 0, desc->max_size);
        else desc->data = RtlReAllocateHeap(GetProcessHeap(), 0, tmp, desc->max_size);
    }

    if (!desc->data)
    {
        RtlFreeHeap(GetProcessHeap(), 0, tmp);
        return FALSE;
    }

    memcpy(desc->data + desc->size, buffer, size);
    desc->size += size;
    return TRUE;
}

#include "psh_hid_macros.h"

static BOOL hid_descriptor_append_usage(struct hid_descriptor *desc, USAGE usage)
{
    const BYTE template[] =
    {
        USAGE(2, usage),
    };

    return hid_descriptor_append(desc, template, sizeof(template));
}

BOOL hid_descriptor_begin(struct hid_descriptor *desc, USAGE usage_page, USAGE usage)
{
    const BYTE template[] =
    {
        USAGE_PAGE(2, usage_page),
        USAGE(2, usage),
        COLLECTION(1, Application),
            USAGE(1, 0),
    };

    memset(desc, 0, sizeof(*desc));
    return hid_descriptor_append(desc, template, sizeof(template));
}

BOOL hid_descriptor_end(struct hid_descriptor *desc)
{
    static const BYTE template[] =
    {
        END_COLLECTION,
    };

    return hid_descriptor_append(desc, template, sizeof(template));
}

void hid_descriptor_free(struct hid_descriptor *desc)
{
    RtlFreeHeap(GetProcessHeap(), 0, desc->data);
}

BOOL hid_descriptor_begin_report(struct hid_descriptor *desc, BYTE type, BYTE *id)
{
    const BYTE report_id = ++desc->next_report_id[type];
    const BYTE template[] =
    {
        COLLECTION(1, Report),
            REPORT_ID(1, report_id),
    };

    *id = report_id;
    return hid_descriptor_append(desc, template, sizeof(template));
}

BOOL hid_descriptor_end_report(struct hid_descriptor *desc)
{
    static const BYTE template[] =
    {
        END_COLLECTION,
    };

    return hid_descriptor_append(desc, template, sizeof(template));
}

BOOL hid_descriptor_add_buttons(struct hid_descriptor *desc, USAGE usage_page,
                                USAGE usage_min, USAGE usage_max)
{
    const BYTE template[] =
    {
        USAGE_PAGE(2, usage_page),
        USAGE_MINIMUM(2, usage_min),
        USAGE_MAXIMUM(2, usage_max),
        LOGICAL_MINIMUM(1, 0),
        LOGICAL_MAXIMUM(1, 1),
        PHYSICAL_MINIMUM(1, 0),
        PHYSICAL_MAXIMUM(1, 1),
        REPORT_COUNT(2, usage_max - usage_min + 1),
        REPORT_SIZE(1, 1),
        INPUT(1, Data|Var|Abs),
    };

    return hid_descriptor_append(desc, template, sizeof(template));
}

BOOL hid_descriptor_add_padding(struct hid_descriptor *desc, BYTE bitcount)
{
    const BYTE template[] =
    {
        REPORT_COUNT(1, bitcount),
        REPORT_SIZE(1, 1),
        INPUT(1, Cnst|Var|Abs),
    };

    return hid_descriptor_append(desc, template, sizeof(template));
}

BOOL hid_descriptor_add_hatswitch(struct hid_descriptor *desc, INT count)
{
    const BYTE template[] =
    {
        USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
        USAGE(1, HID_USAGE_GENERIC_HATSWITCH),
        LOGICAL_MINIMUM(1, 1),
        LOGICAL_MAXIMUM(1, 8),
        PHYSICAL_MINIMUM(1, 0),
        PHYSICAL_MAXIMUM(2, 8),
        REPORT_SIZE(1, 4),
        REPORT_COUNT(4, count),
        UNIT(1, 0x0e /* none */),
        INPUT(1, Data|Var|Abs|Null),
    };

    return hid_descriptor_append(desc, template, sizeof(template));
}

BOOL hid_descriptor_add_axes(struct hid_descriptor *desc, BYTE count, USAGE usage_page,
                             const USAGE *usages, BOOL rel, INT size, LONG min, LONG max)
{
    const BYTE template_begin[] =
    {
        USAGE_PAGE(1, usage_page),
        COLLECTION(1, Physical),
    };
    const BYTE template_end[] =
    {
        END_COLLECTION,
    };
    const BYTE template_1[] =
    {
        LOGICAL_MINIMUM(1, min),
        LOGICAL_MAXIMUM(1, max),
        PHYSICAL_MINIMUM(1, min),
        PHYSICAL_MAXIMUM(1, max),
        REPORT_SIZE(1, size),
        REPORT_COUNT(1, count),
        INPUT(1, Data|Var|(rel ? Rel : Abs)),
    };
    const BYTE template_2[] =
    {
        LOGICAL_MINIMUM(2, min),
        LOGICAL_MAXIMUM(2, max),
        PHYSICAL_MINIMUM(2, min),
        PHYSICAL_MAXIMUM(2, max),
        REPORT_SIZE(1, size),
        REPORT_COUNT(1, count),
        INPUT(1, Data|Var|(rel ? Rel : Abs)),
    };
    const BYTE template_4[] =
    {
        LOGICAL_MINIMUM(4, min),
        LOGICAL_MAXIMUM(4, max),
        PHYSICAL_MINIMUM(4, min),
        PHYSICAL_MAXIMUM(4, max),
        REPORT_SIZE(1, size),
        REPORT_COUNT(1, count),
        INPUT(1, Data|Var|(rel ? Rel : Abs)),
    };
    int i;

    if (!hid_descriptor_append(desc, template_begin, sizeof(template_begin)))
        return FALSE;

    for (i = 0; i < count; i++)
    {
        if (!hid_descriptor_append_usage(desc, usages[i]))
            return FALSE;
    }

    if (size >= 16)
    {
        if (!hid_descriptor_append(desc, template_4, sizeof(template_4)))
            return FALSE;
    }
    else if (size >= 8)
    {
        if (!hid_descriptor_append(desc, template_2, sizeof(template_2)))
            return FALSE;
    }
    else
    {
        if (!hid_descriptor_append(desc, template_1, sizeof(template_1)))
            return FALSE;
    }

    if (!hid_descriptor_append(desc, template_end, sizeof(template_end)))
        return FALSE;

    return TRUE;
}

BOOL hid_descriptor_add_haptics(struct hid_descriptor *desc, BYTE *id, struct haptics *haptics)
{
    const BYTE report_id = ++desc->next_report_id[HidP_Output];
    const BYTE haptics_feature_report = desc->haptics_feature_report = ++desc->next_report_id[HidP_Feature];
    const BYTE haptics_waveform_report = desc->haptics_waveform_report = ++desc->next_report_id[HidP_Output];
    const BYTE template[] =
    {
        USAGE_PAGE(2, HID_USAGE_PAGE_VENDOR_DEFINED_BEGIN),
        COLLECTION(1, Report),
            REPORT_ID(1, report_id),
            /* padding */
            REPORT_COUNT(1, 0x02),
            REPORT_SIZE(1, 0x08),
            OUTPUT(1, Data|Var|Abs),
            /* actuators */
            USAGE(1, 0x01),
            LOGICAL_MINIMUM(1, 0x00),
            LOGICAL_MAXIMUM(1, 0xff),
            PHYSICAL_MINIMUM(1, 0x00),
            PHYSICAL_MAXIMUM(1, 0xff),
            REPORT_SIZE(1, 0x08),
            REPORT_COUNT(1, 0x02),
            OUTPUT(1, Data|Var|Abs),
            /* padding */
            REPORT_COUNT(1, 0x02),
            REPORT_SIZE(1, 0x08),
            OUTPUT(1, Data|Var|Abs),
        END_COLLECTION,

        USAGE_PAGE(2, HID_USAGE_PAGE_HAPTICS),
        USAGE(1, HID_USAGE_HAPTICS_SIMPLE_CONTROLLER),
        COLLECTION(1, Logical),
            REPORT_ID(1, haptics_feature_report),

            USAGE(1, HID_USAGE_HAPTICS_WAVEFORM_LIST),
            COLLECTION(1, NamedArray),
                USAGE_PAGE(1, HID_USAGE_PAGE_ORDINAL),
                USAGE(1, 3), /* HID_USAGE_HAPTICS_WAVEFORM_RUMBLE */
                USAGE(1, 4), /* HID_USAGE_HAPTICS_WAVEFORM_BUZZ */
                REPORT_COUNT(1, 2),
                REPORT_SIZE(1, 16),
                FEATURE(1, Data|Var|Abs|Null),
            END_COLLECTION,

            USAGE_PAGE(2, HID_USAGE_PAGE_HAPTICS),
            USAGE(1, HID_USAGE_HAPTICS_DURATION_LIST),
            COLLECTION(1, NamedArray),
                USAGE_PAGE(1, HID_USAGE_PAGE_ORDINAL),
                USAGE(1, 3), /* 0 (HID_USAGE_HAPTICS_WAVEFORM_RUMBLE) */
                USAGE(1, 4), /* 0 (HID_USAGE_HAPTICS_WAVEFORM_BUZZ) */
                REPORT_COUNT(1, 2),
                REPORT_SIZE(1, 16),
                FEATURE(1, Data|Var|Abs|Null),
            END_COLLECTION,

            USAGE_PAGE(2, HID_USAGE_PAGE_HAPTICS),
            USAGE(1, HID_USAGE_HAPTICS_WAVEFORM_CUTOFF_TIME),
            UNIT(2, 0x1001), /* seconds */
            UNIT_EXPONENT(1, -3), /* 10^-3 */
            LOGICAL_MINIMUM(4, 0x00000000),
            LOGICAL_MAXIMUM(4, 0x7fffffff),
            PHYSICAL_MINIMUM(4, 0x00000000),
            PHYSICAL_MAXIMUM(4, 0x7fffffff),
            REPORT_SIZE(1, 32),
            REPORT_COUNT(1, 1),
            FEATURE(1, Data|Var|Abs),
            /* reset global items */
            UNIT(1, 0), /* None */
            UNIT_EXPONENT(1, 0),

            REPORT_ID(1, haptics_waveform_report),
            USAGE(1, HID_USAGE_HAPTICS_MANUAL_TRIGGER),
            LOGICAL_MINIMUM(1, 1),
            LOGICAL_MAXIMUM(1, 4),
            PHYSICAL_MINIMUM(1, 1),
            PHYSICAL_MAXIMUM(1, 4),
            REPORT_SIZE(1, 16),
            REPORT_COUNT(1, 1),
            OUTPUT(1, Data|Var|Abs),

            USAGE(1, HID_USAGE_HAPTICS_INTENSITY),
            LOGICAL_MINIMUM(4, 0x00000000),
            LOGICAL_MAXIMUM(4, 0x0000ffff),
            PHYSICAL_MINIMUM(4, 0x00000000),
            PHYSICAL_MAXIMUM(4, 0x0000ffff),
            REPORT_SIZE(1, 16),
            REPORT_COUNT(1, 1),
            OUTPUT(1, Data|Var|Abs),
        END_COLLECTION,
    };

    *id = report_id;
    if (!hid_descriptor_append(desc, template, sizeof(template))) return FALSE;

    haptics->features.waveform_list[0] = HID_USAGE_HAPTICS_WAVEFORM_RUMBLE;
    haptics->features.waveform_list[1] = HID_USAGE_HAPTICS_WAVEFORM_BUZZ;
    haptics->features.duration_list[0] = 0;
    haptics->features.duration_list[1] = 0;
    haptics->features.waveform_cutoff_time = 1000;
    return TRUE;
}

void handle_haptics_set_output_report(struct hid_descriptor *desc, struct haptics *haptics,
                                      HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    ULONG capacity = packet->reportBufferLen;

    if (packet->reportId == desc->haptics_waveform_report)
    {
        struct haptics_waveform *output = (struct haptics_waveform *)(packet->reportBuffer + 1);
        io->Information = sizeof(*output) + 1;

        if (capacity < io->Information)
            io->Status = STATUS_BUFFER_TOO_SMALL;
        else if (output->manual_trigger == 0 || output->manual_trigger > HAPTICS_WAVEFORM_LAST_INDEX)
            io->Status = STATUS_INVALID_PARAMETER;
        else
        {
            if (output->manual_trigger == HAPTICS_WAVEFORM_STOP_INDEX) memset(haptics->waveforms, 0, sizeof(haptics->waveforms));
            else haptics->waveforms[output->manual_trigger] = *output;
            io->Status = STATUS_SUCCESS;
        }
    }
    else
    {
        io->Information = 0;
        io->Status = STATUS_NOT_IMPLEMENTED;
    }
}

void handle_haptics_set_feature_report(struct hid_descriptor *desc, struct haptics *haptics,
                                       HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    ULONG capacity = packet->reportBufferLen;

    if (packet->reportId == desc->haptics_feature_report)
    {
        struct haptics_features *features = (struct haptics_features *)(packet->reportBuffer + 1);
        io->Information = sizeof(*features) + 1;

        if (capacity < io->Information)
            io->Status = STATUS_BUFFER_TOO_SMALL;
        else
        {
            haptics->features.waveform_cutoff_time = features->waveform_cutoff_time;
            io->Status = STATUS_SUCCESS;
        }
    }
    else
    {
        io->Information = 0;
        io->Status = STATUS_NOT_IMPLEMENTED;
    }
}

void handle_haptics_get_feature_report(struct hid_descriptor *desc, struct haptics *haptics,
                                       HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    ULONG capacity = packet->reportBufferLen;

    if (packet->reportId == desc->haptics_feature_report)
    {
        struct haptics_features *features = (struct haptics_features *)(packet->reportBuffer + 1);
        io->Information = sizeof(*features) + 1;

        if (capacity < io->Information)
            io->Status = STATUS_BUFFER_TOO_SMALL;
        else
        {
            *features = haptics->features;
            io->Status = STATUS_SUCCESS;
        }
    }
    else
    {
        io->Information = 0;
        io->Status = STATUS_NOT_IMPLEMENTED;
    }
}

#include "pop_hid_macros.h"
