/*  DirectInput HID Joystick device
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

#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>

#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "winuser.h"
#include "winerror.h"
#include "winreg.h"

#include "ddk/hidsdi.h"
#include "setupapi.h"
#include "devguid.h"
#include "dinput.h"

#include "wine/debug.h"

#include "dinput_private.h"
#include "device_private.h"
#include "joystick_private.h"

#include "initguid.h"
#include "devpkey.h"

WINE_DEFAULT_DEBUG_CHANNEL(dinput);

DEFINE_GUID( hid_joystick_guid, 0x9e573edb, 0x7734, 0x11d2, 0x8d, 0x4a, 0x23, 0x90, 0x3f, 0xb6, 0xbd, 0xf7 );
DEFINE_DEVPROPKEY( DEVPROPKEY_HID_HANDLE, 0xbc62e415, 0xf4fe, 0x405c, 0x8e, 0xda, 0x63, 0x6f, 0xb5, 0x9f, 0x08, 0x98, 2 );

#define ALIGN_SIZE( size, alignment ) (((size) + (alignment - 1)) & ~((alignment - 1)))
#define ALIGN_PTR( ptr, alignment ) ((void *)ALIGN_SIZE( (UINT_PTR)(ptr), alignment ))

static inline const char *debugstr_hidp_link_collection_node( HIDP_LINK_COLLECTION_NODE *node )
{
    return wine_dbg_sprintf( "Usage %04x:%04x, Parent %d, NbChild %d, Next %d, First %d, Type %x, Alias %d, User %p",
                             node->LinkUsagePage, node->LinkUsage, node->Parent, node->NumberOfChildren,
                             node->NextSibling, node->FirstChild, node->CollectionType, node->IsAlias, node->UserContext );
}

static inline const char *debugstr_hidp_button_caps( HIDP_BUTTON_CAPS *caps )
{
    const char *str;

    if (!caps->IsRange)
        str = wine_dbg_sprintf( "Usage %04x:%04x, Idx %02x,", caps->UsagePage, caps->NotRange.Usage,
                                caps->NotRange.DataIndex );
    else
        str = wine_dbg_sprintf( "Usage %04x:%04x-%04x, Idx %02x-%02x,", caps->UsagePage, caps->Range.UsageMin,
                                caps->Range.UsageMax, caps->Range.DataIndexMin, caps->Range.DataIndexMax );

    if (!caps->IsStringRange)
        str = wine_dbg_sprintf( "%s Str %d,", str, caps->NotRange.StringIndex );
    else
        str = wine_dbg_sprintf( "%s Str %d-%d,", str, caps->Range.StringMin, caps->Range.StringMax );

    if (!caps->IsDesignatorRange)
        str = wine_dbg_sprintf( "%s DIdx %d,", str, caps->NotRange.DesignatorIndex );
    else
        str = wine_dbg_sprintf( "%s DIdx %d-%d,", str, caps->Range.DesignatorMin, caps->Range.DesignatorMax );

    return wine_dbg_sprintf( "%s RId %2x, Alias %d, Bit %d, LnkCol %d, LnkUsg %04x:%04x, Abs %d", str,
                             caps->ReportID, caps->IsAlias, caps->BitField, caps->LinkCollection,
                             caps->LinkUsagePage, caps->LinkUsage, caps->IsAbsolute );
}

static inline const char *debugstr_hidp_value_caps( HIDP_VALUE_CAPS *caps )
{
    const char *str;
    if (!caps->IsRange)
        str = wine_dbg_sprintf( "Usage %04x:%04x, Idx %02x,", caps->UsagePage, caps->NotRange.Usage,
                                caps->NotRange.DataIndex );
    else
        str = wine_dbg_sprintf( "Usage %04x:%04x-%04x, Idx %02x-%02x,", caps->UsagePage, caps->Range.UsageMin,
                                caps->Range.UsageMax, caps->Range.DataIndexMin, caps->Range.DataIndexMax );

    if (!caps->IsStringRange)
        str = wine_dbg_sprintf( "%s Str %d,", str, caps->NotRange.StringIndex );
    else
        str = wine_dbg_sprintf( "%s Str %d-%d,", str, caps->Range.StringMin, caps->Range.StringMax );

    if (!caps->IsDesignatorRange)
        str = wine_dbg_sprintf( "%s DIdx %d,", str, caps->NotRange.DesignatorIndex );
    else
        str = wine_dbg_sprintf( "%s DIdx %d-%d,", str, caps->Range.DesignatorMin, caps->Range.DesignatorMax );

    return wine_dbg_sprintf( "%s RId %2x, Alias %d, Bit %d, LnkCol %d, LnkUsg %04x:%04x, Abs %d, Null %d, "
                             "BitSz %d, RCnt %3d, UnitEx %x, Unit %02x, Log %02x-%02x, Phy %03x-%03x",
                             str, caps->ReportID, caps->IsAlias, caps->BitField, caps->LinkCollection,
                             caps->LinkUsagePage, caps->LinkUsage, caps->IsAbsolute, caps->HasNull,
                             caps->BitSize, caps->ReportCount, caps->UnitsExp, caps->Units,
                             caps->LogicalMin, caps->LogicalMax, caps->PhysicalMin, caps->PhysicalMax );
}

struct hid_object
{
    enum { LINK_COLLECTION_NODE, BUTTON_CAPS, VALUE_CAPS } type;
    DWORD index;
    union
    {
        HIDP_LINK_COLLECTION_NODE *node;
        HIDP_BUTTON_CAPS *button;
        HIDP_VALUE_CAPS *value;
    };
};

struct hid_extra_value_caps
{
    LONG deadzone;
    LONG saturation;
};

struct hid_joystick
{
    IDirectInputDeviceImpl base;
    DIJOYSTATE2 state;

    HANDLE device;
    OVERLAPPED read_ovl;
    PHIDP_PREPARSED_DATA preparsed;

    DIDEVICEINSTANCEW instance;
    WCHAR device_path[MAX_PATH];
    HIDD_ATTRIBUTES attrs;
    DIDEVCAPS dev_caps;
    HIDP_CAPS caps;
    BYTE report_id;

    HIDP_LINK_COLLECTION_NODE *collection_nodes;

    HIDP_BUTTON_CAPS *input_button_caps;
    HIDP_BUTTON_CAPS *output_button_caps;
    HIDP_BUTTON_CAPS *feature_button_caps;

    HIDP_VALUE_CAPS *input_value_caps;
    HIDP_VALUE_CAPS *output_value_caps;
    HIDP_VALUE_CAPS *feature_value_caps;

    USAGE_AND_PAGE *usages_buf;
    ULONG usages_count;

    char *input_report_buf;
    char *output_report_buf;
    char *feature_report_buf;

    struct hid_extra_value_caps *extra_value_caps;
};

static inline struct hid_joystick *impl_from_IDirectInputDevice8W( IDirectInputDevice8W *iface )
{
    return CONTAINING_RECORD( CONTAINING_RECORD( iface, IDirectInputDeviceImpl, IDirectInputDevice8W_iface ),
                              struct hid_joystick, base );
}

typedef BOOL (*enum_hid_objects_callback)( struct hid_joystick *impl, struct hid_object *object, DIDEVICEOBJECTINSTANCEW *instance, void *data );

static BOOL enum_hid_objects_if( struct hid_joystick *impl, const DIPROPHEADER *header, DWORD flags,
                                 enum_hid_objects_callback callback, struct hid_object *object,
                                 DIDEVICEOBJECTINSTANCEW *instance, void *data )
{
    if (flags != DIDFT_ALL && !(flags & DIDFT_GETTYPE( instance->dwType ))) return TRUE;

    switch (header->dwHow)
    {
    case DIPH_DEVICE:
        return callback( impl, object, instance, data );
    case DIPH_BYOFFSET:
        if (header->dwObj != instance->dwOfs) return TRUE;
        return callback( impl, object, instance, data );
    case DIPH_BYID:
        if ((header->dwObj & 0x00ffffff) != (instance->dwType & 0x00ffffff)) return TRUE;
        return callback( impl, object, instance, data );
    case DIPH_BYUSAGE:
        if (LOWORD(header->dwObj) != instance->wUsagePage || HIWORD(header->dwObj) != instance->wUsage) return TRUE;
        return callback( impl, object, instance, data );
    default:
        FIXME( "Unimplemented header dwHow %x dwObj %x\n", header->dwHow, header->dwObj );
        break;
    }

    return TRUE;
}

static void enum_hid_objects( struct hid_joystick *impl, const DIPROPHEADER *header, DWORD flags,
                              enum_hid_objects_callback callback, void *data, BOOL verbose )
{
    DIDEVICEOBJECTINSTANCEW instance = {sizeof(DIDEVICEOBJECTINSTANCEW)};
    struct hid_object object = {0};
    DWORD axis = 0, pov = 0, button = 0, i, j;

    for (i = 0; i < impl->caps.NumberInputValueCaps; ++i)
    {
        object.type = VALUE_CAPS;
        object.value = impl->input_value_caps + i;

        if (object.value->IsAlias)
            TRACE( "Ignoring input value %s, aliased.\n", debugstr_hidp_value_caps( object.value ) );
        else if (object.value->UsagePage >= HID_USAGE_PAGE_VENDOR_DEFINED_BEGIN)
            TRACE( "Ignoring input value %s, vendor specific.\n", debugstr_hidp_value_caps( object.value ) );
        else if (object.value->UsagePage == HID_USAGE_PAGE_HAPTICS)
            TRACE( "Ignoring input value %s, haptics page.\n", debugstr_hidp_value_caps( object.value ) );
        else if (object.value->UsagePage == HID_USAGE_PAGE_PID)
            TRACE( "Ignoring input value %s, haptics page.\n", debugstr_hidp_value_caps( object.value ) );
        else if (object.value->UsagePage != HID_USAGE_PAGE_GENERIC)
            FIXME( "Ignoring input value %s, usage page not implemented.\n", debugstr_hidp_value_caps( object.value ) );
        else if (object.value->IsRange)
            FIXME( "Ignoring input value %s, usage range not implemented.\n", debugstr_hidp_value_caps( object.value ) );
        else if (object.value->ReportCount > 1)
            FIXME( "Ignoring input value %s, array not implemented.\n", debugstr_hidp_value_caps( object.value ) );
        else
        {
            if (verbose) TRACE( "Found input value %s.\n", debugstr_hidp_value_caps( object.value ) );
            switch (object.value->NotRange.Usage)
            {
            case HID_USAGE_GENERIC_X:
                instance.guidType = GUID_XAxis;
                instance.dwOfs = DIJOFS_X;
                instance.dwType = DIDFT_ABSAXIS | DIDFT_MAKEINSTANCE( axis++ );
                instance.dwFlags = DIDOI_ASPECTPOSITION;
                instance.wUsagePage = object.value->UsagePage;
                instance.wUsage = object.value->NotRange.Usage;
                instance.wReportId = object.value->ReportID;
                if (!enum_hid_objects_if( impl, header, flags, callback, &object, &instance, data )) return;
                object.index++;
                break;
            case HID_USAGE_GENERIC_Y:
                instance.guidType = GUID_YAxis;
                instance.dwOfs = DIJOFS_Y;
                instance.dwType = DIDFT_ABSAXIS | DIDFT_MAKEINSTANCE( axis++ );
                instance.dwFlags = DIDOI_ASPECTPOSITION;
                instance.wUsagePage = object.value->UsagePage;
                instance.wUsage = object.value->NotRange.Usage;
                instance.wReportId = object.value->ReportID;
                if (!enum_hid_objects_if( impl, header, flags, callback, &object, &instance, data )) return;
                object.index++;
                break;
            case HID_USAGE_GENERIC_Z:
                instance.guidType = GUID_ZAxis;
                instance.dwOfs = DIJOFS_Z;
                instance.dwType = DIDFT_ABSAXIS | DIDFT_MAKEINSTANCE( axis++ );
                instance.dwFlags = DIDOI_ASPECTPOSITION;
                instance.wUsagePage = object.value->UsagePage;
                instance.wUsage = object.value->NotRange.Usage;
                instance.wReportId = object.value->ReportID;
                if (!enum_hid_objects_if( impl, header, flags, callback, &object, &instance, data )) return;
                object.index++;
                break;
            case HID_USAGE_GENERIC_RX:
                instance.guidType = GUID_RxAxis;
                instance.dwOfs = DIJOFS_RX;
                instance.dwType = DIDFT_ABSAXIS | DIDFT_MAKEINSTANCE( axis++ );
                instance.dwFlags = DIDOI_ASPECTPOSITION;
                instance.wUsagePage = object.value->UsagePage;
                instance.wUsage = object.value->NotRange.Usage;
                instance.wReportId = object.value->ReportID;
                if (!enum_hid_objects_if( impl, header, flags, callback, &object, &instance, data )) return;
                object.index++;
                break;
            case HID_USAGE_GENERIC_RY:
                instance.guidType = GUID_RyAxis;
                instance.dwOfs = DIJOFS_RY;
                instance.dwType = DIDFT_ABSAXIS | DIDFT_MAKEINSTANCE( axis++ );
                instance.dwFlags = DIDOI_ASPECTPOSITION;
                instance.wUsagePage = object.value->UsagePage;
                instance.wUsage = object.value->NotRange.Usage;
                instance.wReportId = object.value->ReportID;
                if (!enum_hid_objects_if( impl, header, flags, callback, &object, &instance, data )) return;
                object.index++;
                break;
            case HID_USAGE_GENERIC_RZ:
                instance.guidType = GUID_RzAxis;
                instance.dwOfs = DIJOFS_RZ;
                instance.dwType = DIDFT_ABSAXIS | DIDFT_MAKEINSTANCE( axis++ );
                instance.dwFlags = DIDOI_ASPECTPOSITION;
                instance.wUsagePage = object.value->UsagePage;
                instance.wUsage = object.value->NotRange.Usage;
                instance.wReportId = object.value->ReportID;
                if (!enum_hid_objects_if( impl, header, flags, callback, &object, &instance, data )) return;
                object.index++;
                break;
            case HID_USAGE_GENERIC_SLIDER:
                instance.guidType = GUID_Slider;
                instance.dwOfs = DIJOFS_SLIDER( 0 );
                instance.dwType = DIDFT_ABSAXIS | DIDFT_MAKEINSTANCE( axis++ );
                instance.dwFlags = DIDOI_ASPECTPOSITION;
                instance.wUsagePage = object.value->UsagePage;
                instance.wUsage = object.value->NotRange.Usage;
                instance.wReportId = object.value->ReportID;
                if (!enum_hid_objects_if( impl, header, flags, callback, &object, &instance, data )) return;
                object.index++;
                break;
            case HID_USAGE_GENERIC_HATSWITCH:
                instance.guidType = GUID_POV;
                instance.dwOfs = DIJOFS_POV( 0 );
                instance.dwType = DIDFT_POV | DIDFT_MAKEINSTANCE( pov++ );
                instance.dwFlags = DIDOI_ASPECTPOSITION;
                instance.wUsagePage = object.value->UsagePage;
                instance.wUsage = object.value->NotRange.Usage;
                instance.wReportId = object.value->ReportID;
                if (!enum_hid_objects_if( impl, header, flags, callback, &object, &instance, data )) return;
                object.index++;
                break;
            default:
                FIXME( "Ignoring input value %s, usage not implemented.\n", debugstr_hidp_value_caps( object.value ) );
                break;
            }
        }
    }

    for (i = 0; i < impl->caps.NumberOutputValueCaps; ++i)
    {
        object.type = VALUE_CAPS;
        object.value = impl->output_value_caps + i;

        if (verbose) TRACE( "Found output value %s.\n", debugstr_hidp_value_caps( object.value ) );
    }

    for (i = 0; i < impl->caps.NumberFeatureValueCaps; ++i)
    {
        object.type = VALUE_CAPS;
        object.value = impl->feature_value_caps + i;

        if (verbose) TRACE( "Found feature value %s.\n", debugstr_hidp_value_caps( object.value ) );
    }

    for (i = 0; i < impl->caps.NumberInputButtonCaps; ++i)
    {
        object.type = BUTTON_CAPS;
        object.button = impl->input_button_caps + i;

        if (object.button->IsAlias)
            TRACE( "Ignoring input button %s, aliased.\n", debugstr_hidp_button_caps( object.button ) );
        else if (object.button->UsagePage >= HID_USAGE_PAGE_VENDOR_DEFINED_BEGIN)
            TRACE( "Ignoring input button %s, vendor specific.\n", debugstr_hidp_button_caps( object.button ) );
        else if (object.button->UsagePage == HID_USAGE_PAGE_HAPTICS)
            TRACE( "Ignoring input button %s, haptics page.\n", debugstr_hidp_button_caps( object.button ) );
        else if (object.button->UsagePage == HID_USAGE_PAGE_PID)
            TRACE( "Ignoring input button %s, haptics page.\n", debugstr_hidp_button_caps( object.button ) );
        else if (object.button->UsagePage != HID_USAGE_PAGE_BUTTON)
            FIXME( "Ignoring input button %s, usage page not implemented.\n", debugstr_hidp_button_caps( object.button ) );
        else if (object.button->IsRange)
        {
            if (object.button->Range.UsageMin <= 0 || object.button->Range.UsageMax >= 128)
                FIXME( "Ignoring input button %s, invalid usage.\n", debugstr_hidp_button_caps( object.button ) );
            else for (j = object.button->Range.UsageMin; j <= object.button->Range.UsageMax; ++j)
            {
                if (verbose) TRACE( "Found input button %s.\n", debugstr_hidp_button_caps( object.button ) );
                instance.guidType = GUID_Button;
                instance.dwOfs = DIJOFS_BUTTON( j );
                instance.dwType = DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE( button++ );
                instance.dwFlags = 0;
                instance.wUsagePage = object.button->UsagePage;
                instance.wUsage = j;
                instance.wReportId = object.button->ReportID;
                if (!enum_hid_objects_if( impl, header, flags, callback, &object, &instance, data )) return;
                object.index++;
            }
        }
        else if (object.button->NotRange.Usage <= 0 || object.button->NotRange.Usage >= 128)
            FIXME( "Ignoring input button %s, invalid usage.\n", debugstr_hidp_button_caps( object.button ) );
        else
        {
            if (verbose) TRACE( "Found input button %s.\n", debugstr_hidp_button_caps( object.button ) );
            instance.guidType = GUID_Button;
            instance.dwOfs = DIJOFS_BUTTON( object.button->NotRange.Usage );
            instance.dwType = DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE( button++ );
            instance.dwFlags = 0;
            instance.wUsagePage = object.button->UsagePage;
            instance.wUsage = object.button->NotRange.Usage;
            instance.wReportId = object.button->ReportID;
            if (!enum_hid_objects_if( impl, header, flags, callback, &object, &instance, data )) return;
            object.index++;
        }
    }

    for (i = 0; i < impl->caps.NumberOutputButtonCaps; ++i)
    {
        object.type = BUTTON_CAPS;
        object.button = impl->output_button_caps + i;

        if (verbose) TRACE( "Found output button %s.\n", debugstr_hidp_button_caps( object.button ) );
    }

    for (i = 0; i < impl->caps.NumberFeatureButtonCaps; ++i)
    {
        object.type = BUTTON_CAPS;
        object.button = impl->feature_button_caps + i;

        if (verbose) TRACE( "Found feature button %s.\n", debugstr_hidp_button_caps( object.button ) );
    }

    for (i = 0; i < impl->caps.NumberLinkCollectionNodes; ++i)
    {
        object.type = LINK_COLLECTION_NODE;
        object.node = impl->collection_nodes + i;

        if (object.node->IsAlias)
            TRACE( "Ignoring collection %s, aliased.\n", debugstr_hidp_link_collection_node( object.node ) );
        else if (object.node->LinkUsagePage >= HID_USAGE_PAGE_VENDOR_DEFINED_BEGIN)
            TRACE( "Ignoring collection %s, vendor specific.\n", debugstr_hidp_link_collection_node( object.node ) );
        else if (object.node->LinkUsagePage == HID_USAGE_PAGE_HAPTICS)
            TRACE( "Ignoring collection %s, haptics page.\n", debugstr_hidp_link_collection_node( object.node ) );
        else if (object.node->LinkUsagePage == HID_USAGE_PAGE_PID)
            TRACE( "Ignoring collection %s, physical page.\n", debugstr_hidp_link_collection_node( object.node ) );
        else if (object.node->LinkUsagePage != HID_USAGE_PAGE_GENERIC)
            FIXME( "Ignoring collection %s, link usage page not implemented.\n", debugstr_hidp_link_collection_node( object.node ) );
        else
        {
            if (verbose) TRACE( "Found collection %s.\n", debugstr_hidp_link_collection_node( object.node ) );
            instance.guidType = GUID_Unknown;
            instance.dwOfs = 0;
            instance.dwType = DIDFT_COLLECTION | DIDFT_NODATA;
            instance.dwFlags = 0;
            instance.wUsagePage = object.node->LinkUsagePage;
            instance.wUsage = object.node->LinkUsage;
            instance.wReportId = 0;
            if (!enum_hid_objects_if( impl, header, flags, callback, &object, &instance, data )) return;
            object.index++;
        }
    }
}

static ULONG WINAPI hid_joystick_Release( IDirectInputDevice8W *iface )
{
    struct hid_joystick *impl = impl_from_IDirectInputDevice8W( iface );
    struct hid_joystick tmp = *impl;
    ULONG res;

    if (!(res = IDirectInputDevice2WImpl_Release( iface )))
    {
        HidD_FreePreparsedData( tmp.preparsed );
        CancelIoEx( tmp.device, &tmp.read_ovl );
        CloseHandle( tmp.base.read_event );
        CloseHandle( tmp.device );
    }

    return res;
}

static HRESULT WINAPI hid_joystick_GetCapabilities( IDirectInputDevice8W *iface, DIDEVCAPS *caps )
{
    struct hid_joystick *impl = impl_from_IDirectInputDevice8W( iface );

    TRACE( "iface %p, caps %p.\n", iface, caps );

    if (!caps) return E_POINTER;

    *caps = impl->dev_caps;
    return DI_OK;
}

struct enum_objects_params
{
    LPDIENUMDEVICEOBJECTSCALLBACKW callback;
    void *ref;
};

static BOOL enum_objects_callback( struct hid_joystick *impl, struct hid_object *object,
                                   DIDEVICEOBJECTINSTANCEW *instance, void *data )
{
    struct enum_objects_params *params = data;
    return params->callback( instance, params->ref ) == DIENUM_CONTINUE;
}

static HRESULT WINAPI hid_joystick_EnumObjects( IDirectInputDevice8W *iface, LPDIENUMDEVICEOBJECTSCALLBACKW callback,
                                                void *ref, DWORD flags )
{
    struct hid_joystick *impl = impl_from_IDirectInputDevice8W( iface );
    struct enum_objects_params params = {callback, ref};
    DIPROPHEADER header = {sizeof(header), sizeof(header), DIPH_DEVICE, 0};

    TRACE( "iface %p, callback %p, ref %p, flags %#x.\n", iface, callback, ref, flags );

    if (!callback) return DIERR_INVALIDPARAM;

    enum_hid_objects( impl, &header, flags, enum_objects_callback, &params, FALSE );

    return S_OK;
}

static BOOL get_property_prop_range( struct hid_joystick *impl, struct hid_object *object,
                                     DIDEVICEOBJECTINSTANCEW *instance, void *data )
{
    DIPROPRANGE *value = data;
    if (object->type != VALUE_CAPS) return TRUE;
    value->lMin = object->value->PhysicalMin;
    value->lMax = object->value->PhysicalMax;
    return TRUE;
}

static BOOL get_property_prop_deadzone( struct hid_joystick *impl, struct hid_object *object,
                                        DIDEVICEOBJECTINSTANCEW *instance, void *data )
{
    struct hid_extra_value_caps *extra;
    DIPROPDWORD *deadzone = data;
    if (object->type != VALUE_CAPS) return TRUE;
    extra = impl->extra_value_caps + (object->value - impl->input_value_caps);
    deadzone->dwData = extra->deadzone;
    return TRUE;
}

static BOOL get_property_prop_saturation( struct hid_joystick *impl, struct hid_object *object,
                                          DIDEVICEOBJECTINSTANCEW *instance, void *data )
{
    struct hid_extra_value_caps *extra;
    DIPROPDWORD *deadzone = data;
    if (object->type != VALUE_CAPS) return TRUE;
    extra = impl->extra_value_caps + (object->value - impl->input_value_caps);
    deadzone->dwData = extra->saturation;
    return TRUE;
}

static HRESULT WINAPI hid_joystick_GetProperty( IDirectInputDevice8W *iface, REFGUID guid, DIPROPHEADER *header )
{
    struct hid_joystick *impl = impl_from_IDirectInputDevice8W( iface );

    TRACE( "iface %p, guid %s, header %p\n", iface, debugstr_guid( guid ), header );

    if (!header) return DIERR_INVALIDPARAM;
    if (!IS_DIPROP( guid )) return DI_OK;

    switch (LOWORD( guid ))
    {
    case (DWORD_PTR)DIPROP_RANGE:
    {
        DIPROPRANGE *value = (DIPROPRANGE *)header;
        enum_hid_objects( impl, header, DIDFT_AXIS, get_property_prop_range, value, FALSE );
        return DI_OK;
    }
    case (DWORD_PTR)DIPROP_DEADZONE:
    {
        enum_hid_objects( impl, header, DIDFT_AXIS, get_property_prop_deadzone, header, FALSE );
        return DI_OK;
    }
    case (DWORD_PTR)DIPROP_SATURATION:
    {
        enum_hid_objects( impl, header, DIDFT_AXIS, get_property_prop_saturation, header, FALSE );
        return DI_OK;
    }
    case (DWORD_PTR)DIPROP_PRODUCTNAME:
    {
        DIPROPSTRING *value = (DIPROPSTRING *)header;
        lstrcpynW( value->wsz, impl->instance.tszProductName, MAX_PATH );
        return DI_OK;
    }
    case (DWORD_PTR)DIPROP_INSTANCENAME:
    {
        DIPROPSTRING *value = (DIPROPSTRING *)header;
        lstrcpynW( value->wsz, impl->instance.tszInstanceName, MAX_PATH );
        return DI_OK;
    }
    case (DWORD_PTR)DIPROP_VIDPID:
    {
        DIPROPDWORD *value = (DIPROPDWORD *)header;
        if (!impl->attrs.VendorID || !impl->attrs.ProductID) return DIERR_UNSUPPORTED;
        value->dwData = MAKELONG( impl->attrs.VendorID, impl->attrs.ProductID );
        return DI_OK;
    }
    case (DWORD_PTR)DIPROP_JOYSTICKID:
    {
        DIPROPDWORD *value = (DIPROPDWORD *)header;
        value->dwData = impl->instance.guidInstance.Data3;
        return DI_OK;
    }
    case (DWORD_PTR)DIPROP_GUIDANDPATH:
    {
        DIPROPGUIDANDPATH *value = (DIPROPGUIDANDPATH *)header;
        lstrcpynW( value->wszPath, impl->device_path, MAX_PATH );
        return DI_OK;
    }
    default: return IDirectInputDevice2WImpl_GetProperty( iface, guid, header );
    }
}

static BOOL set_property_prop_range( struct hid_joystick *impl, struct hid_object *object,
                                     DIDEVICEOBJECTINSTANCEW *instance, void *data )
{
    DIPROPRANGE *value = data;
    if (object->type != VALUE_CAPS) return TRUE;
    object->value->PhysicalMin = value->lMin;
    object->value->PhysicalMax = value->lMax;

    if (instance->dwType & DIDFT_POV) object->value->PhysicalMax -= value->lMax / (object->value->LogicalMax - object->value->LogicalMin + 1);
    return TRUE;
}

static BOOL set_property_prop_deadzone( struct hid_joystick *impl, struct hid_object *object,
                                        DIDEVICEOBJECTINSTANCEW *instance, void *data )
{
    struct hid_extra_value_caps *extra;
    DIPROPDWORD *deadzone = data;
    if (object->type != VALUE_CAPS) return TRUE;
    extra = impl->extra_value_caps + (object->value - impl->input_value_caps);
    extra->deadzone = deadzone->dwData;
    return TRUE;
}

static BOOL set_property_prop_saturation( struct hid_joystick *impl, struct hid_object *object,
                                          DIDEVICEOBJECTINSTANCEW *instance, void *data )
{
    struct hid_extra_value_caps *extra;
    DIPROPDWORD *saturation = data;
    if (object->type != VALUE_CAPS) return TRUE;
    extra = impl->extra_value_caps + (object->value - impl->input_value_caps);
    extra->saturation = saturation->dwData;
    return TRUE;
}

static HRESULT WINAPI hid_joystick_SetProperty( IDirectInputDevice8W *iface, REFGUID guid, const DIPROPHEADER *header )
{
    struct hid_joystick *impl = impl_from_IDirectInputDevice8W( iface );

    TRACE( "iface %p, guid %s, header %p\n", iface, debugstr_guid( guid ), header );

    if (!header) return DIERR_INVALIDPARAM;
    if (!IS_DIPROP( guid )) return DI_OK;

    switch (LOWORD( guid ))
    {
    case (DWORD_PTR)DIPROP_RANGE:
    {
        DIPROPRANGE *value = (DIPROPRANGE *)header;
        enum_hid_objects( impl, header, DIDFT_AXIS, set_property_prop_range, (void *)value, FALSE );
        return DI_OK;
    }
    case (DWORD_PTR)DIPROP_DEADZONE:
    {
        enum_hid_objects( impl, header, DIDFT_AXIS, set_property_prop_deadzone, (void *)header, FALSE );
        return DI_OK;
    }
    case (DWORD_PTR)DIPROP_SATURATION:
    {
        enum_hid_objects( impl, header, DIDFT_AXIS, set_property_prop_saturation, (void *)header, FALSE );
        return DI_OK;
    }
    default: return IDirectInputDevice2WImpl_SetProperty( iface, guid, header );
    }
}

static HRESULT WINAPI hid_joystick_Acquire( IDirectInputDevice8W *iface )
{
    struct hid_joystick *impl = impl_from_IDirectInputDevice8W( iface );
    ULONG report_len = impl->caps.InputReportByteLength;
    HRESULT hr;

    TRACE( "iface %p.\n", iface );

    if ((hr = IDirectInputDevice2WImpl_Acquire( iface )) != DI_OK) return hr;

    memset( &impl->read_ovl, 0, sizeof(impl->read_ovl) );
    impl->read_ovl.hEvent = impl->base.read_event;
    if (ReadFile( impl->device, impl->input_report_buf, report_len, NULL, &impl->read_ovl ))
        impl->base.read_callback( iface );

    return DI_OK;
}

static HRESULT WINAPI hid_joystick_Unacquire( IDirectInputDevice8W *iface )
{
    struct hid_joystick *impl = impl_from_IDirectInputDevice8W( iface );
    HRESULT hr;
    BOOL ret;

    TRACE( "iface %p.\n", iface );

    if ((hr = IDirectInputDevice2WImpl_Unacquire( iface )) != DI_OK) return hr;

    ret = CancelIoEx( impl->device, &impl->read_ovl );
    if (!ret) WARN( "CancelIoEx failed, last error %u\n", GetLastError() );

    return DI_OK;
}

static HRESULT WINAPI hid_joystick_GetDeviceState( IDirectInputDevice8W *iface, DWORD len, void *ptr )
{
    struct hid_joystick *impl = impl_from_IDirectInputDevice8W( iface );

    TRACE( "iface %p, len %u, ptr %p.\n", iface, len, ptr );

    if (!ptr) return DIERR_INVALIDPARAM;

    fill_DataFormat( ptr, len, &impl->state, &impl->base.data_format );

    return DI_OK;
}

static BOOL get_object_info( struct hid_joystick *impl, struct hid_object *object,
                             DIDEVICEOBJECTINSTANCEW *instance, void *data )
{
    DIDEVICEOBJECTINSTANCEW *dest = data;
    memcpy( dest, instance, dest->dwSize );
    return FALSE;
}

static HRESULT WINAPI hid_joystick_GetObjectInfo( IDirectInputDevice8W *iface, DIDEVICEOBJECTINSTANCEW *instance,
                                                  DWORD obj, DWORD how )
{
    struct hid_joystick *impl = impl_from_IDirectInputDevice8W( iface );
    DIPROPHEADER header = {sizeof(header), sizeof(header), how, obj};

    TRACE( "iface %p, instance %p, obj %#x, how %#x.\n", iface, instance, obj, how );

    if (!instance) return E_POINTER;
    if (instance->dwSize != sizeof(DIDEVICEOBJECTINSTANCE_DX3W) &&
        instance->dwSize != sizeof(DIDEVICEOBJECTINSTANCEW))
        return DIERR_INVALIDPARAM;

    enum_hid_objects( impl, &header, DIDFT_ALL, get_object_info, NULL, FALSE );

    return S_OK;
}

static HRESULT WINAPI hid_joystick_GetDeviceInfo( IDirectInputDevice8W *iface, DIDEVICEINSTANCEW *instance )
{
    struct hid_joystick *impl = impl_from_IDirectInputDevice8W( iface );

    TRACE( "iface %p, instance %p.\n", iface, instance );

    if (!instance) return E_POINTER;
    if (instance->dwSize != sizeof(DIDEVICEINSTANCE_DX3W) &&
        instance->dwSize != sizeof(DIDEVICEINSTANCEW))
        return DIERR_INVALIDPARAM;

    memcpy( instance, &impl->instance, instance->dwSize );
    return S_OK;
}

static HRESULT WINAPI hid_joystick_CreateEffect( IDirectInputDevice8W *iface, REFGUID rguid,
                                                 const DIEFFECT *effect, IDirectInputEffect **out,
                                                 IUnknown *outer )
{
    FIXME( "iface %p, rguid %s, effect %p, out %p, outer %p stub!\n", iface, debugstr_guid( rguid ),
           effect, out, outer );

    if (!out) return E_POINTER;
    if (!rguid || !effect) return DI_NOEFFECT;

    return E_NOTIMPL;
}

static HRESULT WINAPI hid_joystick_EnumEffects( IDirectInputDevice8W *iface, LPDIENUMEFFECTSCALLBACKW callback,
                                                void *ref, DWORD type )
{
    DIEFFECTINFOW info = {sizeof(info)};
    HRESULT hr;

    TRACE( "iface %p, callback %p, ref %p, type %#x.\n", iface, callback, ref, type );

    if (!callback) return DIERR_INVALIDPARAM;

    type = DIEFT_GETTYPE(type);

    if (type == DIEFT_ALL || type == DIEFT_CONSTANTFORCE)
    {
        hr = IDirectInputDevice8_GetEffectInfo(iface, &info, &GUID_ConstantForce);
        if (hr == S_OK && !callback(&info, ref)) return DI_OK;
        else if (FAILED(hr) && hr != DIERR_DEVICENOTREG) return hr;
    }

    if (type == DIEFT_ALL || type == DIEFT_RAMPFORCE)
    {
        hr = IDirectInputDevice8_GetEffectInfo(iface, &info, &GUID_RampForce);
        if (hr == S_OK && !callback(&info, ref)) return DI_OK;
        else if (FAILED(hr) && hr != DIERR_DEVICENOTREG) return hr;
    }

    if (type == DIEFT_ALL || type == DIEFT_PERIODIC)
    {
        hr = IDirectInputDevice8_GetEffectInfo(iface, &info, &GUID_Square);
        if (hr == S_OK && !callback(&info, ref)) return DI_OK;
        else if (FAILED(hr) && hr != DIERR_DEVICENOTREG) return hr;

        hr = IDirectInputDevice8_GetEffectInfo(iface, &info, &GUID_Sine);
        if (hr == S_OK && !callback(&info, ref)) return DI_OK;
        else if (FAILED(hr) && hr != DIERR_DEVICENOTREG) return hr;

        hr = IDirectInputDevice8_GetEffectInfo(iface, &info, &GUID_Triangle);
        if (hr == S_OK && !callback(&info, ref)) return DI_OK;
        else if (FAILED(hr) && hr != DIERR_DEVICENOTREG) return hr;

        hr = IDirectInputDevice8_GetEffectInfo(iface, &info, &GUID_SawtoothUp);
        if (hr == S_OK && !callback(&info, ref)) return DI_OK;
        else if (FAILED(hr) && hr != DIERR_DEVICENOTREG) return hr;

        hr = IDirectInputDevice8_GetEffectInfo(iface, &info, &GUID_SawtoothDown);
        if (hr == S_OK && !callback(&info, ref)) return DI_OK;
        else if (FAILED(hr) && hr != DIERR_DEVICENOTREG) return hr;
    }

    if (type == DIEFT_ALL || type == DIEFT_CONDITION)
    {
        hr = IDirectInputDevice8_GetEffectInfo(iface, &info, &GUID_Spring);
        if (hr == S_OK && !callback(&info, ref)) return DI_OK;
        else if (FAILED(hr) && hr != DIERR_DEVICENOTREG) return hr;

        hr = IDirectInputDevice8_GetEffectInfo(iface, &info, &GUID_Damper);
        if (hr == S_OK && !callback(&info, ref)) return DI_OK;
        else if (FAILED(hr) && hr != DIERR_DEVICENOTREG) return hr;

        hr = IDirectInputDevice8_GetEffectInfo(iface, &info, &GUID_Inertia);
        if (hr == S_OK && !callback(&info, ref)) return DI_OK;
        else if (FAILED(hr) && hr != DIERR_DEVICENOTREG) return hr;

        hr = IDirectInputDevice8_GetEffectInfo(iface, &info, &GUID_Friction);
        if (hr == S_OK && !callback(&info, ref)) return DI_OK;
        else if (FAILED(hr) && hr != DIERR_DEVICENOTREG) return hr;
    }

    return DI_OK;
}

static HRESULT WINAPI hid_joystick_GetEffectInfo( IDirectInputDevice8W *iface, DIEFFECTINFOW *info, REFGUID guid )
{
    struct hid_joystick *impl = impl_from_IDirectInputDevice8W( iface );
    HIDP_BUTTON_CAPS button;
    HIDP_VALUE_CAPS value;
    NTSTATUS status;
    USHORT i, collection, count;
    USAGE usage = 0;

    FIXME( "iface %p, info %p, guid %s stub!\n", iface, info, debugstr_guid( guid ) );

    if (!info) return E_POINTER;
    if (info->dwSize != sizeof(DIEFFECTINFOW)) return DIERR_INVALIDPARAM;

    info->guid = *guid;
#if 0
    DWORD dwStaticParams;
    DWORD dwDynamicParams;
    WCHAR tszName[MAX_PATH];
#endif

    if (IsEqualGUID(guid, &GUID_ConstantForce))
    {
        usage = HID_USAGE_PID_ET_CONSTANT_FORCE;
        info->dwEffType = DIEFT_CONSTANTFORCE;
    }

    if (IsEqualGUID(guid, &GUID_RampForce))
    {
        usage = HID_USAGE_PID_ET_RAMP;
        info->dwEffType = DIEFT_RAMPFORCE;
    }

    if (IsEqualGUID(guid, &GUID_Square))
    {
        usage = HID_USAGE_PID_ET_SQUARE;
        info->dwEffType = DIEFT_PERIODIC;
    }
    if (IsEqualGUID(guid, &GUID_Sine))
    {
        usage = HID_USAGE_PID_ET_SINE;
        info->dwEffType = DIEFT_PERIODIC;
    }
    if (IsEqualGUID(guid, &GUID_Triangle))
    {
        usage = HID_USAGE_PID_ET_TRIANGLE;
        info->dwEffType = DIEFT_PERIODIC;
    }
    if (IsEqualGUID(guid, &GUID_SawtoothUp))
    {
        usage = HID_USAGE_PID_ET_SAWTOOTH_UP;
        info->dwEffType = DIEFT_PERIODIC;
    }
    if (IsEqualGUID(guid, &GUID_SawtoothDown))
    {
        usage = HID_USAGE_PID_ET_SAWTOOTH_DOWN;
        info->dwEffType = DIEFT_PERIODIC;
    }

    if (IsEqualGUID(guid, &GUID_Spring))
    {
        usage = HID_USAGE_PID_ET_SPRING;
        info->dwEffType = DIEFT_CONDITION;
    }
    if (IsEqualGUID(guid, &GUID_Damper))
    {
        usage = HID_USAGE_PID_ET_DAMPER;
        info->dwEffType = DIEFT_CONDITION;
    }
    if (IsEqualGUID(guid, &GUID_Inertia))
    {
        usage = HID_USAGE_PID_ET_INERTIA;
        info->dwEffType = DIEFT_CONDITION;
    }
    if (IsEqualGUID(guid, &GUID_Friction))
    {
        usage = HID_USAGE_PID_ET_FRICTION;
        info->dwEffType = DIEFT_CONDITION;
    }

    if (!usage) return DI_NOEFFECT;

    for (i = 0; i < impl->caps.NumberLinkCollectionNodes; ++i)
    {
        if (impl->collection_nodes[i].LinkUsagePage != HID_USAGE_PAGE_PID) continue;
        if (impl->collection_nodes[i].LinkUsage == usage) break;
    }

    if (i == impl->caps.NumberLinkCollectionNodes) return DIERR_DEVICENOTREG;
    else collection = i;

    /* check for HID_USAGE_PID_OP_EFFECT_START */
    usage = HID_USAGE_PID_OP_EFFECT_START;
    for (i = 0; i < impl->caps.NumberOutputButtonCaps; ++i)
    {
        button = impl->feature_button_caps[i];
        if (button.LinkCollection != collection) continue;
        if (button.UsagePage != HID_USAGE_PAGE_PID) continue;
        if (button.IsRange && button.Range.UsageMin <= usage && button.Range.UsageMax >= usage) break;
        if (!button.IsRange && button.NotRange.Usage == usage) break;
    }
    if (i == impl->caps.NumberOutputButtonCaps) return DIERR_DEVICENOTREG;

    for (i = HidP_Output; i <= HidP_Feature; ++i)
    {
        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_ATTACK_LEVEL, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwEffType |= DIEFT_FFATTACK;

        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_ATTACK_TIME, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwEffType |= DIEFT_FFATTACK;

        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_FADE_LEVEL, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwEffType |= DIEFT_FFFADE;

        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_FADE_TIME, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwEffType |= DIEFT_FFFADE;

        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_DEAD_BAND, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwEffType |= DIEFT_DEADBAND;

        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_START_DELAY, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwEffType |= DIEFT_STARTDELAY;

        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_DURATION, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwDynamicParams |= DIEP_DURATION;

        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_SAMPLE_PERIOD, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwDynamicParams |= DIEP_SAMPLEPERIOD;

        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_GAIN, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwDynamicParams |= DIEP_GAIN;

        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_TRIGGER_BUTTON, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwDynamicParams |= DIEP_TRIGGERBUTTON;

        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_TRIGGER_REPEAT_INTERVAL, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwDynamicParams |= DIEP_TRIGGERREPEATINTERVAL;

        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_AXES_ENABLE, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwDynamicParams |= DIEP_AXES;

        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_DIRECTION, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwDynamicParams |= DIEP_DIRECTION;

        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_SET_ENVELOPE_REPORT, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwDynamicParams |= DIEP_ENVELOPE;

        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_TYPE_SPECIFIC_BLOCK_OFFSET, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwDynamicParams |= DIEP_TYPESPECIFICPARAMS;

        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_START_DELAY, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwDynamicParams |= DIEP_STARTDELAY;

#if 0
        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_OP_EFFECT_START, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwDynamicParams |= DIEP_START;

        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwDynamicParams |= DIEP_NORESTART;

        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwDynamicParams |= DIEP_NODOWNLOAD;

        count = 1;
        status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_, &value, &count, impl->preparsed );
        if (status == HIDP_STATUS_SUCCESS && count) info->dwDynamicParams |= DIEB_NOTRIGGER;
