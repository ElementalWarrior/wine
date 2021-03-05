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

#include <stdarg.h>

#include "mmdevapi.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mmdevapi);

DEFINE_GUID(nulldrv_render_guid,0x093cc70f,0xd22c,0x46ca,0x82,0x07,0xf5,0xfe,0xab,0x10,0x78,0x70);
DEFINE_GUID(nulldrv_capture_guid,0x4dab6710,0x0396,0x447a,0xb8,0x36,0x77,0x18,0x96,0x59,0xe0,0xce);

int WINAPI nulldrv_GetPriority(void)
{
    return Priority_Low;
}

HRESULT WINAPI nulldrv_GetEndpointIDs(EDataFlow flow, WCHAR ***ids, GUID **guids, UINT *num, UINT *default_index)
{
    static const WCHAR endpoint_name[] = L"null";
    WCHAR **id = NULL;
    GUID *guid = NULL;

    TRACE("flow %x, ids %p, guids %p, num %p, default_index %p.\n", flow, ids, guids, num, default_index);

    *num = 1;
    *default_index = 0;
    *ids = NULL;
    *guids = NULL;

    if (!(id = malloc(sizeof(*id)))) goto error;
    if (!(guid = malloc(sizeof(*guid)))) goto error;
    if (!(id[0] = malloc(sizeof(endpoint_name)))) goto error;

    memcpy(id[0], endpoint_name, sizeof(endpoint_name));
    if (flow == eRender) *guid = nulldrv_render_guid;
    else *guid = nulldrv_capture_guid;

    *ids = id;
    *guids = guid;
    return S_OK;

error:
    if (id && id[0]) free(id[0]);
    if (guid) free(guid);
    if (id) free(id);
    return E_OUTOFMEMORY;
}

HRESULT WINAPI nulldrv_GetAudioEndpoint(GUID *guid, IMMDevice *dev, IAudioClient **out)
{
    FIXME("guid %s, dev %p, out %p stub!\n", wine_dbgstr_guid(guid), dev, out);
    return E_NOTIMPL;
}

HRESULT WINAPI nulldrv_GetAudioSessionManager(IMMDevice *device, IAudioSessionManager2 **out)
{
    FIXME("device %p, out %p stub!\n", device, out);
    return E_NOTIMPL;
}

static inline const char *wine_dbgstr_propertykey(const PROPERTYKEY *key)
{
    return wine_dbg_sprintf("{%s,%d}", wine_dbgstr_guid(&key->fmtid), key->pid);
}

HRESULT WINAPI nulldrv_GetPropValue(GUID *guid, const PROPERTYKEY *prop, PROPVARIANT *out)
{
    FIXME("guid %s, prop %s, out %p stub!\n", wine_dbgstr_guid(guid), wine_dbgstr_propertykey(prop), out);
    return E_NOTIMPL;
}
