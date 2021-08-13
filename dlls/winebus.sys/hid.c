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

BOOL hid_descriptor_add_haptics(struct hid_descriptor *desc, struct haptics *haptics)
{
    const BYTE haptics_feature_report = desc->haptics_feature_report = ++desc->next_report_id[HidP_Feature];
    const BYTE haptics_waveform_report = desc->haptics_waveform_report = ++desc->next_report_id[HidP_Output];
    const BYTE template[] =
    {
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

static BOOL hid_descriptor_add_envelope(struct hid_descriptor *desc)
{
    const BYTE report_id = desc->physical_set_envelope_report = ++desc->next_report_id[HidP_Output];
    const BYTE template[] =
    {
        /* Envelope Report Definition */
        USAGE(1, WINE_HID_USAGE_PID_SET_ENVELOPE_REPORT),
        COLLECTION(1, Logical),
            REPORT_ID(1, report_id),

            USAGE(1, WINE_HID_USAGE_PID_PARAMETER_BLOCK_OFFSET),
            LOGICAL_MAXIMUM(2, 0x7ffd),
            REPORT_SIZE(1, 15),
            REPORT_COUNT(1, 1),
            OUTPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_ROM_FLAG),
            LOGICAL_MAXIMUM(1, 1),
            REPORT_SIZE(1, 1),
            OUTPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_ATTACK_LEVEL),
            USAGE(1, WINE_HID_USAGE_PID_FADE_LEVEL),
            LOGICAL_MAXIMUM(2, 0x00ff),
            REPORT_SIZE(1, 8),
            REPORT_COUNT(1, 2),
            OUTPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_ATTACK_TIME),
            USAGE(1, WINE_HID_USAGE_PID_FADE_TIME),
            LOGICAL_MAXIMUM(2, 10000),
            PHYSICAL_MAXIMUM(2, 10000),
            UNIT(2, 0x1003), /* Eng Lin:Time */
            UNIT_EXPONENT(1, -3),
            REPORT_SIZE(1, 16),
            OUTPUT(1, Data|Var|Abs),

            PHYSICAL_MAXIMUM(1, 0),
            UNIT(1, 0),
            UNIT_EXPONENT(1, 0),
        END_COLLECTION,
    };

    return hid_descriptor_append(desc, template, sizeof(template));
}

static BOOL hid_descriptor_add_periodic(struct hid_descriptor *desc)
{
    const BYTE report_id = desc->physical_set_periodic_report = ++desc->next_report_id[HidP_Output];
    const BYTE template[] =
    {
        /* Periodic Report Definition */
        USAGE(1, WINE_HID_USAGE_PID_SET_PERIODIC_REPORT),
        COLLECTION(1, Logical),
            REPORT_ID(1, report_id),

            USAGE(1, WINE_HID_USAGE_PID_PARAMETER_BLOCK_OFFSET),
            LOGICAL_MAXIMUM(2, 0x7ffd),
            REPORT_SIZE(1, 15),
            REPORT_COUNT(1, 1),
            OUTPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_ROM_FLAG),
            LOGICAL_MAXIMUM(1, 1),
            REPORT_SIZE(1, 1),
            OUTPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_MAGNITUDE),
            USAGE(1, WINE_HID_USAGE_PID_OFFSET),
            USAGE(1, WINE_HID_USAGE_PID_PHASE),
            LOGICAL_MAXIMUM(2, 0x00ff),
            REPORT_SIZE(1, 8),
            REPORT_COUNT(1, 3),
            OUTPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_PERIOD),
            LOGICAL_MAXIMUM(2, 10000),
            PHYSICAL_MAXIMUM(2, 10000),
            UNIT(2, 0x1003), /* Eng Lin:Time */
            UNIT_EXPONENT(1, -3),
            REPORT_SIZE(1, 16),
            REPORT_COUNT(1, 1),
            OUTPUT(1, Data|Var|Abs),

            PHYSICAL_MAXIMUM(1, 0),
            UNIT(1, 0), /* None */
            UNIT_EXPONENT(1, 0),
        END_COLLECTION,
    };

    return hid_descriptor_append(desc, template, sizeof(template));
}

