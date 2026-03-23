// Debug UI — Dear ImGui implementation (Phase 4)
// Replaces the old raygui-based debug panels with ImGui.

#include "debug_ui.hpp"

#include "engine/core/math_types.hpp"
#include "engine/core/logging.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#if RAYFLOW_HAS_DX11
#include <imgui_impl_dx11.h>
#include <d3d11.h>
#endif

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>

namespace ui::debug {

static rf::Backend s_activeBackend = rf::Backend::OpenGL;

// ============================================================================
// Cached frame stats (updated twice per second to avoid flicker)
// ============================================================================

static int    s_cachedFps{0};
static float  s_cachedFrameTimeMs{0.0f};
static float  s_fpsAccumulator{0.0f};
static int    s_fpsFrameCount{0};
constexpr float kFpsUpdateInterval = 0.5f;

static void update_frame_stats(const UIViewModel& vm) {
    s_fpsAccumulator += vm.dt;
    s_fpsFrameCount++;
    if (s_fpsAccumulator >= kFpsUpdateInterval) {
        s_cachedFps = static_cast<int>(static_cast<float>(s_fpsFrameCount) / s_fpsAccumulator + 0.5f);
        s_cachedFrameTimeMs = (s_fpsAccumulator / static_cast<float>(s_fpsFrameCount)) * 1000.0f;
        s_fpsAccumulator = 0.0f;
        s_fpsFrameCount = 0;
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

void init(GLFWwindow* window, rf::Backend backend, void* dx11Device, void* dx11Context) {
    s_activeBackend = backend;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Disable automatic imgui.ini saving (we manage our own config system)
    io.IniFilename = nullptr;

    // Dark theme with engine-appropriate colors
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.ScrollbarRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.Alpha = 0.95f;

    if (backend == rf::Backend::OpenGL) {
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 410 core");
    }
#if RAYFLOW_HAS_DX11
    else if (backend == rf::Backend::DirectX11) {
        ImGui_ImplGlfw_InitForOther(window, true);
        if (dx11Device && dx11Context) {
            ImGui_ImplDX11_Init(static_cast<ID3D11Device*>(dx11Device),
                                static_cast<ID3D11DeviceContext*>(dx11Context));
        }
    }
#endif

    std::fprintf(stderr, "[DebugUI] Dear ImGui %s initialized (backend: %s)\n",
                 IMGUI_VERSION, backend == rf::Backend::OpenGL ? "OpenGL" : "DX11");
}

void shutdown() {
    if (s_activeBackend == rf::Backend::OpenGL) {
        ImGui_ImplOpenGL3_Shutdown();
    }
#if RAYFLOW_HAS_DX11
    else if (s_activeBackend == rf::Backend::DirectX11) {
        ImGui_ImplDX11_Shutdown();
    }
#endif
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void new_frame() {
    if (s_activeBackend == rf::Backend::OpenGL) {
        ImGui_ImplOpenGL3_NewFrame();
    }
#if RAYFLOW_HAS_DX11
    else if (s_activeBackend == rf::Backend::DirectX11) {
        ImGui_ImplDX11_NewFrame();
    }
#endif
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void render_draw_data() {
    ImGui::Render();
    if (s_activeBackend == rf::Backend::OpenGL) {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
#if RAYFLOW_HAS_DX11
    else if (s_activeBackend == rf::Backend::DirectX11) {
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
#endif
}

// ============================================================================
// Panel helpers
// ============================================================================

static void draw_player_info(const UIViewModel& vm) {
    if (!ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    const auto& p = vm.player;

    ImGui::Text("Position: %.1f, %.1f, %.1f", p.position.x, p.position.y, p.position.z);
    ImGui::Text("Velocity: %.2f, %.2f, %.2f", p.velocity.x, p.velocity.y, p.velocity.z);
    ImGui::Text("Yaw: %.1f  Pitch: %.1f", p.yaw, p.pitch);
    ImGui::Separator();
    ImGui::Text("On Ground: %s", p.on_ground ? "Yes" : "No");
    ImGui::Text("Sprinting: %s", p.sprinting ? "Yes" : "No");
    ImGui::Text("Creative:  %s", p.creative ? "Yes" : "No");
    ImGui::Separator();
    ImGui::Text("Health: %d / %d", p.health, p.max_health);
    ImGui::Text("Team: %u", static_cast<unsigned>(p.team_id));
}

static void draw_net_info(const UIViewModel& vm) {
    if (!ImGui::CollapsingHeader("Network", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    const auto& n = vm.net;

    if (n.is_connecting) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Connecting...");
    } else if (n.connection_failed) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Connection Failed");
        if (!n.connection_error.empty()) {
            ImGui::TextWrapped("Error: %s", n.connection_error.c_str());
        }
    } else if (n.has_server_hello) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Connected");
    } else {
        ImGui::TextDisabled("Disconnected");
    }

    ImGui::Separator();
    ImGui::Text("Ping:       %u ms", n.ping_ms);
    ImGui::Text("Tick Rate:  %u", n.tick_rate);
    ImGui::Text("Server Tick: %llu", static_cast<unsigned long long>(n.server_tick));
    ImGui::Text("Player ID:  %u", n.player_id);
    ImGui::Text("World Seed: %u", n.world_seed);
}

static void draw_frame_info(const UIViewModel& vm) {
    if (!ImGui::CollapsingHeader("Frame", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Text("FPS: %d", s_cachedFps);
    ImGui::Text("Frame Time: %.2f ms", s_cachedFrameTimeMs);
    ImGui::Text("Screen: %d x %d", vm.screen_width, vm.screen_height);
}

// ============================================================================
// Public API
// ============================================================================

DebugUIResult draw_interactive(const DebugUIState& current, const UIViewModel& vm) {
    DebugUIResult out;
    out.state = current;

    update_frame_stats(vm);

    // Main debug window
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 480), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Debug Panel (F1)")) {
        draw_frame_info(vm);
        ImGui::Spacing();

        // Toggles
        ImGui::Checkbox("Show Player Info", &out.state.show_player_info);
        ImGui::Checkbox("Show Network Info", &out.state.show_net_info);

        ImGui::Spacing();
        ImGui::SliderFloat("Camera Sensitivity", &out.state.camera_sensitivity, 0.01f, 1.0f, "%.3f");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (out.state.show_player_info) {
            draw_player_info(vm);
            ImGui::Spacing();
        }

        if (out.state.show_net_info) {
            draw_net_info(vm);
        }
    }
    ImGui::End();

    return out;
}

DebugUIResult draw_overlay(const DebugUIState& current, const UIViewModel& vm) {
    DebugUIResult out;
    out.state = current;

    update_frame_stats(vm);

    // Overlay: transparent, no interaction, top-left corner
    const float padding = 10.0f;
    ImGui::SetNextWindowPos(ImVec2(padding, padding), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5f);

    ImGuiWindowFlags overlay_flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("##DebugOverlay", nullptr, overlay_flags)) {
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Frame");
        ImGui::Text("FPS: %d  (%.2f ms)", s_cachedFps, s_cachedFrameTimeMs);
        ImGui::Text("Screen: %d x %d", vm.screen_width, vm.screen_height);

        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "Player");
        ImGui::Text("Pos:  %.1f  %.1f  %.1f",
                     vm.player.position.x, vm.player.position.y, vm.player.position.z);
        ImGui::Text("Vel:  %.2f  %.2f  %.2f",
                     vm.player.velocity.x, vm.player.velocity.y, vm.player.velocity.z);
        
        const float speed = std::sqrt(
            vm.player.velocity.x * vm.player.velocity.x +
            vm.player.velocity.y * vm.player.velocity.y +
            vm.player.velocity.z * vm.player.velocity.z);
        ImGui::Text("Speed: %.2f  Ground: %s", speed, vm.player.on_ground ? "Y" : "N");
        ImGui::Text("Yaw: %.1f  Pitch: %.1f", vm.player.yaw, vm.player.pitch);
        ImGui::Text("HP: %d/%d  Team: %u", vm.player.health, vm.player.max_health,
                     static_cast<unsigned>(vm.player.team_id));

        ImGui::Spacing();

        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.5f, 1.0f), "Network");
        if (vm.net.has_server_hello) {
            ImGui::Text("Ping: %u ms  Tick: %llu",
                         vm.net.ping_ms,
                         static_cast<unsigned long long>(vm.net.server_tick));
            ImGui::Text("Rate: %u  ID: %u  Seed: %u",
                         vm.net.tick_rate, vm.net.player_id, vm.net.world_seed);
            if (vm.net.is_remote_connection) {
                ImGui::Text("Remote");
            } else {
                ImGui::TextDisabled("Local");
            }
        } else if (vm.net.is_connecting) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Connecting...");
        } else {
            ImGui::TextDisabled("Disconnected");
        }

        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.9f, 0.6f, 1.0f, 1.0f), "Game");
        const char* screen_name = "Unknown";
        switch (vm.game_screen) {
            case GameScreen::MainMenu:    screen_name = "MainMenu";    break;
            case GameScreen::ConnectMenu: screen_name = "ConnectMenu"; break;
            case GameScreen::Connecting:  screen_name = "Connecting";  break;
            case GameScreen::Playing:     screen_name = "Playing";     break;
            case GameScreen::Paused:      screen_name = "Paused";      break;
            case GameScreen::Settings:    screen_name = "Settings";    break;
        }
        ImGui::Text("Screen: %s", screen_name);
        if (vm.game.match_in_progress) {
            ImGui::Text("Match: In Progress");
        } else if (vm.game.match_ended) {
            ImGui::Text("Match: Ended (winner: %u)", static_cast<unsigned>(vm.game.winner_team));
        }
    }
    ImGui::End();

    return out;
}

} // namespace ui::debug
