/*
 * Human Input Devices
 *
 * Copyright (C) 2015 Aric Stewart
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

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "winioctl.h"
#include "ddk/wdm.h"

#include "hidusage.h"
#include "ddk/hidpi.h"
#include "wine/hid.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(hidp);

static NTSTATUS get_value_caps_range( struct hid_preparsed_data *preparsed, HIDP_REPORT_TYPE report_type, ULONG report_len,
                                      const struct hid_value_caps **caps, const struct hid_value_caps **caps_end )
{
    if (!preparsed || preparsed->magic != HID_MAGIC) return HIDP_STATUS_INVALID_PREPARSED_DATA;

    switch (report_type)
    {
    case HidP_Input:
        if (report_len && report_len != preparsed->caps.InputReportByteLength)
            return HIDP_STATUS_INVALID_REPORT_LENGTH;
        *caps = HID_INPUT_VALUE_CAPS( preparsed );
        break;
    case HidP_Output:
        if (report_len && report_len != preparsed->caps.OutputReportByteLength)
            return HIDP_STATUS_INVALID_REPORT_LENGTH;
        *caps = HID_OUTPUT_VALUE_CAPS( preparsed );
        break;
    case HidP_Feature:
        if (report_len && report_len != preparsed->caps.FeatureReportByteLength)
            return HIDP_STATUS_INVALID_REPORT_LENGTH;
        *caps = HID_FEATURE_VALUE_CAPS( preparsed );
        break;
    default:
        return HIDP_STATUS_INVALID_REPORT_TYPE;
    }

    *caps_end = *caps + preparsed->value_caps_count[report_type];
    return HIDP_STATUS_SUCCESS;
}

struct caps_filter
{
    BOOLEAN buttons;
    BOOLEAN values;
    BOOLEAN array;
    USAGE   usage_page;
    USHORT  collection;
    USAGE   usage;
    UCHAR   report_id;
};

static BOOL match_value_caps( const struct hid_value_caps *caps, const struct caps_filter *filter )
{
    if (!caps->usage_min && !caps->usage_max) return FALSE;
    if (filter->buttons && !HID_VALUE_CAPS_IS_BUTTON( caps )) return FALSE;
    if (filter->values && HID_VALUE_CAPS_IS_BUTTON( caps )) return FALSE;
    if (filter->usage_page && filter->usage_page != caps->usage_page) return FALSE;
    if (filter->collection && filter->collection != caps->link_collection) return FALSE;
    if (!filter->usage) return TRUE;
    return caps->usage_min <= filter->usage && caps->usage_max >= filter->usage;
}

typedef NTSTATUS (*enum_value_caps_callback)( const struct hid_value_caps *caps, void *user );

static NTSTATUS enum_value_caps( struct hid_preparsed_data *preparsed, HIDP_REPORT_TYPE report_type,
                                 ULONG report_len, const struct caps_filter *filter,
                                 enum_value_caps_callback callback, void *user, USHORT *count )
{
    const struct hid_value_caps *caps, *caps_end;
    NTSTATUS status;
    BOOL incompatible = FALSE;
    LONG remaining = *count;

    for (status = get_value_caps_range( preparsed, report_type, report_len, &caps, &caps_end );
         status == HIDP_STATUS_SUCCESS && caps != caps_end; caps++)
    {
        if (!match_value_caps( caps, filter )) continue;
        if (filter->report_id && caps->report_id != filter->report_id) incompatible = TRUE;
        else if (filter->array && (caps->is_range || caps->report_count <= 1)) return HIDP_STATUS_NOT_VALUE_ARRAY;
        else if (remaining-- > 0) status = callback( caps, user );
    }

    if (status == HIDP_STATUS_NULL) status = HIDP_STATUS_SUCCESS;
    if (status != HIDP_STATUS_SUCCESS) return status;

    *count -= remaining;
    if (*count == 0) return incompatible ? HIDP_STATUS_INCOMPATIBLE_REPORT_ID : HIDP_STATUS_USAGE_NOT_FOUND;
    if (remaining < 0) return HIDP_STATUS_BUFFER_TOO_SMALL;
    return HIDP_STATUS_SUCCESS;
}

/* copy count bits from src, starting at (-shift) bit if < 0, to dst starting at (shift) bit if > 0 */
static void copy_bits( unsigned char *dst, const unsigned char *src, int count, int shift )
{
    unsigned char bits, mask;
    size_t src_shift = shift < 0 ? (-shift & 7) : 0;
    size_t dst_shift = shift > 0 ? (shift & 7) : 0;
    if (shift < 0) src += -shift / 8;
    if (shift > 0) dst += shift / 8;

    if (src_shift == 0 && dst_shift == 0)
    {
        memcpy( dst, src, count / 8 );
        dst += count / 8;
        src += count / 8;
        count &= 7;
    }

    if (!count) return;

    bits = *dst << (8 - dst_shift);
    count += dst_shift;

    while (count > 8)
    {
        *dst = bits >> (8 - dst_shift);
        bits = *(unsigned short *)src++ >> src_shift;
        *dst++ |= bits << dst_shift;
        count -= 8;
    }

    bits >>= (8 - dst_shift);
    if (count <= 8 - src_shift) bits |= (*src >> src_shift) << dst_shift;
    else bits |= (*(unsigned short *)src >> src_shift) << dst_shift;

    mask = (1 << count) - 1;
    *dst = (bits & mask) | (*dst & ~mask);
}

