#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace shared::maps {

// Returns the runtime `maps/` directory (next to executable).
// Maps are always loose files, never packed into PAK.
std::filesystem::path runtime_maps_dir();

// Entry representing a map file.
struct MapFileEntry {
    std::filesystem::path path;      // Full path to the file
    std::string filename;            // Just the filename (e.g., "island.rfmap")
};

// Lists all available .rfmap files from the maps/ directory.
std::vector<MapFileEntry> list_available_maps();

} // namespace shared::maps
