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

    // Sound lifetime
    LUA_DECLARE(FMODCreateSound);
    LUA_DECLARE(FMODFreeSound);

    // Playback
    LUA_DECLARE(FMODPlaySound);
    LUA_DECLARE(FMODStopSound);

    // Channel control
    LUA_DECLARE(FMODSetSoundVolume);
    LUA_DECLARE(FMODSetSoundPitch);
    LUA_DECLARE(FMODSetSoundPosition);
    LUA_DECLARE(FMODIsSoundPlaying);

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
