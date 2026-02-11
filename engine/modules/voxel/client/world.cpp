#include "world.hpp"
#include "engine/client/core/resources.hpp"
#include "engine/core/logging.hpp"
#include "engine/core/math_types.hpp"
#include <glm/gtc/matrix_transform.hpp>
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
    extract_lights_from_map();
}

void World::clear_map_template() {
    map_template_.reset();
    chunks_.clear();
    static_lights_.clear();
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
    
    chunk->set_dirty_callback([this](Chunk* c) {
        if (std::find(dirty_chunks_.begin(), dirty_chunks_.end(), c) == dirty_chunks_.end()) {
            dirty_chunks_.push_back(c);
        }
    });
    
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
        
        newChunk->set_dirty_callback([this](Chunk* c) {
            if (std::find(dirty_chunks_.begin(), dirty_chunks_.end(), c) == dirty_chunks_.end()) {
                dirty_chunks_.push_back(c);
            }
        });
        
        chunks_[key] = std::move(newChunk);
    }
    
    for (int y = 0; y < CHUNK_HEIGHT; ++y) {
        for (int lz = 0; lz < CHUNK_DEPTH; ++lz) {
            for (int lx = 0; lx < CHUNK_WIDTH; ++lx) {
                const std::size_t idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(CHUNK_WIDTH * CHUNK_DEPTH) +
                                        static_cast<std::size_t>(lz) * static_cast<std::size_t>(CHUNK_WIDTH) +
                                        static_cast<std::size_t>(lx);
                Block bt = static_cast<Block>(blockData[idx]);
                
                if (static_cast<BlockType>(bt) == BlockType::Light) {
                    bt = static_cast<Block>(BlockType::Air);
                }
                
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
    
    Chunk* north_chunk = get_chunk(chunkX, chunkZ - 1);
    Chunk* south_chunk = get_chunk(chunkX, chunkZ + 1);
    Chunk* east_chunk = get_chunk(chunkX + 1, chunkZ);
    Chunk* west_chunk = get_chunk(chunkX - 1, chunkZ);
    
    for (int y = 0; y < CHUNK_HEIGHT; ++y) {
        for (int lz = 0; lz < CHUNK_DEPTH; ++lz) {
            for (int lx = 0; lx < CHUNK_WIDTH; ++lx) {
                Block block = chunk->get_block(lx, y, lz);
                auto blockType = static_cast<BlockType>(block);
                
                if (!uses_connections(blockType)) continue;
                
                BlockRuntimeState state{};
                
                // North neighbor (z-1)
                if (lz == 0 && north_chunk) {
                    state.north = can_fence_connect_to(static_cast<BlockType>(north_chunk->get_block(lx, y, CHUNK_DEPTH - 1)));
                } else if (lz > 0) {
                    state.north = can_fence_connect_to(static_cast<BlockType>(chunk->get_block(lx, y, lz - 1)));
                }
                
                // South neighbor (z+1)
                if (lz == CHUNK_DEPTH - 1 && south_chunk) {
                    state.south = can_fence_connect_to(static_cast<BlockType>(south_chunk->get_block(lx, y, 0)));
                } else if (lz < CHUNK_DEPTH - 1) {
                    state.south = can_fence_connect_to(static_cast<BlockType>(chunk->get_block(lx, y, lz + 1)));
                }
                
                // East neighbor (x+1)
                if (lx == CHUNK_WIDTH - 1 && east_chunk) {
                    state.east = can_fence_connect_to(static_cast<BlockType>(east_chunk->get_block(0, y, lz)));
                } else if (lx < CHUNK_WIDTH - 1) {
                    state.east = can_fence_connect_to(static_cast<BlockType>(chunk->get_block(lx + 1, y, lz)));
                }
                
                // West neighbor (x-1)
                if (lx == 0 && west_chunk) {
                    state.west = can_fence_connect_to(static_cast<BlockType>(west_chunk->get_block(CHUNK_WIDTH - 1, y, lz)));
                } else if (lx > 0) {
                    state.west = can_fence_connect_to(static_cast<BlockType>(chunk->get_block(lx - 1, y, lz)));
                }
                
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
                            
                            if (static_cast<BlockType>(bt) == BlockType::Light) {
                                bt = static_cast<Block>(BlockType::Air);
                            }
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
    
    const int base_x = chunk_x * CHUNK_WIDTH;
    const int base_z = chunk_z * CHUNK_DEPTH;
    
    for (int x = 0; x < CHUNK_WIDTH; x++) {
        const int world_xi = base_x + x;
        const float world_x = static_cast<float>(world_xi);
        
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            const int world_zi = base_z + z;
            const float world_z = static_cast<float>(world_zi);
            
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

void World::update(const rf::Vec3& player_position) {
    load_chunks_around_player(player_position);
    unload_distant_chunks(player_position);
    
    if (dirty_chunks_.empty()) {
        for (auto& [key, chunk] : chunks_) {
            (void)key;
            if (chunk->needs_mesh_update()) {
                dirty_chunks_.push_back(chunk.get());
            }
        }
    }
    
    if (!dirty_chunks_.empty()) {
        // IMPORTANT: generating many chunk meshes in a single frame can stall for seconds.
        // Keep mesh rebuild work budgeted per-frame so lighting/chunk streaming stays responsive.
        constexpr float kMeshBudgetMs = 4.0f;
        const auto t0 = std::chrono::steady_clock::now();
        
        auto it = dirty_chunks_.begin();
        while (it != dirty_chunks_.end()) {
            Chunk* chunk = *it;
            
            if (chunk->needs_mesh_update()) {
                chunk->generate_mesh(*this);
                
                const auto t1 = std::chrono::steady_clock::now();
                const float ms = std::chrono::duration<float, std::milli>(t1 - t0).count();
                
                it = dirty_chunks_.erase(it);
                
                if (ms >= kMeshBudgetMs) break;
            } else {
                it = dirty_chunks_.erase(it);
            }
        }
    }
    
    last_player_position_ = player_position;
}

void World::load_chunks_around_player(const rf::Vec3& player_position) {
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

void World::unload_distant_chunks(const rf::Vec3& player_position) {
    int player_chunk_x = static_cast<int>(std::floor(player_position.x / CHUNK_WIDTH));
    int player_chunk_z = static_cast<int>(std::floor(player_position.z / CHUNK_DEPTH));
    
    constexpr int UNLOAD_DIST_SQ = CHUNK_UNLOAD_DISTANCE * CHUNK_UNLOAD_DISTANCE;
    
    for (auto it = chunks_.begin(); it != chunks_.end(); ) {
        int dx = it->first.first - player_chunk_x;
        int dz = it->first.second - player_chunk_z;
        
        if (dx * dx + dz * dz > UNLOAD_DIST_SQ) {
            auto dirty_it = std::find(dirty_chunks_.begin(), dirty_chunks_.end(), it->second.get());
            if (dirty_it != dirty_chunks_.end()) {
                dirty_chunks_.erase(dirty_it);
            }
            it = chunks_.erase(it);
        } else {
            ++it;
        }
    }
}

// =============================================================================
// Rendering
// =============================================================================

void World::render(const rf::Camera& camera) const {
    if (!voxel_shader_.isValid()) return;

    auto& shader = const_cast<rf::GLShader&>(voxel_shader_);
    shader.bind();

    // MVP = projection * view (model is identity for chunks — vertices are in world space)
    rf::Mat4 view = camera.viewMatrix();
    rf::Mat4 proj = camera.projectionMatrix();
    rf::Mat4 model = rf::Mat4(1.0f);
    rf::Mat4 mvp = proj * view * model;

    shader.setMat4("mvp", mvp);
    shader.setMat4("matModel", model);
    rf::Mat4 normalMat = glm::transpose(glm::inverse(model));
    shader.setMat4("matNormal", normalMat);

    // Diffuse color (white by default)
    shader.setVec4("colDiffuse", rf::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

    // Sun direction (based on time of day)
    float angle = (time_of_day_ / 24.0f) * glm::two_pi<float>() - glm::half_pi<float>();
    rf::Vec3 sunDir = glm::normalize(rf::Vec3(
        std::cos(angle), std::sin(angle), 0.3f
    ));
    shader.setVec3("sunDir", sunDir);

    // Sun and ambient colors
    rf::Vec3 sunColor = rf::Vec3(1.0f, 0.95f, 0.9f) * sun_intensity_;
    rf::Vec3 ambientColor = rf::Vec3(0.4f, 0.45f, 0.55f) * ambient_intensity_;
    shader.setVec3("sunColor", sunColor);
    shader.setVec3("ambientColor", ambientColor);

    // Camera position (for specular / point light distance)
    shader.setVec3("viewPos", camera.position());

    // Point lights
    auto all_lights = scene_lights_;
    all_lights.insert(all_lights.end(), static_lights_.begin(), static_lights_.end());
    int light_count = std::min(static_cast<int>(all_lights.size()), 32);
    shader.setInt("lightCount", light_count);

    for (int i = 0; i < light_count; ++i) {
        if (i < static_cast<int>(light_locs_.size())) {
            shader.setVec3(light_locs_[i].position, all_lights[i].position);
            shader.setVec3(light_locs_[i].color, all_lights[i].color);
            shader.setFloat(light_locs_[i].radius, all_lights[i].radius);
            shader.setFloat(light_locs_[i].intensity, all_lights[i].intensity);
        }
    }

    // Atlas texture is bound to unit 0 by the caller (block_registry or engine)
    shader.setInt("texture0", 0);

    // Draw all chunks
    for (const auto& [coord, chunk] : chunks_) {
        if (chunk && chunk->is_generated()) {
            chunk->render();
        }
    }

    rf::GLShader::unbind();
}

void World::load_voxel_shader() {
    if (voxel_shader_.isValid()) return;

    bool ok = voxel_shader_.loadFromFiles("shaders/voxel.vs", "shaders/voxel.fs");
    if (ok) {
        // Cache uniform locations
        light_count_loc_ = voxel_shader_.getUniformLocation("lightCount");
        sun_dir_loc_ = voxel_shader_.getUniformLocation("sunDir");
        sun_col_loc_ = voxel_shader_.getUniformLocation("sunColor");
        amb_col_loc_ = voxel_shader_.getUniformLocation("ambientColor");
        view_pos_loc_ = voxel_shader_.getUniformLocation("viewPos");

        light_locs_.clear();
        light_locs_.reserve(32);
        for (int i = 0; i < 32; i++) {
            std::string base = "lights[" + std::to_string(i) + "]";
            LightLocations locs;
            locs.position = voxel_shader_.getUniformLocation(base + ".position");
            locs.color = voxel_shader_.getUniformLocation(base + ".color");
            locs.radius = voxel_shader_.getUniformLocation(base + ".radius");
            locs.intensity = voxel_shader_.getUniformLocation(base + ".intensity");
            light_locs_.push_back(locs);
        }

        TraceLog(LOG_INFO, "Voxel shader loaded successfully");
    } else {
        TraceLog(LOG_WARNING, "Voxel shader failed to load");
    }
}

void World::unload_voxel_shader() {
    voxel_shader_.destroy();
    light_locs_.clear();
    TraceLog(LOG_INFO, "Voxel shader unloaded");
}

float World::sample_skylight01(int x, int y, int z) const {
    auto it = chunks_.find({x >> 4, z >> 4});
    if (it != chunks_.end()) {
        return it->second->get_light(x & 15, y, z & 15) / 15.0f;
    }
    return 1.0f;
}

void World::set_static_lights(const std::vector<rf::Vec3>& positions) {
    static_lights_.clear();
    static_lights_.reserve(positions.size());
    
    // Default light properties for map lights
    const rf::Vec3 default_color = {1.0f, 0.95f, 0.85f};
    const float default_radius = 12.0f;
    const float default_intensity = 1.2f;
    
    for (const auto& pos : positions) {
        static_lights_.push_back(PointLight{
            .position = pos,
            .color = default_color,
            .radius = default_radius,
            .intensity = default_intensity
        });
    }
    
    TraceLog(LOG_INFO, "Set %zu static lights from map", static_lights_.size());
}

void World::extract_lights_from_map() {
    static_lights_.clear();
    
    if (!map_template_) {
        return;
    }
    
    std::vector<rf::Vec3> light_positions;
    
    // Scan all chunks in the map template for Light blocks
    for (const auto& [chunk_coord, chunk_data] : map_template_->chunks) {
        const int chunk_x = chunk_coord.first;
        const int chunk_z = chunk_coord.second;
        const int base_x = chunk_x * CHUNK_WIDTH;
        const int base_z = chunk_z * CHUNK_DEPTH;
        
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
                for (int x = 0; x < CHUNK_WIDTH; ++x) {
                    const std::size_t idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(CHUNK_WIDTH * CHUNK_DEPTH) +
                                            static_cast<std::size_t>(z) * static_cast<std::size_t>(CHUNK_WIDTH) +
                                            static_cast<std::size_t>(x);
                    
                    const auto block_type = chunk_data.blocks[idx];
                    
                    if (block_type == BlockType::Light) {
                        rf::Vec3 pos = {
                            static_cast<float>(base_x + x) + 0.5f,
                            static_cast<float>(y) + 0.5f,
                            static_cast<float>(base_z + z) + 0.5f
                        };
                        light_positions.push_back(pos);
                    }
                }
            }
        }
    }
    
    set_static_lights(light_positions);
}

} // namespace voxel
