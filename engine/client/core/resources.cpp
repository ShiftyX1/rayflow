#include "resources.hpp"

#include "engine/vfs/vfs.hpp"
#include "engine/maps/runtime_paths.hpp"
#include "engine/core/math_types.hpp"
#include "engine/core/logging.hpp"
#include "engine/renderer/gpu/render_device.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace resources {

namespace {

rf::RenderDevice* s_device = nullptr;

const char* get_extension(const std::string& path) {
    auto dot = path.rfind('.');
    if (dot == std::string::npos) return "";
    return path.c_str() + dot;
}

std::filesystem::path get_executable_dir() {
#if defined(_WIN32)
    constexpr unsigned long kMaxPath = 260;
    char buffer[kMaxPath] = {0};
    GetModuleFileNameA(nullptr, buffer, kMaxPath);
    return std::filesystem::path(buffer).parent_path();
#elif defined(__APPLE__)
    char buffer[1024] = {0};
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        return std::filesystem::path(buffer).parent_path();
    }
    return std::filesystem::current_path();
#elif defined(__linux__)
    char buffer[1024] = {0};
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len > 0) {
        buffer[len] = '\0';
        return std::filesystem::path(buffer).parent_path();
    }
    return std::filesystem::current_path();
#else
    return std::filesystem::current_path();
#endif
}

} // namespace

void set_render_device(rf::RenderDevice* device) {
    s_device = device;
}

rf::RenderDevice* render_device() {
    return s_device;
}

void init() {
    std::filesystem::path gameDir = get_executable_dir();
    
    // Set base path for runtime paths (maps, etc.)
    shared::maps::set_base_path(gameDir);

#if RAYFLOW_USE_PAK
    engine::vfs::init(gameDir, engine::vfs::InitFlags::None);

    if (!engine::vfs::mount("assets.pak", "/")) {
        TraceLog(LOG_WARNING, "[resources] assets.pak not found, falling back to loose files");
    } else {
        TraceLog(LOG_INFO, "[resources] Mounted assets.pak (Release mode)");
    }
    
    // Mount scripts.pak (separate archive for game scripts, allows independent updates)
    if (engine::vfs::mount("scripts.pak", "/")) {
        TraceLog(LOG_INFO, "[resources] Mounted scripts.pak");
    }
#else
    engine::vfs::init(gameDir, engine::vfs::InitFlags::LooseOnly);
    TraceLog(LOG_INFO, "[resources] Using loose files (Debug mode)");
#endif
}

void shutdown() {
    s_device = nullptr;
    engine::vfs::shutdown();
}

bool is_pak_mode() {
#if RAYFLOW_USE_PAK
    return true;
#else
    return false;
#endif
}

std::unique_ptr<rf::ITexture> load_texture(const std::string& path) {
    if (!s_device) {
        TraceLog(LOG_ERROR, "[resources] No render device set, cannot load texture: %s", path.c_str());
        return nullptr;
    }
    auto tex = s_device->createTexture();
    if (!tex->loadFromFile(path)) {
        TraceLog(LOG_WARNING, "[resources] Failed to load texture: %s", path.c_str());
        return nullptr;
    }
    return tex;
}

std::unique_ptr<rf::ITexture> load_image(const std::string& path) {
    if (!s_device) {
        TraceLog(LOG_ERROR, "[resources] No render device set, cannot load image: %s", path.c_str());
        return nullptr;
    }
    auto tex = s_device->createTexture();
    tex->retainPixelData(true);
    if (!tex->loadFromFile(path)) {
        TraceLog(LOG_WARNING, "[resources] Failed to load image: %s", path.c_str());
        return nullptr;
    }
    return tex;
}

std::unique_ptr<rf::IShader> load_shader(const char* vsPath, const char* fsPath) {
    if (!s_device) {
        TraceLog(LOG_ERROR, "[resources] No render device set, cannot load shader");
        return nullptr;
    }
    auto shader = s_device->createShader();
    if (!shader->loadFromFiles(vsPath ? vsPath : "", fsPath ? fsPath : "")) {
        TraceLog(LOG_WARNING, "[resources] Failed to load shader: vs=%s fs=%s",
                   vsPath ? vsPath : "(null)", fsPath ? fsPath : "(null)");
        return nullptr;
    }
    return shader;
}

