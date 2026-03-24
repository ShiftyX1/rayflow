#include "dx11_shader.hpp"
#include "dx11_render_device.hpp"
#include "engine/client/core/resources.hpp"
#include "engine/core/logging.hpp"

#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstring>

namespace rf {

DX11Shader::DX11Shader(DX11RenderDevice* device) : device_(device) {}

DX11Shader::~DX11Shader() { destroy(); }


bool DX11Shader::compileShader(const std::string& source, const std::string& entryPoint,
                                const std::string& target, ID3DBlob** blob) {
    ComPtr<ID3DBlob> errorBlob;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompile(source.c_str(), source.size(), nullptr, nullptr, nullptr,
                             entryPoint.c_str(), target.c_str(), flags, 0, blob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            TraceLog(LOG_ERROR, "[DX11Shader] Compile error: %s",
                     static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        return false;
    }
    return true;
}

bool DX11Shader::loadFromSource(const std::string& vertexSrc, const std::string& fragmentSrc) {
    destroy();

    auto* d3dDevice = device_->device();
    if (!d3dDevice) return false;

    // Compile vertex shader
    ComPtr<ID3DBlob> vsBlob;
    if (!compileShader(vertexSrc, "VSMain", "vs_5_0", &vsBlob)) {
        TraceLog(LOG_ERROR, "[DX11Shader] Failed to compile vertex shader");
        return false;
    }

    HRESULT hr = d3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                                nullptr, &vertexShader_);
    if (FAILED(hr)) return false;

    // Compile pixel shader
    ComPtr<ID3DBlob> psBlob;
    if (!compileShader(fragmentSrc, "PSMain", "ps_5_0", &psBlob)) {
        TraceLog(LOG_ERROR, "[DX11Shader] Failed to compile pixel shader");
        return false;
    }

    hr = d3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                       nullptr, &pixelShader_);
    if (FAILED(hr)) return false;

    // Create input layout matching the standard vertex attribute layout:
    // 0: POSITION  float3
    // 1: TEXCOORD0 float2
    // 2: TEXCOORD1 float2
    // 3: NORMAL    float3
    // 4: COLOR     unorm4
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,       1, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  1, DXGI_FORMAT_R32G32_FLOAT,       2, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT,    3, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",     0, DXGI_FORMAT_R8G8B8A8_UNORM,     4, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = d3dDevice->CreateInputLayout(layout, _countof(layout),
                                       vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                       &inputLayout_);
    if (FAILED(hr)) {
        TraceLog(LOG_WARNING, "[DX11Shader] Failed to create full input layout, trying position-only");
        // Fallback: position-only layout (for fullscreen triangle, skybox cube, etc.)
        D3D11_INPUT_ELEMENT_DESC posOnly[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        hr = d3dDevice->CreateInputLayout(posOnly, 1,
                                           vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                           &inputLayout_);
        if (FAILED(hr)) {
            TraceLog(LOG_ERROR, "[DX11Shader] Failed to create any input layout");
            return false;
        }
    }

    // Reflect uniforms from both shader stages
    reflectUniforms(vsBlob.Get(), psBlob.Get());

    valid_ = true;
    TraceLog(LOG_INFO, "[DX11Shader] Compiled successfully (%zu uniforms)", uniforms_.size());
    return true;
}

bool DX11Shader::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    // Remap GLSL paths to HLSL: "shaders/voxel.vs" -> "shaders/hlsl/voxel.vs.hlsl"
    auto toHlsl = [](const std::string& glPath) -> std::string {
        // Find "shaders/" prefix
        size_t pos = glPath.find("shaders/");
        if (pos == std::string::npos) return glPath + ".hlsl";
        return glPath.substr(0, pos) + "shaders/hlsl/" + glPath.substr(pos + 8) + ".hlsl";
    };

    std::string vsPath = toHlsl(vertexPath);
    std::string psPath = toHlsl(fragmentPath);

    std::string vsSrc = resources::load_text(vsPath);
    std::string psSrc = resources::load_text(psPath);

    if (vsSrc.empty()) {
        TraceLog(LOG_ERROR, "[DX11Shader] Failed to read vertex shader: %s", vsPath.c_str());
        return false;
    }
    if (psSrc.empty()) {
        TraceLog(LOG_ERROR, "[DX11Shader] Failed to read pixel shader: %s", psPath.c_str());
        return false;
    }

    return loadFromSource(vsSrc, psSrc);
}

