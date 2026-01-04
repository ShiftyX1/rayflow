#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace engine::vfs {

// Virtual File System initialization flags.
enum class InitFlags : std::uint32_t {
    None = 0,
    LooseOnly = 1 << 0,   // Dev mode: only read loose files, ignore .pak archives.
    NoOverride = 1 << 1,  // Disable loose file override (pak-only mode).
};

inline InitFlags operator|(InitFlags a, InitFlags b) {
    return static_cast<InitFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

inline bool operator&(InitFlags a, InitFlags b) {
    return (static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b)) != 0;
}

// File metadata returned by stat().
struct FileStat {
    std::uint64_t size{0};
    bool is_directory{false};
    bool from_archive{false};  // true if file comes from a .pak archive.
};

// Initialize VFS with base game directory.
// Should be called once at startup before any file operations.
//
// @param gameDir  Root directory containing loose files and .pak archives.
// @param flags    Initialization flags (default: None).
void init(const std::filesystem::path& gameDir, InitFlags flags = InitFlags::None);

// Shutdown VFS and release all mounted archives.
void shutdown();

// Check if VFS has been initialized.
bool is_initialized();

// Mount a .pak archive (RFPK format).
// Files in the archive will be accessible via their paths inside the archive.
// Multiple archives can be mounted; later mounts have lower priority than earlier ones.
//
// @param pakFile    Path to .pak file (relative to gameDir or absolute).
// @param mountPoint Virtual path prefix for archive contents (default: "/" = root).
// @return true if mounted successfully.
bool mount(const std::filesystem::path& pakFile, const std::string& mountPoint = "/");

// Unmount a previously mounted archive.
void unmount(const std::filesystem::path& pakFile);

// Read entire file contents into memory.
// Search order (unless overridden by InitFlags):
//   1. Loose files in gameDir
//   2. Mounted archives (in mount order)
//
// @param virtualPath  Path relative to game root (e.g., "textures/terrain.png").
// @return File contents, or std::nullopt if not found.
std::optional<std::vector<std::uint8_t>> read_file(const std::string& virtualPath);

// Read file contents as string (convenience for text files).
std::optional<std::string> read_text_file(const std::string& virtualPath);

// Check if a file or directory exists.
bool exists(const std::string& virtualPath);

// Get file metadata.
std::optional<FileStat> stat(const std::string& virtualPath);

// List contents of a directory.
// Returns combined listing from loose files and archives (deduplicated).
// Names ending with '/' indicate subdirectories.
std::vector<std::string> list_dir(const std::string& virtualPath);

// Resolve a virtual path to an actual filesystem path (loose file only).
// Returns std::nullopt if file doesn't exist as loose file.
// Useful when APIs require actual file paths (e.g., some raylib functions).
std::optional<std::filesystem::path> resolve_loose_path(const std::string& virtualPath);

// Get the base game directory.
const std::filesystem::path& get_game_dir();

} // namespace engine::vfs
