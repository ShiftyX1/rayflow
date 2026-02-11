#include "mesh.hpp"
#include "engine/core/logging.hpp"

namespace renderer {

MeshData MeshBuilder::create_cube(float size) {
    MeshData data;
    // NOTE(migration): GenMeshCube/LoadMaterialDefault are raylib calls — stubbed for Phase 0
    // data.mesh = GenMeshCube(size, size, size);
    // data.material = LoadMaterialDefault();
    data.mesh = RaylibMeshPlaceholder{};
    data.material = RaylibMaterialPlaceholder{};
    data.valid = true;
    return data;
}

MeshData MeshBuilder::create_plane(float width, float depth) {
    MeshData data;
    // NOTE(migration): GenMeshPlane/LoadMaterialDefault are raylib calls — stubbed for Phase 0
    // data.mesh = GenMeshPlane(width, depth, 1, 1);
    // data.material = LoadMaterialDefault();
    data.mesh = RaylibMeshPlaceholder{};
    data.material = RaylibMaterialPlaceholder{};
    data.valid = true;
    return data;
}

void MeshBuilder::destroy(MeshData& mesh_data) {
    if (mesh_data.valid) {
        // NOTE(migration): UnloadMesh/UnloadMaterial are raylib calls — stubbed for Phase 0
        // UnloadMesh(mesh_data.mesh);
        // UnloadMaterial(mesh_data.material);
        mesh_data.valid = false;
    }
}

} // namespace renderer