static BOOL hid_descriptor_add_condition(struct hid_descriptor *desc)
{
    const BYTE report_id = desc->physical_set_condition_report = ++desc->next_report_id[HidP_Output];
    const BYTE template[] =
    {
        /* Condition Report Definition */
        USAGE(1, WINE_HID_USAGE_PID_SET_CONDITION_REPORT),
        COLLECTION(1, Logical),
            REPORT_ID(1, report_id),

            USAGE(1, WINE_HID_USAGE_PID_PARAMETER_BLOCK_OFFSET),
            LOGICAL_MAXIMUM(2, 0x7ffd),
            REPORT_SIZE(1, 15),
            REPORT_COUNT(1, 1),
            OUTPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_ROM_FLAG),
            LOGICAL_MAXIMUM(1, 1),
            REPORT_SIZE(1, 1),
            OUTPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_CP_OFFSET),
            USAGE(1, WINE_HID_USAGE_PID_POSITIVE_COEFFICIENT),
            USAGE(1, WINE_HID_USAGE_PID_NEGATIVE_COEFFICIENT),
            USAGE(1, WINE_HID_USAGE_PID_POSITIVE_SATURATION),
            USAGE(1, WINE_HID_USAGE_PID_NEGATIVE_SATURATION),
            USAGE(1, WINE_HID_USAGE_PID_DEAD_BAND),
            LOGICAL_MAXIMUM(2, 0x00ff),
            REPORT_SIZE(1, 8),
            REPORT_COUNT(1, 6),
            OUTPUT(1, Data|Var|Abs),
        END_COLLECTION,
    };

    return hid_descriptor_append(desc, template, sizeof(template));
}

static BOOL hid_descriptor_add_constant_force(struct hid_descriptor *desc)
{
    const BYTE report_id = desc->physical_set_constant_report = ++desc->next_report_id[HidP_Output];
    const BYTE template[] =
    {
        /* Constant Force Report Definition */
        USAGE(1, WINE_HID_USAGE_PID_SET_CONSTANT_FORCE_REPORT),
        COLLECTION(1, Logical),
            REPORT_ID(1, report_id),

            USAGE(1, WINE_HID_USAGE_PID_PARAMETER_BLOCK_OFFSET),
            LOGICAL_MAXIMUM(2, 0x7ffd),
            REPORT_SIZE(1, 15),
            REPORT_COUNT(1, 1),
            OUTPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_ROM_FLAG),
            LOGICAL_MAXIMUM(1, 1),
            REPORT_SIZE(1, 1),
            OUTPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_MAGNITUDE),
            LOGICAL_MAXIMUM(2, 0x00ff),
            REPORT_SIZE(1, 8),
            OUTPUT(1, Data|Var|Abs),
        END_COLLECTION,
    };

    return hid_descriptor_append(desc, template, sizeof(template));
}

static BOOL hid_descriptor_add_ramp_force(struct hid_descriptor *desc)
{
    const BYTE report_id = desc->physical_set_ramp_report = ++desc->next_report_id[HidP_Output];
    const BYTE template[] =
    {
        /* Ramp Force Report Definition */
        USAGE(1, WINE_HID_USAGE_PID_SET_RAMP_FORCE_REPORT),
        COLLECTION(1, Logical),
            REPORT_ID(1, report_id),

            USAGE(1, WINE_HID_USAGE_PID_PARAMETER_BLOCK_OFFSET),
            LOGICAL_MAXIMUM(2, 0x7ffd),
            REPORT_SIZE(1, 15),
            REPORT_COUNT(1, 1),
            OUTPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_ROM_FLAG),
            LOGICAL_MAXIMUM(1, 1),
            REPORT_SIZE(1, 1),
            OUTPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_RAMP_START),
            USAGE(1, WINE_HID_USAGE_PID_RAMP_END),
            LOGICAL_MAXIMUM(2, 0x00ff),
            REPORT_SIZE(1, 8),
            REPORT_COUNT(1, 2),
            OUTPUT(1, Data|Var|Abs),
        END_COLLECTION,
    };

    return hid_descriptor_append(desc, template, sizeof(template));
}

