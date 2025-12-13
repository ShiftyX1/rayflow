#pragma once

#include <raylib.h>

namespace renderer {

struct MeshData {
    Mesh mesh{};
    Material material{};
    bool valid{false};
};

class MeshBuilder {
public:
    static MeshData create_cube(float size = 1.0f);
    static MeshData create_plane(float width, float depth);
    static void destroy(MeshData& mesh_data);
};

} // namespace renderer
