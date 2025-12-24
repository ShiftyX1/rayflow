#pragma once

// RayFlow PAK Archive Format (RFPK)
//
// Simple uncompressed archive format for game assets.
// No external dependencies, fast random access.
//
// Layout:
// ┌─────────────────────────────────────┐
// │ Header (24 bytes)                   │
// │   magic[4]      = "RFPK"            │
// │   version       : u32 = 1           │
// │   entry_count   : u32               │
// │   toc_offset    : u64               │
// ├─────────────────────────────────────┤
// │ File Data (variable)                │
// │   Concatenated file contents        │
// │   (no alignment padding)            │
// ├─────────────────────────────────────┤
// │ Table of Contents (variable)        │
// │   For each entry:                   │
// │     offset      : u64               │
// │     size        : u64               │
// │     path_len    : u32               │
// │     path        : char[path_len]    │
// └─────────────────────────────────────┘

#include <cstddef>
#include <cstdint>

namespace shared::vfs {

// PAK file magic number: "RFPK" in little-endian
constexpr std::uint32_t PAK_MAGIC = 0x4B504652;  // 'R','F','P','K'

// Current format version
constexpr std::uint32_t PAK_VERSION = 1;

// Header size in bytes
constexpr std::size_t PAK_HEADER_SIZE = 24;

// Maximum path length (to prevent malicious files)
constexpr std::size_t PAK_MAX_PATH_LENGTH = 4096;

#pragma pack(push, 1)

struct PakHeader {
    std::uint32_t magic{PAK_MAGIC};
    std::uint32_t version{PAK_VERSION};
    std::uint32_t entryCount{0};
    std::uint32_t reserved{0};  // Alignment padding, reserved for future use
    std::uint64_t tocOffset{0};
};

static_assert(sizeof(PakHeader) == PAK_HEADER_SIZE, "PakHeader must be 24 bytes");

// TOC entry header (path follows immediately after)
struct PakTocEntry {
    std::uint64_t offset{0};
    std::uint64_t size{0};
    std::uint32_t pathLength{0};
    // char path[pathLength] follows
};

#pragma pack(pop)

} // namespace shared::vfs