static BOOL hid_descriptor_add_custom_force(struct hid_descriptor *desc)
{
    const BYTE custom_force_data_report = desc->physical_custom_force_data_report = ++desc->next_report_id[HidP_Output];
    const BYTE download_force_sample_report = desc->physical_download_force_sample_report = ++desc->next_report_id[HidP_Output];
    const BYTE set_custom_force_report = desc->physical_set_custom_force_report = ++desc->next_report_id[HidP_Output];
    const BYTE template[] =
    {
        /* Custom Force Data Report Definition */
        /*  Downloads are always into RAM space so the ROM usage is not declared. */
        USAGE(1, WINE_HID_USAGE_PID_CUSTOM_FORCE_DATA_REPORT),
        COLLECTION(1, Logical),
            REPORT_ID(1, custom_force_data_report),

            USAGE(1, WINE_HID_USAGE_PID_PARAMETER_BLOCK_OFFSET),
            LOGICAL_MAXIMUM(2, 0x7ffd),
            REPORT_SIZE(1, 15),
            REPORT_COUNT(1, 1),
            OUTPUT(1, Data|Var|Abs),

            USAGE(4, (HID_USAGE_PAGE_GENERIC<<16)|HID_USAGE_GENERIC_BYTE_COUNT),
            LOGICAL_MAXIMUM(2, 0x0100),
            REPORT_SIZE(1, 9),
            OUTPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_CUSTOM_FORCE_DATA),
            LOGICAL_MAXIMUM(2, 0x00ff),
            REPORT_SIZE(1, 8),
            REPORT_COUNT(2, 0x0100),
            OUTPUT(2, Data|Var|Abs|Buff),
        END_COLLECTION,

        /* Download Force Sample Definition */
        USAGE(1, WINE_HID_USAGE_PID_DOWNLOAD_FORCE_SAMPLE),
        COLLECTION(1, Logical),
            REPORT_ID(1, download_force_sample_report),

            USAGE(4, (HID_USAGE_PAGE_GENERIC<<16)|HID_USAGE_GENERIC_POINTER),
            COLLECTION(1, Logical),
                USAGE(4, (HID_USAGE_PAGE_GENERIC<<16)|HID_USAGE_GENERIC_X),
                USAGE(4, (HID_USAGE_PAGE_GENERIC<<16)|HID_USAGE_GENERIC_Y),
                LOGICAL_MINIMUM(1, 0x81),
                LOGICAL_MAXIMUM(1, 0x7f),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 2),
                OUTPUT(1, Data|Var|Abs),
            END_COLLECTION,
        END_COLLECTION,

        /* Define the Custom Force parameter block */
        /*  Custom Effects are always RAM based */
        /*  so ROM flags are not declared. */
        USAGE(1, WINE_HID_USAGE_PID_SET_CUSTOM_FORCE_REPORT),
        COLLECTION(1, Logical),
            REPORT_ID(1, set_custom_force_report),

            /*  Parameter block offset in pool */
            /*  Custom Force data offset in pool */
            USAGE(1, WINE_HID_USAGE_PID_PARAMETER_BLOCK_OFFSET),
            USAGE(1, WINE_HID_USAGE_PID_CUSTOM_FORCE_DATA_OFFSET),
            USAGE(1, WINE_HID_USAGE_PID_SAMPLE_COUNT),
            LOGICAL_MINIMUM(1, 0),
            LOGICAL_MAXIMUM(2, 0x7ffd),
            REPORT_COUNT(1, 3),
            REPORT_SIZE(1, 16),
            OUTPUT(1, Data|Var|Abs),
        END_COLLECTION,
    };

    return hid_descriptor_append(desc, template, sizeof(template));
}

