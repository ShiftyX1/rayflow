#pragma once

// =============================================================================
// IMesh — Abstract GPU mesh (vertex buffer) interface
//
// Backend-agnostic. Implemented by GLMesh, DX11Mesh, etc.
// =============================================================================

#include "engine/core/export.hpp"
#include "engine/core/math_types.hpp"
#include "engine/renderer/gpu/gpu_types.hpp"

#include <cstdint>

namespace rf {

class RAYFLOW_CLIENT_API IMesh {
public:
    virtual ~IMesh() = default;

    // Non-copyable
    IMesh(const IMesh&) = delete;
    IMesh& operator=(const IMesh&) = delete;

    // ----- Upload -----

    /// Upload vertex data with the standard 5-attribute layout.
    /// @param vertexCount  Number of vertices.
    /// @param positions    3 floats per vertex (required).
    /// @param texcoords    2 floats per vertex (may be nullptr).
    /// @param texcoords2   2 floats per vertex (may be nullptr).
    /// @param normals      3 floats per vertex (may be nullptr).
    /// @param colors       4 unsigned bytes per vertex (may be nullptr).
    /// @param dynamic      Hint for frequently updated meshes.
    virtual void upload(int vertexCount,
                        const float* positions,
                        const float* texcoords = nullptr,
                        const float* texcoords2 = nullptr,
                        const float* normals = nullptr,
                        const std::uint8_t* colors = nullptr,
                        bool dynamic = false) = 0;

    /// Re-upload data to existing buffers (for dynamic meshes).
    virtual void update(int vertexCount,
                        const float* positions,
                        const float* texcoords = nullptr,
                        const float* texcoords2 = nullptr,
                        const float* normals = nullptr,
                        const std::uint8_t* colors = nullptr) = 0;

    /// Upload a simple position-only mesh.
    virtual void uploadPositionOnly(const float* positions, int vertexCount) = 0;

    /// Release GPU resources.
    virtual void destroy() = 0;

    // ----- Drawing -----

    /// Draw the mesh with the given primitive type.
    virtual void draw(PrimitiveType mode = PrimitiveType::Triangles) const = 0;

    // ----- Accessors -----

    virtual bool isValid() const = 0;
    virtual int vertexCount() const = 0;

protected:
    IMesh() = default;
    IMesh(IMesh&&) noexcept = default;
    IMesh& operator=(IMesh&&) noexcept = default;
};

} // namespace rf
