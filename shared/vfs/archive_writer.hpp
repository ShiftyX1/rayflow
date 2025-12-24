#pragma once

#include "pak_format.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace shared::vfs {

// PAK archive writer.
// Creates a .pak file from a set of files.
class ArchiveWriter {
public:
    ArchiveWriter();
    ~ArchiveWriter();

    // Non-copyable.
    ArchiveWriter(const ArchiveWriter&) = delete;
    ArchiveWriter& operator=(const ArchiveWriter&) = delete;

    // Begin writing to a new archive file.
    // @return true on success.
    bool begin(const std::filesystem::path& archivePath);

    // Add a file to the archive.
    // @param archivePath  Path inside the archive (e.g., "textures/terrain.png").
    // @param data         File contents.
    // @return true on success.
    bool add_file(const std::string& archivePath, const std::vector<std::uint8_t>& data);

    // Add a file from disk.
    // @param archivePath  Path inside the archive.
    // @param sourcePath   Path to source file on disk.
    // @return true on success.
    bool add_file_from_disk(const std::string& archivePath, const std::filesystem::path& sourcePath);

    // Finalize the archive (write TOC and header).
    // @return true on success.
    bool finalize();

    // Cancel and remove incomplete archive.
    void cancel();

    // Get number of files added so far.
    std::uint32_t file_count() const { return static_cast<std::uint32_t>(entries_.size()); }

private:
    struct PendingEntry {
        std::string path;
        std::uint64_t offset{0};
        std::uint64_t size{0};
    };

    std::filesystem::path outputPath_;
    FILE* file_{nullptr};
    std::vector<PendingEntry> entries_;
    std::uint64_t currentOffset_{PAK_HEADER_SIZE};  // Start after header
    bool finalized_{false};
};

} // namespace shared::vfs
