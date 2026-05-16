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

std::unordered_map<void*, std::vector<uint32_t>> CLuaFMODDefs::s_resourceSounds;

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
        // Low-pass / high-pass filters
        {"fmodSetChannelLowPass",     FMODSetChannelLowPass},
        {"fmodRemoveChannelLowPass",  FMODRemoveChannelLowPass},
        {"fmodSetChannelHighPass",    FMODSetChannelHighPass},
        {"fmodRemoveChannelHighPass", FMODRemoveChannelHighPass},
        // Flanger
        {"fmodSetChannelFlanger",     FMODSetChannelFlanger},
        {"fmodRemoveChannelFlanger",  FMODRemoveChannelFlanger},
        // Chorus
        {"fmodSetChannelChorus",      FMODSetChannelChorus},
        {"fmodRemoveChannelChorus",   FMODRemoveChannelChorus},
        // Distortion
        {"fmodSetChannelDistortion",     FMODSetChannelDistortion},
        {"fmodRemoveChannelDistortion",  FMODRemoveChannelDistortion},
        // Volume categories
        {"fmodSetChannelCategory", FMODSetChannelCategory},
        {"fmodSetCategoryVolume",  FMODSetCategoryVolume},
        {"fmodGetCategoryVolume",  FMODGetCategoryVolume},
        // Master volume
        {"fmodSetMasterVolume",   FMODSetMasterVolume},
        {"fmodGetMasterVolume",   FMODGetMasterVolume},
        // Named parameters
        {"fmodSetParameter",      FMODSetParameter},
        {"fmodGetParameter",      FMODGetParameter},
        // DSP chain query
        {"fmodGetChannelEffects",  FMODGetChannelEffects},
        // Occlusion / obstruction
        {"fmodSetChannelOcclusion", FMODSetChannelOcclusion},
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

    TrackSound(luaVM, soundId);
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

    TrackSound(luaVM, soundId);
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

    TrackSound(luaVM, soundId);
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

    UntrackSound(soundId);
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

// ─── Low-pass filter ─────────────────────────────────────────────────────────

// fmodSetChannelLowPass(channelId, cutoffHz [, resonance=1.0]) -> bool
int CLuaFMODDefs::FMODSetChannelLowPass(lua_State* luaVM)
{
    uint32_t channelId = 0;
    float    cutoffHz  = 5000.0f;
    float    resonance = 1.0f;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(channelId);
    argStream.ReadNumber(cutoffHz);
    argStream.ReadNumber(resonance, 1.0f);

    if (argStream.HasErrors() || channelId == 0)
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    if (!g_pCore->FMODApplyChannelLowPass(channelId, cutoffHz, resonance)) { FMOD_PUSH_ERROR(luaVM); }
    lua_pushboolean(luaVM, true);
    return 1;
}

// fmodRemoveChannelLowPass(channelId) -> bool
int CLuaFMODDefs::FMODRemoveChannelLowPass(lua_State* luaVM)
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

    if (!g_pCore->FMODRemoveChannelLowPass(channelId)) { FMOD_PUSH_ERROR(luaVM); }
    lua_pushboolean(luaVM, true);
    return 1;
}

// ─── High-pass filter ────────────────────────────────────────────────────────

// fmodSetChannelHighPass(channelId, cutoffHz [, resonance=1.0]) -> bool
int CLuaFMODDefs::FMODSetChannelHighPass(lua_State* luaVM)
{
    uint32_t channelId = 0;
    float    cutoffHz  = 250.0f;
    float    resonance = 1.0f;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(channelId);
    argStream.ReadNumber(cutoffHz);
    argStream.ReadNumber(resonance, 1.0f);

    if (argStream.HasErrors() || channelId == 0)
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    if (!g_pCore->FMODApplyChannelHighPass(channelId, cutoffHz, resonance)) { FMOD_PUSH_ERROR(luaVM); }
    lua_pushboolean(luaVM, true);
    return 1;
}

// fmodRemoveChannelHighPass(channelId) -> bool
int CLuaFMODDefs::FMODRemoveChannelHighPass(lua_State* luaVM)
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

    if (!g_pCore->FMODRemoveChannelHighPass(channelId)) { FMOD_PUSH_ERROR(luaVM); }
    lua_pushboolean(luaVM, true);
    return 1;
}

// ─── Flanger DSP ─────────────────────────────────────────────────────────────

