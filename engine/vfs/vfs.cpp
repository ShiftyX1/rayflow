#include "vfs.hpp"

#include "archive_reader.hpp"

#include <algorithm>
#include <fstream>
#include <mutex>
#include <unordered_set>

namespace engine::vfs {

namespace {

struct MountedArchive {
    std::string mountPoint;
    mutable ArchiveReader reader;  // mutable because extract() modifies internal state
};

struct VfsState {
    std::filesystem::path gameDir;
    InitFlags flags{InitFlags::None};
    std::vector<MountedArchive> archives;
    bool initialized{false};
    mutable std::mutex mutex;
};

VfsState& state() {
    static VfsState s;
    return s;
}

// Normalize virtual path: remove leading/trailing slashes, collapse double slashes.
std::string normalize_path(const std::string& path) {
    std::string result;
    result.reserve(path.size());

    bool lastWasSlash = true;  // Treat start as after slash to skip leading slashes.
    for (char c : path) {
        if (c == '\\') {
            c = '/';
        }
        if (c == '/') {
            if (!lastWasSlash) {
                result += c;
            }
            lastWasSlash = true;
        } else {
            result += c;
            lastWasSlash = false;
        }
    }

    // Remove trailing slash.
    if (!result.empty() && result.back() == '/') {
        result.pop_back();
    }

    return result;
}

// Check if a path matches a mount point.
// Returns the path relative to mount point, or nullopt if no match.
std::optional<std::string> match_mount_point(const std::string& virtualPath,
                                              const std::string& mountPoint) {
    if (mountPoint.empty() || mountPoint == "/") {
        return virtualPath;
    }

    std::string normalMount = normalize_path(mountPoint);
    if (virtualPath.length() < normalMount.length()) {
        return std::nullopt;
    }

    if (virtualPath.compare(0, normalMount.length(), normalMount) != 0) {
        return std::nullopt;
    }

    if (virtualPath.length() == normalMount.length()) {
        return "";
    }

    if (virtualPath[normalMount.length()] != '/') {
        return std::nullopt;
    }

    return virtualPath.substr(normalMount.length() + 1);
}

std::optional<std::vector<std::uint8_t>> read_loose_file(const std::filesystem::path& filePath) {
    std::error_code ec;
    if (!std::filesystem::exists(filePath, ec) || ec) {
        return std::nullopt;
    }

    const auto size = std::filesystem::file_size(filePath, ec);
    if (ec) {
        return std::nullopt;
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> data(size);
    if (size > 0) {
        file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
        if (!file) {
            return std::nullopt;
        }
    }

    return data;
}

}  // namespace

void init(const std::filesystem::path& gameDir, InitFlags flags) {
    auto& s = state();
    std::lock_guard lock(s.mutex);

    s.gameDir = gameDir;
    s.flags = flags;
    s.archives.clear();
    s.initialized = true;
}

void shutdown() {
    auto& s = state();
    std::lock_guard lock(s.mutex);

    s.archives.clear();
    s.gameDir.clear();
    s.flags = InitFlags::None;
    s.initialized = false;
}

bool is_initialized() {
    auto& s = state();
    std::lock_guard lock(s.mutex);
    return s.initialized;
}

bool mount(const std::filesystem::path& pakFile, const std::string& mountPoint) {
    auto& s = state();
    std::lock_guard lock(s.mutex);

    if (!s.initialized) {
        return false;
    }

    if (s.flags & InitFlags::LooseOnly) {
        // In loose-only mode, skip mounting archives silently.
        return true;
    }

    std::filesystem::path fullPath = pakFile.is_absolute() ? pakFile : s.gameDir / pakFile;

    MountedArchive ma;
    ma.mountPoint = normalize_path(mountPoint);

    if (!ma.reader.open(fullPath)) {
        return false;
    }

    s.archives.push_back(std::move(ma));
    return true;
}

void unmount(const std::filesystem::path& pakFile) {
    auto& s = state();
    std::lock_guard lock(s.mutex);

    std::filesystem::path fullPath = pakFile.is_absolute() ? pakFile : s.gameDir / pakFile;

    auto it = std::remove_if(s.archives.begin(), s.archives.end(),
                              [&fullPath](const MountedArchive& ma) {
                                  return ma.reader.path() == fullPath;
                              });
    s.archives.erase(it, s.archives.end());
}

std::optional<std::vector<std::uint8_t>> read_file(const std::string& virtualPath) {
    auto& s = state();
    std::lock_guard lock(s.mutex);

    if (!s.initialized) {
        return std::nullopt;
    }

    const std::string normalized = normalize_path(virtualPath);

    // 1. Try loose files first (unless NoOverride is set).
    if (!(s.flags & InitFlags::NoOverride)) {
        std::filesystem::path loosePath = s.gameDir / normalized;
        auto looseData = read_loose_file(loosePath);
        if (looseData) {
            return looseData;
        }
    }

    // 2. Try mounted archives (in order).
    if (!(s.flags & InitFlags::LooseOnly)) {
        for (const auto& ma : s.archives) {
            auto relative = match_mount_point(normalized, ma.mountPoint);
            if (!relative) {
                continue;
            }
            auto data = ma.reader.extract(*relative);
            if (data) {
                return data;
            }
        }
    }

    return std::nullopt;
}

std::optional<std::string> read_text_file(const std::string& virtualPath) {
    auto data = read_file(virtualPath);
    if (!data) {
        return std::nullopt;
    }
    return std::string(reinterpret_cast<const char*>(data->data()), data->size());
}

bool exists(const std::string& virtualPath) {
    auto& s = state();
    std::lock_guard lock(s.mutex);

    if (!s.initialized) {
        return false;
    }

    const std::string normalized = normalize_path(virtualPath);

    // Check loose file.
    if (!(s.flags & InitFlags::NoOverride)) {
        std::filesystem::path loosePath = s.gameDir / normalized;
        std::error_code ec;
        if (std::filesystem::exists(loosePath, ec) && !ec) {
            return true;
        }
    }

    // Check archives.
    if (!(s.flags & InitFlags::LooseOnly)) {
        for (const auto& ma : s.archives) {
            auto relative = match_mount_point(normalized, ma.mountPoint);
            if (!relative) {
                continue;
            }
            if (ma.reader.has_file(*relative)) {
                return true;
            }
        }
    }

    return false;
}

std::optional<FileStat> stat(const std::string& virtualPath) {
    auto& s = state();
    std::lock_guard lock(s.mutex);

    if (!s.initialized) {
        return std::nullopt;
    }

    const std::string normalized = normalize_path(virtualPath);

    // Check loose file first.
    if (!(s.flags & InitFlags::NoOverride)) {
        std::filesystem::path loosePath = s.gameDir / normalized;
        std::error_code ec;
        if (std::filesystem::exists(loosePath, ec) && !ec) {
            FileStat fs;
            fs.is_directory = std::filesystem::is_directory(loosePath, ec) && !ec;
            if (!fs.is_directory) {
                fs.size = std::filesystem::file_size(loosePath, ec);
                if (ec) {
                    fs.size = 0;
                }
            }
            fs.from_archive = false;
            return fs;
        }
    }

    // Check archives.
    if (!(s.flags & InitFlags::LooseOnly)) {
        for (const auto& ma : s.archives) {
            auto relative = match_mount_point(normalized, ma.mountPoint);
            if (!relative) {
                continue;
            }
            const auto* entry = ma.reader.get_entry(*relative);
            if (entry) {
                FileStat fs;
                fs.size = entry->size;
                fs.is_directory = false;  // PAK entries are files, not directories
                fs.from_archive = true;
                return fs;
            }
        }
    }

    return std::nullopt;
}

std::vector<std::string> list_dir(const std::string& virtualPath) {
    auto& s = state();
    std::lock_guard lock(s.mutex);

    std::unordered_set<std::string> seen;
    std::vector<std::string> result;

    if (!s.initialized) {
        return result;
    }

    const std::string normalized = normalize_path(virtualPath);

    // List loose files.
    if (!(s.flags & InitFlags::NoOverride)) {
        std::filesystem::path loosePath = s.gameDir / normalized;
        std::error_code ec;
        if (std::filesystem::is_directory(loosePath, ec) && !ec) {
            for (const auto& entry : std::filesystem::directory_iterator(loosePath, ec)) {
                if (ec) {
                    break;
                }
                std::string name = entry.path().filename().string();
                if (entry.is_directory()) {
                    name += '/';
                }
                if (seen.insert(name).second) {
                    result.push_back(name);
                }
            }
        }
    }

    // List from archives.
    if (!(s.flags & InitFlags::LooseOnly)) {
        for (const auto& ma : s.archives) {
            auto relative = match_mount_point(normalized, ma.mountPoint);
            if (!relative) {
                continue;
            }
            auto archiveEntries = ma.reader.list_directory(*relative);
            for (auto& name : archiveEntries) {
                if (seen.insert(name).second) {
                    result.push_back(std::move(name));
                }
            }
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::optional<std::filesystem::path> resolve_loose_path(const std::string& virtualPath) {
    auto& s = state();
    std::lock_guard lock(s.mutex);

    if (!s.initialized) {
        return std::nullopt;
    }

    const std::string normalized = normalize_path(virtualPath);
    std::filesystem::path loosePath = s.gameDir / normalized;

    std::error_code ec;
    if (std::filesystem::exists(loosePath, ec) && !ec) {
        return loosePath;
    }

    return std::nullopt;
}

const std::filesystem::path& get_game_dir() {
    return state().gameDir;
}

} // namespace engine::vfs
