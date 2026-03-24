#pragma once

// =============================================================================
// console_panel.hpp — ImGui-based in-game developer console (Source-style).
//
// Displays streamed engine logs with colored severity, and provides a Lua
// command input field with history navigation.
// =============================================================================

#include "engine/core/export.hpp"

namespace engine::console {
class ConsoleLogSink;
class ConsoleLuaState;
} // namespace engine::console

namespace ui::debug {

/// Draw the developer console window.
/// @param sink   Log ring-buffer displayed in the scrolling area.
/// @param lua    Lua state used to execute typed commands.
/// @param open   Pointer to the visibility flag (toggled by the window close button).
RAYFLOW_CLIENT_API void draw_console(engine::console::ConsoleLogSink& sink,
                                     engine::console::ConsoleLuaState& lua,
                                     bool* open);

} // namespace ui::debug
