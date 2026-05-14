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
    // Volume category indices passed to SetCategoryVolume / SetChannelCategory
    static constexpr int CAT_SFX     = 0;
    static constexpr int CAT_AMBIENT = 1;
    static constexpr int CAT_MUSIC   = 2;
    static constexpr int CAT_COUNT   = 3;

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
    // SetChannelReverbWet accepts dB (-80 = dry, 0 = 0 dB); converts to linear internally.
    void SetReverbPreset(const char* presetName);
    void SetReverbWetLevel(float wetDB);
    bool SetChannelReverbWet(uint32_t channelId, float wetDB);

    // Master volume for all FMOD sounds (0.0 – 1.0, default 0.5)
    void  SetMasterVolume(float volume);
    float GetMasterVolume() const;

    // Volume categories — sub-groups of the master for independent mixing
    bool  SetChannelCategory(uint32_t channelId, int category);
    bool  SetCategoryVolume(int category, float volume);
    float GetCategoryVolume(int category) const;

    // Echo DSP — per-channel bouncing-echo effect (FMOD_DSP_TYPE_ECHO)
    bool ApplyChannelEcho(uint32_t channelId, float delayMS, float feedbackPct, float wetDB);
    bool RemoveChannelEcho(uint32_t channelId);

    // Low-pass / high-pass filters
    bool ApplyChannelLowPass(uint32_t channelId, float cutoffHz, float resonance = 1.0f);
    bool RemoveChannelLowPass(uint32_t channelId);
    bool ApplyChannelHighPass(uint32_t channelId, float cutoffHz, float resonance = 1.0f);
    bool RemoveChannelHighPass(uint32_t channelId);

    // Flanger (mix 0–100 %, depth 0.01–1.0, rate 0–20 Hz)
    bool ApplyChannelFlanger(uint32_t channelId, float mix, float depth, float rate);
    bool RemoveChannelFlanger(uint32_t channelId);

    // Chorus (mix 0–100 %, depth 0–100 %, rate 0–20 Hz)
    bool ApplyChannelChorus(uint32_t channelId, float mix, float depth, float rate);
    bool RemoveChannelChorus(uint32_t channelId);

    // Distortion (level 0.0–1.0)
    bool ApplyChannelDistortion(uint32_t channelId, float level);
    bool RemoveChannelDistortion(uint32_t channelId);

    // Named float parameters (used by Lua ambience system)
    void  SetParameter(const char* name, float value);
    float GetParameter(const char* name, float defaultValue = 0.0f) const;

    // Version string, e.g. "2.02.21"
    SString GetVersion() const;

private:
    uint32_t       CreateSoundInternal(const char* path, bool b3D, bool bLoop, bool bStream);
    FMOD::Channel* GetChannel(uint32_t channelId);
    FMOD::ChannelGroup* GetCategoryGroup(int category) const;

    // Releases all DSPs on a channel. Pass removeFirst=true in StopChannel (channel still live);
    // pass false in Update purge (channel already dead, removeDSP would fail).
    void ReleaseChannelDSPs(uint32_t channelId, bool removeFirst);

    // dB to linear gain (clamped to [0, 1]). -80 dB and below → 0.
    static float DBToLinear(float dB) noexcept;

    FMOD::System*       m_pSystem;
    FMOD::ChannelGroup* m_pMasterGroup;
    FMOD::ChannelGroup* m_pCategoryGroups[CAT_COUNT]{};

    FMOD_RESULT m_lastResult{FMOD_OK};

    struct SoundInfo
    {
        FMOD::Sound* pSound;
        bool         b3D;
    };

    struct ChannelInfo
    {
        FMOD::Channel* pChannel;
        // DSP slots — nullptr means not attached
        FMOD::DSP*     pEchoDSP        = nullptr;
        FMOD::DSP*     pLowPassDSP     = nullptr;
        FMOD::DSP*     pHighPassDSP    = nullptr;
        FMOD::DSP*     pFlangerDSP     = nullptr;
        FMOD::DSP*     pChorusDSP      = nullptr;
        FMOD::DSP*     pDistortionDSP  = nullptr;
        bool           b3D;
        int            category        = CAT_SFX;
        float          reverbWet       = -999.0f; // sentinel: not opted in
        float          velX = 0.f, velY = 0.f, velZ = 0.f;
    };

    std::unordered_map<uint32_t, SoundInfo>   m_sounds;
    std::unordered_map<uint32_t, ChannelInfo> m_channels;
    std::unordered_map<std::string, float>    m_parameters;

    FMOD_REVERB_PROPERTIES m_reverbProps;

    uint32_t m_nextSoundId{1};
    uint32_t m_nextChannelId{1};
};
