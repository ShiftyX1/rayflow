#include "block_data.hpp"

namespace shared::voxel {

static BlockData g_block_data[MAX_BLOCK_TYPES]{};

void register_block_data(BlockType type, const BlockData& data) {
    g_block_data[static_cast<std::size_t>(type)] = data;
}

const BlockData& get_block_data(BlockType type) {
    return g_block_data[static_cast<std::size_t>(type)];
}

// ============================================================================
// block.hpp function implementations (backed by g_block_data)
// ============================================================================

const BlockLightProps& get_light_props(BlockType bt) {
    return g_block_data[static_cast<std::size_t>(bt)].light;
}

BlockCollisionInfo get_collision_info(BlockType type) {
    return g_block_data[static_cast<std::size_t>(type)].collision;
}

bool is_full_collision_block(BlockType type) {
    auto& c = g_block_data[static_cast<std::size_t>(type)].collision;
    return c.hasCollision &&
           c.minX == 0.f && c.maxX == 1.f &&
           c.minZ == 0.f && c.maxZ == 1.f;
}

bool is_slab(BlockType type) {
    return g_block_data[static_cast<std::size_t>(type)].is_slab;
}

bool is_vegetation(BlockType type) {
    return g_block_data[static_cast<std::size_t>(type)].is_vegetation;
}

namespace util {

bool is_solid(BlockType type) {
    return g_block_data[static_cast<std::size_t>(type)].is_solid;
}

bool is_transparent(BlockType type) {
    return g_block_data[static_cast<std::size_t>(type)].is_transparent;
}

bool is_full_opaque(BlockType type) {
    return g_block_data[static_cast<std::size_t>(type)].is_full_opaque;
}

} // namespace util

} // namespace shared::voxel
