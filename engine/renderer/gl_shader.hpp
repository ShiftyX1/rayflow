#pragma once

// =============================================================================
// GLShader — OpenGL shader program wrapper
//
// Phase 2: Replaces raylib Shader / LoadShader / SetShaderValue etc.
// Compiles vertex + fragment GLSL sources, links, manages uniform locations.
// =============================================================================

#include "engine/core/export.hpp"
#include "engine/core/math_types.hpp"
#include "engine/renderer/gpu/gpu_shader.hpp"

#include <glad/gl.h>

#include <string>
#include <unordered_map>

namespace rf {

class RAYFLOW_CLIENT_API GLShader : public IShader {
public:
    GLShader() = default;
    ~GLShader() override;

    // Non-copyable, movable
    GLShader(const GLShader&) = delete;
    GLShader& operator=(const GLShader&) = delete;
    GLShader(GLShader&& other) noexcept;
    GLShader& operator=(GLShader&& other) noexcept;

    /// Compile and link from source strings.
    /// @return true on success.
    bool loadFromSource(const std::string& vertexSrc, const std::string& fragmentSrc) override;

    /// Compile and link from file paths (reads via resources::load_text).
    /// @return true on success.
    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) override;

    /// Destroy the GL program.
    void destroy() override;

    /// Bind / unbind this program.
    void bind() const override;
    void unbind() const override;

    /// Is the shader valid (linked successfully)?
    bool isValid() const override { return program_ != 0; }
    GLuint id() const { return program_; }

    // ----- Uniform setters -----

    int getUniformLocation(const std::string& name) const override;

    void setInt(const std::string& name, int value) const override;
    void setFloat(const std::string& name, float value) const override;
    void setVec2(const std::string& name, const Vec2& v) const override;
    void setVec3(const std::string& name, const Vec3& v) const override;
    void setVec4(const std::string& name, const Vec4& v) const override;
    void setMat4(const std::string& name, const Mat4& m) const override;

    // Direct location overloads (avoid repeated hash-map lookup)
    void setInt(int loc, int value) const override;
    void setFloat(int loc, float value) const override;
    void setVec2(int loc, const Vec2& v) const override;
    void setVec3(int loc, const Vec3& v) const override;
    void setVec4(int loc, const Vec4& v) const override;
    void setMat4(int loc, const Mat4& m) const override;

private:
    static GLuint compileShader(GLenum type, const std::string& source);

    GLuint program_{0};
    mutable std::unordered_map<std::string, GLint> uniformCache_;
};

} // namespace rf
