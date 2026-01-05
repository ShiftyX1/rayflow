#include "runtime_paths.hpp"

#include <algorithm>
#include <system_error>

namespace shared::maps {

namespace {
    std::filesystem::path g_base_path;
}

void set_base_path(const std::filesystem::path& path) {
    g_base_path = path;
}

std::filesystem::path runtime_maps_dir() {
    // Maps are always in maps/ next to the executable.
    if (g_base_path.empty()) {
        // Fallback to current directory if not set
        return std::filesystem::path{"maps"};
    }
    return g_base_path / "maps";
}

std::vector<MapFileEntry> list_available_maps() {
    namespace fs = std::filesystem;
    std::vector<MapFileEntry> result;

    const fs::path mapsDir = runtime_maps_dir();
    std::error_code ec;

    // Create directory if it doesn't exist.
    if (!fs::exists(mapsDir, ec)) {
        fs::create_directories(mapsDir, ec);
    }

    if (fs::exists(mapsDir, ec) && fs::is_directory(mapsDir, ec)) {
        for (const auto& entry : fs::directory_iterator(mapsDir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;
            const fs::path file = entry.path();
            if (file.extension() != ".rfmap") continue;

            result.push_back(MapFileEntry{file, file.filename().string()});
        }
    }

    // Sort by filename.
    std::sort(result.begin(), result.end(), [](const MapFileEntry& a, const MapFileEntry& b) {
        return a.filename < b.filename;
    });

    return result;
}

} // namespace shared::maps