BOOL hid_descriptor_add_physical_effects(struct hid_descriptor *desc, USHORT count, USAGE *usages)
{
    const BYTE device_state_report = desc->physical_device_state_report = ++desc->next_report_id[HidP_Output];
    const BYTE device_gain_report = desc->physical_device_gain_report = ++desc->next_report_id[HidP_Feature];
    const BYTE device_state[] =
    {
        USAGE_PAGE(1, HID_USAGE_PAGE_PID),
        USAGE(1, WINE_HID_USAGE_PID_DEVICE_CONTROL_REPORT),
        COLLECTION(1, Logical),
            REPORT_ID(1, device_state_report),

            USAGE(1, WINE_HID_USAGE_PID_DEVICE_CONTROL),
            COLLECTION(1, Logical),
                USAGE(1, WINE_HID_USAGE_PID_DC_ENABLE_ACTUATORS),
                USAGE(1, WINE_HID_USAGE_PID_DC_DISABLE_ACTUATORS),
                USAGE(1, WINE_HID_USAGE_PID_DC_STOP_ALL_EFFECTS),
                USAGE(1, WINE_HID_USAGE_PID_DC_DEVICE_RESET),
                USAGE(1, WINE_HID_USAGE_PID_DC_DEVICE_PAUSE),
                USAGE(1, WINE_HID_USAGE_PID_DC_DEVICE_CONTINUE),
                LOGICAL_MINIMUM(1, 1),
                LOGICAL_MAXIMUM(1, 6),
                REPORT_SIZE(1, 1),
                REPORT_COUNT(1, 4),
                OUTPUT(1, Data|Ary|Abs),
            END_COLLECTION,
        END_COLLECTION,

        USAGE(1, WINE_HID_USAGE_PID_DEVICE_GAIN_REPORT),
        COLLECTION(1, Logical),
            REPORT_ID(1, device_gain_report),

            USAGE(1, WINE_HID_USAGE_PID_DEVICE_GAIN),
            LOGICAL_MAXIMUM(2, 0x00ff),
            REPORT_SIZE(1, 8),
            REPORT_COUNT(1, 1),
            FEATURE(1, Data|Var|Abs),
        END_COLLECTION,
    };

    const BYTE create_effect_report = desc->physical_create_effect_report = ++desc->next_report_id[HidP_Feature];
    const BYTE query_effect_report = desc->physical_query_effect_report = ++desc->next_report_id[HidP_Feature];
    const BYTE free_effect_report = desc->physical_free_effect_report = ++desc->next_report_id[HidP_Feature];
    const BYTE new_effect_header[] =
    {
        /* Create new effect */
        USAGE(1, WINE_HID_USAGE_PID_CREATE_NEW_EFFECT_REPORT),
        COLLECTION(1, Logical),
            REPORT_ID(1, create_effect_report),

            USAGE(1, WINE_HID_USAGE_PID_EFFECT_TYPE),
            COLLECTION(1, Logical),
    };
    const BYTE new_effect_footer[] =
    {
                LOGICAL_MAXIMUM(1, count),
                LOGICAL_MINIMUM(1, 1),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 1),
                FEATURE(1, Data|Ary|Abs),
            END_COLLECTION,

            USAGE(4, (HID_USAGE_PAGE_GENERIC<<16)|HID_USAGE_GENERIC_BYTE_COUNT),
            LOGICAL_MINIMUM(1, 0),
            LOGICAL_MAXIMUM(1, 0xff),
            REPORT_SIZE(1, 8),
            REPORT_COUNT(1, 1),
            FEATURE(1, Data|Var|Abs),
        END_COLLECTION,

        /* Query created effect */
        USAGE(1, WINE_HID_USAGE_PID_BLOCK_LOAD_REPORT),
        COLLECTION(1, Logical),
            REPORT_ID(1, query_effect_report),

            USAGE(1, WINE_HID_USAGE_PID_EFFECT_BLOCK_INDEX),
            LOGICAL_MAXIMUM(1, 10),
            LOGICAL_MINIMUM(1, 1),
            REPORT_SIZE(1, 8),
            REPORT_COUNT(1, 1),
            FEATURE(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_BLOCK_LOAD_STATUS),
            COLLECTION(1, Logical),
                USAGE(1, WINE_HID_USAGE_PID_BLOCK_LOAD_SUCCESS),
                USAGE(1, WINE_HID_USAGE_PID_BLOCK_LOAD_FULL),
                USAGE(1, WINE_HID_USAGE_PID_BLOCK_LOAD_ERROR),
                LOGICAL_MAXIMUM(1, 3),
                LOGICAL_MINIMUM(1, 1),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 1),
                FEATURE(1, Data|Ary|Abs),
            END_COLLECTION,

            USAGE(1, WINE_HID_USAGE_PID_RAM_POOL_AVAILABLE),
            LOGICAL_MINIMUM(1, 0),
            LOGICAL_MAXIMUM(2, 0xffff),
            REPORT_SIZE(1, 16),
            REPORT_COUNT(1, 1),
            FEATURE(1, Data|Var|Abs),
        END_COLLECTION,

        /* Free created effect */
        USAGE(1, WINE_HID_USAGE_PID_BLOCK_FREE_REPORT),
        COLLECTION(1, Logical),
            REPORT_ID(1, free_effect_report),

            USAGE(1, WINE_HID_USAGE_PID_EFFECT_BLOCK_INDEX),
            LOGICAL_MAXIMUM(1, 10),
            LOGICAL_MINIMUM(1, 1),
            REPORT_SIZE(1, 8),
            REPORT_COUNT(1, 1),
            FEATURE(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_RAM_POOL_AVAILABLE),
            LOGICAL_MINIMUM(1, 0),
            LOGICAL_MAXIMUM(2, 0xffff),
            REPORT_SIZE(1, 16),
            REPORT_COUNT(1, 1),
            FEATURE(1, Data|Var|Abs),
        END_COLLECTION,
    };

    const BYTE set_effect_report = desc->physical_set_effect_report = ++desc->next_report_id[HidP_Output];
    const BYTE effect_operation_report = desc->physical_effect_operation_report = ++desc->next_report_id[HidP_Output];
    const BYTE effect_state_report = desc->physical_effect_state_report = ++desc->next_report_id[HidP_Input];
    const BYTE set_effect_header[] =
    {
        /* Set effect properties */
        USAGE(1, WINE_HID_USAGE_PID_SET_EFFECT_REPORT),
        COLLECTION(1, Logical),
            REPORT_ID(1, set_effect_report),

            USAGE(1, WINE_HID_USAGE_PID_EFFECT_BLOCK_INDEX),
            LOGICAL_MAXIMUM(1, 0x7f),
            REPORT_SIZE(1, 7),
            REPORT_COUNT(1, 1),
            OUTPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_ROM_FLAG),
            LOGICAL_MAXIMUM(1, 1),
            REPORT_SIZE(1, 1),
            OUTPUT(1, Data|Var|Abs),

            /* Define the available effect types. Effect Type is a named array that will */
            /*  accept any of the ET usages listed. */
            USAGE(1, WINE_HID_USAGE_PID_EFFECT_TYPE),
            COLLECTION(1, Logical),
    };
    const BYTE set_effect_footer[] =
    {
                LOGICAL_MINIMUM(1, 1),
                LOGICAL_MAXIMUM(1, 10),
                REPORT_SIZE(1, 8),
                OUTPUT(1, Data|Ary|Abs),
            END_COLLECTION,

            USAGE(1, WINE_HID_USAGE_PID_DURATION),
            USAGE(1, WINE_HID_USAGE_PID_TRIGGER_REPEAT_INTERVAL),
            LOGICAL_MINIMUM(1, 0),
            LOGICAL_MAXIMUM(2, 10000),
            PHYSICAL_MAXIMUM(2, 10000),
            REPORT_SIZE(1, 16),
            UNIT(2, 0x1003), /* Eng Lin:Time */
            UNIT_EXPONENT(1, -3),
            REPORT_COUNT(1, 2),
            OUTPUT(1, Data|Var|Abs),

            UNIT_EXPONENT(1, -6),
            USAGE(1, WINE_HID_USAGE_PID_SAMPLE_PERIOD),
            REPORT_COUNT(1, 1),
            OUTPUT(1, Data|Var|Abs),
            PHYSICAL_MAXIMUM(1, 0),
            UNIT_EXPONENT(1, 0),
            UNIT(1, 0), /* None */

            USAGE(1, WINE_HID_USAGE_PID_GAIN),
            USAGE(1, WINE_HID_USAGE_PID_TRIGGER_BUTTON),
            LOGICAL_MAXIMUM(1, 0x7f),
            REPORT_SIZE(1, 8),
            REPORT_COUNT(1, 2),
            OUTPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_AXES_ENABLE),
            COLLECTION(1, Logical),
                USAGE(4, (HID_USAGE_PAGE_GENERIC<<16)|HID_USAGE_GENERIC_POINTER),
                COLLECTION(1, Physical),
                    USAGE(4, (HID_USAGE_PAGE_GENERIC<<16)|HID_USAGE_GENERIC_X),
                    USAGE(4, (HID_USAGE_PAGE_GENERIC<<16)|HID_USAGE_GENERIC_Y),
                    LOGICAL_MAXIMUM(1, 1),
                    REPORT_SIZE(1, 1),
                    REPORT_COUNT(1, 2),
                    OUTPUT(1, Data|Var|Abs),
                END_COLLECTION,
            END_COLLECTION,

            REPORT_COUNT(1, 6),
            OUTPUT(1, Cnst|Var|Abs), /* 6-bit pad */

            USAGE(1, WINE_HID_USAGE_PID_DIRECTION),
            COLLECTION(1, Logical),
                USAGE(4, (HID_USAGE_PAGE_GENERIC<<16)|HID_USAGE_GENERIC_POINTER),
                COLLECTION(1, Physical),
                    USAGE(4, (HID_USAGE_PAGE_GENERIC<<16)|HID_USAGE_GENERIC_X),
                    USAGE(4, (HID_USAGE_PAGE_GENERIC<<16)|HID_USAGE_GENERIC_Y),
                    LOGICAL_MINIMUM(1, 0),
                    LOGICAL_MAXIMUM(2, 0x00ff),
                    PHYSICAL_MAXIMUM(1, 0x168),
                    UNIT(2, 0x0014), /* Eng Rot:Angular Pos */
                    REPORT_SIZE(1, 8),
                    REPORT_COUNT(1, 2),
                    OUTPUT(1, Data|Var|Abs),
                    UNIT(1, 0), /* None */
                    PHYSICAL_MAXIMUM(1, 0),
                END_COLLECTION,
            END_COLLECTION,

            USAGE(1, WINE_HID_USAGE_PID_TYPE_SPECIFIC_BLOCK_OFFSET),
            COLLECTION(1, Logical),
                USAGE(4, (HID_USAGE_PAGE_ORDINAL<<16)|1),
                USAGE(4, (HID_USAGE_PAGE_ORDINAL<<16)|2),
                LOGICAL_MAXIMUM(2, 0x7ffd),
                REPORT_SIZE(1, 16),
                REPORT_COUNT(1, 2),
                OUTPUT(1, Data|Var|Abs),
            END_COLLECTION,
        END_COLLECTION,

        /* Control effect state */
        USAGE(1, WINE_HID_USAGE_PID_EFFECT_OPERATION_REPORT),
        COLLECTION(1, Logical),
            REPORT_ID(1, effect_operation_report),

            USAGE(1, WINE_HID_USAGE_PID_EFFECT_BLOCK_INDEX),
            LOGICAL_MAXIMUM(1, 0x7f),
            REPORT_SIZE(1, 7),
            REPORT_COUNT(1, 1),
            OUTPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_ROM_FLAG),
            LOGICAL_MAXIMUM(1, 1),
            REPORT_SIZE(1, 1),
            OUTPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_EFFECT_OPERATION),
            COLLECTION(1, Logical),
                USAGE(1, WINE_HID_USAGE_PID_OP_EFFECT_START),
                USAGE(1, WINE_HID_USAGE_PID_OP_EFFECT_START_SOLO),
                USAGE(1, WINE_HID_USAGE_PID_OP_EFFECT_STOP),
                LOGICAL_MINIMUM(1, 1),
                LOGICAL_MAXIMUM(1, 3),
                REPORT_SIZE(1, 8),
                OUTPUT(1, Data|Ary|Abs),
            END_COLLECTION,

            USAGE(1, WINE_HID_USAGE_PID_LOOP_COUNT),
            LOGICAL_MINIMUM(1, 0),
            LOGICAL_MAXIMUM(2, 0x00ff),
            OUTPUT(1, Data|Var|Abs),
        END_COLLECTION,

        /* Report effect state */
        USAGE(1, WINE_HID_USAGE_PID_STATE_REPORT),
        COLLECTION(1, Logical),
            REPORT_ID(1, effect_state_report),

            USAGE(1, WINE_HID_USAGE_PID_EFFECT_BLOCK_INDEX),
            LOGICAL_MAXIMUM(1, 0x7f),
            REPORT_SIZE(1, 7),
            INPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_ROM_FLAG),
            LOGICAL_MAXIMUM(1, 1),
            REPORT_SIZE(1, 1),
            REPORT_COUNT(1, 1),
            INPUT(1, Data|Var|Abs),

            USAGE(1, WINE_HID_USAGE_PID_EFFECT_PLAYING),
            USAGE(1, WINE_HID_USAGE_PID_ACTUATORS_ENABLED),
            USAGE(1, WINE_HID_USAGE_PID_SAFETY_SWITCH),
            USAGE(1, WINE_HID_USAGE_PID_ACTUATOR_POWER),
            REPORT_SIZE(1, 1),
            REPORT_COUNT(1, 4),
            INPUT(1, Data|Var|Abs),

            INPUT(1, Cnst|Var|Abs),     /*  4-bit pad */
        END_COLLECTION,
    };

    USHORT i;
    BOOL envelope = FALSE, condition = FALSE, periodic = FALSE, constant = FALSE, ramp = FALSE, custom = FALSE;

    if (!hid_descriptor_append(desc, device_state, sizeof(device_state)))
        return FALSE;

    if (!hid_descriptor_append(desc, new_effect_header, sizeof(new_effect_header)))
        return FALSE;
    for (i = 0; i < count; ++i)
        if (!hid_descriptor_append_usage(desc, usages[i]))
            return FALSE;
    if (!hid_descriptor_append(desc, new_effect_footer, sizeof(new_effect_footer)))
        return FALSE;


    if (!hid_descriptor_append(desc, set_effect_header, sizeof(set_effect_header)))
        return FALSE;
    for (i = 0; i < count; ++i)
        if (!hid_descriptor_append_usage(desc, usages[i]))
            return FALSE;
    if (!hid_descriptor_append(desc, set_effect_footer, sizeof(set_effect_footer)))
        return FALSE;

    for (i = 0; i < count; ++i)
    {
        if (usages[i] == WINE_HID_USAGE_PID_ET_SINE ||
            usages[i] == WINE_HID_USAGE_PID_ET_SQUARE ||
            usages[i] == WINE_HID_USAGE_PID_ET_TRIANGLE ||
            usages[i] == WINE_HID_USAGE_PID_ET_SAWTOOTH_UP ||
            usages[i] == WINE_HID_USAGE_PID_ET_SAWTOOTH_DOWN)
            envelope = periodic = TRUE;
        if (usages[i] == WINE_HID_USAGE_PID_ET_SPRING ||
            usages[i] == WINE_HID_USAGE_PID_ET_DAMPER ||
            usages[i] == WINE_HID_USAGE_PID_ET_INERTIA ||
            usages[i] == WINE_HID_USAGE_PID_ET_FRICTION)
            condition = TRUE;
        if (usages[i] == WINE_HID_USAGE_PID_ET_CONSTANT_FORCE)
            envelope = constant = TRUE;
        if (usages[i] == WINE_HID_USAGE_PID_ET_RAMP)
            envelope = ramp = TRUE;
        if (usages[i] == WINE_HID_USAGE_PID_ET_CUSTOM_FORCE_DATA)
            custom = TRUE;
    }

    if (envelope && !hid_descriptor_add_envelope(desc))
        return FALSE;
    if (periodic && !hid_descriptor_add_periodic(desc))
        return FALSE;
    if (condition && !hid_descriptor_add_condition(desc))
        return FALSE;
    if (constant && !hid_descriptor_add_constant_force(desc))
        return FALSE;
    if (ramp && !hid_descriptor_add_ramp_force(desc))
        return FALSE;
    if (custom && !hid_descriptor_add_custom_force(desc))
        return FALSE;

    return TRUE;
}

