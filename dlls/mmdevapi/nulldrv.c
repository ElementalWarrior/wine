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

int WINAPI nulldrv_GetPriority(void)
{
    return Priority_Low;
}

HRESULT WINAPI nulldrv_GetEndpointIDs(EDataFlow flow, WCHAR ***ids, GUID **guids, UINT *num, UINT *default_index)
{
    FIXME("flow %x, ids %p, guids %p, num %p, default_index %p stub!\n", flow, ids, guids, num, default_index);
    return E_NOTIMPL;
}

HRESULT WINAPI nulldrv_GetAudioEndpoint(void *key, IMMDevice *dev, IAudioClient **out)
{
    FIXME("key %p, dev %p, out %p stub!\n", key, dev, out);
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
