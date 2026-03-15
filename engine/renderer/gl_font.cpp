#include "gl_font.hpp"

#include "engine/core/logging.hpp"
#include "engine/client/core/resources.hpp"
#include "engine/vfs/vfs.hpp"

#include <stb_truetype.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <vector>

namespace rf {

// ============================================================================
// Static members
// ============================================================================

GLShader GLFont::sTextShader_;
Mat4     GLFont::sProjection_{1.0f};
bool     GLFont::sShaderInitialised_{false};
GLuint   GLFont::sVAO_{0};
GLuint   GLFont::sVBO_{0};
bool     GLFont::sQuadInitialised_{false};

// ============================================================================
// Move semantics / destructor
// ============================================================================

GLFont::~GLFont() {
    destroy();
}

GLFont::GLFont(GLFont&& other) noexcept
    : fontSize_(other.fontSize_)
    , atlasWidth_(other.atlasWidth_)
    , atlasHeight_(other.atlasHeight_)
    , atlasTexture_(other.atlasTexture_)
    , ascent_(other.ascent_)
    , descent_(other.descent_)
    , lineGap_(other.lineGap_)
{
    std::memcpy(glyphs_, other.glyphs_, sizeof(glyphs_));
    other.atlasTexture_ = 0;
    other.fontSize_ = 0;
}

GLFont& GLFont::operator=(GLFont&& other) noexcept {
    if (this != &other) {
        destroy();
        fontSize_     = other.fontSize_;
        atlasWidth_   = other.atlasWidth_;
        atlasHeight_  = other.atlasHeight_;
        atlasTexture_ = other.atlasTexture_;
        ascent_       = other.ascent_;
        descent_      = other.descent_;
        lineGap_      = other.lineGap_;
        std::memcpy(glyphs_, other.glyphs_, sizeof(glyphs_));
        other.atlasTexture_ = 0;
        other.fontSize_ = 0;
    }
    return *this;
}

// ============================================================================
// Loading
// ============================================================================

bool GLFont::loadFromFile(const std::string& path, float fontSize) {
    // Try VFS first, then filesystem
    std::vector<std::uint8_t> fileData;

#if RAYFLOW_USE_PAK
    auto vfsData = engine::vfs::read_file(path);
    if (vfsData) {
        fileData = std::move(*vfsData);
    }
#endif

    if (fileData.empty()) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            TraceLog(LOG_WARNING, "[GLFont] Failed to open font file: %s", path.c_str());
            return false;
        }
        auto size = file.tellg();
        file.seekg(0);
        fileData.resize(static_cast<size_t>(size));
        file.read(reinterpret_cast<char*>(fileData.data()), size);
    }

    return loadFromMemory(fileData.data(), static_cast<int>(fileData.size()), fontSize);
}

bool GLFont::loadFromMemory(const std::uint8_t* data, int dataSize, float fontSize) {
    if (!data || dataSize <= 0 || fontSize <= 0.0f) return false;
    destroy();
    fontSize_ = fontSize;
    return bakeAtlas(data, dataSize);
}

void GLFont::destroy() {
    if (atlasTexture_) {
        glDeleteTextures(1, &atlasTexture_);
        atlasTexture_ = 0;
    }
    fontSize_ = 0;
}

// ============================================================================
// Atlas baking
// ============================================================================