NTSTATUS WINAPI HidP_GetButtonCaps( HIDP_REPORT_TYPE report_type, HIDP_BUTTON_CAPS *caps, USHORT *caps_count,
                                    PHIDP_PREPARSED_DATA preparsed_data )
{
    return HidP_GetSpecificButtonCaps( report_type, 0, 0, 0, caps, caps_count, preparsed_data );
}

NTSTATUS WINAPI HidP_GetCaps( PHIDP_PREPARSED_DATA preparsed_data, HIDP_CAPS *caps )
{
    struct hid_preparsed_data *preparsed = (struct hid_preparsed_data *)preparsed_data;

    TRACE( "preparsed_data %p, caps %p.\n", preparsed_data, caps );

    if (!preparsed || preparsed->magic != HID_MAGIC) return HIDP_STATUS_INVALID_PREPARSED_DATA;

    *caps = preparsed->caps;
    return HIDP_STATUS_SUCCESS;
}

struct usage_value_params
{
    void *value_buf;
    USHORT value_len;
    void *report_buf;
};

static LONG sign_extend( ULONG value, const struct hid_value_caps *caps )
{
    UINT sign = 1 << (caps->bit_size - 1);
    if (sign <= 1 || caps->logical_min >= 0) return value;
    return value - ((value & sign) << 1);
}

static NTSTATUS get_scaled_usage_value( const struct hid_value_caps *caps, void *user )
{
    struct usage_value_params *params = user;
    ULONG unsigned_value = 0, bit_count = caps->bit_size * caps->report_count;
    LONG signed_value, *value = params->value_buf;

    if ((bit_count + 7) / 8 > sizeof(unsigned_value)) return HIDP_STATUS_BUFFER_TOO_SMALL;
    if (sizeof(LONG) > params->value_len) return HIDP_STATUS_BUFFER_TOO_SMALL;
    copy_bits( (unsigned char *)&unsigned_value, params->report_buf, bit_count, -caps->start_bit );
    signed_value = sign_extend( unsigned_value, caps );

    if (caps->logical_min > caps->logical_max || caps->physical_min > caps->physical_max)
        return HIDP_STATUS_BAD_LOG_PHY_VALUES;
    if (caps->logical_min > signed_value || caps->logical_max < signed_value)
        return HIDP_STATUS_VALUE_OUT_OF_RANGE;

    if (!caps->physical_min && !caps->physical_max) *value = signed_value;
    else *value = caps->physical_min + MulDiv( signed_value - caps->logical_min, caps->physical_max - caps->physical_min,
                                               caps->logical_max - caps->logical_min );
    return HIDP_STATUS_NULL;
}

NTSTATUS WINAPI HidP_GetScaledUsageValue( HIDP_REPORT_TYPE report_type, USAGE usage_page, USHORT collection,
                                          USAGE usage, LONG *value, PHIDP_PREPARSED_DATA preparsed_data,
                                          char *report_buf, ULONG report_len )
{
    struct usage_value_params params = {.value_buf = value, .value_len = sizeof(*value), .report_buf = report_buf};
    struct hid_preparsed_data *preparsed = (struct hid_preparsed_data *)preparsed_data;
    struct caps_filter filter = {.values = TRUE, .usage_page = usage_page, .collection = collection, .usage = usage };
    USHORT count = 1;

    TRACE( "report_type %d, usage_page %x, collection %d, usage %x, value %p, preparsed_data %p, report_buf %p, report_len %u.\n",
           report_type, usage_page, collection, usage, value, preparsed_data, report_buf, report_len );

    *value = 0;
    if (!report_len) return HIDP_STATUS_INVALID_REPORT_LENGTH;

    filter.report_id = report_buf[0];
    return enum_value_caps( preparsed, report_type, report_len, &filter, get_scaled_usage_value, &params, &count );
}

