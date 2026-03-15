#pragma once

#include "engine/core/export.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace shared::maps {

// Set the base directory for runtime paths (should be directory containing the executable).
// Call this at startup before using other functions.
RAYFLOW_CORE_API void set_base_path(const std::filesystem::path& path);

// Returns the runtime `maps/` directory (next to executable).
// Maps are always loose files, never packed into PAK.
RAYFLOW_CORE_API std::filesystem::path runtime_maps_dir();

// Entry representing a map file.
struct MapFileEntry {
    std::filesystem::path path;      // Full path to the file
    std::string filename;            // Just the filename (e.g., "island.rfmap")
};

// Lists all available .rfmap files from the maps/ directory.
RAYFLOW_CORE_API std::vector<MapFileEntry> list_available_maps();

} // namespace shared::maps
