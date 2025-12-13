#pragma once

#include "block.hpp"
#include <raylib.h>
#include <array>
#include <string>

namespace voxel {

// Block properties
struct BlockInfo {
    const char* name;
    bool is_solid;
    bool is_transparent;
    float hardness;
    int required_tool_level;
    
    // Texture atlas coordinates (tile indices for each face)
    // Order: +X, -X, +Y (top), -Y (bottom), +Z, -Z
    std::array<int, 6> texture_indices;
};

class BlockRegistry {
public:
    static BlockRegistry& instance();
    
    bool init(const std::string& atlas_path);
    void destroy();
    
    const BlockInfo& get_block_info(BlockType type) const;
    Rectangle get_texture_rect(BlockType type, int face) const;
    Texture2D get_atlas_texture() const { return atlas_texture_; }
    
    bool is_initialized() const { return initialized_; }
    
private:
    BlockRegistry() = default;
    ~BlockRegistry();
    
    BlockRegistry(const BlockRegistry&) = delete;
    BlockRegistry& operator=(const BlockRegistry&) = delete;
    
    void register_blocks();
    
    std::array<BlockInfo, static_cast<size_t>(BlockType::Count)> blocks_{};
    Texture2D atlas_texture_{};
    int atlas_tile_size_{16};
    int atlas_tiles_per_row_{16};
    bool initialized_{false};
};

} // namespace voxel
