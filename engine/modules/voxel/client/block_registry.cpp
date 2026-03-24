#include "block_registry.hpp"
#include "engine/client/core/resources.hpp"
#include "engine/core/logging.hpp"
#include "engine/core/math_types.hpp"
#include <algorithm>
#include <cmath>
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
    
    atlas_texture_ = resources::load_texture(atlas_path);
    if (!atlas_texture_ || !atlas_texture_->isValid()) {
        TraceLog(LOG_ERROR, "Failed to load texture atlas: %s", atlas_path.c_str());
        return false;
    }
    
    atlas_tile_size_ = 16;
    atlas_tiles_per_row_ = atlas_texture_->width() / atlas_tile_size_;

    // Load colormaps with CPU pixel retention for biome tinting
    grass_colormap_ = resources::load_image("textures/grasscolor.png");
    if (!grass_colormap_ || !grass_colormap_->isValid()) {
        TraceLog(LOG_WARNING, "[voxel] grasscolor.png not found; grass recolor will use fallback");
    }

    foliage_colormap_ = resources::load_image("textures/foliagecolor.png");
    if (!foliage_colormap_ || !foliage_colormap_->isValid()) {
        TraceLog(LOG_WARNING, "[voxel] foliagecolor.png not found; foliage recolor will use fallback");
    }
    
    initialized_ = true;
    TraceLog(LOG_INFO, "Block registry initialized (game must call register_block() to populate)");
    
    return true;
}

void BlockRegistry::destroy() {
    if (initialized_) {
        atlas_texture_.reset();
        grass_colormap_.reset();
        foliage_colormap_.reset();
        initialized_ = false;
    }
}

rf::Color BlockRegistry::sample_colormap_(const rf::ITexture& tex, float temperature, float humidity, rf::Color fallback) {
    if (!tex.isValid()) {
        return fallback;
    }

    // Minecraft-style colormap lookup:
    // x = temperature * (width - 1), y = humidity * (height - 1)
    temperature = std::clamp(temperature, 0.0f, 1.0f);
    humidity = std::clamp(humidity, 0.0f, 1.0f);
    // Clamp humidity to not exceed temperature (triangular region)
    humidity *= temperature;

    int x = static_cast<int>((1.0f - temperature) * (tex.width() - 1));
    int y = static_cast<int>((1.0f - humidity) * (tex.height() - 1));

    rf::Color c = tex.samplePixel(x, y);
    // If samplePixel returned blank (no pixel data), use fallback
    if (c.a == 0 && c.r == 0 && c.g == 0 && c.b == 0) {
        return fallback;
    }
    return c;
}

rf::Color BlockRegistry::sample_grass_color(float temperature, float humidity) const {
    const rf::Color fallback{120, 200, 80, 255};
    if (!grass_colormap_) return fallback;
    return sample_colormap_(*grass_colormap_, temperature, humidity, fallback);
}

rf::Color BlockRegistry::sample_foliage_color(float temperature, float humidity) const {
    const rf::Color fallback{90, 180, 70, 255};
    if (!foliage_colormap_) return fallback;
    return sample_colormap_(*foliage_colormap_, temperature, humidity, fallback);
}

void BlockRegistry::register_block(BlockType type, const BlockInfo& info) {
    blocks_[static_cast<size_t>(type)] = info;
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
