#pragma once

// =============================================================================
// DX11RenderDevice — DirectX 11 implementation of RenderDevice
// =============================================================================

#include "engine/renderer/gpu/render_device.hpp"
#include "engine/renderer/gpu/gpu_types.hpp"

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace rf {

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

class DX11Shader;

class DX11RenderDevice : public RenderDevice {
public:
    DX11RenderDevice() = default;
    ~DX11RenderDevice() override;

    Backend backend() const override { return Backend::DirectX11; }

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

    // ----- DX11-specific accessors (used by DX11 resource classes) -----
    ID3D11Device*        device()  const { return device_.Get(); }
    ID3D11DeviceContext* context() const { return context_.Get(); }

    void* nativeDevice()  const override { return device_.Get(); }
    void* nativeContext() const override { return context_.Get(); }

    /// Bind the default (backbuffer) render target.
    void bindDefaultRenderTarget();

    /// No-op — state changes are now applied immediately.
    /// Kept for API compatibility (called by Batch2D).
    void flushState();

    /// Flush constant buffers of the currently active shader (if any are dirty).
    void flushActiveShaderConstants() const;

    /// Set/clear the currently bound shader (called by DX11Shader::bind/unbind).
    void setActiveShader(const DX11Shader* shader) { activeShader_ = shader; }

private:
    bool createSwapChain(HWND hwnd, int width, int height);
    bool createBackbufferViews();
    void applyDepthStencilState();
    void applyRasterizerState();
    void applyBlendState();

    ComPtr<ID3D11Device>           device_;
    ComPtr<ID3D11DeviceContext>    context_;
    ComPtr<IDXGISwapChain>         swapChain_;
    ComPtr<ID3D11RenderTargetView> backbufferRTV_;
    ComPtr<ID3D11DepthStencilView> depthStencilView_;
    ComPtr<ID3D11Texture2D>        depthStencilBuffer_;

    // Current state (used to build D3D11 state objects)
    float clearColor_[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    int   viewportX_ = 0, viewportY_ = 0, viewportW_ = 0, viewportH_ = 0;

    bool      depthTestEnabled_  = true;
    bool      depthWriteEnabled_ = true;
    DepthFunc depthFunc_         = DepthFunc::Less;

    CullMode  cullMode_          = CullMode::Back;
    bool      polyOffsetEnabled_ = false;
    float     polyOffsetFactor_  = 0.0f;
    float     polyOffsetUnits_   = 0.0f;

    BlendMode blendMode_         = BlendMode::None;

    int backbufferWidth_  = 0;
    int backbufferHeight_ = 0;
    bool initialized_ = false;

    const DX11Shader* activeShader_ = nullptr;
};

} // namespace rf
