#pragma once

#include "engine/core/export.hpp"
#include "engine/renderer/gpu/gpu_texture.hpp"
#include "engine/renderer/gpu/gpu_shader.hpp"
#include "engine/renderer/gpu/gpu_mesh.hpp"
#include "engine/renderer/gl_font.hpp"
#include <memory>
#include <string>

namespace rf { class RenderDevice; }

namespace resources {

RAYFLOW_CLIENT_API void init();

RAYFLOW_CLIENT_API void shutdown();

RAYFLOW_CLIENT_API bool is_pak_mode();

/// Set the active render device used for creating GPU resources.
RAYFLOW_CLIENT_API void set_render_device(rf::RenderDevice* device);

/// Get the active render device.
RAYFLOW_CLIENT_API rf::RenderDevice* render_device();

/// Load a 2D texture from a file path. Caller takes ownership.
RAYFLOW_CLIENT_API std::unique_ptr<rf::ITexture> load_texture(const std::string& path);

/// Load a texture and retain CPU pixel data (for colormap sampling etc.).
RAYFLOW_CLIENT_API std::unique_ptr<rf::ITexture> load_image(const std::string& path);

/// Load and compile a shader program from vertex/fragment paths.
RAYFLOW_CLIENT_API std::unique_ptr<rf::IShader> load_shader(const char* vsPath, const char* fsPath);

/// Create a unit cube mesh (positions + normals).
RAYFLOW_CLIENT_API std::unique_ptr<rf::IMesh> create_cube(float size = 1.0f);

/// Create a wireframe cube mesh (positions only, drawn as lines).
RAYFLOW_CLIENT_API std::unique_ptr<rf::IMesh> create_wireframe_cube(float size = 1.0f);

/// Create a cube mesh with UVs (for textured overlays).
RAYFLOW_CLIENT_API std::unique_ptr<rf::IMesh> create_cube_with_uvs(float size = 1.0f);

/// Load a TTF font at the given pixel size. Caller takes ownership.
RAYFLOW_CLIENT_API rf::GLFont load_font(const std::string& path, int fontSize = 20);

RAYFLOW_CLIENT_API std::string load_text(const std::string& path);

} // namespace resources
