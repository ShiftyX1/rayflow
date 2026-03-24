#include "dx11_texture.hpp"
#include "dx11_render_device.hpp"
#include "engine/core/logging.hpp"
#include "engine/client/core/resources.hpp"

#include <stb_image.h>
#include <cstring>
#include <cmath>

namespace rf {

DX11Texture::DX11Texture(DX11RenderDevice* device) : device_(device) {}

DX11Texture::~DX11Texture() { destroy(); }

bool DX11Texture::loadFromFile(const std::string& path) {
    int w, h, ch;
    stbi_set_flip_vertically_on_load(0);  // DX11 expects top-down
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data) {
        TraceLog(LOG_ERROR, "[DX11Texture] Failed to load: %s", path.c_str());
        return false;
    }

    bool ok = uploadTexture2D(data, w, h, true);

    if (retainPixels_ && ok) {
        pixelData_.assign(data, data + w * h * 4);
    }

    stbi_image_free(data);
    channels_ = ch;
    return ok;
}

bool DX11Texture::loadFromMemory(const std::uint8_t* data, int width, int height, int channels) {
    // Convert to RGBA if needed
    std::vector<uint8_t> rgba;
    const uint8_t* srcData = data;

    if (channels == 3) {
        rgba.resize(width * height * 4);
        for (int i = 0; i < width * height; ++i) {
            rgba[i * 4 + 0] = data[i * 3 + 0];
            rgba[i * 4 + 1] = data[i * 3 + 1];
            rgba[i * 4 + 2] = data[i * 3 + 2];
            rgba[i * 4 + 3] = 255;
        }
        srcData = rgba.data();
    } else if (channels == 1) {
        rgba.resize(width * height * 4);
        for (int i = 0; i < width * height; ++i) {
            rgba[i * 4 + 0] = data[i];
            rgba[i * 4 + 1] = data[i];
            rgba[i * 4 + 2] = data[i];
            rgba[i * 4 + 3] = 255;
        }
        srcData = rgba.data();
    }

    channels_ = channels;
    bool ok = uploadTexture2D(srcData, width, height, false);

    if (retainPixels_ && ok) {
        pixelData_.assign(srcData, srcData + width * height * 4);
    }

    return ok;
}

bool DX11Texture::uploadTexture2D(const uint8_t* rgba, int w, int h, bool generateMips) {
    destroy();

    auto* d3dDevice = device_->device();
    if (!d3dDevice) return false;

    width_ = w;
    height_ = h;
    isCubemap_ = false;

    UINT mipLevels = generateMips ? static_cast<UINT>(std::floor(std::log2(std::max(w, h)))) + 1 : 1;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width     = w;
    desc.Height    = h;
    desc.MipLevels = generateMips ? 0 : 1;  // 0 = auto-generate all mips
    desc.ArraySize = 1;
    desc.Format    = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage     = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (generateMips ? D3D11_BIND_RENDER_TARGET : 0);
    desc.MiscFlags = generateMips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;

    HRESULT hr;
    if (generateMips) {
        // Create empty texture, then update subresource and generate mips
        hr = d3dDevice->CreateTexture2D(&desc, nullptr, &texture_);
        if (FAILED(hr)) return false;

        device_->context()->UpdateSubresource(texture_.Get(), 0, nullptr,
                                               rgba, w * 4, 0);
    } else {
        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem     = rgba;
        initData.SysMemPitch = w * 4;
        hr = d3dDevice->CreateTexture2D(&desc, &initData, &texture_);
        if (FAILED(hr)) return false;
    }

    // Create shader resource view
    hr = d3dDevice->CreateShaderResourceView(texture_.Get(), nullptr, &srv_);
    if (FAILED(hr)) return false;

    if (generateMips) {
        device_->context()->GenerateMips(srv_.Get());
    }

    createSamplerState();
    return true;
}

