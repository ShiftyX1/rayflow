#include "world.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// Simple Perlin noise implementation
static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

static float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

static float grad(int hash, float x, float y) {
    int h = hash & 7;
    float u = h < 4 ? x : y;
    float v = h < 4 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

// Permutation table for Perlin noise
static unsigned char perm[512];
static bool perm_initialized = false;

static void init_perlin(unsigned int seed) {
    if (perm_initialized) return;
    
    // Initialize with seed
    srand(seed);
    for (int i = 0; i < 256; i++) {
        perm[i] = i;
    }
    
    // Shuffle
    for (int i = 255; i > 0; i--) {
        int j = rand() % (i + 1);
        unsigned char temp = perm[i];
        perm[i] = perm[j];
        perm[j] = temp;
    }
    
    // Duplicate
    for (int i = 0; i < 256; i++) {
        perm[256 + i] = perm[i];
    }
    
    perm_initialized = true;
}

static float perlin_noise(float x, float y) {
    int X = (int)floor(x) & 255;
    int Y = (int)floor(y) & 255;
    
    x -= floor(x);
    y -= floor(y);
    
    float u = fade(x);
    float v = fade(y);
    
    int a = perm[X] + Y;
    int b = perm[X + 1] + Y;
    
    return lerp(
        lerp(grad(perm[a], x, y), grad(perm[b], x - 1, y), u),
        lerp(grad(perm[a + 1], x, y - 1), grad(perm[b + 1], x - 1, y - 1), u),
        v
    );
}

// Multi-octave Perlin noise
static float octave_perlin(float x, float y, int octaves, float persistence) {
    float total = 0;
    float frequency = 1;
    float amplitude = 1;
    float max_value = 0;
    
    for (int i = 0; i < octaves; i++) {
        total += perlin_noise(x * frequency, y * frequency) * amplitude;
        max_value += amplitude;
        amplitude *= persistence;
        frequency *= 2;
    }
    
    return total / max_value;
}

int world_hash_coords(int chunk_x, int chunk_z) {
    unsigned int hash = (unsigned int)chunk_x * 73856093 ^ (unsigned int)chunk_z * 19349663;
    return hash % CHUNK_HASH_SIZE;
}

// Create world
World* world_create(unsigned int seed) {
    World* world = (World*)malloc(sizeof(World));
    if (!world) return NULL;
    
    world->chunk_count = 0;
    world->render_distance = 8;
    world->seed = seed;
    world->last_player_pos = (Vector3){0, 0, 0};
    
    for (int i = 0; i < CHUNK_HASH_SIZE; i++) {
        world->chunk_hash[i] = NULL;
    }
    
    // Initialize Perlin noise
    init_perlin(seed);
    
    printf("World created with seed: %u (infinite chunk generation enabled)\n", seed);
    
    return world;
}

// Destroy world
void world_destroy(World* world) {
    if (!world) return;
    
    int total_chunks = world->chunk_count;
    
    for (int i = 0; i < CHUNK_HASH_SIZE; i++) {
        ChunkNode* node = world->chunk_hash[i];
        while (node) {
            ChunkNode* next = node->next;
            if (node->chunk) {
                chunk_destroy(node->chunk);
            }
            free(node);
            node = next;
        }
    }
    
    free(world);
    printf("World destroyed. Total chunks generated: %d\n", total_chunks);
}

void world_add_chunk(World* world, Chunk* chunk) {
    if (!world || !chunk) return;
    
    int hash = world_hash_coords(chunk->chunk_x, chunk->chunk_z);
    
    ChunkNode* node = (ChunkNode*)malloc(sizeof(ChunkNode));
    if (!node) return;
    
    node->chunk = chunk;
    node->next = world->chunk_hash[hash];
    world->chunk_hash[hash] = node;
    world->chunk_count++;
}

void world_remove_chunk(World* world, int chunk_x, int chunk_z) {
    if (!world) return;
    
    int hash = world_hash_coords(chunk_x, chunk_z);
    ChunkNode** node = &world->chunk_hash[hash];
    
    while (*node) {
        if ((*node)->chunk->chunk_x == chunk_x && 
            (*node)->chunk->chunk_z == chunk_z) {
            ChunkNode* to_remove = *node;
            *node = (*node)->next;
            chunk_destroy(to_remove->chunk);
            free(to_remove);
            world->chunk_count--;
            return;
        }
        node = &(*node)->next;
    }
}

Chunk* world_get_chunk(World* world, int chunk_x, int chunk_z) {
    if (!world) return NULL;
    
    int hash = world_hash_coords(chunk_x, chunk_z);
    ChunkNode* node = world->chunk_hash[hash];
    
    while (node) {
        if (node->chunk->chunk_x == chunk_x && 
            node->chunk->chunk_z == chunk_z) {
            return node->chunk;
        }
        node = node->next;
    }
    
    return NULL;
}

Chunk* world_get_or_create_chunk(World* world, int chunk_x, int chunk_z) {
    if (!world) return NULL;
    
    Chunk* chunk = world_get_chunk(world, chunk_x, chunk_z);
    if (chunk) return chunk;
    
    if (world->chunk_count >= MAX_LOADED_CHUNKS) {
        printf("Warning: Max chunks reached (%d), unload distant chunks first\n", MAX_LOADED_CHUNKS);
        return NULL;
    }
    
    chunk = chunk_create(chunk_x, chunk_z);
    if (!chunk) return NULL;
    
    world_add_chunk(world, chunk);
    world_generate_chunk_terrain(world, chunk);
    
    return chunk;
}

void world_generate_chunk_terrain(World* world, Chunk* chunk) {
    if (!chunk || chunk->is_generated) return;
    
    float scale = 0.05f;
    int base_height = 32;
    
    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            float world_x = chunk->world_position.x + x;
            float world_z = chunk->world_position.z + z;
            
            float noise = octave_perlin(world_x * scale, world_z * scale, 4, 0.5f);
            int height = base_height + (int)(noise * 20.0f);
            
            if (height < 1) height = 1;
            if (height >= CHUNK_HEIGHT) height = CHUNK_HEIGHT - 1;
            
            for (int y = 0; y <= height; y++) {
                Voxel voxel_type;
                
                if (y == height) {
                    if (height < 28) {
                        voxel_type = BLOCK_SAND;
                    } else {
                        voxel_type = BLOCK_GRASS;
                    }
                } else if (y > height - 4) {
                    voxel_type = BLOCK_DIRT;
                } else {
                    voxel_type = BLOCK_STONE;
                }
                
                voxel_set(chunk, x, y, z, voxel_type);
            }
        }
    }
    
    chunk->is_generated = true;
    chunk->needs_mesh_update = true;
}