NTSTATUS handle_physical_set_output_report(struct hid_descriptor *desc, struct haptics *haptics,
                                           BYTE *report, ULONG_PTR *length)
{
    ULONG_PTR capacity = *length;

#if 0
    const BYTE report_id = desc->physical_set_envelope_report = ++desc->next_report_id[HidP_Output];
    const BYTE report_id = desc->physical_set_periodic_report = ++desc->next_report_id[HidP_Output];
    const BYTE report_id = desc->physical_set_condition_report = ++desc->next_report_id[HidP_Output];
    const BYTE report_id = desc->physical_set_constant_report = ++desc->next_report_id[HidP_Output];
    const BYTE report_id = desc->physical_set_ramp_report = ++desc->next_report_id[HidP_Output];
    const BYTE custom_force_data_report = desc->physical_custom_force_data_report = ++desc->next_report_id[HidP_Output];
    const BYTE download_force_sample_report = desc->physical_download_force_sample_report = ++desc->next_report_id[HidP_Output];
    const BYTE set_custom_force_report = desc->physical_set_custom_force_report = ++desc->next_report_id[HidP_Output];
    const BYTE device_state_report = desc->physical_device_state_report = ++desc->next_report_id[HidP_Output];
    const BYTE set_effect_report = desc->physical_set_effect_report = ++desc->next_report_id[HidP_Output];
    const BYTE effect_operation_report = desc->physical_effect_operation_report = ++desc->next_report_id[HidP_Output];

    const BYTE effect_state_report = desc->physical_effect_state_report = ++desc->next_report_id[HidP_Input];
#endif

    if (capacity && report[0] == desc->haptics_waveform_report)
    {
        struct haptics_waveform *output = (struct haptics_waveform *)(report + 1);
        if (capacity - 1 < sizeof(*output)) return STATUS_BUFFER_TOO_SMALL;
        if (output->manual_trigger == 0 || output->manual_trigger > HAPTICS_WAVEFORM_LAST_INDEX) return STATUS_INVALID_PARAMETER;

        if (output->manual_trigger == HAPTICS_WAVEFORM_STOP_INDEX) memset(haptics->waveforms, 0, sizeof(haptics->waveforms));
        else haptics->waveforms[output->manual_trigger] = *output;

        *length = sizeof(*output) + 1;
        return STATUS_SUCCESS;
    }

    *length = 0;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS handle_physical_set_feature_report(struct hid_descriptor *desc, struct haptics *haptics,
                                            BYTE *report, ULONG_PTR *length)
{
    ULONG_PTR capacity = *length;

#if 0
    const BYTE device_gain_report = desc->physical_device_gain_report = ++desc->next_report_id[HidP_Feature];

    const BYTE create_effect_report = desc->physical_create_effect_report = ++desc->next_report_id[HidP_Feature];
    const BYTE free_effect_report = desc->physical_free_effect_report = ++desc->next_report_id[HidP_Feature];
#endif

    if (capacity && report[0] == desc->haptics_feature_report)
    {
        struct haptics_features *features = (struct haptics_features *)(report + 1);
        if (capacity - 1 < sizeof(*features)) return STATUS_BUFFER_TOO_SMALL;
        haptics->features.waveform_cutoff_time = features->waveform_cutoff_time;
        *length = sizeof(*features) + 1;
        return STATUS_SUCCESS;
    }

    *length = 0;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS handle_physical_get_feature_report(struct hid_descriptor *desc, struct haptics *haptics,
                                            BYTE *report, ULONG_PTR *length)
{
    ULONG_PTR capacity = *length;

    if (capacity && report[0] == desc->physical_query_effect_report)
    {
        return STATUS_SUCCESS;
    }

    *length = 0;
    return STATUS_NOT_IMPLEMENTED;
}

#include "pop_hid_macros.h"
