#pragma once

// =============================================================================
// GLRenderDevice — OpenGL implementation of RenderDevice
//
// Creates GL resources and manages OpenGL global state.
// =============================================================================

#include "engine/core/export.hpp"
#include "engine/renderer/gpu/render_device.hpp"

#include <glad/gl.h>

namespace rf {

class RAYFLOW_CLIENT_API GLRenderDevice : public RenderDevice {
public:
    GLRenderDevice() = default;
    ~GLRenderDevice() override = default;

    Backend backend() const override { return Backend::OpenGL; }

    // ----- Resource creation -----
    std::unique_ptr<IShader>      createShader() override;
    std::unique_ptr<ITexture>     createTexture() override;
    std::unique_ptr<IMesh>        createMesh() override;
    std::unique_ptr<IFramebuffer> createFramebuffer() override;

    // ----- Global GPU state -----
    void setViewport(int x, int y, int width, int height) override;
    void getViewport(int outViewport[4]) const override;
    void clear(bool color = true, bool depth = true) override;
    void setClearColor(float r, float g, float b, float a = 1.0f) override;
    void setDepthTest(bool enabled) override;
    void setDepthFunc(DepthFunc func) override;
    void setDepthWrite(bool enabled) override;
    void setBlendMode(BlendMode mode) override;
    void setCullMode(CullMode mode) override;
    void setPolygonOffset(bool enabled, float factor = 0.0f, float units = 0.0f) override;
    void present() override;

    // ----- Initialization -----
    bool init(void* nativeWindowHandle, int width, int height) override;
    void shutdown() override;
    void onResize(int width, int height) override;

private:
    bool initialized_{false};
};

} // namespace rf
