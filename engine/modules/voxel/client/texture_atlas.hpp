#pragma once

#include "engine/core/math_types.hpp"

namespace voxel {

class TextureAtlas {
public:
    bool load(const char* path);
    void unload();
    
    // NOTE(migration): Texture2D placeholder. Phase 2 will use GLTexture.
    struct Tex2DPlaceholder { unsigned int id{0}; int width{0}; int height{0}; };
    Tex2DPlaceholder get_texture() const { return texture_; }
    int get_tile_size() const { return tile_size_; }
    int get_tiles_per_row() const { return tiles_per_row_; }
    
    rf::Rect get_tile_rect(int tile_index) const;
    
    void get_tile_uvs(int tile_index, float* u0, float* v0, float* u1, float* v1) const;
    
private:
    Tex2DPlaceholder texture_{};
    int tile_size_{16};
    int tiles_per_row_{16};
    bool loaded_{false};
};

} // namespace voxel
