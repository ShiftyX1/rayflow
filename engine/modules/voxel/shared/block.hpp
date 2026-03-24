#pragma once

// =============================================================================
// Block Types and Properties - Shared voxel definitions
// Block IDs and struct definitions shared between client/server.
// Append-only enum — do not reorder or remove existing values.
//
// Block PROPERTY DATA (light, collision, flags) is registered at runtime by
// each game via register_block_data() (see block_data.hpp).
// The utility functions below (is_slab, is_solid, etc.) read from that registry.
// =============================================================================

#include "engine/core/export.hpp"
#include <cstddef>
#include <cstdint>

namespace shared::voxel {

// Forward declare BlockShape for block properties
enum class BlockShape : std::uint8_t;

// Block types (must stay stable across client/server/protocol)
// WARNING: append-only enum! Do not reorder or remove existing values.
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
    
    // Non-full blocks (slabs, fences, etc.)
    StoneSlab,
    StoneSlabTop,
    WoodSlab,
    WoodSlabTop,
    OakFence,
    
    // Vegetation (cross-shaped, no collision)
    TallGrass,
    Poppy,
    Dandelion,
    DeadBush,
    
    // Team blocks (BW-1): colored team markers for BedWars spawn points / beds
    TeamRed,
    TeamBlue,
    TeamGreen,
    TeamYellow,
    
    Count
};

// Minecraft-style lighting properties per block type.
struct BlockLightProps {
    std::uint8_t emission;         // 0..15 light emitted by this block
    std::uint8_t blockAttenuation; // extra attenuation for block light (0 = no penalty)
    std::uint8_t skyAttenuation;   // extra attenuation for sky light (0 = no penalty)
    bool opaqueForLight;           // true = blocks all light propagation
    bool skyDimVertical;           // true = sky dims 1 per block downward into this block (leaves/water)
};

// Chunk dimensions (shared constants for terrain logic)
constexpr int CHUNK_WIDTH = 16;
constexpr int CHUNK_HEIGHT = 256;
constexpr int CHUNK_DEPTH = 16;
constexpr int CHUNK_SIZE = CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH;

// Collision shape information for server physics.
struct BlockCollisionInfo {
    float minX;
    float maxX;
    float minY;
    float maxY;
    float minZ;
    float maxZ;
    bool hasCollision;
};

// ============================================================================
// Runtime block-data queries (implemented in block_data.cpp)
// Backed by the runtime array populated via register_block_data().
// ============================================================================

RAYFLOW_CORE_API const BlockLightProps& get_light_props(BlockType bt);
RAYFLOW_CORE_API BlockCollisionInfo     get_collision_info(BlockType type);
RAYFLOW_CORE_API bool                   is_full_collision_block(BlockType type);
RAYFLOW_CORE_API bool                   is_slab(BlockType type);
RAYFLOW_CORE_API bool                   is_vegetation(BlockType type);

// Pure type-mapping helpers (no data dependency, kept constexpr).
constexpr BlockType get_base_slab_type(BlockType type) {
    switch (type) {
        case BlockType::StoneSlabTop: return BlockType::StoneSlab;
        case BlockType::WoodSlabTop: return BlockType::WoodSlab;
        default: return type;
    }
}

[[deprecated("Use BlockRuntimeState.slabType instead")]]
constexpr bool is_bottom_slab(BlockType type) {
    return type == BlockType::StoneSlab || type == BlockType::WoodSlab;
}

[[deprecated("Use BlockRuntimeState.slabType instead")]]
constexpr bool is_top_slab(BlockType type) {
    return type == BlockType::StoneSlabTop || type == BlockType::WoodSlabTop;
}

namespace util {

RAYFLOW_CORE_API bool is_solid(BlockType type);
RAYFLOW_CORE_API bool is_transparent(BlockType type);
RAYFLOW_CORE_API bool is_full_opaque(BlockType type);

} // namespace util

} // namespace shared::voxel
