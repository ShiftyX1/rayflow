#include "chunk.hpp"
#include "block_registry.hpp"
#include "block_model_loader.hpp"
#include "world.hpp"
#include "engine/client/core/config.hpp"
#include "../shared/block_shape.hpp"
#include <raylib.h>
#include <cstring>
#include <chrono>
#include <vector>
#include <cstdio>
#include <algorithm>
#include <cmath>

namespace voxel {

Chunk::Chunk(int chunk_x, int chunk_z) 
    : chunk_x_(chunk_x), chunk_z_(chunk_z) {
    world_position_ = {
        static_cast<float>(chunk_x * CHUNK_WIDTH),
        0.0f,
        static_cast<float>(chunk_z * CHUNK_DEPTH)
    };
    blocks_.fill(static_cast<Block>(BlockType::Air));
}

Chunk::~Chunk() {
    cleanup_mesh();
}

Chunk::Chunk(Chunk&& other) noexcept 
    : blocks_(std::move(other.blocks_))
    , block_states_(std::move(other.block_states_))
    , world_position_(other.world_position_)
    , chunk_x_(other.chunk_x_)
    , chunk_z_(other.chunk_z_)
    , needs_mesh_update_(other.needs_mesh_update_)
    , is_generated_(other.is_generated_)
    , has_mesh_(other.has_mesh_)
    , mesh_(other.mesh_)
    , model_(other.model_) {
    other.has_mesh_ = false;
    other.mesh_ = {};
    other.model_ = {};
}

Chunk& Chunk::operator=(Chunk&& other) noexcept {
    if (this != &other) {
        cleanup_mesh();
        
        blocks_ = std::move(other.blocks_);
        block_states_ = std::move(other.block_states_);
        world_position_ = other.world_position_;
        chunk_x_ = other.chunk_x_;
        chunk_z_ = other.chunk_z_;
        needs_mesh_update_ = other.needs_mesh_update_;
        is_generated_ = other.is_generated_;
        has_mesh_ = other.has_mesh_;
        mesh_ = other.mesh_;
        model_ = other.model_;
        
        other.has_mesh_ = false;
        other.mesh_ = {};
        other.model_ = {};
    }
    return *this;
}

void Chunk::cleanup_mesh() {
    if (has_mesh_) {
        UnloadModel(model_);
        has_mesh_ = false;
    }
}

int Chunk::get_index(int x, int y, int z) {
    return y * CHUNK_WIDTH * CHUNK_DEPTH + z * CHUNK_WIDTH + x;
}

bool Chunk::is_valid_position(int x, int y, int z) const {
    return x >= 0 && x < CHUNK_WIDTH &&
           y >= 0 && y < CHUNK_HEIGHT &&
           z >= 0 && z < CHUNK_DEPTH;
}

Block Chunk::get_block(int x, int y, int z) const {
    if (!is_valid_position(x, y, z)) {
        return static_cast<Block>(BlockType::Air);
    }
    return blocks_[get_index(x, y, z)];
}

void Chunk::set_block(int x, int y, int z, Block type) {
    if (!is_valid_position(x, y, z)) return;
    blocks_[get_index(x, y, z)] = type;
    needs_mesh_update_ = true;
}

shared::voxel::BlockRuntimeState Chunk::get_block_state(int x, int y, int z) const {
    if (!is_valid_position(x, y, z)) {
        return shared::voxel::BlockRuntimeState::defaults();
    }
    int idx = get_index(x, y, z);
    auto it = block_states_.find(idx);
    if (it != block_states_.end()) {
        return it->second;
    }
    return shared::voxel::BlockRuntimeState::defaults();
}

void Chunk::set_block_state(int x, int y, int z, shared::voxel::BlockRuntimeState state) {
    if (!is_valid_position(x, y, z)) return;
    int idx = get_index(x, y, z);
    if (state == shared::voxel::BlockRuntimeState::defaults()) {
        block_states_.erase(idx);
    } else {
        block_states_[idx] = state;
    }
    needs_mesh_update_ = true;
}

void Chunk::set_block_with_state(int x, int y, int z, Block type, shared::voxel::BlockRuntimeState state) {
    if (!is_valid_position(x, y, z)) return;
    int idx = get_index(x, y, z);
    blocks_[idx] = type;
    if (state == shared::voxel::BlockRuntimeState::defaults()) {
        block_states_.erase(idx);
    } else {
        block_states_[idx] = state;
    }
    needs_mesh_update_ = true;
}

void Chunk::generate_mesh(const World& world) {
    const auto t_total0 = std::chrono::steady_clock::now();

    const float temperature = std::clamp(world.temperature(), 0.0f, 1.0f);
    const float humidity = std::clamp(world.humidity(), 0.0f, 1.0f);
    const Color grass_tint = BlockRegistry::instance().sample_grass_color(temperature, humidity);
    const Color foliage_tint = BlockRegistry::instance().sample_foliage_color(temperature, humidity);

    cleanup_mesh();

    light_markers_ws_.clear();
    
    std::vector<float> vertices;
    std::vector<float> texcoords;
    std::vector<float> texcoords2;
    std::vector<float> normals;
    std::vector<unsigned char> colors;
    
    auto& registry = BlockRegistry::instance();
    float atlas_size = static_cast<float>(registry.get_atlas_texture().width);
    float tile_size = 16.0f;
    float uv_size = tile_size / atlas_size;
    
    static const float face_vertices[6][6][3] = {
        // +X face
        {{1,0,0}, {1,1,0}, {1,1,1}, {1,0,0}, {1,1,1}, {1,0,1}},
        // -X face  
        {{0,0,1}, {0,1,1}, {0,1,0}, {0,0,1}, {0,1,0}, {0,0,0}},
        // +Y face (top)
        {{0,1,0}, {0,1,1}, {1,1,1}, {0,1,0}, {1,1,1}, {1,1,0}},
        // -Y face (bottom)
        {{0,0,1}, {0,0,0}, {1,0,0}, {0,0,1}, {1,0,0}, {1,0,1}},
        // +Z face
        {{1,0,1}, {1,1,1}, {0,1,1}, {1,0,1}, {0,1,1}, {0,0,1}},
        // -Z face
        {{0,0,0}, {0,1,0}, {1,1,0}, {0,0,0}, {1,1,0}, {1,0,0}}
    };
    
    static const float face_uvs[6][6][2] = {
        // +X face
        {{1,1}, {1,0}, {0,0}, {1,1}, {0,0}, {0,1}},
        // -X face
        {{1,1}, {1,0}, {0,0}, {1,1}, {0,0}, {0,1}},
        // +Y face (top)
        {{0,0}, {0,1}, {1,1}, {0,0}, {1,1}, {1,0}},
        // -Y face (bottom)
        {{0,1}, {0,0}, {1,0}, {0,1}, {1,0}, {1,1}},
        // +Z face
        {{1,1}, {1,0}, {0,0}, {1,1}, {0,0}, {0,1}},
        // -Z face
        {{1,1}, {1,0}, {0,0}, {1,1}, {0,0}, {0,1}}
    };
    
    static const float face_normals[6][3] = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
    };

