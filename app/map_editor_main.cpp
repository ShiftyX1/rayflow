#include "../client/core/config.hpp"
#include "../client/core/logger.hpp"
#include "../client/ecs/components.hpp"
#include "../client/ecs/systems/input_system.hpp"
#include "../client/ecs/systems/player_system.hpp"
#include "../client/ecs/systems/render_system.hpp"
#include "../client/net/client_session.hpp"
#include "../client/renderer/lighting_raymarch.hpp"
#include "../client/renderer/skybox.hpp"
#include "../client/voxel/block_interaction.hpp"
#include "../client/voxel/block_registry.hpp"
#include "../client/voxel/world.hpp"

#include "../shared/maps/rfmap_io.hpp"
#include "../shared/transport/local_transport.hpp"
#include "../server/core/server.hpp"

#include "../ui/raygui.h"

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
    std::filesystem::path baseDir{"maps"};
    std::vector<std::filesystem::path> files;
    std::string listText;
    int scrollIndex = 0;
    int active = -1;
};

struct SkyboxParams {
    bool open = false;
    bool needsRefresh = true;
    std::filesystem::path baseDir{"textures/skybox/panorama"};
    std::vector<std::uint8_t> ids; // list index -> skyboxKind value
    std::string listText;
    int scrollIndex = 0;
    int active = -1;
};

static std::filesystem::path choose_maps_dir() {
    namespace fs = std::filesystem;
    const fs::path candidates[] = {fs::path{"maps"}, fs::path{"../maps"}, fs::path{"../../maps"}};
    for (const auto& p : candidates) {
        if (fs::exists(p) && fs::is_directory(p)) return p;
    }
    return fs::path{"maps"};
}

static std::filesystem::path choose_skybox_panorama_dir() {
    namespace fs = std::filesystem;
    const fs::path candidates[] = {
        fs::path{"textures/skybox/panorama"},
        fs::path{"../textures/skybox/panorama"},
        fs::path{"../../textures/skybox/panorama"},
    };
    for (const auto& p : candidates) {
        if (fs::exists(p) && fs::is_directory(p)) return p;
    }
    return fs::path{"textures/skybox/panorama"};
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
    namespace fs = std::filesystem;

    p.baseDir = choose_skybox_panorama_dir();
    p.ids.clear();
    p.listText.clear();
    p.scrollIndex = 0;
    p.active = -1;

    // Index 0 is always "None".
    p.ids.push_back(0);
    p.listText = "None";

    struct Entry {
        std::uint8_t id{0};
        fs::path name;
    };
    std::vector<Entry> tmp;

    std::error_code ec;
    for (const auto& it : fs::directory_iterator(p.baseDir, ec)) {
        if (ec) break;
        if (!it.is_regular_file()) continue;
        const fs::path file = it.path();
        if (file.extension() != ".png") continue;

        std::uint8_t id = 0;
        const std::string fname = file.filename().string();
        if (!try_parse_panorama_sky_id(fname, id)) continue;
        if (id == 0) continue;
        tmp.push_back(Entry{id, file.filename()});
    }

    std::sort(tmp.begin(), tmp.end(), [](const Entry& a, const Entry& b) {
        if (a.id != b.id) return a.id < b.id;
        return a.name.string() < b.name.string();
    });

    for (const auto& e : tmp) {
        p.ids.push_back(e.id);
        p.listText.push_back(';');
        p.listText += e.name.string();
    }

    // Restore selection to current id if present.
    for (int i = 0; i < static_cast<int>(p.ids.size()); i++) {
        if (p.ids[static_cast<std::size_t>(i)] == currentId) {
            p.active = i;
            break;
        }
    }
}

