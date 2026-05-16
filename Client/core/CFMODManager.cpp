/*****************************************************************************
 *
 *  PROJECT:     Midnight Purple:SA
 *  FILE:        core/CFMODManager.cpp
 *  PURPOSE:     FMOD audio engine manager
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CFMODManager.h"
#include <fmod_errors.h>
#include <cmath>

// ─── Static helpers ───────────────────────────────────────────────────────────

float CFMODManager::DBToLinear(float dB) noexcept
{
    if (dB <= -80.0f)
        return 0.0f;
    float linear = std::pow(10.0f, dB / 20.0f);
    return linear > 1.0f ? 1.0f : linear;
}

// ─── Constructor / destructor ─────────────────────────────────────────────────

CFMODManager::CFMODManager() : m_pSystem(nullptr), m_pMasterGroup(nullptr),
    m_reverbProps([]{ FMOD_REVERB_PROPERTIES p = FMOD_PRESET_OFF; return p; }())
{
    m_lastResult = FMOD::System_Create(&m_pSystem);
    if (m_lastResult != FMOD_OK)
    {
        WriteDebugEvent(SString("CFMODManager: System_Create failed (%s)", GetLastErrorString()));
        m_pSystem = nullptr;
        return;
    }

    m_pSystem->setSoftwareFormat(44100, FMOD_SPEAKERMODE_STEREO, 0);

    // GTA:SA is right-handed (Z=up, Y=north, X=east).
    m_lastResult = m_pSystem->init(512, FMOD_INIT_3D_RIGHTHANDED, nullptr);
    if (m_lastResult != FMOD_OK)
    {
        WriteDebugEvent(SString("CFMODManager: init failed (%s)", GetLastErrorString()));
        m_pSystem->release();
        m_pSystem = nullptr;
        return;
    }

    // 1 GTA unit ≈ 1 metre
    m_pSystem->set3DSettings(1.0f, 1.0f, 1.0f);

    m_pSystem->getMasterChannelGroup(&m_pMasterGroup);
    if (m_pMasterGroup)
        m_pMasterGroup->setVolume(0.5f);

    // Volume categories: SFX, Ambient, Music — all sub-groups of master
    const char* catNames[CAT_COUNT] = {"SFX", "Ambient", "Music"};
    for (int i = 0; i < CAT_COUNT; ++i)
    {
        m_pSystem->createChannelGroup(catNames[i], &m_pCategoryGroups[i]);
        if (m_pCategoryGroups[i] && m_pMasterGroup)
            m_pMasterGroup->addGroup(m_pCategoryGroups[i]);
    }

    WriteDebugEvent(SString("CFMODManager: initialized — version %s", GetVersion().c_str()));
}

CFMODManager::~CFMODManager()
{
    if (m_pSystem)
    {
        m_pSystem->release();
        m_pSystem = nullptr;
    }
}

const char* CFMODManager::GetLastErrorString() const noexcept
{
    return FMOD_ErrorString(m_lastResult);
}

SString CFMODManager::GetVersion() const
{
    if (!m_pSystem)
        return "unavailable";

    unsigned int ver = 0;
    m_pSystem->getVersion(&ver);

    unsigned int major = (ver >> 16) & 0xff;
    unsigned int minor = (ver >>  8) & 0xff;
    unsigned int patch =  ver        & 0xff;
    return SString("%u.%02u.%02u", major, minor, patch);
}

// ─── Private helpers ──────────────────────────────────────────────────────────

FMOD::ChannelGroup* CFMODManager::GetCategoryGroup(int category) const
{
    if (category >= 0 && category < CAT_COUNT && m_pCategoryGroups[category])
        return m_pCategoryGroups[category];
    return m_pMasterGroup;
}

void CFMODManager::ReleaseChannelDSPs(uint32_t channelId, bool removeFirst)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end())
        return;

    ChannelInfo& info = it->second;
    FMOD::DSP** slots[] = {
        &info.pEchoDSP, &info.pLowPassDSP, &info.pHighPassDSP,
        &info.pFlangerDSP, &info.pChorusDSP, &info.pDistortionDSP
    };
    for (FMOD::DSP** pp : slots)
    {
        if (*pp)
        {
            if (removeFirst)
                info.pChannel->removeDSP(*pp);
            (*pp)->release();
            *pp = nullptr;
        }
    }
}

// ─── Frame update ─────────────────────────────────────────────────────────────

void CFMODManager::Update(const CVector& vecPosition, const CVector& vecForward, const CVector& vecUp)
{
    if (!m_pSystem)
        return;

    FMOD_VECTOR pos     = {vecPosition.fX, vecPosition.fY, vecPosition.fZ};
    FMOD_VECTOR vel     = {0.0f, 0.0f, 0.0f};
    FMOD_VECTOR forward = {vecForward.fX, vecForward.fY, vecForward.fZ};
    FMOD_VECTOR up      = {vecUp.fX, vecUp.fY, vecUp.fZ};

    m_pSystem->set3DListenerAttributes(0, &pos, &vel, &forward, &up);

    // Purge finished channels — release all attached DSPs without removeDSP
    // (channel is already dead, removeDSP would be a no-op or error)
    for (auto it = m_channels.begin(); it != m_channels.end();)
    {
        bool isPlaying = false;
        FMOD_RESULT r  = it->second.pChannel->isPlaying(&isPlaying);
        if (r == FMOD_ERR_INVALID_HANDLE || (r == FMOD_OK && !isPlaying))
        {
            ReleaseChannelDSPs(it->first, false);
            it = m_channels.erase(it);
        }
        else
            ++it;
    }

    m_pSystem->update();
}

// ─── Internal helper ─────────────────────────────────────────────────────────

uint32_t CFMODManager::CreateSoundInternal(const char* path, bool b3D, bool bLoop, bool bStream)
{
    if (!m_pSystem || !path)
    {
        m_lastResult = FMOD_ERR_INVALID_PARAM;
        return 0;
    }

    FMOD_MODE mode = FMOD_DEFAULT;
    mode |= bStream ? FMOD_CREATESTREAM : FMOD_CREATECOMPRESSEDSAMPLE;
    mode |= b3D ? (FMOD_3D | FMOD_3D_LINEARROLLOFF) : FMOD_2D;
    if (bLoop)
        mode |= FMOD_LOOP_NORMAL;

    FMOD::Sound* pSound = nullptr;
    m_lastResult = m_pSystem->createSound(path, mode, nullptr, &pSound);
    if (m_lastResult != FMOD_OK)
        return 0;

    uint32_t id      = m_nextSoundId++;
    m_sounds[id]     = {pSound, b3D};
    return id;
}

// ─── Sound lifetime ───────────────────────────────────────────────────────────

uint32_t CFMODManager::CreateSound(const char* path, bool b3D, bool bLoop)
{
    return CreateSoundInternal(path, b3D, bLoop, false);
}

uint32_t CFMODManager::CreateStream(const char* path, bool b3D, bool bLoop)
{
    return CreateSoundInternal(path, b3D, bLoop, true);
}

uint32_t CFMODManager::CreateSoundFromMemory(const void* pData, size_t dataSize, bool b3D, bool bLoop)
{
    if (!m_pSystem || !pData || dataSize == 0)
    {
        m_lastResult = FMOD_ERR_INVALID_PARAM;
        return 0;
    }

    FMOD_CREATESOUNDEXINFO exinfo{};
    exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
    exinfo.length = static_cast<unsigned int>(dataSize);

    FMOD_MODE mode = FMOD_OPENMEMORY;
    mode |= b3D ? (FMOD_3D | FMOD_3D_LINEARROLLOFF) : FMOD_2D;
    if (bLoop)
        mode |= FMOD_LOOP_NORMAL;

    FMOD::Sound* pSound = nullptr;
    m_lastResult        = m_pSystem->createSound(static_cast<const char*>(pData), mode, &exinfo, &pSound);
    if (m_lastResult != FMOD_OK)
        return 0;

    uint32_t id  = m_nextSoundId++;
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

// ─── Playback ─────────────────────────────────────────────────────────────────

uint32_t CFMODManager::PlaySound(uint32_t soundId, float x, float y, float z, float minDist, float maxDist)
{
    if (!m_pSystem)
    {
        m_lastResult = FMOD_ERR_UNINITIALIZED;
        return 0;
    }

    auto it = m_sounds.find(soundId);
    if (it == m_sounds.end())
    {
        m_lastResult = FMOD_ERR_INVALID_HANDLE;
        return 0;
    }

    // Default to SFX category group; call SetChannelCategory to move it later
    FMOD::ChannelGroup* pGroup  = GetCategoryGroup(CAT_SFX);
    FMOD::Channel*      pChannel = nullptr;
    m_lastResult = m_pSystem->playSound(it->second.pSound, pGroup, true, &pChannel);
    if (m_lastResult != FMOD_OK)
        return 0;

    FMOD_VECTOR pos = {x, y, z};
    FMOD_VECTOR vel = {0.0f, 0.0f, 0.0f};
    pChannel->set3DAttributes(&pos, &vel);
    pChannel->set3DMinMaxDistance(minDist, maxDist);
    pChannel->setPaused(false);

    uint32_t channelId    = m_nextChannelId++;
    m_channels[channelId] = {pChannel, {}, {}, {}, {}, {}, {}, it->second.b3D, CAT_SFX, -999.0f};
    return channelId;
}

// ─── Channel control ──────────────────────────────────────────────────────────

FMOD::Channel* CFMODManager::GetChannel(uint32_t channelId)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end())
    {
        m_lastResult = FMOD_ERR_INVALID_HANDLE;
        return nullptr;
    }

    bool        isPlaying = false;
    FMOD_RESULT r         = it->second.pChannel->isPlaying(&isPlaying);

    if (r == FMOD_ERR_INVALID_HANDLE || (r == FMOD_OK && !isPlaying))
    {
        ReleaseChannelDSPs(channelId, false);
        m_channels.erase(it);
        m_lastResult = FMOD_ERR_INVALID_HANDLE;
        return nullptr;
    }

    m_lastResult = FMOD_OK;
    return it->second.pChannel;
}

bool CFMODManager::StopChannel(uint32_t channelId)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end())
    {
        m_lastResult = FMOD_ERR_INVALID_HANDLE;
        return false;
    }

    ChannelInfo& info = it->second;

    bool        isPlaying = false;
    FMOD_RESULT r         = info.pChannel->isPlaying(&isPlaying);

    // Remove DSPs before stopping so FMOD cleans up the DSP chain properly
    ReleaseChannelDSPs(channelId, r != FMOD_ERR_INVALID_HANDLE);

    if (r != FMOD_ERR_INVALID_HANDLE)
        info.pChannel->stop();

    m_channels.erase(it);
    m_lastResult = FMOD_OK;
    return true;
}

bool CFMODManager::PauseChannel(uint32_t channelId, bool bPause)
{
    FMOD::Channel* pChannel = GetChannel(channelId);
    if (!pChannel)
        return false;

    m_lastResult = pChannel->setPaused(bPause);
    return m_lastResult == FMOD_OK;
}

bool CFMODManager::IsChannelPaused(uint32_t channelId)
{
    FMOD::Channel* pChannel = GetChannel(channelId);
    if (!pChannel)
        return false;

    bool bPaused  = false;
    m_lastResult  = pChannel->getPaused(&bPaused);
    return bPaused;
}

bool CFMODManager::SetChannelVolume(uint32_t channelId, float volume)
{
    FMOD::Channel* pChannel = GetChannel(channelId);
    if (!pChannel)
        return false;

    m_lastResult = pChannel->setVolume(volume);
    return m_lastResult == FMOD_OK;
}

bool CFMODManager::SetChannelPitch(uint32_t channelId, float pitch)
{
    FMOD::Channel* pChannel = GetChannel(channelId);
    if (!pChannel)
        return false;

    m_lastResult = pChannel->setPitch(pitch);
    return m_lastResult == FMOD_OK;
}

bool CFMODManager::SetChannelPosition(uint32_t channelId, float x, float y, float z)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end())
    {
        m_lastResult = FMOD_ERR_INVALID_HANDLE;
        return false;
    }

    FMOD::Channel* pChannel = GetChannel(channelId);
    if (!pChannel)
        return false;

    FMOD_VECTOR pos = {x, y, z};
    FMOD_VECTOR vel = {it->second.velX, it->second.velY, it->second.velZ};
    m_lastResult    = pChannel->set3DAttributes(&pos, &vel);
    return m_lastResult == FMOD_OK;
}

bool CFMODManager::SetChannelVelocity(uint32_t channelId, float vx, float vy, float vz)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end())
    {
        m_lastResult = FMOD_ERR_INVALID_HANDLE;
        return false;
    }

    if (!it->second.b3D)
    {
        m_lastResult = FMOD_ERR_INVALID_PARAM;
        return false;
    }

    FMOD::Channel* pChannel = GetChannel(channelId);
    if (!pChannel)
        return false;

    FMOD_VECTOR currentPos = {0.f, 0.f, 0.f};
    FMOD_VECTOR oldVel     = {0.f, 0.f, 0.f};
    pChannel->get3DAttributes(&currentPos, &oldVel);

    it->second.velX = vx;
    it->second.velY = vy;
    it->second.velZ = vz;

    FMOD_VECTOR newVel = {vx, vy, vz};
    m_lastResult       = pChannel->set3DAttributes(&currentPos, &newVel);
    return m_lastResult == FMOD_OK;
}

bool CFMODManager::SetChannelLooped(uint32_t channelId, bool bLoop)
{
    FMOD::Channel* pChannel = GetChannel(channelId);
    if (!pChannel)
        return false;

    FMOD_MODE currentMode = 0;
    pChannel->getMode(&currentMode);
    currentMode &= ~(FMOD_LOOP_NORMAL | FMOD_LOOP_BIDI | FMOD_LOOP_OFF);
    currentMode |= bLoop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;

    m_lastResult = pChannel->setMode(currentMode);
    return m_lastResult == FMOD_OK;
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

// ─── Channel read-back ────────────────────────────────────────────────────────

bool CFMODManager::GetChannelLooped(uint32_t channelId, bool& outLooped)
{
    FMOD::Channel* pChannel = GetChannel(channelId);
    if (!pChannel)
        return false;

    FMOD_MODE mode = 0;
    m_lastResult   = pChannel->getMode(&mode);
    if (m_lastResult != FMOD_OK)
        return false;

    outLooped = (mode & FMOD_LOOP_NORMAL) != 0;
    return true;
}

bool CFMODManager::GetChannelPosition3D(uint32_t channelId, float& outX, float& outY, float& outZ)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end())
    {
        m_lastResult = FMOD_ERR_INVALID_HANDLE;
        return false;
    }

    if (!it->second.b3D)
    {
        m_lastResult = FMOD_ERR_INVALID_PARAM;
        return false;
    }

    FMOD::Channel* pChannel = GetChannel(channelId);
    if (!pChannel)
        return false;

    FMOD_VECTOR pos = {0.f, 0.f, 0.f};
    FMOD_VECTOR vel = {0.f, 0.f, 0.f};
    m_lastResult    = pChannel->get3DAttributes(&pos, &vel);
    if (m_lastResult != FMOD_OK)
        return false;

    outX = pos.x;
    outY = pos.y;
    outZ = pos.z;
    return true;
}

bool CFMODManager::GetChannelVolume(uint32_t channelId, float& outVolume)
{
    FMOD::Channel* pChannel = GetChannel(channelId);
    if (!pChannel)
        return false;

    m_lastResult = pChannel->getVolume(&outVolume);
    return m_lastResult == FMOD_OK;
}

bool CFMODManager::GetChannelPitch(uint32_t channelId, float& outPitch)
{
    FMOD::Channel* pChannel = GetChannel(channelId);
    if (!pChannel)
        return false;

    m_lastResult = pChannel->getPitch(&outPitch);
    return m_lastResult == FMOD_OK;
}

// ─── Reverb ───────────────────────────────────────────────────────────────────

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
    m_lastResult  = m_pSystem->setReverbProperties(0, &m_reverbProps);

    // Re-apply stored wet level for all opted-in channels (dB → linear)
    for (auto& [id, info] : m_channels)
    {
        if (info.reverbWet < -900.0f)
            continue;
        bool isPlaying = false;
        if (info.pChannel->isPlaying(&isPlaying) == FMOD_OK && isPlaying)
            info.pChannel->setReverbProperties(0, DBToLinear(info.reverbWet));
    }
}

bool CFMODManager::SetChannelReverbWet(uint32_t channelId, float wetDB)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end())
    {
        m_lastResult = FMOD_ERR_INVALID_HANDLE;
        return false;
    }

    FMOD::Channel* pChannel = GetChannel(channelId);
    if (!pChannel)
        return false;

    it->second.reverbWet = wetDB;
    // FMOD::Channel::setReverbProperties takes a LINEAR 0-1 wet send, not dB
    m_lastResult = pChannel->setReverbProperties(0, DBToLinear(wetDB));
    return m_lastResult == FMOD_OK;
}

void CFMODManager::SetReverbWetLevel(float wetDB)
{
    if (!m_pSystem)
        return;

    m_reverbProps.WetLevel = wetDB;
    m_lastResult           = m_pSystem->setReverbProperties(0, &m_reverbProps);

    for (auto& [id, info] : m_channels)
    {
        if (info.reverbWet < -900.0f)
            continue;
        bool isPlaying = false;
        if (info.pChannel->isPlaying(&isPlaying) == FMOD_OK && isPlaying)
            info.pChannel->setReverbProperties(0, DBToLinear(info.reverbWet));
    }
}

// ─── Master volume ────────────────────────────────────────────────────────────

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

// ─── Volume categories ────────────────────────────────────────────────────────

bool CFMODManager::SetChannelCategory(uint32_t channelId, int category)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end())
    {
        m_lastResult = FMOD_ERR_INVALID_HANDLE;
        return false;
    }

    FMOD::ChannelGroup* pGroup = GetCategoryGroup(category);
    m_lastResult = it->second.pChannel->setChannelGroup(pGroup);
    if (m_lastResult == FMOD_OK)
        it->second.category = category;
    return m_lastResult == FMOD_OK;
}

bool CFMODManager::SetCategoryVolume(int category, float volume)
{
    FMOD::ChannelGroup* pGroup = GetCategoryGroup(category);
    if (!pGroup)
    {
        m_lastResult = FMOD_ERR_INVALID_PARAM;
        return false;
    }
    m_lastResult = pGroup->setVolume(volume);
    return m_lastResult == FMOD_OK;
}

float CFMODManager::GetCategoryVolume(int category) const
{
    FMOD::ChannelGroup* pGroup = GetCategoryGroup(category);
    if (!pGroup)
        return 0.0f;
    float volume = 0.0f;
    pGroup->getVolume(&volume);
    return volume;
}

// ─── Echo DSP ─────────────────────────────────────────────────────────────────

bool CFMODManager::ApplyChannelEcho(uint32_t channelId, float delayMS, float feedbackPct, float wetDB)
{
    if (!m_pSystem)
    {
        m_lastResult = FMOD_ERR_UNINITIALIZED;
        return false;
    }

    auto it = m_channels.find(channelId);
    if (it == m_channels.end())
    {
        m_lastResult = FMOD_ERR_INVALID_HANDLE;
        return false;
    }

    ChannelInfo& info = it->second;

    bool        isPlaying = false;
    FMOD_RESULT r         = info.pChannel->isPlaying(&isPlaying);
    if (r == FMOD_ERR_INVALID_HANDLE || (r == FMOD_OK && !isPlaying))
    {
        ReleaseChannelDSPs(channelId, false);
        m_channels.erase(it);
        m_lastResult = FMOD_ERR_INVALID_HANDLE;
        return false;
    }

    if (info.pEchoDSP)
    {
        info.pEchoDSP->setParameterFloat(FMOD_DSP_ECHO_DELAY,    delayMS);
        info.pEchoDSP->setParameterFloat(FMOD_DSP_ECHO_FEEDBACK,  feedbackPct);
        m_lastResult = info.pEchoDSP->setParameterFloat(FMOD_DSP_ECHO_WETLEVEL, wetDB);
    }
    else
    {
        FMOD::DSP* pDSP = nullptr;
        m_lastResult    = m_pSystem->createDSPByType(FMOD_DSP_TYPE_ECHO, &pDSP);
        if (m_lastResult != FMOD_OK)
            return false;

        pDSP->setParameterFloat(FMOD_DSP_ECHO_DELAY,    delayMS);
        pDSP->setParameterFloat(FMOD_DSP_ECHO_FEEDBACK,  feedbackPct);
        pDSP->setParameterFloat(FMOD_DSP_ECHO_DRYLEVEL,  0.0f);
        pDSP->setParameterFloat(FMOD_DSP_ECHO_WETLEVEL,  wetDB);

        m_lastResult = info.pChannel->addDSP(FMOD_CHANNELCONTROL_DSP_HEAD, pDSP);
        if (m_lastResult != FMOD_OK)
        {
            pDSP->release();
            return false;
        }

        info.pEchoDSP = pDSP;
    }

    return m_lastResult == FMOD_OK;
}

bool CFMODManager::RemoveChannelEcho(uint32_t channelId)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end())
    {
        m_lastResult = FMOD_ERR_INVALID_HANDLE;
        return false;
    }

    ChannelInfo& info = it->second;
    if (!info.pEchoDSP)
    {
        m_lastResult = FMOD_ERR_DSP_NOTFOUND;
        return false;
    }

    bool        isPlaying = false;
    FMOD_RESULT r         = info.pChannel->isPlaying(&isPlaying);
    if (r != FMOD_ERR_INVALID_HANDLE)
        info.pChannel->removeDSP(info.pEchoDSP);

    info.pEchoDSP->release();
    info.pEchoDSP = nullptr;
    m_lastResult  = FMOD_OK;
    return true;
}

// ─── Low-pass filter ─────────────────────────────────────────────────────────

bool CFMODManager::ApplyChannelLowPass(uint32_t channelId, float cutoffHz, float resonance)
{
    if (!m_pSystem) { m_lastResult = FMOD_ERR_UNINITIALIZED; return false; }

    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) { m_lastResult = FMOD_ERR_INVALID_HANDLE; return false; }

    ChannelInfo& info = it->second;
    bool isPlaying = false;
    FMOD_RESULT r = info.pChannel->isPlaying(&isPlaying);
    if (r == FMOD_ERR_INVALID_HANDLE || (r == FMOD_OK && !isPlaying))
    {
        ReleaseChannelDSPs(channelId, false);
        m_channels.erase(it);
        m_lastResult = FMOD_ERR_INVALID_HANDLE;
        return false;
    }

    if (info.pLowPassDSP)
    {
        info.pLowPassDSP->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF, cutoffHz);
        m_lastResult = info.pLowPassDSP->setParameterFloat(FMOD_DSP_LOWPASS_RESONANCE, resonance);
    }
    else
    {
        FMOD::DSP* pDSP = nullptr;
        m_lastResult = m_pSystem->createDSPByType(FMOD_DSP_TYPE_LOWPASS, &pDSP);
        if (m_lastResult != FMOD_OK) return false;

        pDSP->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF,    cutoffHz);
        pDSP->setParameterFloat(FMOD_DSP_LOWPASS_RESONANCE, resonance);

        m_lastResult = info.pChannel->addDSP(FMOD_CHANNELCONTROL_DSP_HEAD, pDSP);
        if (m_lastResult != FMOD_OK) { pDSP->release(); return false; }

        info.pLowPassDSP = pDSP;
    }
    return m_lastResult == FMOD_OK;
}

bool CFMODManager::RemoveChannelLowPass(uint32_t channelId)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) { m_lastResult = FMOD_ERR_INVALID_HANDLE; return false; }

    ChannelInfo& info = it->second;
    if (!info.pLowPassDSP) { m_lastResult = FMOD_ERR_DSP_NOTFOUND; return false; }

    bool isPlaying = false;
    if (info.pChannel->isPlaying(&isPlaying) != FMOD_ERR_INVALID_HANDLE)
        info.pChannel->removeDSP(info.pLowPassDSP);

    info.pLowPassDSP->release();
    info.pLowPassDSP = nullptr;
    m_lastResult = FMOD_OK;
    return true;
}

// ─── High-pass filter ────────────────────────────────────────────────────────

bool CFMODManager::ApplyChannelHighPass(uint32_t channelId, float cutoffHz, float resonance)
{
    if (!m_pSystem) { m_lastResult = FMOD_ERR_UNINITIALIZED; return false; }

    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) { m_lastResult = FMOD_ERR_INVALID_HANDLE; return false; }

    ChannelInfo& info = it->second;
    bool isPlaying = false;
    FMOD_RESULT r = info.pChannel->isPlaying(&isPlaying);
    if (r == FMOD_ERR_INVALID_HANDLE || (r == FMOD_OK && !isPlaying))
    {
        ReleaseChannelDSPs(channelId, false);
        m_channels.erase(it);
        m_lastResult = FMOD_ERR_INVALID_HANDLE;
        return false;
    }

    if (info.pHighPassDSP)
    {
        info.pHighPassDSP->setParameterFloat(FMOD_DSP_HIGHPASS_CUTOFF, cutoffHz);
        m_lastResult = info.pHighPassDSP->setParameterFloat(FMOD_DSP_HIGHPASS_RESONANCE, resonance);
    }
    else
    {
        FMOD::DSP* pDSP = nullptr;
        m_lastResult = m_pSystem->createDSPByType(FMOD_DSP_TYPE_HIGHPASS, &pDSP);
        if (m_lastResult != FMOD_OK) return false;

        pDSP->setParameterFloat(FMOD_DSP_HIGHPASS_CUTOFF,    cutoffHz);
        pDSP->setParameterFloat(FMOD_DSP_HIGHPASS_RESONANCE, resonance);

        m_lastResult = info.pChannel->addDSP(FMOD_CHANNELCONTROL_DSP_HEAD, pDSP);
        if (m_lastResult != FMOD_OK) { pDSP->release(); return false; }

        info.pHighPassDSP = pDSP;
    }
    return m_lastResult == FMOD_OK;
}

bool CFMODManager::RemoveChannelHighPass(uint32_t channelId)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) { m_lastResult = FMOD_ERR_INVALID_HANDLE; return false; }

    ChannelInfo& info = it->second;
    if (!info.pHighPassDSP) { m_lastResult = FMOD_ERR_DSP_NOTFOUND; return false; }

    bool isPlaying = false;
    if (info.pChannel->isPlaying(&isPlaying) != FMOD_ERR_INVALID_HANDLE)
        info.pChannel->removeDSP(info.pHighPassDSP);

    info.pHighPassDSP->release();
    info.pHighPassDSP = nullptr;
    m_lastResult = FMOD_OK;
    return true;
}

// ─── Flanger DSP ─────────────────────────────────────────────────────────────

bool CFMODManager::ApplyChannelFlanger(uint32_t channelId, float mix, float depth, float rate)
{
    if (!m_pSystem) { m_lastResult = FMOD_ERR_UNINITIALIZED; return false; }

    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) { m_lastResult = FMOD_ERR_INVALID_HANDLE; return false; }

    ChannelInfo& info = it->second;
    bool isPlaying = false;
    FMOD_RESULT r = info.pChannel->isPlaying(&isPlaying);
    if (r == FMOD_ERR_INVALID_HANDLE || (r == FMOD_OK && !isPlaying))
    {
        ReleaseChannelDSPs(channelId, false);
        m_channels.erase(it);
        m_lastResult = FMOD_ERR_INVALID_HANDLE;
        return false;
    }

    if (info.pFlangerDSP)
    {
        info.pFlangerDSP->setParameterFloat(FMOD_DSP_FLANGE_MIX,   mix);
        info.pFlangerDSP->setParameterFloat(FMOD_DSP_FLANGE_DEPTH, depth);
        m_lastResult = info.pFlangerDSP->setParameterFloat(FMOD_DSP_FLANGE_RATE, rate);
    }
    else
    {
        FMOD::DSP* pDSP = nullptr;
        m_lastResult = m_pSystem->createDSPByType(FMOD_DSP_TYPE_FLANGE, &pDSP);
        if (m_lastResult != FMOD_OK) return false;

        pDSP->setParameterFloat(FMOD_DSP_FLANGE_MIX,   mix);
        pDSP->setParameterFloat(FMOD_DSP_FLANGE_DEPTH, depth);
        pDSP->setParameterFloat(FMOD_DSP_FLANGE_RATE,  rate);

        m_lastResult = info.pChannel->addDSP(FMOD_CHANNELCONTROL_DSP_HEAD, pDSP);
        if (m_lastResult != FMOD_OK) { pDSP->release(); return false; }

        info.pFlangerDSP = pDSP;
    }
    return m_lastResult == FMOD_OK;
}

bool CFMODManager::RemoveChannelFlanger(uint32_t channelId)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) { m_lastResult = FMOD_ERR_INVALID_HANDLE; return false; }

    ChannelInfo& info = it->second;
    if (!info.pFlangerDSP) { m_lastResult = FMOD_ERR_DSP_NOTFOUND; return false; }

    bool isPlaying = false;
    if (info.pChannel->isPlaying(&isPlaying) != FMOD_ERR_INVALID_HANDLE)
        info.pChannel->removeDSP(info.pFlangerDSP);

    info.pFlangerDSP->release();
    info.pFlangerDSP = nullptr;
    m_lastResult = FMOD_OK;
    return true;
}

// ─── Chorus DSP ──────────────────────────────────────────────────────────────

bool CFMODManager::ApplyChannelChorus(uint32_t channelId, float mix, float depth, float rate)
{
    if (!m_pSystem) { m_lastResult = FMOD_ERR_UNINITIALIZED; return false; }

    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) { m_lastResult = FMOD_ERR_INVALID_HANDLE; return false; }

    ChannelInfo& info = it->second;
    bool isPlaying = false;
    FMOD_RESULT r = info.pChannel->isPlaying(&isPlaying);
    if (r == FMOD_ERR_INVALID_HANDLE || (r == FMOD_OK && !isPlaying))
    {
        ReleaseChannelDSPs(channelId, false);
        m_channels.erase(it);
        m_lastResult = FMOD_ERR_INVALID_HANDLE;
        return false;
    }

    if (info.pChorusDSP)
    {
        info.pChorusDSP->setParameterFloat(FMOD_DSP_CHORUS_MIX,   mix);
        info.pChorusDSP->setParameterFloat(FMOD_DSP_CHORUS_DEPTH, depth);
        m_lastResult = info.pChorusDSP->setParameterFloat(FMOD_DSP_CHORUS_RATE, rate);
    }
    else
    {
        FMOD::DSP* pDSP = nullptr;
        m_lastResult = m_pSystem->createDSPByType(FMOD_DSP_TYPE_CHORUS, &pDSP);
        if (m_lastResult != FMOD_OK) return false;

        pDSP->setParameterFloat(FMOD_DSP_CHORUS_MIX,   mix);
        pDSP->setParameterFloat(FMOD_DSP_CHORUS_DEPTH, depth);
        pDSP->setParameterFloat(FMOD_DSP_CHORUS_RATE,  rate);

        m_lastResult = info.pChannel->addDSP(FMOD_CHANNELCONTROL_DSP_HEAD, pDSP);
        if (m_lastResult != FMOD_OK) { pDSP->release(); return false; }

        info.pChorusDSP = pDSP;
    }
    return m_lastResult == FMOD_OK;
}

bool CFMODManager::RemoveChannelChorus(uint32_t channelId)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) { m_lastResult = FMOD_ERR_INVALID_HANDLE; return false; }

    ChannelInfo& info = it->second;
    if (!info.pChorusDSP) { m_lastResult = FMOD_ERR_DSP_NOTFOUND; return false; }

    bool isPlaying = false;
    if (info.pChannel->isPlaying(&isPlaying) != FMOD_ERR_INVALID_HANDLE)
        info.pChannel->removeDSP(info.pChorusDSP);

    info.pChorusDSP->release();
    info.pChorusDSP = nullptr;
    m_lastResult = FMOD_OK;
    return true;
}

// ─── Distortion DSP ──────────────────────────────────────────────────────────

bool CFMODManager::ApplyChannelDistortion(uint32_t channelId, float level)
{
    if (!m_pSystem) { m_lastResult = FMOD_ERR_UNINITIALIZED; return false; }

    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) { m_lastResult = FMOD_ERR_INVALID_HANDLE; return false; }

    ChannelInfo& info = it->second;
    bool isPlaying = false;
    FMOD_RESULT r = info.pChannel->isPlaying(&isPlaying);
    if (r == FMOD_ERR_INVALID_HANDLE || (r == FMOD_OK && !isPlaying))
    {
        ReleaseChannelDSPs(channelId, false);
        m_channels.erase(it);
        m_lastResult = FMOD_ERR_INVALID_HANDLE;
        return false;
    }

    if (info.pDistortionDSP)
    {
        m_lastResult = info.pDistortionDSP->setParameterFloat(FMOD_DSP_DISTORTION_LEVEL, level);
    }
    else
    {
        FMOD::DSP* pDSP = nullptr;
        m_lastResult = m_pSystem->createDSPByType(FMOD_DSP_TYPE_DISTORTION, &pDSP);
        if (m_lastResult != FMOD_OK) return false;

        pDSP->setParameterFloat(FMOD_DSP_DISTORTION_LEVEL, level);

        m_lastResult = info.pChannel->addDSP(FMOD_CHANNELCONTROL_DSP_HEAD, pDSP);
        if (m_lastResult != FMOD_OK) { pDSP->release(); return false; }

        info.pDistortionDSP = pDSP;
    }
    return m_lastResult == FMOD_OK;
}

bool CFMODManager::RemoveChannelDistortion(uint32_t channelId)
{
    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) { m_lastResult = FMOD_ERR_INVALID_HANDLE; return false; }

    ChannelInfo& info = it->second;
    if (!info.pDistortionDSP) { m_lastResult = FMOD_ERR_DSP_NOTFOUND; return false; }

    bool isPlaying = false;
    if (info.pChannel->isPlaying(&isPlaying) != FMOD_ERR_INVALID_HANDLE)
        info.pChannel->removeDSP(info.pDistortionDSP);

    info.pDistortionDSP->release();
    info.pDistortionDSP = nullptr;
    m_lastResult = FMOD_OK;
    return true;
}

// ─── Named parameters ─────────────────────────────────────────────────────────

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

// ─── DSP chain query ──────────────────────────────────────────────────────────

bool CFMODManager::GetChannelEffects(uint32_t channelId, SString& outEffects) const
{
    outEffects.clear();
    auto it = m_channels.find(channelId);
    if (it == m_channels.end())
        return false;

    const ChannelInfo& info = it->second;
    auto append = [&](const char* name)
    {
        if (!outEffects.empty()) outEffects += ',';
        outEffects += name;
    };
    if (info.pEchoDSP)       append("echo");
    if (info.pLowPassDSP)    append("lowpass");
    if (info.pHighPassDSP)   append("highpass");
    if (info.pFlangerDSP)    append("flanger");
    if (info.pChorusDSP)     append("chorus");
    if (info.pDistortionDSP) append("distortion");
    return true;
}

// ─── Occlusion / obstruction ─────────────────────────────────────────────────

bool CFMODManager::SetChannelOcclusion(uint32_t channelId, float directOcclusion, float reverbOcclusion)
{
    if (!m_pSystem) { m_lastResult = FMOD_ERR_UNINITIALIZED; return false; }

    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) { m_lastResult = FMOD_ERR_INVALID_HANDLE; return false; }

    ChannelInfo& info = it->second;

    bool isPlaying = false;
    FMOD_RESULT r = info.pChannel->isPlaying(&isPlaying);
    if (r == FMOD_ERR_INVALID_HANDLE || (r == FMOD_OK && !isPlaying))
    {
        ReleaseChannelDSPs(channelId, false);
        m_channels.erase(it);
        m_lastResult = FMOD_ERR_INVALID_HANDLE;
        return false;
    }

    directOcclusion = directOcclusion < 0.0f ? 0.0f : (directOcclusion > 1.0f ? 1.0f : directOcclusion);
    reverbOcclusion = reverbOcclusion < 0.0f ? 0.0f : (reverbOcclusion > 1.0f ? 1.0f : reverbOcclusion);

    m_lastResult = info.pChannel->set3DOcclusion(directOcclusion, reverbOcclusion);
    return m_lastResult == FMOD_OK;
}
