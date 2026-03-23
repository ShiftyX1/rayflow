#pragma once

#include "engine/renderer/gpu/gpu_framebuffer.hpp"
#include "engine/renderer/gpu/gpu_types.hpp"

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

namespace rf {

class DX11RenderDevice;

template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

class DX11Framebuffer : public IFramebuffer {
public:
    explicit DX11Framebuffer(DX11RenderDevice* device);
    ~DX11Framebuffer() override;

    bool create(int width, int height, bool withDepth = true) override;
    bool createMRT(int width, int height,
                   const std::vector<AttachmentDesc>& attachments,
                   bool depthAsTexture = false) override;
    bool createDepthOnly(int width, int height,
                         DepthFormat depthFormat = DepthFormat::Depth24) override;

    bool resize(int width, int height) override;
    void destroy() override;

    void bind() const override;
    void unbind() const override;
    void begin() const override;
    void end() const override;

    void bindColorTexture(int index, int unit) const override;
    void bindDepthTexture(int unit) const override;

    bool isValid() const override;
    int width() const override { return width_; }
    int height() const override { return height_; }
    int colorAttachmentCount() const override { return static_cast<int>(colorRTVs_.size()); }

    std::uintptr_t nativeColorHandle(int index = 0) const override;
    std::uintptr_t nativeDepthHandle() const override;

private:
    DXGI_FORMAT toDXGIFormat(TextureFormat fmt) const;
    DXGI_FORMAT toDXGIDepthFormat(DepthFormat fmt) const;
    DXGI_FORMAT toDXGIDepthSRVFormat(DepthFormat fmt) const;

    DX11RenderDevice* device_;

    struct ColorAttachment {
        ComPtr<ID3D11Texture2D>          texture;
        ComPtr<ID3D11RenderTargetView>   rtv;
        ComPtr<ID3D11ShaderResourceView> srv;
    };
    std::vector<ColorAttachment> colorRTVs_;

    ComPtr<ID3D11Texture2D>          depthTexture_;
    ComPtr<ID3D11DepthStencilView>   depthDSV_;
    ComPtr<ID3D11ShaderResourceView> depthSRV_;

    int width_  = 0;
    int height_ = 0;
    bool depthOnly_ = false;
    bool depthAsTexture_ = false;

    // Store creation params for resize
    std::vector<AttachmentDesc> attachmentDescs_;
    DepthFormat depthFormat_ = DepthFormat::Depth24;
    bool withDepth_ = true;
};

} // namespace rf
