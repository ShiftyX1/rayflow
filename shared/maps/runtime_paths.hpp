#pragma once

#include <filesystem>

namespace shared::maps {

// Returns the best guess for the runtime `maps/` directory.
//
// This project is often run from different working directories (project root, build/, app/, server/, etc).
// To keep map editor exports and in-game loads consistent, we search a small set of common relative paths.
std::filesystem::path runtime_maps_dir();

} // namespace shared::maps
