#pragma once

// =============================================================================
// Window — GLFW window abstraction
//
// Phase 1: Replaces raylib InitWindow/CloseWindow/WindowShouldClose etc.
// Manages GLFW window lifecycle, OpenGL context, delta time and resize events.
// =============================================================================

#include <string>

struct GLFWwindow;

namespace rf {

class Window {
public:
    /// Initialize GLFW, create window with OpenGL context, load glad.
    /// @return true on success.
    bool init(int width, int height, const std::string& title, bool vsync = true);

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

    /// Get underlying GLFW handle (for input callbacks etc.)
    GLFWwindow* handle() const { return window_; }

    /// Singleton-style access (only one window per process).
    static Window& instance();

private:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    static void framebufferSizeCallback(GLFWwindow* window, int w, int h);

    GLFWwindow* window_{nullptr};
    int fbWidth_{0};
    int fbHeight_{0};
    bool resized_{false};
    double lastFrameTime_{0.0};
};

} // namespace rf
