#pragma once

#include "engine/renderer/gl_texture.hpp"
#include "engine/renderer/gl_shader.hpp"
#include "engine/renderer/gl_font.hpp"
#include <string>

namespace resources {

void init();

void shutdown();

bool is_pak_mode();

/// Load a 2D texture from a file path. Caller takes ownership.
rf::GLTexture load_texture(const std::string& path);

/// Load a texture and retain CPU pixel data (for colormap sampling etc.).
rf::GLTexture load_image(const std::string& path);

/// Load and compile a shader program from vertex/fragment paths.
rf::GLShader load_shader(const char* vsPath, const char* fsPath);

/// Load a TTF font at the given pixel size. Caller takes ownership.
rf::GLFont load_font(const std::string& path, int fontSize = 20);

std::string load_text(const std::string& path);

} // namespace resources