static NTSTATUS get_usage_value( const struct hid_value_caps *caps, void *user )
{
    struct usage_value_params *params = user;
    ULONG bit_count = caps->bit_size * caps->report_count;
    if ((bit_count + 7) / 8 > params->value_len) return HIDP_STATUS_BUFFER_TOO_SMALL;
    memset( params->value_buf, 0, params->value_len );
    copy_bits( params->value_buf, params->report_buf, bit_count, -caps->start_bit );
    return HIDP_STATUS_NULL;
}

NTSTATUS WINAPI HidP_GetUsageValue( HIDP_REPORT_TYPE report_type, USAGE usage_page, USHORT collection, USAGE usage,
                                    ULONG *value, PHIDP_PREPARSED_DATA preparsed_data, char *report_buf, ULONG report_len )
{
    struct usage_value_params params = {.value_buf = value, .value_len = sizeof(*value), .report_buf = report_buf};
    struct hid_preparsed_data *preparsed = (struct hid_preparsed_data *)preparsed_data;
    struct caps_filter filter = {.values = TRUE, .usage_page = usage_page, .collection = collection, .usage = usage};
    USHORT count = 1;

    TRACE( "report_type %d, usage_page %x, collection %d, usage %x, value %p, preparsed_data %p, report_buf %p, report_len %u.\n",
           report_type, usage_page, collection, usage, value, preparsed_data, report_buf, report_len );

    if (!report_len) return HIDP_STATUS_INVALID_REPORT_LENGTH;

    filter.report_id = report_buf[0];
    return enum_value_caps( preparsed, report_type, report_len, &filter, get_usage_value, &params, &count );
}

NTSTATUS WINAPI HidP_GetUsageValueArray( HIDP_REPORT_TYPE report_type, USAGE usage_page, USHORT collection,
                                         USAGE usage, char *value_buf, USHORT value_len,
                                         PHIDP_PREPARSED_DATA preparsed_data, char *report_buf, ULONG report_len )
{
    struct usage_value_params params = {.value_buf = value_buf, .value_len = value_len, .report_buf = report_buf};
    struct hid_preparsed_data *preparsed = (struct hid_preparsed_data *)preparsed_data;
    struct caps_filter filter = {.values = TRUE, .array = TRUE, .usage_page = usage_page, .collection = collection, .usage = usage};
    USHORT count = 1;

    TRACE( "report_type %d, usage_page %x, collection %d, usage %x, value_buf %p, value_len %u, "
           "preparsed_data %p, report_buf %p, report_len %u.\n",
           report_type, usage_page, collection, usage, value_buf, value_len, preparsed_data, report_buf, report_len );

    if (!report_len) return HIDP_STATUS_INVALID_REPORT_LENGTH;

    filter.report_id = report_buf[0];
    return enum_value_caps( preparsed, report_type, report_len, &filter, get_usage_value, &params, &count );
}


struct get_usage_params
{
    USAGE *usages;
    USAGE *usages_end;
    char *report_buf;
};

static NTSTATUS get_usage( const struct hid_value_caps *caps, void *user )
{
    struct get_usage_params *params = user;
    ULONG bit, last;
    BYTE index;

    if (HID_VALUE_CAPS_IS_ARRAY( caps ))
    {
        for (bit = caps->start_bit, last = bit + caps->report_count * caps->bit_size - 1; bit <= last; bit += 8)
        {
            if (!(index = params->report_buf[bit / 8])) continue;
            if (params->usages < params->usages_end) *params->usages = caps->usage_min + index - caps->start_index;
            params->usages++;
        }
        return HIDP_STATUS_SUCCESS;
    }

    for (bit = caps->start_bit, last = bit + caps->usage_max - caps->usage_min; bit <= last; ++bit)
    {
        if (!(params->report_buf[bit / 8] & (1 << (bit % 8)))) continue;
        if (params->usages < params->usages_end) *params->usages = caps->usage_min + bit - caps->start_bit;
        params->usages++;
    }

    return HIDP_STATUS_SUCCESS;
}

