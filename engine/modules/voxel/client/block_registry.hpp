#pragma once

#include "block.hpp"
#include "engine/core/math_types.hpp"
#include <array>
#include <string>

namespace voxel {

struct BlockInfo {
    const char* name;
    bool is_solid;
    bool is_transparent;
    float hardness;
    int required_tool_level;
    
    std::array<int, 6> texture_indices;
};

class BlockRegistry {
public:
    // NOTE(migration): Texture2D/Image are raylib types. Phase 2 will replace.
    struct Tex2DPlaceholder { unsigned int id{0}; int width{0}; int height{0}; };
    struct ImagePlaceholder {};

    static BlockRegistry& instance();
    
    bool init(const std::string& atlas_path);
    void destroy();
    
    const BlockInfo& get_block_info(BlockType type) const;
    rf::Rect get_texture_rect(BlockType type, int face) const;
    Tex2DPlaceholder get_atlas_texture() const { return atlas_texture_; }

    rf::Color sample_grass_color(float temperature, float humidity) const;
    rf::Color sample_foliage_color(float temperature, float humidity) const;
    
    bool is_initialized() const { return initialized_; }
    
private:
    BlockRegistry() = default;
    ~BlockRegistry();
    
    BlockRegistry(const BlockRegistry&) = delete;
    BlockRegistry& operator=(const BlockRegistry&) = delete;
    
    void register_blocks();
    
    std::array<BlockInfo, static_cast<size_t>(BlockType::Count)> blocks_{};
    Tex2DPlaceholder atlas_texture_{};
    int atlas_tile_size_{16};
    int atlas_tiles_per_row_{16};

    ImagePlaceholder grass_colormap_{};
    ImagePlaceholder foliage_colormap_{};
    bool grass_colormap_loaded_{false};
    bool foliage_colormap_loaded_{false};

    static rf::Color sample_colormap_(const ImagePlaceholder& img, bool loaded, float temperature, float humidity, rf::Color fallback);
    bool initialized_{false};
};

} // namespace voxel