bool GLFont::bakeAtlas(const std::uint8_t* ttfData, int ttfLen) {
    (void)ttfLen;

    stbtt_fontinfo fontInfo;
    if (!stbtt_InitFont(&fontInfo, ttfData, stbtt_GetFontOffsetForIndex(ttfData, 0))) {
        TraceLog(LOG_WARNING, "[GLFont] stbtt_InitFont failed");
        return false;
    }

    float scale = stbtt_ScaleForPixelHeight(&fontInfo, fontSize_);

    // Font metrics
    int iAscent, iDescent, iLineGap;
    stbtt_GetFontVMetrics(&fontInfo, &iAscent, &iDescent, &iLineGap);
    ascent_  = iAscent  * scale;
    descent_ = iDescent * scale;
    lineGap_ = iLineGap * scale;

    // Choose atlas size — start small, grow if necessary
    atlasWidth_  = 512;
    atlasHeight_ = 512;

    // We'll repeatedly try packing at increasing sizes
    std::vector<std::uint8_t> bitmap;
    stbtt_pack_context packCtx;
    stbtt_packedchar packedChars[kCharCount];

    for (int attempt = 0; attempt < 4; ++attempt) {
        bitmap.assign(atlasWidth_ * atlasHeight_, 0);

        if (!stbtt_PackBegin(&packCtx, bitmap.data(), atlasWidth_, atlasHeight_, 0, 1, nullptr)) {
            TraceLog(LOG_WARNING, "[GLFont] stbtt_PackBegin failed");
            return false;
        }

        stbtt_PackSetOversampling(&packCtx, 2, 2); // 2x oversampling for quality

        int result = stbtt_PackFontRange(&packCtx, ttfData, 0, fontSize_,
                                         kFirstChar, kCharCount, packedChars);
        stbtt_PackEnd(&packCtx);

        if (result != 0) break; // success

        // Double atlas size and retry
        if (atlasWidth_ <= atlasHeight_) {
            atlasWidth_ *= 2;
        } else {
            atlasHeight_ *= 2;
        }
    }

    // Fill glyph info
    float invW = 1.0f / static_cast<float>(atlasWidth_);
    float invH = 1.0f / static_cast<float>(atlasHeight_);

    for (int i = 0; i < kCharCount; ++i) {
        const auto& pc = packedChars[i];
        auto& g = glyphs_[i];

        // Quad offset relative to cursor (already in pixel coords at baked size)
        g.x0 = pc.xoff;
        g.y0 = pc.yoff;
        g.x1 = pc.xoff2;
        g.y1 = pc.yoff2;

        // UV in atlas
        g.s0 = pc.x0 * invW;
        g.t0 = pc.y0 * invH;
        g.s1 = pc.x1 * invW;
        g.t1 = pc.y1 * invH;

        g.xAdvance = pc.xadvance;
    }

    // Upload to GL texture (single-channel alpha)
    glGenTextures(1, &atlasTexture_);
    glBindTexture(GL_TEXTURE_2D, atlasTexture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, atlasWidth_, atlasHeight_,
                 0, GL_RED, GL_UNSIGNED_BYTE, bitmap.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    TraceLog(LOG_INFO, "[GLFont] Atlas baked: %dx%d, %.0fpx", atlasWidth_, atlasHeight_, fontSize_);
    return true;
}

// ============================================================================
// Text shader (shared by all GLFont instances)
// ============================================================================

static const char* kTextVertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

out vec2 vUV;
out vec4 vColor;

uniform mat4 uProjection;

void main() {
    vUV    = aUV;
    vColor = aColor;
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
)";

static const char* kTextFragSrc = R"(
#version 330 core
in vec2 vUV;
in vec4 vColor;

out vec4 FragColor;

uniform sampler2D uAtlas;

void main() {
    float a = texture(uAtlas, vUV).r;
    FragColor = vec4(vColor.rgb, vColor.a * a);
}
)";

GLShader& GLFont::textShader() {
    if (!sShaderInitialised_) {
        if (!sTextShader_.loadFromSource(kTextVertSrc, kTextFragSrc)) {
            TraceLog(LOG_ERROR, "[GLFont] Failed to compile text shader");
        }
        sShaderInitialised_ = true;
    }
    return sTextShader_;
}

// ============================================================================
// Shared quad VAO/VBO
// ============================================================================

void GLFont::ensureQuad() {
    if (sQuadInitialised_) return;

    glGenVertexArrays(1, &sVAO_);
    glGenBuffers(1, &sVBO_);

    glBindVertexArray(sVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, sVBO_);

    // We'll upload data dynamically per drawText call.
    // Vertex layout: vec2 pos, vec2 uv, vec4 color = 8 floats per vertex
    constexpr int kStride = 8 * sizeof(float);

    // pos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, kStride, (void*)0);

    // uv
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, kStride, (void*)(2 * sizeof(float)));

    // color
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, kStride, (void*)(4 * sizeof(float)));

    glBindVertexArray(0);
    sQuadInitialised_ = true;
}

// ============================================================================
// Projection
// ============================================================================

void GLFont::setProjection(float width, float height) {
    sProjection_ = glm::ortho(0.0f, width, height, 0.0f, -1.0f, 1.0f);
}

// ============================================================================
// drawText
// ============================================================================

