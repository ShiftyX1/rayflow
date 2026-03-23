#pragma once

// =============================================================================
// Legacy MeshBuilder — thin wrapper around IMesh for backwards compatibility.
// New code should use resources::create_cube() or RenderDevice directly.
// =============================================================================

#include "engine/core/export.hpp"
#include "engine/renderer/gpu/gpu_mesh.hpp"
#include <memory>

namespace renderer {

struct MeshData {
    std::unique_ptr<rf::IMesh> mesh{};
    bool valid{false};
};

class RAYFLOW_CLIENT_API MeshBuilder {
public:
    static MeshData create_cube(float size = 1.0f);
    static MeshData create_plane(float width, float depth);
    static void destroy(MeshData& mesh_data);
};

} // namespace renderer
