#pragma once

#include "block.hpp"
#include <raylib.h>
#include <array>
#include <memory>
#include <vector>

namespace voxel {

class World;

class Chunk {
public:
    Chunk(int chunk_x, int chunk_z);
    ~Chunk();
    
    // Non-copyable
    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;
    
    // Movable
    Chunk(Chunk&& other) noexcept;
    Chunk& operator=(Chunk&& other) noexcept;
    
    // Block access
    Block get_block(int x, int y, int z) const;
    void set_block(int x, int y, int z, Block type);
    
    // Mesh generation
    void generate_mesh(const World& world);
    void render() const;
    
    // Getters
    int get_chunk_x() const { return chunk_x_; }
    int get_chunk_z() const { return chunk_z_; }
    Vector3 get_world_position() const { return world_position_; }
    bool needs_mesh_update() const { return needs_mesh_update_; }
    bool is_generated() const { return is_generated_; }
    
    void mark_dirty() { needs_mesh_update_ = true; }
    void set_generated(bool value) { is_generated_ = value; }
    
private:
    static int get_index(int x, int y, int z);
    bool is_valid_position(int x, int y, int z) const;
    void cleanup_mesh();
    
    std::array<Block, CHUNK_SIZE> blocks_{};
    Vector3 world_position_{0, 0, 0};
    int chunk_x_{0};
    int chunk_z_{0};
    
    bool needs_mesh_update_{true};
    bool is_generated_{false};
    bool has_mesh_{false};
    
    Mesh mesh_{};
    Model model_{};

    // LS-1: non-solid Light blocks are rendered as separate markers.
    std::vector<Vector3> light_markers_ws_{};
};

} // namespace voxel
