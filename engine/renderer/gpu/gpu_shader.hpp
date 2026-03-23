#pragma once

// =============================================================================
// IShader — Abstract shader program interface
//
// Backend-agnostic. Implemented by GLShader, DX11Shader, etc.
// =============================================================================

#include "engine/core/export.hpp"
#include "engine/core/math_types.hpp"

#include <string>

namespace rf {

class RAYFLOW_CLIENT_API IShader {
public:
    virtual ~IShader() = default;

    // Non-copyable
    IShader(const IShader&) = delete;
    IShader& operator=(const IShader&) = delete;

    /// Compile and link from source strings.
    virtual bool loadFromSource(const std::string& vertexSrc, const std::string& fragmentSrc) = 0;

    /// Compile and link from file paths.
    virtual bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) = 0;

    /// Release GPU resources.
    virtual void destroy() = 0;

    /// Bind this shader as active.
    virtual void bind() const = 0;

    /// Unbind any shader.
    virtual void unbind() const = 0;

    /// Is the shader valid (compiled & linked)?
    virtual bool isValid() const = 0;

    // ----- Uniform setters (by name) -----

    virtual void setInt(const std::string& name, int value) const = 0;
    virtual void setFloat(const std::string& name, float value) const = 0;
    virtual void setVec2(const std::string& name, const Vec2& v) const = 0;
    virtual void setVec3(const std::string& name, const Vec3& v) const = 0;
    virtual void setVec4(const std::string& name, const Vec4& v) const = 0;
    virtual void setMat4(const std::string& name, const Mat4& m) const = 0;

    // ----- Uniform setters (by location/index) -----

    virtual void setInt(int loc, int value) const = 0;
    virtual void setFloat(int loc, float value) const = 0;
    virtual void setVec2(int loc, const Vec2& v) const = 0;
    virtual void setVec3(int loc, const Vec3& v) const = 0;
    virtual void setVec4(int loc, const Vec4& v) const = 0;
    virtual void setMat4(int loc, const Mat4& m) const = 0;

    /// Get uniform location by name (returns -1 if not found).
    virtual int getUniformLocation(const std::string& name) const = 0;

protected:
    IShader() = default;
    IShader(IShader&&) noexcept = default;
    IShader& operator=(IShader&&) noexcept = default;
};

} // namespace rf
