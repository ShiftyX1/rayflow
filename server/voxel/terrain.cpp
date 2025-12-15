#include "terrain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace server::voxel {

namespace {

float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

float grad(int hash, float x, float y) {
    int h = hash & 7;
    float u = h < 4 ? x : y;
    float v = h < 4 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

} // namespace

Terrain::Terrain(std::uint32_t seed)
    : seed_(seed) {
    init_perlin_();
}

int Terrain::floor_div_(int a, int b) {
    // Floors towards -inf for negative values.
    if (b == 0) return 0;
    if (a >= 0) return a / b;
    return -(((-a) + b - 1) / b);
}

void Terrain::set_map_template(shared::maps::MapTemplate map) {
    map_template_ = std::move(map);
    // Keep runtime overrides, but drop any that are now redundant.
    if (!overrides_.empty()) {
        for (auto it = overrides_.begin(); it != overrides_.end();) {
            const auto base = get_template_block_(it->first.x, it->first.y, it->first.z);
            if (it->second == base) {
                it = overrides_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

bool Terrain::is_within_template_bounds(int x, int y, int z) const {
    (void)y;
    if (!map_template_) return false;
    const auto& b = map_template_->bounds;
    const int cx = floor_div_(x, shared::voxel::CHUNK_WIDTH);
    const int cz = floor_div_(z, shared::voxel::CHUNK_DEPTH);
    return (cx >= b.chunkMinX && cx <= b.chunkMaxX && cz >= b.chunkMinZ && cz <= b.chunkMaxZ);
}

shared::voxel::BlockType Terrain::get_template_block_(int x, int y, int z) const {
    using shared::voxel::BlockType;

    if (!map_template_) return BlockType::Air;
    if (y < 0 || y >= shared::voxel::CHUNK_HEIGHT) return BlockType::Air;

    const auto& b = map_template_->bounds;
    const int cx = floor_div_(x, shared::voxel::CHUNK_WIDTH);
    const int cz = floor_div_(z, shared::voxel::CHUNK_DEPTH);
    if (cx < b.chunkMinX || cx > b.chunkMaxX || cz < b.chunkMinZ || cz > b.chunkMaxZ) {
        return BlockType::Air;
    }

    const auto* chunk = map_template_->find_chunk(cx, cz);
    if (!chunk) {
        return BlockType::Air;
    }

    const int lx = x - cx * shared::voxel::CHUNK_WIDTH;
    const int lz = z - cz * shared::voxel::CHUNK_DEPTH;
    if (lx < 0 || lx >= shared::voxel::CHUNK_WIDTH || lz < 0 || lz >= shared::voxel::CHUNK_DEPTH) {
        return BlockType::Air;
    }

    const std::size_t idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(shared::voxel::CHUNK_WIDTH) * static_cast<std::size_t>(shared::voxel::CHUNK_DEPTH) +
                            static_cast<std::size_t>(lz) * static_cast<std::size_t>(shared::voxel::CHUNK_WIDTH) +
                            static_cast<std::size_t>(lx);
    return chunk->blocks[idx];
}

shared::voxel::BlockType Terrain::get_base_block_(int x, int y, int z) const {
    using shared::voxel::BlockType;

    if (y < 0 || y >= shared::voxel::CHUNK_HEIGHT) {
        return BlockType::Air;
    }

    const float world_x = static_cast<float>(x);
    const float world_z = static_cast<float>(z);

    const float noise = octave_perlin_(world_x * 0.02f, world_z * 0.02f, 4, 0.5f);
    const int height = static_cast<int>(60 + noise * 20);

    if (y == 0) {
        return BlockType::Bedrock;
    }

    if (y < height - 4) {
        return BlockType::Stone;
    }

    if (y < height - 1) {
        return BlockType::Dirt;
    }

    if (y == height - 1) {
        return BlockType::Grass;
    }

    return BlockType::Air;
}

void Terrain::init_perlin_() const {
    if (perm_initialized_) return;

    // Keep algorithm consistent with the current client implementation.
    std::srand(seed_);
    for (int i = 0; i < 256; i++) {
        perm_[i] = static_cast<unsigned char>(i);
    }

    for (int i = 255; i > 0; i--) {
        int j = std::rand() % (i + 1);
        std::swap(perm_[i], perm_[j]);
    }

    for (int i = 0; i < 256; i++) {
        perm_[256 + i] = perm_[i];
    }

    perm_initialized_ = true;
}

float Terrain::perlin_noise_(float x, float y) const {
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;

    x -= std::floor(x);
    y -= std::floor(y);

    float u = fade(x);
    float v = fade(y);

    int a = perm_[X] + Y;
    int b = perm_[X + 1] + Y;

    return lerp(
        lerp(grad(perm_[a], x, y), grad(perm_[b], x - 1, y), u),
        lerp(grad(perm_[a + 1], x, y - 1), grad(perm_[b + 1], x - 1, y - 1), u),
        v);
}

float Terrain::octave_perlin_(float x, float y, int octaves, float persistence) const {
    float total = 0;
    float frequency = 1;
    float amplitude = 1;
    float max_value = 0;

    for (int i = 0; i < octaves; i++) {
        total += perlin_noise_(x * frequency, y * frequency) * amplitude;
        max_value += amplitude;
        amplitude *= persistence;
        frequency *= 2;
    }

    return total / max_value;
}

shared::voxel::BlockType Terrain::get_block(int x, int y, int z) const {
    if (!overrides_.empty()) {
        const BlockKey key{x, y, z};
        const auto it = overrides_.find(key);
        if (it != overrides_.end()) {
            return it->second;
        }
    }

    if (map_template_) {
        return get_template_block_(x, y, z);
    }

    return get_base_block_(x, y, z);
}

void Terrain::set_block(int x, int y, int z, shared::voxel::BlockType type) {
    // Ignore out-of-range edits.
    if (y < 0 || y >= shared::voxel::CHUNK_HEIGHT) {
        return;
    }

    const BlockKey key{x, y, z};
    const auto base = map_template_ ? get_template_block_(x, y, z) : get_base_block_(x, y, z);

    // If the requested type matches the base terrain, we can drop the override.
    if (type == base) {
        if (!overrides_.empty()) {
            overrides_.erase(key);
        }
        return;
    }

    overrides_[key] = type;
}

} // namespace server::voxel
