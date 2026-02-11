#pragma once

#include "engine/core/math_types.hpp"

namespace renderer {

// NOTE(migration): MeshData still wraps raylib types.
// Phase 2 will replace with GLMesh.
struct RaylibMeshPlaceholder {};
struct RaylibMaterialPlaceholder {};

struct MeshData {
    RaylibMeshPlaceholder mesh{};
    RaylibMaterialPlaceholder material{};
    bool valid{false};
};

class MeshBuilder {
public:
    static MeshData create_cube(float size = 1.0f);
    static MeshData create_plane(float width, float depth);
    static void destroy(MeshData& mesh_data);
};

} // namespace renderer
