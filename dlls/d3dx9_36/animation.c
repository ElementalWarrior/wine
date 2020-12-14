/*
 * Animation Controller operations specific to D3DX9.
 *
 * Copyright (C) 2015 Christian Costa
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


#include "d3dx9_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3dx);

struct d3dx9_animation_controller
{
    ID3DXAnimationController ID3DXAnimationController_iface;
    LONG ref;

    UINT max_outputs;
    UINT max_sets;
    UINT max_tracks;
    UINT max_events;
};

static inline struct d3dx9_animation_controller *impl_from_ID3DXAnimationController(ID3DXAnimationController *iface)
{
    return CONTAINING_RECORD(iface, struct d3dx9_animation_controller, ID3DXAnimationController_iface);
}

static HRESULT WINAPI d3dx9_animation_controller_QueryInterface(ID3DXAnimationController *iface, REFIID riid, void **out)
{
    TRACE("iface %p, riid %s, out %p.\n", iface, debugstr_guid(riid), out);

    if (IsEqualGUID(riid, &IID_IUnknown) ||
        IsEqualGUID(riid, &IID_ID3DXAnimationController))
    {
        iface->lpVtbl->AddRef(iface);
        *out = iface;
        return D3D_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(riid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI d3dx9_animation_controller_AddRef(ID3DXAnimationController *iface)
{
    struct d3dx9_animation_controller *animation = impl_from_ID3DXAnimationController(iface);
    ULONG refcount = InterlockedIncrement(&animation->ref);

    TRACE("%p increasing refcount to %u.\n", animation, refcount);

    return refcount;
}

static ULONG WINAPI d3dx9_animation_controller_Release(ID3DXAnimationController *iface)
{
    struct d3dx9_animation_controller *animation = impl_from_ID3DXAnimationController(iface);
    ULONG refcount = InterlockedDecrement(&animation->ref);

    TRACE("%p decreasing refcount to %u.\n", animation, refcount);

    if (!refcount)
    {
        HeapFree(GetProcessHeap(), 0, animation);
    }

    return refcount;
}

static UINT WINAPI d3dx9_animation_controller_GetMaxNumAnimationOutputs(ID3DXAnimationController *iface)
{
    struct d3dx9_animation_controller *animation = impl_from_ID3DXAnimationController(iface);

    TRACE("iface %p.\n", iface);

    return animation->max_outputs;
}

static UINT WINAPI d3dx9_animation_controller_GetMaxNumAnimationSets(ID3DXAnimationController *iface)
{
    struct d3dx9_animation_controller *animation = impl_from_ID3DXAnimationController(iface);

    TRACE("iface %p.\n", iface);

    return animation->max_sets;
}

static UINT WINAPI d3dx9_animation_controller_GetMaxNumTracks(ID3DXAnimationController *iface)
{
    struct d3dx9_animation_controller *animation = impl_from_ID3DXAnimationController(iface);

    TRACE("iface %p.\n", iface);

    return animation->max_tracks;
}

static UINT WINAPI d3dx9_animation_controller_GetMaxNumEvents(ID3DXAnimationController *iface)
{
    struct d3dx9_animation_controller *animation = impl_from_ID3DXAnimationController(iface);

    TRACE("iface %p.\n", iface);

    return animation->max_events;
}

static HRESULT WINAPI d3dx9_animation_controller_RegisterAnimationOutput(ID3DXAnimationController *iface,
        const char *name, D3DXMATRIX *matrix, D3DXVECTOR3 *scale, D3DXQUATERNION *rotation, D3DXVECTOR3 *translation)
{
    FIXME("iface %p, name %s, matrix %p, scale %p, rotation %p, translation %p stub.\n", iface, debugstr_a(name),
            matrix, scale, rotation, translation);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_controller_RegisterAnimationSet(ID3DXAnimationController *iface,
        ID3DXAnimationSet *anim_set)
{
    FIXME("iface %p, anim_set %p stub.\n", iface, anim_set);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_controller_UnregisterAnimationSet(ID3DXAnimationController *iface,
        ID3DXAnimationSet *anim_set)
{
    FIXME("iface %p, anim_set %p stub.\n", iface, anim_set);

    return E_NOTIMPL;
}

static UINT WINAPI d3dx9_animation_controller_GetNumAnimationSets(ID3DXAnimationController *iface)
{
    FIXME("iface %p stub.\n", iface);

    return 0;
}

static HRESULT WINAPI d3dx9_animation_controller_GetAnimationSet(ID3DXAnimationController *iface,
        UINT index, ID3DXAnimationSet **anim_set)
{
    FIXME("iface %p, index %u, anim_set %p stub.\n", iface, index, anim_set);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_controller_GetAnimationSetByName(ID3DXAnimationController *iface,
        const char *name, ID3DXAnimationSet **anim_set)
{
    FIXME("iface %p, name %s, anim_set %p stub.\n", iface, debugstr_a(name), anim_set);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_controller_AdvanceTime(ID3DXAnimationController *iface, double time_delta,
        ID3DXAnimationCallbackHandler *callback_handler)
{
    FIXME("iface %p, time_delta %.16e, callback_handler %p stub.\n", iface, time_delta, callback_handler);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_controller_Reset(ID3DXAnimationController *iface)
{
    FIXME("iface %p stub.\n", iface);

    return E_NOTIMPL;
}

static double WINAPI d3dx9_animation_controller_GetTime(ID3DXAnimationController *iface)
{
    FIXME("iface %p stub.\n", iface);

    return 0.0;
}

static HRESULT WINAPI d3dx9_animation_controller_SetTrackAnimationSet(ID3DXAnimationController *iface,
        UINT track, ID3DXAnimationSet *anim_set)
{
    FIXME("iface %p, track %u, anim_set %p stub.\n", iface, track, anim_set);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_controller_GetTrackAnimationSet(ID3DXAnimationController *iface,
        UINT track, ID3DXAnimationSet **anim_set)
{
    FIXME("iface %p, track %u, anim_set %p stub.\n", iface, track, anim_set);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_controller_SetTrackPriority(ID3DXAnimationController *iface,
        UINT track, D3DXPRIORITY_TYPE priority)
{
    FIXME("iface %p, track %u, priority %u stub.\n", iface, track, priority);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_controller_SetTrackSpeed(ID3DXAnimationController *iface,
        UINT track, float speed)
{
    FIXME("iface %p, track %u, speed %.8e stub.\n", iface, track, speed);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_controller_SetTrackWeight(ID3DXAnimationController *iface,
        UINT track, float weight)
{
    FIXME("iface %p, track %u, weight %.8e stub.\n", iface, track, weight);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_controller_SetTrackPosition(ID3DXAnimationController *iface,
        UINT track, double position)
{
    FIXME("iface %p, track %u, position %.16e stub.\n", iface, track, position);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_controller_SetTrackEnable(ID3DXAnimationController *iface,
        UINT track, BOOL enable)
{
    FIXME("iface %p, track %u, enable %#x stub.\n", iface, track, enable);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_controller_SetTrackDesc(ID3DXAnimationController *iface,
        UINT track, D3DXTRACK_DESC *desc)
{
    FIXME("iface %p, track %u, desc %p stub.\n", iface, track, desc);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_controller_GetTrackDesc(ID3DXAnimationController *iface,
        UINT track, D3DXTRACK_DESC *desc)
{
    FIXME("iface %p, track %u, desc %p stub.\n", iface, track, desc);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_controller_SetPriorityBlend(ID3DXAnimationController *iface,
        float blend_weight)
{
    FIXME("iface %p, blend_weight %.8e stub.\n", iface, blend_weight);

    return E_NOTIMPL;
}

static float WINAPI d3dx9_animation_controller_GetPriorityBlend(ID3DXAnimationController *iface)
{
    FIXME("iface %p stub.\n", iface);

    return 0.0f;
}

static D3DXEVENTHANDLE WINAPI d3dx9_animation_controller_KeyTrackSpeed(ID3DXAnimationController *iface,
        UINT track, float new_speed, double start_time, double duration, D3DXTRANSITION_TYPE transition)
{
    FIXME("iface %p, track %u, new_speed %.8e, start_time %.16e, duration %.16e, transition %u stub.\n", iface,
            track, new_speed, start_time, duration, transition);

    return 0;
}

static D3DXEVENTHANDLE WINAPI d3dx9_animation_controller_KeyTrackWeight(ID3DXAnimationController *iface,
        UINT track, float new_weight, double start_time, double duration, D3DXTRANSITION_TYPE transition)
{
    FIXME("iface %p, track %u, new_weight %.8e, start_time %.16e, duration %.16e, transition %u stub.\n", iface,
            track, new_weight, start_time, duration, transition);

    return 0;
}

static D3DXEVENTHANDLE WINAPI d3dx9_animation_controller_KeyTrackPosition(ID3DXAnimationController *iface,
        UINT track, double new_position, double start_time)
{
    FIXME("iface %p, track %u, new_position %.16e, start_time %.16e stub.\n", iface,
            track, new_position, start_time);

    return 0;
}

static D3DXEVENTHANDLE WINAPI d3dx9_animation_controller_KeyTrackEnable(ID3DXAnimationController *iface,
        UINT track, BOOL new_enable, double start_time)
{
    FIXME("iface %p, track %u, new_enable %#x, start_time %.16e stub.\n", iface,
            track, new_enable, start_time);

    return 0;
}

static D3DXEVENTHANDLE WINAPI d3dx9_animation_controller_KeyTrackBlend(ID3DXAnimationController *iface,
        float new_blend_weight, double start_time, double duration, D3DXTRANSITION_TYPE transition)
{
    FIXME("iface %p, new_blend_weight %.8e, start_time %.16e, duration %.16e, transition %u stub.\n", iface,
            new_blend_weight, start_time, duration, transition);

    return 0;
}

static HRESULT WINAPI d3dx9_animation_controller_UnkeyEvent(ID3DXAnimationController *iface, D3DXEVENTHANDLE event)
{
    FIXME("iface %p, event %u stub.\n", iface, event);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_controller_UnkeyAllTrackEvents(ID3DXAnimationController *iface, UINT track)
{
    FIXME("iface %p, track %u stub.\n", iface, track);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_controller_UnkeyAllPriorityBlends(ID3DXAnimationController *iface)
{
    FIXME("iface %p stub.\n", iface);

    return E_NOTIMPL;
}

static D3DXEVENTHANDLE WINAPI d3dx9_animation_controller_GetCurrentTrackEvent(ID3DXAnimationController *iface,
        UINT track, D3DXEVENT_TYPE event_type)
{
    FIXME("iface %p, track %u, event_type %u stub.\n", iface, track, event_type);

    return 0;
}

static D3DXEVENTHANDLE WINAPI d3dx9_animation_controller_GetCurrentPriorityBlend(ID3DXAnimationController *iface)
{
    FIXME("iface %p stub.\n", iface);

    return 0;
}

static D3DXEVENTHANDLE WINAPI d3dx9_animation_controller_GetUpcomingTrackEvent(ID3DXAnimationController *iface,
        UINT track, D3DXEVENTHANDLE event)
{
    FIXME("iface %p, track %u, event %u stub.\n", iface, track, event);

    return 0;
}

static D3DXEVENTHANDLE WINAPI d3dx9_animation_controller_GetUpcomingPriorityBlend(ID3DXAnimationController *iface,
        D3DXEVENTHANDLE event)
{
    FIXME("iface %p, event %u stub.\n", iface, event);

    return 0;
}

static HRESULT WINAPI d3dx9_animation_controller_ValidateEvent(ID3DXAnimationController *iface, D3DXEVENTHANDLE event)
{
    FIXME("iface %p, event %u stub.\n", iface, event);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_controller_GetEventDesc(ID3DXAnimationController *iface,
        D3DXEVENTHANDLE event, D3DXEVENT_DESC *desc)
{
    FIXME("iface %p, event %u, desc %p stub.\n", iface, event, desc);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_controller_CloneAnimationController(ID3DXAnimationController *iface, UINT max_outputs,
        UINT max_sets, UINT max_tracks, UINT max_events, ID3DXAnimationController **anim_controller)
{
    FIXME("iface %p, max_outputs %u, max_sets %u, max_tracks %u, max_events %u, anim_controller %p stub.\n",
            iface, max_outputs, max_sets, max_tracks, max_events, anim_controller);

    return E_NOTIMPL;
}

static const struct ID3DXAnimationControllerVtbl d3dx9_animation_controller_vtbl =
{
    d3dx9_animation_controller_QueryInterface,
    d3dx9_animation_controller_AddRef,
    d3dx9_animation_controller_Release,
    d3dx9_animation_controller_GetMaxNumAnimationOutputs,
    d3dx9_animation_controller_GetMaxNumAnimationSets,
    d3dx9_animation_controller_GetMaxNumTracks,
    d3dx9_animation_controller_GetMaxNumEvents,
    d3dx9_animation_controller_RegisterAnimationOutput,
    d3dx9_animation_controller_RegisterAnimationSet,
    d3dx9_animation_controller_UnregisterAnimationSet,
    d3dx9_animation_controller_GetNumAnimationSets,
    d3dx9_animation_controller_GetAnimationSet,
    d3dx9_animation_controller_GetAnimationSetByName,
    d3dx9_animation_controller_AdvanceTime,
    d3dx9_animation_controller_Reset,
    d3dx9_animation_controller_GetTime,
    d3dx9_animation_controller_SetTrackAnimationSet,
    d3dx9_animation_controller_GetTrackAnimationSet,
    d3dx9_animation_controller_SetTrackPriority,
    d3dx9_animation_controller_SetTrackSpeed,
    d3dx9_animation_controller_SetTrackWeight,
    d3dx9_animation_controller_SetTrackPosition,
    d3dx9_animation_controller_SetTrackEnable,
    d3dx9_animation_controller_SetTrackDesc,
    d3dx9_animation_controller_GetTrackDesc,
    d3dx9_animation_controller_SetPriorityBlend,
    d3dx9_animation_controller_GetPriorityBlend,
    d3dx9_animation_controller_KeyTrackSpeed,
    d3dx9_animation_controller_KeyTrackWeight,
    d3dx9_animation_controller_KeyTrackPosition,
    d3dx9_animation_controller_KeyTrackEnable,
    d3dx9_animation_controller_KeyTrackBlend,
    d3dx9_animation_controller_UnkeyEvent,
    d3dx9_animation_controller_UnkeyAllTrackEvents,
    d3dx9_animation_controller_UnkeyAllPriorityBlends,
    d3dx9_animation_controller_GetCurrentTrackEvent,
    d3dx9_animation_controller_GetCurrentPriorityBlend,
    d3dx9_animation_controller_GetUpcomingTrackEvent,
    d3dx9_animation_controller_GetUpcomingPriorityBlend,
    d3dx9_animation_controller_ValidateEvent,
    d3dx9_animation_controller_GetEventDesc,
    d3dx9_animation_controller_CloneAnimationController
};

HRESULT WINAPI D3DXCreateAnimationController(UINT max_outputs, UINT max_sets,
        UINT max_tracks, UINT max_events, ID3DXAnimationController **controller)
{
    struct d3dx9_animation_controller *object;

    TRACE("max_outputs %u, max_sets %u, max_tracks %u, max_events %u, controller %p.\n",
            max_outputs, max_sets, max_tracks, max_events, controller);

    if (!max_outputs || !max_sets || !max_tracks || !max_events || !controller)
        return D3D_OK;

    object = HeapAlloc(GetProcessHeap(), 0, sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    object->ID3DXAnimationController_iface.lpVtbl = &d3dx9_animation_controller_vtbl;
    object->ref = 1;
    object->max_outputs = max_outputs;
    object->max_sets    = max_sets;
    object->max_tracks  = max_tracks;
    object->max_events  = max_events;

    *controller = &object->ID3DXAnimationController_iface;

    return D3D_OK;
}

struct d3dx9_animation_frame_set
{
    ID3DXKeyframedAnimationSet ID3DXKeyframedAnimationSet_iface;
    LONG ref;

    char *name;
    double ticks_per_second;
    D3DXPLAYBACK_TYPE playback_type;
    UINT animation_count;
    UINT callback_key_count;
    const D3DXKEY_CALLBACK *callback_keys;
};

static inline struct d3dx9_animation_frame_set *impl_from_ID3DXKeyframedAnimationSet(ID3DXKeyframedAnimationSet *iface)
{
    return CONTAINING_RECORD(iface, struct d3dx9_animation_frame_set, ID3DXKeyframedAnimationSet_iface);
}

static HRESULT WINAPI d3dx9_animation_framed_QueryInterface(ID3DXKeyframedAnimationSet *iface, REFIID riid, void **obj)
{
    TRACE("iface %p, riid %s, out %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualGUID(riid, &IID_IUnknown) ||
        IsEqualGUID(riid, &IID_ID3DXAnimationSet) ||
        IsEqualGUID(riid, &IID_ID3DXKeyframedAnimationSet))
    {
        iface->lpVtbl->AddRef(iface);
        *obj = iface;
        return D3D_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI d3dx9_animation_framed_AddRef(ID3DXKeyframedAnimationSet *iface)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    ULONG refcount = InterlockedIncrement(&framed->ref);

    TRACE("%p increasing refcount to %u.\n", framed, refcount);

    return refcount;
}

static ULONG WINAPI d3dx9_animation_framed_Release(ID3DXKeyframedAnimationSet *iface)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    ULONG refcount = InterlockedDecrement(&framed->ref);

    TRACE("%p decreasing refcount to %u.\n", framed, refcount);

    if (!refcount)
    {
        heap_free(framed->name);
        HeapFree(GetProcessHeap(), 0, framed);
    }

    return refcount;
}

static const char * WINAPI d3dx9_animation_framed_GetName(ID3DXKeyframedAnimationSet *iface)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    TRACE("framed %p.\n", framed);
    return framed->name;
}

static DOUBLE WINAPI d3dx9_animation_framed_GetPeriod(ID3DXKeyframedAnimationSet *iface)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p stub.\n", framed);
    return 0.0f;
}

static DOUBLE WINAPI d3dx9_animation_framed_GetPeriodicPosition(ID3DXKeyframedAnimationSet *iface, DOUBLE position)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p stub.\n", framed);
    return 0.0f;
}

static UINT WINAPI d3dx9_animation_framed_GetNumAnimations(ID3DXKeyframedAnimationSet *iface)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p stub.\n", framed);
    return 0;
}

static HRESULT WINAPI d3dx9_animation_framed_GetAnimationNameByIndex(ID3DXKeyframedAnimationSet *iface, UINT index, const char **name)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, name %p stub.\n", framed, name);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_GetAnimationIndexByName(ID3DXKeyframedAnimationSet *iface, const char *name, UINT *index)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, name %s, index %p stub.\n", framed, debugstr_a(name), index);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_GetSRT(ID3DXKeyframedAnimationSet *iface, DOUBLE periodic_position, UINT animation, D3DXVECTOR3 *scale,
        D3DXQUATERNION *rotation, D3DXVECTOR3 *translation)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, periodic_position %f, animation %u, scale %p rotation %p translation %p stub.\n",
            framed, periodic_position, animation, scale, rotation, translation);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_GetCallback(ID3DXKeyframedAnimationSet *iface, double position, DWORD flags, double *callback_position,
        void **callback_data)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, position %f, flags 0x%08x, callback_position %p, callback_data %p stub.\n",
                framed, position, flags, callback_position, callback_data);
    return E_NOTIMPL;
}

static D3DXPLAYBACK_TYPE WINAPI d3dx9_animation_framed_GetPlaybackType(ID3DXKeyframedAnimationSet *iface)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    TRACE("framed %p.\n", framed);
    return framed->playback_type;
}

static DOUBLE WINAPI d3dx9_animation_framed_GetSourceTicksPerSecond(ID3DXKeyframedAnimationSet *iface)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    TRACE("framed %p.\n", framed);
    return framed->ticks_per_second;
}

static UINT WINAPI d3dx9_animation_framed_GetNumScaleKeys(ID3DXKeyframedAnimationSet *iface, UINT keys)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, keys %u stub.\n", framed, keys);
    return 0;
}

static HRESULT WINAPI d3dx9_animation_framed_GetScaleKeys(ID3DXKeyframedAnimationSet *iface, UINT animation, LPD3DXKEY_VECTOR3 scale_keys)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, animation %u, scale_keys %p stub.\n", framed, animation, scale_keys);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_GetScaleKey(ID3DXKeyframedAnimationSet *iface, UINT animation, UINT key, LPD3DXKEY_VECTOR3 scale_key)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, animation %u, key %u, scale_key %p stub.\n", framed, animation, key, scale_key);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_SetScaleKey(ID3DXKeyframedAnimationSet *iface, UINT animation, UINT key, LPD3DXKEY_VECTOR3 scale_key)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, animation %u, key %u, scale_key %p stub.\n", framed, animation, key, scale_key);
    return E_NOTIMPL;
}

static UINT WINAPI d3dx9_animation_framed_GetNumRotationKeys(ID3DXKeyframedAnimationSet *iface, UINT animation)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, animation %u stub.\n", framed, animation);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_GetRotationKeys(ID3DXKeyframedAnimationSet *iface, UINT animation, LPD3DXKEY_QUATERNION rotation_keys)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, animation %u, rotation_keys %p stub.\n", framed, animation, rotation_keys);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_GetRotationKey(ID3DXKeyframedAnimationSet *iface, UINT animation, UINT key, LPD3DXKEY_QUATERNION rotation_key)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, animation %u, key %u, rotation_key %p stub.\n", framed, animation, key, rotation_key);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_SetRotationKey(ID3DXKeyframedAnimationSet *iface, UINT animation, UINT key, LPD3DXKEY_QUATERNION rotation_key)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, animation %u, key %u, rotation_key %p stub.\n", framed, animation, key, rotation_key);
    return E_NOTIMPL;
}

static UINT WINAPI d3dx9_animation_framed_GetNumTranslationKeys(ID3DXKeyframedAnimationSet *iface, UINT animation)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, animation %u stub.\n", framed, animation);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_GetTranslationKeys(ID3DXKeyframedAnimationSet *iface, UINT animation, LPD3DXKEY_VECTOR3 translation_keys)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, animation %u, rotation_key %p stub.\n", framed, animation, translation_keys);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_GetTranslationKey(ID3DXKeyframedAnimationSet *iface, UINT animation, UINT key, LPD3DXKEY_VECTOR3 translation_key)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, animation %u, key %u, translation_key %p stub.\n", framed, animation, key, translation_key);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_SetTranslationKey(ID3DXKeyframedAnimationSet *iface, UINT animation, UINT key, LPD3DXKEY_VECTOR3 translation_key)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, animation %u, key %u, translation_key %p stub.\n", framed, animation, key, translation_key);
    return E_NOTIMPL;
}

static UINT WINAPI d3dx9_animation_framed_GetNumCallbackKeys(ID3DXKeyframedAnimationSet *iface)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p stub.\n", framed);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_GetCallbackKeys(ID3DXKeyframedAnimationSet *iface, LPD3DXKEY_CALLBACK callback_keys)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, callback_keys %p stub.\n", framed, callback_keys);

    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_GetCallbackKey(ID3DXKeyframedAnimationSet *iface, UINT key, LPD3DXKEY_CALLBACK callback_key)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, key %u, callback_key %p stub.\n", framed, key, callback_key);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_SetCallbackKey(ID3DXKeyframedAnimationSet *iface, UINT key, LPD3DXKEY_CALLBACK callback_key)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, key %u, callback_key %p stub.\n", framed, key, callback_key);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_UnregisterScaleKey(ID3DXKeyframedAnimationSet *iface, UINT animation, UINT key)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, animation %u, key %u stub.\n", framed, animation, key);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_UnregisterRotationKey(ID3DXKeyframedAnimationSet *iface, UINT animation, UINT key)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, animation %u, key %u stub.\n", framed, animation, key);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_UnregisterTranslationKey(ID3DXKeyframedAnimationSet *iface, UINT animation, UINT key)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, animation %u, key %u stub.\n", framed, animation, key);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_RegisterAnimationSRTKeys(ID3DXKeyframedAnimationSet *iface, const char *name, UINT num_scale_keys,
        UINT num_rotation_keys, UINT num_translation_keys, const D3DXKEY_VECTOR3 *scale_keys,
        const D3DXKEY_QUATERNION *rotation_keys, const D3DXKEY_VECTOR3 *translation_keys,
        DWORD *animation_index)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, name %s, num_scale_keys %u, num_rotation_keys %u, rotation_keys %p, num_translation_keys %u, scale_keys %p, "
           "rotation_keys %p, translation_keys %p, animation_index %p stub.\n",
                framed, debugstr_a(name), num_scale_keys, num_rotation_keys, rotation_keys, num_translation_keys, scale_keys,
                rotation_keys, translation_keys, animation_index);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_Compress(ID3DXKeyframedAnimationSet *iface, DWORD flags, float lossiness,
        D3DXFRAME *hierarchy, ID3DXBuffer **compressed_data)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, flags 0x%08x, lossiness %f, hierarchy %p, compressed_data %p stub.\n", framed, flags, lossiness,
            hierarchy, compressed_data);
    return E_NOTIMPL;
}

static HRESULT WINAPI d3dx9_animation_framed_UnregisterAnimation(ID3DXKeyframedAnimationSet *iface, UINT index)
{
    struct d3dx9_animation_frame_set *framed = impl_from_ID3DXKeyframedAnimationSet(iface);
    FIXME("framed %p, index %u stub.\n", framed, index);
    return E_NOTIMPL;
}

static const struct ID3DXKeyframedAnimationSetVtbl d3dx9_animation_framed_vtbl =
{
    d3dx9_animation_framed_QueryInterface,
    d3dx9_animation_framed_AddRef,
    d3dx9_animation_framed_Release,
    d3dx9_animation_framed_GetName,
    d3dx9_animation_framed_GetPeriod,
    d3dx9_animation_framed_GetPeriodicPosition,
    d3dx9_animation_framed_GetNumAnimations,
    d3dx9_animation_framed_GetAnimationNameByIndex,
    d3dx9_animation_framed_GetAnimationIndexByName,
    d3dx9_animation_framed_GetSRT,
    d3dx9_animation_framed_GetCallback,
    d3dx9_animation_framed_GetPlaybackType,
    d3dx9_animation_framed_GetSourceTicksPerSecond,
    d3dx9_animation_framed_GetNumScaleKeys,
    d3dx9_animation_framed_GetScaleKeys,
    d3dx9_animation_framed_GetScaleKey,
    d3dx9_animation_framed_SetScaleKey,
    d3dx9_animation_framed_GetNumRotationKeys,
    d3dx9_animation_framed_GetRotationKeys,
    d3dx9_animation_framed_GetRotationKey,
    d3dx9_animation_framed_SetRotationKey,
    d3dx9_animation_framed_GetNumTranslationKeys,
    d3dx9_animation_framed_GetTranslationKeys,
    d3dx9_animation_framed_GetTranslationKey,
    d3dx9_animation_framed_SetTranslationKey,
    d3dx9_animation_framed_GetNumCallbackKeys,
    d3dx9_animation_framed_GetCallbackKeys,
    d3dx9_animation_framed_GetCallbackKey,
    d3dx9_animation_framed_SetCallbackKey,
    d3dx9_animation_framed_UnregisterScaleKey,
    d3dx9_animation_framed_UnregisterRotationKey,
    d3dx9_animation_framed_UnregisterTranslationKey,
    d3dx9_animation_framed_RegisterAnimationSRTKeys,
    d3dx9_animation_framed_Compress,
    d3dx9_animation_framed_UnregisterAnimation
};

HRESULT WINAPI D3DXCreateKeyframedAnimationSet(const char *name, double ticks_per_second,
        D3DXPLAYBACK_TYPE playback_type, UINT animation_count, UINT callback_key_count,
        const D3DXKEY_CALLBACK *callback_keys, ID3DXKeyframedAnimationSet **animation_set)
{
    struct d3dx9_animation_frame_set *object;

    TRACE("name %s, ticks_per_second %.16e, playback_type %u, animation_count %u, "
            "callback_key_count %u, callback_keys %p, animation_set %p.\n",
            debugstr_a(name), ticks_per_second, playback_type, animation_count,
            callback_key_count, callback_keys, animation_set);

    if(!animation_count)
        return D3DERR_INVALIDCALL;

    object = heap_alloc(sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    object->ID3DXKeyframedAnimationSet_iface.lpVtbl = &d3dx9_animation_framed_vtbl;
    object->ref = 1;
    object->name = heap_alloc( strlen(name)+1 );
    if(!object->name)
    {
        heap_free(object);
        return E_OUTOFMEMORY;
    }
    strcpy(object->name, name);
    object->ticks_per_second   = ticks_per_second;
    object->playback_type      = playback_type;
    object->animation_count    = animation_count;
    object->callback_key_count = callback_key_count;
    object->callback_keys      = callback_keys;

    *animation_set = &object->ID3DXKeyframedAnimationSet_iface;

    return D3D_OK;
}
