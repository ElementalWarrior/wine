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

#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winreg.h"

#define COBJMACROS
#include "audioclient.h"
#include "audiopolicy.h"
#include "endpointvolume.h"
#include "mmdeviceapi.h"
#include "spatialaudioclient.h"

#include "initguid.h"

#include "wine/debug.h"

#include "mmdevapi.h"

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

struct audio_client
{
    IAudioClient3 IAudioClient3_iface;
    LONG refcount;

    EDataFlow dataflow;
    IMMDevice *parent;
    IUnknown *marshal;
};

struct audio_client *impl_from_IAudioClient3(IAudioClient3 *iface)
{
    return CONTAINING_RECORD(iface, struct audio_client, IAudioClient3_iface);
}

static HRESULT STDMETHODCALLTYPE audio_client_QueryInterface(
        IAudioClient3 *iface, REFIID iid, void **object)
{
    struct audio_client *impl = impl_from_IAudioClient3(iface);

    TRACE("iface %p, iid %s, object %p stub!\n", iface, debugstr_guid(iid), object);

    if (IsEqualIID(iid, &IID_IUnknown) ||
        IsEqualIID(iid, &IID_IAudioClient) ||
        IsEqualIID(iid, &IID_IAudioClient2) ||
        IsEqualIID(iid, &IID_IAudioClient3))
    {
        IAudioClient3_AddRef(iface);
        *object = iface;
        return S_OK;
    }

    if (IsEqualIID(iid, &IID_IMarshal))
        return IUnknown_QueryInterface(impl->marshal, iid, object);

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *object = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE audio_client_AddRef(
        IAudioClient3 *iface)
{
    struct audio_client *impl = impl_from_IAudioClient3(iface);
    ULONG rc = InterlockedIncrement(&impl->refcount);

    TRACE("%p increasing refcount to %u.\n", impl, rc);

    return rc;
}

static ULONG STDMETHODCALLTYPE audio_client_Release(
        IAudioClient3 *iface)
{
    struct audio_client *impl = impl_from_IAudioClient3(iface);
    ULONG rc = InterlockedDecrement(&impl->refcount);

    TRACE("%p decreasing refcount to %u.\n", impl, rc);

    if (!rc)
    {
        IAudioClient3_Stop(iface);
        IMMDevice_Release(impl->parent);
        IUnknown_Release(impl->marshal);
        free(impl);
    }

    return rc;
}

static HRESULT STDMETHODCALLTYPE audio_client_Initialize(
        IAudioClient3 *iface, AUDCLNT_SHAREMODE mode, DWORD flags, REFERENCE_TIME duration,
        REFERENCE_TIME period, const WAVEFORMATEX *format, const GUID *session_guid)
{
    FIXME("iface %p, mode %x, flags %x, duration %I64x, period %I64x, format %p, session_guid %s stub!\n",
          iface, mode, flags, duration, period, format, wine_dbgstr_guid(session_guid) );
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE audio_client_GetBufferSize(IAudioClient3 *iface,
        UINT32 *frame_count)
{
    FIXME("iface %p, frame_count %p stub!\n", iface, frame_count);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE audio_client_GetStreamLatency(IAudioClient3 *iface,
        REFERENCE_TIME *latency)
{
    FIXME("iface %p, latency %p stub!\n", iface, latency);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE audio_client_GetCurrentPadding(IAudioClient3 *iface,
        UINT32 *frame_count)
{
    FIXME("iface %p, frame_count %p stub!\n", iface, frame_count);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE audio_client_IsFormatSupported(IAudioClient3 *iface,
        AUDCLNT_SHAREMODE mode, const WAVEFORMATEX *format, WAVEFORMATEX **closest_match)
{
    FIXME("iface %p, mode %x, format %p, closest_match %p stub!\n",
          iface, mode, format, closest_match);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE audio_client_GetMixFormat(IAudioClient3 *iface,
        WAVEFORMATEX **format)
{
    FIXME("iface %p, format %p stub!\n", iface, format);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE audio_client_GetDevicePeriod(IAudioClient3 *iface,
        REFERENCE_TIME *default_period, REFERENCE_TIME *minimum_period)
{
    FIXME("iface %p, default_period %p, minimum_period %p stub!\n",
          iface, default_period, minimum_period);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE audio_client_Start(IAudioClient3 *iface)
{
    FIXME("iface %p stub!\n", iface);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE audio_client_Stop(IAudioClient3 *iface)
{
    FIXME("iface %p stub!\n", iface);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE audio_client_Reset(IAudioClient3 *iface)
{
    FIXME("iface %p stub!\n", iface);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE audio_client_SetEventHandle(IAudioClient3 *iface,
        HANDLE event)
{
    FIXME("iface %p, event %p stub!\n", iface, event);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE audio_client_GetService(IAudioClient3 *iface,
        REFIID iid, void **object)
{
    FIXME("iface %p, iid %s, object %p stub!\n", iface, wine_dbgstr_guid(iid), object);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE audio_client_IsOffloadCapable(IAudioClient3 *iface,
        AUDIO_STREAM_CATEGORY category, BOOL *offload_capable)
{
    FIXME("iface %p, category %x, offload_capable %p stub!\n", iface, category, offload_capable);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE audio_client_SetClientProperties(IAudioClient3 *iface,
        const AudioClientProperties *prop)
{
    FIXME("iface %p, prop %p stub!\n", iface, prop);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE audio_client_GetBufferSizeLimits(IAudioClient3 *iface,
        const WAVEFORMATEX *format, BOOL event_driven, REFERENCE_TIME *min_duration,
        REFERENCE_TIME *max_duration)
{
    FIXME("iface %p, format %p, event_driven %d, min_duration %p, max_duration %p stub!\n",
          iface, format, event_driven, min_duration, max_duration);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE audio_client_GetSharedModeEnginePeriod(IAudioClient3 *iface,
        const WAVEFORMATEX *format, UINT32 *default_period_frames, UINT32 *unit_period_frames,
        UINT32 *min_period_frames, UINT32 *max_period_frames)
{
    FIXME("iface %p, format %p, default_period_frames %p, unit_period_frames %p, min_period_frames %p, max_period_frames %p stub!\n",
          iface, format, default_period_frames, unit_period_frames, min_period_frames, max_period_frames);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE audio_client_GetCurrentSharedModeEnginePeriod(IAudioClient3 *iface,
        WAVEFORMATEX **cur_format, UINT32 *cur_period_frames)
{
    FIXME("iface %p, cur_format %p, cur_period_frames %p stub!\n",
          iface, cur_format, cur_period_frames);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE audio_client_InitializeSharedAudioStream(IAudioClient3 *iface,
        DWORD flags, UINT32 period_frames, const WAVEFORMATEX *format, const GUID *session_guid)
{
    FIXME("iface %p, flags %x, period_frames %d, format %p, session_guid %s stub!\n",
          iface, flags, period_frames, format, wine_dbgstr_guid(session_guid));
    return E_NOTIMPL;
}

static const IAudioClient3Vtbl audio_client_vtbl =
{
    audio_client_QueryInterface,
    audio_client_AddRef,
    audio_client_Release,
    /*** IAudioClient methods ***/
    audio_client_Initialize,
    audio_client_GetBufferSize,
    audio_client_GetStreamLatency,
    audio_client_GetCurrentPadding,
    audio_client_IsFormatSupported,
    audio_client_GetMixFormat,
    audio_client_GetDevicePeriod,
    audio_client_Start,
    audio_client_Stop,
    audio_client_Reset,
    audio_client_SetEventHandle,
    audio_client_GetService,
    /*** IAudioClient2 methods ***/
    audio_client_IsOffloadCapable,
    audio_client_SetClientProperties,
    audio_client_GetBufferSizeLimits,
    /*** IAudioClient3 methods ***/
    audio_client_GetSharedModeEnginePeriod,
    audio_client_GetCurrentSharedModeEnginePeriod,
    audio_client_InitializeSharedAudioStream,
};

HRESULT WINAPI nulldrv_GetAudioEndpoint(GUID *guid, IMMDevice *dev, IAudioClient **out)
{
    struct audio_client *impl;
    EDataFlow dataflow;
    HRESULT hr;

    TRACE("guid %s, dev %p, out %p.\n", wine_dbgstr_guid(guid), dev, out);

    if (IsEqualGUID(guid, &nulldrv_render_guid)) dataflow = eRender;
    else if (IsEqualGUID(guid, &nulldrv_capture_guid)) dataflow = eCapture;
    else return E_UNEXPECTED;

    *out = NULL;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;

    impl->IAudioClient3_iface.lpVtbl = &audio_client_vtbl;
    impl->refcount = 1;
    impl->dataflow = dataflow;
    impl->parent = dev;

    if (FAILED(hr = CoCreateFreeThreadedMarshaler((IUnknown *)&impl->IAudioClient3_iface, &impl->marshal)))
    {
        free(impl);
        return hr;
    }

    IMMDevice_AddRef(impl->parent);

    *out = (IAudioClient *)&impl->IAudioClient3_iface;
    return S_OK;
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
