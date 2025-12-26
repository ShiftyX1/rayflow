# Resource System Instructions

## Overview

RayFlow uses a custom Virtual File System (VFS) with a proprietary RFPK archive format for asset management. The system supports two modes:

- **Debug mode** (`RAYFLOW_USE_PAK=0`): Reads loose files directly from disk for fast iteration.
- **Release mode** (`RAYFLOW_USE_PAK=1`): Reads from `assets.pak` archive, with loose file fallback.

## Architecture

```
shared/vfs/
├── pak_format.hpp      # RFPK binary format definitions
├── archive_reader.hpp  # PAK file reader
├── archive_reader.cpp
├── archive_writer.hpp  # PAK file writer (used by pack_assets tool)
├── archive_writer.cpp
├── vfs.hpp             # Virtual File System API
└── vfs.cpp

client/core/
├── resources.hpp       # High-level resource loading helpers
└── resources.cpp       # raylib integration (textures, shaders, fonts)
```

## Hard Rules

### Layering

1. **VFS belongs in `shared/`** — both client and tools use it.
2. **`resources.*` belongs in `client/`** — it depends on raylib.
3. **Server must NOT use VFS** — server is headless, no asset loading needed.
4. **Never include raylib headers in `shared/vfs/`**.

### Initialization

1. **Always call `resources::init()` after `InitWindow()`** — VFS needs working directory.
2. **Always call `resources::shutdown()` before `CloseWindow()`**.
3. **Do not manually call `shared::vfs::init()`** — `resources::init()` handles it.

### Path Format

1. **Use forward slashes** — `"textures/terrain.png"`, not backslashes.
2. **No leading slash** — `"textures/foo.png"`, not `"/textures/foo.png"`.
3. **Paths are relative to game root** — where the executable runs.

### Build System

1. **`RAYFLOW_USE_PAK` is set automatically by CMake** based on `CMAKE_BUILD_TYPE`.
2. **Debug builds** get `RAYFLOW_USE_PAK=0` (loose files).
3. **Release/RelWithDebInfo/MinSizeRel** get `RAYFLOW_USE_PAK=1` (PAK mode).
4. **Do not hardcode** `RAYFLOW_USE_PAK` — use CMake's `RAYFLOW_USE_PAK` option.

## API Usage

### Loading Resources

```cpp
#include "client/core/resources.hpp"

// In main() or Game::init():
resources::init();

// Loading assets (works in both Debug and Release):
Texture2D tex = resources::load_texture("textures/terrain.png");
Shader shader = resources::load_shader("shaders/voxel.vs", "shaders/voxel.fs");
Font font = resources::load_font("fonts/Inter_18pt-Regular.ttf", 24);
Image img = resources::load_image("textures/skybox/panorama/sky.png");
std::string xml = resources::load_text("ui/hud.xml");

// Cleanup:
resources::shutdown();
```

### Checking Mode

```cpp
if (resources::is_pak_mode()) {
    TraceLog(LOG_INFO, "Running in Release mode (PAK)");
} else {
    TraceLog(LOG_INFO, "Running in Debug mode (loose files)");
}
```

### Direct VFS Access (Advanced)

For lower-level operations, use the VFS API directly:

```cpp
#include "vfs/vfs.hpp"

// Check if file exists
if (shared::vfs::exists("ui/custom.xml")) { ... }

// List directory contents
auto files = shared::vfs::list_dir("textures/");

// Get file metadata
auto stat = shared::vfs::stat("textures/terrain.png");
if (stat && stat->from_archive) {
    // File is from .pak archive
}

// Read raw bytes
auto data = shared::vfs::read_file("data/config.bin");

// Read text file
auto text = shared::vfs::read_text_file("ui/styles.css");
```

## RFPK Archive Format

### Structure

```
┌─────────────────────────────────────┐
│ Header (24 bytes)                   │
│   magic[4]      = "RFPK"            │
│   version       : u32 = 1           │
│   entry_count   : u32               │
│   reserved      : u32 = 0           │
│   toc_offset    : u64               │
├─────────────────────────────────────┤
│ File Data (variable)                │
│   Concatenated file contents        │
│   (no compression, no alignment)    │
├─────────────────────────────────────┤
│ Table of Contents (variable)        │
│   For each entry:                   │
│     offset      : u64               │
│     size        : u64               │
│     path_len    : u32               │
│     path        : char[path_len]    │
└─────────────────────────────────────┘
```

### Properties

- **No compression** — PNG/TTF are already compressed; avoids CPU overhead.
- **No encryption** — open-source project, no need for obfuscation.
- **No external dependencies** — custom format, header-only C++.
- **Fast random access** — TOC at end allows seeking directly to any file.

## Adding New Asset Types

### 1. Add Copy Target in CMakeLists.txt

If adding a new directory (e.g., `sounds/`):

```cmake
# In the rayflow_copy_assets target:
COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/client/static/sounds
    ${CMAKE_BINARY_DIR}/sounds
```

### 2. Add to Pack List

The `rayflow_pack_assets` target already packs all directories. If you need to exclude files:

```cmake
# In app/pack_assets.cpp, modify the extensions filter or add exclusions
```

### 3. Add Resource Loader (Optional)

If the asset type needs special handling, add a loader to `resources.hpp`:

```cpp
// In resources.hpp:
Sound load_sound(const std::string& path);

// In resources.cpp:
Sound load_sound(const std::string& path) {
#if RAYFLOW_USE_PAK
    auto data = shared::vfs::read_file(path);
    if (data) {
        // Load from memory using raylib's LoadSoundFromWave or similar
    }
    return Sound{0};
#else
    return LoadSound(path.c_str());
#endif
}
```

## Packed Assets

The following directories are packed into `assets.pak`:

| Directory | Contents |
|-----------|----------|
| `textures/` | Block atlas, destroy stages, skybox, UI graphics |
| `shaders/` | GLSL vertex/fragment shaders |
| `fonts/` | TTF font files |
| `ui/` | XML layouts and CSS stylesheets |

## Troubleshooting

### "File not found" in Release but works in Debug

1. Check that the file is being copied/packed:
   ```bash
   # List PAK contents
   ./pack_assets --list build/assets.pak
   ```
2. Verify the path matches exactly (case-sensitive on Linux/macOS).
3. Ensure `resources::init()` was called.

### Build uses wrong mode

1. Check CMake configuration:
   ```bash
   grep RAYFLOW_USE_PAK CMakeCache.txt
   ```
2. If wrong, do a clean rebuild:
   ```bash
   rm -rf CMakeCache.txt CMakeFiles && cmake .. -DCMAKE_BUILD_TYPE=Release
   ```

### Hot-reload not working

Hot-reload only works in Debug mode with loose files. In Release mode, you must rebuild the PAK:
```bash
make rayflow_pack_assets
```

## Performance Considerations

1. **Prefer batch loading** — Load textures/shaders at startup, not every frame.
2. **Cache loaded resources** — Don't call `load_texture()` repeatedly for the same path.
3. **PAK mode is faster for many files** — One file handle vs. many.
4. **Memory mapping not used** — Simple `fread()` is sufficient for game-sized assets.

## Future Extensions

Potential improvements (not yet implemented):

- Compression (LZ4) for shader/text files
- Asset streaming for large files
- Patching system for updates
- Content validation/checksums

