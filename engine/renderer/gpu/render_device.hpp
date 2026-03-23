#pragma once

// =============================================================================
// RenderDevice — Abstract GPU device & resource factory
//
// Central point for creating GPU resources and managing global render state.
// Each backend (OpenGL, DirectX 11) provides a concrete implementation.
//
// Usage:
//   auto device = rf::createRenderDevice(rf::Backend::OpenGL);
//   auto shader = device->createShader();
//   shader->loadFromFiles("voxel.vs", "voxel.fs");
// =============================================================================

#include "engine/core/export.hpp"
#include "engine/core/math_types.hpp"
#include "engine/renderer/gpu/gpu_types.hpp"

#include <memory>

namespace rf {

class IShader;
class ITexture;
class IMesh;
class IFramebuffer;

class RAYFLOW_CLIENT_API RenderDevice {
public:
    virtual ~RenderDevice() = default;

    // Non-copyable
    RenderDevice(const RenderDevice&) = delete;
    RenderDevice& operator=(const RenderDevice&) = delete;

    /// Which backend this device represents.
    virtual Backend backend() const = 0;

    // ----- Resource creation -----

    virtual std::unique_ptr<IShader>      createShader() = 0;
    virtual std::unique_ptr<ITexture>     createTexture() = 0;
    virtual std::unique_ptr<IMesh>        createMesh() = 0;
    virtual std::unique_ptr<IFramebuffer> createFramebuffer() = 0;

    // ----- Global GPU state -----

    /// Set the viewport rectangle (in pixels).
    virtual void setViewport(int x, int y, int width, int height) = 0;

    /// Get the current viewport.
    virtual void getViewport(int outViewport[4]) const = 0;

    /// Clear color and/or depth buffers.
    virtual void clear(bool color = true, bool depth = true) = 0;

    /// Set the clear color.
    virtual void setClearColor(float r, float g, float b, float a = 1.0f) = 0;

    /// Enable or disable depth testing.
    virtual void setDepthTest(bool enabled) = 0;

    /// Set the depth comparison function.
    virtual void setDepthFunc(DepthFunc func) = 0;

    /// Enable or disable writing to the depth buffer.
    virtual void setDepthWrite(bool enabled) = 0;

    /// Set blending mode.
    virtual void setBlendMode(BlendMode mode) = 0;

    /// Set face culling mode.
    virtual void setCullMode(CullMode mode) = 0;

    /// Enable or disable polygon offset (depth bias).
    virtual void setPolygonOffset(bool enabled, float factor = 0.0f, float units = 0.0f) = 0;

    /// Present the back buffer (swap chain present / buffer swap).
    /// For OpenGL this is a no-op (GLFW handles swap). For DX11 it calls Present().
    virtual void present() = 0;

    // ----- Backend initialization -----

    /// Initialize the render device for a given window.
    /// @param nativeWindowHandle  Platform-specific window handle (HWND on Windows, GLFWwindow* for GL).
    /// @param width, height       Initial framebuffer size.
    virtual bool init(void* nativeWindowHandle, int width, int height) = 0;

    /// Shutdown and release all device resources.
    virtual void shutdown() = 0;

    /// Handle window resize (recreate swap chain, etc.).
    virtual void onResize(int width, int height) = 0;

    /// Return the native graphics device pointer (ID3D11Device*, etc.) or nullptr.
    virtual void* nativeDevice() const { return nullptr; }

    /// Return the native device context pointer (ID3D11DeviceContext*, etc.) or nullptr.
    virtual void* nativeContext() const { return nullptr; }

protected:
    RenderDevice() = default;
};

/// Create a render device for the specified backend.
/// Returns nullptr if the backend is not available on this platform.
RAYFLOW_CLIENT_API std::unique_ptr<RenderDevice> createRenderDevice(Backend backend);

} // namespace rf