#endif

        if (info->dwEffType & DIEFT_CONDITION)
        {
            count = 1;
            status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_POSITIVE_SATURATION, &value, &count, impl->preparsed );
            if (status == HIDP_STATUS_SUCCESS && count) info->dwEffType |= DIEFT_SATURATION | DIEFT_POSNEGSATURATION;

            count = 1;
            status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_NEGATIVE_SATURATION, &value, &count, impl->preparsed );
            if (status == HIDP_STATUS_SUCCESS && count) info->dwEffType |= DIEFT_SATURATION | DIEFT_POSNEGSATURATION;

            count = 1;
            status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_POSITIVE_COEFFICIENT, &value, &count, impl->preparsed );
            if (status == HIDP_STATUS_SUCCESS && count) info->dwEffType |= DIEFT_POSNEGCOEFFICIENTS;

            count = 1;
            status = HidP_GetSpecificValueCaps( i, HID_USAGE_PAGE_PID, collection, HID_USAGE_PID_NEGATIVE_COEFFICIENT, &value, &count, impl->preparsed );
            if (status == HIDP_STATUS_SUCCESS && count) info->dwEffType |= DIEFT_POSNEGCOEFFICIENTS;
        }
    }

    return DI_OK;
}

static HRESULT WINAPI hid_joystick_GetForceFeedbackState( IDirectInputDevice8W *iface, DWORD *out )
{
    struct hid_joystick *impl = impl_from_IDirectInputDevice8W( iface );
    NTSTATUS status;
    ULONG value, flags = 0;

    TRACE( "iface %p, out %p.\n", iface, out );

    if (!out) return E_POINTER;

    (*out) = 0;

    memset( impl->feature_report_buf, 0, impl->caps.FeatureReportByteLength);

    if (!HidD_GetFeature( impl->device, impl->feature_report_buf, impl->caps.FeatureReportByteLength ))
        WARN( "HidD_GetFeature failed, error %u\n", GetLastError() );

    if ((status = HidP_GetUsageValue( HidP_Feature, HID_USAGE_PAGE_PID, 0, HID_USAGE_PID_DC_DEVICE_RESET, &value,
                                      impl->preparsed, impl->feature_report_buf, impl->caps.FeatureReportByteLength )) != HIDP_STATUS_SUCCESS)
        WARN( "HidP_GetUsageValue returned %x\n", status );
    if (value) flags |= DIGFFS_EMPTY;

    if ((status = HidP_GetUsageValue( HidP_Feature, HID_USAGE_PAGE_PID, 0, HID_USAGE_PID_DC_STOP_ALL_EFFECTS, &value,
                                      impl->preparsed, impl->feature_report_buf, impl->caps.FeatureReportByteLength )) != HIDP_STATUS_SUCCESS)
        WARN( "HidP_GetUsageValue returned %x\n", status );
    if (value) flags |= DIGFFS_STOPPED;

    if ((status = HidP_GetUsageValue( HidP_Feature, HID_USAGE_PAGE_PID, 0, HID_USAGE_PID_DC_DEVICE_PAUSE, &value,
                                      impl->preparsed, impl->feature_report_buf, impl->caps.FeatureReportByteLength )) != HIDP_STATUS_SUCCESS)
        WARN( "HidP_GetUsageValue returned %x\n", status );
    if (value) flags |= DIGFFS_PAUSED;

    if ((status = HidP_GetUsageValue( HidP_Feature, HID_USAGE_PAGE_PID, 0, HID_USAGE_PID_DC_ENABLE_ACTUATORS, &value,
                                      impl->preparsed, impl->feature_report_buf, impl->caps.FeatureReportByteLength )) != HIDP_STATUS_SUCCESS)
        WARN( "HidP_GetUsageValue returned %x\n", status );
    if (value) flags |= DIGFFS_ACTUATORSON;

    if ((status = HidP_GetUsageValue( HidP_Feature, HID_USAGE_PAGE_PID, 0, HID_USAGE_PID_DC_DISABLE_ACTUATORS, &value,
                                      impl->preparsed, impl->feature_report_buf, impl->caps.FeatureReportByteLength )) != HIDP_STATUS_SUCCESS)
        WARN( "HidP_GetUsageValue returned %x\n", status );
    if (value) flags |= DIGFFS_ACTUATORSOFF;

    *out = flags;

#if 0
#define DIGFFS_POWERON          0x00000040
#define DIGFFS_POWEROFF         0x00000080
#define DIGFFS_SAFETYSWITCHON   0x00000100
#define DIGFFS_SAFETYSWITCHOFF  0x00000200
#define DIGFFS_USERFFSWITCHON   0x00000400
#define DIGFFS_USERFFSWITCHOFF  0x00000800
#define DIGFFS_DEVICELOST       0x80000000
#endif

    return DI_OK;
}

