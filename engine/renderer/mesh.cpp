#include "mesh.hpp"
#include "engine/core/logging.hpp"

namespace renderer {

MeshData MeshBuilder::create_cube(float size) {
    MeshData data;
    data.mesh = rf::GLMesh::createCube(size);
    data.valid = data.mesh.isValid();
    return data;
}

MeshData MeshBuilder::create_plane(float width, float depth) {
    MeshData data;
    // TODO(Phase 5): Implement GLMesh::createPlane()
    (void)width;
    (void)depth;
    data.valid = false;
    return data;
}

void MeshBuilder::destroy(MeshData& mesh_data) {
    if (mesh_data.valid) {
        mesh_data.mesh.destroy();
        mesh_data.valid = false;
    }
}

} // namespace renderer
