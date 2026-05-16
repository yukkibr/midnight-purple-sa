/*****************************************************************************
 *
 *  PROJECT:     Midnight Purple:SA
 *  FILE:        mods/deathmatch/logic/luadefs/CLuaFMODDefs.h
 *  PURPOSE:     Lua bindings for the FMOD audio engine
 *
 *****************************************************************************/

#pragma once
#include "CLuaDefs.h"
#include <unordered_map>
#include <vector>

class CLuaMain;

class CLuaFMODDefs : public CLuaDefs
{
public:
    static void LoadFunctions();

    // Called from CLuaManager::RemoveVirtualMachine — frees all sounds created by the resource
    static void OnLuaMainRemoved(CLuaMain* pLuaMain);

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

    // Low-pass / high-pass filters
    LUA_DECLARE(FMODSetChannelLowPass);
    LUA_DECLARE(FMODRemoveChannelLowPass);
    LUA_DECLARE(FMODSetChannelHighPass);
    LUA_DECLARE(FMODRemoveChannelHighPass);

    // Flanger DSP
    LUA_DECLARE(FMODSetChannelFlanger);
    LUA_DECLARE(FMODRemoveChannelFlanger);

    // Chorus DSP
    LUA_DECLARE(FMODSetChannelChorus);
    LUA_DECLARE(FMODRemoveChannelChorus);

    // Distortion DSP
    LUA_DECLARE(FMODSetChannelDistortion);
    LUA_DECLARE(FMODRemoveChannelDistortion);

    // Volume categories (0=sfx, 1=ambient, 2=music)
    LUA_DECLARE(FMODSetChannelCategory);
    LUA_DECLARE(FMODSetCategoryVolume);
    LUA_DECLARE(FMODGetCategoryVolume);

    // Master volume
    LUA_DECLARE(FMODSetMasterVolume);
    LUA_DECLARE(FMODGetMasterVolume);

    // Named parameters (ambience system)
    LUA_DECLARE(FMODSetParameter);
    LUA_DECLARE(FMODGetParameter);

    // DSP chain query
    LUA_DECLARE(FMODGetChannelEffects);

    // Occlusion / obstruction
    LUA_DECLARE(FMODSetChannelOcclusion);

private:
    // Per-resource sound tracking: LuaMain* → list of sound IDs it created
    static std::unordered_map<void*, std::vector<uint32_t>> s_resourceSounds;

    static void TrackSound(lua_State* luaVM, uint32_t soundId);
    static void UntrackSound(uint32_t soundId);
    static int  StringToCategory(const SString& name);
};
