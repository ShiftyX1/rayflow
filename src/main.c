#include "raylib.h"
#include "voxel/world.h"
#include "voxel/block_registry.h"
#include "voxel/block_interaction.h"
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
    
    // Initiate block registry with texture atlas
    // IMPORTANT: Place your texture atlas in the folder build/textures/terrain.png after building the project
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
    
    // Initialize block interaction system
    BlockInteraction block_interaction;
    block_interaction_init(&block_interaction);
    // Default tool - hand
    block_interaction_set_tool(&block_interaction, TOOL_NONE, TOOL_LEVEL_HAND);
    
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
    printf("  Left Mouse Button - Break block\n");
    printf("  1-5 - Select tool (1=Hand, 2=Wood Pickaxe, 3=Stone Pickaxe, 4=Iron Pickaxe, 5=Diamond Pickaxe)\n");
    printf("  ESC - Exit\n");
    
    bool should_exit = false;
    
    while (!WindowShouldClose() && !should_exit) {
        float delta_time = GetFrameTime();
        
        // Check for ESC key to exit
        if (IsKeyPressed(KEY_ESCAPE)) {
            should_exit = true;
            break;
        }
        
        // Tool selection
        if (IsKeyPressed(KEY_ONE)) {
            block_interaction_set_tool(&block_interaction, TOOL_NONE, TOOL_LEVEL_HAND);
            printf("Selected: Hand\n");
        }
        if (IsKeyPressed(KEY_TWO)) {
            block_interaction_set_tool(&block_interaction, TOOL_PICKAXE, TOOL_LEVEL_WOOD);
            printf("Selected: Wooden Pickaxe\n");
        }
        if (IsKeyPressed(KEY_THREE)) {
            block_interaction_set_tool(&block_interaction, TOOL_PICKAXE, TOOL_LEVEL_STONE);
            printf("Selected: Stone Pickaxe\n");
        }
        if (IsKeyPressed(KEY_FOUR)) {
            block_interaction_set_tool(&block_interaction, TOOL_PICKAXE, TOOL_LEVEL_IRON);
            printf("Selected: Iron Pickaxe\n");
        }
        if (IsKeyPressed(KEY_FIVE)) {
            block_interaction_set_tool(&block_interaction, TOOL_PICKAXE, TOOL_LEVEL_DIAMOND);
            printf("Selected: Diamond Pickaxe\n");
        }
        
        // Update player (handles input, physics, and collisions)
        player_update(player, world, delta_time);
        
        // Get player camera
        Camera3D camera = player_get_camera(player);
        
        // Calculate camera direction for raycast
        Vector3 camera_direction = {
            camera.target.x - camera.position.x,
            camera.target.y - camera.position.y,
            camera.target.z - camera.position.z
        };
        
        // Update block interaction
        bool is_breaking = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
        block_interaction_update(&block_interaction, world, camera.position, camera_direction, is_breaking, delta_time);
        
        // Update world (load/unload chunks)
        world_update(world, player->position);
        
        // Render
        BeginDrawing();
        ClearBackground((Color){135, 206, 235, 255}); // Sky blue
        
        BeginMode3D(camera);
        
        // Render world
        world_render(world, camera);
        
        // Render block highlight
        block_interaction_render_highlight(&block_interaction, camera);
        
        // Render break overlay
        block_interaction_render_break_overlay(&block_interaction, camera);
        
        // Draw grid for reference
        // DrawGrid(100, 1.0f);
        
        EndMode3D();
        
        // Draw UI
        // Crosshair
        block_interaction_render_crosshair(screenWidth, screenHeight);
        
        // TODO: Improve UI rendering (use proper font, better layout)
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
        
        // Display current tool
        const char* tool_names[] = {"Hand", "Pickaxe", "Axe", "Shovel", "Hoe", "Sword", "Shears"};
        const char* level_names[] = {"", "Wooden", "Stone", "Iron", "Diamond", "Netherite"};
        const char* tool_name = tool_names[block_interaction.current_tool < TOOL_COUNT ? block_interaction.current_tool : 0];
        const char* level_name = level_names[block_interaction.current_tool_level < 6 ? block_interaction.current_tool_level : 0];
        
        if (block_interaction.current_tool == TOOL_NONE) {
            DrawText("Tool: Hand", 10, 120, 16, DARKGRAY);
        } else {
            DrawText(TextFormat("Tool: %s %s", level_name, tool_name), 10, 120, 16, DARKGRAY);
        }
        
        // Display information about the block under the crosshair
        if (block_interaction.current_target.hit) {
            BlockProperties* block_props = block_registry_get(block_interaction.current_target.block_type);
            if (block_props) {
                DrawText(TextFormat("Looking at: %s", block_props->name), 10, 140, 16, DARKGRAY);
                
                // Breaking progress (TODO: change to a block texture overlay)
                if (block_interaction.break_state.is_breaking) {
                    float progress = block_interaction.break_state.break_progress / block_interaction.break_state.total_break_time;
                    if (progress > 1.0f) progress = 1.0f;
                    DrawText(TextFormat("Breaking: %.0f%%", progress * 100.0f), 10, 160, 16, ORANGE);
                }
            }
        }
        
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
