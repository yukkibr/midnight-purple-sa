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
- All FMOD sounds route through a master `ChannelGroup` for global volume control independent of BASS.
- Automatic channel cleanup every frame — finished channels are purged from the internal map without manual book-keeping from scripts.
- Sounds start paused, 3D attributes are applied, then unpaused — preventing a single-frame positional pop on spawn.

### New Lua API (`fmod*` globals)

All functions are available client-side. Sound and channel IDs are plain integers managed by `CFMODManager`.

**Sound lifetime**

| Function | Returns | Description |
|----------|---------|-------------|
| `fmodCreateSound(path, [is3D=false], [loop=false])` | soundId / `false` | Load a sound asset from a resource path. |
| `fmodFreeSound(soundId)` | bool | Release the sound and free its memory. |

**Playback**

| Function | Returns | Description |
|----------|---------|-------------|
| `fmodPlaySound(soundId, [x, y, z, [minDist=1, maxDist=100]])` | channelId / `false` | Spawn a playing instance of a sound. |
| `fmodStopSound(channelId)` | bool | Stop and remove the channel. |
| `fmodIsSoundPlaying(channelId)` | bool | Check whether a channel is still active. |

**Per-channel control**

| Function | Returns | Description |
|----------|---------|-------------|
| `fmodSetSoundVolume(channelId, volume)` | bool | Volume 0.0 – 1.0. |
| `fmodSetSoundPitch(channelId, pitch)` | bool | 1.0 = normal, 2.0 = one octave up. |
| `fmodSetSoundPosition(channelId, x, y, z)` | bool | Update world position for a 3D channel. |

**Environmental reverb** (global slot 0; opt-in per channel)

| Function | Returns | Description |
|----------|---------|-------------|
| `fmodSetReverbPreset(preset)` | bool | Set the global reverb character. Presets: `"off"`, `"generic"`, `"room"`, `"bathroom"`, `"cave"`, `"city"`, `"alley"`, `"forest"`, `"mountains"`, `"plain"`, `"sewerpipe"`, `"underwater"`, `"stonecorridor"`, `"hallway"`, `"stoneroom"`, `"auditorium"`, `"concerthall"`, `"arena"`, `"hangar"`, `"parkinglot"`, `"paddedcell"`, `"livingroom"`, `"quarry"`. |
| `fmodSetReverbWetLevel(wetDB)` | bool | Override the output level (-80 to +20 dB) without changing the preset's character. |
| `fmodSetChannelReverb(channelId, wetDB)` | bool | Connect a channel to the reverb bus. Channels are **dry by default**; call this to opt in. |

**Echo DSP** (per-channel)

| Function | Returns | Description |
|----------|---------|-------------|
| `fmodSetChannelEcho(channelId, delayMS, feedbackPct, wetDB)` | bool | Attach or update an echo effect on a channel. |
| `fmodRemoveChannelEcho(channelId)` | bool | Remove the echo DSP from a channel. |

**Master volume**

| Function | Returns | Description |
|----------|---------|-------------|
| `fmodSetMasterVolume(volume)` | bool | Scale all FMOD sounds together (0.0 – 1.0, default 0.5). |
| `fmodGetMasterVolume()` | number | Read the current FMOD master volume. |

**Named float parameters** (ambience system)

| Function | Returns | Description |
|----------|---------|-------------|
| `fmodSetParameter(name, value)` | bool | Store a named float for use by the ambience scripting layer. |
| `fmodGetParameter(name [, default=0])` | number | Read back a stored parameter. |

---

## Roadmap

The items below represent what is needed to consider the FMOD integration feature-complete.

### Core audio features
- [ ] **Doppler effect** — pass per-channel velocity to `set3DAttributes` so moving sources shift pitch relative to the listener
- [ ] **Pause / resume** — `fmodPauseSound(channelId)` / `fmodResumeSound(channelId)`
- [ ] **Streaming audio** — create sounds via `FMOD_CREATESTREAM` for long music tracks, avoiding full decode into RAM
- [ ] **Loop state toggle** — `fmodSetSoundLooped` / `fmodGetSoundLooped` to change loop mode after creation
- [ ] **Read-back functions** — `fmodGetSoundPosition`, `fmodGetSoundVolume`, `fmodGetSoundPitch`

### DSP / effects
- [ ] **Additional DSP types** — low-pass / high-pass filter, distortion, flanger, chorus exposed as Lua functions
- [ ] **DSP chain query** — `fmodGetChannelEffects()` to list active DSPs on a channel

### Architecture & scripting
- [ ] **Per-resource sound tracking** — sounds should be automatically freed when the resource that created them stops, matching how MTA handles BASS sounds
- [ ] **Volume categories** — separate master volumes for SFX, ambient, and music channels (`fmodSetCategoryVolume`)
- [ ] **Server-triggered playback** — server Lua instructs clients to play a positioned FMOD sound, syncing audio events across players
- [ ] **Occlusion / obstruction** — geometry-aware audio muffling when solid objects are between a sound source and the listener (FMOD Geometry API)

### Vehicle engine sounds (primary motivator)
- [ ] **Engine sound layer** — a dedicated `CEngineAudioController` that drives FMOD channel pitch and volume from vehicle RPM and speed each frame
- [ ] **Per-vehicle sound bank** — assign custom sound assets to vehicle models via Lua (`fmodSetVehicleEngineSound(vehicleModel, soundId)`)
- [ ] **Rev limiter / gear shift events** — fire one-shot FMOD sounds on gear change and rev limiter hit
- [ ] **Exhaust 3D positioning** — bind the engine sound source to the vehicle's exhaust bone position instead of the vehicle origin

### Quality of life
- [ ] **FMOD error propagation** — surface `FMOD_RESULT` codes back to Lua (`false, errorCode, errorString`) instead of plain `false`
- [ ] **`fmodGetVersion()`** — expose the FMOD Core library version string for diagnostic output

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