bool DX11Texture::loadCubemap(const std::string faces[6]) {
    destroy();

    auto* d3dDevice = device_->device();
    if (!d3dDevice) return false;

    // Load all 6 faces
    struct FaceData {
        unsigned char* pixels = nullptr;
        int w = 0, h = 0;
    };
    FaceData faceData[6];

    for (int i = 0; i < 6; ++i) {
        int ch;
        stbi_set_flip_vertically_on_load(0);
        faceData[i].pixels = stbi_load(faces[i].c_str(), &faceData[i].w, &faceData[i].h, &ch, 4);
        if (!faceData[i].pixels) {
            TraceLog(LOG_ERROR, "[DX11Texture] Failed to load cubemap face %d: %s", i, faces[i].c_str());
            for (int j = 0; j <= i; ++j) stbi_image_free(faceData[j].pixels);
            return false;
        }
    }

    int size = faceData[0].w;
    width_ = height_ = size;
    isCubemap_ = true;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width     = size;
    desc.Height    = size;
    desc.MipLevels = 1;
    desc.ArraySize = 6;
    desc.Format    = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage     = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    D3D11_SUBRESOURCE_DATA initData[6] = {};
    for (int i = 0; i < 6; ++i) {
        initData[i].pSysMem     = faceData[i].pixels;
        initData[i].SysMemPitch = size * 4;
    }

    HRESULT hr = d3dDevice->CreateTexture2D(&desc, initData, &texture_);
    for (int i = 0; i < 6; ++i) stbi_image_free(faceData[i].pixels);
    if (FAILED(hr)) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MipLevels = 1;
    srvDesc.TextureCube.MostDetailedMip = 0;

    hr = d3dDevice->CreateShaderResourceView(texture_.Get(), &srvDesc, &srv_);
    if (FAILED(hr)) return false;

    createSamplerState();
    return true;
}

