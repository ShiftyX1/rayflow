#include "dx11_mesh.hpp"
#include "dx11_render_device.hpp"
#include "dx11_shader.hpp"
#include "engine/core/logging.hpp"

namespace rf {

DX11Mesh::DX11Mesh(DX11RenderDevice* device) : device_(device) {}

DX11Mesh::~DX11Mesh() { destroy(); }

bool DX11Mesh::createBuffer(ComPtr<ID3D11Buffer>& buf, const void* data,
                             UINT byteSize, bool dynamic) {
    auto* d3dDevice = device_->device();
    if (!d3dDevice || byteSize == 0) return false;

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = byteSize;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    if (dynamic) {
        desc.Usage          = D3D11_USAGE_DYNAMIC;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    } else {
        desc.Usage = D3D11_USAGE_DEFAULT;
    }

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = data;

    buf.Reset();
    return SUCCEEDED(d3dDevice->CreateBuffer(&desc, data ? &initData : nullptr, &buf));
}

void DX11Mesh::updateBuffer(const ComPtr<ID3D11Buffer>& buf, const void* data, UINT byteSize) {
    auto* ctx = device_->context();
    if (!ctx || !buf || !data || byteSize == 0) return;

    if (dynamic_) {
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = ctx->Map(buf.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) {
            std::memcpy(mapped.pData, data, byteSize);
            ctx->Unmap(buf.Get(), 0);
        }
    } else {
        ctx->UpdateSubresource(buf.Get(), 0, nullptr, data, 0, 0);
    }
}

void DX11Mesh::upload(int vertexCount,
                       const float* positions,
                       const float* texcoords,
                       const float* texcoords2,
                       const float* normals,
                       const std::uint8_t* colors,
                       bool dynamic) {
    destroy();
    if (vertexCount <= 0 || !positions) return;

    vertexCount_ = vertexCount;
    dynamic_ = dynamic;

    // Slot 0: positions (float3)
    createBuffer(positionBuffer_, positions,
                 static_cast<UINT>(vertexCount * 3 * sizeof(float)), dynamic);

    // Slot 1: texcoords (float2)
    if (texcoords) {
        createBuffer(texcoordBuffer_, texcoords,
                     static_cast<UINT>(vertexCount * 2 * sizeof(float)), dynamic);
    }

    // Slot 2: texcoords2 (float2)
    if (texcoords2) {
        createBuffer(texcoord2Buffer_, texcoords2,
                     static_cast<UINT>(vertexCount * 2 * sizeof(float)), dynamic);
    }

    // Slot 3: normals (float3)
    if (normals) {
        createBuffer(normalBuffer_, normals,
                     static_cast<UINT>(vertexCount * 3 * sizeof(float)), dynamic);
    }

    // Slot 4: colors (ubyte4)
    if (colors) {
        createBuffer(colorBuffer_, colors,
                     static_cast<UINT>(vertexCount * 4 * sizeof(std::uint8_t)), dynamic);
    }

    valid_ = positionBuffer_ != nullptr;
}

void DX11Mesh::update(int vertexCount,
                       const float* positions,
                       const float* texcoords,
                       const float* texcoords2,
                       const float* normals,
                       const std::uint8_t* colors) {
    if (!valid_ || vertexCount <= 0) return;
    vertexCount_ = vertexCount;

    if (positions && positionBuffer_)
        updateBuffer(positionBuffer_, positions, vertexCount * 3 * sizeof(float));
    if (texcoords && texcoordBuffer_)
        updateBuffer(texcoordBuffer_, texcoords, vertexCount * 2 * sizeof(float));
    if (texcoords2 && texcoord2Buffer_)
        updateBuffer(texcoord2Buffer_, texcoords2, vertexCount * 2 * sizeof(float));
    if (normals && normalBuffer_)
        updateBuffer(normalBuffer_, normals, vertexCount * 3 * sizeof(float));
    if (colors && colorBuffer_)
        updateBuffer(colorBuffer_, colors, vertexCount * 4 * sizeof(std::uint8_t));
}

void DX11Mesh::uploadPositionOnly(const float* positions, int vertexCount) {
    upload(vertexCount, positions, nullptr, nullptr, nullptr, nullptr, false);
}

void DX11Mesh::draw(PrimitiveType mode) const {
    auto* ctx = device_->context();
    if (!ctx || !valid_) return;

    // Flush any dirty constant buffers so uniforms set after bind() reach the GPU
    device_->flushActiveShaderConstants();

    // Set primitive topology
    D3D11_PRIMITIVE_TOPOLOGY topology;
    switch (mode) {
        case PrimitiveType::Triangles: topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;  break;
        case PrimitiveType::Lines:     topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;      break;
        case PrimitiveType::LineStrip: topology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;     break;
        case PrimitiveType::Points:    topology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;     break;
        default:                       topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;  break;
    }
    ctx->IASetPrimitiveTopology(topology);

    ID3D11Buffer* buffers[5] = { nullptr };
    UINT strides[5] = { 0 };
    UINT offsets[5] = { 0, 0, 0, 0, 0 };

    buffers[0] = positionBuffer_.Get();   strides[0] = 3 * sizeof(float);
    buffers[1] = texcoordBuffer_.Get();   strides[1] = 2 * sizeof(float);
    buffers[2] = texcoord2Buffer_.Get();  strides[2] = 2 * sizeof(float);
    buffers[3] = normalBuffer_.Get();     strides[3] = 3 * sizeof(float);
    buffers[4] = colorBuffer_.Get();      strides[4] = 4 * sizeof(std::uint8_t);

    ctx->IASetVertexBuffers(0, 5, buffers, strides, offsets);

    ctx->Draw(vertexCount_, 0);
}

bool DX11Mesh::isValid() const { return valid_; }

void DX11Mesh::destroy() {
    positionBuffer_.Reset();
    texcoordBuffer_.Reset();
    texcoord2Buffer_.Reset();
    normalBuffer_.Reset();
    colorBuffer_.Reset();
    vertexCount_ = 0;
    valid_ = false;
}

} // namespace rf
