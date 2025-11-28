#include "raylib.h"
#include "voxel/world.h"
#include "voxel/block_registry.h"
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
    
    // Инициализация реестра блоков с текстурным атласом
    // ВАЖНО: Поместите ваш атлас текстур в папку src/static/textures/blocks_atlas.png
    if (!block_registry_init("textures/terrain.png")) {
        fprintf(stderr, "Failed to initialize block registry!\n");
        CloseWindow();
        return -1;
    }
    
    // Create camera
    Camera3D camera = { 0 };
    camera.position = (Vector3){ 50.0f, 50.0f, 50.0f };
    camera.target = (Vector3){ 0.0f, 30.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 70.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    
    // Create voxel world
    unsigned int seed = (unsigned int)time(NULL);
    World* world = world_create(seed);
    
    // Camera control variables
    float camera_speed = 20.0f;
    float camera_sensitivity = 0.01f;
    float yaw = -90.0f;
    float pitch = -20.0f;
    
    DisableCursor();
    
    printf("Voxel world initialized!\n");
    printf("Controls:\n");
    printf("  WASD - Move camera\n");
    printf("  Mouse - Look around\n");
    printf("  Space/Shift - Up/Down\n");
    printf("  ESC - Exit\n");
    
    while (!WindowShouldClose()) {
        float delta_time = GetFrameTime();
        
        // Mouse look
        Vector2 mouse_delta = GetMouseDelta();
        yaw += mouse_delta.x * camera_sensitivity;
        pitch -= mouse_delta.y * camera_sensitivity;
        
        // Clamp pitch
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
        
        // Calculate camera direction
        Vector3 direction;
        direction.x = cosf(yaw * DEG2RAD) * cosf(pitch * DEG2RAD);
        direction.y = sinf(pitch * DEG2RAD);
        direction.z = sinf(yaw * DEG2RAD) * cosf(pitch * DEG2RAD);
        float dir_len = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
        direction.x /= dir_len;
        direction.y /= dir_len;
        direction.z /= dir_len;
        
        // Camera movement
        Vector3 forward = direction;
        forward.y = 0;
        float fwd_len = sqrtf(forward.x * forward.x + forward.z * forward.z);
        if (fwd_len > 0.001f) {
            forward.x /= fwd_len;
            forward.z /= fwd_len;
        }
        
        // Calculate right vector (cross product of forward and up)
        Vector3 right;
        right.x = forward.z;
        right.y = 0;
        right.z = -forward.x;
        
        float move_speed = camera_speed * delta_time;
        
        if (IsKeyDown(KEY_W)) {
            camera.position.x += forward.x * move_speed;
            camera.position.z += forward.z * move_speed;
        }
        if (IsKeyDown(KEY_S)) {
            camera.position.x -= forward.x * move_speed;
            camera.position.z -= forward.z * move_speed;
        }
        if (IsKeyDown(KEY_A)) {
            camera.position.x += right.x * move_speed;
            camera.position.z += right.z * move_speed;
        }
        if (IsKeyDown(KEY_D)) {
            camera.position.x -= right.x * move_speed;
            camera.position.z -= right.z * move_speed;
        }
        if (IsKeyDown(KEY_SPACE)) {
            camera.position.y += move_speed;
        }
        if (IsKeyDown(KEY_LEFT_SHIFT)) {
            camera.position.y -= move_speed;
        }
        
        // Update camera target
        camera.target.x = camera.position.x + direction.x;
        camera.target.y = camera.position.y + direction.y;
        camera.target.z = camera.position.z + direction.z;
        
        // Update world (load/unload chunks)
        world_update(world, camera.position);
        
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
        DrawText("Voxel Engine - Minecraft-like", 10, 10, 20, BLACK);
        DrawText(TextFormat("Position: (%.1f, %.1f, %.1f)", 
            camera.position.x, camera.position.y, camera.position.z), 10, 40, 16, DARKGRAY);
        DrawText(TextFormat("Chunks loaded: %d", world->chunk_count), 10, 60, 16, DARKGRAY);
        DrawFPS(10, screenHeight - 30);
        
        EndDrawing();
    }
    
    // Cleanup
    world_destroy(world);
    block_registry_destroy();
    CloseWindow();
    
    return 0;
}
