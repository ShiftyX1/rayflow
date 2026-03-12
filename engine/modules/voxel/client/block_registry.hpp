#pragma once

#include "block.hpp"
#include "engine/core/math_types.hpp"
#include "engine/renderer/gl_texture.hpp"
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
    static BlockRegistry& instance();
    
    bool init(const std::string& atlas_path);
    void destroy();
    
    const BlockInfo& get_block_info(BlockType type) const;
    rf::Rect get_texture_rect(BlockType type, int face) const;

    /// Returns the atlas GPU texture (for binding during voxel rendering).
    const rf::GLTexture& get_atlas_texture() const { return atlas_texture_; }

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
    rf::GLTexture atlas_texture_;
    int atlas_tile_size_{16};
    int atlas_tiles_per_row_{16};

    rf::GLTexture grass_colormap_;
    rf::GLTexture foliage_colormap_;

    static rf::Color sample_colormap_(const rf::GLTexture& tex, float temperature, float humidity, rf::Color fallback);
    bool initialized_{false};
};

} // namespace voxel
