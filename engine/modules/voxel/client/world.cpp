#include "world.hpp"
#include "engine/client/core/resources.hpp"
#include <raylib.h>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <random>
#include <algorithm>

#include "block.hpp"

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
    load_voxel_shader();
    TraceLog(LOG_INFO, "World created with seed: %u (infinite chunk generation enabled)", seed);
}

World::~World() {
    unload_voxel_shader();
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

float World::temperature() const {
    if (temperature_override_.has_value()) {
        return std::clamp(*temperature_override_, 0.0f, 1.0f);
    }
    if (map_template_.has_value()) {
        return std::clamp(map_template_->visualSettings.temperature, 0.0f, 1.0f);
    }
    return 0.5f;
}

void World::set_temperature_override(float temperature) {
    temperature_override_ = std::clamp(temperature, 0.0f, 1.0f);
}

void World::clear_temperature_override() {
    temperature_override_.reset();
}

float World::humidity() const {
    if (humidity_override_.has_value()) {
        return std::clamp(*humidity_override_, 0.0f, 1.0f);
    }
    if (map_template_.has_value()) {
        return std::clamp(map_template_->visualSettings.humidity, 0.0f, 1.0f);
    }
    return 1.0f;
}

void World::set_humidity_override(float humidity) {
    humidity_override_ = std::clamp(humidity, 0.0f, 1.0f);
}

void World::clear_humidity_override() {
    humidity_override_.reset();
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
    
    // Get or create chunk to handle blocks set before chunks are loaded
    Chunk* chunk = get_or_create_chunk(chunk_x, chunk_z);
    if (!chunk) return;
    
    int local_x = x - chunk_x * CHUNK_WIDTH;
    int local_z = z - chunk_z * CHUNK_DEPTH;

    const Block old_type = chunk->get_block(local_x, y, local_z);
    chunk->set_block(local_x, y, local_z, type);

    // If we edited a block on a chunk edge, the adjacent chunk's mesh must be rebuilt
    // too, otherwise the newly-exposed (or newly-hidden) neighbor face can be missing.
    auto mark_chunk_dirty = [this](int cx, int cz) {
        auto it2 = chunks_.find({cx, cz});
        if (it2 != chunks_.end()) {
            it2->second->mark_dirty();
        }
    };

    if (local_x == 0) {
        mark_chunk_dirty(chunk_x - 1, chunk_z);
    } else if (local_x == CHUNK_WIDTH - 1) {
        mark_chunk_dirty(chunk_x + 1, chunk_z);
    }

    if (local_z == 0) {
        mark_chunk_dirty(chunk_x, chunk_z - 1);
    } else if (local_z == CHUNK_DEPTH - 1) {
        mark_chunk_dirty(chunk_x, chunk_z + 1);
    }
}

shared::voxel::BlockRuntimeState World::get_block_state(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_HEIGHT) {
        return shared::voxel::BlockRuntimeState::defaults();
    }
    
    int chunk_x = (x >= 0) ? (x / CHUNK_WIDTH) : ((x + 1) / CHUNK_WIDTH - 1);
    int chunk_z = (z >= 0) ? (z / CHUNK_DEPTH) : ((z + 1) / CHUNK_DEPTH - 1);
    
    auto it = chunks_.find({chunk_x, chunk_z});
    if (it == chunks_.end()) {
        return shared::voxel::BlockRuntimeState::defaults();
    }
    
    int local_x = x - chunk_x * CHUNK_WIDTH;
    int local_z = z - chunk_z * CHUNK_DEPTH;
    
    return it->second->get_block_state(local_x, y, local_z);
}

void World::set_block_state(int x, int y, int z, shared::voxel::BlockRuntimeState state) {
    if (y < 0 || y >= CHUNK_HEIGHT) return;
    
    int chunk_x = (x >= 0) ? (x / CHUNK_WIDTH) : ((x + 1) / CHUNK_WIDTH - 1);
    int chunk_z = (z >= 0) ? (z / CHUNK_DEPTH) : ((z + 1) / CHUNK_DEPTH - 1);
    
    auto it = chunks_.find({chunk_x, chunk_z});
    if (it == chunks_.end()) return;
    
    int local_x = x - chunk_x * CHUNK_WIDTH;
    int local_z = z - chunk_z * CHUNK_DEPTH;
    
    it->second->set_block_state(local_x, y, local_z, state);
}

