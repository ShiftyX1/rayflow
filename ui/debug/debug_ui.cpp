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

DebugUIResult draw(const DebugUIState& current, const UIViewModel& vm) {
    DebugUIResult out;
    out.state = current;

    const int w = 360;
    const int h = 200;
    Rectangle panel = { static_cast<float>(vm.screen_width - w - 10), 10.0f, static_cast<float>(w), static_cast<float>(h) };

    GuiPanel(panel, "Debug UI (F1)");

    Rectangle r = { panel.x + 10, panel.y + 30, panel.width - 20, 20 };

    {
        bool checked = out.state.show_player_info;
        GuiCheckBox(r, "Show player info", &checked);
        out.state.show_player_info = checked;
    }
    r.y += 26;

    {
        bool checked = out.state.show_net_info;
        GuiCheckBox(r, "Show net info", &checked);
        out.state.show_net_info = checked;
    }
    r.y += 26;

    r.height = 18;
    GuiLabel(r, "Camera sensitivity");
    r.y += 20;

    Rectangle slider = { r.x, r.y, r.width, 18 };
    {
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

} // namespace ui::debug
