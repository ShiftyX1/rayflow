#ifndef MESH_H
#define MESH_H

#include <raylib.h>
#include "../utils/math_types.h"

// Mesh data structure
typedef struct {
    Model model;           // Raylib model
    bool is_loaded;        // Is mesh loaded
    char name[64];         // Mesh name for identification
} MeshData;

// Mesh functions
MeshData* mesh_create_cube(Vector3 size, Color color);
MeshData* mesh_create_sphere(float radius, int rings, int slices, Color color);
MeshData* mesh_create_plane(Vector2 size, Color color);
void mesh_destroy(MeshData* mesh);

#endif // MESH_H
