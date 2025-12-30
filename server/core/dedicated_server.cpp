#include "dedicated_server.hpp"

#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "../../shared/constants.hpp"
#include "../../shared/maps/rfmap_io.hpp"
#include "../../shared/maps/runtime_paths.hpp"
#include "../../shared/voxel/block_state.hpp"
#include "../scripting/script_engine.hpp"

namespace server::core {

namespace {

constexpr float kDegToRad = 0.017453292519943295f;
constexpr float kPlayerWidth = shared::kPlayerWidth;
constexpr float kPlayerHeight = shared::kPlayerHeight;
constexpr float kGravity = 20.0f;
constexpr float kJumpVelocity = 8.0f;
constexpr float kEps = 1e-4f;
constexpr float kSkin = 1e-3f;
// Maximum height that can be auto-stepped up without jumping (like Minecraft's 0.5 block step-up)
constexpr float kMaxStepUpHeight = 0.5f + kEps;

struct SvLogCfg {
    bool enabled{true};
    bool init{true};
    bool rx{true};
    bool tx{true};
    bool move{false};
    bool coll{false};
};

static SvLogCfg g_log{};

static void logf(std::uint64_t tick, const char* tag, const char* fmt, ...) {
    if (!g_log.enabled) return;
    
    bool show = false;
    if (std::strcmp(tag, "init") == 0) show = g_log.init;
    else if (std::strcmp(tag, "rx") == 0) show = g_log.rx;
    else if (std::strcmp(tag, "tx") == 0) show = g_log.tx;
    else if (std::strcmp(tag, "move") == 0) show = g_log.move;
    else if (std::strcmp(tag, "coll") == 0) show = g_log.coll;
    else show = true;
    
    if (!show) return;

    std::fprintf(stderr, "[rfds][%llu][%s] ", static_cast<unsigned long long>(tick), tag);
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fprintf(stderr, "\n");
}

static inline int fast_floor(float v) {
    return static_cast<int>(std::floor(v));
}

static bool check_aabb_collision(
    const shared::voxel::BlockCollisionInfo& coll,
    int bx, int by, int bz,
    float player_x, float player_y, float player_z,
    float player_half_w, float player_height, float player_half_d
) {
    if (!coll.hasCollision) return false;
    
    // Block collision bounds in world coordinates
    float block_min_x = static_cast<float>(bx) + coll.minX;
    float block_max_x = static_cast<float>(bx) + coll.maxX;
    float block_min_y = static_cast<float>(by) + coll.minY;
    float block_max_y = static_cast<float>(by) + coll.maxY;
    float block_min_z = static_cast<float>(bz) + coll.minZ;
    float block_max_z = static_cast<float>(bz) + coll.maxZ;
    
    // Player bounds
    float player_min_x = player_x - player_half_w;
    float player_max_x = player_x + player_half_w;
    float player_min_y = player_y;
    float player_max_y = player_y + player_height;
    float player_min_z = player_z - player_half_d;
    float player_max_z = player_z + player_half_d;
    
    // Check AABB overlap in all 3 axes
    return player_min_x < block_max_x && player_max_x > block_min_x &&
           player_min_y < block_max_y && player_max_y > block_min_y &&
           player_min_z < block_max_z && player_max_z > block_min_z;
}

static bool check_block_collision_3d(
    shared::voxel::BlockType block_type, 
    int bx, int by, int bz,
    float player_x, float player_y, float player_z,
    float player_half_w, float player_height, float player_half_d
) {
    auto coll = shared::voxel::get_collision_info(block_type);
    if (!coll.hasCollision) return false;
    
    return check_aabb_collision(coll, bx, by, bz, player_x, player_y, player_z,
                                player_half_w, player_height, player_half_d);
}

static bool check_block_collision_3d_with_state(
    shared::voxel::BlockType block_type,
    shared::voxel::BlockRuntimeState state,
    int bx, int by, int bz,
    float player_x, float player_y, float player_z,
    float player_half_w, float player_height, float player_half_d
) {
    shared::voxel::BlockCollisionInfo boxes[5];
    int count = shared::voxel::get_collision_boxes(block_type, state, boxes, 5);
    
    for (int i = 0; i < count; ++i) {
        if (check_aabb_collision(boxes[i], bx, by, bz, player_x, player_y, player_z,
                                 player_half_w, player_height, player_half_d)) {
            return true;
        }
    }
    return false;
}

// Check if player AABB at given Y overlaps with block's collision shape (Y-only check for full blocks)
static bool check_block_collision_y(shared::voxel::BlockType block_type, int by, float player_y, float player_height) {
    auto coll = shared::voxel::get_collision_info(block_type);
    if (!coll.hasCollision) return false;
    
    // For non-full XZ blocks (like fences), this simple Y check doesn't apply
    if (!shared::voxel::is_full_collision_block(block_type)) {
        return false; // Handled by 3D check
    }
    
    // Block collision bounds in world coordinates
    float block_min_y = static_cast<float>(by) + coll.minY;
    float block_max_y = static_cast<float>(by) + std::min(coll.maxY, 1.0f);
    
    // Player bounds
    float player_min_y = player_y;
    float player_max_y = player_y + player_height;
    
    // Check AABB overlap in Y
    return player_min_y < block_max_y && player_max_y > block_min_y;
}

// Get the effective ground height at a block position (top of collision shape)
static float get_block_ground_height(shared::voxel::BlockType block_type, int by) {
    auto coll = shared::voxel::get_collision_info(block_type);
    if (!coll.hasCollision) return static_cast<float>(by);
    return static_cast<float>(by) + std::min(coll.maxY, 1.0f);
}

// Find the maximum obstacle height in front of the player at feet level.
// Returns the height above player's feet (0 = no obstacle, 0.5 = half slab, 1.0+ = full block or fence).
static float get_obstacle_step_height(
    const server::voxel::Terrain& terrain,
    float px, float py, float pz,
    float half_w, float half_d
) {
    int feet_y = fast_floor(py);
    float max_step_height = 0.0f;
    
    // Check blocks at feet level in the player's footprint
    for (int bx = fast_floor(px - half_w + kEps); bx <= fast_floor(px + half_w - kEps); bx++) {
        for (int bz = fast_floor(pz - half_d + kEps); bz <= fast_floor(pz + half_d - kEps); bz++) {
            auto block_type = terrain.get_block(bx, feet_y, bz);
            auto coll = shared::voxel::get_collision_info(block_type);
            if (!coll.hasCollision) continue;
            
            // Check if player overlaps with this block in XZ
            float block_min_x = static_cast<float>(bx) + coll.minX;
            float block_max_x = static_cast<float>(bx) + coll.maxX;
            float block_min_z = static_cast<float>(bz) + coll.minZ;
            float block_max_z = static_cast<float>(bz) + coll.maxZ;
            
            if (px - half_w < block_max_x && px + half_w > block_min_x &&
                pz - half_d < block_max_z && pz + half_d > block_min_z) {
                float ground_height = get_block_ground_height(block_type, feet_y);
                float step_height = ground_height - py;
                if (step_height > max_step_height && step_height > 0.0f) {
                    max_step_height = step_height;
                }
            }
        }
    }
    
    return max_step_height;
}

// Try to step up onto blocks when moving horizontally.
// Returns true if step-up was applied.
static bool try_step_up(
    const server::voxel::Terrain& terrain,
    float px, float& py, float pz,
    float half_w, float height, float half_d,
    std::uint64_t serverTick
) {
    float step_height = get_obstacle_step_height(terrain, px, py, pz, half_w, half_d);
    
    // Only step up if obstacle is low enough
    if (step_height <= 0.0f || step_height > kMaxStepUpHeight) {
        return false;
    }
    
    // Check if there's headroom after stepping up
    float new_y = py + step_height + kSkin;
    int head_y = fast_floor(new_y + height - kEps);
    
    for (int bx = fast_floor(px - half_w + kEps); bx <= fast_floor(px + half_w - kEps); bx++) {
        for (int bz = fast_floor(pz - half_d + kEps); bz <= fast_floor(pz + half_d - kEps); bz++) {
            auto block_type = terrain.get_block(bx, head_y, bz);
            auto coll = shared::voxel::get_collision_info(block_type);
            if (!coll.hasCollision) continue;
            
            // Check if would collide after step-up
            if (check_block_collision_3d(block_type, bx, head_y, bz, px, new_y, pz, half_w, height, half_d)) {
                return false; // No headroom
            }
        }
    }
    
    // Apply step-up
    py = new_y;
    logf(serverTick, "coll", "step-up height=%.3f new_y=%.3f", step_height, py);
    return true;
}

// Collision helpers (same logic as server.cpp, updated for non-full blocks)
static void resolve_x(const server::voxel::Terrain& terrain, float& px, float py, float pz, 
                      float& vx, float dx) {
    if (dx == 0.0f) return;
    const float hw = kPlayerWidth * 0.5f;
    // Check from one block below (for tall collision like fences with 1.5 height)
    int minY = fast_floor(py + kEps) - 1;
    if (minY < 0) minY = 0;
    int maxY = fast_floor(py + kPlayerHeight - kEps);
    int minZ = fast_floor(pz - hw + kEps);
    int maxZ = fast_floor(pz + hw - kEps);
    
    if (dx > 0.0f) {
        int checkX = fast_floor((px + hw) - kEps);
        for (int by = minY; by <= maxY; by++) {
            for (int bz = minZ; bz <= maxZ; bz++) {
                auto block_type = terrain.get_block(checkX, by, bz);
                auto block_state = terrain.get_block_state(checkX, by, bz);
                if (check_block_collision_3d_with_state(block_type, block_state, checkX, by, bz, px, py, pz, hw, kPlayerHeight, hw)) {
                    shared::voxel::BlockCollisionInfo boxes[5];
                    int count = shared::voxel::get_collision_boxes(block_type, block_state, boxes, 5);
                    float block_edge = static_cast<float>(checkX) + 1.0f; // Default to full block
                    for (int i = 0; i < count; ++i) {
                        if (check_aabb_collision(boxes[i], checkX, by, bz, px, py, pz, hw, kPlayerHeight, hw)) {
                            float edge = static_cast<float>(checkX) + boxes[i].minX;
                            if (edge < block_edge) block_edge = edge;
                        }
                    }
                    px = block_edge - hw - kSkin;
                    vx = 0.0f;
                    return;
                }
            }
        }
    } else {
        int checkX = fast_floor((px - hw) + kEps);
        for (int by = minY; by <= maxY; by++) {
            for (int bz = minZ; bz <= maxZ; bz++) {
                auto block_type = terrain.get_block(checkX, by, bz);
                auto block_state = terrain.get_block_state(checkX, by, bz);
                if (check_block_collision_3d_with_state(block_type, block_state, checkX, by, bz, px, py, pz, hw, kPlayerHeight, hw)) {
                    shared::voxel::BlockCollisionInfo boxes[5];
                    int count = shared::voxel::get_collision_boxes(block_type, block_state, boxes, 5);
                    float block_edge = static_cast<float>(checkX);
                    for (int i = 0; i < count; ++i) {
                        if (check_aabb_collision(boxes[i], checkX, by, bz, px, py, pz, hw, kPlayerHeight, hw)) {
                            float edge = static_cast<float>(checkX) + boxes[i].maxX;
                            if (edge > block_edge) block_edge = edge;
                        }
                    }
                    px = block_edge + hw + kSkin;
                    vx = 0.0f;
                    return;
                }
            }
        }
    }
}

static void resolve_z(const server::voxel::Terrain& terrain, float px, float py, float& pz,
                      float& vz, float dz) {
    if (dz == 0.0f) return;
    const float hw = kPlayerWidth * 0.5f;
    // Check from one block below (for tall collision like fences with 1.5 height)
    int minY = fast_floor(py + kEps) - 1;
    if (minY < 0) minY = 0;
    int maxY = fast_floor(py + kPlayerHeight - kEps);
    int minX = fast_floor(px - hw + kEps);
    int maxX = fast_floor(px + hw - kEps);
    
    if (dz > 0.0f) {
        int checkZ = fast_floor((pz + hw) - kEps);
        for (int by = minY; by <= maxY; by++) {
            for (int bx = minX; bx <= maxX; bx++) {
                auto block_type = terrain.get_block(bx, by, checkZ);
                auto block_state = terrain.get_block_state(bx, by, checkZ);
                if (check_block_collision_3d_with_state(block_type, block_state, bx, by, checkZ, px, py, pz, hw, kPlayerHeight, hw)) {
                    shared::voxel::BlockCollisionInfo boxes[5];
                    int count = shared::voxel::get_collision_boxes(block_type, block_state, boxes, 5);
                    float block_edge = static_cast<float>(checkZ) + 1.0f;
                    for (int i = 0; i < count; ++i) {
                        if (check_aabb_collision(boxes[i], bx, by, checkZ, px, py, pz, hw, kPlayerHeight, hw)) {
                            float edge = static_cast<float>(checkZ) + boxes[i].minZ;
                            if (edge < block_edge) block_edge = edge;
                        }
                    }
                    pz = block_edge - hw - kSkin;
                    vz = 0.0f;
                    return;
                }
            }
        }
    } else {
        int checkZ = fast_floor((pz - hw) + kEps);
        for (int by = minY; by <= maxY; by++) {
            for (int bx = minX; bx <= maxX; bx++) {
                auto block_type = terrain.get_block(bx, by, checkZ);
                auto block_state = terrain.get_block_state(bx, by, checkZ);
                if (check_block_collision_3d_with_state(block_type, block_state, bx, by, checkZ, px, py, pz, hw, kPlayerHeight, hw)) {
                    shared::voxel::BlockCollisionInfo boxes[5];
                    int count = shared::voxel::get_collision_boxes(block_type, block_state, boxes, 5);
                    float block_edge = static_cast<float>(checkZ);
                    for (int i = 0; i < count; ++i) {
                        if (check_aabb_collision(boxes[i], bx, by, checkZ, px, py, pz, hw, kPlayerHeight, hw)) {
                            float edge = static_cast<float>(checkZ) + boxes[i].maxZ;
                            if (edge > block_edge) block_edge = edge;
                        }
                    }
                    pz = block_edge + hw + kSkin;
                    vz = 0.0f;
                    return;
                }
            }
        }
    }
}

static void resolve_y(const server::voxel::Terrain& terrain, float px, float& py, float pz,
                      float& vy, float dy, bool& onGround) {
    const float hw = kPlayerWidth * 0.5f;
    
    if (dy <= 0.0f) {
        // Check both the block at player's feet and one below
        for (int checkY = fast_floor(py - kEps); checkY >= fast_floor(py - 1.0f); checkY--) {
            for (int bx = fast_floor(px - hw + kEps); bx <= fast_floor(px + hw - kEps); bx++) {
                for (int bz = fast_floor(pz - hw + kEps); bz <= fast_floor(pz + hw - kEps); bz++) {
                    auto block_type = terrain.get_block(bx, checkY, bz);
                    auto block_state = terrain.get_block_state(bx, checkY, bz);
                    
                    shared::voxel::BlockCollisionInfo boxes[5];
                    int count = shared::voxel::get_collision_boxes(block_type, block_state, boxes, 5);
                    
                    for (int i = 0; i < count; ++i) {
                        const auto& coll = boxes[i];
                        if (!coll.hasCollision) continue;
                        
                        float block_min_x = static_cast<float>(bx) + coll.minX;
                        float block_max_x = static_cast<float>(bx) + coll.maxX;
                        float block_min_z = static_cast<float>(bz) + coll.minZ;
                        float block_max_z = static_cast<float>(bz) + coll.maxZ;
                        
                        if (!(px - hw < block_max_x && px + hw > block_min_x &&
                              pz - hw < block_max_z && pz + hw > block_min_z)) {
                            continue; // No XZ overlap
                        }
                        
                        float ground_height = static_cast<float>(checkY) + std::min(coll.maxY, 1.0f);
                        
                        if (py <= ground_height + kEps && py > ground_height - 0.5f) {
                            py = ground_height;
                            if (vy < 0.0f) vy = 0.0f;
                            onGround = true;
                            return;
                        }
                    }
                }
            }
        }
    }
    
    if (dy > 0.0f) {
        int checkY = fast_floor((py + kPlayerHeight) - kEps);
        for (int bx = fast_floor(px - hw + kEps); bx <= fast_floor(px + hw - kEps); bx++) {
            for (int bz = fast_floor(pz - hw + kEps); bz <= fast_floor(pz + hw - kEps); bz++) {
                auto block_type = terrain.get_block(bx, checkY, bz);
                auto block_state = terrain.get_block_state(bx, checkY, bz);
                
                shared::voxel::BlockCollisionInfo boxes[5];
                int count = shared::voxel::get_collision_boxes(block_type, block_state, boxes, 5);
                
                for (int i = 0; i < count; ++i) {
                    const auto& coll = boxes[i];
                    if (!coll.hasCollision) continue;
                    
                    float block_min_x = static_cast<float>(bx) + coll.minX;
                    float block_max_x = static_cast<float>(bx) + coll.maxX;
                    float block_min_z = static_cast<float>(bz) + coll.minZ;
                    float block_max_z = static_cast<float>(bz) + coll.maxZ;
                    
                    if (!(px - hw < block_max_x && px + hw > block_min_x &&
                          pz - hw < block_max_z && pz + hw > block_min_z)) {
                        continue; // No XZ overlap
                    }
                    
                    float block_bottom = static_cast<float>(checkY) + coll.minY;
                    
                    if (py + kPlayerHeight > block_bottom) {
                        py = block_bottom - kPlayerHeight;
                        if (vy > 0.0f) vy = 0.0f;
                        return;
                    }
                }
            }
        }
    }
}

static bool load_latest_rfmap(shared::maps::MapTemplate* outMap, std::filesystem::path* outPath) {
    if (!outMap) return false;
    const auto mapsDir = shared::maps::runtime_maps_dir();
    std::error_code ec;
    if (!std::filesystem::exists(mapsDir, ec)) return false;

    std::filesystem::path bestPath;
    std::filesystem::file_time_type bestTime{};
    bool haveBest = false;

    for (const auto& entry : std::filesystem::directory_iterator(mapsDir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        if (entry.path().extension() != ".rfmap") continue;
        auto t = entry.last_write_time(ec);
        if (ec) continue;
        if (!haveBest || t > bestTime) {
            bestTime = t;
            bestPath = entry.path();
            haveBest = true;
        }
    }

    if (!haveBest) return false;

    shared::maps::MapTemplate map;
    std::string err;
    if (!shared::maps::read_rfmap(bestPath, &map, &err)) return false;
    if (map.mapId.empty() || map.version == 0) return false;

    *outMap = std::move(map);
    if (outPath) *outPath = bestPath;
    return true;
}

// Raycast from eye position to target block.
// Returns true if the target block is reachable (no solid blocks in the way).
static bool raycast_hit_block(const server::voxel::Terrain& terrain,
                              float eyeX, float eyeY, float eyeZ,
                              int targetX, int targetY, int targetZ,
                              float maxDist) {
    // Target block center
    const float tx = targetX + 0.5f;
    const float ty = targetY + 0.5f;
    const float tz = targetZ + 0.5f;

    // Direction
    float dx = tx - eyeX;
    float dy = ty - eyeY;
    float dz = tz - eyeZ;
    const float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    
    if (dist < kEps) return true;  // Already at target
    if (dist > maxDist) return false;  // Too far

    // Normalize
    dx /= dist;
    dy /= dist;
    dz /= dist;

    // Step through blocks using DDA-like algorithm
    const float stepSize = 0.25f;  // Step in world units
    float t = 0.0f;

    while (t < dist) {
        const float px = eyeX + dx * t;
        const float py = eyeY + dy * t;
        const float pz = eyeZ + dz * t;

        const int bx = fast_floor(px);
        const int by = fast_floor(py);
        const int bz = fast_floor(pz);

        // If we reached the target block, success
        if (bx == targetX && by == targetY && bz == targetZ) {
            return true;
        }

        // If we hit a solid block that's not our target, fail
        if (shared::voxel::util::is_solid(terrain.get_block(bx, by, bz))) {
            return false;
        }

        t += stepSize;
    }

    // Check final position (the target itself)
    return true;
}

} // namespace

// =============================================================================
// DedicatedServer implementation
// =============================================================================

DedicatedServer::DedicatedServer(const Config& config)
    : config_(config), tickRate_(config.tickRate) {
    
    g_log.enabled = config.logging.enabled;
    g_log.init = config.logging.init;
    g_log.rx = config.logging.rx;
    g_log.tx = config.logging.tx;
    g_log.move = config.logging.move;
    g_log.coll = config.logging.coll;

    worldSeed_ = static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
    terrain_ = std::make_unique<server::voxel::Terrain>(worldSeed_);

    // Try to load map template
    shared::maps::MapTemplate map;
    std::filesystem::path path;
    if (load_latest_rfmap(&map, &path)) {
        hasMapTemplate_ = true;
        mapId_ = map.mapId;
        mapVersion_ = map.version;

        if (map.has_scripts()) {
            init_script_engine_();
            if (scriptEngine_) {
                auto result = scriptEngine_->load_map_scripts(map.scriptData);
                if (result) {
                    logf(0, "init", "loaded map scripts");
                }
            }
        }

        terrain_->set_map_template(std::move(map));
        logf(0, "init", "loaded map: %s (id=%s v%u)", 
             path.filename().string().c_str(), mapId_.c_str(), mapVersion_);
    }

    logf(0, "init", "tickRate=%u worldSeed=%u maxClients=%zu",
         tickRate_, worldSeed_, config_.maxClients);
}

DedicatedServer::~DedicatedServer() {
    stop();
}

bool DedicatedServer::start() {
    if (running_.load()) return false;

    // Setup connection callbacks
    netServer_.onConnect = [this](auto conn) {
        handle_client_connect_(conn);
    };
    netServer_.onDisconnect = [this](auto conn) {
        handle_client_disconnect_(conn);
    };

    if (!netServer_.start(config_.port, config_.maxClients)) {
        logf(0, "init", "ERROR: failed to bind port %u", config_.port);
        return false;
    }

    logf(0, "init", "listening on port %u", config_.port);
    running_.store(true);
    thread_ = std::thread([this]() { run_loop_(); });
    return true;
}

void DedicatedServer::stop() {
    if (!running_.exchange(false)) return;

    if (thread_.joinable()) {
        thread_.join();
    }

    netServer_.stop();
    clients_.clear();
    connToPlayer_.clear();
    logf(serverTick_, "init", "server stopped");
}

std::size_t DedicatedServer::client_count() const {
    std::size_t count = 0;
    for (const auto& [id, client] : clients_) {
        if (client.phase == ClientState::Phase::InGame) {
            count++;
        }
    }
    return count;
}

void DedicatedServer::run_loop_() {
    using clock = std::chrono::steady_clock;
    const auto tickDuration = std::chrono::duration<double>(1.0 / static_cast<double>(tickRate_));
    auto nextTick = clock::now();

    while (running_.load()) {
        nextTick += std::chrono::duration_cast<clock::duration>(tickDuration);
        tick_once_();
        std::this_thread::sleep_until(nextTick);
    }
}

void DedicatedServer::tick_once_() {
    serverTick_++;

    // Poll network events (handles connect/disconnect via callbacks)
    netServer_.poll(0);

    // Process messages from all clients
    for (auto& [playerId, client] : clients_) {
        process_client_messages_(client);
    }

    // Simulate physics for all in-game clients
    const float dt = 1.0f / static_cast<float>(tickRate_);
    for (auto& [playerId, client] : clients_) {
        if (client.phase == ClientState::Phase::InGame) {
            simulate_client_(client, dt);

            // Send snapshot to this client
            shared::proto::StateSnapshot snap;
            snap.serverTick = serverTick_;
            snap.playerId = client.playerId;
            snap.px = client.px;
            snap.py = client.py;
            snap.pz = client.pz;
            snap.vx = client.vx;
            snap.vy = client.vy;
            snap.vz = client.vz;
            client.connection->send(snap);
        }
    }

    // Process script commands
    if (scriptEngine_ && scriptEngine_->has_scripts()) {
        scriptEngine_->update(dt);
        process_script_commands_();
    }
}

void DedicatedServer::handle_client_connect_(std::shared_ptr<shared::transport::ENetConnection> conn) {
    auto playerId = nextPlayerId_++;

    ClientState client;
    client.connection = conn;
    client.playerId = playerId;
    client.phase = ClientState::Phase::Connected;

    clients_[playerId] = std::move(client);
    connToPlayer_[conn.get()] = playerId;

    logf(serverTick_, "init", "client connected (pending hello), assigned playerId=%u, ping=%ums",
         playerId, conn->ping_ms());
}

void DedicatedServer::handle_client_disconnect_(std::shared_ptr<shared::transport::ENetConnection> conn) {
    auto it = connToPlayer_.find(conn.get());
    if (it == connToPlayer_.end()) return;

    auto playerId = it->second;
    connToPlayer_.erase(it);

    auto clientIt = clients_.find(playerId);
    if (clientIt != clients_.end()) {
        bool wasInGame = (clientIt->second.phase == ClientState::Phase::InGame);
        clients_.erase(clientIt);

        logf(serverTick_, "init", "client disconnected playerId=%u", playerId);

        if (wasInGame && onPlayerLeave) {
            onPlayerLeave(playerId);
        }
    }
}

void DedicatedServer::process_client_messages_(ClientState& client) {
    if (!client.connection) return;

    shared::proto::Message msg;
    while (client.connection->try_recv(msg)) {
        handle_message_(client, msg);
    }
}

void DedicatedServer::handle_message_(ClientState& client, shared::proto::Message& msg) {
    // ClientHello
    if (std::holds_alternative<shared::proto::ClientHello>(msg)) {
        const auto& hello = std::get<shared::proto::ClientHello>(msg);
        logf(serverTick_, "rx", "ClientHello from player=%u version=%u name=%s",
             client.playerId, hello.version, hello.clientName.c_str());

        client.phase = ClientState::Phase::Handshaking;

        shared::proto::ServerHello resp;
        resp.acceptedVersion = hello.version;
        resp.tickRate = tickRate_;
        resp.worldSeed = worldSeed_;
        resp.hasMapTemplate = hasMapTemplate_;
        if (hasMapTemplate_) {
            resp.mapId = mapId_;
            resp.mapVersion = mapVersion_;
        }
        client.connection->send(resp);
        logf(serverTick_, "tx", "ServerHello to player=%u", client.playerId);
        return;
    }

    // JoinMatch
    if (std::holds_alternative<shared::proto::JoinMatch>(msg)) {
        logf(serverTick_, "rx", "JoinMatch from player=%u", client.playerId);

        if (client.phase != ClientState::Phase::Handshaking) {
            shared::proto::ActionRejected rej;
            rej.seq = 0;
            rej.reason = shared::proto::RejectReason::NotAllowed;
            client.connection->send(rej);
            return;
        }

        client.phase = ClientState::Phase::InGame;

        if (scriptEngine_ && scriptEngine_->has_scripts()) {
            scriptEngine_->on_player_join(client.playerId);
        }

        shared::proto::JoinAck ack;
        ack.playerId = client.playerId;
        client.connection->send(ack);
        logf(serverTick_, "tx", "JoinAck playerId=%u", client.playerId);

        // Send chunk data for the initial view radius around spawn (0,0)
        // For now send a fixed radius of chunks. In the future this should be dynamic
        // based on player position and view distance settings.
        constexpr int INITIAL_CHUNK_RADIUS = 4;  // Send 9x9 = 81 chunks initially
        
        int chunksSent = 0;
        for (int cz = -INITIAL_CHUNK_RADIUS; cz <= INITIAL_CHUNK_RADIUS; ++cz) {
            for (int cx = -INITIAL_CHUNK_RADIUS; cx <= INITIAL_CHUNK_RADIUS; ++cx) {
                shared::proto::ChunkData chunkMsg;
                chunkMsg.chunkX = cx;
                chunkMsg.chunkZ = cz;
                chunkMsg.blocks = terrain_->get_chunk_data(cx, cz);
                client.connection->send(chunkMsg);
                chunksSent++;
            }
        }
        logf(serverTick_, "tx", "sent %d chunks to player=%u", chunksSent, client.playerId);

        if (onPlayerJoin) {
            onPlayerJoin(client.playerId);
        }
        return;
    }

    // InputFrame
    if (std::holds_alternative<shared::proto::InputFrame>(msg)) {
        if (client.phase != ClientState::Phase::InGame) return;
        client.lastInput = std::get<shared::proto::InputFrame>(msg);
        return;
    }

    // TryBreakBlock
    if (std::holds_alternative<shared::proto::TryBreakBlock>(msg)) {
        if (client.phase != ClientState::Phase::InGame) return;
        const auto& req = std::get<shared::proto::TryBreakBlock>(msg);

        // Validate range
        if (req.y < 0 || req.y >= shared::voxel::CHUNK_HEIGHT) {
            shared::proto::ActionRejected rej{req.seq, shared::proto::RejectReason::Invalid};
            client.connection->send(rej);
            return;
        }

        // Check reach distance
        const float eyeX = client.px;
        const float eyeY = client.py + shared::kPlayerEyeHeight;
        const float eyeZ = client.pz;
        const float cx = req.x + 0.5f, cy = req.y + 0.5f, cz = req.z + 0.5f;
        const float dx = cx - eyeX;
        const float dy = cy - eyeY;
        const float dz = cz - eyeZ;
        if (dx*dx + dy*dy + dz*dz > shared::kBlockReachDistance * shared::kBlockReachDistance) {
            shared::proto::ActionRejected rej{req.seq, shared::proto::RejectReason::OutOfRange};
            client.connection->send(rej);
            return;
        }

        // Raycast line-of-sight check
        if (!raycast_hit_block(*terrain_, eyeX, eyeY, eyeZ, req.x, req.y, req.z, shared::kBlockReachDistance)) {
            shared::proto::ActionRejected rej{req.seq, shared::proto::RejectReason::NoLineOfSight};
            client.connection->send(rej);
            return;
        }

        auto cur = terrain_->get_block(req.x, req.y, req.z);
        if (cur == shared::voxel::BlockType::Air) {
            shared::proto::ActionRejected rej{req.seq, shared::proto::RejectReason::Invalid};
            client.connection->send(rej);
            return;
        }

        // Check if block is protected (template blocks, bedrock, etc.)
        if (!terrain_->can_player_break(req.x, req.y, req.z, cur)) {
            shared::proto::ActionRejected rej{req.seq, shared::proto::RejectReason::ProtectedBlock};
            client.connection->send(rej);
            return;
        }

        terrain_->break_player_block(req.x, req.y, req.z);
        terrain_->set_block_state(req.x, req.y, req.z, shared::voxel::BlockRuntimeState::defaults());

        shared::proto::BlockBroken broken;
        broken.x = req.x;
        broken.y = req.y;
        broken.z = req.z;
        broadcast_(broken);
        
        // Update neighbor connections (fences that were connected to this block)
        auto neighborUpdates = terrain_->update_neighbor_states(req.x, req.y, req.z);
        for (const auto& update : neighborUpdates) {
            shared::proto::BlockPlaced neighborPlaced;
            neighborPlaced.x = update.x;
            neighborPlaced.y = update.y;
            neighborPlaced.z = update.z;
            neighborPlaced.blockType = update.type;
            neighborPlaced.stateByte = update.state.to_byte();
            broadcast_(neighborPlaced);
        }
        return;
    }

    // TryPlaceBlock
    if (std::holds_alternative<shared::proto::TryPlaceBlock>(msg)) {
        if (client.phase != ClientState::Phase::InGame) return;
        const auto& req = std::get<shared::proto::TryPlaceBlock>(msg);

        if (req.y < 0 || req.y >= shared::voxel::CHUNK_HEIGHT) {
            shared::proto::ActionRejected rej{req.seq, shared::proto::RejectReason::Invalid};
            client.connection->send(rej);
            return;
        }

        const float eyeX = client.px;
        const float eyeY = client.py + shared::kPlayerEyeHeight;
        const float eyeZ = client.pz;
        const float cx = req.x + 0.5f, cy = req.y + 0.5f, cz = req.z + 0.5f;
        const float dx = cx - eyeX;
        const float dy = cy - eyeY;
        const float dz = cz - eyeZ;
        if (dx*dx + dy*dy + dz*dz > shared::kBlockReachDistance * shared::kBlockReachDistance) {
            shared::proto::ActionRejected rej{req.seq, shared::proto::RejectReason::OutOfRange};
            client.connection->send(rej);
            return;
        }

        // Raycast line-of-sight check (must be able to see the position to place)
        if (!raycast_hit_block(*terrain_, eyeX, eyeY, eyeZ, req.x, req.y, req.z, shared::kBlockReachDistance)) {
            shared::proto::ActionRejected rej{req.seq, shared::proto::RejectReason::NoLineOfSight};
            client.connection->send(rej);
            return;
        }

        auto cur = terrain_->get_block(req.x, req.y, req.z);
        auto curState = terrain_->get_block_state(req.x, req.y, req.z);
        
        // Handle slab merging: placing slab on same-category slab
        using namespace shared::voxel;
        if (is_slab(req.blockType) && is_slab(cur)) {
            SlabCategory placingCat = get_slab_category(get_base_slab_type(req.blockType));
            SlabCategory existingCat = get_slab_category(get_base_slab_type(cur));
            
            if (placingCat == existingCat && curState.slabType != SlabType::Double) {
                // Determine where the new slab would go
                SlabType newSlabType = determine_slab_type_from_hit(req.hitY, req.face);
                
                // Can merge if existing is bottom and placing top, or vice versa
                if ((curState.slabType == SlabType::Bottom && newSlabType == SlabType::Top) ||
                    (curState.slabType == SlabType::Top && newSlabType == SlabType::Bottom)) {
                    // Merge into full block
                    BlockType fullBlock = get_double_slab_type(placingCat);
                    terrain_->set_block(req.x, req.y, req.z, fullBlock);
                    terrain_->set_block_state(req.x, req.y, req.z, BlockRuntimeState::defaults());
                    
                    shared::proto::BlockPlaced placed;
                    placed.x = req.x;
                    placed.y = req.y;
                    placed.z = req.z;
                    placed.blockType = fullBlock;
                    placed.stateByte = 0;
                    broadcast_(placed);
                    return;
                }
            }
        }
        
        // Normal placement: position must be air
        if (cur != shared::voxel::BlockType::Air) {
            shared::proto::ActionRejected rej{req.seq, shared::proto::RejectReason::Invalid};
            client.connection->send(rej);
            return;
        }

        // Check collision with all players
        const float blockMinX = static_cast<float>(req.x);
        const float blockMaxX = static_cast<float>(req.x + 1);
        const float blockMinY = static_cast<float>(req.y);
        const float blockMaxY = static_cast<float>(req.y + 1);
        const float blockMinZ = static_cast<float>(req.z);
        const float blockMaxZ = static_cast<float>(req.z + 1);

        bool collides = false;
        for (const auto& [playerId, other] : clients_) {
            if (other.phase != ClientState::Phase::InGame) continue;

            const float hw = kPlayerWidth * 0.5f;
            const float playerMinX = other.px - hw;
            const float playerMaxX = other.px + hw;
            const float playerMinY = other.py;
            const float playerMaxY = other.py + kPlayerHeight;
            const float playerMinZ = other.pz - hw;
            const float playerMaxZ = other.pz + hw;

            // AABB intersection test
            if (blockMinX < playerMaxX && blockMaxX > playerMinX &&
                blockMinY < playerMaxY && blockMaxY > playerMinY &&
                blockMinZ < playerMaxZ && blockMaxZ > playerMinZ) {
                collides = true;
                break;
            }
        }

        if (collides) {
            shared::proto::ActionRejected rej{req.seq, shared::proto::RejectReason::Collision};
            client.connection->send(rej);
            return;
        }

        // Normalize slab types (StoneSlabTop -> StoneSlab)
        BlockType finalBlockType = get_base_slab_type(req.blockType);
        terrain_->place_player_block(req.x, req.y, req.z, finalBlockType);
        
        // Compute block state (connections for fences, slab type for slabs)
        auto state = terrain_->compute_block_state(req.x, req.y, req.z, finalBlockType);
        
        // For slabs: override state with placement-based top/bottom
        if (is_slab(finalBlockType)) {
            state.slabType = determine_slab_type_from_hit(req.hitY, req.face);
        }
        
        terrain_->set_block_state(req.x, req.y, req.z, state);

        shared::proto::BlockPlaced placed;
        placed.x = req.x;
        placed.y = req.y;
        placed.z = req.z;
        placed.blockType = finalBlockType;
        placed.stateByte = state.to_byte();
        broadcast_(placed);
        
        // Update neighbor connections (fences connecting to this block)
        auto neighborUpdates = terrain_->update_neighbor_states(req.x, req.y, req.z);
        for (const auto& update : neighborUpdates) {
            shared::proto::BlockPlaced neighborPlaced;
            neighborPlaced.x = update.x;
            neighborPlaced.y = update.y;
            neighborPlaced.z = update.z;
            neighborPlaced.blockType = update.type;
            neighborPlaced.stateByte = update.state.to_byte();
            broadcast_(neighborPlaced);
        }
        return;
    }
}

void DedicatedServer::simulate_client_(ClientState& client, float dt) {
    const auto& input = client.lastInput;
    const float speed = input.sprint ? 8.0f : 5.0f;

    const float yawRad = input.yaw * kDegToRad;
    const float forwardX = std::sin(yawRad);
    const float forwardZ = std::cos(yawRad);
    const float rightX = std::cos(yawRad);
    const float rightZ = -std::sin(yawRad);

    const float moveX = input.moveX * speed;
    const float moveZ = input.moveY * speed;

    client.vx = rightX * moveX + forwardX * moveZ;
    client.vz = rightZ * moveX + forwardZ * moveZ;

    // Jump
    const bool jumpPressed = input.jump && !client.lastJumpHeld;
    client.lastJumpHeld = input.jump;

    if (client.onGround && jumpPressed) {
        client.vy = kJumpVelocity;
        client.onGround = false;
    }

    // Gravity
    if (!client.onGround) {
        client.vy -= kGravity * dt;
    } else if (client.vy < 0.0f) {
        client.vy = 0.0f;
    }

    // Integrate and collide with step-up support
    const float half_w = kPlayerWidth * 0.5f;
    
    float dx = client.vx * dt;
    if (dx != 0.0f) {
        float old_px = client.px;
        client.px += dx;
        resolve_x(*terrain_, client.px, client.py, client.pz, client.vx, dx);
        
        // If we got blocked and we're on ground, try step-up
        if (client.onGround && client.px == old_px && client.vx == 0.0f) {
            // Temporarily move to where we wanted to go
            client.px = old_px + dx;
            float step_height = get_obstacle_step_height(*terrain_, client.px, client.py, client.pz, half_w, half_w);
            if (step_height > 0.0f && step_height <= kMaxStepUpHeight) {
                // Check headroom
                float new_y = client.py + step_height + kSkin;
                int head_y = fast_floor(new_y + kPlayerHeight - kEps);
                bool has_headroom = true;
                for (int bx = fast_floor(client.px - half_w + kEps); bx <= fast_floor(client.px + half_w - kEps) && has_headroom; bx++) {
                    for (int bz = fast_floor(client.pz - half_w + kEps); bz <= fast_floor(client.pz + half_w - kEps) && has_headroom; bz++) {
                        auto block_type = terrain_->get_block(bx, head_y, bz);
                        if (check_block_collision_3d(block_type, bx, head_y, bz, client.px, new_y, client.pz, half_w, kPlayerHeight, half_w)) {
                            has_headroom = false;
                        }
                    }
                }
                if (has_headroom) {
                    client.py = new_y;
                    logf(serverTick_, "coll", "step-up X height=%.3f new_y=%.3f", step_height, client.py);
                } else {
                    client.px = old_px; // Revert
                    client.vx = 0.0f;
                }
            } else {
                client.px = old_px; // Revert
                client.vx = 0.0f;
            }
        }
    }

    float dz = client.vz * dt;
    if (dz != 0.0f) {
        float old_pz = client.pz;
        client.pz += dz;
        resolve_z(*terrain_, client.px, client.py, client.pz, client.vz, dz);
        
        // If we got blocked and we're on ground, try step-up
        if (client.onGround && client.pz == old_pz && client.vz == 0.0f) {
            // Temporarily move to where we wanted to go
            client.pz = old_pz + dz;
            float step_height = get_obstacle_step_height(*terrain_, client.px, client.py, client.pz, half_w, half_w);
            if (step_height > 0.0f && step_height <= kMaxStepUpHeight) {
                // Check headroom
                float new_y = client.py + step_height + kSkin;
                int head_y = fast_floor(new_y + kPlayerHeight - kEps);
                bool has_headroom = true;
                for (int bx = fast_floor(client.px - half_w + kEps); bx <= fast_floor(client.px + half_w - kEps) && has_headroom; bx++) {
                    for (int bz = fast_floor(client.pz - half_w + kEps); bz <= fast_floor(client.pz + half_w - kEps) && has_headroom; bz++) {
                        auto block_type = terrain_->get_block(bx, head_y, bz);
                        if (check_block_collision_3d(block_type, bx, head_y, bz, client.px, new_y, client.pz, half_w, kPlayerHeight, half_w)) {
                            has_headroom = false;
                        }
                    }
                }
                if (has_headroom) {
                    client.py = new_y;
                    logf(serverTick_, "coll", "step-up Z height=%.3f new_y=%.3f", step_height, client.py);
                } else {
                    client.pz = old_pz; // Revert
                    client.vz = 0.0f;
                }
            } else {
                client.pz = old_pz; // Revert
                client.vz = 0.0f;
            }
        }
    }

    float dy = client.vy * dt;
    client.py += dy;
    client.onGround = false;
    resolve_y(*terrain_, client.px, client.py, client.pz, client.vy, dy, client.onGround);
}

void DedicatedServer::broadcast_(const shared::proto::Message& msg) {
    for (auto& [playerId, client] : clients_) {
        if (client.phase == ClientState::Phase::InGame && client.connection) {
            client.connection->send(msg);
        }
    }
}

void DedicatedServer::broadcast_except_(const shared::proto::Message& msg, shared::proto::PlayerId except) {
    for (auto& [playerId, client] : clients_) {
        if (playerId != except && client.phase == ClientState::Phase::InGame && client.connection) {
            client.connection->send(msg);
        }
    }
}

void DedicatedServer::init_script_engine_() {
    scriptEngine_ = std::make_unique<server::scripting::ScriptEngine>();
}

void DedicatedServer::process_script_commands_() {
    // TODO: process script commands and broadcast to clients
}

} // namespace server::core
