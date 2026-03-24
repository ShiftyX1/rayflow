#pragma once


#include "engine/renderer/gpu/gpu_shader.hpp"
#include "engine/renderer/gpu/gpu_types.hpp"

#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace rf {

class DX11RenderDevice;

template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

class DX11Shader : public IShader {
public:
    explicit DX11Shader(DX11RenderDevice* device);
    ~DX11Shader() override;

    bool loadFromSource(const std::string& vertexSrc, const std::string& fragmentSrc) override;
    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) override;
    void destroy() override;

    void bind() const override;
    void unbind() const override;
    bool isValid() const override;

    // Uniform setters (by name)
    void setInt(const std::string& name, int value) const override;
    void setFloat(const std::string& name, float value) const override;
    void setVec2(const std::string& name, const Vec2& v) const override;
    void setVec3(const std::string& name, const Vec3& v) const override;
    void setVec4(const std::string& name, const Vec4& v) const override;
    void setMat4(const std::string& name, const Mat4& m) const override;

    // Uniform setters (by location — uses same cbuffer offset)
    void setInt(int loc, int value) const override;
    void setFloat(int loc, float value) const override;
    void setVec2(int loc, const Vec2& v) const override;
    void setVec3(int loc, const Vec3& v) const override;
    void setVec4(int loc, const Vec4& v) const override;
    void setMat4(int loc, const Mat4& m) const override;

    int getUniformLocation(const std::string& name) const override;

    /// Access the input layout for mesh binding.
    ID3D11InputLayout* inputLayout() const { return inputLayout_.Get(); }

private:
    bool compileShader(const std::string& source, const std::string& entryPoint,
                       const std::string& target, ID3DBlob** blob);
    void reflectUniforms(ID3DBlob* vsBlob, ID3DBlob* psBlob);
    void flushConstantBuffers() const;

    friend class DX11Mesh;
    friend class DX11RenderDevice;

    DX11RenderDevice* device_;

    ComPtr<ID3D11VertexShader>  vertexShader_;
    ComPtr<ID3D11PixelShader>   pixelShader_;
    ComPtr<ID3D11InputLayout>   inputLayout_;

    // Constant buffer for vertex shader uniforms (cb0)
    ComPtr<ID3D11Buffer>        vsCBuffer_;
    // Constant buffer for pixel shader uniforms (cb0)
    ComPtr<ID3D11Buffer>        psCBuffer_;

    mutable std::vector<uint8_t> vsCBData_;
    mutable std::vector<uint8_t> psCBData_;
    mutable bool vsCBDirty_ = false;
    mutable bool psCBDirty_ = false;

public:
    struct UniformInfo {
        int   offset;     // Byte offset into cbuffer data
        int   size;       // Size in bytes
        bool  isVertex;   // true = VS cbuffer, false = PS cbuffer
    };
private:
    std::unordered_multimap<std::string, UniformInfo> uniforms_;

    bool valid_ = false;
};

} // namespace rf
