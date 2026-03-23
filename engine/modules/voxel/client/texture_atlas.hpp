#pragma once

#include "engine/core/export.hpp"
#include "engine/core/math_types.hpp"
#include "engine/renderer/gpu/gpu_texture.hpp"
#include <memory>

namespace voxel {

class RAYFLOW_VOXEL_API TextureAtlas {
public:
    bool load(const char* path);
    void unload();
    
    const rf::ITexture& get_texture() const { return *texture_; }
    bool has_texture() const { return texture_ && texture_->isValid(); }
    int get_tile_size() const { return tile_size_; }
    int get_tiles_per_row() const { return tiles_per_row_; }
    
    rf::Rect get_tile_rect(int tile_index) const;
    
    void get_tile_uvs(int tile_index, float* u0, float* v0, float* u1, float* v1) const;
    
private:
    std::unique_ptr<rf::ITexture> texture_;
    int tile_size_{16};
    int tiles_per_row_{16};
    bool loaded_{false};
};

} // namespace voxel
