#include "scene.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Scene* scene_create(const char* name) {
    Scene* scene = (Scene*)malloc(sizeof(Scene));
    if (!scene) return NULL;
    
    scene->entity_count = 0;
    memset(scene->entities, 0, sizeof(scene->entities));
    strncpy(scene->name, name, sizeof(scene->name) - 1);
    
    // Setup default camera
    scene->camera.position = (Vector3){ 10.0f, 10.0f, 10.0f };
    scene->camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    scene->camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    scene->camera.fovy = 45.0f;
    scene->camera.projection = CAMERA_PERSPECTIVE;
    
    return scene;
}

void scene_destroy(Scene* scene) {
    if (!scene) return;
    
    for (int i = 0; i < scene->entity_count; i++) {
        if (scene->entities[i]) {
            entity_destroy(scene->entities[i]);
        }
    }
    free(scene);
}

Entity* scene_add_entity(Scene* scene, MeshData* mesh, EntityTransform transform) {
    if (!scene || scene->entity_count >= MAX_ENTITIES) {
        return NULL;
    }
    
    Entity* entity = entity_create(mesh, transform);
    if (!entity) return NULL;
    
    scene->entities[scene->entity_count++] = entity;
    
    printf("Added entity %d to scene '%s' (total: %d)\n", 
           entity->id, scene->name, scene->entity_count);
    
    return entity;
}

void scene_remove_entity(Scene* scene, int entity_id) {
    if (!scene) return;
    
    for (int i = 0; i < scene->entity_count; i++) {
        if (scene->entities[i] && scene->entities[i]->id == entity_id) {
            entity_destroy(scene->entities[i]);
            
            // Shift entities down
            for (int j = i; j < scene->entity_count - 1; j++) {
                scene->entities[j] = scene->entities[j + 1];
            }
            scene->entities[--scene->entity_count] = NULL;
            
            printf("Removed entity %d from scene '%s'\n", entity_id, scene->name);
            return;
        }
    }
}

void scene_update(Scene* scene, float delta_time) {
    if (!scene) return;
    
    // Update camera controls
    UpdateCamera(&scene->camera, CAMERA_ORBITAL);
    
    // Update entities (placeholder for future logic)
    for (int i = 0; i < scene->entity_count; i++) {
        if (scene->entities[i] && scene->entities[i]->is_active) {
            // Future: entity update logic
        }
    }
}

void scene_render(Scene* scene) {
    if (!scene) return;
    
    BeginMode3D(scene->camera);
    
    // Draw grid
    DrawGrid(10, 1.0f);
    
    // Render all entities
    for (int i = 0; i < scene->entity_count; i++) {
        Entity* entity = scene->entities[i];
        if (entity && entity->is_active && entity->mesh && entity->mesh->is_loaded) {
            Vector3 position = {
                entity->transform.position.x,
                entity->transform.position.y,
                entity->transform.position.z
            };
            Vector3 scale = {
                entity->transform.scale.x,
                entity->transform.scale.y,
                entity->transform.scale.z
            };
            
            // Draw model with transform
            DrawModelEx(entity->mesh->model, position, 
                       (Vector3){0.0f, 1.0f, 0.0f}, 0.0f, scale, WHITE);
        }
    }
    
    EndMode3D();
}
