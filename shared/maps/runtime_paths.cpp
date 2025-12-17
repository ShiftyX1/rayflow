#include "runtime_paths.hpp"

#include <system_error>

namespace shared::maps {

std::filesystem::path runtime_maps_dir() {
    namespace fs = std::filesystem;

    // Common launch locations:
    // - from build/: maps/
    // - from project root: build/maps/
    // - from app/ or server/: ../build/maps/
    // - from nested IDE run dirs: ../../build/maps/
    const fs::path candidates[] = {
        fs::path{"maps"},
        fs::path{"../maps"},
        fs::path{"../../maps"},
        fs::path{"build/maps"},
        fs::path{"../build/maps"},
        fs::path{"../../build/maps"},
    };

    std::error_code ec;
    for (const auto& p : candidates) {
        ec.clear();
        if (fs::exists(p, ec) && !ec && fs::is_directory(p, ec) && !ec) {
            return p;
        }
    }

    return fs::path{"maps"};
}

} // namespace shared::maps