static HRESULT WINAPI hid_joystick_SendForceFeedbackCommand( IDirectInputDevice8W *iface, DWORD flags )
{
    struct hid_joystick *impl = impl_from_IDirectInputDevice8W( iface );
    NTSTATUS status;

    TRACE( "iface %p, flags %x.\n", iface, flags );

    memset( impl->feature_report_buf, 0, impl->caps.FeatureReportByteLength);

    if ((status = HidP_SetUsageValue( HidP_Feature, HID_USAGE_PAGE_PID, 0, HID_USAGE_PID_DC_DEVICE_RESET, flags & DISFFC_RESET ? 1 : 0,
                                      impl->preparsed, impl->feature_report_buf, impl->caps.FeatureReportByteLength )) != HIDP_STATUS_SUCCESS)
        WARN( "HidP_SetUsageValue returned %x\n", status );
    if ((status = HidP_SetUsageValue( HidP_Feature, HID_USAGE_PAGE_PID, 0, HID_USAGE_PID_DC_STOP_ALL_EFFECTS, flags & DISFFC_STOPALL ? 1 : 0,
                                      impl->preparsed, impl->feature_report_buf, impl->caps.FeatureReportByteLength )) != HIDP_STATUS_SUCCESS)
        WARN( "HidP_SetUsageValue returned %x\n", status );
    if ((status = HidP_SetUsageValue( HidP_Feature, HID_USAGE_PAGE_PID, 0, HID_USAGE_PID_DC_DEVICE_PAUSE, flags & DISFFC_PAUSE ? 1 : 0,
                                      impl->preparsed, impl->feature_report_buf, impl->caps.FeatureReportByteLength )) != HIDP_STATUS_SUCCESS)
        WARN( "HidP_SetUsageValue returned %x\n", status );
    if ((status = HidP_SetUsageValue( HidP_Feature, HID_USAGE_PAGE_PID, 0, HID_USAGE_PID_DC_DEVICE_CONTINUE, flags & DISFFC_CONTINUE ? 1 : 0,
                                      impl->preparsed, impl->feature_report_buf, impl->caps.FeatureReportByteLength )) != HIDP_STATUS_SUCCESS)
        WARN( "HidP_SetUsageValue returned %x\n", status );
    if ((status = HidP_SetUsageValue( HidP_Feature, HID_USAGE_PAGE_PID, 0, HID_USAGE_PID_DC_ENABLE_ACTUATORS, flags & DISFFC_SETACTUATORSON ? 1 : 0,
                                      impl->preparsed, impl->feature_report_buf, impl->caps.FeatureReportByteLength )) != HIDP_STATUS_SUCCESS)
        WARN( "HidP_SetUsageValue returned %x\n", status );
    if ((status = HidP_SetUsageValue( HidP_Feature, HID_USAGE_PAGE_PID, 0, HID_USAGE_PID_DC_DISABLE_ACTUATORS, flags & DISFFC_SETACTUATORSOFF ? 1 : 0,
                                      impl->preparsed, impl->feature_report_buf, impl->caps.FeatureReportByteLength )) != HIDP_STATUS_SUCCESS)
        WARN( "HidP_SetUsageValue returned %x\n", status );

    if (!HidD_SetFeature( impl->device, impl->feature_report_buf, impl->caps.FeatureReportByteLength ))
        WARN( "HidD_SetFeature failed, error %u\n", GetLastError() );

    return DI_OK;
}

