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

    // Called every frame from CCore::DoPostFramePulse
    void Update(const CVector& vecPosition, const CVector& vecForward, const CVector& vecUp);

    // Sound lifetime
    uint32_t CreateSound(const char* path, bool b3D, bool bLoop = false);
    void     FreeSound(uint32_t soundId);

    // Playback — returns channel handle (0 on failure)
    uint32_t PlaySound(uint32_t soundId, float x, float y, float z, float minDist, float maxDist);

    // Channel control
    bool StopChannel(uint32_t channelId);
    bool SetChannelVolume(uint32_t channelId, float volume);
    bool SetChannelPitch(uint32_t channelId, float pitch);
    bool SetChannelPosition(uint32_t channelId, float x, float y, float z);
    bool IsChannelPlaying(uint32_t channelId);

    // Reverb — slot 0 is the global environment reverb
    // Channels do NOT receive reverb automatically; call SetChannelReverbWet to opt in.
    void SetReverbPreset(const char* presetName);
    void SetReverbWetLevel(float wetDB);                       // override WetLevel of current preset (dB, -80..20)
    bool SetChannelReverbWet(uint32_t channelId, float wetDB); // connect channel to reverb slot 0 (dB: 0=unity, -80=off)

    // Master volume for all FMOD sounds (0.0 – 1.0, default 0.5)
    void  SetMasterVolume(float volume);
    float GetMasterVolume() const;

    // Echo DSP — per-channel bouncing-echo effect (FMOD_DSP_TYPE_ECHO)
    bool ApplyChannelEcho(uint32_t channelId, float delayMS, float feedbackPct, float wetDB);
    bool RemoveChannelEcho(uint32_t channelId);

    // Named float parameters (used by Lua ambience system)
    void  SetParameter(const char* name, float value);
    float GetParameter(const char* name, float defaultValue = 0.0f) const;

private:
    FMOD::Channel* GetChannel(uint32_t channelId);

    FMOD::System*       m_pSystem;
    FMOD::ChannelGroup* m_pMasterGroup;   // all FMOD sounds route through here

    struct SoundInfo
    {
        FMOD::Sound* pSound;
        bool         b3D;
    };

    struct ChannelInfo
    {
        FMOD::Channel* pChannel;
        FMOD::DSP*     pEchoDSP  = nullptr;   // owned; nullptr = no echo active
        bool           b3D;
        float          reverbWet;   // dB stored so we can re-apply on preset change; -999 = not opted in
    };

    std::unordered_map<uint32_t, SoundInfo>   m_sounds;
    std::unordered_map<uint32_t, ChannelInfo> m_channels;
    std::unordered_map<std::string, float>    m_parameters;

    FMOD_REVERB_PROPERTIES m_reverbProps;   // current slot-0 properties (stored for WetLevel edits)

    uint32_t m_nextSoundId{1};
    uint32_t m_nextChannelId{1};
};
