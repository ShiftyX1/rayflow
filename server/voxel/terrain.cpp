#include "terrain.hpp"

#include <algorithm>
#include <cmath>
#include <random>

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
            if (it->second == base && player_placed_.find(it->first) == player_placed_.end()) {
                it = overrides_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void Terrain::set_override_(int x, int y, int z, shared::voxel::BlockType type, bool keep_if_matches_base) {
    // Ignore out-of-range edits.
    if (y < 0 || y >= shared::voxel::CHUNK_HEIGHT) {
        return;
    }

    const BlockKey key{x, y, z};
    const auto base = map_template_ ? get_template_block_(x, y, z) : get_base_block_(x, y, z);

    if (!keep_if_matches_base && type == base) {
        if (!overrides_.empty()) {
            overrides_.erase(key);
        }
        return;
    }

    overrides_[key] = type;
}

void Terrain::place_player_block(int x, int y, int z, shared::voxel::BlockType type) {
    if (y < 0 || y >= shared::voxel::CHUNK_HEIGHT) {
        return;
    }

    const BlockKey key{x, y, z};
    player_placed_.insert(key);

    // Keep the override even if it matches the template/base block type.
    // This allows a player to rebuild a template block and still have it be breakable.
    set_override_(x, y, z, type, /*keep_if_matches_base=*/true);
}

void Terrain::break_player_block(int x, int y, int z) {
    if (y < 0 || y >= shared::voxel::CHUNK_HEIGHT) {
        return;
    }

    const BlockKey key{x, y, z};
    player_placed_.erase(key);

    // Breaking results in Air. If base/template is non-air, keep the override (represents broken template).
    set_override_(x, y, z, shared::voxel::BlockType::Air, /*keep_if_matches_base=*/false);
}

bool Terrain::is_player_placed(int x, int y, int z) const {
    const BlockKey key{x, y, z};
    return player_placed_.find(key) != player_placed_.end();
}

bool Terrain::can_player_break(int x, int y, int z, shared::voxel::BlockType current) const {
    using shared::voxel::BlockType;

    if (current == BlockType::Air) return false;
    if (current == BlockType::Bedrock) return false;

    // Procedural (no template): keep legacy behavior (everything except bedrock is breakable).
    if (!map_template_) {
        return true;
    }

    if (is_player_placed(x, y, z)) {
        return true;
    }

    // Template protection: blocks that exist in the template are protected by default.
    const auto templ = get_template_block_(x, y, z);
    if (templ != BlockType::Air) {
        const std::size_t idx = static_cast<std::size_t>(templ);
        if (idx < map_template_->breakableTemplateBlocks.size() && map_template_->breakableTemplateBlocks[idx]) {
            return true;
        }
        return false;
    }

    // Non-template blocks inside a templated match are only breakable if they were player-placed.
    return false;
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

    if (void_base_) {
        (void)x;
        (void)y;
        (void)z;
        return BlockType::Air;
    }

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

    // NOTE: Deterministic local PRNG; do not use std::rand/std::srand in simulation.
    // Keep algorithm consistent with the client implementation.
    std::mt19937 rng(seed_);
    for (int i = 0; i < 256; i++) {
        perm_[i] = static_cast<unsigned char>(i);
    }

    for (int i = 255; i > 0; i--) {
        std::uniform_int_distribution<int> dist(0, i);
        const int j = dist(rng);
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
    // Editor/system edits do not mark blocks as player-placed.
    // They also preserve the previous behavior of dropping redundant overrides.
    set_override_(x, y, z, type, /*keep_if_matches_base=*/false);
}

std::vector<Terrain::BlockModification> Terrain::get_all_modifications() const {
    std::vector<BlockModification> result;
    result.reserve(overrides_.size());
    for (const auto& [key, type] : overrides_) {
        auto stateIt = block_states_.find(key);
        shared::voxel::BlockRuntimeState state = (stateIt != block_states_.end()) 
            ? stateIt->second 
            : shared::voxel::BlockRuntimeState::defaults();
        result.push_back({key.x, key.y, key.z, type, state});
    }
    return result;
}

// ============================================================================
// BlockRuntimeState Management
// ============================================================================

shared::voxel::BlockRuntimeState Terrain::get_block_state(int x, int y, int z) const {
    const BlockKey key{x, y, z};
    auto it = block_states_.find(key);
    if (it != block_states_.end()) {
        return it->second;
    }
    // Return default state based on block type
    auto type = get_block(x, y, z);
    if (shared::voxel::is_slab(type)) {
        shared::voxel::BlockRuntimeState state{};
        state.slabType = shared::voxel::get_default_slab_type(type);
        return state;
    }
    return shared::voxel::BlockRuntimeState::defaults();
}

void Terrain::set_block_state(int x, int y, int z, shared::voxel::BlockRuntimeState state) {
    const BlockKey key{x, y, z};
    if (state == shared::voxel::BlockRuntimeState::defaults()) {
        block_states_.erase(key);
    } else {
        block_states_[key] = state;
    }
}

shared::voxel::BlockRuntimeState Terrain::compute_block_state(int x, int y, int z, 
                                                        shared::voxel::BlockType type) const {
    using namespace shared::voxel;
    
    BlockRuntimeState state{};
    
    // Handle slabs
    if (is_slab(type)) {
        state.slabType = get_default_slab_type(type);
        return state;
    }
    
    // Handle fences/walls - check neighbors for connections
    if (uses_connections(type)) {
        // North (-Z)
        auto northType = get_block(x, y, z - 1);
        state.north = can_fence_connect_to(northType);
        
        // South (+Z)
        auto southType = get_block(x, y, z + 1);
        state.south = can_fence_connect_to(southType);
        
        // East (+X)
        auto eastType = get_block(x + 1, y, z);
        state.east = can_fence_connect_to(eastType);
        
        // West (-X)
        auto westType = get_block(x - 1, y, z);
        state.west = can_fence_connect_to(westType);
        
        return state;
    }
    
    return state;
}

std::vector<Terrain::BlockModification> Terrain::update_neighbor_states(int x, int y, int z) {
    using namespace shared::voxel;
    
    std::vector<BlockModification> updates;
    
    // Check each horizontal neighbor
    constexpr std::array<std::tuple<int, int>, 4> neighbors = {{
        {0, -1},  // North
        {0, +1},  // South
        {+1, 0},  // East
        {-1, 0}   // West
    }};
    
    for (const auto& [dx, dz] : neighbors) {
        int nx = x + dx;
        int nz = z + dz;
        
        auto neighborType = get_block(nx, y, nz);
        
        // Only update blocks that use connections
        if (!uses_connections(neighborType)) continue;
        
        // Compute new state for neighbor
        auto oldState = get_block_state(nx, y, nz);
        auto newState = compute_block_state(nx, y, nz, neighborType);
        
        if (oldState != newState) {
            set_block_state(nx, y, nz, newState);
            updates.push_back({nx, y, nz, neighborType, newState});
        }
    }
    
    return updates;
}

std::vector<std::uint8_t> Terrain::get_chunk_data(int chunkX, int chunkZ) const {
    constexpr int WIDTH = shared::voxel::CHUNK_WIDTH;
    constexpr int DEPTH = shared::voxel::CHUNK_DEPTH;
    constexpr int HEIGHT = shared::voxel::CHUNK_HEIGHT;
    constexpr std::size_t CHUNK_SIZE = static_cast<std::size_t>(WIDTH) * 
                                       static_cast<std::size_t>(DEPTH) * 
                                       static_cast<std::size_t>(HEIGHT);

    std::vector<std::uint8_t> blocks(CHUNK_SIZE);

    const int worldBaseX = chunkX * WIDTH;
    const int worldBaseZ = chunkZ * DEPTH;

    // Fill chunk with block data
    // Index order: y * (WIDTH * DEPTH) + z * WIDTH + x (local coords)
    for (int y = 0; y < HEIGHT; ++y) {
        for (int lz = 0; lz < DEPTH; ++lz) {
            for (int lx = 0; lx < WIDTH; ++lx) {
                const int worldX = worldBaseX + lx;
                const int worldZ = worldBaseZ + lz;
                
                // get_block handles overrides, template, and base terrain
                const auto blockType = get_block(worldX, y, worldZ);
                
                const std::size_t idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(WIDTH * DEPTH) +
                                        static_cast<std::size_t>(lz) * static_cast<std::size_t>(WIDTH) +
                                        static_cast<std::size_t>(lx);
                blocks[idx] = static_cast<std::uint8_t>(blockType);
            }
        }
    }

    return blocks;
}

} // namespace server::voxel
