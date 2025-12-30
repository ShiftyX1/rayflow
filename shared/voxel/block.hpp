#pragma once

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
    BlueOrchid,
    
    Count
};

// Minecraft-style lighting properties per block type.
// These are shared between client and server to ensure deterministic light computation.
// Values are intentionally small:
// - emission: [0..15]
// - *Attenuation: extra attenuation (in addition to the base per-step cost of 1)
struct BlockLightProps {
    std::uint8_t emission;         // 0..15 light emitted by this block
    std::uint8_t blockAttenuation; // extra attenuation for block light (0 = no penalty)
    std::uint8_t skyAttenuation;   // extra attenuation for sky light (0 = no penalty)
    bool opaqueForLight;           // true = blocks all light propagation
    bool skyDimVertical;           // true = sky dims 1 per block downward into this block (leaves/water)
};

constexpr BlockLightProps BLOCK_LIGHT_PROPS[static_cast<std::size_t>(BlockType::Count)] = {
    // Air
    BlockLightProps{ 0u, 0u, 0u, false, false },
    // Stone
    BlockLightProps{ 0u, 0u, 0u, true, false },
    // Dirt
    BlockLightProps{ 0u, 0u, 0u, true, false },
    // Grass
    BlockLightProps{ 0u, 0u, 0u, true, false },
    // Sand
    BlockLightProps{ 0u, 0u, 0u, true, false },
    // Water (treated like leaves for skylight vertical dimming)
    BlockLightProps{ 0u, 0u, 0u, false, true },
    // Wood
    BlockLightProps{ 0u, 0u, 0u, true, false },
    // Leaves
    BlockLightProps{ 0u, 0u, 0u, false, true },
    // Bedrock
    BlockLightProps{ 0u, 0u, 0u, true, false },
    // Gravel
    BlockLightProps{ 0u, 0u, 0u, true, false },
    // Coal
    BlockLightProps{ 0u, 0u, 0u, true, false },
    // Iron
    BlockLightProps{ 0u, 0u, 0u, true, false },
    // Gold
    BlockLightProps{ 0u, 0u, 0u, true, false },
    // Diamond
    BlockLightProps{ 0u, 0u, 0u, true, false },
    // Light (LS-1)
    BlockLightProps{ 15u, 0u, 0u, false, false },
    // StoneSlab (bottom) - partial block, lets light through from above
    BlockLightProps{ 0u, 0u, 0u, false, false },
    // StoneSlabTop - partial block
    BlockLightProps{ 0u, 0u, 0u, false, false },
    // WoodSlab (bottom)
    BlockLightProps{ 0u, 0u, 0u, false, false },
    // WoodSlabTop
    BlockLightProps{ 0u, 0u, 0u, false, false },
    // OakFence - mostly transparent
    BlockLightProps{ 0u, 0u, 0u, false, false },
    // TallGrass - transparent vegetation
    BlockLightProps{ 0u, 0u, 0u, false, false },
    // Poppy - transparent vegetation
    BlockLightProps{ 0u, 0u, 0u, false, false },
    // Dandelion - transparent vegetation
    BlockLightProps{ 0u, 0u, 0u, false, false },
    // BlueOrchid - transparent vegetation
    BlockLightProps{ 0u, 0u, 0u, false, false },
};

inline constexpr const BlockLightProps& get_light_props(BlockType bt) {
    return BLOCK_LIGHT_PROPS[static_cast<std::size_t>(bt)];
}

// Chunk dimensions (shared constants for terrain logic)
constexpr int CHUNK_WIDTH = 16;
constexpr int CHUNK_HEIGHT = 256;
constexpr int CHUNK_DEPTH = 16;
constexpr int CHUNK_SIZE = CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH;

// Collision shape information for server physics.
// These define the collision bounds for non-full blocks.
struct BlockCollisionInfo {
    float minX;  // Minimum X bound (0.0 = block west edge)
    float maxX;  // Maximum X bound (1.0 = block east edge)
    float minY;  // Minimum Y bound (0.0 = block floor)
    float maxY;  // Maximum Y bound (1.0 = block ceiling)
    float minZ;  // Minimum Z bound (0.0 = block north edge)
    float maxZ;  // Maximum Z bound (1.0 = block south edge)
    bool hasCollision;  // Whether the block has any collision
};