static HRESULT WINAPI hid_joystick_EnumCreatedEffectObjects( IDirectInputDevice8W *iface,
                                                             LPDIENUMCREATEDEFFECTOBJECTSCALLBACK callback,
                                                             void *ref, DWORD flags )
{
    FIXME( "iface %p, callback %p, ref %p, flags %#x stub!\n", iface, callback, ref, flags );

    if (!callback) return DIERR_INVALIDPARAM;

    return E_NOTIMPL;
}

struct parse_input_report_params
{
    DIJOYSTATE2 old_state;
    char *report_buf;
    DWORD report_len;
    DWORD time;
    DWORD seq;
};

static BOOL parse_input_report( struct hid_joystick *impl, struct hid_object *object,
                                DIDEVICEOBJECTINSTANCEW *instance, void *data )
{
    IDirectInputDevice8W *iface = &impl->base.IDirectInputDevice8W_iface;
    struct parse_input_report_params *params = data;
    NTSTATUS status;
    DWORD i = DIDFT_GETINSTANCE( instance->dwType );
    ULONG logical_value;
    LONG value, neutral;

    if (instance->dwType & DIDFT_BUTTON)
    {
        if (object->type != BUTTON_CAPS)
        {
            FIXME( "Unimplemented button object type %x", object->type );
            return TRUE;
        }
        if (params->old_state.rgbButtons[i] == impl->state.rgbButtons[i]) return TRUE;
        queue_event( iface, instance->dwType, impl->state.rgbButtons[i], params->time, params->seq );
    }
    else if (instance->dwType & (DIDFT_POV | DIDFT_AXIS))
    {
        struct hid_extra_value_caps *extra;
        HIDP_VALUE_CAPS *caps = object->value;
        if (object->type != VALUE_CAPS)
        {
            FIXME( "Unimplemented axis / pov object type %x", object->type );
            return TRUE;
        }
        extra = impl->extra_value_caps + (caps - impl->input_value_caps);

        if ((status = HidP_GetUsageValue( HidP_Input, instance->wUsagePage, 0, instance->wUsage, &logical_value,
                                          impl->preparsed, params->report_buf, params->report_len )) != HIDP_STATUS_SUCCESS)
            WARN( "HidP_GetUsageValue returned %x\n", status );

        if ((LONG)logical_value < caps->LogicalMin || (LONG)logical_value > caps->LogicalMax) value = -1;
        else value = MulDiv( logical_value - caps->LogicalMin, caps->PhysicalMax - caps->PhysicalMin, caps->LogicalMax - caps->LogicalMin ) + caps->PhysicalMin;

        if (instance->dwType & DIDFT_AXIS)
        {
            neutral = (caps->PhysicalMax + caps->PhysicalMin) / 2;
            if (abs( (LONG)value - neutral ) <= extra->deadzone) value = neutral;
            if ((LONG)value >= caps->PhysicalMax - extra->saturation) value = caps->PhysicalMax;
            if ((LONG)value <= caps->PhysicalMin + extra->saturation) value = caps->PhysicalMin;
        }

        switch (instance->dwOfs)
        {
        case DIJOFS_X:
            if (impl->state.lX == value) break;
            impl->state.lX = value;
            queue_event( iface, instance->dwType, value, params->time, params->seq );
            break;
        case DIJOFS_Y:
            if (impl->state.lY == value) break;
            impl->state.lY = value;
            queue_event( iface, instance->dwType, value, params->time, params->seq );
            break;
        case DIJOFS_Z:
            if (impl->state.lZ == value) break;
            impl->state.lZ = value;
            queue_event( iface, instance->dwType, value, params->time, params->seq );
            break;
        case DIJOFS_RX:
            if (impl->state.lRx == value) break;
            impl->state.lRx = value;
            queue_event( iface, instance->dwType, value, params->time, params->seq );
            break;
        case DIJOFS_RY:
            if (impl->state.lRy == value) break;
            impl->state.lRy = value;
            queue_event( iface, instance->dwType, value, params->time, params->seq );
            break;
        case DIJOFS_RZ:
            if (impl->state.lRz == value) break;
            impl->state.lRz = value;
            queue_event( iface, instance->dwType, value, params->time, params->seq );
            break;
        case DIJOFS_POV( 0 ):
            if (impl->state.rgdwPOV[0] == value) break;
            impl->state.rgdwPOV[0] = value;
            queue_event( iface, instance->dwType, value, params->time, params->seq );
            break;
        default:
            FIXME( "Unimplemented offset %x.\n", instance->dwOfs );
            break;
        }
    }

    return TRUE;
}

