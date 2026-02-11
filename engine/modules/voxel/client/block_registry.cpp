#include "block_registry.hpp"
#include "engine/client/core/resources.hpp"
#include "engine/core/logging.hpp"
#include "engine/core/math_types.hpp"
#include <cstdio>
#include <algorithm>
#include <cmath>

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
    
    // NOTE(migration): resources::load_texture returns TexturePlaceholder (id only);
    // copy id into Tex2DPlaceholder which also carries width/height.
    {
        auto tex = resources::load_texture(atlas_path);
        atlas_texture_.id = tex.id;
    }
    if (atlas_texture_.id == 0) {
        TraceLog(LOG_ERROR, "Failed to load texture atlas: %s", atlas_path.c_str());
        return false;
    }
    
    atlas_tile_size_ = 16;
    atlas_tiles_per_row_ = atlas_texture_.width / atlas_tile_size_;

    // NOTE(migration): resources::load_image returns resources::ImagePlaceholder (empty stub).
    // Colormap loading is stubbed for Phase 0 — colormaps will always use fallback.
    {
        auto img = resources::load_image("textures/grasscolor.png");
        (void)img;
    }
    grass_colormap_loaded_ = false;
    if (!grass_colormap_loaded_) {
        TraceLog(LOG_WARNING, "[voxel] grasscolor.png not found; grass recolor will use fallback");
    }

    {
        auto img = resources::load_image("textures/foliagecolor.png");
        (void)img;
    }
    foliage_colormap_loaded_ = false;
    if (!foliage_colormap_loaded_) {
        TraceLog(LOG_WARNING, "[voxel] foliagecolor.png not found; foliage recolor will use fallback");
    }
    
    register_blocks();
    
    initialized_ = true;
    TraceLog(LOG_INFO, "Block registry initialized with %d block types", static_cast<int>(BlockType::Count));
    
    return true;
}

void BlockRegistry::destroy() {
    if (initialized_) {
        // NOTE(migration): UnloadTexture/UnloadImage are raylib calls — stubbed for Phase 0
        // UnloadTexture(atlas_texture_);
        atlas_texture_ = {};

        if (grass_colormap_loaded_) {
            // UnloadImage(grass_colormap_);
            grass_colormap_ = {};
            grass_colormap_loaded_ = false;
        }
        if (foliage_colormap_loaded_) {
            // UnloadImage(foliage_colormap_);
            foliage_colormap_ = {};
            foliage_colormap_loaded_ = false;
        }

        initialized_ = false;
    }
}

rf::Color BlockRegistry::sample_colormap_(const ImagePlaceholder& img, bool loaded, float temperature, float humidity, rf::Color fallback) {
    // NOTE(migration): Image data access and GetImageColor are raylib calls — stubbed for Phase 0.
    // Always returns fallback until Phase 2 provides a real image sampling path.
    (void)img;
    (void)temperature;
    (void)humidity;
    if (!loaded) {
        return fallback;
    }
    return fallback;
}

rf::Color BlockRegistry::sample_grass_color(float temperature, float humidity) const {
    const rf::Color fallback{120, 200, 80, 255};
    return sample_colormap_(grass_colormap_, grass_colormap_loaded_, temperature, humidity, fallback);
}

rf::Color BlockRegistry::sample_foliage_color(float temperature, float humidity) const {
    const rf::Color fallback{90, 180, 70, 255};
    return sample_colormap_(foliage_colormap_, foliage_colormap_loaded_, temperature, humidity, fallback);
}

