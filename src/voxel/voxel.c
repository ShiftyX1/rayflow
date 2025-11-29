#include "voxel.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Get voxel index in the array
int chunk_get_voxel_index(int x, int y, int z) {
    if (x < 0 || x >= CHUNK_WIDTH || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) {
        return -1;
    }
    return x + CHUNK_WIDTH * (y + CHUNK_HEIGHT * z);
}

// Get voxel at position
Voxel voxel_get(Chunk* chunk, int x, int y, int z) {
    int index = chunk_get_voxel_index(x, y, z);
    if (index < 0) return BLOCK_AIR;
    return chunk->voxels[index];
}

// Set voxel at position
void voxel_set(Chunk* chunk, int x, int y, int z, Voxel type) {
    int index = chunk_get_voxel_index(x, y, z);
    if (index >= 0) {
        chunk->voxels[index] = type;
        chunk->needs_mesh_update = true;
    }
}

// Check if voxel is solid
bool voxel_is_solid(Voxel type) {
    BlockProperties* props = block_registry_get((BlockType)type);
    return props->is_solid;
}

// Check if voxel is transparent
bool voxel_is_transparent(Voxel type) {
    BlockProperties* props = block_registry_get((BlockType)type);
    return props->is_transparent;
}

// Create chunk
Chunk* chunk_create(int chunk_x, int chunk_z) {
    Chunk* chunk = (Chunk*)malloc(sizeof(Chunk));
    if (!chunk) return NULL;
    
    memset(chunk->voxels, BLOCK_AIR, CHUNK_SIZE);
    chunk->world_position = (Vector3){
        (float)(chunk_x * CHUNK_WIDTH),
        0.0f,
        (float)(chunk_z * CHUNK_DEPTH)
    };
    chunk->chunk_x = chunk_x;
    chunk->chunk_z = chunk_z;
    chunk->needs_mesh_update = true;
    chunk->is_generated = false;
    chunk->has_mesh = false;
    
    return chunk;
}

// Destroy chunk
void chunk_destroy(Chunk* chunk) {
    if (!chunk) return;
    
    if (chunk->has_mesh) {
        UnloadModel(chunk->model);
        chunk->has_mesh = false;
    }
    
    free(chunk);
}

// Texture coordinates for each voxel type (atlas coordinates)
typedef struct {
    float u1, v1; // Top-left
    float u2, v2; // Bottom-right
} TexCoords;

// Get texture coordinates for a voxel face using block registry
TexCoords get_voxel_tex_coords(VoxelType type, int face) {
    TexCoords coords;
    
    TextureUV uv = block_registry_get_texture_uv((BlockType)type, face);
    
    coords.u1 = uv.u_min;
    coords.v1 = uv.v_min;
    coords.u2 = uv.u_max;
    coords.v2 = uv.v_max;
    
    return coords;
}

