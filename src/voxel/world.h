#ifndef WORLD_H
#define WORLD_H

#include "voxel.h"
#include <raylib.h>

#define CHUNK_HASH_SIZE 1024
#define MAX_LOADED_CHUNKS 512
#define CHUNK_UNLOAD_DISTANCE 12

typedef struct ChunkNode {
    Chunk* chunk;
    struct ChunkNode* next;
} ChunkNode;

typedef struct {
    ChunkNode* chunk_hash[CHUNK_HASH_SIZE];
    int chunk_count;
    int render_distance;
    unsigned int seed;
    Vector3 last_player_pos;
} World;

World* world_create(unsigned int seed);
void world_destroy(World* world);
void world_update(World* world, Vector3 player_pos);
void world_render(World* world, Camera3D camera);
Chunk* world_get_chunk(World* world, int chunk_x, int chunk_z);
Chunk* world_get_or_create_chunk(World* world, int chunk_x, int chunk_z);
void world_add_chunk(World* world, Chunk* chunk);
void world_remove_chunk(World* world, int chunk_x, int chunk_z);
void world_unload_distant_chunks(World* world, Vector3 player_pos);
void world_generate_chunk_terrain(World* world, Chunk* chunk);
int world_hash_coords(int chunk_x, int chunk_z);

#endif // WORLD_H
