#pragma once

#include "../runtime/ui_view_model.hpp"
#include "engine/core/export.hpp"
#include "engine/renderer/gpu/gpu_types.hpp"

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

/// Initialize Dear ImGui context and platform/renderer backends.
/// Must be called after the window and graphics context are created.
/// For DX11: pass the ID3D11Device* and ID3D11DeviceContext* as void pointers.
RAYFLOW_CLIENT_API void init(GLFWwindow* window, rf::Backend backend = rf::Backend::OpenGL,
                             void* dx11Device = nullptr, void* dx11Context = nullptr);

/// Shut down Dear ImGui (call before destroying the window).
RAYFLOW_CLIENT_API void shutdown();

/// Begin a new ImGui frame. Call once per frame before any draw_* calls.
RAYFLOW_CLIENT_API void new_frame();

/// Render the ImGui draw data. Call after all draw_* calls.
RAYFLOW_CLIENT_API void render_draw_data();

RAYFLOW_CLIENT_API DebugUIResult draw_interactive(const DebugUIState& current, const UIViewModel& vm);
RAYFLOW_CLIENT_API DebugUIResult draw_overlay(const DebugUIState& current, const UIViewModel& vm);

} // namespace ui::debug
