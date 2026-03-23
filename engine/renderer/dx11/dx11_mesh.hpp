#pragma once

#include "engine/renderer/gpu/gpu_mesh.hpp"
#include "engine/renderer/gpu/gpu_types.hpp"

#include <d3d11.h>
#include <wrl/client.h>

namespace rf {

class DX11RenderDevice;

template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

class DX11Mesh : public IMesh {
public:
    explicit DX11Mesh(DX11RenderDevice* device);
    ~DX11Mesh() override;

    void upload(int vertexCount,
                const float* positions,
                const float* texcoords = nullptr,
                const float* texcoords2 = nullptr,
                const float* normals = nullptr,
                const std::uint8_t* colors = nullptr,
                bool dynamic = false) override;

    void update(int vertexCount,
                const float* positions,
                const float* texcoords = nullptr,
                const float* texcoords2 = nullptr,
                const float* normals = nullptr,
                const std::uint8_t* colors = nullptr) override;

    void uploadPositionOnly(const float* positions, int vertexCount) override;

    void destroy() override;
    void draw(PrimitiveType mode = PrimitiveType::Triangles) const override;

    bool isValid() const override;
    int vertexCount() const override { return vertexCount_; }

private:
    bool createBuffer(ComPtr<ID3D11Buffer>& buf, const void* data,
                      UINT byteSize, bool dynamic);
    void updateBuffer(const ComPtr<ID3D11Buffer>& buf, const void* data, UINT byteSize);

    DX11RenderDevice* device_;

    // Separate buffer per attribute slot (matching GL VBO-per-attribute pattern)
    ComPtr<ID3D11Buffer> positionBuffer_;   // slot 0: float3
    ComPtr<ID3D11Buffer> texcoordBuffer_;   // slot 1: float2
    ComPtr<ID3D11Buffer> texcoord2Buffer_;  // slot 2: float2
    ComPtr<ID3D11Buffer> normalBuffer_;     // slot 3: float3
    ComPtr<ID3D11Buffer> colorBuffer_;      // slot 4: ubyte4

    int  vertexCount_ = 0;
    bool dynamic_     = false;
    bool valid_       = false;
};

} // namespace rf
