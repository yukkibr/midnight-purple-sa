/*****************************************************************************
 *
 *  PROJECT:     Midnight Purple:SA
 *  FILE:        core/CFMODManager.h
 *  PURPOSE:     FMOD audio engine manager
 *
 *****************************************************************************/

#pragma once

#include <fmod.hpp>
#include <CVector.h>
#include <unordered_map>
#include <string>
#include <cstdint>

class CFMODManager
{
public:
    CFMODManager();
    ~CFMODManager();

    bool          IsInitialized() const noexcept { return m_pSystem != nullptr; }
    FMOD::System* GetSystem() const noexcept { return m_pSystem; }

    // Error inspection — valid after any method that can fail
    FMOD_RESULT GetLastResult() const noexcept { return m_lastResult; }
    const char* GetLastErrorString() const noexcept;

    // Called every frame from CCore::DoPostFramePulse
    void Update(const CVector& vecPosition, const CVector& vecForward, const CVector& vecUp);

    // Sound lifetime — CreateSound loads fully into RAM; CreateStream decodes on the fly (long tracks)
    uint32_t CreateSound(const char* path, bool b3D, bool bLoop = false);
    uint32_t CreateStream(const char* path, bool b3D, bool bLoop = false);
    uint32_t CreateSoundFromMemory(const void* pData, size_t dataSize, bool b3D, bool bLoop = false);
    void     FreeSound(uint32_t soundId);

    // Playback — returns channel handle (0 on failure)
    uint32_t PlaySound(uint32_t soundId, float x, float y, float z, float minDist, float maxDist);

    // Channel control
    bool StopChannel(uint32_t channelId);
    bool PauseChannel(uint32_t channelId, bool bPause);
    bool IsChannelPaused(uint32_t channelId);
    bool SetChannelVolume(uint32_t channelId, float volume);
    bool SetChannelPitch(uint32_t channelId, float pitch);
    bool SetChannelPosition(uint32_t channelId, float x, float y, float z);
    bool SetChannelVelocity(uint32_t channelId, float vx, float vy, float vz);
    bool SetChannelLooped(uint32_t channelId, bool bLoop);
    bool IsChannelPlaying(uint32_t channelId);

    // Channel read-back
    bool GetChannelLooped(uint32_t channelId, bool& outLooped);
    bool GetChannelPosition3D(uint32_t channelId, float& outX, float& outY, float& outZ);
    bool GetChannelVolume(uint32_t channelId, float& outVolume);
    bool GetChannelPitch(uint32_t channelId, float& outPitch);

    // Reverb — slot 0 is the global environment reverb
    void SetReverbPreset(const char* presetName);
    void SetReverbWetLevel(float wetDB);
    bool SetChannelReverbWet(uint32_t channelId, float wetDB);

    // Master volume for all FMOD sounds (0.0 – 1.0, default 0.5)
    void  SetMasterVolume(float volume);
    float GetMasterVolume() const;

    // Echo DSP — per-channel bouncing-echo effect (FMOD_DSP_TYPE_ECHO)
    bool ApplyChannelEcho(uint32_t channelId, float delayMS, float feedbackPct, float wetDB);
    bool RemoveChannelEcho(uint32_t channelId);

    // Named float parameters (used by Lua ambience system)
    void  SetParameter(const char* name, float value);
    float GetParameter(const char* name, float defaultValue = 0.0f) const;

    // Version string, e.g. "2.02.21"
    SString GetVersion() const;

private:
    uint32_t       CreateSoundInternal(const char* path, bool b3D, bool bLoop, bool bStream);
    FMOD::Channel* GetChannel(uint32_t channelId);

    FMOD::System*       m_pSystem;
    FMOD::ChannelGroup* m_pMasterGroup;

    FMOD_RESULT m_lastResult{FMOD_OK};

    struct SoundInfo
    {
        FMOD::Sound* pSound;
        bool         b3D;
    };

    struct ChannelInfo
    {
        FMOD::Channel* pChannel;
        FMOD::DSP*     pEchoDSP  = nullptr;
        bool           b3D;
        float          reverbWet;
        // Stored so SetChannelVelocity can re-apply position in the same call
        float velX = 0.f, velY = 0.f, velZ = 0.f;
    };

    std::unordered_map<uint32_t, SoundInfo>   m_sounds;
    std::unordered_map<uint32_t, ChannelInfo> m_channels;
    std::unordered_map<std::string, float>    m_parameters;

    FMOD_REVERB_PROPERTIES m_reverbProps;

    uint32_t m_nextSoundId{1};
    uint32_t m_nextChannelId{1};
};
