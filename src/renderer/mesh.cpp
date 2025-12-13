#include "mesh.hpp"

namespace renderer {

MeshData MeshBuilder::create_cube(float size) {
    MeshData data;
    data.mesh = GenMeshCube(size, size, size);
    data.material = LoadMaterialDefault();
    data.valid = true;
    return data;
}

MeshData MeshBuilder::create_plane(float width, float depth) {
    MeshData data;
    data.mesh = GenMeshPlane(width, depth, 1, 1);
    data.material = LoadMaterialDefault();
    data.valid = true;
    return data;
}

void MeshBuilder::destroy(MeshData& mesh_data) {
    if (mesh_data.valid) {
        UnloadMesh(mesh_data.mesh);
        UnloadMaterial(mesh_data.material);
        mesh_data.valid = false;
    }
}

} // namespace renderer