NTSTATUS WINAPI HidP_GetUsages( HIDP_REPORT_TYPE report_type, USAGE usage_page, USHORT collection, USAGE *usages,
                                ULONG *usages_len, PHIDP_PREPARSED_DATA preparsed_data, char *report_buf, ULONG report_len )
{
    struct hid_preparsed_data *preparsed = (struct hid_preparsed_data *)preparsed_data;
    struct get_usage_params params = {.usages = usages, .usages_end = usages + *usages_len, .report_buf = report_buf};
    struct caps_filter filter = {.buttons = TRUE, .usage_page = usage_page, .collection = collection};
    NTSTATUS status;
    USHORT limit = -1;

    TRACE( "report_type %d, collection %d, usages %p, usages_len %p, preparsed_data %p, report_buf %p, report_len %u.\n",
           report_type, collection, usages, usages_len, preparsed_data, report_buf, report_len );

    if (!report_len) return HIDP_STATUS_INVALID_REPORT_LENGTH;

    filter.report_id = report_buf[0];
    status = enum_value_caps( preparsed, report_type, report_len, &filter, get_usage, &params, &limit );
    *usages_len = params.usages - usages;
    if (status != HIDP_STATUS_SUCCESS) return status;

    if (params.usages > params.usages_end) return HIDP_STATUS_BUFFER_TOO_SMALL;
    return status;
}

NTSTATUS WINAPI HidP_GetValueCaps( HIDP_REPORT_TYPE report_type, HIDP_VALUE_CAPS *caps, USHORT *caps_count,
                                   PHIDP_PREPARSED_DATA preparsed_data )
{
    return HidP_GetSpecificValueCaps( report_type, 0, 0, 0, caps, caps_count, preparsed_data );
}

NTSTATUS WINAPI HidP_InitializeReportForID( HIDP_REPORT_TYPE report_type, UCHAR report_id,
                                            PHIDP_PREPARSED_DATA preparsed_data, char *report_buf, ULONG report_len )
{
    struct hid_preparsed_data *preparsed = (struct hid_preparsed_data *)preparsed_data;
    const struct hid_value_caps *caps, *end;
    NTSTATUS status;

    TRACE( "report_type %d, report_id %x, preparsed_data %p, report_buf %p, report_len %u.\n", report_type,
           report_id, preparsed_data, report_buf, report_len );

    if (!report_len) return HIDP_STATUS_INVALID_REPORT_LENGTH;

    status = get_value_caps_range( preparsed, report_type, report_len, &caps, &end );
    if (status != HIDP_STATUS_SUCCESS) return status;

    while (caps != end && (caps->report_id != report_id || (!caps->usage_min && !caps->usage_max))) caps++;
    if (caps == end) return HIDP_STATUS_REPORT_DOES_NOT_EXIST;

    memset( report_buf, 0, report_len );
    report_buf[0] = report_id;
    return HIDP_STATUS_SUCCESS;
}

static NTSTATUS get_usage_list_length( const struct hid_value_caps *caps, void *data )
{
    *(ULONG *)data += caps->report_count;
    return HIDP_STATUS_SUCCESS;
}

ULONG WINAPI HidP_MaxUsageListLength( HIDP_REPORT_TYPE report_type, USAGE usage_page, PHIDP_PREPARSED_DATA preparsed_data )
{
    struct hid_preparsed_data *preparsed = (struct hid_preparsed_data *)preparsed_data;
    struct caps_filter filter = {.buttons = TRUE, .usage_page = usage_page};
    USHORT limit = -1;
    ULONG count = 0;

    TRACE( "report_type %d, usage_page %x, preparsed_data %p.\n", report_type, usage_page, preparsed_data );

    enum_value_caps( preparsed, report_type, 0, &filter, get_usage_list_length, &count, &limit );
    return count;
}

static NTSTATUS set_usage_value( const struct hid_value_caps *caps, void *user )
{
    struct usage_value_params *params = user;
    ULONG bit_count = caps->bit_size * caps->report_count;
    if ((bit_count + 7) / 8 > params->value_len) return HIDP_STATUS_BUFFER_TOO_SMALL;
    copy_bits( params->report_buf, params->value_buf, bit_count, caps->start_bit );
    return HIDP_STATUS_NULL;
}

NTSTATUS WINAPI HidP_SetUsageValue( HIDP_REPORT_TYPE report_type, USAGE usage_page, USHORT collection, USAGE usage,
                                    ULONG value, PHIDP_PREPARSED_DATA preparsed_data, char *report_buf, ULONG report_len )
{
    struct usage_value_params params = {.value_buf = &value, .value_len = sizeof(value), .report_buf = report_buf};
    struct hid_preparsed_data *preparsed = (struct hid_preparsed_data *)preparsed_data;
    struct caps_filter filter = {.values = TRUE, .usage_page = usage_page, .collection = collection, .usage = usage};
    USHORT count = 1;

    TRACE( "report_type %d, usage_page %x, collection %d, usage %x, value %u, preparsed_data %p, report_buf %p, report_len %u.\n",
           report_type, usage_page, collection, usage, value, preparsed_data, report_buf, report_len );

    if (!report_len) return HIDP_STATUS_INVALID_REPORT_LENGTH;

    filter.report_id = report_buf[0];
    return enum_value_caps( preparsed, report_type, report_len, &filter, set_usage_value, &params, &count );
}