bool DX11Texture::loadCubemapFromPanorama(const std::string& panoramaPath, int faceSize) {
    // For now, load as 6 identical faces from the same image
    // A proper equirect→cubemap conversion would require a compute/render pass
    int w, h, ch;
    stbi_set_flip_vertically_on_load(0);
    unsigned char* panoData = stbi_load(panoramaPath.c_str(), &w, &h, &ch, 4);
    if (!panoData) {
        TraceLog(LOG_ERROR, "[DX11Texture] Failed to load panorama: %s", panoramaPath.c_str());
        return false;
    }

    // Simple equirectangular → cubemap conversion on CPU
    width_ = height_ = faceSize;
    isCubemap_ = true;

    auto* d3dDevice = device_->device();
    if (!d3dDevice) { stbi_image_free(panoData); return false; }

    // Generate cubemap faces from equirectangular panorama
    std::vector<uint8_t> faces[6];
    for (int f = 0; f < 6; ++f) {
        faces[f].resize(faceSize * faceSize * 4);
    }

    auto samplePano = [&](float x, float y, float z) -> Color {
        float len = std::sqrt(x*x + y*y + z*z);
        x /= len; y /= len; z /= len;
        float u = 0.5f + std::atan2(z, x) / (2.0f * 3.14159265f);
        float v = 0.5f - std::asin(y) / 3.14159265f;
        int px = static_cast<int>(u * w) % w;
        int py = static_cast<int>(v * h) % h;
        if (px < 0) px += w;
        if (py < 0) py += h;
        int idx = (py * w + px) * 4;
        return { panoData[idx], panoData[idx+1], panoData[idx+2], panoData[idx+3] };
    };

    for (int f = 0; f < 6; ++f) {
        for (int y = 0; y < faceSize; ++y) {
            for (int x = 0; x < faceSize; ++x) {
                float fx = (2.0f * (x + 0.5f) / faceSize - 1.0f);
                float fy = (2.0f * (y + 0.5f) / faceSize - 1.0f);
                float dx = 0, dy = 0, dz = 0;
                switch (f) {
                    case 0: dx =  1; dy = -fy; dz = -fx; break; // +X
                    case 1: dx = -1; dy = -fy; dz =  fx; break; // -X
                    case 2: dx =  fx; dy =  1; dz =  fy; break; // +Y
                    case 3: dx =  fx; dy = -1; dz = -fy; break; // -Y
                    case 4: dx =  fx; dy = -fy; dz =  1; break; // +Z
                    case 5: dx = -fx; dy = -fy; dz = -1; break; // -Z
                }
                Color c = samplePano(dx, dy, dz);
                int idx = (y * faceSize + x) * 4;
                faces[f][idx+0] = c.r;
                faces[f][idx+1] = c.g;
                faces[f][idx+2] = c.b;
                faces[f][idx+3] = c.a;
            }
        }
    }

    stbi_image_free(panoData);

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width     = faceSize;
    desc.Height    = faceSize;
    desc.MipLevels = 1;
    desc.ArraySize = 6;
    desc.Format    = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage     = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    D3D11_SUBRESOURCE_DATA initData[6] = {};
    for (int i = 0; i < 6; ++i) {
        initData[i].pSysMem     = faces[i].data();
        initData[i].SysMemPitch = faceSize * 4;
    }

    HRESULT hr = d3dDevice->CreateTexture2D(&desc, initData, &texture_);
    if (FAILED(hr)) {
        TraceLog(LOG_ERROR, "[DX11Texture] CreateTexture2D failed for cubemap (hr=0x%08X)", hr);
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MipLevels = 1;
    srvDesc.TextureCube.MostDetailedMip = 0;

    hr = d3dDevice->CreateShaderResourceView(texture_.Get(), &srvDesc, &srv_);
    if (FAILED(hr)) {
        TraceLog(LOG_ERROR, "[DX11Texture] CreateSRV failed for cubemap (hr=0x%08X)", hr);
        return false;
    }

    createSamplerState();
    return true;
}

void DX11Texture::bind(int unit) const {
    if (!srv_ || !device_->context()) return;
    ID3D11ShaderResourceView* views[] = { srv_.Get() };
    device_->context()->PSSetShaderResources(unit, 1, views);

    if (sampler_) {
        ID3D11SamplerState* samplers[] = { sampler_.Get() };
        device_->context()->PSSetSamplers(unit, 1, samplers);
    }
}

void DX11Texture::unbind(int unit) const {
    if (!device_->context()) return;
    ID3D11ShaderResourceView* nullSRV[] = { nullptr };
    device_->context()->PSSetShaderResources(unit, 1, nullSRV);
}

void DX11Texture::setFilter(TextureFilter filter) {
    filter_ = filter;
    createSamplerState();
}

bool DX11Texture::createSamplerState() {
    auto* d3dDevice = device_->device();
    if (!d3dDevice) return false;

    D3D11_SAMPLER_DESC desc = {};
    desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    desc.MinLOD = 0;
    desc.MaxLOD = D3D11_FLOAT32_MAX;

    switch (filter_) {
        case TextureFilter::Nearest:
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            break;
        case TextureFilter::Linear:
            desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            break;
        case TextureFilter::NearestMipmap:
            desc.Filter = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
            break;
        case TextureFilter::LinearMipmap:
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            break;
    }

    sampler_.Reset();
    return SUCCEEDED(d3dDevice->CreateSamplerState(&desc, &sampler_));
}

bool DX11Texture::isValid() const { return srv_ != nullptr; }

Color DX11Texture::samplePixel(int x, int y) const {
    if (pixelData_.empty() || x < 0 || y < 0 || x >= width_ || y >= height_)
        return { 0, 0, 0, 0 };
    int idx = (y * width_ + x) * 4;
    return { pixelData_[idx], pixelData_[idx+1], pixelData_[idx+2], pixelData_[idx+3] };
}

void DX11Texture::retainPixelData(bool retain) {
    retainPixels_ = retain;
    if (!retain) pixelData_.clear();
}

std::uintptr_t DX11Texture::nativeHandle() const {
    return reinterpret_cast<std::uintptr_t>(srv_.Get());
}

void DX11Texture::destroy() {
    sampler_.Reset();
    srv_.Reset();
    texture_.Reset();
    pixelData_.clear();
    width_ = height_ = 0;
    channels_ = 0;
    isCubemap_ = false;
}

} // namespace rf
