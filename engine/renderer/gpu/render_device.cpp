#include "engine/renderer/gpu/render_device.hpp"
#include "engine/renderer/gl_render_device.hpp"

#ifdef RAYFLOW_HAS_DX11
#include "engine/renderer/dx11/dx11_render_device.hpp"
#endif

namespace rf {

std::unique_ptr<RenderDevice> createRenderDevice(Backend backend) {
    switch (backend) {
        case Backend::OpenGL:
            return std::make_unique<GLRenderDevice>();

#ifdef RAYFLOW_HAS_DX11
        case Backend::DirectX11:
            return std::make_unique<DX11RenderDevice>();
#endif

        default:
            return nullptr;
    }
}

} // namespace rf
