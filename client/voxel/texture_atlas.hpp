#pragma once

#include <raylib.h>

namespace voxel {

class TextureAtlas {
public:
    bool load(const char* path);
    void unload();
    
    Texture2D get_texture() const { return texture_; }
    int get_tile_size() const { return tile_size_; }
    int get_tiles_per_row() const { return tiles_per_row_; }
    
    Rectangle get_tile_rect(int tile_index) const;
    
    // Get UV coordinates for a tile (normalized 0-1)
    void get_tile_uvs(int tile_index, float* u0, float* v0, float* u1, float* v1) const;
    
private:
    Texture2D texture_{};
    int tile_size_{16};
    int tiles_per_row_{16};
    bool loaded_{false};
};

} // namespace voxel
