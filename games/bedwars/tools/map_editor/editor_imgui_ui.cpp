// Editor ImGui UI — Dear ImGui implementation for the map editor.
// Replaces old raygui-based editor panels.

#include "editor_imgui_ui.hpp"

#include "engine/modules/voxel/client/block_registry.hpp"
#include "engine/maps/runtime_paths.hpp"
#include "engine/vfs/vfs.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace editor::editor_ui {

// ============================================================================
// Lifecycle (ImGui context is managed by engine UIManager)
// ============================================================================

void init() {
    // ImGui context already created by UIManager — nothing to do here.
}

void shutdown() {
    // ImGui context destroyed by UIManager — nothing to do here.
}

void new_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void render_draw_data() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// ============================================================================
// Skybox discovery helpers
// ============================================================================

static bool try_parse_panorama_sky_id(const std::string& filename, std::uint8_t& outId) {
    constexpr const char* kPrefix = "Panorama_Sky_";
    const std::size_t pos = filename.find(kPrefix);
    if (pos == std::string::npos) return false;
    const std::size_t numPos = pos + std::char_traits<char>::length(kPrefix);
    if (numPos + 2 > filename.size()) return false;
    const char c0 = filename[numPos];
    const char c1 = filename[numPos + 1];
    if (c0 < '0' || c0 > '9' || c1 < '0' || c1 > '9') return false;
    const int v = (c0 - '0') * 10 + (c1 - '0');
    if (v < 0 || v > 255) return false;
    outId = static_cast<std::uint8_t>(v);
    return true;
}

static void refresh_skybox_params(SkyboxParams& p, std::uint8_t currentId) {
    p.ids.clear();
    p.names.clear();
    p.active = -1;

    // First entry: None
    p.ids.push_back(0);
    p.names.push_back("None");

    struct Entry { std::uint8_t id; std::string name; };
    std::vector<Entry> tmp;

    const auto files = engine::vfs::list_dir("textures/skybox/panorama");
    for (const auto& fname : files) {
        if (fname.empty() || fname.back() == '/') continue;
        if (fname.size() < 4 || fname.substr(fname.size() - 4) != ".png") continue;
        std::uint8_t id = 0;
        if (!try_parse_panorama_sky_id(fname, id)) continue;
        if (id == 0) continue;
        tmp.push_back(Entry{id, fname});
    }
    std::sort(tmp.begin(), tmp.end(), [](const Entry& a, const Entry& b) { return a.id < b.id; });

    for (const auto& e : tmp) {
        p.ids.push_back(e.id);
        p.names.push_back(e.name);
    }

    for (int i = 0; i < static_cast<int>(p.ids.size()); i++) {
        if (p.ids[static_cast<std::size_t>(i)] == currentId) {
            p.active = i;
            break;
        }
    }
    p.needsRefresh = false;
}

// ============================================================================
// Block picker helpers
// ============================================================================

static void refresh_block_picker(BlockPickerParams& p, const std::vector<shared::voxel::BlockType>& palette, int currentIndex) {
    p.types.clear();
    p.names.clear();
    p.active = -1;

    auto& reg = voxel::BlockRegistry::instance();
    for (std::size_t i = 0; i < palette.size(); i++) {
        auto t = palette[i];
        // Skip slab tops (not directly placeable)
        if (t == shared::voxel::BlockType::StoneSlabTop ||
            t == shared::voxel::BlockType::WoodSlabTop) continue;
        p.types.push_back(t);
        p.names.push_back(reg.get_block_info(t).name);
    }

    // Map current activeBlockIndex to picker list index
    if (currentIndex >= 0 && currentIndex < static_cast<int>(palette.size())) {
        auto currentType = palette[static_cast<std::size_t>(currentIndex)];
        for (int i = 0; i < static_cast<int>(p.types.size()); i++) {
            if (p.types[static_cast<std::size_t>(i)] == currentType) {
                p.active = i;
                break;
            }
        }
    }
    p.needsRefresh = false;
}

