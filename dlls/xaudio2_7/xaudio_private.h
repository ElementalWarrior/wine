/*
 * Copyright (c) 2015 Andrew Eikum for CodeWeavers
 * Copyright (c) 2018 Ethan Lee for CodeWeavers
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

#include "windef.h"
#include "wine/list.h"

#include "xaudio2.h"
#include "xapo.h"

#include <F3DAudio.h>
#include <FACT.h>
#include <FACT3D.h>
#include <FAPO.h>
#include <FAPOBase.h>
#include <FAPOFX.h>
#include <FAudio.h>
#include <FAudioFX.h>

#include <pthread.h>

#define MAKE_FUNCPTR(f) extern typeof(f) * p##f DECLSPEC_HIDDEN;
MAKE_FUNCPTR(FAudio_AddRef)
#ifdef HAVE_FAUDIO_COMMITOPERATIONSET
MAKE_FUNCPTR(FAudio_CommitOperationSet)
#else
MAKE_FUNCPTR(FAudio_CommitChanges)
#endif
MAKE_FUNCPTR(FAudio_CreateMasteringVoice)
MAKE_FUNCPTR(FAudio_CreateMasteringVoice8)
MAKE_FUNCPTR(FAudio_CreateSourceVoice)
MAKE_FUNCPTR(FAudio_CreateSubmixVoice)
MAKE_FUNCPTR(FAudio_GetDeviceCount)
MAKE_FUNCPTR(FAudio_GetDeviceDetails)
MAKE_FUNCPTR(FAudio_GetPerformanceData)
MAKE_FUNCPTR(FAudio_Initialize)
MAKE_FUNCPTR(FAudio_RegisterForCallbacks)
MAKE_FUNCPTR(FAudio_Release)
MAKE_FUNCPTR(FAudio_SetDebugConfiguration)
MAKE_FUNCPTR(FAudio_StartEngine)
MAKE_FUNCPTR(FAudio_StopEngine)

MAKE_FUNCPTR(FAudioVoice_DestroyVoice)
MAKE_FUNCPTR(FAudioVoice_DisableEffect)
MAKE_FUNCPTR(FAudioVoice_EnableEffect)
MAKE_FUNCPTR(FAudioVoice_GetChannelVolumes)
MAKE_FUNCPTR(FAudioVoice_GetEffectParameters)
MAKE_FUNCPTR(FAudioVoice_GetEffectState)
MAKE_FUNCPTR(FAudioVoice_GetFilterParameters)
MAKE_FUNCPTR(FAudioVoice_GetOutputFilterParameters)
MAKE_FUNCPTR(FAudioVoice_GetOutputMatrix)
MAKE_FUNCPTR(FAudioVoice_GetVoiceDetails)
MAKE_FUNCPTR(FAudioVoice_GetVolume)
MAKE_FUNCPTR(FAudioVoice_SetChannelVolumes)
MAKE_FUNCPTR(FAudioVoice_SetEffectChain)
MAKE_FUNCPTR(FAudioVoice_SetEffectParameters)
MAKE_FUNCPTR(FAudioVoice_SetFilterParameters)
MAKE_FUNCPTR(FAudioVoice_SetOutputFilterParameters)
MAKE_FUNCPTR(FAudioVoice_SetOutputMatrix)
MAKE_FUNCPTR(FAudioVoice_SetOutputVoices)
MAKE_FUNCPTR(FAudioVoice_SetVolume)

MAKE_FUNCPTR(FAudioSourceVoice_Discontinuity)
MAKE_FUNCPTR(FAudioSourceVoice_ExitLoop)
MAKE_FUNCPTR(FAudioSourceVoice_FlushSourceBuffers)
MAKE_FUNCPTR(FAudioSourceVoice_GetFrequencyRatio)
MAKE_FUNCPTR(FAudioSourceVoice_GetState)
MAKE_FUNCPTR(FAudioSourceVoice_SetFrequencyRatio)
MAKE_FUNCPTR(FAudioSourceVoice_SetSourceSampleRate)
MAKE_FUNCPTR(FAudioSourceVoice_Start)
MAKE_FUNCPTR(FAudioSourceVoice_Stop)
MAKE_FUNCPTR(FAudioSourceVoice_SubmitSourceBuffer)

MAKE_FUNCPTR(FAudioMasteringVoice_GetChannelMask)

MAKE_FUNCPTR(FAudioCOMConstructWithCustomAllocatorEXT)
MAKE_FUNCPTR(FAudioCreate)
MAKE_FUNCPTR(FAudioCreateReverb)
MAKE_FUNCPTR(FAudioCreateReverb9)
#ifdef HAVE_FAUDIOCREATEREVERB9WITHCUSTOMALLOCATOREXT
MAKE_FUNCPTR(FAudioCreateReverb9WithCustomAllocatorEXT)
#endif
MAKE_FUNCPTR(FAudioCreateReverbWithCustomAllocatorEXT)
MAKE_FUNCPTR(FAudioCreateVolumeMeter)
MAKE_FUNCPTR(FAudioCreateVolumeMeterWithCustomAllocatorEXT)
#ifdef HAVE_FAUDIOLINKEDVERSION
MAKE_FUNCPTR(FAudioLinkedVersion)
#endif

MAKE_FUNCPTR(F3DAudioCalculate)
MAKE_FUNCPTR(F3DAudioInitialize)
#ifdef HAVE_F3DAUDIOINITIALIZE8
MAKE_FUNCPTR(F3DAudioInitialize8)
#endif

MAKE_FUNCPTR(FACTAudioEngine_AddRef)
MAKE_FUNCPTR(FACTAudioEngine_CreateInMemoryWaveBank)
MAKE_FUNCPTR(FACTAudioEngine_CreateSoundBank)
MAKE_FUNCPTR(FACTAudioEngine_CreateStreamingWaveBank)
MAKE_FUNCPTR(FACTAudioEngine_DoWork)
MAKE_FUNCPTR(FACTAudioEngine_GetCategory)
MAKE_FUNCPTR(FACTAudioEngine_GetFinalMixFormat)
MAKE_FUNCPTR(FACTAudioEngine_GetGlobalVariable)
MAKE_FUNCPTR(FACTAudioEngine_GetGlobalVariableIndex)
MAKE_FUNCPTR(FACTAudioEngine_GetRendererCount)
MAKE_FUNCPTR(FACTAudioEngine_GetRendererDetails)
MAKE_FUNCPTR(FACTAudioEngine_Initialize)
MAKE_FUNCPTR(FACTAudioEngine_Pause)
MAKE_FUNCPTR(FACTAudioEngine_PrepareWave)
MAKE_FUNCPTR(FACTAudioEngine_RegisterNotification)
MAKE_FUNCPTR(FACTAudioEngine_Release)
MAKE_FUNCPTR(FACTAudioEngine_SetGlobalVariable)
MAKE_FUNCPTR(FACTAudioEngine_SetVolume)
MAKE_FUNCPTR(FACTAudioEngine_ShutDown)
MAKE_FUNCPTR(FACTAudioEngine_Stop)
MAKE_FUNCPTR(FACTAudioEngine_UnRegisterNotification)

MAKE_FUNCPTR(FACTCreateEngineWithCustomAllocatorEXT)

MAKE_FUNCPTR(FACTCue_Destroy)
MAKE_FUNCPTR(FACTCue_GetProperties)
MAKE_FUNCPTR(FACTCue_GetState)
MAKE_FUNCPTR(FACTCue_GetVariable)
MAKE_FUNCPTR(FACTCue_GetVariableIndex)
MAKE_FUNCPTR(FACTCue_Pause)
MAKE_FUNCPTR(FACTCue_Play)
MAKE_FUNCPTR(FACTCue_SetMatrixCoefficients)
MAKE_FUNCPTR(FACTCue_SetVariable)
MAKE_FUNCPTR(FACTCue_Stop)

MAKE_FUNCPTR(FACTSoundBank_Destroy)
MAKE_FUNCPTR(FACTSoundBank_GetCueIndex)
MAKE_FUNCPTR(FACTSoundBank_GetCueProperties)
MAKE_FUNCPTR(FACTSoundBank_GetNumCues)
MAKE_FUNCPTR(FACTSoundBank_GetState)
MAKE_FUNCPTR(FACTSoundBank_Play)
MAKE_FUNCPTR(FACTSoundBank_Prepare)
MAKE_FUNCPTR(FACTSoundBank_Stop)

MAKE_FUNCPTR(FACTWave_Destroy)
MAKE_FUNCPTR(FACTWave_GetProperties)
MAKE_FUNCPTR(FACTWave_GetState)
MAKE_FUNCPTR(FACTWave_Pause)
MAKE_FUNCPTR(FACTWave_Play)
MAKE_FUNCPTR(FACTWave_SetMatrixCoefficients)
MAKE_FUNCPTR(FACTWave_SetPitch)
MAKE_FUNCPTR(FACTWave_SetVolume)
MAKE_FUNCPTR(FACTWave_Stop)

MAKE_FUNCPTR(FACTWaveBank_Destroy)
MAKE_FUNCPTR(FACTWaveBank_GetNumWaves)
MAKE_FUNCPTR(FACTWaveBank_GetState)
MAKE_FUNCPTR(FACTWaveBank_GetWaveIndex)
MAKE_FUNCPTR(FACTWaveBank_GetWaveProperties)
MAKE_FUNCPTR(FACTWaveBank_Play)
MAKE_FUNCPTR(FACTWaveBank_Prepare)
MAKE_FUNCPTR(FACTWaveBank_Stop)

MAKE_FUNCPTR(FAPOFX_CreateFXWithCustomAllocatorEXT)
#undef MAKE_FUNCPTR

extern BOOL load_faudio(void) DECLSPEC_HIDDEN;
extern void unload_faudio(void) DECLSPEC_HIDDEN;

#if XAUDIO2_VER == 0
#define COMPAT_E_INVALID_CALL E_INVALIDARG
#define COMPAT_E_DEVICE_INVALIDATED XAUDIO20_E_DEVICE_INVALIDATED
#else
#define COMPAT_E_INVALID_CALL XAUDIO2_E_INVALID_CALL
#define COMPAT_E_DEVICE_INVALIDATED XAUDIO2_E_DEVICE_INVALIDATED
#endif

typedef struct _XA2XAPOImpl {
    IXAPO *xapo;
    IXAPOParameters *xapo_params;

    LONG ref;

    FAPO FAPO_vtbl;
} XA2XAPOImpl;

typedef struct _XA2XAPOFXImpl {
    IXAPO IXAPO_iface;
    IXAPOParameters IXAPOParameters_iface;

    FAPO *fapo;
} XA2XAPOFXImpl;

typedef struct _XA2VoiceImpl {
    IXAudio2SourceVoice IXAudio2SourceVoice_iface;
#if XAUDIO2_VER == 0
    IXAudio20SourceVoice IXAudio20SourceVoice_iface;
#elif XAUDIO2_VER <= 3
    IXAudio23SourceVoice IXAudio23SourceVoice_iface;
#elif XAUDIO2_VER <= 7
    IXAudio27SourceVoice IXAudio27SourceVoice_iface;
#endif

    IXAudio2SubmixVoice IXAudio2SubmixVoice_iface;
#if XAUDIO2_VER == 0
    IXAudio20SubmixVoice IXAudio20SubmixVoice_iface;
#elif XAUDIO2_VER <= 3
    IXAudio23SubmixVoice IXAudio23SubmixVoice_iface;
#elif XAUDIO2_VER <= 7
    IXAudio27SubmixVoice IXAudio27SubmixVoice_iface;
#endif

    IXAudio2MasteringVoice IXAudio2MasteringVoice_iface;
#if XAUDIO2_VER == 0
    IXAudio20MasteringVoice IXAudio20MasteringVoice_iface;
#elif XAUDIO2_VER <= 3
    IXAudio23MasteringVoice IXAudio23MasteringVoice_iface;
#elif XAUDIO2_VER <= 7
    IXAudio27MasteringVoice IXAudio27MasteringVoice_iface;
#endif

    FAudioVoiceCallback FAudioVoiceCallback_vtbl;
    FAudioEffectChain *effect_chain;

    BOOL in_use;

    CRITICAL_SECTION lock;

    IXAudio2VoiceCallback *cb;

    FAudioVoice *faudio_voice;

    struct {
        FAudioEngineCallEXT proc;
        FAudio *faudio;
        float *stream;
    } engine_params;

    BOOL stop_engine_thread;
    HANDLE engine_thread;
    pthread_cond_t engine_done, engine_ready;
    pthread_mutex_t engine_lock;

    struct list entry;
} XA2VoiceImpl;

typedef struct _IXAudio2Impl {
    IXAudio2 IXAudio2_iface;

#if XAUDIO2_VER == 0
    IXAudio20 IXAudio20_iface;
#elif XAUDIO2_VER <= 2
    IXAudio22 IXAudio22_iface;
#elif XAUDIO2_VER <= 3
    IXAudio23 IXAudio23_iface;
#elif XAUDIO2_VER <= 7
    IXAudio27 IXAudio27_iface;
#endif

    CRITICAL_SECTION lock;

    struct list voices;

    FAudio *faudio;

    FAudioEngineCallback FAudioEngineCallback_vtbl;

    XA2VoiceImpl mst;

    DWORD last_query_glitches;

    UINT32 ncbs;
    IXAudio2EngineCallback **cbs;
} IXAudio2Impl;

#if XAUDIO2_VER == 0
extern const IXAudio20SourceVoiceVtbl XAudio20SourceVoice_Vtbl DECLSPEC_HIDDEN;
extern const IXAudio20SubmixVoiceVtbl XAudio20SubmixVoice_Vtbl DECLSPEC_HIDDEN;
extern const IXAudio20MasteringVoiceVtbl XAudio20MasteringVoice_Vtbl DECLSPEC_HIDDEN;
extern XA2VoiceImpl *impl_from_IXAudio20SourceVoice(IXAudio20SourceVoice *iface) DECLSPEC_HIDDEN;
extern XA2VoiceImpl *impl_from_IXAudio20SubmixVoice(IXAudio20SubmixVoice *iface) DECLSPEC_HIDDEN;
extern XA2VoiceImpl *impl_from_IXAudio20MasteringVoice(IXAudio20MasteringVoice *iface) DECLSPEC_HIDDEN;
#elif XAUDIO2_VER <= 3
extern const IXAudio23SourceVoiceVtbl XAudio23SourceVoice_Vtbl DECLSPEC_HIDDEN;
extern const IXAudio23SubmixVoiceVtbl XAudio23SubmixVoice_Vtbl DECLSPEC_HIDDEN;
extern const IXAudio23MasteringVoiceVtbl XAudio23MasteringVoice_Vtbl DECLSPEC_HIDDEN;
extern XA2VoiceImpl *impl_from_IXAudio23SourceVoice(IXAudio23SourceVoice *iface) DECLSPEC_HIDDEN;
extern XA2VoiceImpl *impl_from_IXAudio23SubmixVoice(IXAudio23SubmixVoice *iface) DECLSPEC_HIDDEN;
extern XA2VoiceImpl *impl_from_IXAudio23MasteringVoice(IXAudio23MasteringVoice *iface) DECLSPEC_HIDDEN;
#elif XAUDIO2_VER <= 7
extern const IXAudio27SourceVoiceVtbl XAudio27SourceVoice_Vtbl DECLSPEC_HIDDEN;
extern const IXAudio27SubmixVoiceVtbl XAudio27SubmixVoice_Vtbl DECLSPEC_HIDDEN;
extern const IXAudio27MasteringVoiceVtbl XAudio27MasteringVoice_Vtbl DECLSPEC_HIDDEN;
extern XA2VoiceImpl *impl_from_IXAudio27SourceVoice(IXAudio27SourceVoice *iface) DECLSPEC_HIDDEN;
extern XA2VoiceImpl *impl_from_IXAudio27SubmixVoice(IXAudio27SubmixVoice *iface) DECLSPEC_HIDDEN;
extern XA2VoiceImpl *impl_from_IXAudio27MasteringVoice(IXAudio27MasteringVoice *iface) DECLSPEC_HIDDEN;
#endif

#if XAUDIO2_VER == 0
extern const IXAudio20Vtbl XAudio20_Vtbl DECLSPEC_HIDDEN;
#elif XAUDIO2_VER <= 2
extern const IXAudio22Vtbl XAudio22_Vtbl DECLSPEC_HIDDEN;
#elif XAUDIO2_VER <= 3
extern const IXAudio23Vtbl XAudio23_Vtbl DECLSPEC_HIDDEN;
#elif XAUDIO2_VER <= 7
extern const IXAudio27Vtbl XAudio27_Vtbl DECLSPEC_HIDDEN;
#endif

/* xaudio_dll.c */
extern HRESULT xaudio2_initialize(IXAudio2Impl *This, UINT32 flags, XAUDIO2_PROCESSOR proc) DECLSPEC_HIDDEN;
extern FAudioEffectChain *wrap_effect_chain(const XAUDIO2_EFFECT_CHAIN *pEffectChain) DECLSPEC_HIDDEN;
extern void engine_cb(FAudioEngineCallEXT proc, FAudio *faudio, float *stream, void *user) DECLSPEC_HIDDEN;
extern DWORD WINAPI engine_thread(void *user) DECLSPEC_HIDDEN;

/* xapo.c */
extern HRESULT make_xapo_factory(REFCLSID clsid, REFIID riid, void **ppv) DECLSPEC_HIDDEN;

/* xaudio_allocator.c */
extern void* XAudio_Internal_Malloc(size_t size) DECLSPEC_HIDDEN;
extern void XAudio_Internal_Free(void* ptr) DECLSPEC_HIDDEN;
extern void* XAudio_Internal_Realloc(void* ptr, size_t size) DECLSPEC_HIDDEN;
