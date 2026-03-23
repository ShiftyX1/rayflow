#include "gl_mesh.hpp"

#include "engine/core/logging.hpp"

#include <cstring>

namespace rf {

namespace {
    GLenum toGLPrimitive(PrimitiveType mode) {
        switch (mode) {
            case PrimitiveType::Triangles: return GL_TRIANGLES;
            case PrimitiveType::Lines:     return GL_LINES;
            case PrimitiveType::LineStrip: return GL_LINE_STRIP;
            case PrimitiveType::Points:    return GL_POINTS;
        }
        return GL_TRIANGLES;
    }
} // anonymous namespace

// ============================================================================
// Move semantics
// ============================================================================

GLMesh::~GLMesh() {
    destroy();
}

GLMesh::GLMesh(GLMesh&& other) noexcept
    : vao_(other.vao_)
    , vertexCount_(other.vertexCount_)
    , dynamic_(other.dynamic_)
{
    std::memcpy(vbos_, other.vbos_, sizeof(vbos_));
    other.vao_ = 0;
    std::memset(other.vbos_, 0, sizeof(other.vbos_));
    other.vertexCount_ = 0;
}

GLMesh& GLMesh::operator=(GLMesh&& other) noexcept {
    if (this != &other) {
        destroy();
        vao_ = other.vao_;
        std::memcpy(vbos_, other.vbos_, sizeof(vbos_));
        vertexCount_ = other.vertexCount_;
        dynamic_ = other.dynamic_;
        other.vao_ = 0;
        std::memset(other.vbos_, 0, sizeof(other.vbos_));
        other.vertexCount_ = 0;
    }
    return *this;
}

// ============================================================================
// Upload (separate buffers)
// ============================================================================

void GLMesh::upload(int vertexCount,
                    const float* positions,
                    const float* texcoords,
                    const float* texcoords2,
                    const float* normals,
                    const std::uint8_t* colors,
                    bool dynamic)
{
    destroy();

    if (vertexCount <= 0 || !positions) return;

    vertexCount_ = vertexCount;
    dynamic_ = dynamic;
    GLenum usage = dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    // Position (attribute 0) — required
    glGenBuffers(1, &vbos_[ATTRIB_POSITION]);
    glBindBuffer(GL_ARRAY_BUFFER, vbos_[ATTRIB_POSITION]);
    glBufferData(GL_ARRAY_BUFFER, vertexCount * 3 * sizeof(float), positions, usage);
    glEnableVertexAttribArray(ATTRIB_POSITION);
    glVertexAttribPointer(ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    // TexCoord (attribute 1)
    if (texcoords) {
        glGenBuffers(1, &vbos_[ATTRIB_TEXCOORD]);
        glBindBuffer(GL_ARRAY_BUFFER, vbos_[ATTRIB_TEXCOORD]);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * 2 * sizeof(float), texcoords, usage);
        glEnableVertexAttribArray(ATTRIB_TEXCOORD);
        glVertexAttribPointer(ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    // TexCoord2 (attribute 2) — foliageMask + AO
    if (texcoords2) {
        glGenBuffers(1, &vbos_[ATTRIB_TEXCOORD2]);
        glBindBuffer(GL_ARRAY_BUFFER, vbos_[ATTRIB_TEXCOORD2]);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * 2 * sizeof(float), texcoords2, usage);
        glEnableVertexAttribArray(ATTRIB_TEXCOORD2);
        glVertexAttribPointer(ATTRIB_TEXCOORD2, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    // Normal (attribute 3)
    if (normals) {
        glGenBuffers(1, &vbos_[ATTRIB_NORMAL]);
        glBindBuffer(GL_ARRAY_BUFFER, vbos_[ATTRIB_NORMAL]);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * 3 * sizeof(float), normals, usage);
        glEnableVertexAttribArray(ATTRIB_NORMAL);
        glVertexAttribPointer(ATTRIB_NORMAL, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    // Color (attribute 4) — RGBA unsigned bytes, normalized
    if (colors) {
        glGenBuffers(1, &vbos_[ATTRIB_COLOR]);
        glBindBuffer(GL_ARRAY_BUFFER, vbos_[ATTRIB_COLOR]);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * 4 * sizeof(std::uint8_t), colors, usage);
        glEnableVertexAttribArray(ATTRIB_COLOR);
        glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, nullptr);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ============================================================================
// Update (orphaning for dynamic meshes)
// ============================================================================

void GLMesh::update(int vertexCount,
                    const float* positions,
                    const float* texcoords,
                    const float* texcoords2,
                    const float* normals,
                    const std::uint8_t* colors)
{
    if (!vao_ || !dynamic_) {
        // If not dynamic or not yet created, do a full upload
        upload(vertexCount, positions, texcoords, texcoords2, normals, colors, true);
        return;
    }

    vertexCount_ = vertexCount;
    GLenum usage = GL_DYNAMIC_DRAW;

    glBindVertexArray(vao_);

    // Orphan + re-upload each buffer
    if (positions && vbos_[ATTRIB_POSITION]) {
        glBindBuffer(GL_ARRAY_BUFFER, vbos_[ATTRIB_POSITION]);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * 3 * sizeof(float), nullptr, usage); // orphan
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * 3 * sizeof(float), positions);
    }

    if (texcoords && vbos_[ATTRIB_TEXCOORD]) {
        glBindBuffer(GL_ARRAY_BUFFER, vbos_[ATTRIB_TEXCOORD]);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * 2 * sizeof(float), nullptr, usage);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * 2 * sizeof(float), texcoords);
    }

    if (texcoords2 && vbos_[ATTRIB_TEXCOORD2]) {
        glBindBuffer(GL_ARRAY_BUFFER, vbos_[ATTRIB_TEXCOORD2]);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * 2 * sizeof(float), nullptr, usage);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * 2 * sizeof(float), texcoords2);
    }

    if (normals && vbos_[ATTRIB_NORMAL]) {
        glBindBuffer(GL_ARRAY_BUFFER, vbos_[ATTRIB_NORMAL]);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * 3 * sizeof(float), nullptr, usage);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * 3 * sizeof(float), normals);
    }

    if (colors && vbos_[ATTRIB_COLOR]) {
        glBindBuffer(GL_ARRAY_BUFFER, vbos_[ATTRIB_COLOR]);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * 4 * sizeof(std::uint8_t), nullptr, usage);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * 4 * sizeof(std::uint8_t), colors);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ============================================================================