#if 0
static HRESULT WINAPI hid_joystick_Poll( IDirectInputDevice8W *iface )
{
    struct hid_joystick *impl = impl_from_IDirectInputDevice8W( iface );
    struct parse_input_report_params params;
    DIPROPHEADER header = {sizeof(header), sizeof(header), DIPH_DEVICE, 0};
    NTSTATUS status;
    ULONG count, report_len = impl->caps.InputReportByteLength;
    UINT16 magnitudes[] = {0x7fff, 0x7fff};

    TRACE( "iface %p.\n", iface );

#if 1
    memset( impl->output_report_buf, 0, impl->caps.OutputReportByteLength);
    if ((status = HidP_SetUsageValue( HidP_Output, HID_USAGE_PAGE_PID, 0, HID_USAGE_PID_DC_ENABLE_ACTUATORS, 1,
                                      impl->preparsed, impl->output_report_buf, impl->caps.OutputReportByteLength )) != HIDP_STATUS_SUCCESS)
        WARN( "HidP_SetUsageValue returned %x\n", status );

    if ((status = HidP_SetUsageValue( HidP_Output, HID_USAGE_PAGE_PID, 0, HID_USAGE_PID_DURATION, 0xffffffff,
                                      impl->preparsed, impl->output_report_buf, impl->caps.OutputReportByteLength )) != HIDP_STATUS_SUCCESS)
        WARN( "HidP_SetUsageValue returned %x\n", status );

    if ((status = HidP_SetUsageValueArray( HidP_Output, HID_USAGE_PAGE_PID, 0, HID_USAGE_PID_MAGNITUDE, (char *)&magnitudes, sizeof(magnitudes),
                                           impl->preparsed, impl->output_report_buf, impl->caps.OutputReportByteLength )) != HIDP_STATUS_SUCCESS)
        WARN( "HidP_SetUsageValue returned %x\n", status );

    if (HidD_SetOutputReport( impl->device, impl->output_report_buf, impl->caps.OutputReportByteLength ))
    {
    }
#endif

#if 0
    if (!ReadFile( impl->device, impl->input_report_buf, report_len, &count, NULL )) return DIERR_INPUTLOST;
    if (count > 0)
#else
    impl->input_report_buf[0] = impl->report_id;
    if (HidD_GetInputReport( impl->device, impl->input_report_buf, report_len ))
#endif
    {
        params.old_state = impl->state;
        params.report_buf = impl->input_report_buf;
        params.report_len = report_len;
        params.time = GetCurrentTime();
        params.seq = impl->base.dinput->evsequence++;

        count = impl->usages_count;
        memset( impl->usages_buf, 0, count * sizeof(*impl->usages_buf) );
        status = HidP_GetUsagesEx( HidP_Input, 0, impl->usages_buf, &count, impl->preparsed,
                                   params.report_buf, params.report_len );
        if (status != HIDP_STATUS_SUCCESS) WARN( "HidP_GetUsagesEx returned %x\n", status );

        memset( impl->state.rgbButtons, 0, sizeof(impl->state.rgbButtons) );
        while (count--)
        {
            USAGE_AND_PAGE *button = impl->usages_buf + count;
            if (button->UsagePage != HID_USAGE_PAGE_BUTTON) FIXME( "Unimplemented button usage page %x.\n", button->UsagePage );
            else if (button->Usage >= 128) FIXME( "Ignoring extraneous button %d.\n", button->Usage );
            else impl->state.rgbButtons[button->Usage] = 0x80;
        }

        enum_hid_objects( impl, &header, DIDFT_ALL, parse_input_report, &params, FALSE );
    }

    if (!impl->base.acquired) return DIERR_NOTACQUIRED;
    return DI_NOEFFECT;
}
#endif