void world_unload_distant_chunks(World* world, Vector3 player_pos) {
    if (!world) return;
    
    int player_chunk_x = (int)floor(player_pos.x / CHUNK_WIDTH);
    int player_chunk_z = (int)floor(player_pos.z / CHUNK_DEPTH);
    
    int unloaded_count = 0;
    
    for (int i = 0; i < CHUNK_HASH_SIZE; i++) {
        ChunkNode** node = &world->chunk_hash[i];
        
        while (*node) {
            Chunk* chunk = (*node)->chunk;
            int dx = chunk->chunk_x - player_chunk_x;
            int dz = chunk->chunk_z - player_chunk_z;
            int dist = (int)sqrtf(dx * dx + dz * dz);
            
            if (dist > CHUNK_UNLOAD_DISTANCE) {
                ChunkNode* to_remove = *node;
                *node = (*node)->next;
                chunk_destroy(to_remove->chunk);
                free(to_remove);
                world->chunk_count--;
                unloaded_count++;
            } else {
                node = &(*node)->next;
            }
        }
    }
    
    if (unloaded_count > 0) {
        printf("Unloaded %d distant chunks. Active chunks: %d\n", unloaded_count, world->chunk_count);
    }
}

// Update world (load/unload chunks based on player position)
void world_update(World* world, Vector3 player_pos) {
    if (!world) return;
    
    int player_chunk_x = (int)floor(player_pos.x / CHUNK_WIDTH);
    int player_chunk_z = (int)floor(player_pos.z / CHUNK_DEPTH);
    
    static int update_counter = 0;
    update_counter++;
    if (update_counter >= 60) {
        world_unload_distant_chunks(world, player_pos);
        update_counter = 0;
    }
    
    for (int radius = 0; radius <= world->render_distance; radius++) {
        for (int cx = player_chunk_x - radius; cx <= player_chunk_x + radius; cx++) {
            for (int cz = player_chunk_z - radius; cz <= player_chunk_z + radius; cz++) {
                if (abs(cx - player_chunk_x) == radius || abs(cz - player_chunk_z) == radius) {
                    Chunk* chunk = world_get_or_create_chunk(world, cx, cz);
                    
                    if (chunk && chunk->needs_mesh_update) {
                        chunk_generate_mesh(chunk);
                    }
                }
            }
        }
    }
    
    world->last_player_pos = player_pos;
}

void world_render(World* world, Camera3D camera) {
    if (!world) return;
    
    int rendered = 0;
    float max_render_dist = world->render_distance * CHUNK_WIDTH * 1.5f;
    
    for (int i = 0; i < CHUNK_HASH_SIZE; i++) {
        ChunkNode* node = world->chunk_hash[i];
        
        while (node) {
            Chunk* chunk = node->chunk;
            
            if (chunk && chunk->has_mesh) {
                Vector3 chunk_center = {
                    chunk->world_position.x + CHUNK_WIDTH / 2.0f,
                    chunk->world_position.y + CHUNK_HEIGHT / 2.0f,
                    chunk->world_position.z + CHUNK_DEPTH / 2.0f
                };
                
                float dx = camera.position.x - chunk_center.x;
                float dy = camera.position.y - chunk_center.y;
                float dz = camera.position.z - chunk_center.z;
                float dist = sqrtf(dx * dx + dy * dy + dz * dz);
                
                if (dist <= max_render_dist) {
                    DrawModel(chunk->model, chunk->world_position, 1.0f, WHITE);
                    rendered++;
                }
            }
            
            node = node->next;
        }
    }
}