// Get collision info for a block type (used by server physics)
constexpr BlockCollisionInfo get_collision_info(BlockType type) {
    switch (type) {
        case BlockType::Air:
        case BlockType::Water:
        case BlockType::Light:
            return {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, false};
        
        case BlockType::StoneSlab:
        case BlockType::WoodSlab:
            return {0.0f, 1.0f, 0.0f, 0.5f, 0.0f, 1.0f, true};  // Bottom half
        
        case BlockType::StoneSlabTop:
        case BlockType::WoodSlabTop:
            return {0.0f, 1.0f, 0.5f, 1.0f, 0.0f, 1.0f, true};  // Top half
        
        case BlockType::OakFence:
            // Fence post: 6/16 to 10/16 in XZ, full height + extra for jumping prevention
            return {6.0f/16.0f, 10.0f/16.0f, 0.0f, 1.5f, 6.0f/16.0f, 10.0f/16.0f, true};
        
        // Vegetation - no collision
        case BlockType::TallGrass:
        case BlockType::Poppy:
        case BlockType::Dandelion:
        case BlockType::BlueOrchid:
            return {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, false};
        
        default:
            return {0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, true};  // Full block
    }
}

// Check if block is a full block for collision purposes
constexpr bool is_full_collision_block(BlockType type) {
    auto coll = get_collision_info(type);
    return coll.hasCollision && 
           coll.minX == 0.0f && coll.maxX == 1.0f &&
           coll.minZ == 0.0f && coll.maxZ == 1.0f;
}

// Check if a block type is a slab (half-height block)
// Note: Top/Bottom is now determined by BlockRuntimeState.slabType, not BlockType
constexpr bool is_slab(BlockType type) {
    return type == BlockType::StoneSlab || type == BlockType::StoneSlabTop ||
           type == BlockType::WoodSlab || type == BlockType::WoodSlabTop;
}

// Get the base slab type (ignoring top/bottom distinction)
// StoneSlabTop -> StoneSlab, WoodSlabTop -> WoodSlab
constexpr BlockType get_base_slab_type(BlockType type) {
    switch (type) {
        case BlockType::StoneSlabTop: return BlockType::StoneSlab;
        case BlockType::WoodSlabTop: return BlockType::WoodSlab;
        default: return type;
    }
}

// Check if a block is a bottom slab (occupies lower half)
// DEPRECATED: Use BlockRuntimeState.slabType instead
constexpr bool is_bottom_slab(BlockType type) {
    return type == BlockType::StoneSlab || type == BlockType::WoodSlab;
}

// Check if a block is a top slab (occupies upper half)
// DEPRECATED: Use BlockRuntimeState.slabType instead
constexpr bool is_top_slab(BlockType type) {
    return type == BlockType::StoneSlabTop || type == BlockType::WoodSlabTop;
}

// Check if a block is vegetation (cross-shaped, no collision)
constexpr bool is_vegetation(BlockType type) {
    return type == BlockType::TallGrass || 
           type == BlockType::Poppy || 
           type == BlockType::Dandelion || 
           type == BlockType::BlueOrchid;
}

namespace util {

inline bool is_solid(BlockType type) {
    // Vegetation blocks are not solid
    if (is_vegetation(type)) return false;
    return type != BlockType::Air && type != BlockType::Water && type != BlockType::Light;
}

inline bool is_transparent(BlockType type) {
    // Vegetation is always transparent
    if (is_vegetation(type)) return true;
    // Slabs and fences are partially transparent (can see adjacent faces)
    if (is_slab(type) || type == BlockType::OakFence) {
        return true;
    }
    return type == BlockType::Air || type == BlockType::Water || type == BlockType::Leaves || type == BlockType::Light;
}

// Check if a block fully occludes faces of adjacent blocks
inline bool is_full_opaque(BlockType type) {
    if (is_vegetation(type)) return false;
    if (is_slab(type) || type == BlockType::OakFence) {
        return false;
    }
    return is_solid(type) && !is_transparent(type);
}

} // namespace util

} // namespace shared::voxel