NTSTATUS WINAPI HidP_SetUsageValueArray( HIDP_REPORT_TYPE report_type, USAGE usage_page, USHORT collection,
                                         USAGE usage, char *value_buf, USHORT value_len,
                                         PHIDP_PREPARSED_DATA preparsed_data, char *report_buf, ULONG report_len )
{
    struct usage_value_params params = {.value_buf = value_buf, .value_len = value_len, .report_buf = report_buf};
    struct hid_preparsed_data *preparsed = (struct hid_preparsed_data *)preparsed_data;
    struct caps_filter filter = {.values = TRUE, .array = TRUE, .usage_page = usage_page, .collection = collection, .usage = usage};
    USHORT count = 1;

    TRACE( "report_type %d, usage_page %x, collection %d, usage %x, value_buf %p, value_len %u, "
           "preparsed_data %p, report_buf %p, report_len %u.\n",
           report_type, usage_page, collection, usage, value_buf, value_len, preparsed_data, report_buf, report_len );

    if (!report_len) return HIDP_STATUS_INVALID_REPORT_LENGTH;

    filter.report_id = report_buf[0];
    return enum_value_caps( preparsed, report_type, report_len, &filter, set_usage_value, &params, &count );
}

struct set_usage_params
{
    USAGE usage;
    char *report_buf;
};

static NTSTATUS set_usage( const struct hid_value_caps *caps, void *user )
{
    struct set_usage_params *params = user;
    ULONG bit, last;

    if (HID_VALUE_CAPS_IS_ARRAY( caps ))
    {
        for (bit = caps->start_bit, last = bit + caps->report_count * caps->bit_size - 1; bit <= last; bit += 8)
        {
            if (params->report_buf[bit / 8]) continue;
            params->report_buf[bit / 8] = caps->start_index + params->usage - caps->usage_min;
            break;
        }

        if (bit > last) return HIDP_STATUS_BUFFER_TOO_SMALL;
        return HIDP_STATUS_NULL;
    }

    bit = caps->start_bit + params->usage - caps->usage_min;
    params->report_buf[bit / 8] |= (1 << (bit % 8));
    return HIDP_STATUS_NULL;
}

NTSTATUS WINAPI HidP_SetUsages( HIDP_REPORT_TYPE report_type, USAGE usage_page, USHORT collection, USAGE *usages,
                                ULONG *usage_count, PHIDP_PREPARSED_DATA preparsed_data, char *report_buf, ULONG report_len )
{
    struct hid_preparsed_data *preparsed = (struct hid_preparsed_data *)preparsed_data;
    struct set_usage_params params = {.report_buf = report_buf};
    struct caps_filter filter = {.buttons = TRUE, .usage_page = usage_page, .collection = collection};
    NTSTATUS status;
    USHORT limit = 1;
    ULONG i, count = *usage_count;

    TRACE( "report_type %d, usage_page %x, collection %d, usages %p, usage_count %p, preparsed_data %p, "
           "report_buf %p, report_len %u.\n",
           report_type, usage_page, collection, usages, usage_count, preparsed_data, report_buf, report_len );

    if (!report_len) return HIDP_STATUS_INVALID_REPORT_LENGTH;

    filter.report_id = report_buf[0];
    for (i = 0; i < count; ++i)
    {
        params.usage = filter.usage = usages[i];
        status = enum_value_caps( preparsed, report_type, report_len, &filter, set_usage, &params, &limit );
        if (status != HIDP_STATUS_SUCCESS) return status;
    }

    return HIDP_STATUS_SUCCESS;
}


