#include "dx11_render_device.hpp"
#include "dx11_shader.hpp"
#include "dx11_texture.hpp"
#include "dx11_mesh.hpp"
#include "dx11_framebuffer.hpp"
#include "engine/core/logging.hpp"

#include <d3d11.h>
#include <dxgi.h>

namespace rf {

DX11RenderDevice::~DX11RenderDevice() {
    if (initialized_) shutdown();
}

// ============================================================================
// Resource creation
// ============================================================================

std::unique_ptr<IShader> DX11RenderDevice::createShader() {
    return std::make_unique<DX11Shader>(this);
}

std::unique_ptr<ITexture> DX11RenderDevice::createTexture() {
    return std::make_unique<DX11Texture>(this);
}

std::unique_ptr<IMesh> DX11RenderDevice::createMesh() {
    return std::make_unique<DX11Mesh>(this);
}

std::unique_ptr<IFramebuffer> DX11RenderDevice::createFramebuffer() {
    return std::make_unique<DX11Framebuffer>(this);
}

// ============================================================================
// Global GPU state
// ============================================================================

void DX11RenderDevice::setViewport(int x, int y, int width, int height) {
    viewportX_ = x;
    viewportY_ = y;
    viewportW_ = width;
    viewportH_ = height;

    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = static_cast<float>(x);
    vp.TopLeftY = static_cast<float>(y);
    vp.Width    = static_cast<float>(width);
    vp.Height   = static_cast<float>(height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);
}

void DX11RenderDevice::getViewport(int outViewport[4]) const {
    outViewport[0] = viewportX_;
    outViewport[1] = viewportY_;
    outViewport[2] = viewportW_;
    outViewport[3] = viewportH_;
}

void DX11RenderDevice::clear(bool color, bool depth) {
    if (color && backbufferRTV_) {
        context_->ClearRenderTargetView(backbufferRTV_.Get(), clearColor_);
    }
    if (depth && depthStencilView_) {
        context_->ClearDepthStencilView(depthStencilView_.Get(),
                                         D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                                         1.0f, 0);
    }
}

void DX11RenderDevice::setClearColor(float r, float g, float b, float a) {
    clearColor_[0] = r;
    clearColor_[1] = g;
    clearColor_[2] = b;
    clearColor_[3] = a;
}

void DX11RenderDevice::setDepthTest(bool enabled) {
    if (depthTestEnabled_ != enabled) {
        depthTestEnabled_ = enabled;
        depthStateDirty_ = true;
    }
}

void DX11RenderDevice::setDepthFunc(DepthFunc func) {
    if (depthFunc_ != func) {
        depthFunc_ = func;
        depthStateDirty_ = true;
    }
}

void DX11RenderDevice::setDepthWrite(bool enabled) {
    if (depthWriteEnabled_ != enabled) {
        depthWriteEnabled_ = enabled;
        depthStateDirty_ = true;
    }
}

void DX11RenderDevice::setBlendMode(BlendMode mode) {
    if (blendMode_ != mode) {
        blendMode_ = mode;
        blendStateDirty_ = true;
    }
}

void DX11RenderDevice::setCullMode(CullMode mode) {
    if (cullMode_ != mode) {
        cullMode_ = mode;
        rasterStateDirty_ = true;
    }
}

void DX11RenderDevice::setPolygonOffset(bool enabled, float factor, float units) {
    if (polyOffsetEnabled_ != enabled || polyOffsetFactor_ != factor || polyOffsetUnits_ != units) {
        polyOffsetEnabled_ = enabled;
        polyOffsetFactor_ = factor;
        polyOffsetUnits_ = units;
        rasterStateDirty_ = true;
    }
}

void DX11RenderDevice::present() {
    // Flush any dirty state before presenting
    flushState();

    if (swapChain_) {
        swapChain_->Present(1, 0);  // VSync on
    }
}

void DX11RenderDevice::flushState() {
    if (depthStateDirty_) applyDepthStencilState();
    if (rasterStateDirty_) applyRasterizerState();
    if (blendStateDirty_) applyBlendState();
}

// ============================================================================
// Initialization
// ============================================================================

bool DX11RenderDevice::init(void* nativeWindowHandle, int width, int height) {
    HWND hwnd = static_cast<HWND>(nativeWindowHandle);

    if (!createSwapChain(hwnd, width, height)) {
        TraceLog(LOG_ERROR, "[DX11] Failed to create device and swap chain");
        return false;
    }

    backbufferWidth_ = width;
    backbufferHeight_ = height;

    if (!createBackbufferViews()) {
        TraceLog(LOG_ERROR, "[DX11] Failed to create backbuffer views");
        return false;
    }

    // Set initial state
    setViewport(0, 0, width, height);
    applyDepthStencilState();
    applyRasterizerState();
    applyBlendState();
    bindDefaultRenderTarget();

    initialized_ = true;
    TraceLog(LOG_INFO, "[DX11] Initialized (%dx%d)", width, height);

    return true;
}

void DX11RenderDevice::shutdown() {
    if (context_) context_->ClearState();

    depthStencilView_.Reset();
    depthStencilBuffer_.Reset();
    backbufferRTV_.Reset();
    swapChain_.Reset();
    context_.Reset();
    device_.Reset();

    initialized_ = false;
    TraceLog(LOG_INFO, "[DX11] Shutdown");
}

void DX11RenderDevice::onResize(int width, int height) {
    if (!swapChain_ || width <= 0 || height <= 0) return;

    // Release old views
    context_->OMSetRenderTargets(0, nullptr, nullptr);
    backbufferRTV_.Reset();
    depthStencilView_.Reset();
    depthStencilBuffer_.Reset();

    // Resize swap chain buffers
    HRESULT hr = swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        TraceLog(LOG_ERROR, "[DX11] Failed to resize swap chain (0x%08X)", hr);
        return;
    }

