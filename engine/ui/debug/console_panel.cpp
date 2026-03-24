// console_panel.cpp — ImGui developer console implementation

#include "console_panel.hpp"

#include "engine/core/console/console_log_sink.hpp"
#include "engine/core/console/console_lua_state.hpp"
#include "engine/core/logging.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace ui::debug {

// ============================================================================
// Console persistent state (file-static — single instance)
// ============================================================================

static char              s_inputBuf[512] = {};
static std::vector<std::string> s_history;
static int               s_historyIdx = -1;   // -1 = new line
static bool              s_scrollToBottom = true;
static bool              s_autoScroll = true;

// Level filter (indexed by LOG_* constants 0-7)
static bool s_levelFilter[8] = {
    true,  // LOG_ALL (0)
    true,  // LOG_TRACE (1)
    true,  // LOG_DEBUG (2)
    true,  // LOG_INFO (3)
    true,  // LOG_WARNING (4)
    true,  // LOG_ERROR (5)
    true,  // LOG_FATAL (6)
    false, // LOG_NONE (7) — never shown
};

// Cached snapshot so we don't lock every frame for the same data
static std::vector<engine::console::LogEntry> s_snapshot;
static double s_lastSnapshotTime = -1.0;

// ============================================================================
// Helpers
// ============================================================================

static ImVec4 color_for_level(int level) {
    switch (level) {
        case LOG_TRACE:   return {0.6f, 0.6f, 0.6f, 1.0f};
        case LOG_DEBUG:   return {0.7f, 0.7f, 0.7f, 1.0f};
        case LOG_INFO:    return {0.9f, 0.9f, 0.9f, 1.0f};
        case LOG_WARNING: return {1.0f, 0.85f, 0.2f, 1.0f};
        case LOG_ERROR:   return {1.0f, 0.3f, 0.3f, 1.0f};
        case LOG_FATAL:   return {1.0f, 0.1f, 0.1f, 1.0f};
        default:          return {0.9f, 0.9f, 0.9f, 1.0f};
    }
}

static const char* label_for_level(int level) {
    switch (level) {
        case LOG_ALL:     return "ALL";
        case LOG_TRACE:   return "TRC";
        case LOG_DEBUG:   return "DBG";
        case LOG_INFO:    return "INF";
        case LOG_WARNING: return "WRN";
        case LOG_ERROR:   return "ERR";
        case LOG_FATAL:   return "FTL";
        default:          return "???";
    }
}

// ImGui input-text callback for command history navigation
static int input_callback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        const int prevIdx = s_historyIdx;
        if (data->EventKey == ImGuiKey_UpArrow) {
            if (s_historyIdx == -1) {
                s_historyIdx = static_cast<int>(s_history.size()) - 1;
            } else if (s_historyIdx > 0) {
                --s_historyIdx;
            }
        } else if (data->EventKey == ImGuiKey_DownArrow) {
            if (s_historyIdx >= 0) {
                ++s_historyIdx;
                if (s_historyIdx >= static_cast<int>(s_history.size())) {
                    s_historyIdx = -1;
                }
            }
        }
        if (prevIdx != s_historyIdx) {
            const std::string& entry = (s_historyIdx >= 0)
                ? s_history[static_cast<std::size_t>(s_historyIdx)]
                : std::string{};
            data->DeleteChars(0, data->BufTextLen);
            if (!entry.empty()) {
                data->InsertChars(0, entry.c_str());
            }
        }
    }
    return 0;
}

// ============================================================================
// Public API
// ============================================================================

void draw_console(engine::console::ConsoleLogSink& sink,
                  engine::console::ConsoleLuaState& lua,
                  bool* open) {
    ImGui::SetNextWindowSize(ImVec2(720, 400), ImGuiCond_FirstUseEver);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;

    if (!ImGui::Begin("Developer Console", open, flags)) {
        ImGui::End();
        return;
    }

    // ---- Menu bar: filters + clear ----
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Filter")) {
            ImGui::MenuItem("Trace",   nullptr, &s_levelFilter[LOG_TRACE]);
            ImGui::MenuItem("Debug",   nullptr, &s_levelFilter[LOG_DEBUG]);
            ImGui::MenuItem("Info",    nullptr, &s_levelFilter[LOG_INFO]);
            ImGui::MenuItem("Warning", nullptr, &s_levelFilter[LOG_WARNING]);
            ImGui::MenuItem("Error",   nullptr, &s_levelFilter[LOG_ERROR]);
            ImGui::MenuItem("Fatal",   nullptr, &s_levelFilter[LOG_FATAL]);
            ImGui::EndMenu();
        }
        if (ImGui::SmallButton("Clear")) {
            sink.clear();
        }
        ImGui::Checkbox("Auto-scroll", &s_autoScroll);
        ImGui::EndMenuBar();
    }

    // ---- Log area ----
    const float footerHeight = ImGui::GetStyle().ItemSpacing.y
                             + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("##LogRegion", ImVec2(0, -footerHeight), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);

    // Refresh snapshot (at most once per frame)
    double now = ImGui::GetTime();
    if (now != s_lastSnapshotTime) {
        sink.snapshot(s_snapshot);
        s_lastSnapshotTime = now;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
    for (const auto& entry : s_snapshot) {
        // Level filter
        if (entry.level >= 0 && entry.level < 8 && !s_levelFilter[entry.level])
            continue;

        ImGui::PushStyleColor(ImGuiCol_Text, color_for_level(entry.level));
        ImGui::TextUnformatted(
            ("[" + std::string(label_for_level(entry.level)) + "] " + entry.message).c_str());
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();

    // Auto-scroll
    if (s_autoScroll && s_scrollToBottom) {
        ImGui::SetScrollHereY(1.0f);
    }
    s_scrollToBottom = s_autoScroll;

    ImGui::EndChild();

    // ---- Input line ----
    ImGui::Separator();
    bool reclaimFocus = false;

    ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue
                                   | ImGuiInputTextFlags_CallbackHistory;

    if (ImGui::InputText("##ConsoleInput", s_inputBuf, sizeof(s_inputBuf),
                         inputFlags, input_callback)) {
        std::string cmd(s_inputBuf);
        if (!cmd.empty()) {
            // Push to history (avoid duplicating last entry)
            if (s_history.empty() || s_history.back() != cmd) {
                s_history.push_back(cmd);
            }
            s_historyIdx = -1;

            // Echo command in log
            sink.push(LOG_INFO, "> " + cmd);

            // Execute
            std::string result = lua.execute(cmd);
            if (!result.empty()) {
                // Determine if error (simple heuristic: sol2 errors start
                // with "[sol..." or "[string")
                bool isErr = (result.find("[sol") == 0) ||
                             (result.find("[string") == 0) ||
                             (result.find("error") != std::string::npos &&
                              result.find("error") < 30);
                sink.push(isErr ? LOG_ERROR : LOG_INFO, result);
            }

            s_inputBuf[0] = '\0';
            s_scrollToBottom = true;
        }
        reclaimFocus = true;
    }

    // Keep focus on the input field
    ImGui::SetItemDefaultFocus();
    if (reclaimFocus) {
        ImGui::SetKeyboardFocusHere(-1);
    }

    ImGui::End();
}

} // namespace ui::debug