void DX11Shader::reflectUniforms(ID3DBlob* vsBlob, ID3DBlob* psBlob) {
    uniforms_.clear();

    auto reflectStage = [&](ID3DBlob* blob, bool isVertex) {
        ComPtr<ID3D11ShaderReflection> reflection;
        HRESULT hr = D3DReflect(blob->GetBufferPointer(), blob->GetBufferSize(),
                                 __uuidof(ID3D11ShaderReflection), reinterpret_cast<void**>(reflection.GetAddressOf()));
        if (FAILED(hr)) return;

        D3D11_SHADER_DESC shaderDesc;
        reflection->GetDesc(&shaderDesc);

        // Find constant buffer at slot 0
        for (UINT cb = 0; cb < shaderDesc.ConstantBuffers; ++cb) {
            ID3D11ShaderReflectionConstantBuffer* cbReflect = reflection->GetConstantBufferByIndex(cb);
            D3D11_SHADER_BUFFER_DESC cbDesc;
            cbReflect->GetDesc(&cbDesc);

            // Only process cb0 (our uniform buffer)
            if (cb != 0) continue;

            UINT cbSize = cbDesc.Size;
            auto& cbData = isVertex ? vsCBData_ : psCBData_;
            cbData.resize(cbSize, 0);

            for (UINT v = 0; v < cbDesc.Variables; ++v) {
                ID3D11ShaderReflectionVariable* var = cbReflect->GetVariableByIndex(v);
                D3D11_SHADER_VARIABLE_DESC varDesc;
                var->GetDesc(&varDesc);

                UniformInfo info;
                info.offset   = static_cast<int>(varDesc.StartOffset);
                info.size     = static_cast<int>(varDesc.Size);
                info.isVertex = isVertex;
                uniforms_.emplace(varDesc.Name, info);
            }

            // Create the D3D constant buffer
            D3D11_BUFFER_DESC bufDesc = {};
            bufDesc.ByteWidth      = (cbSize + 15) & ~15;  // 16-byte aligned
            bufDesc.Usage          = D3D11_USAGE_DYNAMIC;
            bufDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
            bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            auto& cbBuffer = isVertex ? vsCBuffer_ : psCBuffer_;
            device_->device()->CreateBuffer(&bufDesc, nullptr, &cbBuffer);
        }
    };

    reflectStage(vsBlob, true);
    reflectStage(psBlob, false);
}

void DX11Shader::bind() const {
    auto* ctx = device_->context();
    if (!ctx || !valid_) return;

    flushConstantBuffers();

    ctx->IASetInputLayout(inputLayout_.Get());
    ctx->VSSetShader(vertexShader_.Get(), nullptr, 0);
    ctx->PSSetShader(pixelShader_.Get(), nullptr, 0);

    if (vsCBuffer_) {
        ID3D11Buffer* buf = vsCBuffer_.Get();
        ctx->VSSetConstantBuffers(0, 1, &buf);
    }
    if (psCBuffer_) {
        ID3D11Buffer* buf = psCBuffer_.Get();
        ctx->PSSetConstantBuffers(0, 1, &buf);
    }

    device_->setActiveShader(this);
}

void DX11Shader::unbind() const {
    auto* ctx = device_->context();
    if (!ctx) return;
    ctx->VSSetShader(nullptr, nullptr, 0);
    ctx->PSSetShader(nullptr, nullptr, 0);
    device_->setActiveShader(nullptr);
}

bool DX11Shader::isValid() const { return valid_; }

void DX11Shader::destroy() {
    vertexShader_.Reset();
    pixelShader_.Reset();
    inputLayout_.Reset();
    vsCBuffer_.Reset();
    psCBuffer_.Reset();
    vsCBData_.clear();
    psCBData_.clear();
    uniforms_.clear();
    valid_ = false;
}

void DX11Shader::flushConstantBuffers() const {
    auto* ctx = device_->context();
    if (!ctx) return;

    auto upload = [&](const ComPtr<ID3D11Buffer>& buf, const std::vector<uint8_t>& data, bool& dirty) {
        if (!dirty || !buf || data.empty()) return;
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = ctx->Map(buf.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) {
            std::memcpy(mapped.pData, data.data(), data.size());
            ctx->Unmap(buf.Get(), 0);
        }
        dirty = false;
    };

    upload(vsCBuffer_, vsCBData_, vsCBDirty_);
    upload(psCBuffer_, psCBData_, psCBDirty_);
}

static void writeUniform(const DX11Shader::UniformInfo* info,
                          std::vector<uint8_t>& vsCB, bool& vsDirty,
                          std::vector<uint8_t>& psCB, bool& psDirty,
                          const void* data, size_t dataSize) {
    if (!info) return;
    auto& cb = info->isVertex ? vsCB : psCB;
    auto& dirty = info->isVertex ? vsDirty : psDirty;
    if (info->offset + static_cast<int>(dataSize) <= static_cast<int>(cb.size())) {
        std::memcpy(cb.data() + info->offset, data, dataSize);
        dirty = true;
    }
}

