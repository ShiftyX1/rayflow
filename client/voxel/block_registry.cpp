#include "block_registry.hpp"
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
    
    atlas_texture_ = LoadTexture(atlas_path.c_str());
    if (atlas_texture_.id == 0) {
        TraceLog(LOG_ERROR, "Failed to load texture atlas: %s", atlas_path.c_str());
        return false;
    }
    
    atlas_tile_size_ = 16;
    atlas_tiles_per_row_ = atlas_texture_.width / atlas_tile_size_;

    // MV-2: optional colormaps for foliage/grass recolor.
    grass_colormap_ = LoadImage("textures/grasscolor.png");
    grass_colormap_loaded_ = (grass_colormap_.data != nullptr);
    if (!grass_colormap_loaded_) {
        TraceLog(LOG_WARNING, "[voxel] grasscolor.png not found; grass recolor will use fallback");
    }

    foliage_colormap_ = LoadImage("textures/foliagecolor.png");
    foliage_colormap_loaded_ = (foliage_colormap_.data != nullptr);
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
        UnloadTexture(atlas_texture_);

        if (grass_colormap_loaded_) {
            UnloadImage(grass_colormap_);
            grass_colormap_loaded_ = false;
        }
        if (foliage_colormap_loaded_) {
            UnloadImage(foliage_colormap_);
            foliage_colormap_loaded_ = false;
        }

        initialized_ = false;
    }
}

Color BlockRegistry::sample_colormap_(const Image& img, bool loaded, float temperature, float humidity, Color fallback) {
    if (!loaded || img.data == nullptr || img.width <= 0 || img.height <= 0) {
        return fallback;
    }

    const float t = std::clamp(temperature, 0.0f, 1.0f);
    const float h = std::clamp(humidity, 0.0f, 1.0f);
    const float adjusted_humidity = h * t;  // MC formula: keeps us in the triangle

    const int w = img.width;
    const int hh = img.height;

    const int x = std::clamp(static_cast<int>(std::lround((1.0f - t) * static_cast<float>(w - 1))), 0, w - 1);
    const int y = std::clamp(static_cast<int>(std::lround((1.0f - adjusted_humidity) * static_cast<float>(hh - 1))), 0, hh - 1);

    return GetImageColor(img, x, y);
}

Color BlockRegistry::sample_grass_color(float temperature, float humidity) const {
    // Fallback tuned to be obviously green if the colormap is missing.
    const Color fallback{120, 200, 80, 255};
    return sample_colormap_(grass_colormap_, grass_colormap_loaded_, temperature, humidity, fallback);
}

Color BlockRegistry::sample_foliage_color(float temperature, float humidity) const {
    const Color fallback{90, 180, 70, 255};
    return sample_colormap_(foliage_colormap_, foliage_colormap_loaded_, temperature, humidity, fallback);
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