void GLFont::drawText(const std::string& text, float x, float y,
                      float size, const Color& color) const
{
    if (!isValid() || text.empty()) return;

    ensureQuad();
    auto& shader = textShader();
    if (!shader.isValid()) return;

    float scale = size / fontSize_;
    float cr = color.r / 255.0f;
    float cg = color.g / 255.0f;
    float cb = color.b / 255.0f;
    float ca = color.a / 255.0f;

    // Build vertex data: 6 verts (2 triangles) per character
    std::vector<float> verts;
    verts.reserve(text.size() * 6 * 8);

    float cursorX = x;
    float cursorY = y + ascent_ * scale; // baseline offset

    for (char ch : text) {
        int ci = static_cast<int>(ch) - kFirstChar;
        if (ci < 0 || ci >= kCharCount) {
            cursorX += glyphs_[0].xAdvance * scale; // space-width fallback
            continue;
        }

        const auto& g = glyphs_[ci];

        float x0 = cursorX + g.x0 * scale;
        float y0 = cursorY + g.y0 * scale;
        float x1 = cursorX + g.x1 * scale;
        float y1 = cursorY + g.y1 * scale;

        // Triangle 1
        verts.insert(verts.end(), {x0, y0, g.s0, g.t0, cr, cg, cb, ca});
        verts.insert(verts.end(), {x1, y0, g.s1, g.t0, cr, cg, cb, ca});
        verts.insert(verts.end(), {x1, y1, g.s1, g.t1, cr, cg, cb, ca});

        // Triangle 2
        verts.insert(verts.end(), {x0, y0, g.s0, g.t0, cr, cg, cb, ca});
        verts.insert(verts.end(), {x1, y1, g.s1, g.t1, cr, cg, cb, ca});
        verts.insert(verts.end(), {x0, y1, g.s0, g.t1, cr, cg, cb, ca});

        cursorX += g.xAdvance * scale;
    }

    if (verts.empty()) return;

    // Upload + draw
    shader.bind();
    shader.setMat4("uProjection", sProjection_);
    shader.setInt("uAtlas", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTexture_);

    glBindVertexArray(sVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, sVBO_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(), GL_DYNAMIC_DRAW);

    // Disable depth test for 2D text overlay
    GLboolean depthWasEnabled;
    glGetBooleanv(GL_DEPTH_TEST, &depthWasEnabled);
    if (depthWasEnabled) glDisable(GL_DEPTH_TEST);

    int vertCount = static_cast<int>(verts.size()) / 8;
    glDrawArrays(GL_TRIANGLES, 0, vertCount);

    if (depthWasEnabled) glEnable(GL_DEPTH_TEST);

    glBindVertexArray(0);
    GLShader::unbind();
}

// ============================================================================
// measureText
// ============================================================================

float GLFont::measureText(const std::string& text, float size) const {
    if (!isValid() || text.empty()) return 0.0f;

    float scale = size / fontSize_;
    float width = 0.0f;

    for (char ch : text) {
        int ci = static_cast<int>(ch) - kFirstChar;
        if (ci < 0 || ci >= kCharCount) {
            width += glyphs_[0].xAdvance * scale;
            continue;
        }
        width += glyphs_[ci].xAdvance * scale;
    }
    return width;
}

float GLFont::lineHeight(float size) const {
    if (!isValid()) return size;
    float scale = size / fontSize_;
    return (ascent_ - descent_ + lineGap_) * scale;
}

// ============================================================================
// Default font
// ============================================================================

static GLFont* sDefaultFont = nullptr;

GLFont* GLFont::defaultFont() {
    if (sDefaultFont && sDefaultFont->isValid()) {
        return sDefaultFont;
    }

    // Try to load a default font from common system paths
    static GLFont font;

    const char* systemFonts[] = {
#if defined(__APPLE__)
        "/System/Library/Fonts/SFNSMono.ttf",
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Helvetica.ttc",
        "/Library/Fonts/Arial.ttf",
#elif defined(_WIN32)
        "C:\\Windows\\Fonts\\consola.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf",
#endif
    };

    for (const char* path : systemFonts) {
        if (font.loadFromFile(path, 20.0f)) {
            sDefaultFont = &font;
            TraceLog(LOG_INFO, "[GLFont] Default font loaded: %s", path);
            return sDefaultFont;
        }
    }

    TraceLog(LOG_WARNING, "[GLFont] No system font found for default font");
    return nullptr;
}

} // namespace rf
