#include "../client/core/config.hpp"
#include "../client/core/logger.hpp"
#include "../client/core/resources.hpp"
#include "../client/ecs/components.hpp"
#include "../client/ecs/systems/input_system.hpp"
#include "../client/ecs/systems/player_system.hpp"
#include "../client/ecs/systems/render_system.hpp"
#include "../client/net/client_session.hpp"
#include "../client/renderer/skybox.hpp"
#include "../client/voxel/block_interaction.hpp"
#include "../client/voxel/block_model_loader.hpp"
#include "../client/voxel/block_registry.hpp"
#include "../client/voxel/world.hpp"

#include "../shared/maps/rfmap_io.hpp"
#include "../shared/maps/runtime_paths.hpp"
#include "../shared/transport/local_transport.hpp"
#include "../shared/vfs/vfs.hpp"
#include "../shared/voxel/block_state.hpp"
#include "../server/core/server.hpp"

#include "../ui/raygui.h"
#include "map_editor_style.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

enum class AppMode {
    Init,
    CreateModal,
    OpenModal,
    Editor,
};

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

    bool editMapId = false;
    bool editVersion = false;
    bool editSizeX = false;
    bool editSizeZ = false;
};

struct OpenParams {
    bool needsRefresh = true;
    std::vector<shared::maps::MapFileEntry> entries;
    std::string listText;
    int scrollIndex = 0;
    int active = -1;
};

struct SkyboxParams {
    bool open = false;
    bool needsRefresh = true;
    std::vector<std::uint8_t> ids; // list index -> skyboxKind value
    std::string listText;
    int scrollIndex = 0;
    int active = -1;
};

struct BlockPickerParams {
    bool open = false;
    bool needsRefresh = true;
    std::vector<voxel::BlockType> types;
    std::string listText;
    int scrollIndex = 0;
    int active = -1;
};

static void refresh_block_picker_params(BlockPickerParams& p, voxel::BlockType currentType) {
    p.types.clear();
    p.listText.clear();
    p.scrollIndex = 0;
    p.active = -1;

    for (std::size_t i = 0; i < static_cast<std::size_t>(voxel::BlockType::Count); i++) {
        const auto t = static_cast<voxel::BlockType>(i);
        
        if (t == voxel::BlockType::StoneSlabTop || t == voxel::BlockType::WoodSlabTop) {
            continue;
        }
        
        p.types.push_back(t);

        const auto& info = voxel::BlockRegistry::instance().get_block_info(t);
        if (!p.listText.empty()) p.listText.push_back(';');
        p.listText += info.name ? info.name : "(unnamed)";
    }

    for (int i = 0; i < static_cast<int>(p.types.size()); i++) {
        if (p.types[static_cast<std::size_t>(i)] == currentType) {
            p.active = i;
            break;
        }
    }
}


static bool try_parse_panorama_sky_id(const std::string& filename, std::uint8_t& outId) {
    // Expected pattern: Panorama_Sky_01-512x512.png
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
    p.listText.clear();
    p.scrollIndex = 0;
    p.active = -1;

    p.ids.push_back(0);
    p.listText = "None";

    struct Entry {
        std::uint8_t id{0};
        std::string name;
    };
    std::vector<Entry> tmp;

    const auto files = shared::vfs::list_dir("textures/skybox/panorama");
    for (const auto& fname : files) {
        if (fname.empty() || fname.back() == '/') continue;
        if (fname.size() < 4 || fname.substr(fname.size() - 4) != ".png") continue;

        std::uint8_t id = 0;
        if (!try_parse_panorama_sky_id(fname, id)) continue;
        if (id == 0) continue;
        tmp.push_back(Entry{id, fname});
    }

    std::sort(tmp.begin(), tmp.end(), [](const Entry& a, const Entry& b) {
        if (a.id != b.id) return a.id < b.id;
        return a.name < b.name;
    });

    for (const auto& e : tmp) {
        p.ids.push_back(e.id);
        p.listText.push_back(';');
        p.listText += e.name;
    }

    for (int i = 0; i < static_cast<int>(p.ids.size()); i++) {
        if (p.ids[static_cast<std::size_t>(i)] == currentId) {
            p.active = i;
            break;
        }
    }
}

static void refresh_open_params(OpenParams& p) {
    p.entries = shared::maps::list_available_maps();
    p.listText.clear();
    p.scrollIndex = 0;
    p.active = -1;

    for (const auto& entry : p.entries) {
        if (!p.listText.empty()) p.listText.push_back(';');
        p.listText += entry.filename;
    }

    if (p.entries.empty()) {
        p.listText = "(no .rfmap files found)";
    }
}

struct RaycastHit {
    bool hit{false};
    int x{0};
    int y{0};
    int z{0};
    int face{0};
    voxel::BlockType blockType{voxel::BlockType::Air};
};

