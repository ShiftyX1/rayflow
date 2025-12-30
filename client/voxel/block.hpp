#pragma once

#include <raylib.h>

#include "../../shared/voxel/block.hpp"

namespace voxel {

using BlockType = shared::voxel::BlockType;

constexpr int CHUNK_WIDTH = shared::voxel::CHUNK_WIDTH;
constexpr int CHUNK_HEIGHT = shared::voxel::CHUNK_HEIGHT;
constexpr int CHUNK_DEPTH = shared::voxel::CHUNK_DEPTH;
constexpr int CHUNK_SIZE = shared::voxel::CHUNK_SIZE;

inline bool is_solid(BlockType type) { return shared::voxel::util::is_solid(type); }
inline bool is_transparent(BlockType type) { return shared::voxel::util::is_transparent(type); }

using Block = std::uint8_t;

} // namespace voxel
