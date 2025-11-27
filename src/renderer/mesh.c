#include "mesh.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

MeshData* mesh_create_cube(Vector3 size, Color color) {
    MeshData* mesh_data = (MeshData*)malloc(sizeof(MeshData));
    if (!mesh_data) return NULL;
    
    Mesh mesh = GenMeshCube(size.x, size.y, size.z);
    mesh_data->model = LoadModelFromMesh(mesh);
    mesh_data->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = color;
    mesh_data->is_loaded = true;
    strncpy(mesh_data->name, "Cube", sizeof(mesh_data->name) - 1);
    
    return mesh_data;
}

MeshData* mesh_create_sphere(float radius, int rings, int slices, Color color) {
    MeshData* mesh_data = (MeshData*)malloc(sizeof(MeshData));
    if (!mesh_data) return NULL;
    
    Mesh mesh = GenMeshSphere(radius, rings, slices);
    mesh_data->model = LoadModelFromMesh(mesh);
    mesh_data->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = color;
    mesh_data->is_loaded = true;
    strncpy(mesh_data->name, "Sphere", sizeof(mesh_data->name) - 1);
    
    return mesh_data;
}

MeshData* mesh_create_plane(Vector2 size, Color color) {
    MeshData* mesh_data = (MeshData*)malloc(sizeof(MeshData));
    if (!mesh_data) return NULL;
    
    Mesh mesh = GenMeshPlane(size.x, size.y, 10, 10);
    mesh_data->model = LoadModelFromMesh(mesh);
    mesh_data->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = color;
    mesh_data->is_loaded = true;
    strncpy(mesh_data->name, "Plane", sizeof(mesh_data->name) - 1);
    
    return mesh_data;
}

void mesh_destroy(MeshData* mesh) {
    if (mesh && mesh->is_loaded) {
        UnloadModel(mesh->model);
        mesh->is_loaded = false;
    }
    free(mesh);
}
