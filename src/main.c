#include "raylib.h"
#include "voxel/world.h"
#include "voxel/block_registry.h"
#include "scene/player.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

int main() {
    const int screenWidth = 1280;
    const int screenHeight = 720;
    
    InitWindow(screenWidth, screenHeight, "RayFlow 3D Engine");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // Disable ESC as default exit key
    
    // Инициализация реестра блоков с текстурным атласом
    // ВАЖНО: Поместите ваш атлас текстур в папку src/static/textures/blocks_atlas.png
    if (!block_registry_init("textures/terrain.png")) {
        fprintf(stderr, "Failed to initialize block registry!\n");
        CloseWindow();
        return -1;
    }
    
    // Create voxel world
    unsigned int seed = (unsigned int)time(NULL);
    World* world = world_create(seed);
    
    // Create player
    Vector3 spawn_position = (Vector3){ 50.0f, 80.0f, 50.0f };
    Player* player = player_create(spawn_position);
    if (!player) {
        fprintf(stderr, "Failed to create player!\n");
        world_destroy(world);
        block_registry_destroy();
        CloseWindow();
        return -1;
    }
    
    DisableCursor();
    
    printf("Voxel world initialized!\n");
    printf("Player spawned at (%.1f, %.1f, %.1f)\n", 
        spawn_position.x, spawn_position.y, spawn_position.z);
    printf("\nControls:\n");
    printf("  WASD - Move player\n");
    printf("  Mouse - Look around\n");
    printf("  Space - Jump (or fly up in creative mode)\n");
    printf("  Left Shift - Fly down in creative mode\n");
    printf("  Left Ctrl - Sprint\n");
    printf("  C - Toggle creative mode\n");
    printf("  ESC - Exit\n");
    
    bool should_exit = false;
    
    while (!WindowShouldClose() && !should_exit) {
        float delta_time = GetFrameTime();
        
        // Check for ESC key to exit
        if (IsKeyPressed(KEY_ESCAPE)) {
            should_exit = true;
            break;
        }
        
        // Update player (handles input, physics, and collisions)
        player_update(player, world, delta_time);
        
        // Get player camera
        Camera3D camera = player_get_camera(player);
        
        // Update world (load/unload chunks)
        world_update(world, player->position);
        
        // Render
        BeginDrawing();
        ClearBackground((Color){135, 206, 235, 255}); // Sky blue
        
        BeginMode3D(camera);
        
        // Render world
        world_render(world, camera);
        
        // Draw grid for reference
        // DrawGrid(100, 1.0f);
        
        EndMode3D();
        
        // Draw UI
        DrawText("Voxel Engine - Player System", 10, 10, 20, BLACK);
        DrawText(TextFormat("Position: (%.1f, %.1f, %.1f)", 
            player->position.x, player->position.y, player->position.z), 10, 40, 16, DARKGRAY);
        DrawText(TextFormat("Velocity: (%.1f, %.1f, %.1f)", 
            player->velocity.x, player->velocity.y, player->velocity.z), 10, 60, 16, DARKGRAY);
        DrawText(TextFormat("On Ground: %s | Sprint: %s | Creative: %s", 
            player->on_ground ? "YES" : "NO",
            player->is_sprinting ? "YES" : "NO",
            player->in_creative_mode ? "YES" : "NO"), 10, 80, 16, DARKGRAY);
        DrawText(TextFormat("Chunks loaded: %d", world->chunk_count), 10, 100, 16, DARKGRAY);
        DrawFPS(10, screenHeight - 30);
        
        EndDrawing();
    }
    
    // Cleanup
    player_destroy(player);
    world_destroy(world);
    block_registry_destroy();
    CloseWindow();
    
    return 0;
}