std::unique_ptr<rf::IMesh> create_cube(float size) {
    if (!s_device) return nullptr;
    float s = size * 0.5f;
    // clang-format off
    float positions[] = {
        s, -s, -s,  s,  s, -s,  s,  s,  s,
        s, -s, -s,  s,  s,  s,  s, -s,  s,
       -s, -s,  s, -s,  s,  s, -s,  s, -s,
       -s, -s,  s, -s,  s, -s, -s, -s, -s,
       -s,  s, -s, -s,  s,  s,  s,  s,  s,
       -s,  s, -s,  s,  s,  s,  s,  s, -s,
       -s, -s,  s, -s, -s, -s,  s, -s, -s,
       -s, -s,  s,  s, -s, -s,  s, -s,  s,
       -s, -s,  s,  s, -s,  s,  s,  s,  s,
       -s, -s,  s,  s,  s,  s, -s,  s,  s,
        s, -s, -s, -s, -s, -s, -s,  s, -s,
        s, -s, -s, -s,  s, -s,  s,  s, -s,
    };
    // clang-format on
    auto mesh = s_device->createMesh();
    mesh->uploadPositionOnly(positions, 36);
    return mesh;
}

std::unique_ptr<rf::IMesh> create_wireframe_cube(float size) {
    if (!s_device) return nullptr;
    float s = size * 0.5f;
    // clang-format off
    float positions[] = {
        -s, -s, -s,   s, -s, -s,
         s, -s, -s,   s, -s,  s,
         s, -s,  s,  -s, -s,  s,
        -s, -s,  s,  -s, -s, -s,
        -s,  s, -s,   s,  s, -s,
         s,  s, -s,   s,  s,  s,
         s,  s,  s,  -s,  s,  s,
        -s,  s,  s,  -s,  s, -s,
        -s, -s, -s,  -s,  s, -s,
         s, -s, -s,   s,  s, -s,
         s, -s,  s,   s,  s,  s,
        -s, -s,  s,  -s,  s,  s,
    };
    // clang-format on
    auto mesh = s_device->createMesh();
    mesh->uploadPositionOnly(positions, 24);
    return mesh;
}

std::unique_ptr<rf::IMesh> create_cube_with_uvs(float size) {
    if (!s_device) return nullptr;
    float s = size * 0.5f;
    // clang-format off
    float positions[] = {
        s, -s, -s,  s,  s, -s,  s,  s,  s,
        s, -s, -s,  s,  s,  s,  s, -s,  s,
       -s, -s,  s, -s,  s,  s, -s,  s, -s,
       -s, -s,  s, -s,  s, -s, -s, -s, -s,
       -s,  s, -s, -s,  s,  s,  s,  s,  s,
       -s,  s, -s,  s,  s,  s,  s,  s, -s,
       -s, -s,  s, -s, -s, -s,  s, -s, -s,
       -s, -s,  s,  s, -s, -s,  s, -s,  s,
       -s, -s,  s,  s, -s,  s,  s,  s,  s,
       -s, -s,  s,  s,  s,  s, -s,  s,  s,
        s, -s, -s, -s, -s, -s, -s,  s, -s,
        s, -s, -s, -s,  s, -s,  s,  s, -s,
    };
    float texcoords[] = {
        0.0f, 1.0f,  0.0f, 0.0f,  1.0f, 0.0f,
        0.0f, 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        0.0f, 1.0f,  0.0f, 0.0f,  1.0f, 0.0f,
        0.0f, 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        0.0f, 1.0f,  0.0f, 0.0f,  1.0f, 0.0f,
        0.0f, 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        0.0f, 1.0f,  0.0f, 0.0f,  1.0f, 0.0f,
        0.0f, 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        0.0f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f,
        0.0f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f,
        0.0f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f,
        0.0f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f,
    };
    // clang-format on
    auto mesh = s_device->createMesh();
    mesh->upload(36, positions, texcoords);
    return mesh;
}

rf::GLFont load_font(const std::string& path, int fontSize) {
    rf::GLFont font;
    if (!font.loadFromFile(path, static_cast<float>(fontSize))) {
        TraceLog(LOG_WARNING, "[resources] Failed to load font: %s (size %d)", path.c_str(), fontSize);
    }
    return font;
}

std::string load_text(const std::string& path) {
#if RAYFLOW_USE_PAK
    auto data = engine::vfs::read_text_file(path);
    if (data) {
        return *data;
    }
    TraceLog(LOG_WARNING, "[resources] Text file not found: %s", path.c_str());
    return "";
#else
    std::ifstream file(path);
    if (file.is_open()) {
        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }
    TraceLog(LOG_WARNING, "[resources] Failed to read text file: %s", path.c_str());
    return "";
#endif
}

} // namespace resources
