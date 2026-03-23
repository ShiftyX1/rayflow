#include "gl_shader.hpp"

#include "engine/core/logging.hpp"
#include "engine/client/core/resources.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <vector>

namespace rf {

// ============================================================================
// Move semantics
// ============================================================================

GLShader::~GLShader() {
    destroy();
}

GLShader::GLShader(GLShader&& other) noexcept
    : program_(other.program_)
    , uniformCache_(std::move(other.uniformCache_))
{
    other.program_ = 0;
}

GLShader& GLShader::operator=(GLShader&& other) noexcept {
    if (this != &other) {
        destroy();
        program_ = other.program_;
        uniformCache_ = std::move(other.uniformCache_);
        other.program_ = 0;
    }
    return *this;
}

// ============================================================================
// Compile / Link
// ============================================================================

GLuint GLShader::compileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(logLen + 1, '\0');
        glGetShaderInfoLog(shader, logLen, nullptr, log.data());
        const char* typeStr = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        TraceLog(LOG_ERROR, "[GLShader] %s compile error:\n%s", typeStr, log.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool GLShader::loadFromSource(const std::string& vertexSrc, const std::string& fragmentSrc) {
    destroy();

    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSrc);
    if (!vs) return false;

    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!fs) {
        glDeleteShader(vs);
        return false;
    }

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);

    // Shaders can be deleted after linking
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint success = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLen = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(logLen + 1, '\0');
        glGetProgramInfoLog(program_, logLen, nullptr, log.data());
        TraceLog(LOG_ERROR, "[GLShader] Link error:\n%s", log.data());
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }

    TraceLog(LOG_INFO, "[GLShader] Program %u linked successfully", program_);
    return true;
}

bool GLShader::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string vsSrc = resources::load_text(vertexPath);
    std::string fsSrc = resources::load_text(fragmentPath);

    if (vsSrc.empty()) {
        TraceLog(LOG_ERROR, "[GLShader] Failed to read vertex shader: %s", vertexPath.c_str());
        return false;
    }
    if (fsSrc.empty()) {
        TraceLog(LOG_ERROR, "[GLShader] Failed to read fragment shader: %s", fragmentPath.c_str());
        return false;
    }

    return loadFromSource(vsSrc, fsSrc);
}

void GLShader::destroy() {
    if (program_) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    uniformCache_.clear();
}

// ============================================================================
// Bind / Unbind
// ============================================================================

void GLShader::bind() const {
    if (program_) {
        glUseProgram(program_);
    }
}

void GLShader::unbind() const {
    glUseProgram(0);
}

// ============================================================================
// Uniform locations
// ============================================================================

int GLShader::getUniformLocation(const std::string& name) const {
    auto it = uniformCache_.find(name);
    if (it != uniformCache_.end()) {
        return it->second;
    }
    GLint loc = glGetUniformLocation(program_, name.c_str());
    uniformCache_[name] = loc;
    return loc;
}

// ============================================================================
// Uniform setters (by name)
// ============================================================================

void GLShader::setInt(const std::string& name, int value) const {
    setInt(getUniformLocation(name), value);
}

void GLShader::setFloat(const std::string& name, float value) const {
    setFloat(getUniformLocation(name), value);
}

void GLShader::setVec2(const std::string& name, const Vec2& v) const {
    setVec2(getUniformLocation(name), v);
}

void GLShader::setVec3(const std::string& name, const Vec3& v) const {
    setVec3(getUniformLocation(name), v);
}

void GLShader::setVec4(const std::string& name, const Vec4& v) const {
    setVec4(getUniformLocation(name), v);
}

void GLShader::setMat4(const std::string& name, const Mat4& m) const {
    setMat4(getUniformLocation(name), m);
}

// ============================================================================
// Uniform setters (by location — avoids cache lookup)
// ============================================================================

void GLShader::setInt(int loc, int value) const {
    if (loc >= 0) glUniform1i(loc, value);
}

void GLShader::setFloat(int loc, float value) const {
    if (loc >= 0) glUniform1f(loc, value);
}

void GLShader::setVec2(int loc, const Vec2& v) const {
    if (loc >= 0) glUniform2fv(loc, 1, glm::value_ptr(v));
}

void GLShader::setVec3(int loc, const Vec3& v) const {
    if (loc >= 0) glUniform3fv(loc, 1, glm::value_ptr(v));
}

void GLShader::setVec4(int loc, const Vec4& v) const {
    if (loc >= 0) glUniform4fv(loc, 1, glm::value_ptr(v));
}

void GLShader::setMat4(int loc, const Mat4& m) const {
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m));
}

} // namespace rf
