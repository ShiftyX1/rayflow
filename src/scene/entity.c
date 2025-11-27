#include "entity.h"
#include <stdlib.h>

static int next_entity_id = 1;

Entity* entity_create(MeshData* mesh, EntityTransform transform) {
    Entity* entity = (Entity*)malloc(sizeof(Entity));
    if (!entity) return NULL;
    
    entity->id = next_entity_id++;
    entity->transform = transform;
    entity->mesh = mesh;
    entity->is_active = true;
    entity->next = NULL;
    
    return entity;
}

void entity_destroy(Entity* entity) {
    if (entity) {
        if (entity->mesh) {
            mesh_destroy(entity->mesh);
        }
        free(entity);
    }
}

void entity_set_position(Entity* entity, Vec3 position) {
    if (entity) {
        entity->transform.position = position;
    }
}

void entity_set_scale(Entity* entity, Vec3 scale) {
    if (entity) {
        entity->transform.scale = scale;
    }
}
