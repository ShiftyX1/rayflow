#pragma once

#include "engine/core/export.hpp"
#include "chunk.hpp"
#include "engine/core/math_types.hpp"
#include "engine/renderer/gl_shader.hpp"
#include "engine/renderer/camera.hpp"
#include "engine/renderer/render_pipeline.hpp"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <vector>
#include <deque>
#include <cstdint>
#include <future>
#include <mutex>

#include "engine/maps/rfmap_io.hpp"
#include "engine/core/thread_pool.hpp"

#include <optional>

namespace voxel {

struct ChunkCoordHash {
    std::size_t operator()(const std::pair<int, int>& coord) const {
        return std::hash<int>()(coord.first) ^ (std::hash<int>()(coord.second) << 16);
    }
};

struct PointLight {
    rf::Vec3 position;
    rf::Vec3 color;
    float radius;
    float intensity;
};

class RAYFLOW_VOXEL_API World {
public:
    explicit World(unsigned int seed);
    ~World();
    
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    
    void set_lights(const std::vector<PointLight>& lights) { scene_lights_ = lights; }
    
    void set_static_lights(const std::vector<rf::Vec3>& positions);
    const std::vector<PointLight>& static_lights() const { return static_lights_; }
    
    Block get_block(int x, int y, int z) const;
    void set_block(int x, int y, int z, Block type);
    
    shared::voxel::BlockRuntimeState get_block_state(int x, int y, int z) const;
    void set_block_state(int x, int y, int z, shared::voxel::BlockRuntimeState state);
    void set_block_with_state(int x, int y, int z, Block type, shared::voxel::BlockRuntimeState state);
    
    Chunk* get_chunk(int chunk_x, int chunk_z);
    Chunk* get_or_create_chunk(int chunk_x, int chunk_z);
    
    void apply_chunk_data(int chunkX, int chunkZ, const std::vector<std::uint8_t>& blockData);
    
    void recompute_chunk_states(int chunkX, int chunkZ);
    
    void update(const rf::Vec3& player_position);
    /// Render all visible chunks using the voxel shader.
    void render(const rf::Camera& camera) const;

    /// Render all visible chunks using the full pipeline (shadows, fog, HDR).
    void render(const rf::Camera& camera, rf::RenderPipeline& pipeline) const;

    /// Render shadow depth pass (call between pipeline.beginShadowPass/endShadowPass).
    void renderShadowPass(rf::IShader& shadowShader, rf::RenderPipeline& pipeline) const;
    
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

    float sample_light01(int x, int y, int z) const;
    float sample_skylight01(int x, int y, int z) const;
    float sample_blocklight01(int x, int y, int z) const { (void)x; (void)y; (void)z; return 0.0f; }

    /// Fog settings derived from environment.
    rf::Vec3 get_fog_color() const;
    float get_fog_start() const;
    float get_fog_end() const;

    void load_voxel_shader();
    void unload_voxel_shader();
    rf::GLShader& get_voxel_shader() { return voxel_shader_; }
    const rf::GLShader& get_voxel_shader() const { return voxel_shader_; }
    bool has_voxel_shader() const { return voxel_shader_.isValid(); }
    
    void set_environment(float timeOfDay, float sunIntensity, float ambientIntensity) {
        time_of_day_ = timeOfDay;
        sun_intensity_ = sunIntensity;
        ambient_intensity_ = ambientIntensity;
    }

    float get_time_of_day() const { return time_of_day_; }
    float get_sun_intensity() const { return sun_intensity_; }
    float get_ambient_intensity() const { return ambient_intensity_; }

private:
    void generate_chunk_terrain(Chunk& chunk);
    void enqueue_chunks_around_player(const rf::Vec3& player_position);
    void process_pending_chunks();
    void process_dirty_chunks();
    void collect_finished_meshes();
    void unload_distant_chunks(const rf::Vec3& player_position);
    void extract_lights_from_map();
    
    float perlin_noise(float x, float y) const;
    float octave_perlin(float x, float y, int octaves, float persistence) const;
    
    using ChunkMap = std::unordered_map<std::pair<int, int>, std::unique_ptr<Chunk>, ChunkCoordHash>;
    ChunkMap chunks_;
    
    unsigned int seed_{0};
    int render_distance_{8};
    rf::Vec3 last_player_position_{0, 0, 0};

    float time_of_day_{12.0f};
    float sun_intensity_{1.0f};
    float ambient_intensity_{0.5f};

    std::optional<shared::maps::MapTemplate> map_template_{};

    std::optional<float> temperature_override_{};

    std::optional<float> humidity_override_{};
    
    mutable std::array<unsigned char, 512> perm_;
    mutable bool perm_initialized_{false};
    void init_perlin() const;

    rf::GLShader voxel_shader_;

    std::vector<PointLight> scene_lights_;
    std::vector<PointLight> static_lights_;
    int light_count_loc_{-1};
    
    // Cached shader locations (resolved once on shader load)
    struct LightLocations {
        int position;
        int color;
        int radius;
        int intensity;
    };
    std::vector<LightLocations> light_locs_;
    int sun_dir_loc_{-1};
    int sun_col_loc_{-1};
    int amb_col_loc_{-1};
    int view_pos_loc_{-1};
    
    std::deque<Chunk*> dirty_chunks_;
    
    // --- Incremental chunk loading (Phase 1) ---
    /// Pending chunk coordinates sorted by distance from player (closest first).
    std::deque<std::pair<int,int>> pending_chunks_;
    /// Set for O(1) dedup of pending coords.
    std::unordered_set<std::pair<int,int>, ChunkCoordHash> pending_set_;
    /// Last player chunk used for enqueue (avoids re-sorting every frame).
    std::pair<int,int> last_player_chunk_{0, 0};
    bool needs_pending_rebuild_{true};
    
    // --- Background mesh generation (Phase 2) ---
    rf::ThreadPool thread_pool_;
    /// Number of mesh tasks currently in flight (lighter than scanning mesh_futures_).
    int mesh_tasks_in_flight_{0};
    struct PendingMesh {
        Chunk* chunk;
        std::future<ChunkMeshData> future;
    };
    std::vector<PendingMesh> mesh_futures_;
};


} // namespace voxel
