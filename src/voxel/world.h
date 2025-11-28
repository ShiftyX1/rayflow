#ifndef WORLD_H
#define WORLD_H

#include "voxel.h"
#include <raylib.h>

#define MAX_CHUNKS 256

// World structure
typedef struct {
    Chunk* chunks[MAX_CHUNKS];
    int chunk_count;
    int render_distance;
    unsigned int seed;
} World;

// World functions
World* world_create(unsigned int seed);
void world_destroy(World* world);
void world_update(World* world, Vector3 player_pos);
void world_render(World* world, Camera3D camera);
Chunk* world_get_chunk(World* world, int chunk_x, int chunk_z);
void world_generate_chunk_terrain(World* world, Chunk* chunk);

#endif // WORLD_H
