#ifndef ENTITY_H
#define ENTITY_H

#include "../utils/math_types.h"
#include "../renderer/mesh.h"

// Entity in the scene
typedef struct Entity {
    int id;                    // Unique entity ID
    EntityTransform transform;       // Position, rotation, scale
    MeshData* mesh;           // Mesh data
    bool is_active;           // Is entity active in scene
    struct Entity* next;      // For linked list
} Entity;

// Entity functions
Entity* entity_create(MeshData* mesh, EntityTransform transform);
void entity_destroy(Entity* entity);
void entity_set_position(Entity* entity, Vec3 position);
void entity_set_scale(Entity* entity, Vec3 scale);

#endif // ENTITY_H