static void refresh_open_params(OpenParams& p) {
    namespace fs = std::filesystem;

    p.baseDir = choose_maps_dir();
    p.files.clear();
    p.listText.clear();
    p.scrollIndex = 0;
    p.active = -1;

    std::vector<fs::path> tmp;
    std::error_code ec;
    for (const auto& it : fs::directory_iterator(p.baseDir, ec)) {
        if (ec) break;
        if (!it.is_regular_file()) continue;
        const fs::path file = it.path();
        if (file.extension() == ".rfmap") tmp.push_back(file.filename());
    }

    std::sort(tmp.begin(), tmp.end());
    for (const auto& name : tmp) {
        p.files.push_back(p.baseDir / name);
        if (!p.listText.empty()) p.listText.push_back(';');
        p.listText += name.string();
    }

    if (p.files.empty()) {
        p.listText = "(no .rfmap files in maps/)";
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

    // First pass: fill island volume with Dirt.
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

    // Second pass: set the top-most filled block at each (x,z) to Grass.
    // This is O(N) over the filled ops, but keeps UX simple.
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

    core::Config::instance().load_from_file("rayflow.conf");
    core::Logger::instance().init(core::Config::instance().logging());

    renderer::LightingRaymarch::instance().init();
    renderer::Skybox::instance().init();

    if (!voxel::BlockRegistry::instance().init("textures/terrain.png")) {
        TraceLog(LOG_ERROR, "[editor] Failed to initialize block registry");
        CloseWindow();
        return -1;
    }

    std::vector<voxel::BlockType> paletteTypes;
    std::string dropdownItems = build_block_dropdown_items(paletteTypes);
    int activeBlockIndex = static_cast<int>(voxel::BlockType::Dirt);
    bool dropdownEditMode = false;

    const std::string newTemplateItems = build_new_template_dropdown_items();
    bool newTemplateDropdownEdit = false;

    AppMode mode = AppMode::Init;
    CreateParams createParams;
    OpenParams openParams;

    SkyboxParams skyboxParams;

    std::optional<shared::maps::MapTemplate> pendingLoadedMap;

    // MV-1: editor-managed render-only settings (exported by the server).
    shared::maps::MapTemplate::VisualSettings visualSettings = shared::maps::default_visual_settings();

    std::vector<SetOp> pendingUploadOps;
    std::size_t uploadCursor = 0;

    // Editor runtime (created only after init screen)
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

        // Start with an empty-bounds template world so the editor doesn't show procedural terrain.
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

        // Apply MV-1 settings in-editor.
        renderer::LightingRaymarch::instance().set_global_light_from_time_of_day(
            visualSettings.timeOfDayHours,
            visualSettings.useMoon,
            visualSettings.sunIntensity,
            visualSettings.ambientIntensity);
        renderer::LightingRaymarch::instance().set_enabled(true);
        renderer::Skybox::instance().set_kind(visualSettings.skyboxKind);

        session->set_on_block_placed([&](const shared::proto::BlockPlaced& ev) {
            if (!world) return;
            world->set_block(ev.x, ev.y, ev.z, static_cast<voxel::Block>(ev.blockType));
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

        // Enter editor with cursor enabled so the user can interact with UI.
        cursorEnabled = true;
        EnableCursor();

        mode = AppMode::Editor;
    };

    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();

        // Global toggle for cursor only inside the editor.
        if (mode == AppMode::Editor && IsKeyPressed(KEY_TAB)) {
            cursorEnabled = !cursorEnabled;
            if (cursorEnabled) {
                EnableCursor();
            } else {
                DisableCursor();
                SetMousePosition(screenWidth / 2, screenHeight / 2);
            }
        }

        // --- INIT/CREATE/OPEN SCREENS ---
        if (mode == AppMode::Init || mode == AppMode::CreateModal || mode == AppMode::OpenModal) {
            BeginDrawing();
            ClearBackground(Color{30, 30, 30, 255});

            GuiLabel(Rectangle{(float)screenWidth * 0.5f - 140, 80, 280, 30}, "Rayflow Map Editor");

            if (mode == AppMode::Init) {
                if (GuiButton(Rectangle{(float)screenWidth * 0.5f - 150, 160, 300, 40}, "Create new map")) {
                    mode = AppMode::CreateModal;
                }
                if (GuiButton(Rectangle{(float)screenWidth * 0.5f - 150, 210, 300, 40}, "Open existing map")) {
                    openParams.needsRefresh = true;
                    mode = AppMode::OpenModal;
                }
            }

            // Create modal
            if (mode == AppMode::CreateModal) {
                DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.55f));
                const Rectangle win{(float)screenWidth * 0.5f - 220, (float)screenHeight * 0.5f - 160, 440, 320};
                GuiPanel(win, "Create new map");

                GuiLabel(Rectangle{win.x + 20, win.y + 40, 80, 24}, "mapId:");
                if (GuiTextBox(Rectangle{win.x + 100, win.y + 40, 320, 24}, createParams.mapId, sizeof(createParams.mapId), createParams.editMapId)) {
                    createParams.editMapId = !createParams.editMapId;
                }

                if (GuiValueBox(Rectangle{win.x + 20, win.y + 74, 400, 24}, "version", &createParams.version, 0, 9999, createParams.editVersion)) {
                    createParams.editVersion = !createParams.editVersion;
                }

                if (GuiValueBox(Rectangle{win.x + 20, win.y + 108, 195, 24}, "sizeXChunks", &createParams.sizeXChunks, 0, 512, createParams.editSizeX)) {
                    createParams.editSizeX = !createParams.editSizeX;
                }
                if (GuiValueBox(Rectangle{win.x + 225, win.y + 108, 195, 24}, "sizeZChunks", &createParams.sizeZChunks, 0, 512, createParams.editSizeZ)) {
                    createParams.editSizeZ = !createParams.editSizeZ;
                }

                GuiLabel(Rectangle{win.x + 20, win.y + 142, 120, 24}, "template:");
                if (GuiDropdownBox(Rectangle{win.x + 140, win.y + 142, 280, 24}, newTemplateItems.c_str(), &createParams.templateKind, newTemplateDropdownEdit)) {
                    newTemplateDropdownEdit = !newTemplateDropdownEdit;
                }

                const bool canCreate = (createParams.mapId[0] != '\0') && (createParams.version > 0) && (createParams.sizeXChunks > 0) && (createParams.sizeZChunks > 0);
                if (GuiButton(Rectangle{win.x + 20, win.y + 260, 190, 36}, "Cancel")) {
                    pendingLoadedMap.reset();
                    mode = AppMode::Init;
                }
                if (GuiButton(Rectangle{win.x + 230, win.y + 260, 190, 36}, canCreate ? "Create" : "Create (fill required)") && canCreate) {
                    pendingLoadedMap.reset();
                    shared::maps::MapTemplate empty = make_empty_template_from_create(createParams);
                    chunkMinX = empty.bounds.chunkMinX;
                    chunkMinZ = empty.bounds.chunkMinZ;
                    chunkMaxX = empty.bounds.chunkMaxX;
                    chunkMaxZ = empty.bounds.chunkMaxZ;

                    // Pre-generate template ops for server upload.
                    if (static_cast<NewMapTemplateKind>(createParams.templateKind) == NewMapTemplateKind::FloatingIsland) {
                        enqueue_template_floating_island(empty, pendingUploadOps);
                    } else {
                        enqueue_template_random_chunks(empty, pendingUploadOps);
                    }
                    uploadCursor = 0;

                    enter_editor();
                }
            }

            // Open modal
            if (mode == AppMode::OpenModal) {
                DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.55f));
                const Rectangle win{(float)screenWidth * 0.5f - 240, (float)screenHeight * 0.5f - 140, 480, 280};
                GuiPanel(win, "Open existing map");

                if (openParams.needsRefresh) {
                    refresh_open_params(openParams);
                    openParams.needsRefresh = false;
                }

                {
                    std::string baseLabel = std::string("dir: ") + openParams.baseDir.string();
                    GuiLabel(Rectangle{win.x + 20, win.y + 40, 440, 20}, baseLabel.c_str());
                }

                GuiListView(Rectangle{win.x + 20, win.y + 64, 440, 120}, openParams.listText.c_str(), &openParams.scrollIndex, &openParams.active);

                const bool hasSelection = (openParams.active >= 0) && (openParams.active < static_cast<int>(openParams.files.size()));
                if (hasSelection) {
                    const std::string selectedLabel = std::string("selected: ") + openParams.files[static_cast<std::size_t>(openParams.active)].string();
                    GuiLabel(Rectangle{win.x + 20, win.y + 188, 440, 18}, selectedLabel.c_str());
                } else {
                    GuiLabel(Rectangle{win.x + 20, win.y + 188, 440, 18}, "selected: (none)");
                }

                if (GuiButton(Rectangle{win.x + 20, win.y + 210, 140, 36}, "Refresh")) {
                    openParams.needsRefresh = true;
                }

                if (GuiButton(Rectangle{win.x + 170, win.y + 210, 140, 36}, "Cancel")) {
                    mode = AppMode::Init;
                }

                if (GuiButton(Rectangle{win.x + 320, win.y + 210, 140, 36}, hasSelection ? "Open" : "Open (select file)")) {
                    if (!hasSelection) {
                        // Nothing selected.
                    } else {
                    shared::maps::MapTemplate map;
                    std::string err;
                    const std::string path = openParams.files[static_cast<std::size_t>(openParams.active)].string();
                    if (shared::maps::read_rfmap(path.c_str(), &map, &err)) {
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
                        TraceLog(LOG_WARNING, "[editor] failed to open map %s: %s", path.c_str(), err.c_str());
                    }
                    }
                }
            }

            EndDrawing();
            continue;
        }

        // --- EDITOR MODE ---
        if (!session || !world || !inputSystem || !playerSystem || !renderSystem || player == entt::null) {
            // Should not happen; fall back to init.
            mode = AppMode::Init;
            continue;
        }

        session->poll();

        // Align the render-world seed with server seed.
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

        // Upload initial template blocks to server (throttled), after join.
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
        renderer::LightingRaymarch::instance().update_volume_if_needed(*world, camera.position);

        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode3D(camera);
        renderer::Skybox::instance().draw(camera);
        renderer::LightingRaymarch::instance().apply_frame_uniforms();
        renderSystem->render(registry, camera);
        if (hit.hit) {
            draw_block_highlight(hit.x, hit.y, hit.z);
        }
        EndMode3D();

        const Rectangle panel{10, 10, 320, 420};
        GuiPanel(panel, "Map Editor");

        Rectangle r{20, 40, 300, 24};
        GuiLabel(r, "Block:");
        r.y += 22;
        if (GuiDropdownBox(Rectangle{20, r.y, 300, 24}, dropdownItems.c_str(), &activeBlockIndex, dropdownEditMode)) {
            dropdownEditMode = !dropdownEditMode;
        }

        r.y += 34;
        GuiLabel(Rectangle{20, r.y, 80, 24}, "mapId:");
        if (GuiTextBox(Rectangle{100, r.y, 220, 24}, createParams.mapId, sizeof(createParams.mapId), createParams.editMapId)) {
            createParams.editMapId = !createParams.editMapId;
        }

        r.y += 30;
        if (GuiValueBox(Rectangle{20, r.y, 300, 24}, "version", &createParams.version, 0, 9999, createParams.editVersion)) {
            createParams.editVersion = !createParams.editVersion;
        }

        r.y += 30;
        if (GuiValueBox(Rectangle{20, r.y, 145, 24}, "minX", &chunkMinX, -512, 512, editMinX)) editMinX = !editMinX;
        if (GuiValueBox(Rectangle{175, r.y, 145, 24}, "minZ", &chunkMinZ, -512, 512, editMinZ)) editMinZ = !editMinZ;

        r.y += 30;
        if (GuiValueBox(Rectangle{20, r.y, 145, 24}, "maxX", &chunkMaxX, -512, 512, editMaxX)) editMaxX = !editMaxX;
        if (GuiValueBox(Rectangle{175, r.y, 145, 24}, "maxZ", &chunkMaxZ, -512, 512, editMaxZ)) editMaxZ = !editMaxZ;

        r.y += 34;
        if (GuiButton(Rectangle{20, r.y, 300, 28}, "Export")) {
            session->send_try_export_map(
                createParams.mapId,
                static_cast<std::uint32_t>(createParams.version),
                chunkMinX, chunkMinZ, chunkMaxX, chunkMaxZ,
                static_cast<std::uint8_t>(visualSettings.skyboxKind),
                visualSettings.timeOfDayHours,
                visualSettings.useMoon,
                visualSettings.sunIntensity,
                visualSettings.ambientIntensity,
                visualSettings.temperature);
        }

        // --- MV-1: visual settings UI ---
        r.y += 40;
        GuiLabel(Rectangle{20, r.y, 300, 20}, "Visual settings");

        r.y += 22;
        GuiLabel(Rectangle{20, r.y, 80, 24}, "Skybox:");
        {
            const std::uint8_t skyId = static_cast<std::uint8_t>(visualSettings.skyboxKind);
            const char* label = (skyId == 0) ? "None" : TextFormat("Panorama_Sky_%02u", static_cast<unsigned>(skyId));
            GuiLabel(Rectangle{100, r.y, 150, 24}, label);
        }
        if (GuiButton(Rectangle{250, r.y, 70, 24}, "Choose")) {
            skyboxParams.needsRefresh = true;
            skyboxParams.open = true;
        }

        r.y += 30;
        GuiSliderBar(
            Rectangle{20, r.y, 300, 20},
            "Time",
            TextFormat("%.1f", visualSettings.timeOfDayHours),
            &visualSettings.timeOfDayHours,
            0.0f,
            24.0f);

        r.y += 28;
        GuiCheckBox(Rectangle{20, r.y, 20, 20}, "Use moon", &visualSettings.useMoon);

        r.y += 28;
        GuiSliderBar(
            Rectangle{20, r.y, 300, 20},
            "Sun",
            TextFormat("%.2f", visualSettings.sunIntensity),
            &visualSettings.sunIntensity,
            0.0f,
            10.0f);

        r.y += 28;
        GuiSliderBar(
            Rectangle{20, r.y, 300, 20},
            "Ambient",
            TextFormat("%.2f", visualSettings.ambientIntensity),
            &visualSettings.ambientIntensity,
            0.0f,
            5.0f);

        r.y += 28;
        GuiSliderBar(
            Rectangle{20, r.y, 300, 20},
            "Temp",
            TextFormat("%.2f", visualSettings.temperature),
            &visualSettings.temperature,
            0.0f,
            1.0f);

        // Apply settings live while editing.
        renderer::LightingRaymarch::instance().set_global_light_from_time_of_day(
            visualSettings.timeOfDayHours,
            visualSettings.useMoon,
            visualSettings.sunIntensity,
            visualSettings.ambientIntensity);
        renderer::LightingRaymarch::instance().set_temperature(visualSettings.temperature);
        renderer::Skybox::instance().set_kind(visualSettings.skyboxKind);

        // Skybox modal (MV-1): select panorama texture from textures/skybox/panorama.
        if (skyboxParams.open) {
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.55f));
            const Rectangle win{(float)screenWidth * 0.5f - 260, (float)screenHeight * 0.5f - 170, 520, 340};
            GuiPanel(win, "Select skybox");

            const std::uint8_t currentId = static_cast<std::uint8_t>(visualSettings.skyboxKind);
            if (skyboxParams.needsRefresh) {
                refresh_skybox_params(skyboxParams, currentId);
                skyboxParams.needsRefresh = false;
            }

            {
                std::string baseLabel = std::string("dir: ") + skyboxParams.baseDir.string();
                GuiLabel(Rectangle{win.x + 20, win.y + 40, 480, 20}, baseLabel.c_str());
            }

            GuiListView(
                Rectangle{win.x + 20, win.y + 64, 480, 180},
                skyboxParams.listText.c_str(),
                &skyboxParams.scrollIndex,
                &skyboxParams.active);

            const bool hasSelection = (skyboxParams.active >= 0) && (skyboxParams.active < static_cast<int>(skyboxParams.ids.size()));
            if (hasSelection) {
                const std::uint8_t id = skyboxParams.ids[static_cast<std::size_t>(skyboxParams.active)];
                const char* sel = (id == 0) ? "selected: None" : TextFormat("selected: Panorama_Sky_%02u", static_cast<unsigned>(id));
                GuiLabel(Rectangle{win.x + 20, win.y + 252, 480, 18}, sel);
            } else {
                GuiLabel(Rectangle{win.x + 20, win.y + 252, 480, 18}, "selected: (none)");
            }

            if (GuiButton(Rectangle{win.x + 20, win.y + 274, 140, 36}, "Refresh")) {
                skyboxParams.needsRefresh = true;
            }

            if (GuiButton(Rectangle{win.x + 170, win.y + 274, 140, 36}, "Cancel")) {
                skyboxParams.open = false;
            }

            if (GuiButton(Rectangle{win.x + 320, win.y + 274, 180, 36}, hasSelection ? "Select" : "Select (choose)")) {
                if (hasSelection) {
                    const std::uint8_t id = skyboxParams.ids[static_cast<std::size_t>(skyboxParams.active)];
                    visualSettings.skyboxKind = static_cast<shared::maps::MapTemplate::SkyboxKind>(id);
                    skyboxParams.open = false;
                }
            }
        }

        if (lastExport.has_value()) {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "Export: ok=%d reason=%u", lastExport->ok ? 1 : 0, static_cast<unsigned>(lastExport->reason));
            GuiLabel(Rectangle{20, r.y + 34, 300, 20}, buf);
        } else if (lastReject.has_value()) {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "Last reject: reason=%u", static_cast<unsigned>(lastReject->reason));
            GuiLabel(Rectangle{20, r.y + 34, 300, 20}, buf);
        }

        if (!cursorEnabled) {
            voxel::BlockInteraction::render_crosshair(screenWidth, screenHeight);
        }

        EndDrawing();
    }

    renderer::LightingRaymarch::instance().shutdown();
    renderer::Skybox::instance().shutdown();
    voxel::BlockRegistry::instance().destroy();
    core::Logger::instance().shutdown();

    CloseWindow();

    if (server) {
        server->stop();
    }
    return 0;
}
