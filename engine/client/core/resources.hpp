#pragma once

#include "engine/renderer/gl_texture.hpp"
#include "engine/renderer/gl_shader.hpp"
#include <string>

namespace resources {

// NOTE(migration): FontPlaceholder remains until Phase 3 (text rendering).
struct FontPlaceholder {};

void init();

void shutdown();

bool is_pak_mode();

/// Load a 2D texture from a file path. Caller takes ownership.
rf::GLTexture load_texture(const std::string& path);

/// Load a texture and retain CPU pixel data (for colormap sampling etc.).
rf::GLTexture load_image(const std::string& path);

/// Load and compile a shader program from vertex/fragment paths.
rf::GLShader load_shader(const char* vsPath, const char* fsPath);

FontPlaceholder load_font(const std::string& path, int fontSize = 20);

std::string load_text(const std::string& path);

} // namespace resources
