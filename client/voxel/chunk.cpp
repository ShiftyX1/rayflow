#include "chunk.hpp"
#include "block_registry.hpp"
#include "../renderer/lighting_raymarch.hpp"
#include <raylib.h>
#include <cstring>
#include <vector>
#include <cstdio>

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

void Chunk::generate_mesh() {
    cleanup_mesh();
    
    std::vector<float> vertices;
    std::vector<float> texcoords;
    std::vector<float> normals;
    
    auto& registry = BlockRegistry::instance();
    float atlas_size = static_cast<float>(registry.get_atlas_texture().width);
    float tile_size = 16.0f;
    float uv_size = tile_size / atlas_size;
    
    // Face vertex offsets (2 triangles = 6 vertices per face)
    // Each face has 4 corners, we make 2 triangles (clockwise winding)
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
    
    // UV coordinates for each vertex of each face (6 vertices per face)
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
    
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            for (int x = 0; x < CHUNK_WIDTH; x++) {
                Block block = get_block(x, y, z);
                if (block == static_cast<Block>(BlockType::Air)) continue;
                
                auto block_type = static_cast<BlockType>(block);
                
                // Check each face
                int neighbors[6][3] = {
                    {x+1, y, z}, {x-1, y, z},
                    {x, y+1, z}, {x, y-1, z},
                    {x, y, z+1}, {x, y, z-1}
                };
                
                for (int face = 0; face < 6; face++) {
                    int nx = neighbors[face][0];
                    int ny = neighbors[face][1];
                    int nz = neighbors[face][2];
                    
                    Block neighbor = get_block(nx, ny, nz);
                    if (!is_transparent(static_cast<BlockType>(neighbor))) continue;
                    
                    // World position of this block
                    float bx = world_position_.x + x;
                    float by = static_cast<float>(y);
                    float bz = world_position_.z + z;
                    
                    // Get texture UV from registry
                    Rectangle tex_rect = registry.get_texture_rect(block_type, face);
                    float u0 = tex_rect.x / atlas_size;
                    float v0 = tex_rect.y / atlas_size;
                    
                    // Add 6 vertices for this face (2 triangles)
                    for (int v = 0; v < 6; v++) {
                        vertices.push_back(bx + face_vertices[face][v][0]);
                        vertices.push_back(by + face_vertices[face][v][1]);
                        vertices.push_back(bz + face_vertices[face][v][2]);
                        
                        texcoords.push_back(u0 + face_uvs[face][v][0] * uv_size);
                        texcoords.push_back(v0 + face_uvs[face][v][1] * uv_size);
                        
                        normals.push_back(face_normals[face][0]);
                        normals.push_back(face_normals[face][1]);
                        normals.push_back(face_normals[face][2]);
                    }
                }
            }
        }
    }
    
    if (vertices.empty()) {
        has_mesh_ = false;
        needs_mesh_update_ = false;
        return;
    }
    
    TraceLog(LOG_DEBUG, "Chunk (%d, %d) mesh: %zu vertices", chunk_x_, chunk_z_, vertices.size() / 3);
    
    // Build mesh from collected data
    mesh_ = {0};
    mesh_.vertexCount = static_cast<int>(vertices.size() / 3);
    mesh_.triangleCount = mesh_.vertexCount / 3;
    
    mesh_.vertices = static_cast<float*>(RL_MALLOC(vertices.size() * sizeof(float)));
    mesh_.texcoords = static_cast<float*>(RL_MALLOC(texcoords.size() * sizeof(float)));
    mesh_.normals = static_cast<float*>(RL_MALLOC(normals.size() * sizeof(float)));
    
    std::memcpy(mesh_.vertices, vertices.data(), vertices.size() * sizeof(float));
    std::memcpy(mesh_.texcoords, texcoords.data(), texcoords.size() * sizeof(float));
    std::memcpy(mesh_.normals, normals.data(), normals.size() * sizeof(float));
    
    UploadMesh(&mesh_, false);
    
    model_ = LoadModelFromMesh(mesh_);
    model_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = registry.get_atlas_texture();

    // Bind the client-only lighting shader (safe even if disabled; the shader itself gates behavior).
    if (renderer::LightingRaymarch::instance().ready()) {
        model_.materials[0].shader = renderer::LightingRaymarch::instance().shader();
    }
    
    has_mesh_ = true;
    needs_mesh_update_ = false;
}

void Chunk::render() const {
    if (has_mesh_) {
        DrawModel(model_, {0, 0, 0}, 1.0f, WHITE);
    }
}

} // namespace voxel