    backbufferWidth_ = width;
    backbufferHeight_ = height;
    createBackbufferViews();
    setViewport(0, 0, width, height);
    bindDefaultRenderTarget();
}

void DX11RenderDevice::bindDefaultRenderTarget() {
    ID3D11RenderTargetView* rtv = backbufferRTV_.Get();
    context_->OMSetRenderTargets(1, &rtv, depthStencilView_.Get());
}

// ============================================================================
// Internal helpers
// ============================================================================

bool DX11RenderDevice::createSwapChain(HWND hwnd, int width, int height) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount       = 1;
    sd.BufferDesc.Width  = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count   = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed     = TRUE;
    sd.SwapEffect   = DXGI_SWAP_EFFECT_DISCARD;

    UINT createFlags = 0;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createFlags,
        featureLevels,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &swapChain_,
        &device_,
        &featureLevel,
        &context_
    );

    if (FAILED(hr)) {
        TraceLog(LOG_ERROR, "[DX11] D3D11CreateDeviceAndSwapChain failed (0x%08X)", hr);
        return false;
    }

    TraceLog(LOG_INFO, "[DX11] Device created (feature level 0x%04X)", featureLevel);
    return true;
}

bool DX11RenderDevice::createBackbufferViews() {
    // Create render target view from back buffer
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) return false;

    hr = device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &backbufferRTV_);
    if (FAILED(hr)) return false;

    // Create depth-stencil buffer and view
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width     = backbufferWidth_;
    depthDesc.Height    = backbufferHeight_;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count   = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage     = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = device_->CreateTexture2D(&depthDesc, nullptr, &depthStencilBuffer_);
    if (FAILED(hr)) return false;

    hr = device_->CreateDepthStencilView(depthStencilBuffer_.Get(), nullptr, &depthStencilView_);
    if (FAILED(hr)) return false;

    return true;
}

