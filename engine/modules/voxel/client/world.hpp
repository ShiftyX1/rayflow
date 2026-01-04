#pragma once

#include "chunk.hpp"
#include <raylib.h>
#include <unordered_map>
#include <memory>
#include <functional>
#include <vector>
#include <cstdint>

#include "engine/maps/rfmap_io.hpp"

#include <optional>

namespace voxel {

struct ChunkCoordHash {
    std::size_t operator()(const std::pair<int, int>& coord) const {
        return std::hash<int>()(coord.first) ^ (std::hash<int>()(coord.second) << 16);
    }
};

class World {
public:
    explicit World(unsigned int seed);
    ~World();
    
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    
    Block get_block(int x, int y, int z) const;
    void set_block(int x, int y, int z, Block type);
    
    shared::voxel::BlockRuntimeState get_block_state(int x, int y, int z) const;
    void set_block_state(int x, int y, int z, shared::voxel::BlockRuntimeState state);
    void set_block_with_state(int x, int y, int z, Block type, shared::voxel::BlockRuntimeState state);
    
    Chunk* get_chunk(int chunk_x, int chunk_z);
    Chunk* get_or_create_chunk(int chunk_x, int chunk_z);
    
    void apply_chunk_data(int chunkX, int chunkZ, const std::vector<std::uint8_t>& blockData);
    
    void recompute_chunk_states(int chunkX, int chunkZ);
    
    void update(const Vector3& player_position);
    void render(const Camera3D& camera) const;
    
    void set_render_distance(int distance) { render_distance_ = distance; }
    int get_render_distance() const { return render_distance_; }
    unsigned int get_seed() const { return seed_; }

    bool has_map_template() const { return map_template_.has_value(); }
    const shared::maps::MapTemplate* map_template() const { return map_template_ ? &*map_template_ : nullptr; }
    void set_map_template(shared::maps::MapTemplate map);
    void clear_map_template();

    float temperature() const;
    void set_temperature_override(float temperature);
    void clear_temperature_override();

    float humidity() const;
    void set_humidity_override(float humidity);
    void clear_humidity_override();

    void mark_all_chunks_dirty();

    float sample_light01(int x, int y, int z) const { (void)x; (void)y; (void)z; return 1.0f; }
    float sample_skylight01(int x, int y, int z) const { (void)x; (void)y; (void)z; return 1.0f; }
    float sample_blocklight01(int x, int y, int z) const { (void)x; (void)y; (void)z; return 0.0f; }

    void load_voxel_shader();
    void unload_voxel_shader();
    Shader get_voxel_shader() const { return voxel_shader_; }
    bool has_voxel_shader() const { return voxel_shader_loaded_; }
    
private:
    void generate_chunk_terrain(Chunk& chunk);
    void load_chunks_around_player(const Vector3& player_position);
    void unload_distant_chunks(const Vector3& player_position);
    
    float perlin_noise(float x, float y) const;
    float octave_perlin(float x, float y, int octaves, float persistence) const;
    
    using ChunkMap = std::unordered_map<std::pair<int, int>, std::unique_ptr<Chunk>, ChunkCoordHash>;
    ChunkMap chunks_;
    
    unsigned int seed_{0};
    int render_distance_{8};
    Vector3 last_player_position_{0, 0, 0};

    std::optional<shared::maps::MapTemplate> map_template_{};

    std::optional<float> temperature_override_{};

    std::optional<float> humidity_override_{};
    
    mutable std::array<unsigned char, 512> perm_;
    mutable bool perm_initialized_{false};
    void init_perlin() const;

    Shader voxel_shader_{};
    bool voxel_shader_loaded_{false};
};

} // namespace voxel
