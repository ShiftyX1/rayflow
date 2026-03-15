#pragma once

#include "../runtime/ui_view_model.hpp"
#include "engine/core/export.hpp"

struct GLFWwindow;

namespace ui::debug {

struct DebugUIState {
    bool show_player_info{true};
    bool show_net_info{true};

    float camera_sensitivity{0.1f};
};

struct DebugUIResult {
    DebugUIState state;
};

/// Initialize Dear ImGui context and GLFW+OpenGL3 backends.
/// Must be called after the GLFW window and OpenGL context are created.
RAYFLOW_CLIENT_API void init(GLFWwindow* window);

/// Shut down Dear ImGui (call before destroying the GLFW window).
RAYFLOW_CLIENT_API void shutdown();

/// Begin a new ImGui frame. Call once per frame before any draw_* calls.
RAYFLOW_CLIENT_API void new_frame();

/// Render the ImGui draw data via OpenGL. Call after all draw_* calls.
RAYFLOW_CLIENT_API void render_draw_data();

RAYFLOW_CLIENT_API DebugUIResult draw_interactive(const DebugUIState& current, const UIViewModel& vm);
RAYFLOW_CLIENT_API DebugUIResult draw_overlay(const DebugUIState& current, const UIViewModel& vm);

} // namespace ui::debug
