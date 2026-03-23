#include "mesh.hpp"
#include "engine/client/core/resources.hpp"
#include "engine/core/logging.hpp"

namespace renderer {

MeshData MeshBuilder::create_cube(float size) {
    MeshData data;
    data.mesh = resources::create_cube(size);
    data.valid = data.mesh && data.mesh->isValid();
    return data;
}

MeshData MeshBuilder::create_plane(float width, float depth) {
    MeshData data;
    // TODO: implement create_plane in resources module
    data.valid = false;
    return data;
}

void MeshBuilder::destroy(MeshData& mesh_data) {
    if (mesh_data.valid) {
        mesh_data.mesh.reset();
        mesh_data.valid = false;
    }
}

} // namespace renderer