// Position-only upload (skybox, fullscreen quad)
// ============================================================================

void GLMesh::uploadPositionOnly(const float* positions, int vertexCount) {
    destroy();
    if (!positions || vertexCount <= 0) return;

    vertexCount_ = vertexCount;
    dynamic_ = false;

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    glGenBuffers(1, &vbos_[ATTRIB_POSITION]);
    glBindBuffer(GL_ARRAY_BUFFER, vbos_[ATTRIB_POSITION]);
    glBufferData(GL_ARRAY_BUFFER, vertexCount * 3 * sizeof(float), positions, GL_STATIC_DRAW);
    glEnableVertexAttribArray(ATTRIB_POSITION);
    glVertexAttribPointer(ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ============================================================================
// Destroy
// ============================================================================

void GLMesh::destroy() {
    for (int i = 0; i < kMaxVBOs; ++i) {
        if (vbos_[i]) {
            glDeleteBuffers(1, &vbos_[i]);
            vbos_[i] = 0;
        }
    }
    if (vao_) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    vertexCount_ = 0;
}

// ============================================================================
// Draw
// ============================================================================

void GLMesh::draw(PrimitiveType mode) const {
    drawGL(toGLPrimitive(mode));
}

void GLMesh::drawGL(GLenum mode) const {
    if (!vao_ || vertexCount_ <= 0) return;
    glBindVertexArray(vao_);
    glDrawArrays(mode, 0, vertexCount_);
    glBindVertexArray(0);
}

// ============================================================================
// Utility mesh generators
// ============================================================================

GLMesh GLMesh::createCube(float size) {
    float s = size * 0.5f;

    // 36 vertices (6 faces * 2 triangles * 3 vertices)
    // clang-format off
    float positions[] = {
        // +X face
        s, -s, -s,  s,  s, -s,  s,  s,  s,
        s, -s, -s,  s,  s,  s,  s, -s,  s,
        // -X face
       -s, -s,  s, -s,  s,  s, -s,  s, -s,
       -s, -s,  s, -s,  s, -s, -s, -s, -s,
        // +Y face
       -s,  s, -s, -s,  s,  s,  s,  s,  s,
       -s,  s, -s,  s,  s,  s,  s,  s, -s,
        // -Y face
       -s, -s,  s, -s, -s, -s,  s, -s, -s,
       -s, -s,  s,  s, -s, -s,  s, -s,  s,
        // +Z face
       -s, -s,  s,  s, -s,  s,  s,  s,  s,
       -s, -s,  s,  s,  s,  s, -s,  s,  s,
        // -Z face
        s, -s, -s, -s, -s, -s, -s,  s, -s,
        s, -s, -s, -s,  s, -s,  s,  s, -s,
    };
    // clang-format on

    GLMesh mesh;
    mesh.uploadPositionOnly(positions, 36);
    return mesh;
}

GLMesh GLMesh::createCubeWithUVs(float size) {
    float s = size * 0.5f;

    // 36 vertices (6 faces * 2 triangles * 3 vertices)
    // Each face maps UV from (0,0) to (1,1)
    // clang-format off
    float positions[] = {
        // +X face
        s, -s, -s,  s,  s, -s,  s,  s,  s,
        s, -s, -s,  s,  s,  s,  s, -s,  s,
        // -X face
       -s, -s,  s, -s,  s,  s, -s,  s, -s,
       -s, -s,  s, -s,  s, -s, -s, -s, -s,
        // +Y face
       -s,  s, -s, -s,  s,  s,  s,  s,  s,
       -s,  s, -s,  s,  s,  s,  s,  s, -s,
        // -Y face
       -s, -s,  s, -s, -s, -s,  s, -s, -s,
       -s, -s,  s,  s, -s, -s,  s, -s,  s,
        // +Z face
       -s, -s,  s,  s, -s,  s,  s,  s,  s,
       -s, -s,  s,  s,  s,  s, -s,  s,  s,
        // -Z face
        s, -s, -s, -s, -s, -s, -s,  s, -s,
        s, -s, -s, -s,  s, -s,  s,  s, -s,
    };
    float texcoords[] = {
        // Each face: two triangles covering full 0-1 UV range
        // +X face
        0.0f, 1.0f,  0.0f, 0.0f,  1.0f, 0.0f,
        0.0f, 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        // -X face
        0.0f, 1.0f,  0.0f, 0.0f,  1.0f, 0.0f,
        0.0f, 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        // +Y face
        0.0f, 1.0f,  0.0f, 0.0f,  1.0f, 0.0f,
        0.0f, 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        // -Y face
        0.0f, 1.0f,  0.0f, 0.0f,  1.0f, 0.0f,
        0.0f, 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        // +Z face
        0.0f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f,
        0.0f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f,
        // -Z face
        0.0f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f,
        0.0f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f,
    };
    // clang-format on

    GLMesh mesh;
    mesh.upload(36, positions, texcoords);
    return mesh;
}

GLMesh GLMesh::createWireframeCube(float size) {
    float s = size * 0.5f;

    // 8 corner vertices
    //   0: (-s, -s, -s)   1: ( s, -s, -s)
    //   2: ( s,  s, -s)   3: (-s,  s, -s)
    //   4: (-s, -s,  s)   5: ( s, -s,  s)
    //   6: ( s,  s,  s)   7: (-s,  s,  s)

    // 12 edges × 2 endpoints = 24 vertices for GL_LINES
    // clang-format off
    float positions[] = {
        // Bottom face edges
        -s, -s, -s,   s, -s, -s,
         s, -s, -s,   s, -s,  s,
         s, -s,  s,  -s, -s,  s,
        -s, -s,  s,  -s, -s, -s,
        // Top face edges
        -s,  s, -s,   s,  s, -s,
         s,  s, -s,   s,  s,  s,
         s,  s,  s,  -s,  s,  s,
        -s,  s,  s,  -s,  s, -s,
        // Vertical edges
        -s, -s, -s,  -s,  s, -s,
         s, -s, -s,   s,  s, -s,
         s, -s,  s,   s,  s,  s,
        -s, -s,  s,  -s,  s,  s,
    };
    // clang-format on

    GLMesh mesh;
    mesh.uploadPositionOnly(positions, 24);
    return mesh;
}

GLMesh GLMesh::createPlane(float width, float depth) {
    float hw = width * 0.5f;
    float hd = depth * 0.5f;

    // 6 vertices (2 triangles) on XZ plane, Y = 0
    // clang-format off
    float positions[] = {
        -hw, 0.0f,  hd,
         hw, 0.0f,  hd,
         hw, 0.0f, -hd,
        -hw, 0.0f,  hd,
         hw, 0.0f, -hd,
        -hw, 0.0f, -hd,
    };
    float texcoords[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
    };
    float normals[] = {
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
    };
    // clang-format on

    GLMesh mesh;
    mesh.upload(6, positions, texcoords, nullptr, normals);
    return mesh;
}

GLMesh GLMesh::createFullscreenTriangle() {
    // One large triangle that covers the entire [-1,1] clip space
    float positions[] = {
        -1.0f, -1.0f, 0.0f,
         3.0f, -1.0f, 0.0f,
        -1.0f,  3.0f, 0.0f,
    };

    GLMesh mesh;
    mesh.uploadPositionOnly(positions, 3);
    return mesh;
}

} // namespace rf
