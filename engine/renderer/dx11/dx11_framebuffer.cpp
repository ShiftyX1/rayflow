#include "dx11_framebuffer.hpp"
#include "dx11_render_device.hpp"
#include "engine/core/logging.hpp"

namespace rf {

DX11Framebuffer::DX11Framebuffer(DX11RenderDevice* device) : device_(device) {}

DX11Framebuffer::~DX11Framebuffer() { destroy(); }


DXGI_FORMAT DX11Framebuffer::toDXGIFormat(TextureFormat fmt) const {
    switch (fmt) {
        case TextureFormat::RGBA8:    return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RGBA16F:  return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case TextureFormat::RGBA32F:  return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case TextureFormat::RGB8:     return DXGI_FORMAT_R8G8B8A8_UNORM; // DX11 has no RGB8
        case TextureFormat::R8:       return DXGI_FORMAT_R8_UNORM;
        case TextureFormat::RG8:      return DXGI_FORMAT_R8G8_UNORM;
        default:                      return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}

DXGI_FORMAT DX11Framebuffer::toDXGIDepthFormat(DepthFormat fmt) const {
    switch (fmt) {
        case DepthFormat::Depth16:          return DXGI_FORMAT_D16_UNORM;
        case DepthFormat::Depth24:          return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case DepthFormat::Depth32F:         return DXGI_FORMAT_D32_FLOAT;
        case DepthFormat::Depth24Stencil8:  return DXGI_FORMAT_D24_UNORM_S8_UINT;
        default:                            return DXGI_FORMAT_D24_UNORM_S8_UINT;
    }
}

DXGI_FORMAT DX11Framebuffer::toDXGIDepthSRVFormat(DepthFormat fmt) const {
    switch (fmt) {
        case DepthFormat::Depth16:          return DXGI_FORMAT_R16_UNORM;
        case DepthFormat::Depth24:          return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case DepthFormat::Depth32F:         return DXGI_FORMAT_R32_FLOAT;
        case DepthFormat::Depth24Stencil8:  return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        default:                            return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    }
}

static DXGI_FORMAT toTypelessDepthFormat(DepthFormat fmt) {
    switch (fmt) {
        case DepthFormat::Depth16:          return DXGI_FORMAT_R16_TYPELESS;
        case DepthFormat::Depth24:          return DXGI_FORMAT_R24G8_TYPELESS;
        case DepthFormat::Depth32F:         return DXGI_FORMAT_R32_TYPELESS;
        case DepthFormat::Depth24Stencil8:  return DXGI_FORMAT_R24G8_TYPELESS;
        default:                            return DXGI_FORMAT_R24G8_TYPELESS;
    }
}

bool DX11Framebuffer::create(int width, int height, bool withDepth) {
    AttachmentDesc desc;
    desc.format = TextureFormat::RGBA8;
    desc.filter = TextureFilter::Linear;
    return createMRT(width, height, { desc }, withDepth);
}

bool DX11Framebuffer::createMRT(int width, int height,
                                 const std::vector<AttachmentDesc>& attachments,
                                 bool depthAsTexture) {
    destroy();

    auto* d3dDevice = device_->device();
    if (!d3dDevice || width <= 0 || height <= 0) return false;

    width_  = width;
    height_ = height;
    depthOnly_ = false;
    depthAsTexture_ = depthAsTexture;
    withDepth_ = true;
    attachmentDescs_ = attachments;

    // Create color attachments
    for (const auto& att : attachments) {
        ColorAttachment ca;
        DXGI_FORMAT fmt = toDXGIFormat(att.format);

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width     = width;
        texDesc.Height    = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format    = fmt;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage     = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = d3dDevice->CreateTexture2D(&texDesc, nullptr, &ca.texture);
        if (FAILED(hr)) { destroy(); return false; }

        hr = d3dDevice->CreateRenderTargetView(ca.texture.Get(), nullptr, &ca.rtv);
        if (FAILED(hr)) { destroy(); return false; }

        hr = d3dDevice->CreateShaderResourceView(ca.texture.Get(), nullptr, &ca.srv);
        if (FAILED(hr)) { destroy(); return false; }

        colorRTVs_.push_back(std::move(ca));
    }

    // Create depth buffer
    {
        DXGI_FORMAT depthFmt = depthAsTexture
            ? toTypelessDepthFormat(DepthFormat::Depth24)
            : toDXGIDepthFormat(DepthFormat::Depth24);

        D3D11_TEXTURE2D_DESC depthDesc = {};
        depthDesc.Width     = width;
        depthDesc.Height    = height;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format    = depthFmt;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage     = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        if (depthAsTexture) depthDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = d3dDevice->CreateTexture2D(&depthDesc, nullptr, &depthTexture_);
        if (FAILED(hr)) { destroy(); return false; }

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = toDXGIDepthFormat(DepthFormat::Depth24);
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        hr = d3dDevice->CreateDepthStencilView(depthTexture_.Get(), &dsvDesc, &depthDSV_);
        if (FAILED(hr)) { destroy(); return false; }

        if (depthAsTexture) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = toDXGIDepthSRVFormat(DepthFormat::Depth24);
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            hr = d3dDevice->CreateShaderResourceView(depthTexture_.Get(), &srvDesc, &depthSRV_);
            if (FAILED(hr)) { destroy(); return false; }
        }
    }

    return true;
}

bool DX11Framebuffer::createDepthOnly(int width, int height, DepthFormat depthFormat) {
    destroy();

    auto* d3dDevice = device_->device();
    if (!d3dDevice || width <= 0 || height <= 0) return false;

    width_  = width;
    height_ = height;
    depthOnly_ = true;
    depthAsTexture_ = true;
    depthFormat_ = depthFormat;
    withDepth_ = true;

    DXGI_FORMAT typelessFmt = toTypelessDepthFormat(depthFormat);

    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width     = width;
    depthDesc.Height    = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format    = typelessFmt;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage     = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = d3dDevice->CreateTexture2D(&depthDesc, nullptr, &depthTexture_);
    if (FAILED(hr)) return false;

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = toDXGIDepthFormat(depthFormat);
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    hr = d3dDevice->CreateDepthStencilView(depthTexture_.Get(), &dsvDesc, &depthDSV_);
    if (FAILED(hr)) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = toDXGIDepthSRVFormat(depthFormat);
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    hr = d3dDevice->CreateShaderResourceView(depthTexture_.Get(), &srvDesc, &depthSRV_);
    if (FAILED(hr)) return false;

    return true;
}

bool DX11Framebuffer::resize(int width, int height) {
    if (depthOnly_) {
        return createDepthOnly(width, height, depthFormat_);
    }
    return createMRT(width, height, attachmentDescs_, depthAsTexture_);
}

void DX11Framebuffer::bind() const {
    auto* ctx = device_->context();
    if (!ctx) return;

    if (depthOnly_) {
        // Depth-only: no color render targets
        ctx->OMSetRenderTargets(0, nullptr, depthDSV_.Get());
    } else {
        std::vector<ID3D11RenderTargetView*> rtvs;
        rtvs.reserve(colorRTVs_.size());
        for (const auto& ca : colorRTVs_) {
            rtvs.push_back(ca.rtv.Get());
        }
        ctx->OMSetRenderTargets(static_cast<UINT>(rtvs.size()), rtvs.data(), depthDSV_.Get());
    }
}

void DX11Framebuffer::unbind() const {
    // Revert to the default backbuffer render target
    device_->bindDefaultRenderTarget();
}

void DX11Framebuffer::begin() const {
    bind();

    D3D11_VIEWPORT vp = {};
    vp.Width  = static_cast<float>(width_);
    vp.Height = static_cast<float>(height_);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    device_->context()->RSSetViewports(1, &vp);
}

void DX11Framebuffer::end() const {
    unbind();
}

void DX11Framebuffer::bindColorTexture(int index, int unit) const {
    auto* ctx = device_->context();
    if (!ctx || index < 0 || index >= static_cast<int>(colorRTVs_.size())) return;

    ID3D11ShaderResourceView* srv = colorRTVs_[index].srv.Get();
    ctx->PSSetShaderResources(unit, 1, &srv);
}

void DX11Framebuffer::bindDepthTexture(int unit) const {
    auto* ctx = device_->context();
    if (!ctx || !depthSRV_) return;

    ID3D11ShaderResourceView* srv = depthSRV_.Get();
    ctx->PSSetShaderResources(unit, 1, &srv);
}

bool DX11Framebuffer::isValid() const {
    return !colorRTVs_.empty() || depthDSV_;
}

std::uintptr_t DX11Framebuffer::nativeColorHandle(int index) const {
    if (index < 0 || index >= static_cast<int>(colorRTVs_.size())) return 0;
    return reinterpret_cast<std::uintptr_t>(colorRTVs_[index].srv.Get());
}

std::uintptr_t DX11Framebuffer::nativeDepthHandle() const {
    return reinterpret_cast<std::uintptr_t>(depthSRV_.Get());
}

void DX11Framebuffer::destroy() {
    colorRTVs_.clear();
    depthSRV_.Reset();
    depthDSV_.Reset();
    depthTexture_.Reset();
    width_ = height_ = 0;
    depthOnly_ = false;
}

} // namespace rf
