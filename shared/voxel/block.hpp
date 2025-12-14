#pragma once

#include <cstdint>

namespace shared::voxel {

// Block types (must stay stable across client/server/protocol)
enum class BlockType : std::uint8_t {
    Air = 0,
    Stone,
    Dirt,
    Grass,
    Sand,
    Water,
    Wood,
    Leaves,
    Bedrock,
    Gravel,
    Coal,
    Iron,
    Gold,
    Diamond,
    // LS-1: map/editor light marker block. Must stay stable (append-only).
    Light,
    Count
};

// Chunk dimensions (shared constants for terrain logic)
constexpr int CHUNK_WIDTH = 16;
constexpr int CHUNK_HEIGHT = 256;
constexpr int CHUNK_DEPTH = 16;
constexpr int CHUNK_SIZE = CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH;

namespace util {

inline bool is_solid(BlockType type) {
    return type != BlockType::Air && type != BlockType::Water && type != BlockType::Light;
}

inline bool is_transparent(BlockType type) {
    return type == BlockType::Air || type == BlockType::Water || type == BlockType::Leaves || type == BlockType::Light;
}

} // namespace util

} // namespace shared::voxel
