#pragma once

#include <cstddef>
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
};

inline constexpr const BlockLightProps& get_light_props(BlockType bt) {
    return BLOCK_LIGHT_PROPS[static_cast<std::size_t>(bt)];
}

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