static void refresh_open_params(OpenParams& p) {
    p.entries = shared::maps::list_available_maps();
    p.active = -1;
    p.needsRefresh = false;
}

// ============================================================================
// Init Screen
// ============================================================================

static void draw_init_screen(EditorUIContext& ctx, EditorUIResult& result) {
    ImGuiIO& io = ImGui::GetIO();
    float sw = io.DisplaySize.x;
    float sh = io.DisplaySize.y;

    // Centered window
    const float winW = 400.0f;
    const float winH = 260.0f;
    ImGui::SetNextWindowPos(ImVec2(sw * 0.5f, sh * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(winW, winH));
    ImGui::Begin("##InitScreen", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    // Title
    ImGui::PushFont(nullptr);
    float titleW = ImGui::CalcTextSize("RAYFLOW MAP EDITOR").x;
    ImGui::SetCursorPosX((winW - titleW) * 0.5f);
    ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f), "RAYFLOW MAP EDITOR");
    ImGui::PopFont();

    float subtitleW = ImGui::CalcTextSize("Create and edit voxel maps for BedWars").x;
    ImGui::SetCursorPosX((winW - subtitleW) * 0.5f);
    ImGui::TextDisabled("Create and edit voxel maps for BedWars");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();

    // Buttons
    const float btnW = 320.0f;
    const float btnH = 40.0f;
    ImGui::SetCursorPosX((winW - btnW) * 0.5f);
    if (ImGui::Button("Create New Map", ImVec2(btnW, btnH))) {
        *ctx.mode = AppMode::CreateModal;
    }

    ImGui::Spacing();
    ImGui::SetCursorPosX((winW - btnW) * 0.5f);
    if (ImGui::Button("Open Existing Map", ImVec2(btnW, btnH))) {
        ctx.openParams->needsRefresh = true;
        *ctx.mode = AppMode::OpenModal;
    }

    ImGui::Spacing();
    ImGui::Spacing();
    float footerW = ImGui::CalcTextSize("v1.0 | Press F1 for help").x;
    ImGui::SetCursorPosX((winW - footerW) * 0.5f);
    ImGui::TextDisabled("v1.0 | Press F1 for help");

    ImGui::End();
}

// ============================================================================
// Create Map Modal
// ============================================================================

