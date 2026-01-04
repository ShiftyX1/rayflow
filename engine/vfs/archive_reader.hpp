#pragma once

#include "pak_format.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine::vfs {

// PAK archive reader.
// Opens a .pak file and provides random access to its contents.
class ArchiveReader {
public:
    ArchiveReader();
    ~ArchiveReader();

    // Non-copyable, movable.
    ArchiveReader(const ArchiveReader&) = delete;
    ArchiveReader& operator=(const ArchiveReader&) = delete;
    ArchiveReader(ArchiveReader&& other) noexcept;
    ArchiveReader& operator=(ArchiveReader&& other) noexcept;

    // Open a PAK archive file.
    // @return true on success.
    bool open(const std::filesystem::path& archivePath);

    // Close the archive and release resources.
    void close();

    // Check if archive is open.
    bool is_open() const;

    // Get archive file path.
    const std::filesystem::path& path() const { return archivePath_; }

    // File entry info.
    struct FileEntry {
        std::string name;
        std::uint64_t offset{0};
        std::uint64_t size{0};
    };

    // Get list of all entries in archive.
    const std::vector<FileEntry>& entries() const { return entries_; }

    // Check if file exists in archive.
    bool has_file(const std::string& path) const;

    // Get file entry by path.
    const FileEntry* get_entry(const std::string& path) const;

    // Extract file contents.
    // @return File data, or std::nullopt on error.
    std::optional<std::vector<std::uint8_t>> extract(const std::string& path);

    // List files in a directory within the archive.
    // @param dirPath  Directory path (e.g., "textures/"). Empty string for root.
    // @return List of entry names (files end without '/', dirs end with '/').
    std::vector<std::string> list_directory(const std::string& dirPath) const;

private:
    bool read_header();
    bool read_toc();

    std::filesystem::path archivePath_;
    std::vector<FileEntry> entries_;
    std::unordered_map<std::string, std::size_t> pathIndex_;  // path -> index in entries_

    FILE* file_{nullptr};
};

} // namespace engine::vfs