// Generate mesh for chunk
void chunk_generate_mesh(Chunk* chunk) {
    if (!chunk) return;
    
    // Free old mesh if exists
    if (chunk->has_mesh) {
        chunk->has_mesh = false;
        UnloadModel(chunk->model);
    }
    
    int face_count = 0;
    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int y = 0; y < CHUNK_HEIGHT; y++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                Voxel voxel = voxel_get(chunk, x, y, z);
                if (voxel == BLOCK_AIR) continue;
                
                // Check each face if it should be rendered
                if (voxel_is_transparent(voxel_get(chunk, x, y + 1, z))) face_count++; // Top
                if (voxel_is_transparent(voxel_get(chunk, x, y - 1, z))) face_count++; // Bottom
                if (voxel_is_transparent(voxel_get(chunk, x, y, z + 1))) face_count++; // Front
                if (voxel_is_transparent(voxel_get(chunk, x, y, z - 1))) face_count++; // Back
                if (voxel_is_transparent(voxel_get(chunk, x - 1, y, z))) face_count++; // Left
                if (voxel_is_transparent(voxel_get(chunk, x + 1, y, z))) face_count++; // Right
            }
        }
    }
    
    if (face_count == 0) {
        chunk->has_mesh = false;
        return;
    }
    
    // Allocate mesh data
    int vertex_count = face_count * 4;
    int triangle_count = face_count * 2;
    
    float* vertices = (float*)malloc(vertex_count * 3 * sizeof(float));
    float* texcoords = (float*)malloc(vertex_count * 2 * sizeof(float));
    float* normals = (float*)malloc(vertex_count * 3 * sizeof(float));
    unsigned short* indices = (unsigned short*)malloc(triangle_count * 3 * sizeof(unsigned short));
    
    int v_index = 0;
    int t_index = 0;
    int i_index = 0;
    
    // Build mesh
    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int y = 0; y < CHUNK_HEIGHT; y++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                Voxel voxel = voxel_get(chunk, x, y, z);
                if (voxel == BLOCK_AIR) continue;
                
                float x0 = (float)x;
                float y0 = (float)y;
                float z0 = (float)z;
                float x1 = x0 + 1.0f;
                float y1 = y0 + 1.0f;
                float z1 = z0 + 1.0f;
                
                // Top face
                if (voxel_is_transparent(voxel_get(chunk, x, y + 1, z))) {
                    TexCoords tex = get_voxel_tex_coords(voxel, 0);
                    
                    vertices[v_index++] = x0; vertices[v_index++] = y1; vertices[v_index++] = z1;
                    vertices[v_index++] = x1; vertices[v_index++] = y1; vertices[v_index++] = z1;
                    vertices[v_index++] = x1; vertices[v_index++] = y1; vertices[v_index++] = z0;
                    vertices[v_index++] = x0; vertices[v_index++] = y1; vertices[v_index++] = z0;
                    
                    texcoords[t_index++] = tex.u1; texcoords[t_index++] = tex.v2;
                    texcoords[t_index++] = tex.u2; texcoords[t_index++] = tex.v2;
                    texcoords[t_index++] = tex.u2; texcoords[t_index++] = tex.v1;
                    texcoords[t_index++] = tex.u1; texcoords[t_index++] = tex.v1;
                    
                    for (int n = 0; n < 4; n++) {
                        normals[v_index - 12 + n * 3] = 0;
                        normals[v_index - 12 + n * 3 + 1] = 1;
                        normals[v_index - 12 + n * 3 + 2] = 0;
                    }
                    
                    int base = (v_index / 3) - 4;
                    indices[i_index++] = base; indices[i_index++] = base + 1; indices[i_index++] = base + 2;
                    indices[i_index++] = base; indices[i_index++] = base + 2; indices[i_index++] = base + 3;
                }
                
                // Bottom face
                if (voxel_is_transparent(voxel_get(chunk, x, y - 1, z))) {
                    TexCoords tex = get_voxel_tex_coords(voxel, 1);
                    
                    vertices[v_index++] = x0; vertices[v_index++] = y0; vertices[v_index++] = z0;
                    vertices[v_index++] = x1; vertices[v_index++] = y0; vertices[v_index++] = z0;
                    vertices[v_index++] = x1; vertices[v_index++] = y0; vertices[v_index++] = z1;
                    vertices[v_index++] = x0; vertices[v_index++] = y0; vertices[v_index++] = z1;
                    
                    texcoords[t_index++] = tex.u1; texcoords[t_index++] = tex.v1;
                    texcoords[t_index++] = tex.u2; texcoords[t_index++] = tex.v1;
                    texcoords[t_index++] = tex.u2; texcoords[t_index++] = tex.v2;
                    texcoords[t_index++] = tex.u1; texcoords[t_index++] = tex.v2;
                    
                    for (int n = 0; n < 4; n++) {
                        normals[v_index - 12 + n * 3] = 0;
                        normals[v_index - 12 + n * 3 + 1] = -1;
                        normals[v_index - 12 + n * 3 + 2] = 0;
                    }
                    
                    int base = (v_index / 3) - 4;
                    indices[i_index++] = base; indices[i_index++] = base + 1; indices[i_index++] = base + 2;
                    indices[i_index++] = base; indices[i_index++] = base + 2; indices[i_index++] = base + 3;
                }
                
                // Front face (+Z)
                if (voxel_is_transparent(voxel_get(chunk, x, y, z + 1))) {
                    TexCoords tex = get_voxel_tex_coords(voxel, 2);
                    
                    vertices[v_index++] = x1; vertices[v_index++] = y0; vertices[v_index++] = z1;
                    vertices[v_index++] = x1; vertices[v_index++] = y1; vertices[v_index++] = z1;
                    vertices[v_index++] = x0; vertices[v_index++] = y1; vertices[v_index++] = z1;
                    vertices[v_index++] = x0; vertices[v_index++] = y0; vertices[v_index++] = z1;
                    
                    texcoords[t_index++] = tex.u2; texcoords[t_index++] = tex.v2;
                    texcoords[t_index++] = tex.u2; texcoords[t_index++] = tex.v1;
                    texcoords[t_index++] = tex.u1; texcoords[t_index++] = tex.v1;
                    texcoords[t_index++] = tex.u1; texcoords[t_index++] = tex.v2;
                    
                    for (int n = 0; n < 4; n++) {
                        normals[v_index - 12 + n * 3] = 0;
                        normals[v_index - 12 + n * 3 + 1] = 0;
                        normals[v_index - 12 + n * 3 + 2] = 1;
                    }
                    
                    int base = (v_index / 3) - 4;
                    indices[i_index++] = base; indices[i_index++] = base + 1; indices[i_index++] = base + 2;
                    indices[i_index++] = base; indices[i_index++] = base + 2; indices[i_index++] = base + 3;
                }
                
                // Back face (-Z)
                if (voxel_is_transparent(voxel_get(chunk, x, y, z - 1))) {
                    TexCoords tex = get_voxel_tex_coords(voxel, 3);
                    
                    vertices[v_index++] = x0; vertices[v_index++] = y0; vertices[v_index++] = z0;
                    vertices[v_index++] = x0; vertices[v_index++] = y1; vertices[v_index++] = z0;
                    vertices[v_index++] = x1; vertices[v_index++] = y1; vertices[v_index++] = z0;
                    vertices[v_index++] = x1; vertices[v_index++] = y0; vertices[v_index++] = z0;
                    
                    texcoords[t_index++] = tex.u1; texcoords[t_index++] = tex.v2;
                    texcoords[t_index++] = tex.u1; texcoords[t_index++] = tex.v1;
                    texcoords[t_index++] = tex.u2; texcoords[t_index++] = tex.v1;
                    texcoords[t_index++] = tex.u2; texcoords[t_index++] = tex.v2;
                    
                    for (int n = 0; n < 4; n++) {
                        normals[v_index - 12 + n * 3] = 0;
                        normals[v_index - 12 + n * 3 + 1] = 0;
                        normals[v_index - 12 + n * 3 + 2] = -1;
                    }
                    
                    int base = (v_index / 3) - 4;
                    indices[i_index++] = base; indices[i_index++] = base + 1; indices[i_index++] = base + 2;
                    indices[i_index++] = base; indices[i_index++] = base + 2; indices[i_index++] = base + 3;
                }
                
                // Left face (-X)
                if (voxel_is_transparent(voxel_get(chunk, x - 1, y, z))) {
                    TexCoords tex = get_voxel_tex_coords(voxel, 4);
                    
                    vertices[v_index++] = x0; vertices[v_index++] = y0; vertices[v_index++] = z1;
                    vertices[v_index++] = x0; vertices[v_index++] = y1; vertices[v_index++] = z1;
                    vertices[v_index++] = x0; vertices[v_index++] = y1; vertices[v_index++] = z0;
                    vertices[v_index++] = x0; vertices[v_index++] = y0; vertices[v_index++] = z0;
                    
                    texcoords[t_index++] = tex.u2; texcoords[t_index++] = tex.v2;
                    texcoords[t_index++] = tex.u2; texcoords[t_index++] = tex.v1;
                    texcoords[t_index++] = tex.u1; texcoords[t_index++] = tex.v1;
                    texcoords[t_index++] = tex.u1; texcoords[t_index++] = tex.v2;
                    
                    for (int n = 0; n < 4; n++) {
                        normals[v_index - 12 + n * 3] = -1;
                        normals[v_index - 12 + n * 3 + 1] = 0;
                        normals[v_index - 12 + n * 3 + 2] = 0;
                    }
                    
                    int base = (v_index / 3) - 4;
                    indices[i_index++] = base; indices[i_index++] = base + 1; indices[i_index++] = base + 2;
                    indices[i_index++] = base; indices[i_index++] = base + 2; indices[i_index++] = base + 3;
                }
                
                // Right face (+X)
                if (voxel_is_transparent(voxel_get(chunk, x + 1, y, z))) {
                    TexCoords tex = get_voxel_tex_coords(voxel, 5);
                    
                    vertices[v_index++] = x1; vertices[v_index++] = y0; vertices[v_index++] = z0;
                    vertices[v_index++] = x1; vertices[v_index++] = y1; vertices[v_index++] = z0;
                    vertices[v_index++] = x1; vertices[v_index++] = y1; vertices[v_index++] = z1;
                    vertices[v_index++] = x1; vertices[v_index++] = y0; vertices[v_index++] = z1;
                    
                    texcoords[t_index++] = tex.u1; texcoords[t_index++] = tex.v2;
                    texcoords[t_index++] = tex.u1; texcoords[t_index++] = tex.v1;
                    texcoords[t_index++] = tex.u2; texcoords[t_index++] = tex.v1;
                    texcoords[t_index++] = tex.u2; texcoords[t_index++] = tex.v2;
                    
                    for (int n = 0; n < 4; n++) {
                        normals[v_index - 12 + n * 3] = 1;
                        normals[v_index - 12 + n * 3 + 1] = 0;
                        normals[v_index - 12 + n * 3 + 2] = 0;
                    }
                    
                    int base = (v_index / 3) - 4;
                    indices[i_index++] = base; indices[i_index++] = base + 1; indices[i_index++] = base + 2;
                    indices[i_index++] = base; indices[i_index++] = base + 2; indices[i_index++] = base + 3;
                }
            }
        }
    }
    
    // Create mesh
    Mesh mesh = { 0 };
    mesh.vertexCount = vertex_count;
    mesh.triangleCount = triangle_count;
    mesh.vertices = vertices;
    mesh.texcoords = texcoords;
    mesh.normals = normals;
    mesh.indices = indices;
    
    UploadMesh(&mesh, false);
    
    chunk->mesh = mesh;
    chunk->model = LoadModelFromMesh(mesh);
    
    // Get texture from the global block registry
    if (g_block_registry.atlas) {
        chunk->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = g_block_registry.atlas->texture;
    }
    
    chunk->has_mesh = true;
    chunk->needs_mesh_update = false;
}
