#include "chunk.hpp"
#include "block_registry.hpp"
#include "world.hpp"
#include "../core/config.hpp"
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

void Chunk::generate_mesh(const World& world) {
    const auto t_total0 = std::chrono::steady_clock::now();

    // MV-2/MV-3: foliage/grass recolor is temperature/humidity-driven (render-only).
    // We use vertexColor as a tint (raylib default shader behavior), so apply the tint here.
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

    static const int face_dir[6][3] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 1, 0}, { 0,-1, 0},
        { 0, 0, 1}, { 0, 0,-1}
    };

    // Face-local axes used for Minecraft-style smooth lighting/AO per corner.
    // Corner order matches face_vertices quad corners (0..3) used by tri_corner_idx.
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

    // Minecraft-style per-vertex AO calculation.
    // For each corner of a face, sample 3 neighbors: side1, side2, corner.
    // AO level = 0 (darkest) to 3 (brightest).
    // Returns float in [0,1] range for shader use.
    auto calc_corner_ao = [&world](int wx, int wy, int wz,
                                    const int* dir,
                                    const int* u_axis,
                                    const int* v_axis,
                                    int u_sign, int v_sign) -> float {
        // Position of the 3 neighbor blocks that affect this corner
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

        // Minecraft AO formula:
        // If both sides are solid, corner doesn't matter (fully occluded)
        int ao_level;
        if (s1 && s2) {
            ao_level = 0;
        } else {
            ao_level = 3 - (s1 ? 1 : 0) - (s2 ? 1 : 0) - (c ? 1 : 0);
        }

        // Convert to [0,1] range with a minimum brightness
        // ao_level 0 -> 0.2, ao_level 3 -> 1.0
        static const float ao_values[4] = { 0.2f, 0.5f, 0.75f, 1.0f };
        return ao_values[ao_level];
    };

    // Corner u/v signs for each of the 4 corners of a quad
    // Corners: 0=(-u,-v), 1=(-u,+v), 2=(+u,+v), 3=(+u,-v)
    static const int corner_u_sign[4] = { -1, -1, +1, +1 };
    static const int corner_v_sign[4] = { -1, +1, +1, -1 };
    
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            for (int x = 0; x < CHUNK_WIDTH; x++) {
                Block block = get_block(x, y, z);
                if (block == static_cast<Block>(BlockType::Air)) continue;
                
                auto block_type = static_cast<BlockType>(block);

                // LS-1: Light is a marker-only block (no cube mesh).
                if (block_type == BlockType::Light) {
                    light_markers_ws_.push_back(Vector3{
                        world_position_.x + static_cast<float>(x) + 0.5f,
                        static_cast<float>(y) + 0.5f,
                        world_position_.z + static_cast<float>(z) + 0.5f,
                    });
                    continue;
                }
                
                // Check each face
                const int wx = chunk_x_ * CHUNK_WIDTH + x;
                const int wy = y;
                const int wz = chunk_z_ * CHUNK_DEPTH + z;
                
                for (int face = 0; face < 6; face++) {
                    const int nwx = wx + face_dir[face][0];
                    const int nwy = wy + face_dir[face][1];
                    const int nwz = wz + face_dir[face][2];

                    Block neighbor = world.get_block(nwx, nwy, nwz);
                    if (!is_transparent(static_cast<BlockType>(neighbor))) continue;
                    
                    // World position of this block
                    float bx = world_position_.x + x;
                    float by = static_cast<float>(y);
                    float bz = world_position_.z + z;
                    
                    // Get texture UV from registry
                    Rectangle tex_rect = registry.get_texture_rect(block_type, face);
                    float u0 = tex_rect.x / atlas_size;
                    float v0 = tex_rect.y / atlas_size;

                    // MV-2: foliage/grass recolor mask.
                    // - Leaves: all faces
                    // - Grass: top face only (+Y)
                    const float foliageMask =
                        (block_type == BlockType::Leaves) ? 1.0f :
                        (block_type == BlockType::Grass && face == 2) ? 1.0f :
                        0.0f;

                    // Pre-compute AO for all 4 corners of this face
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
                    
                    // Add 6 vertices for this face (2 triangles)
                    for (int v = 0; v < 6; v++) {
                        vertices.push_back(bx + face_vertices[face][v][0]);
                        vertices.push_back(by + face_vertices[face][v][1]);
                        vertices.push_back(bz + face_vertices[face][v][2]);
                        
                        texcoords.push_back(u0 + face_uvs[face][v][0] * uv_size);
                        texcoords.push_back(v0 + face_uvs[face][v][1] * uv_size);

                        // Get AO for this vertex's corner
                        const int c = tri_corner_idx[v];
                        const float ao = corner_ao[c];

                        texcoords2.push_back(foliageMask);
                        texcoords2.push_back(ao);
                        
                        normals.push_back(face_normals[face][0]);
                        normals.push_back(face_normals[face][1]);
                        normals.push_back(face_normals[face][2]);

                        // Use vertex color for tint (foliage/grass)
                        unsigned char r = 255;
                        unsigned char g = 255;
                        unsigned char b = 255;

                        // MV-2: apply temperature tint only to foliage/grass faces.
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
    
    // Build mesh from collected data
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
        // Temporarily set shader on model's material
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
