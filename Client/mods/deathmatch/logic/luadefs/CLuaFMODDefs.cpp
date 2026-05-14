/*****************************************************************************
 *
 *  PROJECT:     Midnight Purple:SA
 *  FILE:        mods/deathmatch/logic/luadefs/CLuaFMODDefs.cpp
 *  PURPOSE:     Lua bindings for the FMOD audio engine
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CLuaFMODDefs.h"

// Push false + FMOD error code + error string on FMOD-level failures.
// Argument-validation failures still push just false (1 return) so the
// error message comes from the script debug output, not from FMOD.
#define FMOD_PUSH_ERROR(luaVM)                                        \
    lua_pushboolean(luaVM, false);                                    \
    lua_pushinteger(luaVM, g_pCore->FMODGetLastError());              \
    lua_pushstring(luaVM, g_pCore->FMODGetLastErrorString());         \
    return 3;

// Resolve ":resourceName/file" to an absolute path via the resource manager.
// Returns false (pushing it to Lua) if resolution fails.
static bool ResolveResourcePath(lua_State* luaVM, CLuaManager* pLuaManager, SString& strPath)
{
    CLuaMain* pLuaMain = pLuaManager->GetVirtualMachine(luaVM);
    if (!pLuaMain) return false;

    CResource* pResource = pLuaMain->GetResource();
    if (!pResource) return false;

    SString strResolved;
    if (CResourceManager::ParseResourcePathInput(strPath, pResource, &strResolved, nullptr, true))
        strPath = strResolved;

    return pResource != nullptr;
}

void CLuaFMODDefs::LoadFunctions()
{
    constexpr static const std::pair<const char*, lua_CFunction> functions[]{
        // Version / diagnostics
        {"fmodGetVersion",        FMODGetVersion},
        {"fmodGetLastError",      FMODGetLastError},
        // Sound lifetime
        {"fmodCreateSound",           FMODCreateSound},
        {"fmodCreateStream",          FMODCreateStream},
        {"fmodCreateSoundFromMemory", FMODCreateSoundFromMemory},
        {"fmodFreeSound",             FMODFreeSound},
        // Playback
        {"fmodPlaySound",         FMODPlaySound},
        {"fmodStopSound",         FMODStopSound},
        {"fmodPauseSound",        FMODPauseSound},
        {"fmodResumeSound",       FMODResumeSound},
        {"fmodIsSoundPaused",     FMODIsSoundPaused},
        {"fmodIsSoundPlaying",    FMODIsSoundPlaying},
        // Channel control
        {"fmodSetSoundVolume",    FMODSetSoundVolume},
        {"fmodSetSoundPitch",     FMODSetSoundPitch},
        {"fmodSetSoundPosition",  FMODSetSoundPosition},
        {"fmodSetSoundVelocity",  FMODSetSoundVelocity},
        {"fmodSetSoundLooped",    FMODSetSoundLooped},
        // Channel read-back
        {"fmodGetSoundVolume",    FMODGetSoundVolume},
        {"fmodGetSoundPitch",     FMODGetSoundPitch},
        {"fmodGetSoundPosition",  FMODGetSoundPosition},
        {"fmodGetSoundLooped",    FMODGetSoundLooped},
        // Reverb
        {"fmodSetReverbPreset",   FMODSetReverbPreset},
        {"fmodSetReverbWetLevel", FMODSetReverbWetLevel},
        {"fmodSetChannelReverb",  FMODSetChannelReverb},
        // Echo DSP
        {"fmodSetChannelEcho",    FMODSetChannelEcho},
        {"fmodRemoveChannelEcho", FMODRemoveChannelEcho},
        // Master volume
        {"fmodSetMasterVolume",   FMODSetMasterVolume},
        {"fmodGetMasterVolume",   FMODGetMasterVolume},
        // Named parameters
        {"fmodSetParameter",      FMODSetParameter},
        {"fmodGetParameter",      FMODGetParameter},
    };

    for (const auto& [name, func] : functions)
        CLuaCFunctions::AddFunction(name, func);
}

// ─── Version / diagnostics ────────────────────────────────────────────────────

// fmodGetVersion() -> string  e.g. "2.02.21"
int CLuaFMODDefs::FMODGetVersion(lua_State* luaVM)
{
    SString ver = g_pCore->FMODGetVersion();
    lua_pushstring(luaVM, ver.c_str());
    return 1;
}

// fmodGetLastError() -> errorCode (int), errorString (string)
// Call after any fmod* function returns false to get more detail.
int CLuaFMODDefs::FMODGetLastError(lua_State* luaVM)
{
    lua_pushinteger(luaVM, g_pCore->FMODGetLastError());
    lua_pushstring(luaVM, g_pCore->FMODGetLastErrorString());
    return 2;
}

// ─── Sound lifetime ───────────────────────────────────────────────────────────

// fmodCreateSound(path, [is3D=false], [loop=false]) -> soundId or false, errCode, errStr
// Loads the entire file into memory. Use for short, frequently-played SFX.
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

    if (!ResolveResourcePath(luaVM, m_pLuaManager, strPath))
    {
        lua_pushboolean(luaVM, false);
        return 1;
    }

    uint32_t soundId = g_pCore->FMODCreateSound(strPath.c_str(), b3D, bLoop);
    if (soundId == 0) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushinteger(luaVM, static_cast<lua_Integer>(soundId));
    return 1;
}

// fmodCreateStream(path, [is3D=false], [loop=false]) -> soundId or false, errCode, errStr
// Decodes the file progressively from disk. Use for long music tracks.
int CLuaFMODDefs::FMODCreateStream(lua_State* luaVM)
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

    if (!ResolveResourcePath(luaVM, m_pLuaManager, strPath))
    {
        lua_pushboolean(luaVM, false);
        return 1;
    }

    uint32_t soundId = g_pCore->FMODCreateStream(strPath.c_str(), b3D, bLoop);
    if (soundId == 0) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushinteger(luaVM, static_cast<lua_Integer>(soundId));
    return 1;
}

// fmodCreateSoundFromMemory(data, [is3D=false], [loop=false]) -> soundId or false, errCode, errStr
// Loads a sound from a raw audio data string (e.g. TEA-decrypted .wavlocked contents).
// FMOD copies the buffer internally — the Lua string can be GC'd after this call.
int CLuaFMODDefs::FMODCreateSoundFromMemory(lua_State* luaVM)
{
    SString strData;
    bool    b3D   = false;
    bool    bLoop = false;

    CScriptArgReader argStream(luaVM);
    argStream.ReadString(strData);
    argStream.ReadBool(b3D, false);
    argStream.ReadBool(bLoop, false);

    if (argStream.HasErrors())
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    if (strData.empty())
    {
        lua_pushboolean(luaVM, false);
        return 1;
    }

    uint32_t soundId = g_pCore->FMODCreateSoundFromMemory(strData.data(), strData.size(), b3D, bLoop);
    if (soundId == 0) { FMOD_PUSH_ERROR(luaVM); }

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

// ─── Playback ─────────────────────────────────────────────────────────────────

// fmodPlaySound(soundId, [x, y, z, [minDist=1, maxDist=100]]) -> channelId or false, errCode, errStr
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
    if (channelId == 0) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushinteger(luaVM, static_cast<lua_Integer>(channelId));
    return 1;
}

// fmodStopSound(channelId) -> bool or false, errCode, errStr
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

    if (!g_pCore->FMODStopChannel(channelId)) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushboolean(luaVM, true);
    return 1;
}

// fmodPauseSound(channelId) -> bool or false, errCode, errStr
int CLuaFMODDefs::FMODPauseSound(lua_State* luaVM)
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

    if (!g_pCore->FMODPauseChannel(channelId, true)) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushboolean(luaVM, true);
    return 1;
}

// fmodResumeSound(channelId) -> bool or false, errCode, errStr
int CLuaFMODDefs::FMODResumeSound(lua_State* luaVM)
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

    if (!g_pCore->FMODPauseChannel(channelId, false)) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushboolean(luaVM, true);
    return 1;
}

// fmodIsSoundPaused(channelId) -> bool
int CLuaFMODDefs::FMODIsSoundPaused(lua_State* luaVM)
{
    uint32_t channelId = 0;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(channelId);

    if (argStream.HasErrors() || channelId == 0)
    {
        lua_pushboolean(luaVM, false);
        return 1;
    }

    lua_pushboolean(luaVM, g_pCore->FMODIsChannelPaused(channelId));
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

// ─── Channel control ──────────────────────────────────────────────────────────

// fmodSetSoundVolume(channelId, volume) -> bool  (volume: 0.0 – 1.0)
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

    if (!g_pCore->FMODSetChannelVolume(channelId, volume)) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushboolean(luaVM, true);
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

    if (!g_pCore->FMODSetChannelPitch(channelId, pitch)) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushboolean(luaVM, true);
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

    if (!g_pCore->FMODSetChannelPosition(channelId, x, y, z)) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushboolean(luaVM, true);
    return 1;
}

// fmodSetSoundVelocity(channelId, vx, vy, vz) -> bool
// Sets the world-space velocity (units/s) of a 3D channel for Doppler pitch shift.
// Use getElementVelocity() on the attached object and pass the result here each frame.
int CLuaFMODDefs::FMODSetSoundVelocity(lua_State* luaVM)
{
    uint32_t channelId = 0;
    float    vx = 0.0f, vy = 0.0f, vz = 0.0f;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(channelId);
    argStream.ReadNumber(vx);
    argStream.ReadNumber(vy);
    argStream.ReadNumber(vz);

    if (argStream.HasErrors() || channelId == 0)
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    if (!g_pCore->FMODSetChannelVelocity(channelId, vx, vy, vz)) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushboolean(luaVM, true);
    return 1;
}

// fmodSetSoundLooped(channelId, loop) -> bool
// Changes the loop mode of an already-playing channel.
int CLuaFMODDefs::FMODSetSoundLooped(lua_State* luaVM)
{
    uint32_t channelId = 0;
    bool     bLoop     = false;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(channelId);
    argStream.ReadBool(bLoop);

    if (argStream.HasErrors() || channelId == 0)
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    if (!g_pCore->FMODSetChannelLooped(channelId, bLoop)) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushboolean(luaVM, true);
    return 1;
}

// ─── Channel read-back ────────────────────────────────────────────────────────

// fmodGetSoundVolume(channelId) -> volume (0.0 – 1.0) or false, errCode, errStr
int CLuaFMODDefs::FMODGetSoundVolume(lua_State* luaVM)
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

    float outVolume = 0.0f;
    if (!g_pCore->FMODGetChannelVolume(channelId, outVolume)) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushnumber(luaVM, static_cast<lua_Number>(outVolume));
    return 1;
}

// fmodGetSoundPitch(channelId) -> pitch or false, errCode, errStr
int CLuaFMODDefs::FMODGetSoundPitch(lua_State* luaVM)
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

    float outPitch = 1.0f;
    if (!g_pCore->FMODGetChannelPitch(channelId, outPitch)) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushnumber(luaVM, static_cast<lua_Number>(outPitch));
    return 1;
}

// fmodGetSoundPosition(channelId) -> x, y, z  or  false, errCode, errStr
// Only works for 3D channels. Returns false for 2D sounds.
int CLuaFMODDefs::FMODGetSoundPosition(lua_State* luaVM)
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

    float outX = 0.f, outY = 0.f, outZ = 0.f;
    if (!g_pCore->FMODGetChannelPosition3D(channelId, outX, outY, outZ)) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushnumber(luaVM, static_cast<lua_Number>(outX));
    lua_pushnumber(luaVM, static_cast<lua_Number>(outY));
    lua_pushnumber(luaVM, static_cast<lua_Number>(outZ));
    return 3;
}

// fmodGetSoundLooped(channelId) -> bool  or  false, errCode, errStr
int CLuaFMODDefs::FMODGetSoundLooped(lua_State* luaVM)
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

    bool bLooped = false;
    if (!g_pCore->FMODGetChannelLooped(channelId, bLooped)) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushboolean(luaVM, bLooped);
    return 1;
}

// ─── Reverb ───────────────────────────────────────────────────────────────────

// fmodSetReverbPreset(preset) -> bool
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

// fmodSetChannelReverb(channelId, wet) -> bool
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

    if (!g_pCore->FMODSetChannelReverbWet(channelId, wet)) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushboolean(luaVM, true);
    return 1;
}

// ─── Echo DSP ─────────────────────────────────────────────────────────────────

// fmodSetChannelEcho(channelId, delayMS, feedbackPct, wetDB) -> bool
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

    if (!g_pCore->FMODApplyChannelEcho(channelId, delayMS, feedbackPct, wetDB)) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushboolean(luaVM, true);
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

    if (!g_pCore->FMODRemoveChannelEcho(channelId)) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushboolean(luaVM, true);
    return 1;
}

// ─── Master volume ────────────────────────────────────────────────────────────

// fmodSetMasterVolume(volume) -> bool  (0.0 – 1.0, default 0.5)
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

// ─── Named parameters ─────────────────────────────────────────────────────────

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
