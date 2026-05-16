# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Midnight Purple:SA** is a custom fork of [mtasa-blue](https://github.com/multitheftauto/mtasa-blue) — the open-source C++ client/server for Multi Theft Auto: San Andreas. The fork adds an FMOD audio engine alongside the existing BASS audio system, enabling new 3D audio and engine sound simulation capabilities for Lua scripts without breaking retrocompatibility.

The two audio systems coexist intentionally: **BASS** serves legacy Lua scripts via `CLuaAudioDefs`; **FMOD** is the new parallel system exposed via `CLuaFMODDefs`. Don't replace or conflate them.

## Build (Windows)

**Prerequisites:** Visual Studio 2022 (Desktop C++, MFC components), DirectX SDK June 2010, FMOD installed at `vendor\fmod\` (inc/ and lib/x86/).

```bat
:: Generate solution files (run after adding/removing .cpp/.h files)
win-create-projects.bat

:: Open Build\MTASA.sln in Visual Studio, then build
:: Or build from CLI:
win-build.bat [Debug|Release] [Win32|x64]
```

After first build, run `win-install-data.bat` to copy runtime data.

**Target:** x86/Win32, Release configuration. The primary client project is `Build\Client Core.vcxproj`.

**Maetro build** (Windows 7/8/8.1 compatibility):
```powershell
$env:MTA_MAETRO='true'; .\win-create-projects.bat; $env:MTA_MAETRO=$null
```

## Code Formatting

Run after any C++ changes:
```powershell
# Requires PowerShell 7+
./utils/clang-format.ps1
```

This downloads a pinned clang-format binary to `Build\tmp\` and formats all `.c/.cpp/.h` files under `Client/`, `Server/`, `Shared/`, and `Tests/`.

## Architecture

### Module layout

| Directory | Built as | Purpose |
|-----------|----------|---------|
| `Client/core/` | `Core.dll` | Engine hook layer: DX9, input, GUI, audio managers, `CCore` singleton |
| `Client/mods/deathmatch/` | `Client Deathmatch.dll` | Game logic, Lua VM management, all Lua function bindings |
| `Client/game_sa/` | `Game SA.dll` | GTA:SA engine wrappers |
| `Client/multiplayer_sa/` | `Multiplayer SA.dll` | Sync and netcode |
| `Server/mods/deathmatch/` | Server DM module | Server-side logic and Lua |
| `Shared/` | `Shared.lib` | Math, XML, networking, utilities used by both sides |

### Key types and entry points

- **`CCore`** (`Client/core/CCore.h` / `CCore.cpp`) — central singleton. Owns `CFMODManager`. Calls `CFMODManager::Update()` from `DoPostFramePulse()` with camera position/forward/up vectors.
- **`CFMODManager`** (`Client/core/CFMODManager.h` / `CFMODManager.cpp`) — wraps FMOD::System, manages sound/channel lifetimes by integer ID, exposes listener sync, reverb, echo DSP, and named float parameters.
- **`CLuaFMODDefs`** (`Client/mods/deathmatch/logic/luadefs/CLuaFMODDefs.h/.cpp`) — Lua bindings for FMOD. Registered in `CLuaManager::LoadCFunctions()`.
- **`CLuaDefs` subclasses** — all Lua def classes inherit `CLuaDefs` and use the `LUA_DECLARE(FuncName)` macro, which expands to `static int FuncName(lua_State* luaVM)`. New functions go into the appropriate `CLuaXxxDefs` class and are registered in `LoadFunctions()`.

### Adding new Lua functions (pattern)

1. Declare with `LUA_DECLARE(MyFunc)` in the relevant `CLuaXxxDefs.h`.
2. Implement `int CLuaXxxDefs::MyFunc(lua_State* luaVM)` in the `.cpp`, using `CLuaFunctionParser` helpers from `CLuaDefs` (`ArgumentParser<>` for hard errors, `ArgumentParserWarn<>` for soft).
3. Register in `CLuaXxxDefs::LoadFunctions()` via `lua_pushcclosure` / `lua_setglobal`.
4. If adding to a new def class, include and call `LoadFunctions()` from `CLuaManager::LoadCFunctions()` in `Client/mods/deathmatch/logic/lua/CLuaManager.cpp`.

### FMOD vendor

Headers: `vendor/fmod/inc/`  
Library: `vendor/fmod/lib/x86/fmod_vc.lib`  
Runtime DLL: copy to `Bin\mta\` alongside the client binaries.

## Commit messages

Include the motivation, what was changed, and how it was tested. Future engineers should not have to guess why a change was made from the diff alone.

## Test server

Local server with AC disabled: add `<disableac/>` to the server config. The client connects to `localhost`.
