#include "resources.hpp"

#include "engine/vfs/vfs.hpp"
#include "engine/maps/runtime_paths.hpp"

#include <raylib.h>
#include <cstring>
#include <filesystem>

namespace resources {

namespace {

const char* get_extension(const std::string& path) {
    auto dot = path.rfind('.');
    if (dot == std::string::npos) return "";
    return path.c_str() + dot;
}

} // namespace

void init() {
    std::filesystem::path gameDir = GetApplicationDirectory();
    
    // Set base path for runtime paths (maps, etc.)
    shared::maps::set_base_path(gameDir);

#if RAYFLOW_USE_PAK
    engine::vfs::init(gameDir, engine::vfs::InitFlags::None);

    if (!engine::vfs::mount("assets.pak", "/")) {
        TraceLog(LOG_WARNING, "[resources] assets.pak not found, falling back to loose files");
    } else {
        TraceLog(LOG_INFO, "[resources] Mounted assets.pak (Release mode)");
    }
#else
    engine::vfs::init(gameDir, engine::vfs::InitFlags::LooseOnly);
    TraceLog(LOG_INFO, "[resources] Using loose files (Debug mode)");
#endif
}

void shutdown() {
    engine::vfs::shutdown();
}

bool is_pak_mode() {
#if RAYFLOW_USE_PAK
    return true;
#else
    return false;
#endif
}

Texture2D load_texture(const std::string& path) {
#if RAYFLOW_USE_PAK
    auto data = engine::vfs::read_file(path);
    if (data) {
        const char* ext = get_extension(path);
        Image img = LoadImageFromMemory(ext, data->data(), static_cast<int>(data->size()));
        if (img.data) {
            Texture2D tex = LoadTextureFromImage(img);
            UnloadImage(img);
            return tex;
        }
        TraceLog(LOG_WARNING, "[resources] Failed to decode image: %s", path.c_str());
    } else {
        TraceLog(LOG_WARNING, "[resources] File not found: %s", path.c_str());
    }
    return Texture2D{0};
#else
    return LoadTexture(path.c_str());
#endif
}

Image load_image(const std::string& path) {
#if RAYFLOW_USE_PAK
    auto data = engine::vfs::read_file(path);
    if (data) {
        const char* ext = get_extension(path);
        return LoadImageFromMemory(ext, data->data(), static_cast<int>(data->size()));
    }
    TraceLog(LOG_WARNING, "[resources] File not found: %s", path.c_str());
    return Image{0};
#else
    return LoadImage(path.c_str());
#endif
}

Shader load_shader(const char* vsPath, const char* fsPath) {
#if RAYFLOW_USE_PAK
    std::string vsCode, fsCode;

    if (vsPath) {
        auto vsData = engine::vfs::read_text_file(vsPath);
        if (vsData) {
            vsCode = *vsData;
        } else {
            TraceLog(LOG_WARNING, "[resources] Vertex shader not found: %s", vsPath);
        }
    }

    if (fsPath) {
        auto fsData = engine::vfs::read_text_file(fsPath);
        if (fsData) {
            fsCode = *fsData;
        } else {
            TraceLog(LOG_WARNING, "[resources] Fragment shader not found: %s", fsPath);
        }
    }

    return LoadShaderFromMemory(
        vsCode.empty() ? nullptr : vsCode.c_str(),
        fsCode.empty() ? nullptr : fsCode.c_str()
    );
#else
    return LoadShader(vsPath, fsPath);
#endif
}

Font load_font(const std::string& path, int fontSize) {
#if RAYFLOW_USE_PAK
    TraceLog(LOG_INFO, "[resources] Attempting to load font from PAK: %s", path.c_str());
    auto data = engine::vfs::read_file(path);
    if (data) {
        TraceLog(LOG_INFO, "[resources] Font data loaded from PAK: %zu bytes", data->size());
        const char* ext = get_extension(path);
        TraceLog(LOG_INFO, "[resources] Font extension: %s", ext);
        Font font = LoadFontFromMemory(ext, data->data(), static_cast<int>(data->size()),
                                       fontSize, nullptr, 0);
        if (font.texture.id != 0) {
            TraceLog(LOG_INFO, "[resources] Font loaded successfully from PAK: %s", path.c_str());
            return font;
        }
        TraceLog(LOG_WARNING, "[resources] Failed to decode font: %s", path.c_str());
    } else {
        TraceLog(LOG_WARNING, "[resources] Font file not found in PAK: %s", path.c_str());
    }
    return GetFontDefault();
#else
    return LoadFontEx(path.c_str(), fontSize, nullptr, 0);
#endif
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
    char* text = LoadFileText(path.c_str());
    if (text) {
        std::string result(text);
        UnloadFileText(text);
        return result;
    }
    return "";
#endif
}

} // namespace resources