static RaycastHit raycast_voxels(const voxel::World& world, const Vector3& origin, const Vector3& direction, float maxDistance) {
    RaycastHit result;

    const float len = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (len < 0.0001f) return result;

    Vector3 dir{direction.x / len, direction.y / len, direction.z / len};

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

    while (dist < maxDistance) {
        const voxel::Block b = world.get_block(x, y, z);
        if (b != static_cast<voxel::Block>(voxel::BlockType::Air)) {
            result.hit = true;
            result.x = x;
            result.y = y;
            result.z = z;
            result.face = face;
            result.blockType = static_cast<voxel::BlockType>(b);
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

static void face_to_offset(int face, int& ox, int& oy, int& oz) {
    ox = 0;
    oy = 0;
    oz = 0;
    switch (face) {
        case 0: ox = 1; break;
        case 1: ox = -1; break;
        case 2: oy = 1; break;
        case 3: oy = -1; break;
        case 4: oz = 1; break;
        case 5: oz = -1; break;
        default: break;
    }
}

static void draw_block_highlight(int bx, int by, int bz) {
    Vector3 pos{
        static_cast<float>(bx) + 0.5f,
        static_cast<float>(by) + 0.5f,
        static_cast<float>(bz) + 0.5f,
    };
    DrawCubeWires(pos, 1.02f, 1.02f, 1.02f, BLACK);
}

static std::string build_block_dropdown_items(std::vector<voxel::BlockType>& outTypes) {
    outTypes.clear();
    outTypes.reserve(static_cast<std::size_t>(voxel::BlockType::Count));

    std::string items;
    for (std::size_t i = 0; i < static_cast<std::size_t>(voxel::BlockType::Count); i++) {
        const auto t = static_cast<voxel::BlockType>(i);
        
        if (t == voxel::BlockType::StoneSlabTop || t == voxel::BlockType::WoodSlabTop) {
            continue;
        }
        
        outTypes.push_back(t);

        const auto& info = voxel::BlockRegistry::instance().get_block_info(t);
        if (!items.empty()) items.push_back(';');
        items += info.name ? info.name : "(unnamed)";
    }

    return items;
}

static std::string build_new_template_dropdown_items() {
    return std::string("Floating island;Random chunks");
}

static void derive_bounds_from_sizes(const CreateParams& p, int& chunkMinX, int& chunkMinZ, int& chunkMaxX, int& chunkMaxZ) {
    chunkMinX = 0;
    chunkMinZ = 0;
    chunkMaxX = p.sizeXChunks - 1;
    chunkMaxZ = p.sizeZChunks - 1;
}

static shared::maps::MapTemplate make_empty_template_from_create(const CreateParams& p) {
    shared::maps::MapTemplate map;
    map.mapId = p.mapId;
    map.version = static_cast<std::uint32_t>(p.version);
    derive_bounds_from_sizes(p, map.bounds.chunkMinX, map.bounds.chunkMinZ, map.bounds.chunkMaxX, map.bounds.chunkMaxZ);
    map.worldBoundary = map.bounds;
    map.chunks.clear();
    return map;
}

static void enqueue_template_floating_island(const shared::maps::MapTemplate& templateBounds, std::vector<SetOp>& outOps) {
    outOps.clear();

    const int minX = templateBounds.bounds.chunkMinX * shared::voxel::CHUNK_WIDTH;
    const int minZ = templateBounds.bounds.chunkMinZ * shared::voxel::CHUNK_DEPTH;
    const int maxX = (templateBounds.bounds.chunkMaxX + 1) * shared::voxel::CHUNK_WIDTH - 1;
    const int maxZ = (templateBounds.bounds.chunkMaxZ + 1) * shared::voxel::CHUNK_DEPTH - 1;

    const int centerX = (minX + maxX) / 2;
    const int centerZ = (minZ + maxZ) / 2;
    const int centerY = 64;

    const int widthBlocks = (maxX - minX + 1);
    const int depthBlocks = (maxZ - minZ + 1);
    const int radiusXZ = std::max(6, std::min(widthBlocks, depthBlocks) / 4);
    const int radiusY = std::max(4, radiusXZ / 2);

    const int minY = std::max(0, centerY - radiusY);
    const int maxY = std::min(shared::voxel::CHUNK_HEIGHT - 1, centerY + radiusY);

    const int boundsMinX = std::max(minX, centerX - radiusXZ);
    const int boundsMaxX = std::min(maxX, centerX + radiusXZ);
    const int boundsMinZ = std::max(minZ, centerZ - radiusXZ);
    const int boundsMaxZ = std::min(maxZ, centerZ + radiusXZ);

    for (int z = boundsMinZ; z <= boundsMaxZ; z++) {
        for (int x = boundsMinX; x <= boundsMaxX; x++) {
            const float dx = static_cast<float>(x - centerX) / static_cast<float>(radiusXZ);
            const float dz = static_cast<float>(z - centerZ) / static_cast<float>(radiusXZ);
            const float dXZ2 = dx * dx + dz * dz;
            if (dXZ2 > 1.0f) continue;

            for (int y = minY; y <= maxY; y++) {
                const float dy = static_cast<float>(y - centerY) / static_cast<float>(radiusY);
                const float d2 = dXZ2 + dy * dy;
                if (d2 <= 1.0f) {
                    outOps.push_back(SetOp{x, y, z, shared::voxel::BlockType::Dirt});
                }
            }
        }
    }

    struct XZKey {
        int x;
        int z;
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
    topY.reserve(outOps.size() / 4 + 16);

    for (const auto& op : outOps) {
        XZKey key{op.x, op.z};
        auto it = topY.find(key);
        if (it == topY.end() || op.y > it->second) {
            topY[key] = op.y;
        }
    }

    for (const auto& kv : topY) {
        outOps.push_back(SetOp{kv.first.x, kv.second, kv.first.z, shared::voxel::BlockType::Grass});
    }
}

static void enqueue_template_random_chunks(const shared::maps::MapTemplate& templateBounds, std::vector<SetOp>& outOps) {
    outOps.clear();

    const int minX = templateBounds.bounds.chunkMinX * shared::voxel::CHUNK_WIDTH;
    const int minZ = templateBounds.bounds.chunkMinZ * shared::voxel::CHUNK_DEPTH;
    const int maxX = (templateBounds.bounds.chunkMaxX + 1) * shared::voxel::CHUNK_WIDTH - 1;
    const int maxZ = (templateBounds.bounds.chunkMaxZ + 1) * shared::voxel::CHUNK_DEPTH - 1;

    std::mt19937 rng(static_cast<std::uint32_t>(std::time(nullptr)));
    std::uniform_int_distribution<int> heightJitter(0, 16);

    for (int z = minZ; z <= maxZ; z++) {
        for (int x = minX; x <= maxX; x++) {
            const int h = 48 + heightJitter(rng);
            for (int y = 0; y <= h; y++) {
                shared::voxel::BlockType t = shared::voxel::BlockType::Stone;
                if (y == 0) {
                    t = shared::voxel::BlockType::Bedrock;
                } else if (y == h) {
                    t = shared::voxel::BlockType::Grass;
                } else if (y >= h - 3) {
                    t = shared::voxel::BlockType::Dirt;
                }
                outOps.push_back(SetOp{x, y, z, t});
            }
        }
    }
}

static void enqueue_ops_from_rfmap(const shared::maps::MapTemplate& map, std::vector<SetOp>& outOps) {
    outOps.clear();
    for (const auto& kv : map.chunks) {
        const auto& key = kv.first;
        const auto& chunk = kv.second;
        const int baseX = static_cast<int>(key.first) * shared::voxel::CHUNK_WIDTH;
        const int baseZ = static_cast<int>(key.second) * shared::voxel::CHUNK_DEPTH;
        for (int y = 0; y < shared::voxel::CHUNK_HEIGHT; y++) {
            for (int lz = 0; lz < shared::voxel::CHUNK_DEPTH; lz++) {
                for (int lx = 0; lx < shared::voxel::CHUNK_WIDTH; lx++) {
                    const std::size_t idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(shared::voxel::CHUNK_WIDTH) * static_cast<std::size_t>(shared::voxel::CHUNK_DEPTH) +
                                            static_cast<std::size_t>(lz) * static_cast<std::size_t>(shared::voxel::CHUNK_WIDTH) +
                                            static_cast<std::size_t>(lx);
                    const auto t = chunk.blocks[idx];
                    if (t == shared::voxel::BlockType::Air) continue;
                    outOps.push_back(SetOp{baseX + lx, y, baseZ + lz, t});
                }
            }
        }
    }
}

} // namespace

int main() {
    const int screenWidth = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "Rayflow Map Editor");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    resources::init();

    editor_ui::InitEditorFonts();
    editor_ui::ApplyEditorStyle();

    core::Config::instance().load_from_file("rayflow.conf");
    core::Logger::instance().init(core::Config::instance().logging());
    renderer::Skybox::instance().init();

    if (!voxel::BlockRegistry::instance().init("textures/terrain.png")) {
        TraceLog(LOG_ERROR, "[editor] Failed to initialize block registry");
        CloseWindow();
        return -1;
    }
    
    if (!voxel::BlockModelLoader::instance().init()) {
        TraceLog(LOG_WARNING, "[editor] Failed to initialize block model loader (non-full blocks may render incorrectly)");
    }

    std::vector<voxel::BlockType> paletteTypes;
    build_block_dropdown_items(paletteTypes);
    int activeBlockIndex = static_cast<int>(voxel::BlockType::Dirt);

    BlockPickerParams blockPickerParams;

    const std::string newTemplateItems = build_new_template_dropdown_items();
    bool newTemplateDropdownEdit = false;

    AppMode mode = AppMode::Init;
    CreateParams createParams;
    OpenParams openParams;

    SkyboxParams skyboxParams;

    std::optional<shared::maps::MapTemplate> pendingLoadedMap;

    shared::maps::MapTemplate::VisualSettings visualSettings = shared::maps::default_visual_settings();

    std::vector<SetOp> pendingUploadOps;
    std::size_t uploadCursor = 0;

    std::optional<shared::transport::LocalTransport::Pair> transportPair;
    std::unique_ptr<server::core::Server> server;
    std::unique_ptr<client::net::ClientSession> session;
    std::unique_ptr<voxel::World> world;

    std::optional<shared::proto::ActionRejected> lastReject;
    std::optional<shared::proto::ExportResult> lastExport;

    entt::registry registry;
    std::unique_ptr<ecs::InputSystem> inputSystem;
    std::unique_ptr<ecs::PlayerSystem> playerSystem;
    std::unique_ptr<ecs::RenderSystem> renderSystem;
    entt::entity player = entt::null;

    bool cursorEnabled = true;
    EnableCursor();

    int chunkMinX = 0;
    int chunkMinZ = 0;
    int chunkMaxX = 0;
    int chunkMaxZ = 0;

    bool editMinX = false;
    bool editMinZ = false;
    bool editMaxX = false;
    bool editMaxZ = false;

    const auto enter_editor = [&]() {
        transportPair = shared::transport::LocalTransport::create_pair();

        server::core::Server::Options opts;
        opts.loadLatestMapTemplateFromDisk = false;
        opts.editorCameraMode = true;

        server = std::make_unique<server::core::Server>(transportPair->server, opts);
        server->start();

        session = std::make_unique<client::net::ClientSession>(transportPair->client);
        session->start_handshake();

        unsigned int seed = static_cast<unsigned int>(std::time(nullptr));
        world = std::make_unique<voxel::World>(seed);

        if (pendingLoadedMap.has_value()) {
            visualSettings = pendingLoadedMap->visualSettings;
            world->set_map_template(*pendingLoadedMap);
        } else {
            shared::maps::MapTemplate empty = make_empty_template_from_create(createParams);
            visualSettings = empty.visualSettings;
            world->set_map_template(std::move(empty));
        }

        skyboxParams.needsRefresh = true;

        renderer::Skybox::instance().set_kind(visualSettings.skyboxKind);

        world->set_temperature_override(visualSettings.temperature);
        world->set_humidity_override(visualSettings.humidity);
        world->mark_all_chunks_dirty();

        session->set_on_block_placed([&](const shared::proto::BlockPlaced& ev) {
            if (!world) return;
            auto state = shared::voxel::BlockRuntimeState::from_byte(ev.stateByte);
            world->set_block_with_state(ev.x, ev.y, ev.z, static_cast<voxel::Block>(ev.blockType), state);
        });
        session->set_on_block_broken([&](const shared::proto::BlockBroken& ev) {
            if (!world) return;
            world->set_block(ev.x, ev.y, ev.z, static_cast<voxel::Block>(voxel::BlockType::Air));
        });
        session->set_on_action_rejected([&](const shared::proto::ActionRejected& ev) {
            lastReject = ev;
        });
        session->set_on_export_result([&](const shared::proto::ExportResult& ev) {
            lastExport = ev;
        });

        registry.clear();
        inputSystem = std::make_unique<ecs::InputSystem>();
        playerSystem = std::make_unique<ecs::PlayerSystem>();
        renderSystem = std::make_unique<ecs::RenderSystem>();
        playerSystem->set_client_replica_mode(true);
        playerSystem->set_world(world.get());
        renderSystem->set_world(world.get());

        const Vector3 spawnPos{50.0f, 80.0f, 50.0f};
        player = ecs::PlayerSystem::create_player(registry, spawnPos);

        cursorEnabled = true;
        EnableCursor();

        mode = AppMode::Editor;
    };

    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();

        if (mode == AppMode::Editor && IsKeyPressed(KEY_TAB)) {
            cursorEnabled = !cursorEnabled;
            if (cursorEnabled) {
                EnableCursor();
            } else {
                DisableCursor();
                SetMousePosition(screenWidth / 2, screenHeight / 2);
            }
        }

        if (mode == AppMode::Init || mode == AppMode::CreateModal || mode == AppMode::OpenModal) {
            BeginDrawing();
            ClearBackground(editor_ui::kBgDark);

            for (int i = 0; i < 20; i++) {
                float alpha = 0.03f + (float)i * 0.002f;
                DrawCircle(
                    screenWidth / 2 + (int)(std::sin(i * 0.5f) * 300),
                    screenHeight / 2 + (int)(std::cos(i * 0.7f) * 200),
                    100.0f + i * 20.0f,
                    Fade(editor_ui::kAccentPrimary, alpha)
                );
            }

            const char* titleText = "RAYFLOW MAP EDITOR";
            const auto& fonts = editor_ui::GetFonts();
            Vector2 titleSize = fonts.loaded ? MeasureTextEx(fonts.bold, titleText, 32, 2) : Vector2{(float)MeasureText(titleText, 32), 32};
            float titleX = (screenWidth - titleSize.x) / 2.0f;

            if (fonts.loaded) {
                DrawTextEx(fonts.bold, titleText, {titleX, 60}, 32, 2, editor_ui::kTextPrimary);
            } else {
                DrawText(titleText, (int)titleX, 60, 32, editor_ui::kTextPrimary);
            }

            const char* subtitleText = "Create and edit voxel maps for BedWars";
            Vector2 subSize = fonts.loaded ? MeasureTextEx(fonts.regular, subtitleText, 16, 1) : Vector2{(float)MeasureText(subtitleText, 16), 16};
            float subX = (screenWidth - subSize.x) / 2.0f;
            if (fonts.loaded) {
                DrawTextEx(fonts.regular, subtitleText, {subX, 100}, 16, 1, editor_ui::kTextMuted);
            } else {
                DrawText(subtitleText, (int)subX, 100, 16, editor_ui::kTextMuted);
            }

            if (mode == AppMode::Init) {
                const float btnWidth = 320.0f;
                const float btnHeight = 50.0f;
                const float btnGap = 16.0f;
                const float btnX = (screenWidth - btnWidth) / 2.0f;
                float btnY = screenHeight / 2.0f - 40.0f;

                if (editor_ui::StyledButton({btnX, btnY, btnWidth, btnHeight}, "Create New Map", ICON_FILE_NEW, true)) {
                    mode = AppMode::CreateModal;
                }

                btnY += btnHeight + btnGap;

                if (editor_ui::StyledButton({btnX, btnY, btnWidth, btnHeight}, "Open Existing Map", ICON_FOLDER_OPEN, false)) {
                    openParams.needsRefresh = true;
                    mode = AppMode::OpenModal;
                }

                const char* footerText = "v1.0  |  Press F1 for help";
                Vector2 footerSize = {(float)MeasureText(footerText, 12), 12.0f};
                DrawText(footerText, (int)((screenWidth - footerSize.x) / 2), screenHeight - 40, 12, editor_ui::kTextMuted);
            }

            if (mode == AppMode::CreateModal) {
                editor_ui::DrawModalOverlay(screenWidth, screenHeight);

                const float modalWidth = 500.0f;
                const float modalHeight = 400.0f;
                const Rectangle win{
                    (screenWidth - modalWidth) / 2.0f,
                    (screenHeight - modalHeight) / 2.0f,
                    modalWidth,
                    modalHeight
                };
                editor_ui::DrawModalWindow(win, "Create New Map");

                editor_ui::VerticalLayout layout(win.x + 24, win.y + 56, win.width - 48, 10);

                editor_ui::DrawSectionHeader(layout.NextRow(24), "Map Identity", ICON_INFO);
                layout.AddSpace(4);

                editor_ui::StyledTextBox(layout.NextRow(32), "Map ID", createParams.mapId, sizeof(createParams.mapId), &createParams.editMapId);
                editor_ui::StyledValueBox(layout.NextRow(32), "Version", &createParams.version, 1, 9999, &createParams.editVersion);

                layout.AddSpace(8);

                editor_ui::DrawSectionHeader(layout.NextRow(24), "Dimensions", ICON_BOX_GRID);
                layout.AddSpace(4);

                Rectangle sizeRow = layout.NextRow(32);
                float halfWidth = (sizeRow.width - 16) / 2.0f;
                editor_ui::StyledValueBox({sizeRow.x, sizeRow.y, halfWidth, sizeRow.height}, "Width", &createParams.sizeXChunks, 1, 64, &createParams.editSizeX);
                editor_ui::StyledValueBox({sizeRow.x + halfWidth + 16, sizeRow.y, halfWidth, sizeRow.height}, "Depth", &createParams.sizeZChunks, 1, 64, &createParams.editSizeZ);

                layout.AddSpace(8);

                editor_ui::DrawSectionHeader(layout.NextRow(24), "Starting Template", ICON_LAYERS);
                layout.AddSpace(4);

                Rectangle templateDropdownBounds = layout.NextRow(32);
                editor_ui::DrawStyledLabel({templateDropdownBounds.x, templateDropdownBounds.y, 80, templateDropdownBounds.height}, "Template");

                layout.AddSpace(24);
                const float buttonWidth = (win.width - 48 - 16) / 2.0f;
                const float buttonY = win.y + win.height - 60;

                if (editor_ui::StyledButton({win.x + 24, buttonY, buttonWidth, 40}, "Cancel", ICON_CROSS, false)) {
                    pendingLoadedMap.reset();
                    mode = AppMode::Init;
                }

                const bool canCreate = (createParams.mapId[0] != '\0') && (createParams.version > 0) && (createParams.sizeXChunks > 0) && (createParams.sizeZChunks > 0);
                GuiSetState(canCreate ? STATE_NORMAL : STATE_DISABLED);
                if (editor_ui::StyledButton({win.x + 24 + buttonWidth + 16, buttonY, buttonWidth, 40}, "Create Map", ICON_OK_TICK, true)) {
                    if (canCreate) {
                        pendingLoadedMap.reset();
                        shared::maps::MapTemplate empty = make_empty_template_from_create(createParams);
                        chunkMinX = empty.bounds.chunkMinX;
                        chunkMinZ = empty.bounds.chunkMinZ;
                        chunkMaxX = empty.bounds.chunkMaxX;
                        chunkMaxZ = empty.bounds.chunkMaxZ;

                        if (static_cast<NewMapTemplateKind>(createParams.templateKind) == NewMapTemplateKind::FloatingIsland) {
                            enqueue_template_floating_island(empty, pendingUploadOps);
                        } else {
                            enqueue_template_random_chunks(empty, pendingUploadOps);
                        }
                        uploadCursor = 0;

                        enter_editor();
                    }
                }
                GuiSetState(STATE_NORMAL);

                {
                    Rectangle dropBounds = {
                        templateDropdownBounds.x + 80,
                        templateDropdownBounds.y,
                        templateDropdownBounds.width - 80,
                        templateDropdownBounds.height
                    };
                    if (GuiDropdownBox(dropBounds, newTemplateItems.c_str(), &createParams.templateKind, newTemplateDropdownEdit)) {
                        newTemplateDropdownEdit = !newTemplateDropdownEdit;
                    }
                }
            }

            if (mode == AppMode::OpenModal) {
                editor_ui::DrawModalOverlay(screenWidth, screenHeight);

                const float modalWidth = 540.0f;
                const float modalHeight = 420.0f;
                const Rectangle win{
                    (screenWidth - modalWidth) / 2.0f,
                    (screenHeight - modalHeight) / 2.0f,
                    modalWidth,
                    modalHeight
                };
                editor_ui::DrawModalWindow(win, "Open Existing Map");

                if (openParams.needsRefresh) {
                    refresh_open_params(openParams);
                    openParams.needsRefresh = false;
                }

                editor_ui::VerticalLayout layout(win.x + 24, win.y + 56, win.width - 48, 8);

                std::string dirLabel = "Directory: " + shared::maps::runtime_maps_dir().string();
                editor_ui::DrawStyledLabel(layout.NextRow(20), dirLabel.c_str(), true);

                layout.AddSpace(8);

                Rectangle listBounds = layout.NextRow(200);
                editor_ui::StyledListView(listBounds, openParams.listText.c_str(), &openParams.scrollIndex, &openParams.active);

                layout.AddSpace(8);

                const bool hasSelection = (openParams.active >= 0) && (openParams.active < static_cast<int>(openParams.entries.size()));
                if (hasSelection) {
                    const auto& entry = openParams.entries[static_cast<std::size_t>(openParams.active)];
                    std::string selectedLabel = "Selected: " + entry.filename;
                    editor_ui::DrawStyledLabel(layout.NextRow(20), selectedLabel.c_str(), false);
                } else {
                    editor_ui::DrawStyledLabel(layout.NextRow(20), "No file selected", true);
                }

                const float buttonWidth = (win.width - 48 - 32) / 3.0f;
                const float buttonY = win.y + win.height - 60;

                if (editor_ui::StyledButton({win.x + 24, buttonY, buttonWidth, 40}, "Refresh", ICON_RESTART, false)) {
                    openParams.needsRefresh = true;
                }

                if (editor_ui::StyledButton({win.x + 24 + buttonWidth + 16, buttonY, buttonWidth, 40}, "Cancel", ICON_CROSS, false)) {
                    mode = AppMode::Init;
                }

                GuiSetState(hasSelection ? STATE_NORMAL : STATE_DISABLED);
                if (editor_ui::StyledButton({win.x + 24 + (buttonWidth + 16) * 2, buttonY, buttonWidth, 40}, "Open", ICON_FOLDER_FILE_OPEN, true)) {
                    if (hasSelection) {
                        shared::maps::MapTemplate map;
                        std::string err;
                        const auto& entry = openParams.entries[static_cast<std::size_t>(openParams.active)];
                        const std::string pathStr = entry.path.string();
                        
                        if (shared::maps::read_rfmap(pathStr.c_str(), &map, &err)) {
                            pendingLoadedMap = map;
                            visualSettings = map.visualSettings;
                            std::snprintf(createParams.mapId, sizeof(createParams.mapId), "%s", map.mapId.c_str());
                            createParams.version = static_cast<int>(map.version);
                            chunkMinX = map.bounds.chunkMinX;
                            chunkMinZ = map.bounds.chunkMinZ;
                            chunkMaxX = map.bounds.chunkMaxX;
                            chunkMaxZ = map.bounds.chunkMaxZ;

                            skyboxParams.needsRefresh = true;

                            enqueue_ops_from_rfmap(map, pendingUploadOps);
                            uploadCursor = 0;

                            enter_editor();
                        } else {
                            lastReject.reset();
                            lastExport.reset();
                            TraceLog(LOG_WARNING, "[editor] failed to open map %s: %s", pathStr.c_str(), err.c_str());
                        }
                    }
                }
                GuiSetState(STATE_NORMAL);
            }

            EndDrawing();
            continue;
        }

        if (!session || !world || !inputSystem || !playerSystem || !renderSystem || player == entt::null) {
            mode = AppMode::Init;
            continue;
        }

        session->poll();

        if (const auto& helloOpt = session->server_hello(); helloOpt.has_value()) {
            const unsigned int desiredSeed = static_cast<unsigned int>(helloOpt->worldSeed);
            if (world->get_seed() != desiredSeed) {
                auto preservedTemplate = world->map_template() ? std::optional<shared::maps::MapTemplate>(*world->map_template()) : std::optional<shared::maps::MapTemplate>{};
                world = std::make_unique<voxel::World>(desiredSeed);
                if (preservedTemplate.has_value()) {
                    world->set_map_template(std::move(*preservedTemplate));
                }
                playerSystem->set_world(world.get());
                renderSystem->set_world(world.get());
            }
        }

        if (session->join_ack().has_value() && uploadCursor < pendingUploadOps.size()) {
            constexpr std::size_t kOpsPerFrame = 600;
            const std::size_t end = std::min(pendingUploadOps.size(), uploadCursor + kOpsPerFrame);
            for (std::size_t i = uploadCursor; i < end; i++) {
                const auto& op = pendingUploadOps[i];
                session->send_try_set_block(op.x, op.y, op.z, op.type);
            }
            uploadCursor = end;
        }

        if (!cursorEnabled) {
            inputSystem->update(registry, dt);
            playerSystem->update(registry, dt);
        } else {
            if (registry.all_of<ecs::InputState>(player)) {
                auto& in = registry.get<ecs::InputState>(player);
                in.move_input = {0.0f, 0.0f};
                in.look_input = {0.0f, 0.0f};
                in.jump_pressed = false;
                in.sprint_pressed = false;
                in.primary_action = false;
                in.secondary_action = false;
            }
        }

        auto& transform = registry.get<ecs::Transform>(player);
        auto& fpsCamera = registry.get<ecs::FirstPersonCamera>(player);
        auto& input = registry.get<ecs::InputState>(player);

        const bool camUp = (!cursorEnabled) && IsKeyDown(KEY_E);
        const bool camDown = (!cursorEnabled) && IsKeyDown(KEY_Q);

        session->send_input(
            cursorEnabled ? 0.0f : input.move_input.x,
            cursorEnabled ? 0.0f : input.move_input.y,
            fpsCamera.yaw,
            fpsCamera.pitch,
            cursorEnabled ? false : input.jump_pressed,
            cursorEnabled ? false : input.sprint_pressed,
            camUp,
            camDown);

        if (const auto& snapOpt = session->latest_snapshot(); snapOpt.has_value()) {
            const auto& snap = *snapOpt;
            const float targetX = snap.px;
            const float targetY = snap.py;
            const float targetZ = snap.pz;

            const float t = (dt <= 0.0f) ? 1.0f : (dt * 15.0f);
            const float alpha = (t > 1.0f) ? 1.0f : t;

            transform.position.x = transform.position.x + (targetX - transform.position.x) * alpha;
            transform.position.y = transform.position.y + (targetY - transform.position.y) * alpha;
            transform.position.z = transform.position.z + (targetZ - transform.position.z) * alpha;

            if (registry.all_of<ecs::Velocity>(player)) {
                auto& vel = registry.get<ecs::Velocity>(player);
                vel.linear = {snap.vx, snap.vy, snap.vz};
            }
        }

        Camera3D camera = ecs::PlayerSystem::get_camera(registry, player);
        Vector3 cameraDir{camera.target.x - camera.position.x, camera.target.y - camera.position.y, camera.target.z - camera.position.z};

        RaycastHit hit;
        if (!cursorEnabled) {
            hit = raycast_voxels(*world, camera.position, cameraDir, voxel::BlockInteraction::MAX_REACH_DISTANCE);

            if (hit.hit) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    session->send_try_set_block(hit.x, hit.y, hit.z, shared::voxel::BlockType::Air);
                }

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    int ox = 0, oy = 0, oz = 0;
                    face_to_offset(hit.face, ox, oy, oz);
                    const int px = hit.x + ox;
                    const int py = hit.y + oy;
                    const int pz = hit.z + oz;

                    voxel::BlockType selected = voxel::BlockType::Dirt;
                    if (activeBlockIndex >= 0 && activeBlockIndex < static_cast<int>(paletteTypes.size())) {
                        selected = paletteTypes[activeBlockIndex];
                    }

                    session->send_try_set_block(px, py, pz, static_cast<shared::voxel::BlockType>(selected));
                }
            }
        }

        world->update(transform.position);

        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode3D(camera);
        renderer::Skybox::instance().draw(camera);
        renderSystem->render(registry, camera);
        if (hit.hit) {
            draw_block_highlight(hit.x, hit.y, hit.z);
        }
        EndMode3D();

        const float panelWidth = 340.0f;
        const float panelHeight = static_cast<float>(screenHeight) - 20.0f;
        const Rectangle panel{10, 10, panelWidth, panelHeight};

        DrawRectangleRec(panel, Fade(editor_ui::kBgPanel, 0.95f));
        DrawRectangleLinesEx(panel, 1.0f, editor_ui::kBorderNormal);

        {
            Rectangle headerRect = {panel.x, panel.y, panel.width, 44.0f};
            DrawRectangleRec(headerRect, editor_ui::kBgPanelLight);
            DrawLineEx({panel.x, panel.y + 44.0f}, {panel.x + panel.width, panel.y + 44.0f}, 1.0f, editor_ui::kSeparator);

            const char* headerText = "MAP EDITOR";
            const auto& fonts = editor_ui::GetFonts();
            if (fonts.loaded) {
                DrawTextEx(fonts.bold, headerText, {panel.x + 16, panel.y + 12}, 18, 1, editor_ui::kTextPrimary);
            } else {
                DrawText(headerText, (int)(panel.x + 16), (int)(panel.y + 12), 18, editor_ui::kTextPrimary);
            }

            const char* modeText = cursorEnabled ? "UI MODE" : "EDIT MODE";
            Color modeColor = cursorEnabled ? editor_ui::kWarning : editor_ui::kSuccess;
            float modeX = panel.x + panel.width - 16 - MeasureText(modeText, 12);
            DrawText(modeText, (int)modeX, (int)(panel.y + 16), 12, modeColor);
        }

        editor_ui::VerticalLayout layout(panel.x + 16, panel.y + 60, panel.width - 32, 6);

        editor_ui::DrawSectionHeader(layout.NextRow(24), "Block Palette", ICON_BOX);
        layout.AddSpace(4);

        {
            Rectangle blockRow = layout.NextRow(32);
            editor_ui::DrawStyledLabel({blockRow.x, blockRow.y, 70, blockRow.height}, "Block");

            const char* blockLabel = "(none)";
            if (activeBlockIndex >= 0 && activeBlockIndex < static_cast<int>(paletteTypes.size())) {
                const auto& info = voxel::BlockRegistry::instance().get_block_info(paletteTypes[static_cast<std::size_t>(activeBlockIndex)]);
                blockLabel = info.name ? info.name : "(unnamed)";
            }
            DrawText(blockLabel, (int)(blockRow.x + 75), (int)(blockRow.y + 8), 14, editor_ui::kTextPrimary);

            if (editor_ui::StyledButton({blockRow.x + blockRow.width - 80, blockRow.y, 80, blockRow.height}, "Choose", ICON_BOX, false)) {
                blockPickerParams.needsRefresh = true;
                blockPickerParams.open = true;
            }
        }

        layout.AddSpace(12);

        editor_ui::DrawSectionHeader(layout.NextRow(24), "Map Properties", ICON_INFO);
        layout.AddSpace(4);

        editor_ui::StyledTextBox(layout.NextRow(32), "Map ID", createParams.mapId, sizeof(createParams.mapId), &createParams.editMapId);
        editor_ui::StyledValueBox(layout.NextRow(32), "Version", &createParams.version, 1, 9999, &createParams.editVersion);

        layout.AddSpace(12);

        editor_ui::DrawSectionHeader(layout.NextRow(24), "Export Bounds (Chunks)", ICON_BOX_GRID);
        layout.AddSpace(4);

        Rectangle boundsRow1 = layout.NextRow(32);
        float halfW = (boundsRow1.width - 12) / 2.0f;
        editor_ui::StyledValueBox({boundsRow1.x, boundsRow1.y, halfW, boundsRow1.height}, "Min X", &chunkMinX, -512, 512, &editMinX);
        editor_ui::StyledValueBox({boundsRow1.x + halfW + 12, boundsRow1.y, halfW, boundsRow1.height}, "Min Z", &chunkMinZ, -512, 512, &editMinZ);

        Rectangle boundsRow2 = layout.NextRow(32);
        editor_ui::StyledValueBox({boundsRow2.x, boundsRow2.y, halfW, boundsRow2.height}, "Max X", &chunkMaxX, -512, 512, &editMaxX);
        editor_ui::StyledValueBox({boundsRow2.x + halfW + 12, boundsRow2.y, halfW, boundsRow2.height}, "Max Z", &chunkMaxZ, -512, 512, &editMaxZ);

        layout.AddSpace(12);

        editor_ui::DrawSectionHeader(layout.NextRow(24), "Visual Settings", ICON_COLOR_PICKER);
        layout.AddSpace(4);

        {
            Rectangle skyboxRow = layout.NextRow(32);
            editor_ui::DrawStyledLabel({skyboxRow.x, skyboxRow.y, 70, skyboxRow.height}, "Skybox");

            const std::uint8_t skyId = static_cast<std::uint8_t>(visualSettings.skyboxKind);
            const char* skyLabel = (skyId == 0) ? "None" : TextFormat("Sky %02u", static_cast<unsigned>(skyId));
            DrawText(skyLabel, (int)(skyboxRow.x + 75), (int)(skyboxRow.y + 8), 14, editor_ui::kTextPrimary);

            if (editor_ui::StyledButton({skyboxRow.x + skyboxRow.width - 80, skyboxRow.y, 80, skyboxRow.height}, "Choose", ICON_LENS, false)) {
                skyboxParams.needsRefresh = true;
                skyboxParams.open = true;
            }
        }

        editor_ui::StyledSlider(layout.NextRow(28), "Temp", &visualSettings.temperature, 0.0f, 1.0f, "%.2f");
        editor_ui::StyledSlider(layout.NextRow(28), "Humidity", &visualSettings.humidity, 0.0f, 1.0f, "%.2f");

        renderer::Skybox::instance().set_kind(visualSettings.skyboxKind);

        {
            static float lastAppliedTemp = -1.0f;
            static float lastAppliedHum = -1.0f;
            const bool tempChanged = (world && std::fabs(lastAppliedTemp - visualSettings.temperature) > 0.001f);
            const bool humChanged = (world && std::fabs(lastAppliedHum - visualSettings.humidity) > 0.001f);
            if (tempChanged || humChanged) {
                world->set_temperature_override(visualSettings.temperature);
                world->set_humidity_override(visualSettings.humidity);
                world->mark_all_chunks_dirty();
                lastAppliedTemp = visualSettings.temperature;
                lastAppliedHum = visualSettings.humidity;
            }
        }

        layout.AddSpace(16);

        editor_ui::DrawSeparator(panel.x + 16, layout.GetY(), panel.width - 32);
        layout.AddSpace(12);

        if (editor_ui::StyledButton(layout.NextRow(42), "Export Map", ICON_FILE_SAVE, true)) {
            session->send_try_export_map(
                createParams.mapId,
                static_cast<std::uint32_t>(createParams.version),
                chunkMinX, chunkMinZ, chunkMaxX, chunkMaxZ,
                static_cast<std::uint8_t>(visualSettings.skyboxKind),
                visualSettings.timeOfDayHours,
                visualSettings.useMoon,
                visualSettings.sunIntensity,
                visualSettings.ambientIntensity,
                visualSettings.temperature,
                visualSettings.humidity);
        }

        layout.AddSpace(4);
        if (lastExport.has_value()) {
            const char* statusText = lastExport->ok ? "Export successful!" : "Export failed";
            Color statusColor = lastExport->ok ? editor_ui::kSuccess : editor_ui::kError;
            DrawText(statusText, (int)(panel.x + 16), (int)layout.GetY(), 14, statusColor);
        } else if (lastReject.has_value()) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Action rejected (code: %u)", static_cast<unsigned>(lastReject->reason));
            DrawText(buf, (int)(panel.x + 16), (int)layout.GetY(), 14, editor_ui::kWarning);
        }

        {
            const char* helpText = "TAB: Toggle cursor | LMB: Place | RMB: Remove";
            float helpY = panel.y + panel.height - 28;
            DrawRectangle((int)panel.x, (int)(helpY - 4), (int)panel.width, 32, editor_ui::kBgPanelLight);
            DrawText(helpText, (int)(panel.x + 12), (int)helpY, 11, editor_ui::kTextMuted);
        }

        if (blockPickerParams.open) {
            editor_ui::DrawModalOverlay(screenWidth, screenHeight);

            const float modalWidth = 420.0f;
            const float modalHeight = 480.0f;
            const Rectangle win{
                (screenWidth - modalWidth) / 2.0f,
                (screenHeight - modalHeight) / 2.0f,
                modalWidth,
                modalHeight
            };
            editor_ui::DrawModalWindow(win, "Select Block");

            voxel::BlockType currentType = voxel::BlockType::Dirt;
            if (activeBlockIndex >= 0 && activeBlockIndex < static_cast<int>(paletteTypes.size())) {
                currentType = paletteTypes[static_cast<std::size_t>(activeBlockIndex)];
            }

            if (blockPickerParams.needsRefresh) {
                refresh_block_picker_params(blockPickerParams, currentType);
                blockPickerParams.needsRefresh = false;
            }

            editor_ui::VerticalLayout modalLayout(win.x + 24, win.y + 56, win.width - 48, 8);

            editor_ui::DrawStyledLabel(modalLayout.NextRow(20), "Choose a block type:", true);

            modalLayout.AddSpace(4);
            editor_ui::StyledListView(modalLayout.NextRow(280), blockPickerParams.listText.c_str(), &blockPickerParams.scrollIndex, &blockPickerParams.active);

            modalLayout.AddSpace(4);
            const bool hasSelection = (blockPickerParams.active >= 0) && (blockPickerParams.active < static_cast<int>(blockPickerParams.types.size()));
            if (hasSelection) {
                const auto& info = voxel::BlockRegistry::instance().get_block_info(blockPickerParams.types[static_cast<std::size_t>(blockPickerParams.active)]);
                const char* sel = info.name ? info.name : "(unnamed)";
                std::string selectedLabel = std::string("Selected: ") + sel;
                editor_ui::DrawStyledLabel(modalLayout.NextRow(20), selectedLabel.c_str(), false);
            } else {
                editor_ui::DrawStyledLabel(modalLayout.NextRow(20), "No selection", true);
            }

            const float buttonWidth = (win.width - 48 - 16) / 2.0f;
            const float buttonY = win.y + win.height - 60;

            if (editor_ui::StyledButton({win.x + 24, buttonY, buttonWidth, 40}, "Cancel", ICON_CROSS, false)) {
                blockPickerParams.open = false;
            }

            GuiSetState(hasSelection ? STATE_NORMAL : STATE_DISABLED);
            if (editor_ui::StyledButton({win.x + 24 + buttonWidth + 16, buttonY, buttonWidth, 40}, "Select", ICON_OK_TICK, true)) {
                if (hasSelection) {
                    activeBlockIndex = blockPickerParams.active;
                    blockPickerParams.open = false;
                }
            }
            GuiSetState(STATE_NORMAL);
        }

        if (skyboxParams.open) {
            editor_ui::DrawModalOverlay(screenWidth, screenHeight);

            const float modalWidth = 480.0f;
            const float modalHeight = 380.0f;
            const Rectangle win{
                (screenWidth - modalWidth) / 2.0f,
                (screenHeight - modalHeight) / 2.0f,
                modalWidth,
                modalHeight
            };
            editor_ui::DrawModalWindow(win, "Select Skybox");

            const std::uint8_t currentId = static_cast<std::uint8_t>(visualSettings.skyboxKind);
            if (skyboxParams.needsRefresh) {
                refresh_skybox_params(skyboxParams, currentId);
                skyboxParams.needsRefresh = false;
            }

            editor_ui::VerticalLayout modalLayout(win.x + 24, win.y + 56, win.width - 48, 8);

            editor_ui::DrawStyledLabel(modalLayout.NextRow(20), "Select panorama skybox:", true);

            modalLayout.AddSpace(4);
            editor_ui::StyledListView(modalLayout.NextRow(180), skyboxParams.listText.c_str(), &skyboxParams.scrollIndex, &skyboxParams.active);

            modalLayout.AddSpace(4);
            const bool hasSelection = (skyboxParams.active >= 0) && (skyboxParams.active < static_cast<int>(skyboxParams.ids.size()));
            if (hasSelection) {
                const std::uint8_t id = skyboxParams.ids[static_cast<std::size_t>(skyboxParams.active)];
                const char* sel = (id == 0) ? "Selected: None" : TextFormat("Selected: Panorama_Sky_%02u", static_cast<unsigned>(id));
                editor_ui::DrawStyledLabel(modalLayout.NextRow(20), sel, false);
            } else {
                editor_ui::DrawStyledLabel(modalLayout.NextRow(20), "No selection", true);
            }

            const float buttonWidth = (win.width - 48 - 32) / 3.0f;
            const float buttonY = win.y + win.height - 60;

            if (editor_ui::StyledButton({win.x + 24, buttonY, buttonWidth, 40}, "Refresh", ICON_RESTART, false)) {
                skyboxParams.needsRefresh = true;
            }

            if (editor_ui::StyledButton({win.x + 24 + buttonWidth + 16, buttonY, buttonWidth, 40}, "Cancel", ICON_CROSS, false)) {
                skyboxParams.open = false;
            }

            GuiSetState(hasSelection ? STATE_NORMAL : STATE_DISABLED);
            if (editor_ui::StyledButton({win.x + 24 + (buttonWidth + 16) * 2, buttonY, buttonWidth, 40}, "Select", ICON_OK_TICK, true)) {
                if (hasSelection) {
                    const std::uint8_t id = skyboxParams.ids[static_cast<std::size_t>(skyboxParams.active)];
                    visualSettings.skyboxKind = static_cast<shared::maps::MapTemplate::SkyboxKind>(id);
                    skyboxParams.open = false;
                }
            }
            GuiSetState(STATE_NORMAL);
        }

        if (!cursorEnabled) {
            voxel::BlockInteraction::render_crosshair(screenWidth, screenHeight);
        }

        EndDrawing();
    }

    renderer::Skybox::instance().shutdown();
    voxel::BlockRegistry::instance().destroy();
    core::Logger::instance().shutdown();
    editor_ui::ShutdownEditorFonts();
    resources::shutdown();

    CloseWindow();

    if (server) {
        server->stop();
    }
    return 0;
}
