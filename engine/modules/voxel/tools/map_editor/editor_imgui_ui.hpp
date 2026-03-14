#pragma once

// =============================================================================
// Editor ImGui UI — All editor panels and modals using Dear ImGui.
//
// Uses the ImGui context already created by the engine's UIManager.
// The editor calls new_frame() / render_draw_data() directly, bypassing
// UIManager::render() (which is designed for the game's XML-based UI).
// =============================================================================

#include "map_editor_client.hpp"

#include "games/bedwars/shared/protocol/messages.hpp"

#include <vector>
#include <string>

namespace editor::editor_ui {

// =============================================================================
// Context passed from MapEditorClient to the UI draw function
// =============================================================================

struct EditorUIContext {
    AppMode* mode{nullptr};
    CreateParams* createParams{nullptr};
    OpenParams* openParams{nullptr};
    SkyboxParams* skyboxParams{nullptr};
    BlockPickerParams* blockPickerParams{nullptr};
    shared::maps::MapTemplate::VisualSettings* visualSettings{nullptr};
    std::vector<shared::voxel::BlockType>* paletteTypes{nullptr};
    int* activeBlockIndex{nullptr};
    int* chunkMinX{nullptr};
    int* chunkMinZ{nullptr};
    int* chunkMaxX{nullptr};
    int* chunkMaxZ{nullptr};

    const bedwars::proto::ActionRejected* lastReject{nullptr};
    const bedwars::proto::ExportResult* lastExport{nullptr};

    bool cursorEnabled{true};
    int screenWidth{0};
    int screenHeight{0};
};

// =============================================================================
// Result of a UI frame (actions requested by buttons etc.)
// =============================================================================

struct EditorUIResult {
    bool requestCreateMap{false};
    bool requestOpenMap{false};
    int openMapIndex{-1};
    bool requestExport{false};
};

// =============================================================================
// Lifecycle (these wrap ImGui backend calls)
// =============================================================================

/// Begin ImGui frame (call before draw()).
void new_frame();

/// Finalize ImGui frame and render (call after draw()).
void render_draw_data();

/// Init callback (currently no-op; ImGui already created by UIManager).
void init();

/// Shutdown callback (currently no-op; ImGui destroyed by UIManager).
void shutdown();

// =============================================================================
// Main draw function — renders all editor UI for the current frame
// =============================================================================

EditorUIResult draw(EditorUIContext& ctx);

} // namespace editor::editor_ui