static HRESULT WINAPI hid_joystick_BuildActionMap( IDirectInputDevice8W *iface, DIACTIONFORMATW *format,
                                                   const WCHAR *username, DWORD flags )
{
    FIXME( "iface %p, format %p, username %s, flags %#x stub!\n", iface, format, debugstr_w(username), flags );

    if (!format) return DIERR_INVALIDPARAM;

    return E_NOTIMPL;
}

static HRESULT WINAPI hid_joystick_SetActionMap( IDirectInputDevice8W *iface, DIACTIONFORMATW *format,
                                                 const WCHAR *username, DWORD flags )
{
    struct hid_joystick *impl = impl_from_IDirectInputDevice8W( iface );

    TRACE( "iface %p, format %p, username %s, flags %#x.\n", iface, format, debugstr_w(username), flags );

    if (!format) return DIERR_INVALIDPARAM;

    return _set_action_map( iface, format, username, flags, impl->base.data_format.wine_df );
}

static const IDirectInputDevice8WVtbl hid_joystick_vtbl =
{
    /*** IUnknown methods ***/
    IDirectInputDevice2WImpl_QueryInterface,
    IDirectInputDevice2WImpl_AddRef,
    hid_joystick_Release,
    /*** IDirectInputDevice methods ***/
    hid_joystick_GetCapabilities,
    hid_joystick_EnumObjects,
    hid_joystick_GetProperty,
    hid_joystick_SetProperty,
    hid_joystick_Acquire,
    hid_joystick_Unacquire,
    hid_joystick_GetDeviceState,
    IDirectInputDevice2WImpl_GetDeviceData,
    IDirectInputDevice2WImpl_SetDataFormat,
    IDirectInputDevice2WImpl_SetEventNotification,
    IDirectInputDevice2WImpl_SetCooperativeLevel,
    hid_joystick_GetObjectInfo,
    hid_joystick_GetDeviceInfo,
    IDirectInputDevice2WImpl_RunControlPanel,
    IDirectInputDevice2WImpl_Initialize,
    /*** IDirectInputDevice2 methods ***/
    hid_joystick_CreateEffect,
    hid_joystick_EnumEffects,
    hid_joystick_GetEffectInfo,
    hid_joystick_GetForceFeedbackState,
    hid_joystick_SendForceFeedbackCommand,
    hid_joystick_EnumCreatedEffectObjects,
    IDirectInputDevice2WImpl_Escape,
    IDirectInputDevice2WImpl_Poll,
    IDirectInputDevice2WImpl_SendDeviceData,
    /*** IDirectInputDevice7 methods ***/
    IDirectInputDevice7WImpl_EnumEffectsInFile,
    IDirectInputDevice7WImpl_WriteEffectToFile,
    /*** IDirectInputDevice8 methods ***/
    hid_joystick_BuildActionMap,
    hid_joystick_SetActionMap,
    IDirectInputDevice8WImpl_GetImageInfo,
};

