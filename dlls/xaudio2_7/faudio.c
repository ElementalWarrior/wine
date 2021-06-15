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

#define NONAMELESSUNION
#define COBJMACROS

#include "xaudio_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(xaudio2);

#define MAKE_FUNCPTR(f) typeof(f) * p##f = NULL;
MAKE_FUNCPTR(FAudio_AddRef)
MAKE_FUNCPTR(FAudio_CommitOperationSet)
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
MAKE_FUNCPTR(FAudioCreateReverb9WithCustomAllocatorEXT)
MAKE_FUNCPTR(FAudioCreateReverbWithCustomAllocatorEXT)
MAKE_FUNCPTR(FAudioCreateVolumeMeter)
MAKE_FUNCPTR(FAudioCreateVolumeMeterWithCustomAllocatorEXT)
MAKE_FUNCPTR(FAudioLinkedVersion)

MAKE_FUNCPTR(F3DAudioCalculate)
MAKE_FUNCPTR(F3DAudioInitialize)
MAKE_FUNCPTR(F3DAudioInitialize8)

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

static HMODULE faudio;

BOOL load_faudio(void)
{
    if (!(faudio = LoadLibraryA( "FAudio" )))
    {
        ERR( "FAudio library not found.\n" );
        return FALSE;
    }

#define LOAD_FUNCPTR(f) \
    if (!(p##f = (void *)GetProcAddress( faudio, #f )))  \
    {                                                    \
        ERR( "FAudio function %s not found\n", #f );     \
        FreeLibrary( faudio );                           \
        faudio = NULL;                                   \
        return FALSE;                                    \
    }
LOAD_FUNCPTR(FAudio_AddRef)
LOAD_FUNCPTR(FAudio_CommitOperationSet)
LOAD_FUNCPTR(FAudio_CreateMasteringVoice)
LOAD_FUNCPTR(FAudio_CreateMasteringVoice8)
LOAD_FUNCPTR(FAudio_CreateSourceVoice)
LOAD_FUNCPTR(FAudio_CreateSubmixVoice)
LOAD_FUNCPTR(FAudio_GetDeviceCount)
LOAD_FUNCPTR(FAudio_GetDeviceDetails)
LOAD_FUNCPTR(FAudio_GetPerformanceData)
LOAD_FUNCPTR(FAudio_Initialize)
LOAD_FUNCPTR(FAudio_RegisterForCallbacks)
LOAD_FUNCPTR(FAudio_Release)
LOAD_FUNCPTR(FAudio_SetDebugConfiguration)
LOAD_FUNCPTR(FAudio_StartEngine)
LOAD_FUNCPTR(FAudio_StopEngine)

LOAD_FUNCPTR(FAudioVoice_DestroyVoice)
LOAD_FUNCPTR(FAudioVoice_DisableEffect)
LOAD_FUNCPTR(FAudioVoice_EnableEffect)
LOAD_FUNCPTR(FAudioVoice_GetChannelVolumes)
LOAD_FUNCPTR(FAudioVoice_GetEffectParameters)
LOAD_FUNCPTR(FAudioVoice_GetEffectState)
LOAD_FUNCPTR(FAudioVoice_GetFilterParameters)
LOAD_FUNCPTR(FAudioVoice_GetOutputFilterParameters)
LOAD_FUNCPTR(FAudioVoice_GetOutputMatrix)
LOAD_FUNCPTR(FAudioVoice_GetVoiceDetails)
LOAD_FUNCPTR(FAudioVoice_GetVolume)
LOAD_FUNCPTR(FAudioVoice_SetChannelVolumes)
LOAD_FUNCPTR(FAudioVoice_SetEffectChain)
LOAD_FUNCPTR(FAudioVoice_SetEffectParameters)
LOAD_FUNCPTR(FAudioVoice_SetFilterParameters)
LOAD_FUNCPTR(FAudioVoice_SetOutputFilterParameters)
LOAD_FUNCPTR(FAudioVoice_SetOutputMatrix)
LOAD_FUNCPTR(FAudioVoice_SetOutputVoices)
LOAD_FUNCPTR(FAudioVoice_SetVolume)

LOAD_FUNCPTR(FAudioSourceVoice_Discontinuity)
LOAD_FUNCPTR(FAudioSourceVoice_ExitLoop)
LOAD_FUNCPTR(FAudioSourceVoice_FlushSourceBuffers)
LOAD_FUNCPTR(FAudioSourceVoice_GetFrequencyRatio)
LOAD_FUNCPTR(FAudioSourceVoice_GetState)
LOAD_FUNCPTR(FAudioSourceVoice_SetFrequencyRatio)
LOAD_FUNCPTR(FAudioSourceVoice_SetSourceSampleRate)
LOAD_FUNCPTR(FAudioSourceVoice_Start)
LOAD_FUNCPTR(FAudioSourceVoice_Stop)
LOAD_FUNCPTR(FAudioSourceVoice_SubmitSourceBuffer)

LOAD_FUNCPTR(FAudioMasteringVoice_GetChannelMask)

LOAD_FUNCPTR(FAudioCOMConstructWithCustomAllocatorEXT)
LOAD_FUNCPTR(FAudioCreate)
LOAD_FUNCPTR(FAudioCreateReverb)
LOAD_FUNCPTR(FAudioCreateReverb9)
LOAD_FUNCPTR(FAudioCreateReverb9WithCustomAllocatorEXT)
LOAD_FUNCPTR(FAudioCreateReverbWithCustomAllocatorEXT)
LOAD_FUNCPTR(FAudioCreateVolumeMeter)
LOAD_FUNCPTR(FAudioCreateVolumeMeterWithCustomAllocatorEXT)
LOAD_FUNCPTR(FAudioLinkedVersion)

LOAD_FUNCPTR(F3DAudioCalculate)
LOAD_FUNCPTR(F3DAudioInitialize)
LOAD_FUNCPTR(F3DAudioInitialize8)

LOAD_FUNCPTR(FACTAudioEngine_AddRef)
LOAD_FUNCPTR(FACTAudioEngine_CreateInMemoryWaveBank)
LOAD_FUNCPTR(FACTAudioEngine_CreateSoundBank)
LOAD_FUNCPTR(FACTAudioEngine_CreateStreamingWaveBank)
LOAD_FUNCPTR(FACTAudioEngine_DoWork)
LOAD_FUNCPTR(FACTAudioEngine_GetCategory)
LOAD_FUNCPTR(FACTAudioEngine_GetFinalMixFormat)
LOAD_FUNCPTR(FACTAudioEngine_GetGlobalVariable)
LOAD_FUNCPTR(FACTAudioEngine_GetGlobalVariableIndex)
LOAD_FUNCPTR(FACTAudioEngine_GetRendererCount)
LOAD_FUNCPTR(FACTAudioEngine_GetRendererDetails)
LOAD_FUNCPTR(FACTAudioEngine_Initialize)
LOAD_FUNCPTR(FACTAudioEngine_Pause)
LOAD_FUNCPTR(FACTAudioEngine_PrepareWave)
LOAD_FUNCPTR(FACTAudioEngine_RegisterNotification)
LOAD_FUNCPTR(FACTAudioEngine_Release)
LOAD_FUNCPTR(FACTAudioEngine_SetGlobalVariable)
LOAD_FUNCPTR(FACTAudioEngine_SetVolume)
LOAD_FUNCPTR(FACTAudioEngine_ShutDown)
LOAD_FUNCPTR(FACTAudioEngine_Stop)
LOAD_FUNCPTR(FACTAudioEngine_UnRegisterNotification)

LOAD_FUNCPTR(FACTCreateEngineWithCustomAllocatorEXT)

LOAD_FUNCPTR(FACTCue_Destroy)
LOAD_FUNCPTR(FACTCue_GetProperties)
LOAD_FUNCPTR(FACTCue_GetState)
LOAD_FUNCPTR(FACTCue_GetVariable)
LOAD_FUNCPTR(FACTCue_GetVariableIndex)
LOAD_FUNCPTR(FACTCue_Pause)
LOAD_FUNCPTR(FACTCue_Play)
LOAD_FUNCPTR(FACTCue_SetMatrixCoefficients)
LOAD_FUNCPTR(FACTCue_SetVariable)
LOAD_FUNCPTR(FACTCue_Stop)

LOAD_FUNCPTR(FACTSoundBank_Destroy)
LOAD_FUNCPTR(FACTSoundBank_GetCueIndex)
LOAD_FUNCPTR(FACTSoundBank_GetCueProperties)
LOAD_FUNCPTR(FACTSoundBank_GetNumCues)
LOAD_FUNCPTR(FACTSoundBank_GetState)
LOAD_FUNCPTR(FACTSoundBank_Play)
LOAD_FUNCPTR(FACTSoundBank_Prepare)
LOAD_FUNCPTR(FACTSoundBank_Stop)

LOAD_FUNCPTR(FACTWave_Destroy)
LOAD_FUNCPTR(FACTWave_GetProperties)
LOAD_FUNCPTR(FACTWave_GetState)
LOAD_FUNCPTR(FACTWave_Pause)
LOAD_FUNCPTR(FACTWave_Play)
LOAD_FUNCPTR(FACTWave_SetMatrixCoefficients)
LOAD_FUNCPTR(FACTWave_SetPitch)
LOAD_FUNCPTR(FACTWave_SetVolume)
LOAD_FUNCPTR(FACTWave_Stop)

LOAD_FUNCPTR(FACTWaveBank_Destroy)
LOAD_FUNCPTR(FACTWaveBank_GetNumWaves)
LOAD_FUNCPTR(FACTWaveBank_GetState)
LOAD_FUNCPTR(FACTWaveBank_GetWaveIndex)
LOAD_FUNCPTR(FACTWaveBank_GetWaveProperties)
LOAD_FUNCPTR(FACTWaveBank_Play)
LOAD_FUNCPTR(FACTWaveBank_Prepare)
LOAD_FUNCPTR(FACTWaveBank_Stop)

LOAD_FUNCPTR(FAPOFX_CreateFXWithCustomAllocatorEXT)
#undef LOAD_FUNCPTR

    return TRUE;
}

void unload_faudio(void)
{
    if (faudio) FreeLibrary(faudio);
}
