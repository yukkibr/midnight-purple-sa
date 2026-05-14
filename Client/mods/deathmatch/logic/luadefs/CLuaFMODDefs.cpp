/*****************************************************************************
 *
 *  PROJECT:     Midnight Purple:SA
 *  FILE:        mods/deathmatch/logic/luadefs/CLuaFMODDefs.cpp
 *  PURPOSE:     Lua bindings for the FMOD audio engine
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CLuaFMODDefs.h"

void CLuaFMODDefs::LoadFunctions()
{
    constexpr static const std::pair<const char*, lua_CFunction> functions[]{
        {"fmodCreateSound",      FMODCreateSound},
        {"fmodFreeSound",        FMODFreeSound},
        {"fmodPlaySound",        FMODPlaySound},
        {"fmodStopSound",        FMODStopSound},
        {"fmodSetSoundVolume",   FMODSetSoundVolume},
        {"fmodSetSoundPitch",    FMODSetSoundPitch},
        {"fmodSetSoundPosition", FMODSetSoundPosition},
        {"fmodIsSoundPlaying",   FMODIsSoundPlaying},
        {"fmodSetReverbPreset",  FMODSetReverbPreset},
        {"fmodSetReverbWetLevel",FMODSetReverbWetLevel},
        {"fmodSetChannelReverb", FMODSetChannelReverb},
        {"fmodSetChannelEcho",   FMODSetChannelEcho},
        {"fmodRemoveChannelEcho",FMODRemoveChannelEcho},
        {"fmodSetMasterVolume",  FMODSetMasterVolume},
        {"fmodGetMasterVolume",  FMODGetMasterVolume},
        {"fmodSetParameter",     FMODSetParameter},
        {"fmodGetParameter",     FMODGetParameter},
    };

    for (const auto& [name, func] : functions)
        CLuaCFunctions::AddFunction(name, func);
}

// fmodCreateSound(path, [bool is3D = false], [bool loop = false]) -> soundId or false
int CLuaFMODDefs::FMODCreateSound(lua_State* luaVM)
{
    SString strPath;
    bool    b3D   = false;
    bool    bLoop = false;

    CScriptArgReader argStream(luaVM);
    argStream.ReadString(strPath);
    argStream.ReadBool(b3D, false);
    argStream.ReadBool(bLoop, false);

    if (argStream.HasErrors())
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    // Resolve ":resourceName/path" to an absolute filesystem path, same as playSound()
    CLuaMain* pLuaMain = m_pLuaManager->GetVirtualMachine(luaVM);
    if (!pLuaMain)
    {
        lua_pushboolean(luaVM, false);
        return 1;
    }
    CResource* pResource = pLuaMain->GetResource();
    if (!pResource)
    {
        lua_pushboolean(luaVM, false);
        return 1;
    }

    SString strResolved;
    if (CResourceManager::ParseResourcePathInput(strPath, pResource, &strResolved, nullptr, true))
        strPath = strResolved;

    // ParseResourcePathInput can null pResource on invalid resource URLs
    if (!pResource)
    {
        lua_pushboolean(luaVM, false);
        return 1;
    }

    uint32_t soundId = g_pCore->FMODCreateSound(strPath.c_str(), b3D, bLoop);
    if (soundId == 0)
    {
        lua_pushboolean(luaVM, false);
        return 1;
    }

    lua_pushinteger(luaVM, static_cast<lua_Integer>(soundId));
    return 1;
}

// fmodFreeSound(soundId) -> bool
int CLuaFMODDefs::FMODFreeSound(lua_State* luaVM)
{
    uint32_t soundId = 0;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(soundId);

    if (argStream.HasErrors() || soundId == 0)
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    g_pCore->FMODFreeSound(soundId);
    lua_pushboolean(luaVM, true);
    return 1;
}

// fmodPlaySound(soundId, [x, y, z, [minDist=1, maxDist=100]]) -> channelId or false
int CLuaFMODDefs::FMODPlaySound(lua_State* luaVM)
{
    uint32_t soundId = 0;
    float    x = 0.0f, y = 0.0f, z = 0.0f;
    float    minDist = 1.0f, maxDist = 100.0f;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(soundId);
    argStream.ReadNumber(x, 0.0f);
    argStream.ReadNumber(y, 0.0f);
    argStream.ReadNumber(z, 0.0f);
    argStream.ReadNumber(minDist, 1.0f);
    argStream.ReadNumber(maxDist, 100.0f);

    if (argStream.HasErrors() || soundId == 0)
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    uint32_t channelId = g_pCore->FMODPlaySound(soundId, x, y, z, minDist, maxDist);
    if (channelId == 0)
    {
        lua_pushboolean(luaVM, false);
        return 1;
    }

    lua_pushinteger(luaVM, static_cast<lua_Integer>(channelId));
    return 1;
}

// fmodStopSound(channelId) -> bool
int CLuaFMODDefs::FMODStopSound(lua_State* luaVM)
{
    uint32_t channelId = 0;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(channelId);

    if (argStream.HasErrors() || channelId == 0)
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    lua_pushboolean(luaVM, g_pCore->FMODStopChannel(channelId));
    return 1;
}

// fmodSetSoundVolume(channelId, volume) -> bool  (volume: 0.0 to 1.0)
int CLuaFMODDefs::FMODSetSoundVolume(lua_State* luaVM)
{
    uint32_t channelId = 0;
    float    volume    = 1.0f;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(channelId);
    argStream.ReadNumber(volume);

    if (argStream.HasErrors() || channelId == 0)
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    lua_pushboolean(luaVM, g_pCore->FMODSetChannelVolume(channelId, volume));
    return 1;
}

// fmodSetSoundPitch(channelId, pitch) -> bool  (1.0 = normal, 2.0 = one octave up)
int CLuaFMODDefs::FMODSetSoundPitch(lua_State* luaVM)
{
    uint32_t channelId = 0;
    float    pitch     = 1.0f;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(channelId);
    argStream.ReadNumber(pitch);

    if (argStream.HasErrors() || channelId == 0)
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    lua_pushboolean(luaVM, g_pCore->FMODSetChannelPitch(channelId, pitch));
    return 1;
}

// fmodSetSoundPosition(channelId, x, y, z) -> bool
int CLuaFMODDefs::FMODSetSoundPosition(lua_State* luaVM)
{
    uint32_t channelId = 0;
    float    x = 0.0f, y = 0.0f, z = 0.0f;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(channelId);
    argStream.ReadNumber(x);
    argStream.ReadNumber(y);
    argStream.ReadNumber(z);

    if (argStream.HasErrors() || channelId == 0)
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    lua_pushboolean(luaVM, g_pCore->FMODSetChannelPosition(channelId, x, y, z));
    return 1;
}

// fmodIsSoundPlaying(channelId) -> bool
int CLuaFMODDefs::FMODIsSoundPlaying(lua_State* luaVM)
{
    uint32_t channelId = 0;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(channelId);

    if (argStream.HasErrors() || channelId == 0)
    {
        lua_pushboolean(luaVM, false);
        return 1;
    }

    lua_pushboolean(luaVM, g_pCore->FMODIsChannelPlaying(channelId));
    return 1;
}

// fmodSetReverbPreset(preset) -> bool
// preset: "off", "generic", "room", "bathroom", "cave", "city", "alley", "forest",
//         "mountains", "plain", "sewerpipe", "underwater", "stonecorridor", "hallway",
//         "stoneroom", "auditorium", "concerthall", "arena", "hangar", "parkinglot", ...
int CLuaFMODDefs::FMODSetReverbPreset(lua_State* luaVM)
{
    SString strPreset;

    CScriptArgReader argStream(luaVM);
    argStream.ReadString(strPreset);

    if (argStream.HasErrors())
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    g_pCore->FMODSetReverbPreset(strPreset.c_str());
    lua_pushboolean(luaVM, true);
    return 1;
}

// fmodSetReverbWetLevel(wetDB) -> bool
// Adjusts only the WetLevel (output volume) of the current reverb preset without
// changing its character (decay time, diffusion, etc.).
// wetDB: dB range -80 (silent) to 20 (very loud). 0 = unity (same level as dry).
// Useful values: -3 (tunnel, clear echo), -12 (city, subtle), -28 (plain, barely there)
int CLuaFMODDefs::FMODSetReverbWetLevel(lua_State* luaVM)
{
    float wetDB = -12.0f;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(wetDB);

    if (argStream.HasErrors())
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    g_pCore->FMODSetReverbWetLevel(wetDB);
    lua_pushboolean(luaVM, true);
    return 1;
}

// fmodSetChannelReverb(channelId, wet) -> bool  (wet: 0.0=dry, 1.0=full reverb)
int CLuaFMODDefs::FMODSetChannelReverb(lua_State* luaVM)
{
    uint32_t channelId = 0;
    float    wet       = 1.0f;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(channelId);
    argStream.ReadNumber(wet);

    if (argStream.HasErrors() || channelId == 0)
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    lua_pushboolean(luaVM, g_pCore->FMODSetChannelReverbWet(channelId, wet));
    return 1;
}

// fmodSetChannelEcho(channelId, delayMS, feedbackPct, wetDB) -> bool
// Attaches (or updates) an echo DSP on the channel.
//   delayMS     : echo delay in milliseconds (10–5000, typical tunnel: 60–120)
//   feedbackPct : how much of each echo feeds the next (0–100, typical: 40–55)
//   wetDB       : echo volume in dB relative to dry (-80..10, typical: -6..-2)
int CLuaFMODDefs::FMODSetChannelEcho(lua_State* luaVM)
{
    uint32_t channelId   = 0;
    float    delayMS     = 75.0f;
    float    feedbackPct = 45.0f;
    float    wetDB       = -4.0f;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(channelId);
    argStream.ReadNumber(delayMS);
    argStream.ReadNumber(feedbackPct);
    argStream.ReadNumber(wetDB);

    if (argStream.HasErrors() || channelId == 0)
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    lua_pushboolean(luaVM, g_pCore->FMODApplyChannelEcho(channelId, delayMS, feedbackPct, wetDB));
    return 1;
}

// fmodRemoveChannelEcho(channelId) -> bool
int CLuaFMODDefs::FMODRemoveChannelEcho(lua_State* luaVM)
{
    uint32_t channelId = 0;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(channelId);

    if (argStream.HasErrors() || channelId == 0)
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    lua_pushboolean(luaVM, g_pCore->FMODRemoveChannelEcho(channelId));
    return 1;
}

// fmodSetMasterVolume(volume) -> bool  (volume: 0.0 – 1.0, default 0.5)
int CLuaFMODDefs::FMODSetMasterVolume(lua_State* luaVM)
{
    float volume = 0.5f;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(volume);

    if (argStream.HasErrors())
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    volume = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
    g_pCore->FMODSetMasterVolume(volume);
    lua_pushboolean(luaVM, true);
    return 1;
}

// fmodGetMasterVolume() -> number
int CLuaFMODDefs::FMODGetMasterVolume(lua_State* luaVM)
{
    lua_pushnumber(luaVM, static_cast<lua_Number>(g_pCore->FMODGetMasterVolume()));
    return 1;
}

// fmodSetParameter(name, value) -> bool
int CLuaFMODDefs::FMODSetParameter(lua_State* luaVM)
{
    SString strName;
    float   value = 0.0f;

    CScriptArgReader argStream(luaVM);
    argStream.ReadString(strName);
    argStream.ReadNumber(value);

    if (argStream.HasErrors())
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    g_pCore->FMODSetParameter(strName.c_str(), value);
    lua_pushboolean(luaVM, true);
    return 1;
}

// fmodGetParameter(name [, default=0]) -> number
int CLuaFMODDefs::FMODGetParameter(lua_State* luaVM)
{
    SString strName;
    float   defaultValue = 0.0f;

    CScriptArgReader argStream(luaVM);
    argStream.ReadString(strName);
    argStream.ReadNumber(defaultValue, 0.0f);

    if (argStream.HasErrors())
    {
        lua_pushnumber(luaVM, 0.0);
        return 1;
    }

    float value = g_pCore->FMODGetParameter(strName.c_str(), defaultValue);
    lua_pushnumber(luaVM, static_cast<lua_Number>(value));
    return 1;
}
