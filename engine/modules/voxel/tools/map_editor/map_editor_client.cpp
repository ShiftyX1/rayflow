#include "map_editor_client.hpp"
#include "editor_imgui_ui.hpp"

#include "games/bedwars/shared/protocol/serialization.hpp"

// Engine subsystems accessed through IClientServices
#include "engine/modules/voxel/client/world.hpp"
#include "engine/modules/voxel/client/block_interaction.hpp"
#include "engine/modules/voxel/client/block_registry.hpp"
#include "engine/modules/voxel/client/block_model_loader.hpp"
#include "engine/modules/voxel/shared/block_state.hpp"
#include "engine/ecs/components.hpp"
#include "games/bedwars/client/ecs/systems/fps_input_system.hpp"
#include "games/bedwars/client/ecs/systems/bedwars_player_system.hpp"
#include "engine/ecs/systems/render_system.hpp"
#include "engine/renderer/skybox.hpp"
#include "engine/renderer/camera.hpp"
#include "engine/renderer/batch_2d.hpp"
#include "engine/ui/runtime/ui_manager.hpp"

#include "engine/client/core/input.hpp"
#include "engine/client/core/window.hpp"
#include "engine/core/key_codes.hpp"
#include "engine/core/logging.hpp"
#include "engine/maps/runtime_paths.hpp"
#include "engine/vfs/vfs.hpp"

#include <imgui.h>

#include <glad/gl.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <random>

