#pragma once

#include "chunk.hpp"
#include <raylib.h>
#include <unordered_map>
#include <memory>
#include <functional>

#include "../../shared/maps/rfmap_io.hpp"

#include <optional>

namespace voxel {

// Hash function for chunk coordinates
struct ChunkCoordHash {
    std::size_t operator()(const std::pair<int, int>& coord) const {
        return std::hash<int>()(coord.first) ^ (std::hash<int>()(coord.second) << 16);
    }
};

class World {
public:
    explicit World(unsigned int seed);
    ~World();
    
    // Non-copyable
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    
    // Block access (world coordinates)
    Block get_block(int x, int y, int z) const;
    void set_block(int x, int y, int z, Block type);
    
    // Chunk management
    Chunk* get_chunk(int chunk_x, int chunk_z);
    Chunk* get_or_create_chunk(int chunk_x, int chunk_z);
    
    // Update and render
    void update(const Vector3& player_position);
    void render(const Camera3D& camera) const;
    
    // Configuration
    void set_render_distance(int distance) { render_distance_ = distance; }
    int get_render_distance() const { return render_distance_; }
    unsigned int get_seed() const { return seed_; }

    // MT-1: optional finite map template used for chunk generation.
    bool has_map_template() const { return map_template_.has_value(); }
    const shared::maps::MapTemplate* map_template() const { return map_template_ ? &*map_template_ : nullptr; }
    void set_map_template(shared::maps::MapTemplate map);
    void clear_map_template();

    // MV-2: render-only temperature used for foliage/grass recolor.
    // Range: [0, 1] where 0=cold, 1=hot.
    float temperature() const;
    void set_temperature_override(float temperature);
    void clear_temperature_override();

    void mark_all_chunks_dirty();

    // Lighting disabled - return full brightness
    float sample_light01(int x, int y, int z) const { (void)x; (void)y; (void)z; return 1.0f; }
    float sample_skylight01(int x, int y, int z) const { (void)x; (void)y; (void)z; return 1.0f; }
    float sample_blocklight01(int x, int y, int z) const { (void)x; (void)y; (void)z; return 0.0f; }
    
private:
    void generate_chunk_terrain(Chunk& chunk);
    void load_chunks_around_player(const Vector3& player_position);
    void unload_distant_chunks(const Vector3& player_position);
    
    // Noise functions for terrain generation
    float perlin_noise(float x, float y) const;
    float octave_perlin(float x, float y, int octaves, float persistence) const;
    
    using ChunkMap = std::unordered_map<std::pair<int, int>, std::unique_ptr<Chunk>, ChunkCoordHash>;
    ChunkMap chunks_;
    
    unsigned int seed_{0};
    int render_distance_{8};
    Vector3 last_player_position_{0, 0, 0};

    std::optional<shared::maps::MapTemplate> map_template_{};

    // MV-2: editor/runtime override for render temperature.
    // When set, it takes precedence over template temperature.
    std::optional<float> temperature_override_{};
    
    // Perlin noise permutation table
    mutable std::array<unsigned char, 512> perm_;
    mutable bool perm_initialized_{false};
    void init_perlin() const;
};

} // namespace voxel