static void draw_create_modal(EditorUIContext& ctx, EditorUIResult& result) {
    ImGuiIO& io = ImGui::GetIO();
    float sw = io.DisplaySize.x;
    float sh = io.DisplaySize.y;

    ImGui::SetNextWindowPos(ImVec2(sw * 0.5f, sh * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 380));
    ImGui::Begin("Create New Map", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    auto& p = *ctx.createParams;

    // Map Identity
    if (ImGui::CollapsingHeader("Map Identity", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("Map ID", p.mapId, sizeof(p.mapId));
        ImGui::InputInt("Version", &p.version);
        if (p.version < 1) p.version = 1;
        if (p.version > 9999) p.version = 9999;
    }

    // Dimensions
    if (ImGui::CollapsingHeader("Dimensions (chunks)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderInt("Width", &p.sizeXChunks, 1, 64);
        ImGui::SliderInt("Depth", &p.sizeZChunks, 1, 64);
    }

    // Template
    if (ImGui::CollapsingHeader("Starting Template", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* templates[] = { "Floating Island", "Random Chunks" };
        ImGui::Combo("Template", &p.templateKind, templates, 2);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Buttons
    bool valid = (p.mapId[0] != '\0') && (p.version > 0) && (p.sizeXChunks > 0) && (p.sizeZChunks > 0);

    float btnW = 120.0f;
    ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - btnW * 2 - ImGui::GetStyle().ItemSpacing.x);
    if (ImGui::Button("Cancel", ImVec2(btnW, 0))) {
        *ctx.mode = AppMode::Init;
    }
    ImGui::SameLine();
    if (!valid) ImGui::BeginDisabled();
    if (ImGui::Button("Create Map", ImVec2(btnW, 0))) {
        result.requestCreateMap = true;
    }
    if (!valid) ImGui::EndDisabled();

    ImGui::End();
}

// ============================================================================
// Open Map Modal
// ============================================================================

static void draw_open_modal(EditorUIContext& ctx, EditorUIResult& result) {
    auto& p = *ctx.openParams;

    if (p.needsRefresh) {
        refresh_open_params(p);
    }

    ImGuiIO& io = ImGui::GetIO();
    float sw = io.DisplaySize.x;
    float sh = io.DisplaySize.y;

    ImGui::SetNextWindowPos(ImVec2(sw * 0.5f, sh * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(540, 420));
    ImGui::Begin("Open Existing Map", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    std::string dirLabel = "Directory: " + shared::maps::runtime_maps_dir().string();
    ImGui::TextDisabled("%s", dirLabel.c_str());
    ImGui::Spacing();

    // File list
    if (p.entries.empty()) {
        ImGui::TextDisabled("(no .rfmap files found)");
    } else {
        if (ImGui::BeginListBox("##MapFiles", ImVec2(-FLT_MIN, 200))) {
            for (int i = 0; i < static_cast<int>(p.entries.size()); i++) {
                const bool selected = (p.active == i);
                if (ImGui::Selectable(p.entries[static_cast<std::size_t>(i)].filename.c_str(), selected)) {
                    p.active = i;
                }
            }
            ImGui::EndListBox();
        }
    }

    ImGui::Spacing();
    if (p.active >= 0 && p.active < static_cast<int>(p.entries.size())) {
        ImGui::Text("Selected: %s", p.entries[static_cast<std::size_t>(p.active)].filename.c_str());
    } else {
        ImGui::TextDisabled("No file selected");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Buttons
    float btnW = 100.0f;
    if (ImGui::Button("Refresh", ImVec2(btnW, 0))) {
        p.needsRefresh = true;
    }
    ImGui::SameLine();
    float rightEdge = ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX();
    ImGui::SetCursorPosX(rightEdge - btnW * 2 - ImGui::GetStyle().ItemSpacing.x);
    if (ImGui::Button("Cancel", ImVec2(btnW, 0))) {
        *ctx.mode = AppMode::Init;
    }
    ImGui::SameLine();
    bool hasSelection = (p.active >= 0 && p.active < static_cast<int>(p.entries.size()));
    if (!hasSelection) ImGui::BeginDisabled();
    if (ImGui::Button("Open", ImVec2(btnW, 0))) {
        result.requestOpenMap = true;
        result.openMapIndex = p.active;
    }
    if (!hasSelection) ImGui::EndDisabled();

    ImGui::End();
}

// ============================================================================
// Block Picker Modal
// ============================================================================

static void draw_block_picker(EditorUIContext& ctx, EditorUIResult& /*result*/) {
    auto& bp = *ctx.blockPickerParams;

    if (bp.needsRefresh) {
        refresh_block_picker(bp, *ctx.paletteTypes, *ctx.activeBlockIndex);
    }

    ImGui::SetNextWindowSize(ImVec2(420, 480), ImGuiCond_Appearing);
    if (ImGui::Begin("Block Picker", &bp.open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Choose a block type:");
        ImGui::Spacing();

        if (ImGui::BeginListBox("##BlockList", ImVec2(-FLT_MIN, 280))) {
            for (int i = 0; i < static_cast<int>(bp.names.size()); i++) {
                const bool selected = (bp.active == i);
                if (ImGui::Selectable(bp.names[static_cast<std::size_t>(i)].c_str(), selected)) {
                    bp.active = i;
                }
            }
            ImGui::EndListBox();
        }

        ImGui::Spacing();
        if (bp.active >= 0 && bp.active < static_cast<int>(bp.names.size())) {
            ImGui::Text("Selected: %s", bp.names[static_cast<std::size_t>(bp.active)].c_str());
        } else {
            ImGui::TextDisabled("No selection");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float btnW = 100.0f;
        float rightEdge = ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX();
        ImGui::SetCursorPosX(rightEdge - btnW * 2 - ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::Button("Cancel", ImVec2(btnW, 0))) {
            bp.open = false;
        }
        ImGui::SameLine();
        bool hasSel = (bp.active >= 0 && bp.active < static_cast<int>(bp.types.size()));
        if (!hasSel) ImGui::BeginDisabled();
        if (ImGui::Button("Select", ImVec2(btnW, 0))) {
            // Map picker selection back to palette index
            auto selectedType = bp.types[static_cast<std::size_t>(bp.active)];
            auto& palette = *ctx.paletteTypes;
            for (int i = 0; i < static_cast<int>(palette.size()); i++) {
                if (palette[static_cast<std::size_t>(i)] == selectedType) {
                    *ctx.activeBlockIndex = i;
                    break;
                }
            }
            bp.open = false;
        }
        if (!hasSel) ImGui::EndDisabled();
    }
    ImGui::End();
}

// ============================================================================
// Skybox Picker Modal
// ============================================================================

static void draw_skybox_picker(EditorUIContext& ctx, EditorUIResult& /*result*/) {
    auto& sp = *ctx.skyboxParams;

    if (sp.needsRefresh) {
        refresh_skybox_params(sp, static_cast<std::uint8_t>(ctx.visualSettings->skyboxKind));
    }

    ImGui::SetNextWindowSize(ImVec2(480, 380), ImGuiCond_Appearing);
    if (ImGui::Begin("Skybox Picker", &sp.open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Select panorama skybox:");
        ImGui::Spacing();

        if (ImGui::BeginListBox("##SkyList", ImVec2(-FLT_MIN, 180))) {
            for (int i = 0; i < static_cast<int>(sp.names.size()); i++) {
                const bool selected = (sp.active == i);
                if (ImGui::Selectable(sp.names[static_cast<std::size_t>(i)].c_str(), selected)) {
                    sp.active = i;
                }
            }
            ImGui::EndListBox();
        }

        ImGui::Spacing();
        if (sp.active >= 0 && sp.active < static_cast<int>(sp.ids.size())) {
            auto id = sp.ids[static_cast<std::size_t>(sp.active)];
            if (id == 0) {
                ImGui::Text("Selected: None");
            } else {
                ImGui::Text("Selected: Panorama Sky %02u", static_cast<unsigned>(id));
            }
        } else {
            ImGui::TextDisabled("No selection");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float btnW = 90.0f;
        if (ImGui::Button("Refresh", ImVec2(btnW, 0))) {
            sp.needsRefresh = true;
        }
        ImGui::SameLine();
        float rightEdge = ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX();
        ImGui::SetCursorPosX(rightEdge - btnW * 2 - ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::Button("Cancel", ImVec2(btnW, 0))) {
            sp.open = false;
        }
        ImGui::SameLine();
        bool hasSel = (sp.active >= 0 && sp.active < static_cast<int>(sp.ids.size()));
        if (!hasSel) ImGui::BeginDisabled();
        if (ImGui::Button("Select", ImVec2(btnW, 0))) {
            ctx.visualSettings->skyboxKind = static_cast<shared::maps::MapTemplate::SkyboxKind>(sp.ids[static_cast<std::size_t>(sp.active)]);
            sp.open = false;
        }
        if (!hasSel) ImGui::EndDisabled();
    }
    ImGui::End();
}

// ============================================================================
// Editor Side Panel (in-game)
// ============================================================================

static void draw_editor_panel(EditorUIContext& ctx, EditorUIResult& result) {
    const float panelW = 340.0f;
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, static_cast<float>(ctx.screenHeight) - 20.0f));

    ImGui::Begin("MAP EDITOR", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    // Mode badge
    if (ctx.cursorEnabled) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "UI MODE");
    } else {
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), "EDIT MODE");
    }

    ImGui::Separator();

    // --- Block Palette ---
    if (ImGui::CollapsingHeader("Block", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& palette = *ctx.paletteTypes;
        int idx = *ctx.activeBlockIndex;
        if (idx >= 0 && idx < static_cast<int>(palette.size())) {
            auto t = palette[static_cast<std::size_t>(idx)];
            const auto& info = voxel::BlockRegistry::instance().get_block_info(t);
            ImGui::Text("Current: %s", info.name);
        } else {
            ImGui::TextDisabled("No block selected");
        }
        if (ImGui::Button("Choose Block...")) {
            ctx.blockPickerParams->open = true;
            ctx.blockPickerParams->needsRefresh = true;
        }
    }

    // --- Map Properties ---
    if (ImGui::CollapsingHeader("Map Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("Map ID", ctx.createParams->mapId, sizeof(ctx.createParams->mapId));
        ImGui::InputInt("Version", &ctx.createParams->version);
        if (ctx.createParams->version < 1) ctx.createParams->version = 1;
        if (ctx.createParams->version > 9999) ctx.createParams->version = 9999;
    }

    // --- Export Bounds ---
    if (ImGui::CollapsingHeader("Export Bounds (Chunks)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragInt("Min X", ctx.chunkMinX, 1.0f, -512, 512);
        ImGui::DragInt("Min Z", ctx.chunkMinZ, 1.0f, -512, 512);
        ImGui::DragInt("Max X", ctx.chunkMaxX, 1.0f, -512, 512);
        ImGui::DragInt("Max Z", ctx.chunkMaxZ, 1.0f, -512, 512);
    }

    // --- Visual Settings ---
    if (ImGui::CollapsingHeader("Visual Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& vs = *ctx.visualSettings;

        // Skybox
        {
            const char* skyName = (vs.skyboxKind == shared::maps::MapTemplate::SkyboxKind::None) ? "None" : "Sky";
            ImGui::Text("Skybox: %s (%u)", skyName, static_cast<unsigned>(vs.skyboxKind));
            ImGui::SameLine();
            if (ImGui::SmallButton("Choose...##sky")) {
                ctx.skyboxParams->open = true;
                ctx.skyboxParams->needsRefresh = true;
            }
        }

        ImGui::SliderFloat("Temperature", &vs.temperature, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Humidity", &vs.humidity, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Time of Day", &vs.timeOfDayHours, 0.0f, 24.0f, "%.1f h");
        ImGui::SliderFloat("Sun Intensity", &vs.sunIntensity, 0.0f, 5.0f, "%.2f");
        ImGui::SliderFloat("Ambient", &vs.ambientIntensity, 0.0f, 2.0f, "%.2f");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Export button
    if (ImGui::Button("Export Map", ImVec2(-FLT_MIN, 42))) {
        result.requestExport = true;
    }

    // Status messages
    if (ctx.lastExport) {
        if (ctx.lastExport->ok) {
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "Export successful!");
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "Export failed");
        }
    }
    if (ctx.lastReject) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Action rejected (code: %u)",
            static_cast<unsigned>(ctx.lastReject->reason));
    }

    // Footer
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("TAB: Toggle cursor | LMB: Place | RMB: Remove");

    ImGui::End();
}

// ============================================================================
// Main draw
// ============================================================================

EditorUIResult draw(EditorUIContext& ctx) {
    EditorUIResult result;

    switch (*ctx.mode) {
        case AppMode::Init:
            draw_init_screen(ctx, result);
            break;

        case AppMode::CreateModal:
            draw_create_modal(ctx, result);
            break;

        case AppMode::OpenModal:
            draw_open_modal(ctx, result);
            break;

        case AppMode::Editor:
            draw_editor_panel(ctx, result);

            // Draw open modals
            if (ctx.blockPickerParams->open) {
                draw_block_picker(ctx, result);
            }
            if (ctx.skyboxParams->open) {
                draw_skybox_picker(ctx, result);
            }
            break;
    }

    return result;
}

} // namespace editor::editor_ui
