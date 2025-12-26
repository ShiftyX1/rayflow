# Build System Instructions

This document describes the build system architecture, dependency management,
and the workflow for adding new third-party libraries.

## Build System Overview

### Technology Stack
- **CMake 3.21+** — Build system generator
- **Ninja** — Default build backend (fast incremental builds)
- **C++17** — Language standard (MSVC compatibility, no designated initializers)
- **FetchContent** — Dependency management for header-only and source libs

### Directory Structure
```
cmake/
├── RayflowDependencies.cmake   # All external dependencies
├── toolchains/
│   ├── linux-gcc.cmake         # Linux GCC toolchain
│   ├── linux-clang.cmake       # Linux Clang toolchain
│   ├── macos-clang.cmake       # macOS AppleClang toolchain
│   ├── windows-msvc.cmake      # Windows MSVC toolchain
│   └── windows-mingw.cmake     # Windows MinGW/MSYS2 toolchain
CMakePresets.json               # Build presets (debug, release, CI)
```

### CMake Presets

Use presets for consistent builds across machines:

```bash
# Configure
cmake --preset debug          # Debug build with tests
cmake --preset release        # Release build with PAK assets
cmake --preset ci-linux       # CI Linux preset
cmake --preset ci-windows     # CI Windows MSVC preset

# Build
cmake --build --preset debug
cmake --build --preset release

# Test
ctest --preset debug --output-on-failure
```

Available presets:
| Preset | Type | Tests | PAK | Use Case |
|--------|------|-------|-----|----------|
| `debug` | Debug | ON | OFF | Local development |
| `release` | Release | OFF | ON | Distribution |
| `relwithdebinfo` | RelWithDebInfo | OFF | ON | Profiling |
| `ci-linux` | Debug | ON | OFF | GitHub Actions Linux |
| `ci-macos` | Debug | ON | OFF | GitHub Actions macOS |
| `ci-windows` | Debug | ON | OFF | GitHub Actions Windows MSVC |

### Build Targets

Main targets:
- `rayflow` — Main game client
- `rfds` — Dedicated server (headless)
- `map_builder` — Map editor

Test targets:
- `rayflow_tests_shared` — Shared library tests
- `rayflow_tests_server` — Server tests
- `rayflow_tests_client` — Client tests
- `rayflow_tests_integration` — Integration tests

Build specific target:
```bash
cmake --build --preset debug --target rayflow
cmake --build --preset debug --target rayflow_tests_server
```

## Dependency Management

### Current Dependencies

| Library | Version | Discovery Method | Location |
|---------|---------|-----------------|----------|
| raylib | 5.5 | find_package → manual → FetchContent | System or fetched |
| EnTT | v3.13.2 | FetchContent | Always fetched |
| tinyxml2 | 10.0.0 | FetchContent | Always fetched |
| Catch2 | 3.5.2 | FetchContent (tests only) | Always fetched |

### raylib Discovery Strategy

raylib uses a three-tier discovery strategy:

1. **find_package** — Check for CMake config (vcpkg, proper install)
2. **Manual search** — Look in common paths (Homebrew, MSYS2, apt)
3. **FetchContent** — Download and build from source

Force FetchContent:
```bash
cmake --preset debug -DRAYFLOW_FETCH_RAYLIB=ON
```

### Adding a New Dependency

#### Step 1: Determine Discovery Strategy

- **Header-only lib** → FetchContent always (simple, reproducible)
- **Large compiled lib** → Prefer system package, FetchContent fallback
- **Test-only lib** → FetchContent in tests/ CMakeLists.txt

#### Step 2: Add to RayflowDependencies.cmake

For FetchContent-based dependency:

```cmake
# -----------------------------------------------------------------------------
# LibName (description)
# -----------------------------------------------------------------------------
set(RAYFLOW_LIBNAME_VERSION "v1.0.0" CACHE STRING "LibName version to use")

message(STATUS "Fetching LibName ${RAYFLOW_LIBNAME_VERSION}...")
FetchContent_Declare(
    libname
    GIT_REPOSITORY https://github.com/org/libname.git
    GIT_TAG ${RAYFLOW_LIBNAME_VERSION}
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(libname)
```

For system-preferred dependency (like raylib):

