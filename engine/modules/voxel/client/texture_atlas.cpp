#include "texture_atlas.hpp"
#include "engine/client/core/resources.hpp"

namespace voxel {

bool TextureAtlas::load(const char* path) {
    if (loaded_) {
        unload();
    }
    
    texture_ = resources::load_texture(path);
    if (texture_.id == 0) {
        return false;
    }
    
    tile_size_ = 16;
    tiles_per_row_ = texture_.width / tile_size_;
    loaded_ = true;
    
    return true;
}

void TextureAtlas::unload() {
    if (loaded_) {
        UnloadTexture(texture_);
        loaded_ = false;
    }
}

Rectangle TextureAtlas::get_tile_rect(int tile_index) const {
    int x = tile_index % tiles_per_row_;
    int y = tile_index / tiles_per_row_;
    
    return Rectangle{
        static_cast<float>(x * tile_size_),
        static_cast<float>(y * tile_size_),
        static_cast<float>(tile_size_),
        static_cast<float>(tile_size_)
    };
}

void TextureAtlas::get_tile_uvs(int tile_index, float* u0, float* v0, float* u1, float* v1) const {
    int x = tile_index % tiles_per_row_;
    int y = tile_index / tiles_per_row_;
    
    float tex_width = static_cast<float>(texture_.width);
    float tex_height = static_cast<float>(texture_.height);
    
    *u0 = (x * tile_size_) / tex_width;
    *v0 = (y * tile_size_) / tex_height;
    *u1 = ((x + 1) * tile_size_) / tex_width;
    *v1 = ((y + 1) * tile_size_) / tex_height;
}

} // namespace voxel
