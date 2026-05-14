/*****************************************************************************
 *
 *  PROJECT:     Midnight Purple:SA
 *  FILE:        mods/deathmatch/logic/luadefs/CLuaFMODDefs.h
 *  PURPOSE:     Lua bindings for the FMOD audio engine
 *
 *****************************************************************************/

#pragma once
#include "CLuaDefs.h"

class CLuaFMODDefs : public CLuaDefs
{
public:
    static void LoadFunctions();

    // Version / diagnostics
    LUA_DECLARE(FMODGetVersion);
    LUA_DECLARE(FMODGetLastError);

    // Sound lifetime
    LUA_DECLARE(FMODCreateSound);
    LUA_DECLARE(FMODCreateStream);
    LUA_DECLARE(FMODCreateSoundFromMemory);
    LUA_DECLARE(FMODFreeSound);

    // Playback
    LUA_DECLARE(FMODPlaySound);
    LUA_DECLARE(FMODStopSound);
    LUA_DECLARE(FMODPauseSound);
    LUA_DECLARE(FMODResumeSound);
    LUA_DECLARE(FMODIsSoundPaused);
    LUA_DECLARE(FMODIsSoundPlaying);

    // Channel control
    LUA_DECLARE(FMODSetSoundVolume);
    LUA_DECLARE(FMODSetSoundPitch);
    LUA_DECLARE(FMODSetSoundPosition);
    LUA_DECLARE(FMODSetSoundVelocity);
    LUA_DECLARE(FMODSetSoundLooped);

    // Channel read-back
    LUA_DECLARE(FMODGetSoundVolume);
    LUA_DECLARE(FMODGetSoundPitch);
    LUA_DECLARE(FMODGetSoundPosition);
    LUA_DECLARE(FMODGetSoundLooped);

    // Reverb
    LUA_DECLARE(FMODSetReverbPreset);
    LUA_DECLARE(FMODSetReverbWetLevel);
    LUA_DECLARE(FMODSetChannelReverb);

    // Echo DSP
    LUA_DECLARE(FMODSetChannelEcho);
    LUA_DECLARE(FMODRemoveChannelEcho);

    // Master volume
    LUA_DECLARE(FMODSetMasterVolume);
    LUA_DECLARE(FMODGetMasterVolume);

    // Named parameters (ambience system)
    LUA_DECLARE(FMODSetParameter);
    LUA_DECLARE(FMODGetParameter);
};