NTSTATUS WINAPI HidP_TranslateUsagesToI8042ScanCodes(USAGE *ChangedUsageList,
    ULONG UsageListLength, HIDP_KEYBOARD_DIRECTION KeyAction,
    HIDP_KEYBOARD_MODIFIER_STATE *ModifierState,
    PHIDP_INSERT_SCANCODES InsertCodesProcedure, VOID *InsertCodesContext)
{
    FIXME("stub: %p, %i, %i, %p, %p, %p\n", ChangedUsageList, UsageListLength,
        KeyAction, ModifierState, InsertCodesProcedure, InsertCodesContext);

    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS get_button_caps( const struct hid_value_caps *caps, void *user )
{
    HIDP_BUTTON_CAPS **iter = user, *dst = *iter;
    dst->UsagePage = caps->usage_page;
    dst->ReportID = caps->report_id;
    dst->LinkCollection = caps->link_collection;
    dst->LinkUsagePage = caps->link_usage_page;
    dst->LinkUsage = caps->link_usage;
    dst->BitField = caps->bit_field;
    dst->IsAlias = FALSE;
    dst->IsAbsolute = HID_VALUE_CAPS_IS_ABSOLUTE( caps );
    if (!(dst->IsRange = caps->is_range))
    {
        dst->NotRange.Usage = caps->usage_min;
        dst->NotRange.DataIndex = caps->data_index_min;
    }
    else
    {
        dst->Range.UsageMin = caps->usage_min;
        dst->Range.UsageMax = caps->usage_max;
        dst->Range.DataIndexMin = caps->data_index_min;
        dst->Range.DataIndexMax = caps->data_index_max;
    }
    if (!(dst->IsStringRange = caps->is_string_range))
        dst->NotRange.StringIndex = caps->string_min;
    else
    {
        dst->Range.StringMin = caps->string_min;
        dst->Range.StringMax = caps->string_max;
    }
    if ((dst->IsDesignatorRange = caps->is_designator_range))
        dst->NotRange.DesignatorIndex = caps->designator_min;
    else
    {
        dst->Range.DesignatorMin = caps->designator_min;
        dst->Range.DesignatorMax = caps->designator_max;
    }
    *iter += 1;
    return HIDP_STATUS_SUCCESS;
}

NTSTATUS WINAPI HidP_GetSpecificButtonCaps( HIDP_REPORT_TYPE report_type, USAGE usage_page, USHORT collection,
                                            USAGE usage, HIDP_BUTTON_CAPS *caps, USHORT *caps_count,
                                            PHIDP_PREPARSED_DATA preparsed_data )
{
    struct hid_preparsed_data *preparsed = (struct hid_preparsed_data *)preparsed_data;
    const struct caps_filter filter = {.buttons = TRUE, .usage_page = usage_page, .collection = collection, .usage = usage};

    TRACE( "report_type %d, usage_page %x, collection %d, usage %x, caps %p, caps_count %p, preparsed_data %p.\n",
           report_type, usage_page, collection, usage, caps, caps_count, preparsed_data );

    return enum_value_caps( preparsed, report_type, 0, &filter, get_button_caps, &caps, caps_count );
}

static NTSTATUS get_value_caps( const struct hid_value_caps *caps, void *user )
{
    HIDP_VALUE_CAPS **iter = user, *dst = *iter;
    dst->UsagePage = caps->usage_page;
    dst->ReportID = caps->report_id;
    dst->LinkCollection = caps->link_collection;
    dst->LinkUsagePage = caps->link_usage_page;
    dst->LinkUsage = caps->link_usage;
    dst->BitField = caps->bit_field;
    dst->IsAlias = FALSE;
    dst->IsAbsolute = HID_VALUE_CAPS_IS_ABSOLUTE( caps );
    dst->HasNull = HID_VALUE_CAPS_HAS_NULL( caps );
    dst->BitSize = caps->bit_size;
    dst->ReportCount = caps->is_range ? 1 : caps->report_count;
    dst->UnitsExp = caps->units_exp;
    dst->Units = caps->units;
    dst->LogicalMin = caps->logical_min;
    dst->LogicalMax = caps->logical_max;
    dst->PhysicalMin = caps->physical_min;
    dst->PhysicalMax = caps->physical_max;
    if (!(dst->IsRange = caps->is_range))
    {
        dst->NotRange.Usage = caps->usage_min;
        dst->NotRange.DataIndex = caps->data_index_min;
    }
    else
    {
        dst->Range.UsageMin = caps->usage_min;
        dst->Range.UsageMax = caps->usage_max;
        dst->Range.DataIndexMin = caps->data_index_min;
        dst->Range.DataIndexMax = caps->data_index_max;
    }
    if (!(dst->IsStringRange = caps->is_string_range))
        dst->NotRange.StringIndex = caps->string_min;
    else
    {
        dst->Range.StringMin = caps->string_min;
        dst->Range.StringMax = caps->string_max;
    }
    if ((dst->IsDesignatorRange = caps->is_designator_range))
        dst->NotRange.DesignatorIndex = caps->designator_min;
    else
    {
        dst->Range.DesignatorMin = caps->designator_min;
        dst->Range.DesignatorMax = caps->designator_max;
    }
    *iter += 1;
    return HIDP_STATUS_SUCCESS;
}

NTSTATUS WINAPI HidP_GetSpecificValueCaps( HIDP_REPORT_TYPE report_type, USAGE usage_page, USHORT collection,
                                           USAGE usage, HIDP_VALUE_CAPS *caps, USHORT *caps_count,
                                           PHIDP_PREPARSED_DATA preparsed_data )
{
    struct hid_preparsed_data *preparsed = (struct hid_preparsed_data *)preparsed_data;
    const struct caps_filter filter = {.values = TRUE, .usage_page = usage_page, .collection = collection, .usage = usage};

    TRACE( "report_type %d, usage_page %x, collection %d, usage %x, caps %p, caps_count %p, preparsed_data %p.\n",
           report_type, usage_page, collection, usage, caps, caps_count, preparsed_data );

    return enum_value_caps( preparsed, report_type, 0, &filter, get_value_caps, &caps, caps_count );
}

struct get_usage_and_page_params
{
    USAGE_AND_PAGE *usages;
    USAGE_AND_PAGE *usages_end;
    char *report_buf;
};

static NTSTATUS get_usage_and_page( const struct hid_value_caps *caps, void *user )
{
    struct get_usage_and_page_params *params = user;
    ULONG bit, last;
    BYTE index;

    if (HID_VALUE_CAPS_IS_ARRAY( caps ))
    {
        for (bit = caps->start_bit, last = bit + caps->report_count * caps->bit_size - 1; bit <= last; bit += 8)
        {
            if (!(index = params->report_buf[bit / 8])) continue;
            if (params->usages < params->usages_end)
            {
                params->usages->UsagePage = caps->usage_page;
                params->usages->Usage = caps->usage_min + index - caps->start_index;
            }
            params->usages++;
        }
        return HIDP_STATUS_SUCCESS;
    }

    for (bit = caps->start_bit, last = bit + caps->usage_max - caps->usage_min; bit <= last; bit++)
    {
        if (!(params->report_buf[bit / 8] & (1 << (bit % 8)))) continue;
        if (params->usages < params->usages_end)
        {
            params->usages->UsagePage = caps->usage_page;
            params->usages->Usage = caps->usage_min + bit - caps->start_bit;
        }
        params->usages++;
    }

    return HIDP_STATUS_SUCCESS;
}

NTSTATUS WINAPI HidP_GetUsagesEx( HIDP_REPORT_TYPE report_type, USHORT collection, USAGE_AND_PAGE *usages,
                                  ULONG *usages_len, PHIDP_PREPARSED_DATA preparsed_data, char *report_buf, ULONG report_len )
{
    struct get_usage_and_page_params params = {.usages = usages, .usages_end = usages + *usages_len, .report_buf = report_buf};
    struct hid_preparsed_data *preparsed = (struct hid_preparsed_data *)preparsed_data;
    struct caps_filter filter = {.buttons = TRUE, .collection = collection};
    NTSTATUS status;
    USHORT limit = -1;

    TRACE( "report_type %d, collection %d, usages %p, usages_len %p, preparsed_data %p, report_buf %p, report_len %u.\n",
           report_type, collection, usages, usages_len, preparsed_data, report_buf, report_len );

    if (!report_len) return HIDP_STATUS_INVALID_REPORT_LENGTH;

    filter.report_id = report_buf[0];
    status = enum_value_caps( preparsed, report_type, report_len, &filter, get_usage_and_page, &params, &limit );
    *usages_len = params.usages - usages;
    if (status != HIDP_STATUS_SUCCESS) return status;

    if (params.usages > params.usages_end) return HIDP_STATUS_BUFFER_TOO_SMALL;
    return status;
}

static NTSTATUS count_data( const struct hid_value_caps *caps, void *user )
{
    if (caps->is_range || HID_VALUE_CAPS_IS_BUTTON( caps )) *(ULONG *)user += caps->report_count;
    else *(ULONG *)user += 1;
    return HIDP_STATUS_SUCCESS;
}

ULONG WINAPI HidP_MaxDataListLength( HIDP_REPORT_TYPE report_type, PHIDP_PREPARSED_DATA preparsed_data )
{
    struct hid_preparsed_data *preparsed = (struct hid_preparsed_data *)preparsed_data;
    struct caps_filter filter = {};
    USHORT limit = -1;
    ULONG count = 0;

    TRACE( "report_type %d, preparsed_data %p.\n", report_type, preparsed_data );

    enum_value_caps( preparsed, report_type, 0, &filter, count_data, &count, &limit );
    return count;
}

struct find_all_data_params
{
    HIDP_DATA *data;
    HIDP_DATA *data_end;
    char *report_buf;
};

static NTSTATUS find_all_data( const struct hid_value_caps *caps, void *user )
{
    struct find_all_data_params *params = user;
    HIDP_DATA *data = params->data, *data_end = params->data_end;
    ULONG bit, last, bit_count = caps->bit_size * caps->report_count;
    char *report_buf = params->report_buf;
    BYTE index;

    if (!caps->bit_size) return HIDP_STATUS_SUCCESS;

    if (HID_VALUE_CAPS_IS_ARRAY( caps ))
    {
        for (bit = caps->start_bit, last = bit + caps->report_count * caps->bit_size - 1; bit <= last; bit += 8)
        {
            if (!(index = report_buf[bit / 8])) continue;
            if (data < data_end)
            {
                data->DataIndex = caps->data_index_min + index - caps->start_index;
                data->On = 1;
            }
            data++;
        }
    }
    else if (HID_VALUE_CAPS_IS_BUTTON( caps ))
    {
        for (bit = caps->start_bit, last = bit + caps->usage_max - caps->usage_min; bit <= last; bit++)
        {
            if (!(report_buf[bit / 8] & (1 << (bit % 8)))) continue;
            if (data < data_end)
            {
                data->DataIndex = caps->data_index_min + bit - caps->start_bit;
                data->On = 1;
            }
            data++;
        }
    }
    else if (caps->report_count == 1)
    {
        if (data < data_end)
        {
            data->DataIndex = caps->data_index_min;
            data->RawValue = 0;
            if ((bit_count + 7) / 8 > sizeof(data->RawValue)) return HIDP_STATUS_BUFFER_TOO_SMALL;
            copy_bits( (void *)&data->RawValue, (void *)report_buf, bit_count, -caps->start_bit );
        }
        data++;
    }

    params->data = data;
    return HIDP_STATUS_SUCCESS;
}

NTSTATUS WINAPI HidP_GetData( HIDP_REPORT_TYPE report_type, HIDP_DATA *data, ULONG *data_len,
                              PHIDP_PREPARSED_DATA preparsed_data, char *report_buf, ULONG report_len )
{
    struct find_all_data_params params = {.data = data, .data_end = data + *data_len, .report_buf = report_buf};
    struct hid_preparsed_data *preparsed = (struct hid_preparsed_data *)preparsed_data;
    struct caps_filter filter = {};
    NTSTATUS status;
    USHORT limit = -1;

    TRACE( "report_type %d, data %p, data_len %p, preparsed_data %p, report_buf %p, report_len %u.\n",
           report_type, data, data_len, preparsed_data, report_buf, report_len );

    if (!report_len) return HIDP_STATUS_INVALID_REPORT_LENGTH;

    filter.report_id = report_buf[0];
    status = enum_value_caps( preparsed, report_type, report_len, &filter, find_all_data, &params, &limit );
    *data_len = params.data - data;
    if (status != HIDP_STATUS_SUCCESS) return status;

    if (params.data > params.data_end) return HIDP_STATUS_BUFFER_TOO_SMALL;
    return HIDP_STATUS_SUCCESS;
}

NTSTATUS WINAPI HidP_GetLinkCollectionNodes( HIDP_LINK_COLLECTION_NODE *nodes, ULONG *nodes_len, PHIDP_PREPARSED_DATA preparsed_data )
{
    struct hid_preparsed_data *preparsed = (struct hid_preparsed_data *)preparsed_data;
    struct hid_value_caps *caps = HID_COLLECTION_VALUE_CAPS( preparsed );
    ULONG i, count, capacity = *nodes_len;

    TRACE( "nodes %p, nodes_len %p, preparsed_data %p.\n", nodes, nodes_len, preparsed_data );

    if (!preparsed || preparsed->magic != HID_MAGIC) return HIDP_STATUS_INVALID_PREPARSED_DATA;

    count = *nodes_len = preparsed->caps.NumberLinkCollectionNodes;
    if (capacity < count) return HIDP_STATUS_BUFFER_TOO_SMALL;

    for (i = 0; i < count; ++i)
    {
        nodes[i].LinkUsagePage = caps[i].usage_page;
        nodes[i].LinkUsage = caps[i].usage_min;
        nodes[i].Parent = caps[i].link_collection;
        nodes[i].CollectionType = caps[i].bit_field;
        nodes[i].IsAlias = 0;
        nodes[i].FirstChild = 0;
        nodes[i].NextSibling = 0;
        nodes[i].NumberOfChildren = 0;

        if (i > 0)
        {
            nodes[i].NextSibling = nodes[nodes[i].Parent].FirstChild;
            nodes[nodes[i].Parent].FirstChild = i;
            nodes[nodes[i].Parent].NumberOfChildren++;
        }
    }

    return HIDP_STATUS_SUCCESS;
}
