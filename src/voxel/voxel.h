#ifndef VOXEL_H
#define VOXEL_H

#include <stdint.h>
#include <stdbool.h>
#include <raylib.h>
#include "block_registry.h"

// Используем типы блоков из реестра
typedef BlockType VoxelType;

// Single voxel
typedef uint8_t Voxel;

// Chunk dimensions
#define CHUNK_WIDTH 16
#define CHUNK_HEIGHT 256
#define CHUNK_DEPTH 16
#define CHUNK_SIZE (CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH)

// Chunk structure
typedef struct {
    Voxel voxels[CHUNK_SIZE];
    Vector3 world_position;
    int chunk_x;
    int chunk_z;
    bool needs_mesh_update;
    bool is_generated;
    Mesh mesh;
    Model model;
    bool has_mesh;
} Chunk;

// Voxel functions
Voxel voxel_get(Chunk* chunk, int x, int y, int z);
void voxel_set(Chunk* chunk, int x, int y, int z, Voxel type);
bool voxel_is_solid(Voxel type);
bool voxel_is_transparent(Voxel type);

// Chunk functions
Chunk* chunk_create(int chunk_x, int chunk_z);
void chunk_destroy(Chunk* chunk);
void chunk_generate_mesh(Chunk* chunk); // Теперь использует глобальный реестр
int chunk_get_voxel_index(int x, int y, int z);

#endif // VOXEL_H