static HRESULT hid_joystick_read_state( IDirectInputDevice8W *iface )
{
    struct hid_joystick *impl = impl_from_IDirectInputDevice8W( iface );
    struct parse_input_report_params params;
    DIPROPHEADER header = {sizeof(header), sizeof(header), DIPH_DEVICE, 0};
    ULONG i, count, report_len = impl->caps.InputReportByteLength;
    NTSTATUS status;
    BOOL ret;

    ret = GetOverlappedResult( impl->device, &impl->read_ovl, &count, FALSE );
    if (!ret) WARN( "read failed, error %u\n", GetLastError() );
    else
    {
        const BYTE *report = (BYTE *)impl->input_report_buf;
        TRACE( "read size %u report:\n", count );
        for (i = 0; i < report_len;)
        {
            char buffer[256], *buf = buffer;
            buf += sprintf(buf, "%08x ", i);
            do { buf += sprintf(buf, " %02x", report[i] ); } while (++i % 16 && i < report_len);
            TRACE("%s\n", buffer);
        }
    }

    do
    {
        params.old_state = impl->state;
        params.report_buf = impl->input_report_buf;
        params.report_len = report_len;
        params.time = GetCurrentTime();
        params.seq = impl->base.dinput->evsequence++;

        count = impl->usages_count;
        memset( impl->usages_buf, 0, count * sizeof(*impl->usages_buf) );
        status = HidP_GetUsagesEx( HidP_Input, 0, impl->usages_buf, &count, impl->preparsed,
                                   params.report_buf, params.report_len );
        if (status != HIDP_STATUS_SUCCESS) WARN( "HidP_GetUsagesEx returned %x\n", status );

        memset( impl->state.rgbButtons, 0, sizeof(impl->state.rgbButtons) );
        while (count--)
        {
            USAGE_AND_PAGE *button = impl->usages_buf + count;
            if (button->UsagePage != HID_USAGE_PAGE_BUTTON) FIXME( "Unimplemented button usage page %x.\n", button->UsagePage );
            else if (button->Usage >= 128) FIXME( "Ignoring extraneous button %d.\n", button->Usage );
            else impl->state.rgbButtons[button->Usage] = 0x80;
        }

        enum_hid_objects( impl, &header, DIDFT_ALL, parse_input_report, &params, FALSE );

        memset( &impl->read_ovl, 0, sizeof(impl->read_ovl) );
        impl->read_ovl.hEvent = impl->base.read_event;
    } while (ReadFile( impl->device, impl->input_report_buf, report_len, &count, &impl->read_ovl ));

    return DI_OK;
}

static BOOL hid_joystick_device_try_open( UINT32 handle, const WCHAR *path, HANDLE *device,
                                          PHIDP_PREPARSED_DATA *preparsed, HIDD_ATTRIBUTES *attrs,
                                          HIDP_CAPS *caps, DIDEVICEINSTANCEW *instance, DWORD version )
{
    PHIDP_PREPARSED_DATA preparsed_data = NULL;
    HANDLE device_file;

    device_file = CreateFileW( path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, 0 );
    if (device_file == INVALID_HANDLE_VALUE) return FALSE;

    if (!HidD_GetPreparsedData( device_file, &preparsed_data )) goto failed;
    if (!HidD_GetAttributes( device_file, attrs )) goto failed;
    if (HidP_GetCaps( preparsed_data, caps ) != HIDP_STATUS_SUCCESS) goto failed;

    if (caps->UsagePage == HID_USAGE_PAGE_GAME) FIXME( "Unimplemented HID game usage page!\n" );
    if (caps->UsagePage == HID_USAGE_PAGE_SIMULATION) FIXME( "Unimplemented HID simulation usage page!\n" );
    if (caps->UsagePage != HID_USAGE_PAGE_GENERIC) goto failed;
    if (caps->Usage != HID_USAGE_GENERIC_GAMEPAD && caps->Usage != HID_USAGE_GENERIC_JOYSTICK) goto failed;

    if (!HidD_GetProductString( device_file, instance->tszInstanceName, MAX_PATH )) goto failed;
    if (!HidD_GetProductString( device_file, instance->tszProductName, MAX_PATH )) goto failed;

    instance->guidInstance = hid_joystick_guid;
    instance->guidInstance.Data1 ^= handle;
    instance->guidProduct = DInput_PIDVID_Product_GUID;
    instance->guidProduct.Data1 = MAKELONG( attrs->VendorID, attrs->ProductID );
    instance->dwDevType = get_device_type( version, caps->Usage != HID_USAGE_GENERIC_GAMEPAD ) | DIDEVTYPE_HID;
    instance->guidFFDriver = GUID_NULL;
    instance->wUsagePage = caps->UsagePage;
    instance->wUsage = caps->Usage;

    *device = device_file;
    *preparsed = preparsed_data;
    return TRUE;

failed:
    CloseHandle( device_file );
    HidD_FreePreparsedData( preparsed_data );
    return FALSE;
}

static HRESULT hid_joystick_device_open( int index, DIDEVICEINSTANCEW *filter, WCHAR *device_path,
                                         HANDLE *device, PHIDP_PREPARSED_DATA *preparsed,
                                         HIDD_ATTRIBUTES *attrs, HIDP_CAPS *caps, DWORD version )
{
    char buffer[sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) + MAX_PATH * sizeof(WCHAR)];
    SP_DEVICE_INTERFACE_DETAIL_DATA_W *detail = (void *)buffer;
    SP_DEVICE_INTERFACE_DATA iface = {sizeof(iface)};
    SP_DEVINFO_DATA devinfo = {sizeof(devinfo)};
    DIDEVICEINSTANCEW instance = *filter;
    UINT32 i = 0, handle;
    HDEVINFO set;
    DWORD type;
    GUID hid;

    TRACE( "index %d, product %s, instance %s\n", index, debugstr_guid( &filter->guidProduct ),
           debugstr_guid( &filter->guidInstance ) );

    HidD_GetHidGuid( &hid );

    set = SetupDiGetClassDevsW( &hid, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT );
    if (set == INVALID_HANDLE_VALUE) return DIERR_DEVICENOTREG;

    *device = NULL;
    *preparsed = NULL;
    while (SetupDiEnumDeviceInterfaces( set, NULL, &hid, i++, &iface ))
    {
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW( set, &iface, detail, sizeof(buffer), NULL, &devinfo ))
            continue;
        if (!SetupDiGetDevicePropertyW( set, &devinfo, &DEVPROPKEY_HID_HANDLE, &type,
                                        (BYTE *)&handle, sizeof(handle), NULL, 0 ) ||
            type != DEVPROP_TYPE_UINT32)
            continue;
        if (!hid_joystick_device_try_open( handle, detail->DevicePath, device, preparsed,
                                           attrs, caps, &instance, version ))
            continue;

        /* enumerate device by GUID */
        if (index < 0 && IsEqualGUID( &filter->guidProduct, &instance.guidProduct )) break;
        if (index < 0 && IsEqualGUID( &filter->guidInstance, &instance.guidInstance )) break;

        /* enumerate all devices */
        if (index >= 0 && !index--) break;

        CloseHandle( *device );
        HidD_FreePreparsedData( *preparsed );
        *device = NULL;
        *preparsed = NULL;
    }

    SetupDiDestroyDeviceInfoList( set );
    if (!*device || !*preparsed) return DIERR_DEVICENOTREG;

    lstrcpynW( device_path, detail->DevicePath, MAX_PATH );
    *filter = instance;
    return DI_OK;
}

static HRESULT hid_joystick_enum_device( DWORD type, DWORD flags, DIDEVICEINSTANCEW *instance,
                                         DWORD version, int index )
{
    HIDD_ATTRIBUTES attrs = {sizeof(attrs)};
    PHIDP_PREPARSED_DATA preparsed;
    WCHAR device_path[MAX_PATH];
    HIDP_CAPS caps;
    HANDLE device;
    HRESULT hr;

    TRACE( "type %x, flags %#x, instance %p, version %04x, index %d\n", type, flags, instance, version, index );

    hr = hid_joystick_device_open( index, instance, device_path, &device, &preparsed,
                                   &attrs, &caps, version );
    if (hr != DI_OK) return hr;

    HidD_FreePreparsedData( preparsed );
    CloseHandle( device );

    if (instance->dwSize != sizeof(DIDEVICEINSTANCEW))
        return S_FALSE;
    if (version < 0x0800 && type != DIDEVTYPE_JOYSTICK)
        return S_FALSE;
    if (version >= 0x0800 && type != DI8DEVCLASS_ALL && type != DI8DEVCLASS_GAMECTRL)
        return S_FALSE;

    if (device_disabled_registry( "HID", TRUE ))
        return DIERR_DEVICENOTREG;

    TRACE( "Found device %s, usage %04x:%04x, product %s, instance %s, name %s\n", debugstr_w(device_path),
           instance->wUsagePage, instance->wUsage, debugstr_guid( &instance->guidProduct ),
           debugstr_guid( &instance->guidInstance ), debugstr_w(instance->tszInstanceName) );

    return DI_OK;
}