void DX11RenderDevice::applyDepthStencilState() {
    D3D11_DEPTH_STENCIL_DESC desc = {};
    desc.DepthEnable    = depthTestEnabled_ ? TRUE : FALSE;
    desc.DepthWriteMask = depthWriteEnabled_ ? D3D11_DEPTH_WRITE_MASK_ALL
                                              : D3D11_DEPTH_WRITE_MASK_ZERO;

    switch (depthFunc_) {
        case DepthFunc::Less:         desc.DepthFunc = D3D11_COMPARISON_LESS;          break;
        case DepthFunc::LessEqual:    desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;    break;
        case DepthFunc::Greater:      desc.DepthFunc = D3D11_COMPARISON_GREATER;       break;
        case DepthFunc::GreaterEqual: desc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL; break;
        case DepthFunc::Equal:        desc.DepthFunc = D3D11_COMPARISON_EQUAL;         break;
        case DepthFunc::Always:       desc.DepthFunc = D3D11_COMPARISON_ALWAYS;        break;
        case DepthFunc::Never:        desc.DepthFunc = D3D11_COMPARISON_NEVER;         break;
    }

    desc.StencilEnable = FALSE;

    ComPtr<ID3D11DepthStencilState> state;
    device_->CreateDepthStencilState(&desc, &state);
    if (state) context_->OMSetDepthStencilState(state.Get(), 0);
    depthStateDirty_ = false;
}

void DX11RenderDevice::applyRasterizerState() {
    D3D11_RASTERIZER_DESC desc = {};
    desc.FillMode = D3D11_FILL_SOLID;
    desc.FrontCounterClockwise = TRUE;  // Match OpenGL winding order

    switch (cullMode_) {
        case CullMode::None:  desc.CullMode = D3D11_CULL_NONE;  break;
        case CullMode::Back:  desc.CullMode = D3D11_CULL_BACK;  break;
        case CullMode::Front: desc.CullMode = D3D11_CULL_FRONT; break;
    }

    desc.DepthClipEnable = TRUE;
    desc.ScissorEnable   = FALSE;
    desc.MultisampleEnable = FALSE;
    desc.AntialiasedLineEnable = FALSE;

    if (polyOffsetEnabled_) {
        desc.DepthBias            = static_cast<INT>(polyOffsetUnits_);
        desc.SlopeScaledDepthBias = polyOffsetFactor_;
        desc.DepthBiasClamp       = 0.0f;
    }

    ComPtr<ID3D11RasterizerState> state;
    device_->CreateRasterizerState(&desc, &state);
    if (state) context_->RSSetState(state.Get());
    rasterStateDirty_ = false;
}

void DX11RenderDevice::applyBlendState() {
    D3D11_BLEND_DESC desc = {};
    desc.AlphaToCoverageEnable  = FALSE;
    desc.IndependentBlendEnable = FALSE;

    auto& rt = desc.RenderTarget[0];
    rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    switch (blendMode_) {
        case BlendMode::None:
            rt.BlendEnable = FALSE;
            break;
        case BlendMode::Alpha:
            rt.BlendEnable    = TRUE;
            rt.SrcBlend       = D3D11_BLEND_SRC_ALPHA;
            rt.DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
            rt.BlendOp        = D3D11_BLEND_OP_ADD;
            rt.SrcBlendAlpha  = D3D11_BLEND_ONE;
            rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            rt.BlendOpAlpha   = D3D11_BLEND_OP_ADD;
            break;
        case BlendMode::Additive:
            rt.BlendEnable    = TRUE;
            rt.SrcBlend       = D3D11_BLEND_SRC_ALPHA;
            rt.DestBlend      = D3D11_BLEND_ONE;
            rt.BlendOp        = D3D11_BLEND_OP_ADD;
            rt.SrcBlendAlpha  = D3D11_BLEND_ONE;
            rt.DestBlendAlpha = D3D11_BLEND_ONE;
            rt.BlendOpAlpha   = D3D11_BLEND_OP_ADD;
            break;
        case BlendMode::Premultiplied:
            rt.BlendEnable    = TRUE;
            rt.SrcBlend       = D3D11_BLEND_ONE;
            rt.DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
            rt.BlendOp        = D3D11_BLEND_OP_ADD;
            rt.SrcBlendAlpha  = D3D11_BLEND_ONE;
            rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            rt.BlendOpAlpha   = D3D11_BLEND_OP_ADD;
            break;
    }

    ComPtr<ID3D11BlendState> state;
    device_->CreateBlendState(&desc, &state);
    if (state) context_->OMSetBlendState(state.Get(), nullptr, 0xFFFFFFFF);
    blendStateDirty_ = false;
}

} // namespace rf
