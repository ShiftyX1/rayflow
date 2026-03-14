#pragma once

// =============================================================================
// MapEditorClient — IGameClient implementation for the voxel map editor.
//
// Migrated from the standalone raylib-based map_editor_main.cpp to the
// engine's client architecture (ClientEngine + IGameClient).
//
// Uses:
//   - HDR RenderPipeline (shadow + tone mapping)
//   - ImGui for all editor UI
//   - ECS for player/camera (InputSystem, PlayerSystem)
//   - Engine transport (IClientServices::send) for protocol
//   - Embedded BedWarsServer via local transport (started from main)
// =============================================================================

#include "engine/core/game_interface.hpp"
#include "engine/core/math_types.hpp"
#include "engine/renderer/render_pipeline.hpp"
#include "engine/renderer/camera.hpp"
#include "engine/renderer/gl_shader.hpp"
#include "engine/renderer/gl_mesh.hpp"
#include "engine/maps/rfmap_io.hpp"
#include "engine/maps/runtime_paths.hpp"

#include "games/bedwars/shared/protocol/messages.hpp"

#include <entt/entt.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Forward declarations
namespace ecs {
class InputSystem;
class PlayerSystem;
class RenderSystem;
}

namespace voxel {
using BlockType = shared::voxel::BlockType;
}

namespace editor {

// =============================================================================
// App modes
// =============================================================================

enum class AppMode {
    Init,
    CreateModal,
    OpenModal,
    Editor,
};

// =============================================================================
// Parameter structs (same data as old editor, moved into class)
// =============================================================================

enum class NewMapTemplateKind {
    FloatingIsland = 0,
    RandomChunks = 1,
};

struct SetOp {
    int x{0};
    int y{0};
    int z{0};
    shared::voxel::BlockType type{shared::voxel::BlockType::Air};
};

struct CreateParams {
    char mapId[64] = "default";
    int version = 1;
    int sizeXChunks = 9;
    int sizeZChunks = 9;
    int templateKind = static_cast<int>(NewMapTemplateKind::FloatingIsland);
};

struct OpenParams {
    bool needsRefresh = true;
    std::vector<shared::maps::MapFileEntry> entries;
    int active = -1;
};

struct SkyboxParams {
    bool open = false;
    bool needsRefresh = true;
    std::vector<std::uint8_t> ids;
    std::vector<std::string> names;
    int active = -1;
};

struct BlockPickerParams {
    bool open = false;
    bool needsRefresh = true;
    std::vector<shared::voxel::BlockType> types;
    std::vector<std::string> names;
    int active = -1;
};

// =============================================================================
// Callbacks (set by main entry point)
// =============================================================================

using StartEditorCallback = std::function<void()>;

// =============================================================================
// MapEditorClient
// =============================================================================

class MapEditorClient : public engine::IGameClient {
public:
    MapEditorClient();
    ~MapEditorClient() override;

    // --- Configuration (set before engine.run) ---

    void set_start_editor_callback(StartEditorCallback cb) { onStartEditor_ = std::move(cb); }

    // --- IGameClient implementation ---

    void on_init(engine::IClientServices& engine) override;
    void on_shutdown() override;
    void on_update(float dt) override;
    void on_render() override;
    void on_connected() override;
    void on_disconnected() override;
    void on_server_message(std::span<const std::uint8_t> data) override;

    // --- Public accessors for main to query/set ---

    AppMode mode() const { return mode_; }
    CreateParams& create_params() { return createParams_; }
    OpenParams& open_params() { return openParams_; }

private:
    // --- Message handlers ---
    void handle_message(const bedwars::proto::Message& msg);
    void handle_server_hello(const bedwars::proto::ServerHello& msg);
    void handle_join_ack(const bedwars::proto::JoinAck& msg);
    void handle_state_snapshot(const bedwars::proto::StateSnapshot& msg);
    void handle_block_placed(const bedwars::proto::BlockPlaced& msg);
    void handle_block_broken(const bedwars::proto::BlockBroken& msg);
    void handle_action_rejected(const bedwars::proto::ActionRejected& msg);
    void handle_export_result(const bedwars::proto::ExportResult& msg);

    // --- Sending ---
    void send_client_hello();
    void send_join_match();
    void send_input_frame();
    void send_try_set_block(int x, int y, int z, shared::voxel::BlockType blockType,
                            float hitY = 0.5f, std::uint8_t face = 2);
    void send_try_export_map();

    template<typename T>
    void send_message(const T& msg);

    // --- Update helpers ---
    void update_ecs(float dt);
    void update_interpolation(float dt);
    void update_block_upload();
    void update_visual_settings();
    void process_block_interaction();

    // --- Rendering ---
    void render_3d_scene();
    void render_block_highlight(const rf::Camera& camera);
    void render_ui();

    // --- Template generation ---
    void enter_editor_mode();
    void enqueue_template_ops();

    // --- Helpers ---
    static void build_block_palette(std::vector<shared::voxel::BlockType>& out);

    // --- Raycast ---
    struct RaycastHit {
        bool hit{false};
        int x{0}, y{0}, z{0};
        int face{0};
        float hitY{0.5f};
        shared::voxel::BlockType blockType{shared::voxel::BlockType::Air};
    };
    RaycastHit raycast_voxels(const rf::Vec3& origin, const rf::Vec3& direction, float maxDistance);

private:
    engine::IClientServices* engine_{nullptr};

    // Callback
    StartEditorCallback onStartEditor_;

    // App state
    AppMode mode_{AppMode::Init};
    CreateParams createParams_;
    OpenParams openParams_;
    SkyboxParams skyboxParams_;
    BlockPickerParams blockPickerParams_;

    // Visual settings
    shared::maps::MapTemplate::VisualSettings visualSettings_;
    float lastAppliedTemp_{-1.0f};
    float lastAppliedHum_{-1.0f};
    float lastTime_{-1.0f};
    float lastSun_{-1.0f};
    float lastAmb_{-1.0f};

    // Map data
    std::optional<shared::maps::MapTemplate> pendingLoadedMap_;
    std::vector<SetOp> pendingUploadOps_;
    std::size_t uploadCursor_{0};

    // Export bounds
    int chunkMinX_{0}, chunkMinZ_{0};
    int chunkMaxX_{0}, chunkMaxZ_{0};

    // Block palette
    std::vector<shared::voxel::BlockType> paletteTypes_;
    int activeBlockIndex_{0};

    // Protocol state
    std::uint32_t inputSeq_{0};
    std::uint32_t actionSeq_{0};
    std::optional<bedwars::proto::ServerHello> serverHello_;
    std::optional<bedwars::proto::JoinAck> joinAck_;
    std::optional<bedwars::proto::StateSnapshot> latestSnapshot_;

    // Status messages
    std::optional<bedwars::proto::ActionRejected> lastReject_;
    std::optional<bedwars::proto::ExportResult> lastExport_;

    // ECS
    entt::registry registry_;
    entt::entity playerEntity_{entt::null};
    std::unique_ptr<ecs::InputSystem> inputSystem_;
    std::unique_ptr<ecs::PlayerSystem> playerSystem_;
    std::unique_ptr<ecs::RenderSystem> renderSystem_;

    // Cursor / input mode
    bool cursorEnabled_{true};

    // Render pipeline
    rf::RenderPipeline renderPipeline_;
    bool pipelineInitialized_{false};

    // Highlight wireframe
    rf::GLShader wireShader_;
    rf::GLMesh   wireCube_;
    bool         wireReady_{false};

    // ImGui capture state
    bool imguiCapturesMouse_{false};
    bool imguiCapturesKeyboard_{false};
};

} // namespace editor