namespace editor {

namespace proto = bedwars::proto;

// =============================================================================
// Helper: face offset for block placement
// =============================================================================

static void face_to_offset(int face, int& ox, int& oy, int& oz) {
    ox = oy = oz = 0;
    switch (face) {
        case 0: ox =  1; break;
        case 1: ox = -1; break;
        case 2: oy =  1; break;
        case 3: oy = -1; break;
        case 4: oz =  1; break;
        case 5: oz = -1; break;
        default: break;
    }
}

// =============================================================================
// Template generation helpers (preserved from old editor)
// =============================================================================

static shared::maps::MapTemplate make_empty_template(const CreateParams& p) {
    shared::maps::MapTemplate map;
    map.mapId = p.mapId;
    map.version = static_cast<std::uint32_t>(p.version);
    map.bounds.chunkMinX = 0;
    map.bounds.chunkMinZ = 0;
    map.bounds.chunkMaxX = p.sizeXChunks - 1;
    map.bounds.chunkMaxZ = p.sizeZChunks - 1;
    map.worldBoundary = map.bounds;
    map.chunks.clear();
    return map;
}

static void generate_floating_island(const shared::maps::MapTemplate& tmpl, std::vector<SetOp>& ops) {
    ops.clear();
    const int minX = tmpl.bounds.chunkMinX * shared::voxel::CHUNK_WIDTH;
    const int minZ = tmpl.bounds.chunkMinZ * shared::voxel::CHUNK_DEPTH;
    const int maxX = (tmpl.bounds.chunkMaxX + 1) * shared::voxel::CHUNK_WIDTH - 1;
    const int maxZ = (tmpl.bounds.chunkMaxZ + 1) * shared::voxel::CHUNK_DEPTH - 1;

    const int centerX = (minX + maxX) / 2;
    const int centerZ = (minZ + maxZ) / 2;
    const int centerY = 64;

    const int widthBlocks = (maxX - minX + 1);
    const int depthBlocks = (maxZ - minZ + 1);
    const int radiusXZ = std::max(6, std::min(widthBlocks, depthBlocks) / 4);
    const int radiusY = std::max(4, radiusXZ / 2);

    const int bMinY = std::max(0, centerY - radiusY);
    const int bMaxY = std::min(shared::voxel::CHUNK_HEIGHT - 1, centerY + radiusY);
    const int bMinX = std::max(minX, centerX - radiusXZ);
    const int bMaxX = std::min(maxX, centerX + radiusXZ);
    const int bMinZ = std::max(minZ, centerZ - radiusXZ);
    const int bMaxZ = std::min(maxZ, centerZ + radiusXZ);

    // Build the ellipsoid of dirt
    struct XZKey {
        int x, z;
        bool operator==(const XZKey& o) const { return x == o.x && z == o.z; }
    };
    struct XZHash {
        std::size_t operator()(const XZKey& k) const {
            std::size_t h = 1469598103934665603ull;
            h ^= static_cast<std::size_t>(k.x) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
            h ^= static_cast<std::size_t>(k.z) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
            return h;
        }
    };
    std::unordered_map<XZKey, int, XZHash> topY;

    for (int z = bMinZ; z <= bMaxZ; z++) {
        for (int x = bMinX; x <= bMaxX; x++) {
            const float dx = static_cast<float>(x - centerX) / static_cast<float>(radiusXZ);
            const float dz = static_cast<float>(z - centerZ) / static_cast<float>(radiusXZ);
            const float dXZ2 = dx * dx + dz * dz;
            if (dXZ2 > 1.0f) continue;

            for (int y = bMinY; y <= bMaxY; y++) {
                const float dy = static_cast<float>(y - centerY) / static_cast<float>(radiusY);
                if (dXZ2 + dy * dy <= 1.0f) {
                    ops.push_back(SetOp{x, y, z, shared::voxel::BlockType::Dirt});
                    XZKey key{x, z};
                    auto it = topY.find(key);
                    if (it == topY.end() || y > it->second) topY[key] = y;
                }
            }
        }
    }

    // Top layer = grass
    for (const auto& kv : topY) {
        ops.push_back(SetOp{kv.first.x, kv.second, kv.first.z, shared::voxel::BlockType::Grass});
    }
}

static void generate_random_chunks(const shared::maps::MapTemplate& tmpl, std::vector<SetOp>& ops) {
    ops.clear();
    const int minX = tmpl.bounds.chunkMinX * shared::voxel::CHUNK_WIDTH;
    const int minZ = tmpl.bounds.chunkMinZ * shared::voxel::CHUNK_DEPTH;
    const int maxX = (tmpl.bounds.chunkMaxX + 1) * shared::voxel::CHUNK_WIDTH - 1;
    const int maxZ = (tmpl.bounds.chunkMaxZ + 1) * shared::voxel::CHUNK_DEPTH - 1;

    std::mt19937 rng(static_cast<std::uint32_t>(std::time(nullptr)));
    std::uniform_int_distribution<int> heightJitter(0, 16);

    for (int z = minZ; z <= maxZ; z++) {
        for (int x = minX; x <= maxX; x++) {
            const int h = 48 + heightJitter(rng);
            for (int y = 0; y <= h; y++) {
                auto t = shared::voxel::BlockType::Stone;
                if (y == 0) t = shared::voxel::BlockType::Bedrock;
                else if (y == h) t = shared::voxel::BlockType::Grass;
                else if (y >= h - 3) t = shared::voxel::BlockType::Dirt;
                ops.push_back(SetOp{x, y, z, t});
            }
        }
    }
}

static void enqueue_ops_from_rfmap(const shared::maps::MapTemplate& map, std::vector<SetOp>& ops) {
    ops.clear();
    for (const auto& kv : map.chunks) {
        const auto& key = kv.first;
        const auto& chunk = kv.second;
        const int baseX = static_cast<int>(key.first) * shared::voxel::CHUNK_WIDTH;
        const int baseZ = static_cast<int>(key.second) * shared::voxel::CHUNK_DEPTH;
        for (int y = 0; y < shared::voxel::CHUNK_HEIGHT; y++) {
            for (int lz = 0; lz < shared::voxel::CHUNK_DEPTH; lz++) {
                for (int lx = 0; lx < shared::voxel::CHUNK_WIDTH; lx++) {
                    const auto idx = static_cast<std::size_t>(y) *
                                     static_cast<std::size_t>(shared::voxel::CHUNK_WIDTH) *
                                     static_cast<std::size_t>(shared::voxel::CHUNK_DEPTH) +
                                     static_cast<std::size_t>(lz) *
                                     static_cast<std::size_t>(shared::voxel::CHUNK_WIDTH) +
                                     static_cast<std::size_t>(lx);
                    const auto t = chunk.blocks[idx];
                    if (t == shared::voxel::BlockType::Air) continue;
                    ops.push_back(SetOp{baseX + lx, y, baseZ + lz, t});
                }
            }
        }
    }
}

// =============================================================================
// Lifecycle
// =============================================================================

MapEditorClient::MapEditorClient() {
    visualSettings_ = shared::maps::default_visual_settings();
}

MapEditorClient::~MapEditorClient() = default;

void MapEditorClient::on_init(engine::IClientServices& engine) {
    engine_ = &engine;
    engine_->log(engine::LogLevel::Info, "MapEditorClient initialized");

    // Initialize ECS
    inputSystem_ = std::make_unique<ecs::FpsInputSystem>();
    playerSystem_ = std::make_unique<ecs::BedwarsPlayerSystem>();
    renderSystem_ = std::make_unique<ecs::RenderSystem>();
    playerSystem_->set_client_replica_mode(true);

    // Build block palette
    build_block_palette(paletteTypes_);

    // Cursor starts enabled (UI mode)
    rf::Input::instance().setCursorMode(rf::CursorMode::Normal);

    // Initialize render pipeline
    auto& win = rf::Window::instance();
    if (auto* device = engine_->render_device()) {
        if (renderPipeline_.init(*device, win.width(), win.height())) {
            pipelineInitialized_ = true;
            engine_->log(engine::LogLevel::Info, "Editor render pipeline initialized");
        } else {
            engine_->log(engine::LogLevel::Warning, "Render pipeline failed — fallback active");
        }
    }

    // Initialize wireframe highlight resources
    if (wireShader_.loadFromFiles("shaders/solid.vs", "shaders/solid.fs")) {
        wireCube_ = rf::GLMesh::createCube(1.0f);
        wireReady_ = wireCube_.isValid();
    }

    // Initialize ImGui editor UI
    editor_ui::init();
}

void MapEditorClient::on_shutdown() {
    engine_->log(engine::LogLevel::Info, "MapEditorClient shutting down");

    editor_ui::shutdown();

    renderPipeline_.shutdown();
    pipelineInitialized_ = false;

    wireShader_.destroy();
    wireCube_.destroy();
    wireReady_ = false;

    inputSystem_.reset();
    playerSystem_.reset();
    renderSystem_.reset();
    registry_.clear();
    playerEntity_ = entt::null;

    rf::Input::instance().setCursorMode(rf::CursorMode::Normal);
}

// =============================================================================
// Update
// =============================================================================

void MapEditorClient::on_update(float dt) {
    auto& input = rf::Input::instance();

    // ImGui capture state (from previous frame)
    ImGuiIO& io = ImGui::GetIO();
    imguiCapturesMouse_ = io.WantCaptureMouse;
    imguiCapturesKeyboard_ = io.WantCaptureKeyboard;

    // TAB toggles cursor mode in editor
    if (mode_ == AppMode::Editor && !imguiCapturesKeyboard_ &&
        input.isKeyPressed(rf::input::KEY_TAB)) {
        cursorEnabled_ = !cursorEnabled_;
        if (cursorEnabled_) {
            input.setCursorMode(rf::CursorMode::Normal);
        } else {
            input.setCursorMode(rf::CursorMode::Disabled);
        }
    }

    // Only do gameplay updates in editor mode with active session
    if (mode_ == AppMode::Editor && playerEntity_ != entt::null) {
        update_ecs(dt);
        send_input_frame();
        update_interpolation(dt);
        update_block_upload();

        // Update world around player
        if (auto* world = engine_->world()) {
            auto& transform = registry_.get<ecs::Transform>(playerEntity_);
            world->update(transform.position);
        }

        update_visual_settings();
        process_block_interaction();
    }
}

void MapEditorClient::update_ecs(float dt) {
    if (!cursorEnabled_ && !imguiCapturesMouse_) {
        inputSystem_->update(registry_, dt);
        playerSystem_->update(registry_, dt);
    } else {
        // Clear input when cursor is enabled or ImGui captures
        if (playerEntity_ != entt::null && registry_.all_of<ecs::InputState>(playerEntity_)) {
            auto& in = registry_.get<ecs::InputState>(playerEntity_);
            in.move_input = {0.0f, 0.0f};
            in.look_input = {0.0f, 0.0f};
            in.jump_pressed = false;
            in.sprint_pressed = false;
            in.primary_action = false;
            in.secondary_action = false;
        }
    }
}

void MapEditorClient::send_input_frame() {
    if (playerEntity_ == entt::null) return;

    auto& fpsCamera = registry_.get<ecs::FirstPersonCamera>(playerEntity_);
    auto& input = registry_.get<ecs::InputState>(playerEntity_);
    auto& rfInput = rf::Input::instance();

    const bool camUp = !cursorEnabled_ && rfInput.isKeyDown(rf::input::KEY_E);
    const bool camDown = !cursorEnabled_ && rfInput.isKeyDown(rf::input::KEY_Q);

    proto::InputFrame msg;
    msg.seq = inputSeq_++;
    msg.moveX = cursorEnabled_ ? 0.0f : input.move_input.x;
    msg.moveY = cursorEnabled_ ? 0.0f : input.move_input.y;
    msg.yaw = fpsCamera.yaw;
    msg.pitch = fpsCamera.pitch;
    msg.jump = cursorEnabled_ ? false : input.jump_pressed;
    msg.sprint = cursorEnabled_ ? false : input.sprint_pressed;
    msg.camUp = camUp;
    msg.camDown = camDown;

    send_message(msg);
}

void MapEditorClient::update_interpolation(float dt) {
    if (playerEntity_ == entt::null || !latestSnapshot_.has_value()) return;

    auto& transform = registry_.get<ecs::Transform>(playerEntity_);
    const auto& snap = *latestSnapshot_;

    const float t = (dt <= 0.0f) ? 1.0f : (dt * 15.0f);
    const float alpha = (t > 1.0f) ? 1.0f : t;

    transform.position.x += (snap.px - transform.position.x) * alpha;
    transform.position.y += (snap.py - transform.position.y) * alpha;
    transform.position.z += (snap.pz - transform.position.z) * alpha;

    if (registry_.all_of<ecs::Velocity>(playerEntity_)) {
        auto& vel = registry_.get<ecs::Velocity>(playerEntity_);
        vel.linear = {snap.vx, snap.vy, snap.vz};
    }
}

void MapEditorClient::update_block_upload() {
    if (!joinAck_.has_value() || uploadCursor_ >= pendingUploadOps_.size()) return;

    constexpr std::size_t kOpsPerFrame = 600;
    const std::size_t end = std::min(pendingUploadOps_.size(), uploadCursor_ + kOpsPerFrame);
    for (std::size_t i = uploadCursor_; i < end; i++) {
        const auto& op = pendingUploadOps_[i];
        send_try_set_block(op.x, op.y, op.z, op.type);
    }
    uploadCursor_ = end;
}

void MapEditorClient::update_visual_settings() {
    auto* world = engine_->world();
    if (!world) return;

    const bool tempChanged = std::fabs(lastAppliedTemp_ - visualSettings_.temperature) > 0.001f;
    const bool humChanged = std::fabs(lastAppliedHum_ - visualSettings_.humidity) > 0.001f;
    const bool lightChanged = (
        std::fabs(lastTime_ - visualSettings_.timeOfDayHours) > 0.01f ||
        std::fabs(lastSun_ - visualSettings_.sunIntensity) > 0.01f ||
        std::fabs(lastAmb_ - visualSettings_.ambientIntensity) > 0.01f
    );

    if (tempChanged || humChanged) {
        world->set_temperature_override(visualSettings_.temperature);
        world->set_humidity_override(visualSettings_.humidity);
        world->mark_all_chunks_dirty();
        lastAppliedTemp_ = visualSettings_.temperature;
        lastAppliedHum_ = visualSettings_.humidity;
    }

    if (lightChanged) {
        world->set_environment(visualSettings_.timeOfDayHours, visualSettings_.sunIntensity, visualSettings_.ambientIntensity);
        lastTime_ = visualSettings_.timeOfDayHours;
        lastSun_ = visualSettings_.sunIntensity;
        lastAmb_ = visualSettings_.ambientIntensity;
    }

    renderer::Skybox::instance().set_kind(visualSettings_.skyboxKind);
}

void MapEditorClient::process_block_interaction() {
    if (cursorEnabled_ || imguiCapturesMouse_ || playerEntity_ == entt::null) return;

    auto* world = engine_->world();
    if (!world) return;

    ecs::Camera3D ecsCamera = ecs::BedwarsPlayerSystem::get_camera(registry_, playerEntity_);
    rf::Vec3 camDir = glm::normalize(ecsCamera.target - ecsCamera.position);

    RaycastHit hit = raycast_voxels(ecsCamera.position, camDir,
                                     voxel::BlockInteraction::MAX_REACH_DISTANCE);

    auto& rfInput = rf::Input::instance();

    if (hit.hit) {
        // Right click = remove block
        if (rfInput.isMouseButtonPressed(1)) {
            send_try_set_block(hit.x, hit.y, hit.z, shared::voxel::BlockType::Air);
        }

        // Left click = place block
        if (rfInput.isMouseButtonPressed(0)) {
            int ox, oy, oz;
            face_to_offset(hit.face, ox, oy, oz);
            const int px = hit.x + ox;
            const int py = hit.y + oy;
            const int pz = hit.z + oz;

            shared::voxel::BlockType selected = shared::voxel::BlockType::Dirt;
            if (activeBlockIndex_ >= 0 && activeBlockIndex_ < static_cast<int>(paletteTypes_.size())) {
                selected = paletteTypes_[static_cast<std::size_t>(activeBlockIndex_)];
            }

            send_try_set_block(px, py, pz, selected, hit.hitY, static_cast<std::uint8_t>(hit.face));
        }
    }
}

// =============================================================================
// Rendering
// =============================================================================

void MapEditorClient::on_render() {
    // Begin ImGui frame
    editor_ui::new_frame();

    if (mode_ == AppMode::Editor && playerEntity_ != entt::null) {
        render_3d_scene();
    }

    // Draw editor UI (ImGui)
    render_ui();

    // Render ImGui draw data
    editor_ui::render_draw_data();
}

void MapEditorClient::render_3d_scene() {
    auto& win = rf::Window::instance();
    int sw = win.width();
    int sh = win.height();

    // Get camera from ECS
    ecs::Camera3D ecsCamera = ecs::BedwarsPlayerSystem::get_camera(registry_, playerEntity_);

    // Convert to rf::Camera
    rf::Camera camera;
    camera.setPerspective(ecsCamera.fovy, static_cast<float>(sw) / static_cast<float>(sh), 0.1f, 1000.0f);
    camera.setPosition(ecsCamera.position);
    camera.setTarget(ecsCamera.target);
    camera.setUp(ecsCamera.up);

    // Handle resize
    if (pipelineInitialized_ && (renderPipeline_.width() != sw || renderPipeline_.height() != sh)) {
        renderPipeline_.resize(sw, sh);
    }

    // Compute sun direction
    float timeOfDay = visualSettings_.timeOfDayHours;
    float angle = (timeOfDay / 24.0f) * glm::two_pi<float>() - glm::half_pi<float>();
    rf::Vec3 sunDir = rf::Vec3(std::cos(angle), std::sin(angle), 0.3f);
    sunDir.y = std::max(sunDir.y, 0.05f);
    sunDir = glm::normalize(sunDir);

    if (pipelineInitialized_) {
        // Shadow pass
        glDisable(GL_BLEND);
        renderPipeline_.beginShadowPass(camera, sunDir);
        if (auto* world = engine_->world()) {
            world->renderShadowPass(*renderPipeline_.shadowShader(), renderPipeline_);
        }
        renderPipeline_.endShadowPass();

        // HDR scene pass
        renderPipeline_.beginScene();
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Skybox
        glEnable(GL_BLEND);
        renderer::Skybox::instance().draw(camera);

        // Voxel world
        glDisable(GL_BLEND);
        if (auto* world = engine_->world()) {
            world->render(camera, renderPipeline_);
        }

        // Block highlight (wireframe)
        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        if (!cursorEnabled_) {
            render_block_highlight(camera);
        }

        glDisable(GL_DEPTH_TEST);
        renderPipeline_.endScene();

        // Tone mapping
        renderPipeline_.postProcess(1.0f);
    } else {
        // Fallback: simple forward rendering
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        renderer::Skybox::instance().draw(camera);

        if (auto* world = engine_->world()) {
            world->render(camera);
        }

        if (!cursorEnabled_) {
            render_block_highlight(camera);
        }

        glDisable(GL_DEPTH_TEST);
    }

    // 2D overlay: crosshair
    if (!cursorEnabled_) {
        auto& batch = rf::Batch2D::instance();
        batch.begin(sw, sh);
        int cx = sw / 2, cy = sh / 2;
        batch.drawRect(static_cast<float>(cx - 10), static_cast<float>(cy - 1), 20.0f, 2.0f, rf::Color::White());
        batch.drawRect(static_cast<float>(cx - 1), static_cast<float>(cy - 10), 2.0f, 20.0f, rf::Color::White());
        batch.end();
    }
}

void MapEditorClient::render_block_highlight(const rf::Camera& camera) {
    if (!wireReady_ || playerEntity_ == entt::null) return;

    auto* world = engine_->world();
    if (!world) return;

    ecs::Camera3D ecsCamera = ecs::BedwarsPlayerSystem::get_camera(registry_, playerEntity_);
    rf::Vec3 camDir = glm::normalize(ecsCamera.target - ecsCamera.position);
    RaycastHit hit = raycast_voxels(ecsCamera.position, camDir,
                                     voxel::BlockInteraction::MAX_REACH_DISTANCE);
    if (!hit.hit) return;

    rf::Vec3 pos{
        static_cast<float>(hit.x) + 0.5f,
        static_cast<float>(hit.y) + 0.5f,
        static_cast<float>(hit.z) + 0.5f,
    };

    rf::Mat4 model = glm::translate(rf::Mat4(1.0f), pos)
                   * glm::scale(rf::Mat4(1.0f), rf::Vec3(1.02f));
    rf::Mat4 mvp = camera.viewProjectionMatrix() * model;

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(2.0f);

    wireShader_.bind();
    wireShader_.setMat4("mvp", mvp);
    wireShader_.setVec4("color", rf::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    wireCube_.draw();
    wireShader_.unbind();

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void MapEditorClient::render_ui() {
    auto& win = rf::Window::instance();
    int sw = win.width();
    int sh = win.height();

    editor_ui::EditorUIContext ctx;
    ctx.mode = &mode_;
    ctx.createParams = &createParams_;
    ctx.openParams = &openParams_;
    ctx.skyboxParams = &skyboxParams_;
    ctx.blockPickerParams = &blockPickerParams_;
    ctx.visualSettings = &visualSettings_;
    ctx.paletteTypes = &paletteTypes_;
    ctx.activeBlockIndex = &activeBlockIndex_;
    ctx.chunkMinX = &chunkMinX_;
    ctx.chunkMinZ = &chunkMinZ_;
    ctx.chunkMaxX = &chunkMaxX_;
    ctx.chunkMaxZ = &chunkMaxZ_;
    ctx.lastReject = lastReject_.has_value() ? &*lastReject_ : nullptr;
    ctx.lastExport = lastExport_.has_value() ? &*lastExport_ : nullptr;
    ctx.cursorEnabled = cursorEnabled_;
    ctx.screenWidth = sw;
    ctx.screenHeight = sh;

    editor_ui::EditorUIResult result = editor_ui::draw(ctx);

    if (result.requestCreateMap) {
        pendingLoadedMap_.reset();
        shared::maps::MapTemplate empty = make_empty_template(createParams_);
        chunkMinX_ = empty.bounds.chunkMinX;
        chunkMinZ_ = empty.bounds.chunkMinZ;
        chunkMaxX_ = empty.bounds.chunkMaxX;
        chunkMaxZ_ = empty.bounds.chunkMaxZ;

        enqueue_template_ops();
        enter_editor_mode();
    }

    if (result.requestOpenMap && result.openMapIndex >= 0 &&
        result.openMapIndex < static_cast<int>(openParams_.entries.size())) {
        const auto& entry = openParams_.entries[static_cast<std::size_t>(result.openMapIndex)];
        const std::string pathStr = entry.path.string();

        shared::maps::MapTemplate map;
        std::string err;
        if (shared::maps::read_rfmap(pathStr.c_str(), &map, &err)) {
            pendingLoadedMap_ = map;
            visualSettings_ = map.visualSettings;
            std::snprintf(createParams_.mapId, sizeof(createParams_.mapId), "%s", map.mapId.c_str());
            createParams_.version = static_cast<int>(map.version);
            chunkMinX_ = map.bounds.chunkMinX;
            chunkMinZ_ = map.bounds.chunkMinZ;
            chunkMaxX_ = map.bounds.chunkMaxX;
            chunkMaxZ_ = map.bounds.chunkMaxZ;

            enqueue_ops_from_rfmap(map, pendingUploadOps_);
            uploadCursor_ = 0;

            enter_editor_mode();
        } else {
            engine_->log(engine::LogLevel::Warning, "Failed to open map: " + err);
        }
    }

    if (result.requestExport) {
        send_try_export_map();
    }
}

// =============================================================================
// Editor mode entry
// =============================================================================

void MapEditorClient::enter_editor_mode() {
    // Start the embedded server via callback (sets transport on engine)
    if (onStartEditor_) {
        onStartEditor_();
    }

    mode_ = AppMode::Editor;

    // Initialize ECS player
    registry_.clear();
    const rf::Vec3 spawnPos{50.0f, 80.0f, 50.0f};
    playerEntity_ = ecs::BedwarsPlayerSystem::create_player(registry_, spawnPos);
    playerSystem_->set_world(engine_->world());
    renderSystem_->set_world(engine_->world());

    cursorEnabled_ = true;
    rf::Input::instance().setCursorMode(rf::CursorMode::Normal);
}

void MapEditorClient::enqueue_template_ops() {
    shared::maps::MapTemplate empty = make_empty_template(createParams_);

    if (static_cast<NewMapTemplateKind>(createParams_.templateKind) == NewMapTemplateKind::FloatingIsland) {
        generate_floating_island(empty, pendingUploadOps_);
    } else {
        generate_random_chunks(empty, pendingUploadOps_);
    }
    uploadCursor_ = 0;
}

// =============================================================================
// Networking — Events
// =============================================================================

void MapEditorClient::on_connected() {
    engine_->log(engine::LogLevel::Info, "Editor connected to server");
    send_client_hello();
    send_join_match();
}

void MapEditorClient::on_disconnected() {
    engine_->log(engine::LogLevel::Info, "Editor disconnected from server");
    serverHello_.reset();
    joinAck_.reset();
    latestSnapshot_.reset();
}

void MapEditorClient::on_server_message(std::span<const std::uint8_t> data) {
    auto msg = proto::deserialize(data);
    if (!msg) {
        engine_->log(engine::LogLevel::Warning, "Failed to deserialize server message");
        return;
    }
    handle_message(*msg);
}

// =============================================================================
// Message Handling
// =============================================================================

void MapEditorClient::handle_message(const proto::Message& msg) {
    std::visit([this](const auto& m) {
        using T = std::decay_t<decltype(m)>;

        if constexpr (std::is_same_v<T, proto::ServerHello>) {
            handle_server_hello(m);
        }
        else if constexpr (std::is_same_v<T, proto::JoinAck>) {
            handle_join_ack(m);
        }
        else if constexpr (std::is_same_v<T, proto::StateSnapshot>) {
            handle_state_snapshot(m);
        }
        else if constexpr (std::is_same_v<T, proto::BlockPlaced>) {
            handle_block_placed(m);
        }
        else if constexpr (std::is_same_v<T, proto::BlockBroken>) {
            handle_block_broken(m);
        }
        else if constexpr (std::is_same_v<T, proto::ActionRejected>) {
            handle_action_rejected(m);
        }
        else if constexpr (std::is_same_v<T, proto::ExportResult>) {
            handle_export_result(m);
        }
    }, msg);
}

void MapEditorClient::handle_server_hello(const proto::ServerHello& msg) {
    engine_->log(engine::LogLevel::Info, "ServerHello: tickRate=" + std::to_string(msg.tickRate) +
                 " seed=" + std::to_string(msg.worldSeed));

    serverHello_ = msg;

    // Initialize world with seed
    engine_->init_world(msg.worldSeed);

    // Apply map template if we have one
    if (auto* world = engine_->world()) {
        if (pendingLoadedMap_.has_value()) {
            visualSettings_ = pendingLoadedMap_->visualSettings;
            world->set_map_template(*pendingLoadedMap_);
        } else {
            shared::maps::MapTemplate empty = make_empty_template(createParams_);
            visualSettings_ = empty.visualSettings;
            world->set_map_template(std::move(empty));
        }

        skyboxParams_.needsRefresh = true;
        renderer::Skybox::instance().set_kind(visualSettings_.skyboxKind);
        world->set_temperature_override(visualSettings_.temperature);
        world->set_humidity_override(visualSettings_.humidity);
        world->mark_all_chunks_dirty();

        // Update ECS world references
        playerSystem_->set_world(world);
        renderSystem_->set_world(world);
    }
}

void MapEditorClient::handle_join_ack(const proto::JoinAck& msg) {
    engine_->log(engine::LogLevel::Info, "JoinAck: playerId=" + std::to_string(msg.playerId));
    joinAck_ = msg;
}

void MapEditorClient::handle_state_snapshot(const proto::StateSnapshot& msg) {
    latestSnapshot_ = msg;
}

void MapEditorClient::handle_block_placed(const proto::BlockPlaced& msg) {
    if (auto* world = engine_->world()) {
        auto state = shared::voxel::BlockRuntimeState::from_byte(msg.stateByte);
        world->set_block_with_state(msg.x, msg.y, msg.z, static_cast<voxel::Block>(msg.blockType), state);
    }
}

void MapEditorClient::handle_block_broken(const proto::BlockBroken& msg) {
    if (auto* world = engine_->world()) {
        world->set_block(msg.x, msg.y, msg.z, static_cast<voxel::Block>(shared::voxel::BlockType::Air));
    }
}

void MapEditorClient::handle_action_rejected(const proto::ActionRejected& msg) {
    engine_->log(engine::LogLevel::Warning, "Action rejected: seq=" + std::to_string(msg.seq));
    lastReject_ = msg;
}

void MapEditorClient::handle_export_result(const proto::ExportResult& msg) {
    engine_->log(engine::LogLevel::Info, "ExportResult: ok=" + std::to_string(msg.ok) + " path=" + msg.path);
    lastExport_ = msg;
}

// =============================================================================
// Sending
// =============================================================================

void MapEditorClient::send_client_hello() {
    proto::ClientHello msg;
    msg.version = proto::kProtocolVersion;
    msg.clientName = "MapEditor";
    send_message(msg);
}

void MapEditorClient::send_join_match() {
    proto::JoinMatch msg;
    send_message(msg);
}

void MapEditorClient::send_try_set_block(int x, int y, int z, shared::voxel::BlockType blockType,
                                          float hitY, std::uint8_t face) {
    proto::TrySetBlock msg;
    msg.seq = actionSeq_++;
    msg.x = x;
    msg.y = y;
    msg.z = z;
    msg.blockType = blockType;
    msg.hitY = hitY;
    msg.face = face;
    send_message(msg);
}

void MapEditorClient::send_try_export_map() {
    proto::TryExportMap msg;
    msg.seq = actionSeq_++;
    msg.mapId = createParams_.mapId;
    msg.version = static_cast<std::uint32_t>(createParams_.version);
    msg.chunkMinX = chunkMinX_;
    msg.chunkMinZ = chunkMinZ_;
    msg.chunkMaxX = chunkMaxX_;
    msg.chunkMaxZ = chunkMaxZ_;
    msg.skyboxKind = static_cast<std::uint8_t>(visualSettings_.skyboxKind);
    msg.timeOfDayHours = visualSettings_.timeOfDayHours;
    msg.useMoon = visualSettings_.useMoon;
    msg.sunIntensity = visualSettings_.sunIntensity;
    msg.ambientIntensity = visualSettings_.ambientIntensity;
    msg.temperature = visualSettings_.temperature;
    msg.humidity = visualSettings_.humidity;
    send_message(msg);
}

template<typename T>
void MapEditorClient::send_message(const T& msg) {
    auto data = proto::serialize(proto::Message{msg});
    engine_->send(data);
}

// =============================================================================
// Raycast (adapted from old editor — using rf::Vec3 instead of raylib Vector3)
// =============================================================================

MapEditorClient::RaycastHit MapEditorClient::raycast_voxels(
    const rf::Vec3& origin, const rf::Vec3& direction, float maxDistance)
{
    RaycastHit result;

    const float len = glm::length(direction);
    if (len < 0.0001f) return result;

    rf::Vec3 dir = direction / len;

    int x = static_cast<int>(std::floor(origin.x));
    int y = static_cast<int>(std::floor(origin.y));
    int z = static_cast<int>(std::floor(origin.z));

    const int stepX = (dir.x >= 0.0f) ? 1 : -1;
    const int stepY = (dir.y >= 0.0f) ? 1 : -1;
    const int stepZ = (dir.z >= 0.0f) ? 1 : -1;

    const float tDeltaX = (dir.x != 0.0f) ? std::abs(1.0f / dir.x) : 1e30f;
    const float tDeltaY = (dir.y != 0.0f) ? std::abs(1.0f / dir.y) : 1e30f;
    const float tDeltaZ = (dir.z != 0.0f) ? std::abs(1.0f / dir.z) : 1e30f;

    float tMaxX = (dir.x != 0.0f) ? ((stepX > 0 ? (x + 1 - origin.x) : (origin.x - x)) * tDeltaX) : 1e30f;
    float tMaxY = (dir.y != 0.0f) ? ((stepY > 0 ? (y + 1 - origin.y) : (origin.y - y)) * tDeltaY) : 1e30f;
    float tMaxZ = (dir.z != 0.0f) ? ((stepZ > 0 ? (z + 1 - origin.z) : (origin.z - z)) * tDeltaZ) : 1e30f;

    float dist = 0.0f;
    int face = 0;

    auto* world = engine_->world();
    if (!world) return result;

    while (dist < maxDistance) {
        const voxel::Block b = world->get_block(x, y, z);
        if (b != static_cast<voxel::Block>(shared::voxel::BlockType::Air)) {
            result.hit = true;
            result.x = x;
            result.y = y;
            result.z = z;
            result.face = face;
            result.blockType = static_cast<shared::voxel::BlockType>(b);

            rf::Vec3 hitPoint = origin + dir * dist;
            result.hitY = hitPoint.y - static_cast<float>(y);
            result.hitY = std::clamp(result.hitY, 0.0f, 1.0f);
            return result;
        }

        if (tMaxX < tMaxY && tMaxX < tMaxZ) {
            dist = tMaxX;
            tMaxX += tDeltaX;
            x += stepX;
            face = (stepX > 0) ? 1 : 0;
        } else if (tMaxY < tMaxZ) {
            dist = tMaxY;
            tMaxY += tDeltaY;
            y += stepY;
            face = (stepY > 0) ? 3 : 2;
        } else {
            dist = tMaxZ;
            tMaxZ += tDeltaZ;
            z += stepZ;
            face = (stepZ > 0) ? 5 : 4;
        }
    }

    return result;
}

// =============================================================================
// Block Palette
// =============================================================================

void MapEditorClient::build_block_palette(std::vector<shared::voxel::BlockType>& out) {
    out.clear();
    for (int i = 1; i < static_cast<int>(shared::voxel::BlockType::Count); i++) {
        out.push_back(static_cast<shared::voxel::BlockType>(i));
    }
}

} // namespace editor