// fmodSetChannelFlanger(channelId, mix, depth, rate) -> bool
// mix: 0–100%, depth: 0.01–1.0, rate: 0–20 Hz
int CLuaFMODDefs::FMODSetChannelFlanger(lua_State* luaVM)
{
    uint32_t channelId = 0;
    float    mix       = 50.0f;
    float    depth     = 1.0f;
    float    rate      = 0.1f;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(channelId);
    argStream.ReadNumber(mix);
    argStream.ReadNumber(depth);
    argStream.ReadNumber(rate);

    if (argStream.HasErrors() || channelId == 0)
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    if (!g_pCore->FMODApplyChannelFlanger(channelId, mix, depth, rate)) { FMOD_PUSH_ERROR(luaVM); }
    lua_pushboolean(luaVM, true);
    return 1;
}

// fmodRemoveChannelFlanger(channelId) -> bool
int CLuaFMODDefs::FMODRemoveChannelFlanger(lua_State* luaVM)
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

    if (!g_pCore->FMODRemoveChannelFlanger(channelId)) { FMOD_PUSH_ERROR(luaVM); }
    lua_pushboolean(luaVM, true);
    return 1;
}

// ─── Chorus DSP ──────────────────────────────────────────────────────────────

// fmodSetChannelChorus(channelId, mix, depth, rate) -> bool
// mix: 0–100%, depth: 0–100%, rate: 0–20 Hz
int CLuaFMODDefs::FMODSetChannelChorus(lua_State* luaVM)
{
    uint32_t channelId = 0;
    float    mix       = 50.0f;
    float    depth     = 50.0f;
    float    rate      = 0.8f;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(channelId);
    argStream.ReadNumber(mix);
    argStream.ReadNumber(depth);
    argStream.ReadNumber(rate);

    if (argStream.HasErrors() || channelId == 0)
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    if (!g_pCore->FMODApplyChannelChorus(channelId, mix, depth, rate)) { FMOD_PUSH_ERROR(luaVM); }
    lua_pushboolean(luaVM, true);
    return 1;
}

// fmodRemoveChannelChorus(channelId) -> bool
int CLuaFMODDefs::FMODRemoveChannelChorus(lua_State* luaVM)
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

    if (!g_pCore->FMODRemoveChannelChorus(channelId)) { FMOD_PUSH_ERROR(luaVM); }
    lua_pushboolean(luaVM, true);
    return 1;
}

// ─── Distortion DSP ──────────────────────────────────────────────────────────

// fmodSetChannelDistortion(channelId, level) -> bool
// level: 0.0 (clean) – 1.0 (full clip)
int CLuaFMODDefs::FMODSetChannelDistortion(lua_State* luaVM)
{
    uint32_t channelId = 0;
    float    level     = 0.5f;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(channelId);
    argStream.ReadNumber(level);

    if (argStream.HasErrors() || channelId == 0)
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    if (!g_pCore->FMODApplyChannelDistortion(channelId, level)) { FMOD_PUSH_ERROR(luaVM); }
    lua_pushboolean(luaVM, true);
    return 1;
}

// fmodRemoveChannelDistortion(channelId) -> bool
int CLuaFMODDefs::FMODRemoveChannelDistortion(lua_State* luaVM)
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

    if (!g_pCore->FMODRemoveChannelDistortion(channelId)) { FMOD_PUSH_ERROR(luaVM); }
    lua_pushboolean(luaVM, true);
    return 1;
}

// ─── Volume categories ────────────────────────────────────────────────────────

// fmodSetChannelCategory(channelId, category) -> bool
// category: "sfx"|"ambient"|"music" or 0/1/2
int CLuaFMODDefs::FMODSetChannelCategory(lua_State* luaVM)
{
    uint32_t channelId = 0;
    int      category  = 0;

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(channelId);

    if (argStream.HasErrors() || channelId == 0)
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    if (lua_type(luaVM, 2) == LUA_TSTRING)
    {
        SString strCat;
        argStream.ReadString(strCat);
        category = StringToCategory(strCat);
    }
    else
    {
        argStream.ReadNumber(category, 0);
    }

    if (!g_pCore->FMODSetChannelCategory(channelId, category)) { FMOD_PUSH_ERROR(luaVM); }
    lua_pushboolean(luaVM, true);
    return 1;
}

// fmodSetCategoryVolume(category, volume) -> bool
int CLuaFMODDefs::FMODSetCategoryVolume(lua_State* luaVM)
{
    int   category = 0;
    float volume   = 1.0f;

    CScriptArgReader argStream(luaVM);

    if (lua_type(luaVM, 1) == LUA_TSTRING)
    {
        SString strCat;
        argStream.ReadString(strCat);
        category = StringToCategory(strCat);
    }
    else
    {
        argStream.ReadNumber(category, 0);
    }
    argStream.ReadNumber(volume);

    if (argStream.HasErrors())
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    volume = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
    if (!g_pCore->FMODSetCategoryVolume(category, volume)) { FMOD_PUSH_ERROR(luaVM); }
    lua_pushboolean(luaVM, true);
    return 1;
}

