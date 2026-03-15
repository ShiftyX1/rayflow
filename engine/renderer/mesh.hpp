#pragma once

// =============================================================================
// Legacy MeshBuilder — thin wrapper around GLMesh for backwards compatibility.
// New code should use rf::GLMesh directly.
// =============================================================================

#include "engine/core/export.hpp"
#include "engine/renderer/gl_mesh.hpp"

namespace renderer {

struct MeshData {
    rf::GLMesh mesh{};
    bool valid{false};
};

class RAYFLOW_CLIENT_API MeshBuilder {
public:
    static MeshData create_cube(float size = 1.0f);
    static MeshData create_plane(float width, float depth);
    static void destroy(MeshData& mesh_data);
};

} // namespace renderer