static BOOL init_data_format( struct hid_joystick *impl, struct hid_object *object,
                              DIDEVICEOBJECTINSTANCEW *instance, void *data )
{
    DIDATAFORMAT *format = data;
    DIOBJECTDATAFORMAT *obj_format;

    if (format->rgodf)
    {
        obj_format = format->rgodf + object->index;
        if (IsEqualGUID( &instance->guidType, &GUID_Unknown )) obj_format->pguid = &GUID_Unknown;
        else if (IsEqualGUID( &instance->guidType, &GUID_Button )) obj_format->pguid = &GUID_Button;
        else if (IsEqualGUID( &instance->guidType, &GUID_XAxis )) obj_format->pguid = &GUID_XAxis;
        else if (IsEqualGUID( &instance->guidType, &GUID_YAxis )) obj_format->pguid = &GUID_YAxis;
        else if (IsEqualGUID( &instance->guidType, &GUID_ZAxis )) obj_format->pguid = &GUID_ZAxis;
        else if (IsEqualGUID( &instance->guidType, &GUID_RxAxis )) obj_format->pguid = &GUID_RxAxis;
        else if (IsEqualGUID( &instance->guidType, &GUID_RyAxis )) obj_format->pguid = &GUID_RyAxis;
        else if (IsEqualGUID( &instance->guidType, &GUID_RzAxis )) obj_format->pguid = &GUID_RzAxis;
        else if (IsEqualGUID( &instance->guidType, &GUID_Slider )) obj_format->pguid = &GUID_Slider;
        else if (IsEqualGUID( &instance->guidType, &GUID_POV )) obj_format->pguid = &GUID_POV;
        else obj_format->pguid = NULL;
        obj_format->dwOfs = instance->dwOfs;
        obj_format->dwType = instance->dwType;
        obj_format->dwFlags = instance->dwFlags;
    }
    else
    {
        format->dwNumObjs++;
        if (instance->dwType & DIDFT_ABSAXIS) impl->dev_caps.dwAxes++;
        if (instance->dwType & DIDFT_POV) impl->dev_caps.dwPOVs++;
        if (instance->dwType & DIDFT_PSHBUTTON) impl->dev_caps.dwButtons++;
        if (instance->wReportId && !impl->report_id) impl->report_id = instance->wReportId;
    }

    return TRUE;
}

static HRESULT hid_joystick_create_device( IDirectInputImpl *dinput, REFGUID guid, IDirectInputDevice8W **out )
{
    DIDEVICEINSTANCEW instance = {.dwSize = sizeof(instance), .guidProduct = *guid, .guidInstance = *guid};
    DIPROPHEADER header = {sizeof(header), sizeof(header), DIPH_DEVICE, 0};
    DIPROPRANGE range = {{sizeof(range), sizeof(DIPROPHEADER)}};
    DIPROPDWORD value = {{sizeof(value), sizeof(DIPROPHEADER)}};
    DWORD size = sizeof(struct hid_joystick);
    HIDD_ATTRIBUTES attrs = {sizeof(attrs)};
    DIDEVCAPS dev_caps = {sizeof(dev_caps)};
    DIOBJECTDATAFORMAT *obj_format = NULL;
    struct hid_joystick *impl = NULL;
    PHIDP_PREPARSED_DATA preparsed;
    DIDATAFORMAT *format = NULL;
    WCHAR device_path[MAX_PATH];
    HIDP_CAPS caps;
    HANDLE device;
    HRESULT hr;

    TRACE( "dinput %p, guid %s, out %p\n", dinput, debugstr_guid( guid ), out );

    *out = NULL;
    instance.guidProduct.Data1 = DInput_PIDVID_Product_GUID.Data1;
    instance.guidInstance.Data1 = hid_joystick_guid.Data1;
    if (IsEqualGUID( &DInput_PIDVID_Product_GUID, &instance.guidProduct ))
        instance.guidProduct = *guid;
    else if (IsEqualGUID( &hid_joystick_guid, &instance.guidInstance ))
        instance.guidInstance = *guid;
    else
        return DIERR_DEVICENOTREG;

    hr = hid_joystick_device_open( -1, &instance, device_path, &device, &preparsed,
                                   &attrs, &caps, dinput->dwVersion );
    if (hr != DI_OK) return hr;

    dev_caps.dwFlags = DIDC_ATTACHED | DIDC_EMULATED;
    dev_caps.dwDevType = instance.dwDevType;

    size = ALIGN_SIZE( size, sizeof(void *) );
    size += caps.NumberLinkCollectionNodes * sizeof(HIDP_LINK_COLLECTION_NODE);

    size = ALIGN_SIZE( size, sizeof(void *) );
    size += (caps.NumberInputButtonCaps + caps.NumberOutputButtonCaps + caps.NumberFeatureButtonCaps) * sizeof(HIDP_BUTTON_CAPS);

    size = ALIGN_SIZE( size, sizeof(void *) );
    size += (caps.NumberInputValueCaps + caps.NumberOutputValueCaps + caps.NumberFeatureValueCaps) * sizeof(HIDP_VALUE_CAPS);

    size = ALIGN_SIZE( size, sizeof(void *) );
    size += HidP_MaxUsageListLength( HidP_Input, HID_USAGE_PAGE_BUTTON, preparsed ) * sizeof(USAGE_AND_PAGE);

    size += (caps.InputReportByteLength + caps.OutputReportByteLength + caps.FeatureReportByteLength);

    size = ALIGN_SIZE( size, sizeof(void *) );
    size += (caps.NumberInputValueCaps + caps.NumberOutputValueCaps + caps.NumberFeatureValueCaps) * sizeof(struct hid_extra_value_caps);

    hr = direct_input_device_alloc( size, &hid_joystick_vtbl, guid, dinput, (void **)&impl );
    if (FAILED(hr)) goto failed;

    /* impl->base.crit.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": hid_joystick.base.crit"); */
    impl->base.dwCoopLevel = DISCL_NONEXCLUSIVE | DISCL_BACKGROUND;
    impl->base.read_event = CreateEventA( NULL, FALSE, FALSE, NULL );
    impl->base.read_callback = hid_joystick_read_state;

    impl->device = device;
    impl->preparsed = preparsed;

    impl->instance = instance;
    lstrcpynW( impl->device_path, device_path, MAX_PATH );
    impl->attrs = attrs;
    impl->dev_caps = dev_caps;
    impl->caps = caps;

    impl->collection_nodes = ALIGN_PTR( impl + 1, sizeof(void *) );

    size = caps.NumberLinkCollectionNodes;
    if (HidP_GetLinkCollectionNodes( impl->collection_nodes, &size, preparsed ) != HIDP_STATUS_SUCCESS) goto failed;
    caps.NumberLinkCollectionNodes = size;

    impl->input_button_caps = ALIGN_PTR( impl->collection_nodes + caps.NumberLinkCollectionNodes, sizeof(void *) );
    impl->output_button_caps = impl->input_button_caps + caps.NumberInputButtonCaps;
    impl->feature_button_caps = impl->output_button_caps + caps.NumberOutputButtonCaps;

    HidP_GetButtonCaps( HidP_Input, impl->input_button_caps, &caps.NumberInputButtonCaps, preparsed );
    HidP_GetButtonCaps( HidP_Output, impl->output_button_caps, &caps.NumberOutputButtonCaps, preparsed );
    HidP_GetButtonCaps( HidP_Feature, impl->feature_button_caps, &caps.NumberFeatureButtonCaps, preparsed );

    impl->input_value_caps = ALIGN_PTR( impl->feature_button_caps + caps.NumberFeatureButtonCaps, sizeof(void *) );
    impl->output_value_caps = impl->input_value_caps + caps.NumberInputValueCaps;
    impl->feature_value_caps = impl->output_value_caps + caps.NumberOutputValueCaps;

    HidP_GetValueCaps( HidP_Input, impl->input_value_caps, &caps.NumberInputValueCaps, preparsed );
    HidP_GetValueCaps( HidP_Output, impl->output_value_caps, &caps.NumberOutputValueCaps, preparsed );
    HidP_GetValueCaps( HidP_Feature, impl->feature_value_caps, &caps.NumberFeatureValueCaps, preparsed );

    impl->usages_buf = ALIGN_PTR( impl->feature_value_caps + caps.NumberFeatureValueCaps, sizeof(void *) );
    impl->usages_count = HidP_MaxUsageListLength( HidP_Input, HID_USAGE_PAGE_BUTTON, preparsed );

    impl->input_report_buf = (char *)(impl->usages_buf + impl->usages_count);
    impl->output_report_buf = impl->input_report_buf + caps.InputReportByteLength;
    impl->feature_report_buf = impl->output_report_buf + caps.OutputReportByteLength;

    impl->extra_value_caps = ALIGN_PTR( impl->feature_report_buf + caps.FeatureReportByteLength, sizeof(void *) );

    if (!(format = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*format) ))) goto failed;

    enum_hid_objects( impl, &header, DIDFT_ALL, init_data_format, format, TRUE );
    if (!(obj_format = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, format->dwNumObjs * sizeof(*format->rgodf) ))) goto failed;

    format->dwSize = sizeof(*format);
    format->dwObjSize = sizeof(*format->rgodf);
    format->dwFlags = DIDF_ABSAXIS;
    format->dwDataSize = sizeof(impl->state);
    format->rgodf = obj_format;
    enum_hid_objects( impl, &header, DIDFT_ALL, init_data_format, format, FALSE );

    impl->base.data_format.wine_df = format;

    TRACE( "Created %p\n", impl );
    _dump_DIDATAFORMAT( impl->base.data_format.wine_df );

    range.diph.dwHow = DIPH_DEVICE;
    range.diph.dwObj = 0;
    range.lMin = 0;
    range.lMax = 65535;
    enum_hid_objects( impl, &range.diph, DIDFT_AXIS, set_property_prop_range, &range, FALSE );

    range.diph.dwHow = DIPH_DEVICE;
    range.diph.dwObj = 0;
    range.lMin = 0;
    range.lMax = 36000;
    enum_hid_objects( impl, &range.diph, DIDFT_POV, set_property_prop_range, &range, FALSE );

    *out = &impl->base.IDirectInputDevice8W_iface;
    return DI_OK;

failed:
    HeapFree( GetProcessHeap(), 0, obj_format );
    HeapFree( GetProcessHeap(), 0, format );
    HeapFree( GetProcessHeap(), 0, impl );
    HidD_FreePreparsedData( preparsed );
    CloseHandle( device );
    return hr;
}

const struct dinput_device joystick_hid_device =
{
    "Wine HID joystick driver",
    hid_joystick_enum_device,
    hid_joystick_create_device,
};
