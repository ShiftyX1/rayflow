#include "block_registry.hpp"
#include <cstdio>

namespace voxel {

BlockRegistry& BlockRegistry::instance() {
    static BlockRegistry instance;
    return instance;
}

BlockRegistry::~BlockRegistry() {
    destroy();
}

bool BlockRegistry::init(const std::string& atlas_path) {
    if (initialized_) return true;
    
    atlas_texture_ = LoadTexture(atlas_path.c_str());
    if (atlas_texture_.id == 0) {
        TraceLog(LOG_ERROR, "Failed to load texture atlas: %s", atlas_path.c_str());
        return false;
    }
    
    atlas_tile_size_ = 16;
    atlas_tiles_per_row_ = atlas_texture_.width / atlas_tile_size_;
    
    register_blocks();
    
    initialized_ = true;
    TraceLog(LOG_INFO, "Block registry initialized with %d block types", static_cast<int>(BlockType::Count));
    
    return true;
}

void BlockRegistry::destroy() {
    if (initialized_) {
        UnloadTexture(atlas_texture_);
        initialized_ = false;
    }
}

void BlockRegistry::register_blocks() {
    // Air
    blocks_[static_cast<size_t>(BlockType::Air)] = {
        "Air", false, true, 0.0f, 0, {0, 0, 0, 0, 0, 0}
    };
    
    // Stone
    blocks_[static_cast<size_t>(BlockType::Stone)] = {
        "Stone", true, false, 1.5f, 1, {1, 1, 1, 1, 1, 1}
    };
    
    // Dirt
    blocks_[static_cast<size_t>(BlockType::Dirt)] = {
        "Dirt", true, false, 0.5f, 0, {2, 2, 2, 2, 2, 2}
    };
    
    // Grass
    blocks_[static_cast<size_t>(BlockType::Grass)] = {
        "Grass", true, false, 0.6f, 0, {3, 3, 0, 2, 3, 3}  // top=grass, bottom=dirt, sides=grass_side
    };
    
    // Sand
    blocks_[static_cast<size_t>(BlockType::Sand)] = {
        "Sand", true, false, 0.5f, 0, {18, 18, 18, 18, 18, 18}
    };
    
    // Water
    blocks_[static_cast<size_t>(BlockType::Water)] = {
        "Water", false, true, 100.0f, 0, {205, 205, 205, 205, 205, 205}
    };
    
    // Wood
    blocks_[static_cast<size_t>(BlockType::Wood)] = {
        "Wood", true, false, 2.0f, 0, {20, 20, 21, 21, 20, 20}  // sides=bark, top/bottom=rings
    };
    
    // Leaves
    blocks_[static_cast<size_t>(BlockType::Leaves)] = {
        "Leaves", true, true, 0.2f, 0, {52, 52, 52, 52, 52, 52}
    };
    
    // Bedrock
    blocks_[static_cast<size_t>(BlockType::Bedrock)] = {
        "Bedrock", true, false, -1.0f, 255, {17, 17, 17, 17, 17, 17}
    };
    
    // Gravel
    blocks_[static_cast<size_t>(BlockType::Gravel)] = {
        "Gravel", true, false, 0.6f, 0, {19, 19, 19, 19, 19, 19}
    };
    
    // Coal Ore
    blocks_[static_cast<size_t>(BlockType::Coal)] = {
        "Coal Ore", true, false, 3.0f, 1, {34, 34, 34, 34, 34, 34}
    };
    
    // Iron Ore
    blocks_[static_cast<size_t>(BlockType::Iron)] = {
        "Iron Ore", true, false, 3.0f, 2, {33, 33, 33, 33, 33, 33}
    };
    
    // Gold Ore
    blocks_[static_cast<size_t>(BlockType::Gold)] = {
        "Gold Ore", true, false, 3.0f, 3, {32, 32, 32, 32, 32, 32}
    };
    
    // Diamond Ore
    blocks_[static_cast<size_t>(BlockType::Diamond)] = {
        "Diamond Ore", true, false, 3.0f, 3, {50, 50, 50, 50, 50, 50}
    };

    // Light (LS-1 marker block). Rendered as a marker (not a textured cube).
    blocks_[static_cast<size_t>(BlockType::Light)] = {
        "Light", false, true, 0.0f, 0, {0, 0, 0, 0, 0, 0}
    };
}

const BlockInfo& BlockRegistry::get_block_info(BlockType type) const {
    return blocks_[static_cast<size_t>(type)];
}

Rectangle BlockRegistry::get_texture_rect(BlockType type, int face) const {
    const auto& info = get_block_info(type);
    int tile_index = info.texture_indices[face];
    
    int tile_x = tile_index % atlas_tiles_per_row_;
    int tile_y = tile_index / atlas_tiles_per_row_;
    
    return Rectangle{
        static_cast<float>(tile_x * atlas_tile_size_),
        static_cast<float>(tile_y * atlas_tile_size_),
        static_cast<float>(atlas_tile_size_),
        static_cast<float>(atlas_tile_size_)
    };
}

} // namespace voxel
