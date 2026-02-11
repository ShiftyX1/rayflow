#pragma once

#include "engine/core/math_types.hpp"
#include <string>

namespace resources {

// NOTE(migration): Return types are placeholders.
// Phase 2/3 will replace with GLTexture, GLShader, GLFont.
struct TexturePlaceholder { unsigned int id{0}; };
struct ImagePlaceholder {};
struct ShaderPlaceholder {};
struct FontPlaceholder {};

void init();

void shutdown();

bool is_pak_mode();

TexturePlaceholder load_texture(const std::string& path);

ImagePlaceholder load_image(const std::string& path);

ShaderPlaceholder load_shader(const char* vsPath, const char* fsPath);

FontPlaceholder load_font(const std::string& path, int fontSize = 20);

std::string load_text(const std::string& path);

} // namespace resources
