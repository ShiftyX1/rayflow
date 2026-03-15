#pragma once

// =============================================================================
// GLShader — OpenGL shader program wrapper
//
// Phase 2: Replaces raylib Shader / LoadShader / SetShaderValue etc.
// Compiles vertex + fragment GLSL sources, links, manages uniform locations.
// =============================================================================

#include "engine/core/export.hpp"
#include "engine/core/math_types.hpp"

#include <glad/gl.h>

#include <string>
#include <unordered_map>

namespace rf {

class RAYFLOW_CLIENT_API GLShader {
public:
    GLShader() = default;
    ~GLShader();

    // Non-copyable, movable
    GLShader(const GLShader&) = delete;
    GLShader& operator=(const GLShader&) = delete;
    GLShader(GLShader&& other) noexcept;
    GLShader& operator=(GLShader&& other) noexcept;

    /// Compile and link from source strings.
    /// @return true on success.
    bool loadFromSource(const std::string& vertexSrc, const std::string& fragmentSrc);

    /// Compile and link from file paths (reads via resources::load_text).
    /// @return true on success.
    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);

    /// Destroy the GL program.
    void destroy();

    /// Bind / unbind this program.
    void bind() const;
    static void unbind();

    /// Is the shader valid (linked successfully)?
    bool isValid() const { return program_ != 0; }
    GLuint id() const { return program_; }

    // ----- Uniform setters -----

    GLint getUniformLocation(const std::string& name) const;

    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec2(const std::string& name, const Vec2& v) const;
    void setVec3(const std::string& name, const Vec3& v) const;
    void setVec4(const std::string& name, const Vec4& v) const;
    void setMat4(const std::string& name, const Mat4& m) const;

    // Direct location overloads (avoid repeated hash-map lookup)
    void setInt(GLint loc, int value) const;
    void setFloat(GLint loc, float value) const;
    void setVec2(GLint loc, const Vec2& v) const;
    void setVec3(GLint loc, const Vec3& v) const;
    void setVec4(GLint loc, const Vec4& v) const;
    void setMat4(GLint loc, const Mat4& m) const;

private:
    static GLuint compileShader(GLenum type, const std::string& source);

    GLuint program_{0};
    mutable std::unordered_map<std::string, GLint> uniformCache_;
};

} // namespace rf
