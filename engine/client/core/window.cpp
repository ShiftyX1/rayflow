#include "window.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>

namespace rf {

// ============================================================================
// Singleton
// ============================================================================

Window& Window::instance() {
    static Window inst;
    return inst;
}

Window::~Window() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool Window::init(int width, int height, const std::string& title, bool vsync) {
    if (window_) {
        std::fprintf(stderr, "[Window] Already initialized\n");
        return false;
    }

    if (!glfwInit()) {
        std::fprintf(stderr, "[Window] Failed to initialize GLFW\n");
        return false;
    }

    // Require OpenGL 4.1 core (max supported on macOS)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window_) {
        std::fprintf(stderr, "[Window] Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);

    // Load OpenGL functions via glad2
    if (!gladLoadGL(glfwGetProcAddress)) {
        std::fprintf(stderr, "[Window] Failed to initialize glad (OpenGL loader)\n");
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        return false;
    }

    // VSync
    glfwSwapInterval(vsync ? 1 : 0);

    // Store the initial framebuffer size (may differ from window size on HiDPI)
    glfwGetFramebufferSize(window_, &fbWidth_, &fbHeight_);

    // Set resize callback
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);

    // Initialize timer
    lastFrameTime_ = glfwGetTime();

    std::fprintf(stderr, "[Window] Created %dx%d  OpenGL %s\n",
                 fbWidth_, fbHeight_, reinterpret_cast<const char*>(glGetString(GL_VERSION)));

    return true;
}

void Window::shutdown() {
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

// ============================================================================
// Per-frame
// ============================================================================

bool Window::shouldClose() const {
    return window_ ? glfwWindowShouldClose(window_) : true;
}

void Window::requestClose() {
    if (window_) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
}

void Window::pollEvents() {
    resized_ = false;   // reset before polling
    glfwPollEvents();
}

void Window::swapBuffers() {
    if (window_) {
        glfwSwapBuffers(window_);
    }
}

float Window::updateDeltaTime() {
    double now = glfwGetTime();
    float dt = static_cast<float>(now - lastFrameTime_);
    lastFrameTime_ = now;
    // Clamp to avoid huge spikes after a breakpoint / resize stall
    if (dt > 0.25f) dt = 0.25f;
    return dt;
}

// ============================================================================
// Callbacks
// ============================================================================

void Window::framebufferSizeCallback(GLFWwindow* window, int w, int h) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->fbWidth_ = w;
        self->fbHeight_ = h;
        self->resized_ = true;
    }
}

} // namespace rf
