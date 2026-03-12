#pragma once

// =============================================================================
// Input — GLFW input facade
//
// Phase 1: Replaces raylib IsKeyDown/IsKeyPressed/GetMouseDelta etc.
// Manages keyboard and mouse state via GLFW callbacks with edge detection.
// =============================================================================

#include "engine/core/math_types.hpp"

struct GLFWwindow;

namespace rf {

/// Cursor visibility mode.
enum class CursorMode {
    Normal,     ///< Cursor is visible and free.
    Hidden,     ///< Cursor is hidden but not captured.
    Disabled,   ///< Cursor is hidden and captured (FPS-style).
};

class Input {
public:
    /// Install GLFW callbacks on the given window.
    /// Call once after Window::init().
    void init(GLFWwindow* window);

    /// Update per-frame state (copy current → previous, reset deltas).
    /// Must be called ONCE per frame, BEFORE pollEvents().
    void beginFrame();

    // --- Keyboard ---

    /// True while the key is held down (current frame).
    bool isKeyDown(int key) const;

    /// True only on the frame the key was first pressed.
    bool isKeyPressed(int key) const;

    /// True only on the frame the key was released.
    bool isKeyReleased(int key) const;

    // --- Mouse buttons ---

    bool isMouseButtonDown(int button) const;
    bool isMouseButtonPressed(int button) const;

    // --- Mouse position / motion ---

    /// Mouse position in screen coordinates.
    rf::Vec2 getMousePosition() const;

    /// Mouse movement since last frame.
    rf::Vec2 getMouseDelta() const;

    /// Scroll wheel delta (vertical) since last frame.
    float getScrollDelta() const;

    // --- Text input ---

    /// Return the next character typed (UTF-32 codepoint), or 0 if none.
    /// Call in a loop to drain all chars entered this frame.
    int getCharPressed();

    // --- Cursor control ---

    void setCursorMode(CursorMode mode);
    CursorMode getCursorMode() const { return cursorMode_; }
    bool isCursorHidden() const { return cursorMode_ == CursorMode::Disabled; }

    /// Singleton access.
    static Input& instance();

private:
    Input() = default;
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

    // GLFW callbacks
    static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* w, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* w, double xoffset, double yoffset);
    static void charCallback(GLFWwindow* w, unsigned int codepoint);

    static constexpr int kMaxKeys = 512;
    static constexpr int kMaxMouseButtons = 8;
    static constexpr int kCharQueueSize = 32;

    GLFWwindow* window_{nullptr};

    // Keyboard
    bool keyState_[kMaxKeys]{};
    bool keyPrevState_[kMaxKeys]{};

    // Mouse buttons
    bool mouseState_[kMaxMouseButtons]{};
    bool mousePrevState_[kMaxMouseButtons]{};

    // Mouse position
    double mouseX_{0.0};
    double mouseY_{0.0};
    double mousePrevX_{0.0};
    double mousePrevY_{0.0};
    double mouseDeltaX_{0.0};
    double mouseDeltaY_{0.0};
    bool firstMouseMove_{true};

    // Scroll
    float scrollDelta_{0.0f};

    // Char input queue (ring buffer)
    unsigned int charQueue_[kCharQueueSize]{};
    int charQueueHead_{0};
    int charQueueTail_{0};

    CursorMode cursorMode_{CursorMode::Normal};
};

} // namespace rf