void World::set_block_with_state(int x, int y, int z, Block type, shared::voxel::BlockRuntimeState state) {
    if (y < 0 || y >= CHUNK_HEIGHT) return;
    
    int chunk_x = (x >= 0) ? (x / CHUNK_WIDTH) : ((x + 1) / CHUNK_WIDTH - 1);
    int chunk_z = (z >= 0) ? (z / CHUNK_DEPTH) : ((z + 1) / CHUNK_DEPTH - 1);
    
    auto it = chunks_.find({chunk_x, chunk_z});
    if (it == chunks_.end()) return;
    
    int local_x = x - chunk_x * CHUNK_WIDTH;
    int local_z = z - chunk_z * CHUNK_DEPTH;
    
    it->second->set_block_with_state(local_x, y, local_z, type, state);

    // If we edited a block on a chunk edge, the adjacent chunk's mesh must be rebuilt
    auto mark_chunk_dirty = [this](int cx, int cz) {
        auto it2 = chunks_.find({cx, cz});
        if (it2 != chunks_.end()) {
            it2->second->mark_dirty();
        }
    };

    if (local_x == 0) {
        mark_chunk_dirty(chunk_x - 1, chunk_z);
    } else if (local_x == CHUNK_WIDTH - 1) {
        mark_chunk_dirty(chunk_x + 1, chunk_z);
    }

    if (local_z == 0) {
        mark_chunk_dirty(chunk_x, chunk_z - 1);
    } else if (local_z == CHUNK_DEPTH - 1) {
        mark_chunk_dirty(chunk_x, chunk_z + 1);
    }
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
    
    recompute_chunk_states(chunk_x, chunk_z);
    
    return ptr;
}

void World::apply_chunk_data(int chunkX, int chunkZ, const std::vector<std::uint8_t>& blockData) {
    constexpr std::size_t EXPECTED_SIZE = static_cast<std::size_t>(CHUNK_WIDTH) *
                                          static_cast<std::size_t>(CHUNK_DEPTH) *
                                          static_cast<std::size_t>(CHUNK_HEIGHT);
    
    if (blockData.size() != EXPECTED_SIZE) {
        TraceLog(LOG_WARNING, "apply_chunk_data: invalid size %zu (expected %zu) for chunk (%d, %d)",
                 blockData.size(), EXPECTED_SIZE, chunkX, chunkZ);
        return;
    }
    
    auto key = std::make_pair(chunkX, chunkZ);
    auto it = chunks_.find(key);
    
    Chunk* chunk = nullptr;
    if (it != chunks_.end()) {
        chunk = it->second.get();
    } else {
        auto newChunk = std::make_unique<Chunk>(chunkX, chunkZ);
        chunk = newChunk.get();
        chunks_[key] = std::move(newChunk);
    }
    
    for (int y = 0; y < CHUNK_HEIGHT; ++y) {
        for (int lz = 0; lz < CHUNK_DEPTH; ++lz) {
            for (int lx = 0; lx < CHUNK_WIDTH; ++lx) {
                const std::size_t idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(CHUNK_WIDTH * CHUNK_DEPTH) +
                                        static_cast<std::size_t>(lz) * static_cast<std::size_t>(CHUNK_WIDTH) +
                                        static_cast<std::size_t>(lx);
                Block bt = static_cast<Block>(blockData[idx]);
                chunk->set_block(lx, y, lz, bt);
            }
        }
    }
    
    chunk->set_generated(true);
    chunk->mark_dirty();
    
    auto mark_neighbor = [this](int cx, int cz) {
        auto neighborIt = chunks_.find({cx, cz});
        if (neighborIt != chunks_.end()) {
            neighborIt->second->mark_dirty();
        }
    };
    mark_neighbor(chunkX - 1, chunkZ);
    mark_neighbor(chunkX + 1, chunkZ);
    mark_neighbor(chunkX, chunkZ - 1);
    mark_neighbor(chunkX, chunkZ + 1);
    
    recompute_chunk_states(chunkX, chunkZ);
    
    TraceLog(LOG_DEBUG, "Applied chunk data for (%d, %d)", chunkX, chunkZ);
}