int DX11Shader::getUniformLocation(const std::string& name) const {
    auto it = uniforms_.find(name);
    if (it == uniforms_.end()) return -1;
    return it->second.offset;
}

static void writeUniformAll(const std::unordered_multimap<std::string, DX11Shader::UniformInfo>& uniforms,
                             const std::string& name,
                             std::vector<uint8_t>& vsCB, bool& vsDirty,
                             std::vector<uint8_t>& psCB, bool& psDirty,
                             const void* data, size_t dataSize) {
    auto range = uniforms.equal_range(name);
    for (auto it = range.first; it != range.second; ++it) {
        writeUniform(&it->second, vsCB, vsDirty, psCB, psDirty, data, dataSize);
    }
}

void DX11Shader::setInt(const std::string& name, int value) const {
    writeUniformAll(uniforms_, name, vsCBData_, vsCBDirty_, psCBData_, psCBDirty_, &value, sizeof(value));
}

void DX11Shader::setFloat(const std::string& name, float value) const {
    writeUniformAll(uniforms_, name, vsCBData_, vsCBDirty_, psCBData_, psCBDirty_, &value, sizeof(value));
}

void DX11Shader::setVec2(const std::string& name, const Vec2& v) const {
    writeUniformAll(uniforms_, name, vsCBData_, vsCBDirty_, psCBData_, psCBDirty_,
                    glm::value_ptr(v), sizeof(Vec2));
}

void DX11Shader::setVec3(const std::string& name, const Vec3& v) const {
    writeUniformAll(uniforms_, name, vsCBData_, vsCBDirty_, psCBData_, psCBDirty_,
                    glm::value_ptr(v), sizeof(Vec3));
}

void DX11Shader::setVec4(const std::string& name, const Vec4& v) const {
    writeUniformAll(uniforms_, name, vsCBData_, vsCBDirty_, psCBData_, psCBDirty_,
                    glm::value_ptr(v), sizeof(Vec4));
}

void DX11Shader::setMat4(const std::string& name, const Mat4& m) const {
    writeUniformAll(uniforms_, name, vsCBData_, vsCBDirty_, psCBData_, psCBDirty_,
                    glm::value_ptr(m), sizeof(Mat4));
}

void DX11Shader::setInt(int loc, int value) const {
    // Location is byte offset; try VS first, then PS
    if (loc >= 0 && loc + static_cast<int>(sizeof(int)) <= static_cast<int>(vsCBData_.size())) {
        std::memcpy(vsCBData_.data() + loc, &value, sizeof(value));
        vsCBDirty_ = true;
    } else if (loc >= 0 && loc + static_cast<int>(sizeof(int)) <= static_cast<int>(psCBData_.size())) {
        std::memcpy(psCBData_.data() + loc, &value, sizeof(value));
        psCBDirty_ = true;
    }
}

void DX11Shader::setFloat(int loc, float value) const {
    if (loc >= 0 && loc + static_cast<int>(sizeof(float)) <= static_cast<int>(vsCBData_.size())) {
        std::memcpy(vsCBData_.data() + loc, &value, sizeof(value));
        vsCBDirty_ = true;
    } else if (loc >= 0 && loc + static_cast<int>(sizeof(float)) <= static_cast<int>(psCBData_.size())) {
        std::memcpy(psCBData_.data() + loc, &value, sizeof(value));
        psCBDirty_ = true;
    }
}

void DX11Shader::setVec2(int loc, const Vec2& v) const {
    if (loc >= 0 && loc + static_cast<int>(sizeof(Vec2)) <= static_cast<int>(vsCBData_.size())) {
        std::memcpy(vsCBData_.data() + loc, glm::value_ptr(v), sizeof(Vec2));
        vsCBDirty_ = true;
    }
}

void DX11Shader::setVec3(int loc, const Vec3& v) const {
    if (loc >= 0 && loc + static_cast<int>(sizeof(Vec3)) <= static_cast<int>(vsCBData_.size())) {
        std::memcpy(vsCBData_.data() + loc, glm::value_ptr(v), sizeof(Vec3));
        vsCBDirty_ = true;
    }
}

void DX11Shader::setVec4(int loc, const Vec4& v) const {
    if (loc >= 0 && loc + static_cast<int>(sizeof(Vec4)) <= static_cast<int>(vsCBData_.size())) {
        std::memcpy(vsCBData_.data() + loc, glm::value_ptr(v), sizeof(Vec4));
        vsCBDirty_ = true;
    }
}

void DX11Shader::setMat4(int loc, const Mat4& m) const {
    if (loc >= 0 && loc + static_cast<int>(sizeof(Mat4)) <= static_cast<int>(vsCBData_.size())) {
        std::memcpy(vsCBData_.data() + loc, glm::value_ptr(m), sizeof(Mat4));
        vsCBDirty_ = true;
    }
}

} // namespace rf
