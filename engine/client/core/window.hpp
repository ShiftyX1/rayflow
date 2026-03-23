#pragma once

// =============================================================================
// Window — GLFW window abstraction
//
// Phase 1: Replaces raylib InitWindow/CloseWindow/WindowShouldClose etc.
// Manages GLFW window lifecycle, OpenGL context, delta time and resize events.
// =============================================================================

#include "engine/core/export.hpp"
#include "engine/renderer/gpu/gpu_types.hpp"

#include <string>

struct GLFWwindow;

namespace rf {

class RAYFLOW_CLIENT_API Window {
public:
    /// Initialize GLFW, create window.
    /// For OpenGL: creates GL context + loads GLAD.
    /// For DirectX11: creates window with GLFW_NO_API (no GL context).
    /// @return true on success.
    bool init(int width, int height, const std::string& title,
              bool vsync = true, Backend backend = Backend::OpenGL);

    /// Destroy window and terminate GLFW.
    void shutdown();

    /// Should the window close? (user clicked X or glfwSetWindowShouldClose called)
    bool shouldClose() const;

    /// Request window close.
    void requestClose();

    /// Poll events from GLFW.
    void pollEvents();

    /// Swap front/back buffers.
    void swapBuffers();

    /// Get current framebuffer size in pixels.
    int width() const { return fbWidth_; }
    int height() const { return fbHeight_; }

    /// Returns true if the framebuffer was resized since last frame.
    bool isResized() const { return resized_; }

    /// Clear the resized flag (call at end of frame).
    void clearResizedFlag() { resized_ = false; }

    /// Compute delta time; call once per frame at the top of the loop.
    /// Returns seconds elapsed since the previous call.
    float updateDeltaTime();

    /// Set up default OpenGL state (depth test, blending, etc.).
    void setupDefaultGLState();

    /// Clear the framebuffer (color + depth). Call at the start of each frame.
    void beginFrame();

    /// Update the GL viewport to match the current framebuffer size.
    void updateViewport();

    /// Get underlying GLFW handle (for input callbacks etc.)
    GLFWwindow* handle() const { return window_; }

    /// Get native window handle (HWND on Windows, GLFWwindow* for GL).
    void* nativeWindowHandle() const;

    /// Which backend the window was created for.
    Backend backend() const { return backend_; }

    /// Singleton-style access (only one window per process).
    static Window& instance();

private:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    static void framebufferSizeCallback(GLFWwindow* window, int w, int h);

    GLFWwindow* window_{nullptr};
    Backend backend_{Backend::OpenGL};
    int fbWidth_{0};
    int fbHeight_{0};
    bool resized_{false};
    double lastFrameTime_{0.0};
};

} // namespace rf
