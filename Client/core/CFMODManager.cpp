/*****************************************************************************
 *
 *  PROJECT:     Midnight Purple:SA
 *  FILE:        core/CFMODManager.cpp
 *  PURPOSE:     FMOD audio engine manager
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CFMODManager.h"

CFMODManager::CFMODManager() : m_pSystem(nullptr), m_pMasterGroup(nullptr), m_reverbProps([]{ FMOD_REVERB_PROPERTIES p = FMOD_PRESET_OFF; return p; }())
{
    FMOD_RESULT result = FMOD::System_Create(&m_pSystem);
    if (result != FMOD_OK)
    {
        WriteDebugEvent(SString("CFMODManager: System_Create failed (error %d)", result));
        m_pSystem = nullptr;
        return;
    }

    // Force stereo output so 3D panning is always L/R (must be before init)
    m_pSystem->setSoftwareFormat(44100, FMOD_SPEAKERMODE_STEREO, 0);

    // GTA:SA is right-handed (Z=up, Y=north, X=east).
    // FMOD_INIT_3D_RIGHTHANDED makes FMOD's internal cross products match that convention,
    // giving correct left/right panning as the camera rotates.
    result = m_pSystem->init(512, FMOD_INIT_3D_RIGHTHANDED, nullptr);
    if (result != FMOD_OK)
    {
        WriteDebugEvent(SString("CFMODManager: init failed (error %d)", result));
        m_pSystem->release();
        m_pSystem = nullptr;
        return;
    }

    // 1 GTA unit ≈ 1 metre; distanceFactor=1.0 keeps FMOD distances in world units.
    m_pSystem->set3DSettings(1.0f, 1.0f, 1.0f);

    // All sounds route through the master channel group for global volume control.
    // Default 0.5 so FMOD doesn't overpower BASS (both mix at OS level).
    m_pSystem->getMasterChannelGroup(&m_pMasterGroup);
    if (m_pMasterGroup)
        m_pMasterGroup->setVolume(0.5f);

    WriteDebugEvent("CFMODManager: FMOD initialized (right-handed, stereo)");
}

CFMODManager::~CFMODManager()
{
    if (m_pSystem)
    {
        m_pSystem->release();
        m_pSystem = nullptr;
    }
}

void CFMODManager::Update(const CVector& vecPosition, const CVector& vecForward, const CVector& vecUp)
{
    if (!m_pSystem)
        return;

    FMOD_VECTOR pos     = {vecPosition.fX, vecPosition.fY, vecPosition.fZ};
    FMOD_VECTOR vel     = {0.0f, 0.0f, 0.0f};
    FMOD_VECTOR forward = {vecForward.fX, vecForward.fY, vecForward.fZ};
    FMOD_VECTOR up      = {vecUp.fX, vecUp.fY, vecUp.fZ};

    m_pSystem->set3DListenerAttributes(0, &pos, &vel, &forward, &up);

    // Purge finished channels so m_channels doesn't grow unbounded
    for (auto it = m_channels.begin(); it != m_channels.end();)
    {
        bool isPlaying = false;
        FMOD_RESULT r  = it->second.pChannel->isPlaying(&isPlaying);
        if (r == FMOD_ERR_INVALID_HANDLE || (r == FMOD_OK && !isPlaying))
        {
            if (it->second.pEchoDSP)
            {
                it->second.pEchoDSP->release();
                it->second.pEchoDSP = nullptr;
            }
            it = m_channels.erase(it);
        }
        else
            ++it;
    }

    m_pSystem->update();
}

uint32_t CFMODManager::CreateSound(const char* path, bool b3D, bool bLoop)
{
    if (!m_pSystem || !path)
        return 0;

    FMOD_MODE mode = FMOD_DEFAULT | FMOD_CREATECOMPRESSEDSAMPLE;
    if (b3D)
        mode |= FMOD_3D | FMOD_3D_LINEARROLLOFF;
    else
        mode |= FMOD_2D;
    if (bLoop)
        mode |= FMOD_LOOP_NORMAL;

    FMOD::Sound* pSound = nullptr;
    if (m_pSystem->createSound(path, mode, nullptr, &pSound) != FMOD_OK)
        return 0;

    uint32_t id = m_nextSoundId++;
    m_sounds[id] = {pSound, b3D};
    return id;
}

void CFMODManager::FreeSound(uint32_t soundId)
{
    auto it = m_sounds.find(soundId);
    if (it == m_sounds.end())
        return;

    it->second.pSound->release();
    m_sounds.erase(it);
}

uint32_t CFMODManager::PlaySound(uint32_t soundId, float x, float y, float z, float minDist, float maxDist)
{
    if (!m_pSystem)
        return 0;

    auto it = m_sounds.find(soundId);
    if (it == m_sounds.end())
        return 0;

    FMOD::Channel* pChannel = nullptr;
    // Start paused so we can set 3D attributes before the first audio frame.
    // Route through m_pMasterGroup so global volume applies.
    if (m_pSystem->playSound(it->second.pSound, m_pMasterGroup, true, &pChannel) != FMOD_OK)
        return 0;

    FMOD_VECTOR pos = {x, y, z};
    FMOD_VECTOR vel = {0.0f, 0.0f, 0.0f};
    pChannel->set3DAttributes(&pos, &vel);
    pChannel->set3DMinMaxDistance(minDist, maxDist);

    bool b3D = it->second.b3D;
    // Reverb is opt-in: call SetChannelReverbWet after PlaySound to enable reverb
    // on a specific channel. This avoids all sounds getting reverb by default.

    pChannel->setPaused(false);

    uint32_t channelId = m_nextChannelId++;
    m_channels[channelId] = {pChannel, nullptr, b3D, -999.0f};   // pEchoDSP=nullptr, reverbWet=-999 (not opted in)
    return channelId;
}

FMOD::Channel* CFMODManager::GetChannel(uint32_t channelId)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end())
        return nullptr;

    bool isPlaying = false;
    FMOD_RESULT result = it->second.pChannel->isPlaying(&isPlaying);

    // Remove stale entries: handle recycled by FMOD, or non-looping sound finished
    if (result == FMOD_ERR_INVALID_HANDLE || (result == FMOD_OK && !isPlaying))
    {
        if (it->second.pEchoDSP)
        {
            it->second.pEchoDSP->release();
            it->second.pEchoDSP = nullptr;
        }
        m_channels.erase(it);
        return nullptr;
    }

    return it->second.pChannel;
}

bool CFMODManager::StopChannel(uint32_t channelId)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end())
        return false;

    ChannelInfo& info = it->second;

    // Check handle validity without erasing (GetChannel would erase before we can clean up DSPs)
    bool isPlaying = false;
    FMOD_RESULT r  = info.pChannel->isPlaying(&isPlaying);

    // Release echo DSP in all cases — channel is about to be removed
    if (info.pEchoDSP)
    {
        if (r != FMOD_ERR_INVALID_HANDLE)
            info.pChannel->removeDSP(info.pEchoDSP);
        info.pEchoDSP->release();
        info.pEchoDSP = nullptr;
    }

    if (r != FMOD_ERR_INVALID_HANDLE)
        info.pChannel->stop();

    m_channels.erase(it);
    return true;
}

bool CFMODManager::SetChannelVolume(uint32_t channelId, float volume)
{
    FMOD::Channel* pChannel = GetChannel(channelId);
    if (!pChannel)
        return false;

    pChannel->setVolume(volume);
    return true;
}

bool CFMODManager::SetChannelPitch(uint32_t channelId, float pitch)
{
    FMOD::Channel* pChannel = GetChannel(channelId);
    if (!pChannel)
        return false;

    pChannel->setPitch(pitch);
    return true;
}

bool CFMODManager::SetChannelPosition(uint32_t channelId, float x, float y, float z)
{
    FMOD::Channel* pChannel = GetChannel(channelId);
    if (!pChannel)
        return false;

    FMOD_VECTOR pos = {x, y, z};
    FMOD_VECTOR vel = {0.0f, 0.0f, 0.0f};
    pChannel->set3DAttributes(&pos, &vel);
    return true;
}

bool CFMODManager::IsChannelPlaying(uint32_t channelId)
{
    FMOD::Channel* pChannel = GetChannel(channelId);
    if (!pChannel)
        return false;

    bool isPlaying = false;
    pChannel->isPlaying(&isPlaying);
    return isPlaying;
}

void CFMODManager::SetReverbPreset(const char* presetName)
{
    if (!m_pSystem || !presetName)
        return;

    FMOD_REVERB_PROPERTIES props;
    std::string name(presetName);

    if      (name == "generic")         { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_GENERIC;          props = p; }
    else if (name == "paddedcell")      { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_PADDEDCELL;       props = p; }
    else if (name == "room")            { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_ROOM;             props = p; }
    else if (name == "bathroom")        { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_BATHROOM;         props = p; }
    else if (name == "livingroom")      { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_LIVINGROOM;       props = p; }
    else if (name == "stoneroom")       { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_STONEROOM;        props = p; }
    else if (name == "auditorium")      { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_AUDITORIUM;       props = p; }
    else if (name == "concerthall")     { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_CONCERTHALL;      props = p; }
    else if (name == "cave")            { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_CAVE;             props = p; }
    else if (name == "arena")           { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_ARENA;            props = p; }
    else if (name == "hangar")          { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_HANGAR;           props = p; }
    else if (name == "hallway")         { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_HALLWAY;          props = p; }
    else if (name == "stonecorridor")   { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_STONECORRIDOR;    props = p; }
    else if (name == "alley")           { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_ALLEY;            props = p; }
    else if (name == "forest")          { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_FOREST;           props = p; }
    else if (name == "city")            { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_CITY;             props = p; }
    else if (name == "mountains")       { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_MOUNTAINS;        props = p; }
    else if (name == "quarry")          { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_QUARRY;           props = p; }
    else if (name == "plain")           { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_PLAIN;            props = p; }
    else if (name == "parkinglot")      { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_PARKINGLOT;       props = p; }
    else if (name == "sewerpipe")       { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_SEWERPIPE;        props = p; }
    else if (name == "underwater")      { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_UNDERWATER;       props = p; }
    else                                { FMOD_REVERB_PROPERTIES p = FMOD_PRESET_OFF;              props = p; }

    m_reverbProps = props;
    m_pSystem->setReverbProperties(0, &m_reverbProps);

    // Re-apply each channel's stored wet level so FMOD re-connects them to the
    // updated reverb DSP. Only channels that explicitly opted in (reverbWet != -999)
    // are updated, preserving per-channel user settings.
    for (auto& [id, info] : m_channels)
    {
        if (info.reverbWet < -900.0f)
            continue;   // not opted in
        bool isPlaying = false;
        if (info.pChannel->isPlaying(&isPlaying) == FMOD_OK && isPlaying)
            info.pChannel->setReverbProperties(0, info.reverbWet);
    }
}

bool CFMODManager::SetChannelReverbWet(uint32_t channelId, float wetDB)
{
    // Find the entry directly so we can also store the wet value.
    auto it = m_channels.find(channelId);
    if (it == m_channels.end())
        return false;

    FMOD::Channel* pChannel = GetChannel(channelId);   // also validates the handle
    if (!pChannel)
        return false;

    it->second.reverbWet = wetDB;
    pChannel->setReverbProperties(0, wetDB);
    return true;
}

void CFMODManager::SetReverbWetLevel(float wetDB)
{
    if (!m_pSystem)
        return;

    m_reverbProps.WetLevel = wetDB;
    m_pSystem->setReverbProperties(0, &m_reverbProps);

    // Re-connect opted-in channels so they pick up the new wet level immediately.
    for (auto& [id, info] : m_channels)
    {
        if (info.reverbWet < -900.0f)
            continue;
        bool isPlaying = false;
        if (info.pChannel->isPlaying(&isPlaying) == FMOD_OK && isPlaying)
            info.pChannel->setReverbProperties(0, info.reverbWet);
    }
}

bool CFMODManager::ApplyChannelEcho(uint32_t channelId, float delayMS, float feedbackPct, float wetDB)
{
    if (!m_pSystem)
        return false;

    auto it = m_channels.find(channelId);
    if (it == m_channels.end())
        return false;

    ChannelInfo& info = it->second;

    // Validate channel is still live before touching its DSP chain
    bool isPlaying = false;
    FMOD_RESULT r  = info.pChannel->isPlaying(&isPlaying);
    if (r == FMOD_ERR_INVALID_HANDLE || (r == FMOD_OK && !isPlaying))
    {
        if (info.pEchoDSP) { info.pEchoDSP->release(); info.pEchoDSP = nullptr; }
        m_channels.erase(it);
        return false;
    }

    if (info.pEchoDSP)
    {
        // Already present — update parameters in-place (no remove/re-add needed)
        info.pEchoDSP->setParameterFloat(FMOD_DSP_ECHO_DELAY,    delayMS);
        info.pEchoDSP->setParameterFloat(FMOD_DSP_ECHO_FEEDBACK,  feedbackPct);
        info.pEchoDSP->setParameterFloat(FMOD_DSP_ECHO_WETLEVEL,  wetDB);
    }
    else
    {
        FMOD::DSP* pDSP = nullptr;
        if (m_pSystem->createDSPByType(FMOD_DSP_TYPE_ECHO, &pDSP) != FMOD_OK)
            return false;

        pDSP->setParameterFloat(FMOD_DSP_ECHO_DELAY,    delayMS);
        pDSP->setParameterFloat(FMOD_DSP_ECHO_FEEDBACK,  feedbackPct);
        pDSP->setParameterFloat(FMOD_DSP_ECHO_DRYLEVEL,  0.0f);   // dry signal unaffected
        pDSP->setParameterFloat(FMOD_DSP_ECHO_WETLEVEL,  wetDB);

        if (info.pChannel->addDSP(FMOD_CHANNELCONTROL_DSP_HEAD, pDSP) != FMOD_OK)
        {
            pDSP->release();
            return false;
        }

        info.pEchoDSP = pDSP;
    }

    return true;
}

bool CFMODManager::RemoveChannelEcho(uint32_t channelId)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end())
        return false;

    ChannelInfo& info = it->second;
    if (!info.pEchoDSP)
        return false;

    bool isPlaying = false;
    FMOD_RESULT r  = info.pChannel->isPlaying(&isPlaying);
    if (r != FMOD_ERR_INVALID_HANDLE)
        info.pChannel->removeDSP(info.pEchoDSP);

    info.pEchoDSP->release();
    info.pEchoDSP = nullptr;
    return true;
}

void CFMODManager::SetParameter(const char* name, float value)
{
    if (name)
        m_parameters[name] = value;
}

float CFMODManager::GetParameter(const char* name, float defaultValue) const
{
    if (!name)
        return defaultValue;
    auto it = m_parameters.find(name);
    return (it != m_parameters.end()) ? it->second : defaultValue;
}

void CFMODManager::SetMasterVolume(float volume)
{
    if (m_pMasterGroup)
        m_pMasterGroup->setVolume(volume);
}

float CFMODManager::GetMasterVolume() const
{
    if (!m_pMasterGroup)
        return 0.0f;
    float volume = 0.0f;
    m_pMasterGroup->getVolume(&volume);
    return volume;
}