    static const int face_dir[6][3] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 1, 0}, { 0,-1, 0},
        { 0, 0, 1}, { 0, 0,-1}
    };

    static const int face_u[6][3] = {
        { 0, 0, 1}, { 0, 0,-1},
        { 1, 0, 0}, { 1, 0, 0},
        {-1, 0, 0}, { 1, 0, 0}
    };
    static const int face_v[6][3] = {
        { 0, 1, 0}, { 0, 1, 0},
        { 0, 0, 1}, { 0, 0,-1},
        { 0, 1, 0}, { 0, 1, 0}
    };

    static const int tri_corner_idx[6] = {0, 1, 2, 0, 2, 3};

    auto calc_corner_ao = [&world](int wx, int wy, int wz,
                                    const int* dir,
                                    const int* u_axis,
                                    const int* v_axis,
                                    int u_sign, int v_sign) -> float {
        const int side1_x = wx + dir[0] + u_axis[0] * u_sign;
        const int side1_y = wy + dir[1] + u_axis[1] * u_sign;
        const int side1_z = wz + dir[2] + u_axis[2] * u_sign;

        const int side2_x = wx + dir[0] + v_axis[0] * v_sign;
        const int side2_y = wy + dir[1] + v_axis[1] * v_sign;
        const int side2_z = wz + dir[2] + v_axis[2] * v_sign;

        const int corner_x = wx + dir[0] + u_axis[0] * u_sign + v_axis[0] * v_sign;
        const int corner_y = wy + dir[1] + u_axis[1] * u_sign + v_axis[1] * v_sign;
        const int corner_z = wz + dir[2] + u_axis[2] * u_sign + v_axis[2] * v_sign;

        const bool s1 = is_solid(static_cast<BlockType>(world.get_block(side1_x, side1_y, side1_z)));
        const bool s2 = is_solid(static_cast<BlockType>(world.get_block(side2_x, side2_y, side2_z)));
        const bool c  = is_solid(static_cast<BlockType>(world.get_block(corner_x, corner_y, corner_z)));

        int ao_level;
        if (s1 && s2) {
            ao_level = 0;
        } else {
            ao_level = 3 - (s1 ? 1 : 0) - (s2 ? 1 : 0) - (c ? 1 : 0);
        }

        static const float ao_values[4] = { 0.2f, 0.5f, 0.75f, 1.0f };
        return ao_values[ao_level];
    };

    static const int corner_u_sign[4] = { -1, -1, +1, +1 };
    static const int corner_v_sign[4] = { -1, +1, +1, -1 };

    auto should_cull_model_face = [&world](int wx, int wy, int wz, int face_idx) -> bool {
        Block neighbor = world.get_block(wx, wy, wz);
        auto neighbor_type = static_cast<BlockType>(neighbor);
        
        if (is_transparent(neighbor_type)) return false;
        
        const auto* neighbor_model = BlockModelLoader::instance().get_model(neighbor_type);
        if (!neighbor_model) {
            return true;
        }
        
        return neighbor_model->shape == shared::voxel::BlockShape::Full;
    };

    auto add_element_face = [&](
        float bx, float by, float bz,
        const shared::voxel::ModelElement& elem,
        int face_idx,
        BlockType block_type,
        int wx, int wy, int wz,
        float foliageMask,
        Color tint
    ) {
        float x0 = elem.from[0] / 16.0f;
        float y0 = elem.from[1] / 16.0f;
        float z0 = elem.from[2] / 16.0f;
        float x1 = elem.to[0] / 16.0f;
        float y1 = elem.to[1] / 16.0f;
        float z1 = elem.to[2] / 16.0f;

        float fv[6][3];
        switch (face_idx) {
            case 0: // +X (East)
                fv[0][0] = x1; fv[0][1] = y0; fv[0][2] = z0;
                fv[1][0] = x1; fv[1][1] = y1; fv[1][2] = z0;
                fv[2][0] = x1; fv[2][1] = y1; fv[2][2] = z1;
                fv[3][0] = x1; fv[3][1] = y0; fv[3][2] = z0;
                fv[4][0] = x1; fv[4][1] = y1; fv[4][2] = z1;
                fv[5][0] = x1; fv[5][1] = y0; fv[5][2] = z1;
                break;
            case 1: // -X (West)
                fv[0][0] = x0; fv[0][1] = y0; fv[0][2] = z1;
                fv[1][0] = x0; fv[1][1] = y1; fv[1][2] = z1;
                fv[2][0] = x0; fv[2][1] = y1; fv[2][2] = z0;
                fv[3][0] = x0; fv[3][1] = y0; fv[3][2] = z1;
                fv[4][0] = x0; fv[4][1] = y1; fv[4][2] = z0;
                fv[5][0] = x0; fv[5][1] = y0; fv[5][2] = z0;
                break;
            case 2: // +Y (Up)
                fv[0][0] = x0; fv[0][1] = y1; fv[0][2] = z0;
                fv[1][0] = x0; fv[1][1] = y1; fv[1][2] = z1;
                fv[2][0] = x1; fv[2][1] = y1; fv[2][2] = z1;
                fv[3][0] = x0; fv[3][1] = y1; fv[3][2] = z0;
                fv[4][0] = x1; fv[4][1] = y1; fv[4][2] = z1;
                fv[5][0] = x1; fv[5][1] = y1; fv[5][2] = z0;
                break;
            case 3: // -Y (Down)
                fv[0][0] = x0; fv[0][1] = y0; fv[0][2] = z1;
                fv[1][0] = x0; fv[1][1] = y0; fv[1][2] = z0;
                fv[2][0] = x1; fv[2][1] = y0; fv[2][2] = z0;
                fv[3][0] = x0; fv[3][1] = y0; fv[3][2] = z1;
                fv[4][0] = x1; fv[4][1] = y0; fv[4][2] = z0;
                fv[5][0] = x1; fv[5][1] = y0; fv[5][2] = z1;
                break;
            case 4: // +Z (South)
                fv[0][0] = x1; fv[0][1] = y0; fv[0][2] = z1;
                fv[1][0] = x1; fv[1][1] = y1; fv[1][2] = z1;
                fv[2][0] = x0; fv[2][1] = y1; fv[2][2] = z1;
                fv[3][0] = x1; fv[3][1] = y0; fv[3][2] = z1;
                fv[4][0] = x0; fv[4][1] = y1; fv[4][2] = z1;
                fv[5][0] = x0; fv[5][1] = y0; fv[5][2] = z1;
                break;
            case 5: // -Z (North)
                fv[0][0] = x0; fv[0][1] = y0; fv[0][2] = z0;
                fv[1][0] = x0; fv[1][1] = y1; fv[1][2] = z0;
                fv[2][0] = x1; fv[2][1] = y1; fv[2][2] = z0;
                fv[3][0] = x0; fv[3][1] = y0; fv[3][2] = z0;
                fv[4][0] = x1; fv[4][1] = y1; fv[4][2] = z0;
                fv[5][0] = x1; fv[5][1] = y0; fv[5][2] = z0;
                break;
        }

        const auto& face_data = elem.faces[face_idx];
        float u0_norm = face_data.uv[0] / 16.0f;
        float v0_norm = face_data.uv[1] / 16.0f;
        float u1_norm = face_data.uv[2] / 16.0f;
        float v1_norm = face_data.uv[3] / 16.0f;

        Rectangle tex_rect = registry.get_texture_rect(block_type, face_idx);
        float u_base = tex_rect.x / atlas_size;
        float v_base = tex_rect.y / atlas_size;
        
        float u_scale = uv_size;
        float v_scale = uv_size;

        float fuv[6][2];
        switch (face_idx) {
            case 0: case 1: case 4: case 5: // Side faces
                fuv[0][0] = u_base + u1_norm * u_scale; fuv[0][1] = v_base + v1_norm * v_scale;
                fuv[1][0] = u_base + u1_norm * u_scale; fuv[1][1] = v_base + v0_norm * v_scale;
                fuv[2][0] = u_base + u0_norm * u_scale; fuv[2][1] = v_base + v0_norm * v_scale;
                fuv[3][0] = u_base + u1_norm * u_scale; fuv[3][1] = v_base + v1_norm * v_scale;
                fuv[4][0] = u_base + u0_norm * u_scale; fuv[4][1] = v_base + v0_norm * v_scale;
                fuv[5][0] = u_base + u0_norm * u_scale; fuv[5][1] = v_base + v1_norm * v_scale;
                break;
            case 2: // +Y (top)
                fuv[0][0] = u_base + u0_norm * u_scale; fuv[0][1] = v_base + v0_norm * v_scale;
                fuv[1][0] = u_base + u0_norm * u_scale; fuv[1][1] = v_base + v1_norm * v_scale;
                fuv[2][0] = u_base + u1_norm * u_scale; fuv[2][1] = v_base + v1_norm * v_scale;
                fuv[3][0] = u_base + u0_norm * u_scale; fuv[3][1] = v_base + v0_norm * v_scale;
                fuv[4][0] = u_base + u1_norm * u_scale; fuv[4][1] = v_base + v1_norm * v_scale;
                fuv[5][0] = u_base + u1_norm * u_scale; fuv[5][1] = v_base + v0_norm * v_scale;
                break;
            case 3: // -Y (bottom)
                fuv[0][0] = u_base + u0_norm * u_scale; fuv[0][1] = v_base + v1_norm * v_scale;
                fuv[1][0] = u_base + u0_norm * u_scale; fuv[1][1] = v_base + v0_norm * v_scale;
                fuv[2][0] = u_base + u1_norm * u_scale; fuv[2][1] = v_base + v0_norm * v_scale;
                fuv[3][0] = u_base + u0_norm * u_scale; fuv[3][1] = v_base + v1_norm * v_scale;
                fuv[4][0] = u_base + u1_norm * u_scale; fuv[4][1] = v_base + v0_norm * v_scale;
                fuv[5][0] = u_base + u1_norm * u_scale; fuv[5][1] = v_base + v1_norm * v_scale;
                break;
        }

        float corner_ao[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        for (int corner = 0; corner < 4; corner++) {
            corner_ao[corner] = calc_corner_ao(
                wx, wy, wz,
                face_dir[face_idx],
                face_u[face_idx],
                face_v[face_idx],
                corner_u_sign[corner],
                corner_v_sign[corner]
            );
        }

        for (int v = 0; v < 6; v++) {
            vertices.push_back(bx + fv[v][0]);
            vertices.push_back(by + fv[v][1]);
            vertices.push_back(bz + fv[v][2]);
            
            texcoords.push_back(fuv[v][0]);
            texcoords.push_back(fuv[v][1]);

            const int c = tri_corner_idx[v];
            const float ao = corner_ao[c];

            texcoords2.push_back(foliageMask);
            texcoords2.push_back(ao);
            
            normals.push_back(face_normals[face_idx][0]);
            normals.push_back(face_normals[face_idx][1]);
            normals.push_back(face_normals[face_idx][2]);

            colors.push_back(tint.r);
            colors.push_back(tint.g);
            colors.push_back(tint.b);
            colors.push_back(255);
        }
    };
    
    // Lambda for rendering cross-shaped vegetation (tall grass, flowers)
    // Creates two diagonal quads forming an X shape when viewed from above
    auto add_cross_model = [&](
        float bx, float by, float bz,
        BlockType block_type,
        float foliageMask,
        Color tint
    ) {
        Rectangle tex_rect = registry.get_texture_rect(block_type, 0);
        float u0 = tex_rect.x / atlas_size;
        float v0 = tex_rect.y / atlas_size;
        
        // Cross vertices: two diagonal planes at 45 degrees
        // Plane 1: from (0,0,0) to (1,1,1) diagonal
        // Plane 2: from (1,0,0) to (0,1,1) diagonal
        
        // Offset to center the cross slightly for visual appeal
        const float offset = 0.15f;  // Small offset from block edges
        
        // Diagonal plane 1 (NW-SE): from (offset, 0, offset) to (1-offset, 1, 1-offset)
        // Front face
        float cross1_verts[6][3] = {
            {offset, 0.0f, offset},
            {offset, 1.0f, offset},
            {1.0f - offset, 1.0f, 1.0f - offset},
            {offset, 0.0f, offset},
            {1.0f - offset, 1.0f, 1.0f - offset},
            {1.0f - offset, 0.0f, 1.0f - offset}
        };
        float cross1_uvs[6][2] = {
            {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f},
            {0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}
        };
        // Normal pointing perpendicular to diagonal (normalized (-1, 0, 1))
        float cross1_normal[3] = {-0.707f, 0.0f, 0.707f};
        
        // Back face of plane 1
        float cross1b_verts[6][3] = {
            {1.0f - offset, 0.0f, 1.0f - offset},
            {1.0f - offset, 1.0f, 1.0f - offset},
            {offset, 1.0f, offset},
            {1.0f - offset, 0.0f, 1.0f - offset},
            {offset, 1.0f, offset},
            {offset, 0.0f, offset}
        };
        float cross1b_normal[3] = {0.707f, 0.0f, -0.707f};
        
        // Diagonal plane 2 (NE-SW): from (1-offset, 0, offset) to (offset, 1, 1-offset)
        // Front face
        float cross2_verts[6][3] = {
            {1.0f - offset, 0.0f, offset},
            {1.0f - offset, 1.0f, offset},
            {offset, 1.0f, 1.0f - offset},
            {1.0f - offset, 0.0f, offset},
            {offset, 1.0f, 1.0f - offset},
            {offset, 0.0f, 1.0f - offset}
        };
        float cross2_uvs[6][2] = {
            {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f},
            {0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}
        };
        float cross2_normal[3] = {0.707f, 0.0f, 0.707f};
        
        // Back face of plane 2
        float cross2b_verts[6][3] = {
            {offset, 0.0f, 1.0f - offset},
            {offset, 1.0f, 1.0f - offset},
            {1.0f - offset, 1.0f, offset},
            {offset, 0.0f, 1.0f - offset},
            {1.0f - offset, 1.0f, offset},
            {1.0f - offset, 0.0f, offset}
        };
        float cross2b_normal[3] = {-0.707f, 0.0f, -0.707f};
        
        auto emit_cross_face = [&](float verts[6][3], float uvs[6][2], float normal[3]) {
            for (int v = 0; v < 6; v++) {
                vertices.push_back(bx + verts[v][0]);
                vertices.push_back(by + verts[v][1]);
                vertices.push_back(bz + verts[v][2]);
                
                texcoords.push_back(u0 + uvs[v][0] * uv_size);
                texcoords.push_back(v0 + uvs[v][1] * uv_size);
                
                // No AO for cross models (they're transparent)
                texcoords2.push_back(foliageMask);
                texcoords2.push_back(1.0f);  // Full brightness (no AO)
                
                normals.push_back(normal[0]);
                normals.push_back(normal[1]);
                normals.push_back(normal[2]);
                
                colors.push_back(tint.r);
                colors.push_back(tint.g);
                colors.push_back(tint.b);
                colors.push_back(255);
            }
        };
        
        // Emit all 4 faces (2 planes, front and back each)
        emit_cross_face(cross1_verts, cross1_uvs, cross1_normal);
        emit_cross_face(cross1b_verts, cross1_uvs, cross1b_normal);
        emit_cross_face(cross2_verts, cross2_uvs, cross2_normal);
        emit_cross_face(cross2b_verts, cross2_uvs, cross2b_normal);
    };
    
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            for (int x = 0; x < CHUNK_WIDTH; x++) {
                Block block = get_block(x, y, z);
                if (block == static_cast<Block>(BlockType::Air)) continue;
                
                auto block_type = static_cast<BlockType>(block);

                if (block_type == BlockType::Light) {
                    light_markers_ws_.push_back(Vector3{
                        world_position_.x + static_cast<float>(x) + 0.5f,
                        static_cast<float>(y) + 0.5f,
                        world_position_.z + static_cast<float>(z) + 0.5f,
                    });
                    continue;
                }
                
                const int wx = chunk_x_ * CHUNK_WIDTH + x;
                const int wy = y;
                const int wz = chunk_z_ * CHUNK_DEPTH + z;
                
                float bx = world_position_.x + x;
                float by = static_cast<float>(y);
                float bz = world_position_.z + z;
                
                // Handle vegetation (cross-shaped blocks like tall grass, flowers)
                if (shared::voxel::is_vegetation(block_type)) {
                    // Vegetation uses foliage tint for tall grass, white for flowers
                    const float foliageMask = (block_type == BlockType::TallGrass) ? 1.0f : 0.0f;
                    const Color tint = (foliageMask > 0.5f) ? grass_tint : WHITE;
                    add_cross_model(bx, by, bz, block_type, foliageMask, tint);
                    continue;
                }
                
                const auto* block_model = BlockModelLoader::instance().get_model(block_type);
                
                auto block_state = get_block_state(x, y, z);
                
                const float baseFoliageMask =
                    (block_type == BlockType::Leaves) ? 1.0f : 0.0f;
                const Color baseTint = (baseFoliageMask > 0.5f) ? foliage_tint : WHITE;
                
                if (shared::voxel::is_fence(block_type)) {
                    auto fence_elements = shared::voxel::models::make_fence_elements(
                        block_state.north, block_state.south, 
                        block_state.east, block_state.west);
                    
                    for (const auto& elem : fence_elements) {
                        for (int face = 0; face < 6; face++) {
                            if (!elem.faceEnabled[face]) continue;
                            
                            const int nwx = wx + face_dir[face][0];
                            const int nwy = wy + face_dir[face][1];
                            const int nwz = wz + face_dir[face][2];
                            
                            if (elem.faces[face].cullface && should_cull_model_face(nwx, nwy, nwz, face)) {
                                continue;
                            }
                            
                            add_element_face(bx, by, bz, elem, face, block_type, wx, wy, wz, 0.0f, WHITE);
                        }
                    }
                    continue;
                }
                
                if (shared::voxel::is_slab(block_type)) {
                    auto slab_elem = shared::voxel::models::make_slab_element(block_state.slabType);
                    
                    for (int face = 0; face < 6; face++) {
                        if (!slab_elem.faceEnabled[face]) continue;
                        
                        const int nwx = wx + face_dir[face][0];
                        const int nwy = wy + face_dir[face][1];
                        const int nwz = wz + face_dir[face][2];
                        
                        if (slab_elem.faces[face].cullface && should_cull_model_face(nwx, nwy, nwz, face)) {
                            continue;
                        }
                        
                        add_element_face(bx, by, bz, slab_elem, face, block_type, wx, wy, wz, 0.0f, WHITE);
                    }
                    continue;
                }
                
                if (block_model && block_model->has_elements() && 
                    block_model->shape != shared::voxel::BlockShape::Full) {
                    
                    for (const auto& elem : block_model->elements) {
                        for (int face = 0; face < 6; face++) {
                            if (!elem.faceEnabled[face]) continue;
                            
                            const int nwx = wx + face_dir[face][0];
                            const int nwy = wy + face_dir[face][1];
                            const int nwz = wz + face_dir[face][2];
                            
                            if (elem.faces[face].cullface && should_cull_model_face(nwx, nwy, nwz, face)) {
                                continue;
                            }
                            
                            float foliageMask = baseFoliageMask;
                            if (block_type == BlockType::Grass && face == 2) {
                                foliageMask = 1.0f;
                            }
                            
                            Color tint = WHITE;
                            if (foliageMask > 0.5f) {
                                tint = (block_type == BlockType::Grass) ? grass_tint : foliage_tint;
                            }
                            
                            add_element_face(bx, by, bz, elem, face, block_type, wx, wy, wz, foliageMask, tint);
                        }
                    }
                } else {
                    for (int face = 0; face < 6; face++) {
                        const int nwx = wx + face_dir[face][0];
                        const int nwy = wy + face_dir[face][1];
                        const int nwz = wz + face_dir[face][2];

                        Block neighbor = world.get_block(nwx, nwy, nwz);
                        if (!is_transparent(static_cast<BlockType>(neighbor))) continue;
                        
                        Rectangle tex_rect = registry.get_texture_rect(block_type, face);
                        float u0 = tex_rect.x / atlas_size;
                        float v0 = tex_rect.y / atlas_size;

                        const float foliageMask =
                            (block_type == BlockType::Leaves) ? 1.0f :
                            (block_type == BlockType::Grass && face == 2) ? 1.0f :
                            0.0f;

                        float corner_ao[4];
                        for (int corner = 0; corner < 4; corner++) {
                            corner_ao[corner] = calc_corner_ao(
                                wx, wy, wz,
                                face_dir[face],
                                face_u[face],
                                face_v[face],
                                corner_u_sign[corner],
                                corner_v_sign[corner]
                            );
                        }
                        
                        for (int v = 0; v < 6; v++) {
                            vertices.push_back(bx + face_vertices[face][v][0]);
                            vertices.push_back(by + face_vertices[face][v][1]);
                            vertices.push_back(bz + face_vertices[face][v][2]);
                            
                            texcoords.push_back(u0 + face_uvs[face][v][0] * uv_size);
                            texcoords.push_back(v0 + face_uvs[face][v][1] * uv_size);

                            const int c = tri_corner_idx[v];
                            const float ao = corner_ao[c];

                            texcoords2.push_back(foliageMask);
                            texcoords2.push_back(ao);
                            
                            normals.push_back(face_normals[face][0]);
                            normals.push_back(face_normals[face][1]);
                            normals.push_back(face_normals[face][2]);

                            unsigned char r = 255;
                            unsigned char g = 255;
                            unsigned char b = 255;

                            if (foliageMask > 0.5f) {
                                const Color tint = (block_type == BlockType::Grass) ? grass_tint : foliage_tint;
                                r = tint.r;
                                g = tint.g;
                                b = tint.b;
                            }

                            colors.push_back(r);
                            colors.push_back(g);
                            colors.push_back(b);
                            colors.push_back(255);
                        }
                    }
                }
            }
        }
    }
    
    if (vertices.empty()) {
        has_mesh_ = false;
        needs_mesh_update_ = false;

        const auto t_total1 = std::chrono::steady_clock::now();
        const float total_ms = std::chrono::duration<float, std::milli>(t_total1 - t_total0).count();
        const auto& prof = core::Config::instance().profiling();
        if (prof.enabled && prof.chunk_mesh) {
            static double last_log_s = 0.0;
            const double now_s = GetTime();
            const bool interval_ok = prof.log_every_event || ((now_s - last_log_s) * 1000.0 >= static_cast<double>(std::max(0, prof.log_interval_ms)));
            if (total_ms >= prof.warn_chunk_mesh_ms && interval_ok) {
                TraceLog(LOG_INFO, "[prof] chunk mesh (empty): %.2f ms (chunk=%d,%d)", total_ms, chunk_x_, chunk_z_);
                last_log_s = now_s;
            }
        }
        return;
    }
    
    TraceLog(LOG_DEBUG, "Chunk (%d, %d) mesh: %zu vertices", chunk_x_, chunk_z_, vertices.size() / 3);
    
    mesh_ = {0};
    mesh_.vertexCount = static_cast<int>(vertices.size() / 3);
    mesh_.triangleCount = mesh_.vertexCount / 3;
    
    mesh_.vertices = static_cast<float*>(RL_MALLOC(vertices.size() * sizeof(float)));
    mesh_.texcoords = static_cast<float*>(RL_MALLOC(texcoords.size() * sizeof(float)));
    mesh_.texcoords2 = static_cast<float*>(RL_MALLOC(texcoords2.size() * sizeof(float)));
    mesh_.normals = static_cast<float*>(RL_MALLOC(normals.size() * sizeof(float)));
    mesh_.colors = static_cast<unsigned char*>(RL_MALLOC(colors.size() * sizeof(unsigned char)));
    
    std::memcpy(mesh_.vertices, vertices.data(), vertices.size() * sizeof(float));
    std::memcpy(mesh_.texcoords, texcoords.data(), texcoords.size() * sizeof(float));
    std::memcpy(mesh_.texcoords2, texcoords2.data(), texcoords2.size() * sizeof(float));
    std::memcpy(mesh_.normals, normals.data(), normals.size() * sizeof(float));
    std::memcpy(mesh_.colors, colors.data(), colors.size() * sizeof(unsigned char));
    
    const auto t_up0 = std::chrono::steady_clock::now();
    UploadMesh(&mesh_, false);
    const auto t_up1 = std::chrono::steady_clock::now();

    const float upload_ms = std::chrono::duration<float, std::milli>(t_up1 - t_up0).count();
    {
        const auto& prof = core::Config::instance().profiling();
        if (prof.enabled && prof.upload_mesh) {
            static double last_log_s_upload = 0.0;
            const double now_s = GetTime();
            const bool interval_ok = prof.log_every_event || ((now_s - last_log_s_upload) * 1000.0 >= static_cast<double>(std::max(0, prof.log_interval_ms)));
            if (upload_ms >= prof.warn_upload_mesh_ms && interval_ok) {
                TraceLog(LOG_INFO, "[prof] UploadMesh: %.2f ms (chunk=%d,%d, vtx=%d)", upload_ms, chunk_x_, chunk_z_, mesh_.vertexCount);
                last_log_s_upload = now_s;
            }
        }
    }
    
    model_ = LoadModelFromMesh(mesh_);
    model_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = registry.get_atlas_texture();
    
    has_mesh_ = true;
    needs_mesh_update_ = false;

    const auto t_total1 = std::chrono::steady_clock::now();
    const float total_ms = std::chrono::duration<float, std::milli>(t_total1 - t_total0).count();
    {
        const auto& prof = core::Config::instance().profiling();
        if (prof.enabled && prof.chunk_mesh) {
            static double last_log_s_total = 0.0;
            const double now_s = GetTime();
            const bool interval_ok = prof.log_every_event || ((now_s - last_log_s_total) * 1000.0 >= static_cast<double>(std::max(0, prof.log_interval_ms)));
            if (total_ms >= prof.warn_chunk_mesh_ms && interval_ok) {
                TraceLog(LOG_INFO, "[prof] chunk mesh: %.2f ms (chunk=%d,%d, vtx=%d)", total_ms, chunk_x_, chunk_z_, mesh_.vertexCount);
                last_log_s_total = now_s;
            }
        }
    }
}

void Chunk::render() const {
    if (has_mesh_) {
        DrawModel(model_, {0, 0, 0}, 1.0f, WHITE);
    }

    if (!light_markers_ws_.empty()) {
        for (const auto& p : light_markers_ws_) {
            DrawSphere(p, 0.18f, YELLOW);
        }
    }
}

void Chunk::render(Shader shader) const {
    if (has_mesh_) {
        Model model_copy = model_;
        model_copy.materials[0].shader = shader;
        DrawModel(model_copy, {0, 0, 0}, 1.0f, WHITE);
    }

    if (!light_markers_ws_.empty()) {
        for (const auto& p : light_markers_ws_) {
            DrawSphere(p, 0.18f, YELLOW);
        }
    }
}

} // namespace voxel
