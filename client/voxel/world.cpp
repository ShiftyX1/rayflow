#include "world.hpp"
#include <raylib.h>
#include <cmath>
#include <cstdio>
#include <random>

namespace voxel {

namespace {

constexpr int CHUNK_UNLOAD_DISTANCE = 12;

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

int floor_div_int(int a, int b) {
    if (b == 0) return 0;
    if (a >= 0) return a / b;
    return -(((-a) + b - 1) / b);
}

} // anonymous namespace

World::World(unsigned int seed) : seed_(seed) {
    init_perlin();
    TraceLog(LOG_INFO, "World created with seed: %u (infinite chunk generation enabled)", seed);
}

World::~World() {
    TraceLog(LOG_INFO, "World destroyed. Total chunks generated: %zu", chunks_.size());
}

void World::set_map_template(shared::maps::MapTemplate map) {
    map_template_ = std::move(map);
    chunks_.clear();
}

void World::clear_map_template() {
    map_template_.reset();
    chunks_.clear();
}

void World::mark_all_chunks_dirty() {
    for (auto& [key, chunk] : chunks_) {
        (void)key;
        chunk->mark_dirty();
    }
}

void World::init_perlin() const {
    if (perm_initialized_) return;

    // NOTE: Deterministic local PRNG; do not use std::rand/std::srand here.
    std::mt19937 rng(seed_);
    for (int i = 0; i < 256; i++) {
        perm_[i] = static_cast<unsigned char>(i);
    }
    
    // Shuffle
    for (int i = 255; i > 0; i--) {
        std::uniform_int_distribution<int> dist(0, i);
        const int j = dist(rng);
        std::swap(perm_[i], perm_[j]);
    }
    
    // Duplicate
    for (int i = 0; i < 256; i++) {
        perm_[256 + i] = perm_[i];
    }
    
    perm_initialized_ = true;
}

float World::perlin_noise(float x, float y) const {
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
        v
    );
}

float World::octave_perlin(float x, float y, int octaves, float persistence) const {
    float total = 0;
    float frequency = 1;
    float amplitude = 1;
    float max_value = 0;
    
    for (int i = 0; i < octaves; i++) {
        total += perlin_noise(x * frequency, y * frequency) * amplitude;
        max_value += amplitude;
        amplitude *= persistence;
        frequency *= 2;
    }
    
    return total / max_value;
}

Block World::get_block(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_HEIGHT) {
        return static_cast<Block>(BlockType::Air);
    }
    
    int chunk_x = (x >= 0) ? (x / CHUNK_WIDTH) : ((x + 1) / CHUNK_WIDTH - 1);
    int chunk_z = (z >= 0) ? (z / CHUNK_DEPTH) : ((z + 1) / CHUNK_DEPTH - 1);
    
    auto it = chunks_.find({chunk_x, chunk_z});
    if (it == chunks_.end()) {
        return static_cast<Block>(BlockType::Air);
    }
    
    int local_x = x - chunk_x * CHUNK_WIDTH;
    int local_z = z - chunk_z * CHUNK_DEPTH;
    
    return it->second->get_block(local_x, y, local_z);
}

void World::set_block(int x, int y, int z, Block type) {
    if (y < 0 || y >= CHUNK_HEIGHT) return;
    
    int chunk_x = (x >= 0) ? (x / CHUNK_WIDTH) : ((x + 1) / CHUNK_WIDTH - 1);
    int chunk_z = (z >= 0) ? (z / CHUNK_DEPTH) : ((z + 1) / CHUNK_DEPTH - 1);
    
    auto it = chunks_.find({chunk_x, chunk_z});
    if (it == chunks_.end()) return;
    
    int local_x = x - chunk_x * CHUNK_WIDTH;
    int local_z = z - chunk_z * CHUNK_DEPTH;
    
    it->second->set_block(local_x, y, local_z, type);

    // Client-only lighting cache: mark dirty so emissive/light-block changes update promptly.
    light_volume_dirty_ = true;
}

Chunk* World::get_chunk(int chunk_x, int chunk_z) {
    auto it = chunks_.find({chunk_x, chunk_z});
    return it != chunks_.end() ? it->second.get() : nullptr;
}

Chunk* World::get_or_create_chunk(int chunk_x, int chunk_z) {
    auto key = std::make_pair(chunk_x, chunk_z);
    auto it = chunks_.find(key);
    
    if (it != chunks_.end()) {
        return it->second.get();
    }
    
    auto chunk = std::make_unique<Chunk>(chunk_x, chunk_z);
    generate_chunk_terrain(*chunk);
    chunk->set_generated(true);
    
    auto* ptr = chunk.get();
    chunks_[key] = std::move(chunk);
    
    return ptr;
}

