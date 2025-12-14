#include "debug_ui.hpp"

#include <raylib.h>

// raygui header is vendored in ui/
#include "../raygui.h"

namespace ui::debug {

void init() {
    // Keep default style for now.
}

static void draw_player_info(const UIViewModel& vm) {
    const auto& p = vm.player;

    DrawText("Rayflow Debug", 10, 10, 20, BLACK);
    DrawText(TextFormat("Position: (%.2f, %.2f, %.2f)", p.position.x, p.position.y, p.position.z), 10, 40, 16, DARKGRAY);
    DrawText(TextFormat("Velocity: (%.2f, %.2f, %.2f)", p.velocity.x, p.velocity.y, p.velocity.z), 10, 60, 16, DARKGRAY);
    DrawText(TextFormat("On Ground: %s | Sprint: %s | Creative: %s",
                        p.on_ground ? "YES" : "NO",
                        p.sprinting ? "YES" : "NO",
                        p.creative ? "YES" : "NO"),
             10, 80, 16, DARKGRAY);

    DrawText(TextFormat("Yaw/Pitch: (%.1f, %.1f)", p.yaw, p.pitch), 10, 100, 16, DARKGRAY);
    DrawFPS(10, 130);
}

static void draw_net_info(const UIViewModel& vm) {
    const auto& n = vm.net;

    int y = 160;
    DrawText("Net", 10, y, 18, BLACK);
    y += 22;

    DrawText(TextFormat("ServerHello: %s", n.has_server_hello ? "YES" : "NO"), 10, y, 16, DARKGRAY);
    y += 18;

    if (n.has_server_hello) {
        DrawText(TextFormat("tickRate: %u", n.tick_rate), 10, y, 16, DARKGRAY);
        y += 18;
        DrawText(TextFormat("worldSeed: %u", n.world_seed), 10, y, 16, DARKGRAY);
        y += 18;
    }

    DrawText(TextFormat("JoinAck: %s", n.has_join_ack ? "YES" : "NO"), 10, y, 16, DARKGRAY);
    y += 18;

    if (n.has_join_ack) {
        DrawText(TextFormat("playerId: %u", n.player_id), 10, y, 16, DARKGRAY);
        y += 18;
    }

    DrawText(TextFormat("Snapshot: %s", n.has_snapshot ? "YES" : "NO"), 10, y, 16, DARKGRAY);
    y += 18;

    if (n.has_snapshot) {
        DrawText(TextFormat("serverTick: %llu", static_cast<unsigned long long>(n.server_tick)), 10, y, 16, DARKGRAY);
        y += 18;
    }
}

DebugUIResult draw_interactive(const DebugUIState& current, const UIViewModel& vm) {
    DebugUIResult out;
    out.state = current;

    const int w = 360;
    const int h = 240;
    Rectangle panel = { static_cast<float>(vm.screen_width - w - 10), 10.0f, static_cast<float>(w), static_cast<float>(h) };

    GuiPanel(panel, "Debug UI (F1)");

    const float padding = 10.0f;
    const float row_h = 20.0f;
    const float gap_y = 6.0f;
    const float check_size = 20.0f;
    const float label_pad = 8.0f;

    Rectangle row = { panel.x + padding, panel.y + 30, panel.width - padding * 2.0f, row_h };

    // Checkbox: Show player info
    {
        Rectangle cb = { row.x, row.y, check_size, check_size };
        Rectangle label = { row.x + check_size + label_pad, row.y + 2.0f, row.width - check_size - label_pad, row_h };

        bool checked = out.state.show_player_info;
        GuiCheckBox(cb, "", &checked);
        GuiLabel(label, "Show player info");
        out.state.show_player_info = checked;
    }

    row.y += row_h + gap_y;

    // Checkbox: Show net info
    {
        Rectangle cb = { row.x, row.y, check_size, check_size };
        Rectangle label = { row.x + check_size + label_pad, row.y + 2.0f, row.width - check_size - label_pad, row_h };

        bool checked = out.state.show_net_info;
        GuiCheckBox(cb, "", &checked);
        GuiLabel(label, "Show net info");
        out.state.show_net_info = checked;
    }

    row.y += row_h + gap_y + 4.0f;

    // Checkbox: Lighting raymarch shadows
    {
        Rectangle cb = { row.x, row.y, check_size, check_size };
        Rectangle label = { row.x + check_size + label_pad, row.y + 2.0f, row.width - check_size - label_pad, row_h };

        bool checked = out.state.lighting_raymarch_shadows;
        GuiCheckBox(cb, "", &checked);
        GuiLabel(label, "Lighting: Raymarch Shadows");
        out.state.lighting_raymarch_shadows = checked;
    }

    row.y += row_h + gap_y + 4.0f;

    // Slider: camera sensitivity
    {
        Rectangle label = { row.x, row.y, row.width, 18.0f };
        GuiLabel(label, "Camera sensitivity");
        row.y += 18.0f + 4.0f;

        Rectangle slider = { row.x, row.y, row.width, 18.0f };
        float value = out.state.camera_sensitivity;
        GuiSliderBar(slider, "", TextFormat("%.3f", value), &value, 0.02f, 0.5f);
        out.state.camera_sensitivity = value;
    }

    // Non-panel overlays
    if (out.state.show_player_info) {
        draw_player_info(vm);
    }

    if (out.state.show_net_info) {
        draw_net_info(vm);
    }

    return out;
}

DebugUIResult draw_overlay(const DebugUIState& current, const UIViewModel& vm) {
    DebugUIResult out;
    out.state = current;

    if (out.state.show_player_info) {
        draw_player_info(vm);
    }
    if (out.state.show_net_info) {
        draw_net_info(vm);
    }

    return out;
}

} // namespace ui::debug
