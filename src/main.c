#include "raylib.h"
#include "scene/scene.h"
#include "renderer/mesh.h"
#include "utils/math_types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int main() {
    const int screenWidth = 1280;
    const int screenHeight = 720;
    
    InitWindow(screenWidth, screenHeight, "RayFlow 3D Engine");
    SetTargetFPS(60);
    
    // Create scene
    Scene* main_scene = scene_create("Main Scene");
    
    // Add some objects to the scene
    // 1. Ground plane
    MeshData* plane = mesh_create_plane((Vector2){10.0f, 10.0f}, GRAY);
    EntityTransform plane_transform = transform_identity();
    plane_transform.position = (Vector3){0.0f, 0.0f, 0.0f};
    scene_add_entity(main_scene, plane, plane_transform);
    
    // 2. Red cube
    MeshData* cube1 = mesh_create_cube((Vector3){1.0f, 1.0f, 1.0f}, RED);
    EntityTransform cube1_transform = transform_identity();
    cube1_transform.position = (Vector3){-2.0f, 0.5f, 0.0f};
    scene_add_entity(main_scene, cube1, cube1_transform);
    
    // 3. Blue sphere
    MeshData* sphere1 = mesh_create_sphere(0.75f, 16, 16, BLUE);
    EntityTransform sphere1_transform = transform_identity();
    sphere1_transform.position = (Vector3){2.0f, 0.75f, 0.0f};
    scene_add_entity(main_scene, sphere1, sphere1_transform);
    
    // 4. Green cube
    MeshData* cube2 = mesh_create_cube((Vector3){0.8f, 1.5f, 0.8f}, GREEN);
    EntityTransform cube2_transform = transform_identity();
    cube2_transform.position = (Vector3){0.0f, 0.75f, -2.0f};
    scene_add_entity(main_scene, cube2, cube2_transform);
    
    printf("Scene initialized with %d entities\n", main_scene->entity_count);
    
    while (!WindowShouldClose()) {
        float delta_time = GetFrameTime();
        
        // Update scene
        scene_update(main_scene, delta_time);
        
        // Render
        BeginDrawing();
        ClearBackground((Color){135, 206, 235, 255}); // Sky blue
        
        // Render scene
        scene_render(main_scene);
        
        // Draw UI
        DrawText("RayFlow 3D Engine", 10, 10, 20, BLACK);
        DrawText("Use mouse to rotate camera", 10, 40, 16, DARKGRAY);
        DrawFPS(10, screenHeight - 30);
        
        EndDrawing();
    }
    
    // Cleanup
    scene_destroy(main_scene);
    CloseWindow();
    
    return 0;
}