void World::generate_chunk_terrain(Chunk& chunk) {
    int chunk_x = chunk.get_chunk_x();
    int chunk_z = chunk.get_chunk_z();

    if (map_template_) {
        const auto& b = map_template_->bounds;
        if (chunk_x >= b.chunkMinX && chunk_x <= b.chunkMaxX && chunk_z >= b.chunkMinZ && chunk_z <= b.chunkMaxZ) {
            const auto* src = map_template_->find_chunk(chunk_x, chunk_z);

            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                for (int z = 0; z < CHUNK_DEPTH; z++) {
                    for (int x = 0; x < CHUNK_WIDTH; x++) {
                        Block bt = static_cast<Block>(BlockType::Air);
                        if (src) {
                            const std::size_t idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(CHUNK_WIDTH) * static_cast<std::size_t>(CHUNK_DEPTH) +
                                                    static_cast<std::size_t>(z) * static_cast<std::size_t>(CHUNK_WIDTH) +
                                                    static_cast<std::size_t>(x);
                            bt = static_cast<Block>(src->blocks[idx]);
                        }
                        chunk.set_block(x, y, z, bt);
                    }
                }
            }
            return;
        }

        // Outside template bounds: leave as Air (void).
        for (int y = 0; y < CHUNK_HEIGHT; y++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                for (int x = 0; x < CHUNK_WIDTH; x++) {
                    chunk.set_block(x, y, z, static_cast<Block>(BlockType::Air));
                }
            }
        }
        return;
    }
    
    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            float world_x = static_cast<float>(chunk_x * CHUNK_WIDTH + x);
            float world_z = static_cast<float>(chunk_z * CHUNK_DEPTH + z);
            
            // Generate height using Perlin noise
            float noise = octave_perlin(world_x * 0.02f, world_z * 0.02f, 4, 0.5f);
            int height = static_cast<int>(60 + noise * 20);
            
            // Fill terrain
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                Block block_type;
                
                if (y == 0) {
                    block_type = static_cast<Block>(BlockType::Bedrock);
                } else if (y < height - 4) {
                    block_type = static_cast<Block>(BlockType::Stone);
                } else if (y < height - 1) {
                    block_type = static_cast<Block>(BlockType::Dirt);
                } else if (y == height - 1) {
                    block_type = static_cast<Block>(BlockType::Grass);
                } else {
                    block_type = static_cast<Block>(BlockType::Air);
                }
                
                chunk.set_block(x, y, z, block_type);
            }
        }
    }
}

void World::update(const Vector3& player_position) {
    load_chunks_around_player(player_position);
    unload_distant_chunks(player_position);

    // Client-only lighting cache for rendering (Minecraft-style skylight + blocklight).
    if (light_volume_.update_if_needed(*this, player_position, light_volume_dirty_)) {
        light_volume_dirty_ = false;
    }
    
    // Update meshes for dirty chunks
    for (auto& [key, chunk] : chunks_) {
        if (chunk->needs_mesh_update()) {
            chunk->generate_mesh(*this);
        }
    }
    
    last_player_position_ = player_position;
}

void World::load_chunks_around_player(const Vector3& player_position) {
    int player_chunk_x = static_cast<int>(std::floor(player_position.x / CHUNK_WIDTH));
    int player_chunk_z = static_cast<int>(std::floor(player_position.z / CHUNK_DEPTH));
    
    for (int dx = -render_distance_; dx <= render_distance_; dx++) {
        for (int dz = -render_distance_; dz <= render_distance_; dz++) {
            if (dx * dx + dz * dz <= render_distance_ * render_distance_) {
                get_or_create_chunk(player_chunk_x + dx, player_chunk_z + dz);
            }
        }
    }
}

void World::unload_distant_chunks(const Vector3& player_position) {
    int player_chunk_x = static_cast<int>(std::floor(player_position.x / CHUNK_WIDTH));
    int player_chunk_z = static_cast<int>(std::floor(player_position.z / CHUNK_DEPTH));
    
    std::vector<std::pair<int, int>> to_remove;
    
    for (const auto& [key, chunk] : chunks_) {
        int dx = key.first - player_chunk_x;
        int dz = key.second - player_chunk_z;
        
        if (dx * dx + dz * dz > CHUNK_UNLOAD_DISTANCE * CHUNK_UNLOAD_DISTANCE) {
            to_remove.push_back(key);
        }
    }
    
    for (const auto& key : to_remove) {
        chunks_.erase(key);
    }
}

void World::render(const Camera3D& camera) const {
    for (const auto& [key, chunk] : chunks_) {
        // Simple frustum culling could be added here
        chunk->render();
    }
}

} // namespace voxel