```cmake
# Try find_package first
find_package(LibName ${RAYFLOW_LIBNAME_VERSION} QUIET CONFIG)

if(LibName_FOUND)
    message(STATUS "Found LibName via find_package")
else()
    # Fallback to FetchContent
    message(STATUS "LibName not found, fetching...")
    FetchContent_Declare(...)
    FetchContent_MakeAvailable(libname)
endif()
```

#### Step 3: Link to Targets

In the appropriate CMakeLists.txt:

```cmake
target_link_libraries(my_target PRIVATE libname)
```

For raylib specifically, use the helper function:

```cmake
rayflow_link_raylib(my_target)  # Handles all discovery methods
```

#### Step 4: Update Documentation

1. Add to the dependency table in this file
2. Update README.md if it affects build instructions
3. Test on all CI platforms

### Version Pinning

All dependency versions are centralized at the top of `RayflowDependencies.cmake`:

```cmake
set(RAYFLOW_RAYLIB_VERSION "5.5" CACHE STRING "raylib version")
set(RAYFLOW_ENTT_VERSION "v3.13.2" CACHE STRING "EnTT version")
set(RAYFLOW_TINYXML2_VERSION "10.0.0" CACHE STRING "tinyxml2 version")
```

To override at configure time:
```bash
cmake --preset debug -DRAYFLOW_RAYLIB_VERSION=5.0
```

## C++17 Compatibility Rules

### No Designated Initializers

MSVC in C++17 mode does not support designated initializers. This is a C++20 feature.

❌ **Wrong:**
```cpp
InputFrame frame{
    .seq = 1,
    .moveX = 0.5f,
    .moveY = 0.0f
};
```

✅ **Correct:**
```cpp
InputFrame frame;
frame.seq = 1;
frame.moveX = 0.5f;
frame.moveY = 0.0f;
```

Or use helper functions in tests:
```cpp
// In tests/helpers/test_utils.hpp
inline InputFrame make_input_frame(uint32_t seq, float moveX = 0.f, float moveY = 0.f) {
    InputFrame frame;
    frame.seq = seq;
    frame.moveX = moveX;
    frame.moveY = moveY;
    return frame;
}
```

### Other C++17 Considerations

- Use `std::optional`, `std::variant`, `std::string_view` freely
- `if constexpr` is supported
- Structured bindings work: `auto [x, y] = pair;`
- Avoid `std::filesystem` on older platforms (use platform abstractions)

## CI/CD Pipeline

### GitHub Actions Workflows

Located in `.github/workflows/`:

| Workflow | Trigger | Platforms | Purpose |
|----------|---------|-----------|---------|
| `build.yml` | Push/PR | Linux, macOS, Windows | Compile check |
| `tests.yml` | Push/PR | Linux, macOS, Windows | Run test suite |
| `release.yml` | Tag `v*` | Linux, macOS, Windows | Build artifacts |

### CI Matrix

```yaml
strategy:
  matrix:
    include:
      - os: ubuntu-24.04
        preset: ci-linux
      - os: macos-14
        preset: ci-macos
      - os: windows-2022
        preset: ci-windows
```

### Adding a New CI Platform

1. Create toolchain file in `cmake/toolchains/`
2. Add preset to `CMakePresets.json`
3. Add matrix entry to workflows

## Troubleshooting

### raylib Not Found

```
CMake Error: raylib not available for target...
```

Solutions:
1. Install raylib system-wide (recommended for development)
2. Force FetchContent: `cmake --preset debug -DRAYFLOW_FETCH_RAYLIB=ON`
3. Set `RAYLIB_PATH` environment variable

### MSVC Designated Initializer Error

```
error C7555: use of designated initializers requires at least '/std:c++20'
```

Replace designated initializers with explicit member assignment (see above).

### Ninja Not Found

```
CMake Error: Could not find Ninja
```

Install Ninja:
- **macOS:** `brew install ninja`
- **Linux:** `sudo apt install ninja-build`
- **Windows:** `choco install ninja` or MSYS2 `pacman -S ninja`

### FetchContent Download Fails

```
Failed to download: https://github.com/...
```

Check network/proxy. For CI, ensure actions/checkout runs first.
Consider caching `_deps/` directory for faster CI builds.