// fmodGetCategoryVolume(category) -> number
int CLuaFMODDefs::FMODGetCategoryVolume(lua_State* luaVM)
{
    int category = 0;

    CScriptArgReader argStream(luaVM);

    if (lua_type(luaVM, 1) == LUA_TSTRING)
    {
        SString strCat;
        argStream.ReadString(strCat);
        category = StringToCategory(strCat);
    }
    else
    {
        argStream.ReadNumber(category, 0);
    }

    lua_pushnumber(luaVM, static_cast<lua_Number>(g_pCore->FMODGetCategoryVolume(category)));
    return 1;
}

// ─── Resource tracking ────────────────────────────────────────────────────────

void CLuaFMODDefs::TrackSound(lua_State* luaVM, uint32_t soundId)
{
    CLuaMain* pLuaMain = m_pLuaManager->GetVirtualMachine(luaVM);
    if (pLuaMain)
        s_resourceSounds[static_cast<void*>(pLuaMain)].push_back(soundId);
}

void CLuaFMODDefs::UntrackSound(uint32_t soundId)
{
    for (auto& [key, vec] : s_resourceSounds)
    {
        for (auto it = vec.begin(); it != vec.end(); ++it)
        {
            if (*it == soundId)
            {
                vec.erase(it);
                return;
            }
        }
    }
}

int CLuaFMODDefs::StringToCategory(const SString& name)
{
    if (name == "ambient") return 1;
    if (name == "music")   return 2;
    return 0;  // "sfx" or unrecognised → SFX
}

void CLuaFMODDefs::OnLuaMainRemoved(CLuaMain* pLuaMain)
{
    void* key = static_cast<void*>(pLuaMain);
    auto  it  = s_resourceSounds.find(key);
    if (it == s_resourceSounds.end())
        return;

    for (uint32_t soundId : it->second)
        g_pCore->FMODFreeSound(soundId);

    s_resourceSounds.erase(it);
}

// ─── DSP chain query ──────────────────────────────────────────────────────────

// fmodGetChannelEffects(channelId) -> table  e.g. {"echo","lowpass"}
// Returns an empty table when no DSPs are active; returns false when channelId is invalid.
int CLuaFMODDefs::FMODGetChannelEffects(lua_State* luaVM)
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

    SString csv;
    if (!g_pCore->FMODGetChannelEffects(channelId, csv))
    {
        // Channel not found — return false so scripts can distinguish from "no DSPs"
        lua_pushboolean(luaVM, false);
        return 1;
    }

    lua_newtable(luaVM);
    if (csv.empty())
        return 1;  // valid channel, zero active DSPs → empty table

    // Split comma-separated string into table entries
    int  idx   = 1;
    size_t start = 0;
    size_t end;
    while ((end = csv.find(',', start)) != SString::npos)
    {
        lua_pushlstring(luaVM, csv.c_str() + start, end - start);
        lua_rawseti(luaVM, -2, idx++);
        start = end + 1;
    }
    if (start < csv.size())
    {
        lua_pushstring(luaVM, csv.c_str() + start);
        lua_rawseti(luaVM, -2, idx);
    }
    return 1;
}

// ─── Occlusion / obstruction ─────────────────────────────────────────────────

// fmodSetChannelOcclusion(channelId, directOcclusion [, reverbOcclusion]) -> bool
// directOcclusion  : 0.0 (no block) – 1.0 (fully blocked); FMOD applies frequency-dependent LPF
// reverbOcclusion  : optional, defaults to directOcclusion * 0.5
// Only effective on 3D channels; call with 0, 0 to clear.
int CLuaFMODDefs::FMODSetChannelOcclusion(lua_State* luaVM)
{
    uint32_t channelId       = 0;
    float    directOcclusion = 0.0f;
    float    reverbOcclusion = -1.0f;   // sentinel → default to half of direct

    CScriptArgReader argStream(luaVM);
    argStream.ReadNumber(channelId);
    argStream.ReadNumber(directOcclusion);
    argStream.ReadNumber(reverbOcclusion, -1.0f);

    if (argStream.HasErrors() || channelId == 0)
    {
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());
        lua_pushboolean(luaVM, false);
        return 1;
    }

    if (reverbOcclusion < 0.0f)
        reverbOcclusion = directOcclusion * 0.5f;

    if (!g_pCore->FMODSetChannelOcclusion(channelId, directOcclusion, reverbOcclusion)) { FMOD_PUSH_ERROR(luaVM); }

    lua_pushboolean(luaVM, true);
    return 1;
}
