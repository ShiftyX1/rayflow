#include "input.hpp"

#include <GLFW/glfw3.h>
#include <cstring>

namespace rf {

// ============================================================================
// Singleton
// ============================================================================

Input& Input::instance() {
    static Input inst;
    return inst;
}

// ============================================================================
// Initialization
// ============================================================================

void Input::init(GLFWwindow* window) {
    window_ = window;

    std::memset(keyState_, 0, sizeof(keyState_));
    std::memset(keyPrevState_, 0, sizeof(keyPrevState_));
    std::memset(mouseState_, 0, sizeof(mouseState_));
    std::memset(mousePrevState_, 0, sizeof(mousePrevState_));
    std::memset(charQueue_, 0, sizeof(charQueue_));

    firstMouseMove_ = true;
    scrollDelta_ = 0.0f;
    charQueueHead_ = 0;
    charQueueTail_ = 0;

    // Get initial cursor position
    glfwGetCursorPos(window_, &mouseX_, &mouseY_);
    mousePrevX_ = mouseX_;
    mousePrevY_ = mouseY_;

    // Install callbacks (use window user pointer — Window already set it,
    // so we store Input* as second user pointer via a static instance).
    glfwSetKeyCallback(window_, keyCallback);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetCursorPosCallback(window_, cursorPosCallback);
    glfwSetScrollCallback(window_, scrollCallback);
    glfwSetCharCallback(window_, charCallback);
}

// ============================================================================
// Per-frame
// ============================================================================

void Input::beginFrame() {
    // Copy current → previous
    std::memcpy(keyPrevState_, keyState_, sizeof(keyState_));
    std::memcpy(mousePrevState_, mouseState_, sizeof(mouseState_));

    // Compute mouse delta from accumulated position changes
    mouseDeltaX_ = mouseX_ - mousePrevX_;
    mouseDeltaY_ = mouseY_ - mousePrevY_;
    mousePrevX_ = mouseX_;
    mousePrevY_ = mouseY_;

    // Reset scroll delta (accumulates via callback during pollEvents)
    scrollDelta_ = 0.0f;
}

// ============================================================================
// Keyboard
// ============================================================================

bool Input::isKeyDown(int key) const {
    if (key < 0 || key >= kMaxKeys) return false;
    return keyState_[key];
}

bool Input::isKeyPressed(int key) const {
    if (key < 0 || key >= kMaxKeys) return false;
    return keyState_[key] && !keyPrevState_[key];
}

bool Input::isKeyReleased(int key) const {
    if (key < 0 || key >= kMaxKeys) return false;
    return !keyState_[key] && keyPrevState_[key];
}

// ============================================================================
// Mouse buttons
// ============================================================================

bool Input::isMouseButtonDown(int button) const {
    if (button < 0 || button >= kMaxMouseButtons) return false;
    return mouseState_[button];
}

bool Input::isMouseButtonPressed(int button) const {
    if (button < 0 || button >= kMaxMouseButtons) return false;
    return mouseState_[button] && !mousePrevState_[button];
}

// ============================================================================
// Mouse position / motion
// ============================================================================

rf::Vec2 Input::getMousePosition() const {
    return {static_cast<float>(mouseX_), static_cast<float>(mouseY_)};
}

rf::Vec2 Input::getMouseDelta() const {
    return {static_cast<float>(mouseDeltaX_), static_cast<float>(mouseDeltaY_)};
}

float Input::getScrollDelta() const {
    return scrollDelta_;
}

// ============================================================================
// Text input
// ============================================================================

int Input::getCharPressed() {
    if (charQueueHead_ == charQueueTail_) return 0;
    int ch = static_cast<int>(charQueue_[charQueueTail_]);
    charQueueTail_ = (charQueueTail_ + 1) % kCharQueueSize;
    return ch;
}

// ============================================================================
// Cursor control
// ============================================================================

void Input::setCursorMode(CursorMode mode) {
    cursorMode_ = mode;
    if (!window_) return;

    switch (mode) {
        case CursorMode::Normal:
            glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            break;
        case CursorMode::Hidden:
            glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            break;
        case CursorMode::Disabled:
            glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            break;
    }
}

// ============================================================================
// GLFW Callbacks (static)
// ============================================================================

void Input::keyCallback(GLFWwindow* /*w*/, int key, int /*scancode*/, int action, int /*mods*/) {
    auto& self = instance();
    if (key < 0 || key >= kMaxKeys) return;

    if (action == GLFW_PRESS) {
        self.keyState_[key] = true;
    } else if (action == GLFW_RELEASE) {
        self.keyState_[key] = false;
    }
    // GLFW_REPEAT is intentionally ignored — edge detection is handled
    // by comparing keyState_ vs keyPrevState_.
}

void Input::mouseButtonCallback(GLFWwindow* /*w*/, int button, int action, int /*mods*/) {
    auto& self = instance();
    if (button < 0 || button >= kMaxMouseButtons) return;

    if (action == GLFW_PRESS) {
        self.mouseState_[button] = true;
    } else if (action == GLFW_RELEASE) {
        self.mouseState_[button] = false;
    }
}

void Input::cursorPosCallback(GLFWwindow* /*w*/, double xpos, double ypos) {
    auto& self = instance();

    if (self.firstMouseMove_) {
        self.mousePrevX_ = xpos;
        self.mousePrevY_ = ypos;
        self.firstMouseMove_ = false;
    }

    self.mouseX_ = xpos;
    self.mouseY_ = ypos;
}

void Input::scrollCallback(GLFWwindow* /*w*/, double /*xoffset*/, double yoffset) {
    auto& self = instance();
    self.scrollDelta_ += static_cast<float>(yoffset);
}

void Input::charCallback(GLFWwindow* /*w*/, unsigned int codepoint) {
    auto& self = instance();
    int next = (self.charQueueHead_ + 1) % kCharQueueSize;
    if (next != self.charQueueTail_) {
        self.charQueue_[self.charQueueHead_] = codepoint;
        self.charQueueHead_ = next;
    }
    // else: queue full, drop character
}

} // namespace rf
