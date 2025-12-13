#pragma once

#include <cstdint>
#include <raylib.h>

namespace voxel {

// Block types
enum class BlockType : uint8_t {
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
    Count
};

// Chunk dimensions
constexpr int CHUNK_WIDTH = 16;
constexpr int CHUNK_HEIGHT = 256;
constexpr int CHUNK_DEPTH = 16;
constexpr int CHUNK_SIZE = CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH;

// Block helper functions
inline bool is_solid(BlockType type) {
    return type != BlockType::Air && type != BlockType::Water;
}

inline bool is_transparent(BlockType type) {
    return type == BlockType::Air || type == BlockType::Water || type == BlockType::Leaves;
}

// Single block alias
using Block = uint8_t;

} // namespace voxel