void World::recompute_chunk_states(int chunkX, int chunkZ) {
    using namespace shared::voxel;
    
    auto key = std::make_pair(chunkX, chunkZ);
    auto it = chunks_.find(key);
    if (it == chunks_.end()) return;
    
    Chunk* chunk = it->second.get();
    const int baseX = chunkX * CHUNK_WIDTH;
    const int baseZ = chunkZ * CHUNK_DEPTH;
    
    for (int y = 0; y < CHUNK_HEIGHT; ++y) {
        for (int lz = 0; lz < CHUNK_DEPTH; ++lz) {
            for (int lx = 0; lx < CHUNK_WIDTH; ++lx) {
                Block block = chunk->get_block(lx, y, lz);
                auto blockType = static_cast<BlockType>(block);
                
                if (!uses_connections(blockType)) continue;
                
                BlockRuntimeState state{};
                int wx = baseX + lx;
                int wz = baseZ + lz;
                
                auto northType = static_cast<BlockType>(get_block(wx, y, wz - 1));
                state.north = can_fence_connect_to(northType);
                
                auto southType = static_cast<BlockType>(get_block(wx, y, wz + 1));
                state.south = can_fence_connect_to(southType);
                
                auto eastType = static_cast<BlockType>(get_block(wx + 1, y, wz));
                state.east = can_fence_connect_to(eastType);
                
                auto westType = static_cast<BlockType>(get_block(wx - 1, y, wz));
                state.west = can_fence_connect_to(westType);
                
                chunk->set_block_state(lx, y, lz, state);
            }
        }
    }
    
    chunk->mark_dirty();
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
            int world_xi = chunk_x * CHUNK_WIDTH + x;
            int world_zi = chunk_z * CHUNK_DEPTH + z;
            
            float noise = octave_perlin(world_x * 0.02f, world_z * 0.02f, 4, 0.5f);
            int height = static_cast<int>(60 + noise * 20);
            
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
                } else if (y == height) {
                    // Vegetation generation: use deterministic hash based on position and seed
                    std::uint32_t hash = seed_;
                    hash ^= static_cast<std::uint32_t>(world_xi) * 374761393u;
                    hash ^= static_cast<std::uint32_t>(world_zi) * 668265263u;
                    hash ^= static_cast<std::uint32_t>(y) * 1013904223u;
                    hash = (hash ^ (hash >> 13)) * 1274126177u;
                    hash ^= hash >> 16;
                    
                    const float chance = static_cast<float>(hash & 0xFFFF) / 65535.0f;
                    
                    // ~15% chance for vegetation on grass
                    if (chance < 0.15f) {
                        const float typeChance = static_cast<float>((hash >> 16) & 0xFFFF) / 65535.0f;
                        
                        if (typeChance < 0.70f) {
                            block_type = static_cast<Block>(BlockType::TallGrass);
                        } else if (typeChance < 0.80f) {
                            block_type = static_cast<Block>(BlockType::Poppy);
                        } else if (typeChance < 0.90f) {
                            block_type = static_cast<Block>(BlockType::Dandelion);
                        } else {
                            block_type = static_cast<Block>(BlockType::DeadBush);
                        }
                    } else {
                        block_type = static_cast<Block>(BlockType::Air);
                    }
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
    
    {
        // IMPORTANT: generating many chunk meshes in a single frame can stall for seconds.
        // Keep mesh rebuild work budgeted per-frame so lighting/chunk streaming stays responsive.
        constexpr float kMeshBudgetMs = 4.0f;
        const auto t0 = std::chrono::steady_clock::now();

        for (auto& [key, chunk] : chunks_) {
            (void)key;
            if (!chunk->needs_mesh_update()) continue;

            chunk->generate_mesh(*this);

            const auto t1 = std::chrono::steady_clock::now();
            const float ms = std::chrono::duration<float, std::milli>(t1 - t0).count();
            if (ms >= kMeshBudgetMs) break;
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
    (void)camera; // May be used for frustum culling later
    
    for (const auto& [key, chunk] : chunks_) {
        // Render with voxel shader (AO) if available, otherwise use default
        if (voxel_shader_loaded_) {
            chunk->render(voxel_shader_);
        } else {
            chunk->render();
        }
    }
}

void World::load_voxel_shader() {
    if (voxel_shader_loaded_) return;

    const char* vs_path = "shaders/voxel.vs";
    const char* fs_path = "shaders/voxel.fs";

    voxel_shader_ = resources::load_shader(vs_path, fs_path);
    if (voxel_shader_.id != 0) {
        voxel_shader_loaded_ = true;
        TraceLog(LOG_INFO, "Voxel shader loaded successfully");
    } else {
        TraceLog(LOG_WARNING, "Voxel shader not found, using default shader");
        voxel_shader_loaded_ = false;
    }
}

void World::unload_voxel_shader() {
    if (voxel_shader_loaded_) {
        UnloadShader(voxel_shader_);
        voxel_shader_loaded_ = false;
        TraceLog(LOG_INFO, "Voxel shader unloaded");
    }
}

} // namespace voxel
