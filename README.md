# Midnight Purple: San Andreas

> **This is an independent fork of [Multi Theft Auto: San Andreas](https://github.com/multitheftauto/mtasa-blue).**
> Midnight Purple:SA is **not** affiliated with, endorsed by, or supported by the Multi Theft Auto development team. Issues, pull requests, and questions about this fork should be directed here — **do not** contact the MTA team about Midnight Purple-specific changes.

---

Midnight Purple:SA is a custom MTA:SA client that extends the original multiplayer platform with a second audio engine (FMOD) running alongside the existing BASS stack. The goal is to enable advanced 3D audio, environmental effects, and engine sound simulation in Lua scripts — without breaking any existing script that already uses MTA's native audio functions.

**BASS remains untouched.** All existing `playSound`, `playSound3D`, `setSoundVolume`, etc. functions continue to work exactly as before. FMOD is an additive, parallel system exposed through its own set of `fmod*` Lua globals.

---

## What's New in This Fork

### FMOD Audio Engine Integration

A new `CFMODManager` (`Client/core/`) wraps the FMOD Core API and is owned by `CCore`. It is updated every frame from `DoPostFramePulse()`, keeping the 3D listener position and orientation in sync with the game camera.

**Engine internals:**
- FMOD initialised in right-handed coordinate mode to match GTA:SA's world axes (Z up, Y north).
- All FMOD sounds route through a master `ChannelGroup`. Three sub-groups sit under it — **SFX** (default), **Ambient**, and **Music** — each with an independent volume scalar.
- Automatic channel cleanup every frame — finished channels are purged from the internal map without manual book-keeping from scripts.
- Sounds start paused, 3D attributes are applied, then unpaused — preventing a single-frame positional pop on spawn.
- **Per-resource tracking** — every sound ID is registered against the `CLuaMain` that created it. When a resource stops, all its FMOD sounds are automatically freed, matching how MTA handles BASS sounds.

### New Lua API (`fmod*` globals)

All functions are available client-side. Sound and channel IDs are plain integers managed by `CFMODManager`.

**Sound lifetime**

| Function | Returns | Description |
|----------|---------|-------------|
| `fmodCreateSound(path, [is3D=false], [loop=false])` | soundId / `false` | Load a sound asset from a resource path. |
| `fmodCreateStream(path, [is3D=false], [loop=false])` | soundId / `false` | Stream a long audio file from disk (music-quality, lower RAM). |
| `fmodCreateSoundFromMemory(data, [is3D=false], [loop=false])` | soundId / `false` | Load from a raw audio data string (e.g. TEA-decrypted `.wavlocked` contents). FMOD copies the buffer internally. |
| `fmodFreeSound(soundId)` | bool | Release the sound and free its memory. |

**Playback**

| Function | Returns | Description |
|----------|---------|-------------|
| `fmodPlaySound(soundId, [x, y, z, [minDist=1, maxDist=100]])` | channelId / `false` | Spawn a playing instance of a sound. Omit position for a 2D (non-spatialized) channel. |
| `fmodStopSound(channelId)` | bool | Stop and remove the channel. |
| `fmodPauseSound(channelId)` | bool | Pause a channel without removing it. |
| `fmodResumeSound(channelId)` | bool | Resume a paused channel. |
| `fmodIsSoundPaused(channelId)` | bool | Returns `true` if the channel is paused. |
| `fmodIsSoundPlaying(channelId)` | bool | Returns `true` if the channel is still active and not paused. |

**Per-channel control**

| Function | Returns | Description |
|----------|---------|-------------|
| `fmodSetSoundVolume(channelId, volume)` | bool | Volume 0.0 – 1.0. |
| `fmodGetSoundVolume(channelId)` | number | Read the current volume. |
| `fmodSetSoundPitch(channelId, pitch)` | bool | 1.0 = normal, 2.0 = one octave up. |
| `fmodGetSoundPitch(channelId)` | number | Read the current pitch multiplier. |
| `fmodSetSoundPosition(channelId, x, y, z)` | bool | Update world position for a 3D channel. |
| `fmodGetSoundPosition(channelId)` | x, y, z | Read the current world position. Returns `false` on a 2D channel. |
| `fmodSetSoundVelocity(channelId, vx, vy, vz)` | bool | Set world-space velocity for Doppler pitch shift. |
| `fmodSetSoundLooped(channelId, loop)` | bool | Change loop mode on a live channel without restarting it. |
| `fmodGetSoundLooped(channelId)` | bool | Read the current loop state. |

**Environmental reverb** (global slot 0; opt-in per channel)

| Function | Returns | Description |
|----------|---------|-------------|
| `fmodSetReverbPreset(preset)` | bool | Set the global reverb character. Presets: `"off"`, `"generic"`, `"room"`, `"bathroom"`, `"cave"`, `"city"`, `"alley"`, `"forest"`, `"mountains"`, `"plain"`, `"sewerpipe"`, `"underwater"`, `"stonecorridor"`, `"hallway"`, `"stoneroom"`, `"auditorium"`, `"concerthall"`, `"arena"`, `"hangar"`, `"parkinglot"`, `"paddedcell"`, `"livingroom"`, `"quarry"`. |
| `fmodSetReverbWetLevel(wetDB)` | bool | Override the output level (−80 to +20 dB) without changing the preset character. |
| `fmodSetChannelReverb(channelId, wetDB)` | bool | Connect a channel to the reverb bus. Channels are **dry by default**; call this to opt in. Negative dB values are linear-converted internally. |

**DSP effects** (per-channel; each type occupies one slot; calling Set again updates parameters in-place without recreating the DSP)

| Function | Returns | Description |
|----------|---------|-------------|
| `fmodSetChannelEcho(channelId, delayMS, feedbackPct, wetDB)` | bool | Attach or update an echo/delay effect. |
| `fmodRemoveChannelEcho(channelId)` | bool | Remove the echo DSP. |
| `fmodSetChannelLowPass(channelId, cutoffHz, [resonance=1.0])` | bool | Attenuate frequencies above `cutoffHz`. Useful for distance muffle or interior filtering. |
| `fmodRemoveChannelLowPass(channelId)` | bool | Remove the low-pass filter. |
| `fmodSetChannelHighPass(channelId, cutoffHz, [resonance=1.0])` | bool | Attenuate frequencies below `cutoffHz`. |
| `fmodRemoveChannelHighPass(channelId)` | bool | Remove the high-pass filter. |
| `fmodSetChannelFlanger(channelId, mix, depth, rate)` | bool | Cyclic phase-modulation effect. `mix` 0–100 %, `depth` 0.01–1.0, `rate` 0–20 Hz. |
| `fmodRemoveChannelFlanger(channelId)` | bool | Remove the flanger DSP. |
| `fmodSetChannelChorus(channelId, mix, depth, rate)` | bool | Pitch-varied doubling effect. `mix` 0–100 %, `depth` 0–100 %, `rate` 0–20 Hz. |
| `fmodRemoveChannelChorus(channelId)` | bool | Remove the chorus DSP. |
| `fmodSetChannelDistortion(channelId, level)` | bool | Hard-clipping saturation. `level` 0.0 (clean) – 1.0 (full clip). |
| `fmodRemoveChannelDistortion(channelId)` | bool | Remove the distortion DSP. |

**Volume categories**

Channels belong to one of three sub-groups under the master group. Category volume and master volume multiply together.

| Function | Returns | Description |
|----------|---------|-------------|
| `fmodSetChannelCategory(channelId, category)` | bool | Move a live channel to a category. `category` accepts `"sfx"`, `"ambient"`, `"music"` or integers `0`, `1`, `2`. Default is `"sfx"`. |
| `fmodSetCategoryVolume(category, volume)` | bool | Scale all channels in a category (0.0 – 1.0). Accepts string or integer. |
| `fmodGetCategoryVolume(category)` | number | Read the current category volume scalar. |

**Master volume**

| Function | Returns | Description |
|----------|---------|-------------|
| `fmodSetMasterVolume(volume)` | bool | Scale all FMOD sounds together (0.0 – 1.0, default 0.5). |
| `fmodGetMasterVolume()` | number | Read the current FMOD master volume. |

**Diagnostics**

| Function | Returns | Description |
|----------|---------|-------------|
| `fmodGetVersion()` | string | FMOD Core library version (e.g. `"2.02.21"`). |
| `fmodGetLastError()` | errorCode, errorString | Read the last FMOD result code and its human-readable description. |

**Named float parameters** (ambience system)

| Function | Returns | Description |
|----------|---------|-------------|
| `fmodSetParameter(name, value)` | bool | Store a named float for use by the ambience scripting layer. |
| `fmodGetParameter(name [, default=0])` | number | Read back a stored parameter. |

---

## Roadmap

### Core audio features
- [x] **Doppler effect** — `fmodSetSoundVelocity(ch, vx, vy, vz)` feeds velocity to `set3DAttributes`; FMOD computes pitch shift automatically
- [x] **Pause / resume** — `fmodPauseSound` / `fmodResumeSound` / `fmodIsSoundPaused`
- [x] **Streaming audio** — `fmodCreateStream` uses `FMOD_CREATESTREAM`; identical API to `fmodCreateSound`
- [x] **Loop state toggle** — `fmodSetSoundLooped` / `fmodGetSoundLooped` — change loop mode on a live channel without restarting
- [x] **Read-back functions** — `fmodGetSoundVolume`, `fmodGetSoundPitch`, `fmodGetSoundPosition`, `fmodGetSoundLooped`

### DSP / effects
- [x] **Additional DSP types** — low-pass, high-pass, flanger, chorus, distortion, each exposed as `fmodSet/RemoveChannel*` pairs
- [ ] **DSP chain query** — `fmodGetChannelEffects()` to list active DSPs on a channel

### Architecture & scripting
- [x] **Per-resource sound tracking** — sounds are automatically freed when the resource that created them stops, matching MTA's BASS behaviour
- [x] **Volume categories** — independent volume scalars for SFX, Ambient, and Music sub-groups via `fmodSetCategoryVolume` / `fmodGetCategoryVolume`
- [ ] **Server-triggered playback** — server Lua instructs clients to play a positioned FMOD sound, syncing audio events across players
- [ ] **Occlusion / obstruction** — geometry-aware audio muffling when solid objects are between a sound source and the listener (FMOD Geometry API)

### Vehicle engine sounds (primary motivator)
- [x] **Engine sound layer** — `hikari_Engine` resource fully migrated to FMOD: RPM-driven pitch/volume per channel, environmental reverb opt-in, smooth echo fade, Doppler
- [x] **Per-vehicle sound bank** — `engine_table.lua` assigns sound packs per engine type; `fmodCreateSoundFromMemory` loads TEA-decrypted `.wavlocked` assets
- [x] **Rev limiter / gear shift events** — one-shot BOV and backfire/ALS sounds fire via FMOD with Doppler velocity
- [x] **Exhaust 3D positioning** — engine sound sources bind to the vehicle's exhaust bone (`exhaust_b` → `exhaust_a` → `exhaust` → vehicle origin fallback)
- [x] **Tunnel echo & reverb** — independent raycast-based cover detection with hysteresis; looping channels fade reverb send proportional to `echoIntensity`; one-shot transients (backfire, BOV) receive both Echo DSP and reverb send for consistency
- [x] **Air-absorption LPF** — non-local vehicles beyond 20 m receive a distance-proportional low-pass filter (20 kHz at 20 m → ~3.6 kHz at the audibility limit) simulating high-frequency atmospheric loss
- [x] **Rev limiter distortion** — the high-RPM layer receives a brief `fmodSetChannelDistortion` while bouncing off the limiter, adding a metallic crunch character

### Quality of life
- [x] **FMOD error propagation** — all FMOD-level failures return `false, errorCode, errorString`; `fmodGetLastError()` available at any time
- [x] **`fmodGetVersion()`** — returns the FMOD Core library version string
- [x] **Version label** — in-game client label reads *Midnight Purple 1.7* instead of the upstream *MTA:SA 1.7-custom*

---

## Original MTA:SA — Build Instructions

The sections below are preserved from the upstream project so that contributors can build the full client and server from source.

### Windows

**Prerequisites**
- [Visual Studio 2022](https://visualstudio.microsoft.com/vs/) with:
  - Desktop development with C++
  - Optional component *C++ MFC for latest v145 build tools (x86 & x64)*
- [Microsoft DirectX SDK June 2010](https://wiki.multitheftauto.com/wiki/Compiling_MTASA#Microsoft_DirectX_SDK)
- [Git for Windows](https://git-scm.com/download/win) (optional)

**Additional requirement for this fork**
- FMOD Core SDK (x86) installed to `vendor\fmod\` (`vendor\fmod\inc\` and `vendor\fmod\lib\x86\`)

**Steps**

1. Run `win-create-projects.bat` to generate the Visual Studio solution.
2. Open `Build\MTASA.sln`.
3. Compile (target: **Win32 / Release**).
4. Run `win-install-data.bat` to copy runtime data.

To build from the command line:

```bat
win-build.bat [Debug|Release] [Win32|x64]
```

Visit the wiki article [Compiling MTASA](https://wiki.multitheftauto.com/wiki/Compiling_MTASA) for additional error troubleshooting.

> **Maetro build** (Windows 7 / 8 / 8.1 compatibility):
> ```powershell
> $env:MTA_MAETRO='true'; .\win-create-projects.bat; $env:MTA_MAETRO=$null
> ```

### GNU/Linux (server only)

Supported architectures: x86, x86_64, armhf, arm64 (ARM is experimental).

**Script build**

```sh
./linux-build.sh [--arch=x86|x64|arm|arm64] [--config=debug|release] [--cores=<n>]
./linux-install-data.sh   # optional
```

**Manual build**

```sh
./utils/premake5 gmake
make -C Build/ config=release_x64 all
```

**Docker build**

```sh
# x86_64
docker run --rm -v `pwd`:/build ghcr.io/multitheftauto/mtasa-blue-build:latest --arch=x64
```

See `utils/docker/Dockerfile` for up-to-date build dependencies.

### Adding new source files (Windows)

Re-run `win-create-projects.bat` after adding or removing `.cpp` / `.h` files. Premake regenerates the solution from `premake5.lua`; editing `.vcxproj` files directly is not supported.

---

## Code Formatting

Run after any C++ change (requires PowerShell 7+):

```powershell
./utils/clang-format.ps1
```

This downloads a pinned clang-format binary and formats all `.c/.cpp/.h` files under `Client/`, `Server/`, `Shared/`, and `Tests/`. CI enforces this on Linux; format locally before pushing.

---

## License

Unless otherwise specified, all source code hosted on this repository is licensed under the GPLv3 license. See the [LICENSE](./LICENSE) file for more details.

Grand Theft Auto and all related trademarks are © Rockstar North 1997–2026.  
Multi Theft Auto is © the Multi Theft Auto team. This fork is an independent project and carries no warranty or support from them.
