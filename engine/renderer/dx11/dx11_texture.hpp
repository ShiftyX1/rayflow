#pragma once


#include "engine/renderer/gpu/gpu_texture.hpp"
#include "engine/renderer/gpu/gpu_types.hpp"

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

namespace rf {

class DX11RenderDevice;

template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

class DX11Texture : public ITexture {
public:
    explicit DX11Texture(DX11RenderDevice* device);
    ~DX11Texture() override;

    bool loadFromFile(const std::string& path) override;
    bool loadFromMemory(const std::uint8_t* data, int width, int height, int channels = 4) override;
    bool loadCubemap(const std::string faces[6]) override;
    bool loadCubemapFromPanorama(const std::string& panoramaPath, int faceSize = 512) override;
    void destroy() override;

    void bind(int unit = 0) const override;
    void unbind(int unit = 0) const override;
    void setFilter(TextureFilter filter) override;

    bool isValid() const override;
    int width() const override { return width_; }
    int height() const override { return height_; }
    bool isCubemap() const override { return isCubemap_; }

    Color samplePixel(int x, int y) const override;
    void retainPixelData(bool retain) override;
    std::uintptr_t nativeHandle() const override;

private:
    bool createSamplerState();
    bool uploadTexture2D(const uint8_t* rgba, int w, int h, bool generateMips);

    DX11RenderDevice* device_;

    ComPtr<ID3D11Texture2D>          texture_;
    ComPtr<ID3D11ShaderResourceView> srv_;
    ComPtr<ID3D11SamplerState>       sampler_;

    int width_  = 0;
    int height_ = 0;
    int channels_ = 0;
    bool isCubemap_ = false;
    TextureFilter filter_ = TextureFilter::NearestMipmap;

    bool retainPixels_ = false;
    std::vector<uint8_t> pixelData_;
};

} // namespace rf
