#pragma once

// Resource loading utilities with VFS support.
//
// In Debug mode (RAYFLOW_USE_PAK=0): reads loose files directly via raylib.
// In Release mode (RAYFLOW_USE_PAK=1): reads from .pak archives via VFS.
//
// Usage:
//   auto tex = resources::load_texture("textures/terrain.png");
//   auto shader = resources::load_shader("shaders/voxel.vs", "shaders/voxel.fs");

#include <raylib.h>
#include <string>

namespace resources {

// Initialize resource system (must be called after InitWindow).
// Automatically sets up VFS with appropriate flags based on RAYFLOW_USE_PAK.
void init();

// Shutdown resource system.
void shutdown();

// Check if using packed assets (.pak) mode.
bool is_pak_mode();

// Load texture from virtual path.
// @param path  Virtual path (e.g., "textures/terrain.png")
// @return Loaded texture, or empty texture on failure.
Texture2D load_texture(const std::string& path);

// Load image from virtual path.
// @param path  Virtual path (e.g., "textures/terrain.png")
// @return Loaded image, or empty image on failure.
Image load_image(const std::string& path);

// Load shader from virtual paths.
// @param vsPath  Vertex shader path (e.g., "shaders/voxel.vs"), or nullptr for default.
// @param fsPath  Fragment shader path (e.g., "shaders/voxel.fs"), or nullptr for default.
// @return Loaded shader, or default shader on failure.
Shader load_shader(const char* vsPath, const char* fsPath);

// Load font from virtual path.
// @param path      Font file path (e.g., "ui/fonts/Inter-Regular.ttf")
// @param fontSize  Font size in pixels.
// @return Loaded font, or default font on failure.
Font load_font(const std::string& path, int fontSize = 20);

// Load text file contents.
// @param path  Virtual path (e.g., "ui/hud.xml")
// @return File contents as string, or empty string on failure.
std::string load_text(const std::string& path);

} // namespace resources
