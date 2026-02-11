#pragma once

#include "block.hpp"
#include "../shared/block_state.hpp"
#include "engine/core/math_types.hpp"
#include "engine/renderer/gl_mesh.hpp"
#include "engine/renderer/gl_shader.hpp"
#include <array>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <algorithm>

namespace voxel {

class World;

class Chunk {
public:
    Chunk(int chunk_x, int chunk_z);
    ~Chunk();
    
    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;
    
    Chunk(Chunk&& other) noexcept;
    Chunk& operator=(Chunk&& other) noexcept;
    
    Block get_block(int x, int y, int z) const;
    void set_block(int x, int y, int z, Block type);
    
    shared::voxel::BlockRuntimeState get_block_state(int x, int y, int z) const;
    void set_block_state(int x, int y, int z, shared::voxel::BlockRuntimeState state);
    
    void set_block_with_state(int x, int y, int z, Block type, shared::voxel::BlockRuntimeState state);
    
    void calculate_lighting();
    std::uint8_t get_light(int x, int y, int z) const;
    void set_light(int x, int y, int z, std::uint8_t value);

    void generate_mesh(const World& world);
    
    /// Draw this chunk's mesh (shader must already be bound).
    void render() const;
    
    /// Draw with a specific shader (binds uniform for model matrix).
    void render(rf::GLShader& shader) const;
    
    int get_chunk_x() const { return chunk_x_; }
    int get_chunk_z() const { return chunk_z_; }
    rf::Vec3 get_world_position() const { return world_position_; }
    bool needs_mesh_update() const { return needs_mesh_update_; }
    bool is_generated() const { return is_generated_; }
    bool is_empty() const { return is_empty_; }
    
    void mark_dirty() { 
        needs_mesh_update_ = true;
        if (on_marked_dirty_) on_marked_dirty_(this);
    }
    void set_generated(bool value) { is_generated_ = value; }
    
    void set_dirty_callback(std::function<void(Chunk*)> callback) { on_marked_dirty_ = callback; }
    
private:
    static int get_index(int x, int y, int z);
    bool is_valid_position(int x, int y, int z) const;
    void cleanup_mesh();
    
    std::function<void(Chunk*)> on_marked_dirty_;
    
    std::array<Block, CHUNK_SIZE> blocks_{};
    std::array<std::uint8_t, CHUNK_SIZE> light_map_{};
    std::unordered_map<int, shared::voxel::BlockRuntimeState> block_states_{};
    
    rf::Vec3 world_position_{0, 0, 0};
    int chunk_x_{0};
    int chunk_z_{0};
    
    bool needs_mesh_update_{true};
    bool is_generated_{false};
    bool has_mesh_{false};
    bool is_empty_{false};
    
    // GPU mesh (Phase 2: real OpenGL VAO/VBO)
    rf::GLMesh mesh_;

    std::vector<rf::Vec3> light_markers_ws_{};
};

} // namespace voxel
