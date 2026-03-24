#pragma once

// =============================================================================
// block_data.hpp — Runtime block data registry (shared between client & server)
//
// Games register block properties at startup via register_block_data().
// Engine code queries them through get_block_data().
// Utility functions in block.hpp (is_slab, is_vegetation, etc.) delegate to
// this registry at runtime instead of hardcoding block type logic.
// =============================================================================

#include "block.hpp"
#include "engine/core/export.hpp"

namespace shared::voxel {

/// All shared (client + server) properties for a single block type.
struct BlockData {
    BlockLightProps light{};
    BlockCollisionInfo collision{0.f, 1.f, 0.f, 1.f, 0.f, 1.f, true};

    bool is_solid{false};
    bool is_transparent{true};
    bool is_full_opaque{false};
    bool is_vegetation{false};
    bool is_slab{false};
    bool is_fence{false};
};

/// Maximum number of block types (uint8_t range).
inline constexpr std::size_t MAX_BLOCK_TYPES = 256;

/// Register properties for a single block type.  Call during game init.
RAYFLOW_CORE_API void register_block_data(BlockType type, const BlockData& data);

/// Query block data.  Returns a default (air-like) entry for unregistered IDs.
RAYFLOW_CORE_API const BlockData& get_block_data(BlockType type);

} // namespace shared::voxel
