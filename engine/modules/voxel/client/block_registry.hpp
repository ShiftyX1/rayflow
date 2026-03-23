#pragma once

#include "engine/core/export.hpp"
#include "block.hpp"
#include "engine/core/math_types.hpp"
#include "engine/renderer/gpu/gpu_texture.hpp"
#include <array>
#include <memory>
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

class RAYFLOW_VOXEL_API BlockRegistry {
public:
    static BlockRegistry& instance();
    
    bool init(const std::string& atlas_path);
    void destroy();
    
    const BlockInfo& get_block_info(BlockType type) const;
    rf::Rect get_texture_rect(BlockType type, int face) const;

    /// Returns the atlas GPU texture (for binding during voxel rendering).
    const rf::ITexture& get_atlas_texture() const { return *atlas_texture_; }

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
    std::unique_ptr<rf::ITexture> atlas_texture_;
    int atlas_tile_size_{16};
    int atlas_tiles_per_row_{16};

    std::unique_ptr<rf::ITexture> grass_colormap_;
    std::unique_ptr<rf::ITexture> foliage_colormap_;

    static rf::Color sample_colormap_(const rf::ITexture& tex, float temperature, float humidity, rf::Color fallback);
    bool initialized_{false};
};

} // namespace voxel
