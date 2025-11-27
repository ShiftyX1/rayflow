#ifndef SCENE_H
#define SCENE_H

#include "entity.h"
#include <raylib.h>

#define MAX_ENTITIES 1024

// Scene structure
typedef struct {
    Entity* entities[MAX_ENTITIES];
    int entity_count;
    Camera3D camera;
    char name[64];
} Scene;

// Scene functions
Scene* scene_create(const char* name);
void scene_destroy(Scene* scene);
Entity* scene_add_entity(Scene* scene, MeshData* mesh, EntityTransform transform);
void scene_remove_entity(Scene* scene, int entity_id);
void scene_update(Scene* scene, float delta_time);
void scene_render(Scene* scene);

#endif // SCENE_H
