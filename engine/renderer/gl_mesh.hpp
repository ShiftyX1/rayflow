#pragma once

// =============================================================================
// GLMesh — OpenGL VAO/VBO mesh wrapper
//
// Phase 2: Replaces raylib Mesh / UploadMesh / LoadModelFromMesh.
// Supports flexible vertex attribute layout for voxel chunks and utility meshes.
// =============================================================================

#include "engine/core/export.hpp"
#include "engine/core/math_types.hpp"
#include "engine/renderer/gpu/gpu_mesh.hpp"

#include <glad/gl.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rf {

class RAYFLOW_CLIENT_API GLMesh : public IMesh {
public:
    GLMesh() = default;
    ~GLMesh() override;

    // Non-copyable, movable
    GLMesh(const GLMesh&) = delete;
    GLMesh& operator=(const GLMesh&) = delete;
    GLMesh(GLMesh&& other) noexcept;
    GLMesh& operator=(GLMesh&& other) noexcept;

    // ----- Upload methods (IMesh interface) -----

    /// Upload interleaved or separate-buffer voxel chunk data.
    /// Uses the 5-attribute layout: position(3f), texcoord(2f), texcoord2(2f), normal(3f), color(4ub).
    /// @param vertexCount   Number of vertices (NOT number of floats).
    /// @param positions     3 floats per vertex.
    /// @param texcoords     2 floats per vertex (may be nullptr).
    /// @param texcoords2    2 floats per vertex (may be nullptr).
    /// @param normals       3 floats per vertex (may be nullptr).
    /// @param colors        4 unsigned bytes per vertex (may be nullptr).
    /// @param dynamic       Use GL_DYNAMIC_DRAW for frequently updated meshes.
    void upload(int vertexCount,
                const float* positions,
                const float* texcoords = nullptr,
                const float* texcoords2 = nullptr,
                const float* normals = nullptr,
                const std::uint8_t* colors = nullptr,
                bool dynamic = false) override;

    /// Re-upload all data to existing VBOs (orphaning for dynamic meshes).
    /// Same parameter layout as upload(). Only valid if upload() was called previously.
    void update(int vertexCount,
                const float* positions,
                const float* texcoords = nullptr,
                const float* texcoords2 = nullptr,
                const float* normals = nullptr,
                const std::uint8_t* colors = nullptr) override;

    /// Upload a simple position-only mesh (e.g. skybox cube, fullscreen quad).
    void uploadPositionOnly(const float* positions, int vertexCount) override;

    /// Destroy VAO and all VBOs.
    void destroy() override;

    // ----- Drawing -----

    /// Draw the mesh with abstract primitive type (IMesh interface).
    void draw(PrimitiveType mode = PrimitiveType::Triangles) const override;

    /// Draw with raw OpenGL primitive mode (GL-specific, for backward compat).
    void drawGL(GLenum mode = GL_TRIANGLES) const;

    // ----- Accessors -----

    bool isValid() const override { return vao_ != 0; }
    int vertexCount() const override { return vertexCount_; }
    GLuint vao() const { return vao_; }

    // ----- Utility mesh generators -----

    /// Create a unit cube centered at origin (for skybox).
    static GLMesh createCube(float size = 1.0f);

    /// Create a wireframe cube (GL_LINES) centered at origin.
    static GLMesh createWireframeCube(float size = 1.0f);

    /// Create an XZ plane centered at origin with normals and UVs.
    static GLMesh createPlane(float width = 1.0f, float depth = 1.0f);

    /// Create a cube with per-face UV coordinates (for texture-mapped overlays).
    static GLMesh createCubeWithUVs(float size = 1.0f);

    /// Create a fullscreen triangle (covers [-1,1] NDC).
    static GLMesh createFullscreenTriangle();

private:
    GLuint vao_{0};

    // Separate VBOs for each attribute (allows partial update)
    static constexpr int kMaxVBOs = 5;
    GLuint vbos_[kMaxVBOs]{};

    int vertexCount_{0};
    bool dynamic_{false};
};

} // namespace rf
