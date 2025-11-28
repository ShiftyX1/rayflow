#include "world.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

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

// Create world
World* world_create(unsigned int seed) {
    World* world = (World*)malloc(sizeof(World));
    if (!world) return NULL;
    
    world->chunk_count = 0;
    world->render_distance = 8;
    world->seed = seed;
    
    for (int i = 0; i < MAX_CHUNKS; i++) {
        world->chunks[i] = NULL;
    }
    
    // Initialize Perlin noise
    init_perlin(seed);
    
    printf("World created with seed: %u\n", seed);
    
    return world;
}

// Destroy world
void world_destroy(World* world) {
    if (!world) return;
    
    for (int i = 0; i < world->chunk_count; i++) {
        if (world->chunks[i]) {
            chunk_destroy(world->chunks[i]);
        }
    }
    
    free(world);
}

// Get chunk at position
Chunk* world_get_chunk(World* world, int chunk_x, int chunk_z) {
    if (!world) return NULL;
    
    for (int i = 0; i < world->chunk_count; i++) {
        if (world->chunks[i] && 
            world->chunks[i]->chunk_x == chunk_x && 
            world->chunks[i]->chunk_z == chunk_z) {
            return world->chunks[i];
        }
    }
    
    return NULL;
}

// Generate terrain for chunk
void world_generate_chunk_terrain(World* world, Chunk* chunk) {
    if (!chunk || chunk->is_generated) return;
    
    float scale = 0.05f; // Noise scale
    int base_height = 32; // Base terrain height
    
    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            // Calculate world coordinates
            float world_x = chunk->world_position.x + x;
            float world_z = chunk->world_position.z + z;
            
            // Generate height using Perlin noise
            float noise = octave_perlin(world_x * scale, world_z * scale, 4, 0.5f);
            int height = base_height + (int)(noise * 20.0f);
            
            // Clamp height
            if (height < 1) height = 1;
            if (height >= CHUNK_HEIGHT) height = CHUNK_HEIGHT - 1;
            
            // Fill column
            for (int y = 0; y <= height; y++) {
                Voxel voxel_type;
                
                if (y == height) {
                    // Top layer
                    if (height < 28) {
                        voxel_type = BLOCK_SAND; // Beach
                    } else {
                        voxel_type = BLOCK_GRASS; // Grass
                    }
                } else if (y > height - 4) {
                    // Near surface
                    voxel_type = BLOCK_DIRT;
                } else {
                    // Deep underground
                    voxel_type = BLOCK_STONE;
                }
                
                voxel_set(chunk, x, y, z, voxel_type);
            }
        }
    }
    
    chunk->is_generated = true;
    chunk->needs_mesh_update = true;
}

// Update world (load/unload chunks based on player position)
void world_update(World* world, Vector3 player_pos) {
    if (!world) return;
    
    int player_chunk_x = (int)floor(player_pos.x / CHUNK_WIDTH);
    int player_chunk_z = (int)floor(player_pos.z / CHUNK_DEPTH);
    
    // Load chunks around player
    for (int cx = player_chunk_x - world->render_distance; 
         cx <= player_chunk_x + world->render_distance; cx++) {
        for (int cz = player_chunk_z - world->render_distance; 
             cz <= player_chunk_z + world->render_distance; cz++) {
            
            // Check if chunk exists
            Chunk* chunk = world_get_chunk(world, cx, cz);
            if (!chunk && world->chunk_count < MAX_CHUNKS) {
                // Create new chunk
                chunk = chunk_create(cx, cz);
                if (chunk) {
                    world->chunks[world->chunk_count++] = chunk;
                    world_generate_chunk_terrain(world, chunk);
                }
            }
            
            // Generate mesh if needed
            if (chunk && chunk->needs_mesh_update) {
                chunk_generate_mesh(chunk);
            }
        }
    }
}

// Render world
void world_render(World* world, Camera3D camera) {
    if (!world) return;
    
    for (int i = 0; i < world->chunk_count; i++) {
        Chunk* chunk = world->chunks[i];
        if (!chunk || !chunk->has_mesh) continue;
        
        // Frustum culling (simple distance check)
        Vector3 chunk_center = {
            chunk->world_position.x + CHUNK_WIDTH / 2.0f,
            chunk->world_position.y + CHUNK_HEIGHT / 2.0f,
            chunk->world_position.z + CHUNK_DEPTH / 2.0f
        };
        
        // Calculate distance manually
        float dx = camera.position.x - chunk_center.x;
        float dy = camera.position.y - chunk_center.y;
        float dz = camera.position.z - chunk_center.z;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        if (dist > (world->render_distance * CHUNK_WIDTH * 1.5f)) continue;
        
        // Render chunk
        DrawModel(chunk->model, chunk->world_position, 1.0f, WHITE);
    }
}
