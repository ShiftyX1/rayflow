#pragma once

// =============================================================================
// IFramebuffer — Abstract framebuffer (render target) interface
//
// Backend-agnostic. Implemented by GLFramebuffer, DX11Framebuffer, etc.
// =============================================================================

#include "engine/core/export.hpp"
#include "engine/renderer/gpu/gpu_types.hpp"

#include <vector>

namespace rf {

class RAYFLOW_CLIENT_API IFramebuffer {
public:
    virtual ~IFramebuffer() = default;

    // Non-copyable
    IFramebuffer(const IFramebuffer&) = delete;
    IFramebuffer& operator=(const IFramebuffer&) = delete;

    // ----- Creation -----

    /// Create a simple FBO with one color attachment and optional depth.
    virtual bool create(int width, int height, bool withDepth = true) = 0;

    /// Create an FBO with multiple color attachments (MRT).
    /// @param depthAsTexture  If true, depth is readable in shaders.
    virtual bool createMRT(int width, int height,
                           const std::vector<AttachmentDesc>& attachments,
                           bool depthAsTexture = false) = 0;

    /// Create a depth-only FBO (shadow maps).
    virtual bool createDepthOnly(int width, int height,
                                 DepthFormat depthFormat = DepthFormat::Depth24) = 0;

    /// Resize (destroys and recreates attachments).
    virtual bool resize(int width, int height) = 0;

    /// Release GPU resources.
    virtual void destroy() = 0;

    // ----- Binding -----

    /// Bind as the active render target.
    virtual void bind() const = 0;

    /// Unbind — revert to default framebuffer.
    virtual void unbind() const = 0;

    /// Begin rendering: bind + set viewport.
    virtual void begin() const = 0;

    /// End rendering: unbind.
    virtual void end() const = 0;

    // ----- Texture binding -----

    /// Bind color attachment as a shader input texture.
    virtual void bindColorTexture(int index, int unit) const = 0;

    /// Bind depth texture (only if created with depthAsTexture).
    virtual void bindDepthTexture(int unit) const = 0;

    // ----- Accessors -----

    virtual bool isValid() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual int colorAttachmentCount() const = 0;

    /// Get backend-specific native handle for a color attachment.
    virtual std::uintptr_t nativeColorHandle(int index = 0) const = 0;

    /// Get backend-specific native handle for the depth attachment.
    virtual std::uintptr_t nativeDepthHandle() const = 0;

protected:
    IFramebuffer() = default;
    IFramebuffer(IFramebuffer&&) noexcept = default;
    IFramebuffer& operator=(IFramebuffer&&) noexcept = default;
};

} // namespace rf
