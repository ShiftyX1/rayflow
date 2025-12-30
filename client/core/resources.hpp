#pragma once

#include <raylib.h>
#include <string>

namespace resources {

void init();

void shutdown();

bool is_pak_mode();

Texture2D load_texture(const std::string& path);

Image load_image(const std::string& path);

Shader load_shader(const char* vsPath, const char* fsPath);

Font load_font(const std::string& path, int fontSize = 20);

std::string load_text(const std::string& path);

} // namespace resources