void BlockRegistry::register_blocks() {
    blocks_[static_cast<size_t>(BlockType::Air)] = {
        "Air", false, true, 0.0f, 0, {0, 0, 0, 0, 0, 0}
    };
    
    blocks_[static_cast<size_t>(BlockType::Stone)] = {
        "Stone", true, false, 1.5f, 1, {1, 1, 1, 1, 1, 1}
    };
    
    blocks_[static_cast<size_t>(BlockType::Dirt)] = {
        "Dirt", true, false, 0.5f, 0, {2, 2, 2, 2, 2, 2}
    };
    
    blocks_[static_cast<size_t>(BlockType::Grass)] = {
        "Grass", true, false, 0.6f, 0, {3, 3, 0, 2, 3, 3}  // top=grass, bottom=dirt, sides=grass_side
    };
    
    blocks_[static_cast<size_t>(BlockType::Sand)] = {
        "Sand", true, false, 0.5f, 0, {18, 18, 18, 18, 18, 18}
    };
    
    blocks_[static_cast<size_t>(BlockType::Water)] = {
        "Water", false, true, 100.0f, 0, {205, 205, 205, 205, 205, 205}
    };
    
    blocks_[static_cast<size_t>(BlockType::Wood)] = {
        "Wood", true, false, 2.0f, 0, {20, 20, 21, 21, 20, 20}  // sides=bark, top/bottom=rings
    };
    
    blocks_[static_cast<size_t>(BlockType::Leaves)] = {
        "Leaves", true, true, 0.2f, 0, {52, 52, 52, 52, 52, 52}
    };
    
    blocks_[static_cast<size_t>(BlockType::Bedrock)] = {
        "Bedrock", true, false, -1.0f, 255, {17, 17, 17, 17, 17, 17}
    };
    
    blocks_[static_cast<size_t>(BlockType::Gravel)] = {
        "Gravel", true, false, 0.6f, 0, {19, 19, 19, 19, 19, 19}
    };
    
    blocks_[static_cast<size_t>(BlockType::Coal)] = {
        "Coal Ore", true, false, 3.0f, 1, {34, 34, 34, 34, 34, 34}
    };
    
    blocks_[static_cast<size_t>(BlockType::Iron)] = {
        "Iron Ore", true, false, 3.0f, 2, {33, 33, 33, 33, 33, 33}
    };
    
    blocks_[static_cast<size_t>(BlockType::Gold)] = {
        "Gold Ore", true, false, 3.0f, 3, {32, 32, 32, 32, 32, 32}
    };
    
    blocks_[static_cast<size_t>(BlockType::Diamond)] = {
        "Diamond Ore", true, false, 3.0f, 3, {50, 50, 50, 50, 50, 50}
    };

    blocks_[static_cast<size_t>(BlockType::Light)] = {
        "Light", false, true, 0.0f, 0, {0, 0, 0, 0, 0, 0}
    };
    
    blocks_[static_cast<size_t>(BlockType::StoneSlab)] = {
        "Stone Slab", true, false, 1.5f, 1, {1, 1, 1, 1, 1, 1}
    };
    
    blocks_[static_cast<size_t>(BlockType::StoneSlabTop)] = {
        "Stone Slab Top", true, false, 1.5f, 1, {1, 1, 1, 1, 1, 1}
    };
    
    blocks_[static_cast<size_t>(BlockType::WoodSlab)] = {
        "Wood Slab", true, false, 2.0f, 0, {4, 4, 4, 4, 4, 4}
    };
    
    blocks_[static_cast<size_t>(BlockType::WoodSlabTop)] = {
        "Wood Slab Top", true, false, 2.0f, 0, {4, 4, 4, 4, 4, 4}
    };
    
    blocks_[static_cast<size_t>(BlockType::OakFence)] = {
        "Oak Fence", true, false, 2.0f, 0, {4, 4, 4, 4, 4, 4}
    };
    
    // Vegetation blocks (cross-shaped, transparent, no collision)
    // Using same texture index for all faces since cross models use special rendering
    // Texture indices: tallgrass=39, flower_rose=12, flower_dandelion=13, flower_blue_orchid=175 (need to check atlas)
    blocks_[static_cast<size_t>(BlockType::TallGrass)] = {
        "Tall Grass", false, true, 0.0f, 0, {39, 39, 39, 39, 39, 39}
    };
    
    blocks_[static_cast<size_t>(BlockType::Poppy)] = {
        "Poppy", false, true, 0.0f, 0, {12, 12, 12, 12, 12, 12}
    };
    
    blocks_[static_cast<size_t>(BlockType::Dandelion)] = {
        "Dandelion", false, true, 0.0f, 0, {13, 13, 13, 13, 13, 13}
    };
    
    blocks_[static_cast<size_t>(BlockType::DeadBush)] = {
        "Dead Bush", false, true, 0.0f, 0, {55, 55, 55, 55, 55, 55}
    };
}

const BlockInfo& BlockRegistry::get_block_info(BlockType type) const {
    return blocks_[static_cast<size_t>(type)];
}

rf::Rect BlockRegistry::get_texture_rect(BlockType type, int face) const {
    const auto& info = get_block_info(type);
    int tile_index = info.texture_indices[face];
    
    int tile_x = tile_index % atlas_tiles_per_row_;
    int tile_y = tile_index / atlas_tiles_per_row_;
    
    return rf::Rect{
        static_cast<float>(tile_x * atlas_tile_size_),
        static_cast<float>(tile_y * atlas_tile_size_),
        static_cast<float>(atlas_tile_size_),
        static_cast<float>(atlas_tile_size_)
    };
}

} // namespace voxel
