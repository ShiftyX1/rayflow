# Forgeflow

[![Build](https://github.com/ShiftyX1/rayflow/actions/workflows/build.yml/badge.svg)](https://github.com/ShiftyX1/rayflow/actions/workflows/build.yml)
[![Tests](https://github.com/ShiftyX1/rayflow/actions/workflows/tests.yml/badge.svg)](https://github.com/ShiftyX1/rayflow/actions/workflows/tests.yml)

Forgeflow is an internal game engine developed at Pulse Studio. Built on GLFW (window/input) + OpenGL 4.1 Core (rendering) + GLM (math). Provides an ECS architecture, voxel renderer, multiplayer networking (client-server), UI framework, and map editor.

## Requirements

- **CMake**: 3.21+
- **C++ compiler**: GCC 10+, Clang 13+, or MSVC 2019+
- **Ninja** (recommended) or Make

## Dependencies

All dependencies are fetched automatically via CMake FetchContent — no pre-installation required.

| Library | Version | Description |
|---------|---------|-------------|
| [GLFW](https://www.glfw.org/) | 3.4 | Window, input, OpenGL context |
| [glad](https://github.com/Dav1dde/glad) | — | OpenGL 4.1 core loader |
| [GLM](https://github.com/g-truc/glm) | 1.0.1 | Math (vec, mat, quat) |
| [stb](https://github.com/nothings/stb) | — | Image and font loading |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.91.8 | Debug UI |
| [EnTT](https://github.com/skypjack/entt) | 3.13.2 | Entity Component System |
| [tinyxml2](https://github.com/leethomason/tinyxml2) | 10.0.0 | XML parser |
| [ENet](https://github.com/lsalzman/enet) | 1.3.18 | Network transport (UDP) |
| [LuaJIT](https://luajit.org/) | 2.1 | Scripting |
| [sol2](https://github.com/ThePhD/sol2) | — | C++ bindings for Lua |

## Build

```bash
# Configure (Debug)
cmake --preset debug

# Build
cmake --build --preset debug

# Run tests
ctest --preset debug
```

### Build on Windows (MSVC)

Requires **Visual Studio 2022** with the "Desktop development with C++" workload and **CMake 3.21+**.

The MSVC compiler (`cl.exe`) must be available in the environment. The recommended way is to initialize the VS developer environment via PowerShell before running CMake:

```powershell
# 1. Initialize x64 MSVC environment
Import-Module "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "C:\Program Files\Microsoft Visual Studio\2022\Community" -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'

# 2. Configure
cmake --preset ci-windows

# 3. Build
cmake --build --preset ci-windows
```

> **Note:** Do not use `VsDevCmd.bat` directly in PowerShell — it does not propagate environment variables back to the shell. Use the `Enter-VsDevShell` module as shown above, or run from the **"x64 Native Tools Command Prompt for VS 2022"** shortcut in the Start menu.

### Build presets

| Preset | Description |
|--------|-------------|
| `debug` | Debug build with tests |
| `release` | Release build with optimizations |
| `ci-windows` | Debug + explicit MSVC toolchain (`cl.exe`, x64) |

### Build individual targets

```bash
# Game client
cmake --build --preset debug --target bedwars

# Dedicated server
cmake --build --preset debug --target bedwars_server

# Map editor
cmake --build --preset debug --target map_builder
```

## Project Structure

```
forgeflow/
├── engine/        # Engine source
│   ├── core/      # Headless engine core (server-compatible)
│   ├── client/    # Client systems (renderer, input, UI)
│   ├── ecs/       # Entity Component System
│   ├── ui/        # XML+CSS UI framework
│   ├── vfs/       # Virtual File System / PAK archives
│   ├── transport/ # Networking transport layer
│   ├── maps/      # RFMAP format
│   └── modules/   # Optional modules (voxel)
├── games/         # Game projects
│   └── bedwars/   # Reference implementation
├── tests/         # Unit and integration tests
├── documentation/ # MkDocs documentation
└── build/         # Build output (CMake generated)
    ├── debug/
    └── release/
```

## Documentation

Full documentation is available in `documentation/forgeflow/docs/`.

