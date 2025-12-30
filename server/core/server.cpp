#include "server.hpp"

#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <thread>

#include "../../shared/constants.hpp"
#include "../../shared/maps/rfmap_io.hpp"
#include "../../shared/maps/runtime_paths.hpp"
#include "../../shared/voxel/block_state.hpp"
#include "../scripting/script_engine.hpp"

namespace server::core {

namespace {

constexpr float kDegToRad = 0.017453292519943295f;

// Keep these aligned with the old client-side defaults for now.
constexpr float kPlayerWidth = shared::kPlayerWidth;
constexpr float kPlayerHeight = shared::kPlayerHeight;
constexpr float kGravity = 20.0f;
constexpr float kJumpVelocity = 8.0f;

constexpr float kEps = 1e-4f;
constexpr float kSkin = 1e-3f;

static bool is_valid_map_id(const std::string& mapId) {
    if (mapId.empty()) return false;
    if (mapId.size() > 64) return false;
    for (unsigned char c : mapId) {
        const bool ok = (std::isalnum(c) != 0) || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

static bool load_latest_rfmap(shared::maps::MapTemplate* outMap, std::filesystem::path* outPath) {
    if (!outMap) return false;

    const std::filesystem::path mapsDir = shared::maps::runtime_maps_dir();
    std::error_code ec;
    if (!std::filesystem::exists(mapsDir, ec)) {
        return false;
    }

    std::filesystem::path bestPath;
    std::filesystem::file_time_type bestTime{};
    bool haveBest = false;

    for (const auto& entry : std::filesystem::directory_iterator(mapsDir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        const auto& p = entry.path();
        if (p.extension() != ".rfmap") continue;

        const auto t = entry.last_write_time(ec);
        if (ec) continue;

        if (!haveBest || t > bestTime) {
            bestTime = t;
            bestPath = p;
            haveBest = true;
        }
    }

    if (!haveBest) return false;

    shared::maps::MapTemplate map;
    std::string err;
    if (!shared::maps::read_rfmap(bestPath, &map, &err)) {
        // Defer to filtered sv logger.
        std::fprintf(stderr, "[sv][init][map] failed to read %s: %s\n", bestPath.generic_string().c_str(), err.c_str());
        return false;
    }

    if (map.mapId.empty() || map.version == 0 || !is_valid_map_id(map.mapId)) {
        std::fprintf(stderr, "[sv][init][map] ignoring invalid map metadata in %s (mapId=%s version=%u)\n",
                     bestPath.generic_string().c_str(), map.mapId.c_str(), map.version);
        return false;
    }

    *outMap = std::move(map);
    if (outPath) *outPath = bestPath;
    return true;
}

static bool should_log_movement(std::uint64_t serverTick) {
    // Roughly 1 Hz at 30 TPS.
    return (serverTick % 30) == 0;
}

struct SvLogCfg {
    bool enabled{true};
    bool init{true};
    bool rx{true};
    bool tx{true};
    bool move{true};
    bool coll{true};
};

static SvLogCfg g_sv_log{};

static bool sv_tag_enabled(const char* tag) {
    if (!g_sv_log.enabled) return false;
    if (!tag) return false;

    // Tags used in this file:
    // init, rx, tx, move, coll
    if (std::strcmp(tag, "init") == 0) return g_sv_log.init;
    if (std::strcmp(tag, "rx") == 0) return g_sv_log.rx;
    if (std::strcmp(tag, "tx") == 0) return g_sv_log.tx;
    if (std::strcmp(tag, "move") == 0) return g_sv_log.move;
    if (std::strcmp(tag, "coll") == 0) return g_sv_log.coll;

    // Unknown tag: keep it if logging is enabled.
    return true;
}

static void logf(std::uint64_t serverTick, const char* tag, const char* fmt, ...) {
    if (!sv_tag_enabled(tag)) return;

    std::fprintf(stderr, "[sv][%llu][%s] ", static_cast<unsigned long long>(serverTick), tag);
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fprintf(stderr, "\n");
}

static inline int fast_floor_to_int(float v) {
    return static_cast<int>(std::floor(v));
}

// Maximum height that can be auto-stepped up without jumping (like Minecraft's 0.5 block step-up)
constexpr float kMaxStepUpHeight = 0.5f + kEps;

// Check full 3D AABB collision between player and block
static bool check_block_collision_3d(
    shared::voxel::BlockType block_type, 
    int bx, int by, int bz,
    float player_x, float player_y, float player_z,
    float player_half_w, float player_height, float player_half_d
) {
    auto coll = shared::voxel::get_collision_info(block_type);
    if (!coll.hasCollision) return false;
    
    // Block collision bounds in world coordinates
    // Note: Use full collision height (e.g., 1.5 for fences) to prevent jumping over
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

// Check if player AABB at given Y overlaps with block's collision shape (Y-only check for slabs)
static bool check_block_collision_y(shared::voxel::BlockType block_type, int by, float player_y, float player_height) {
    auto coll = shared::voxel::get_collision_info(block_type);
    if (!coll.hasCollision) return false;
    
    // For non-full XZ blocks (like fences), this simple Y check doesn't apply
    // We should use the full 3D check instead
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
    int feet_y = fast_floor_to_int(py);
    float max_step_height = 0.0f;
    
    // Check blocks at feet level in the player's footprint
    for (int bx = fast_floor_to_int(px - half_w + kEps); bx <= fast_floor_to_int(px + half_w - kEps); bx++) {
        for (int bz = fast_floor_to_int(pz - half_d + kEps); bz <= fast_floor_to_int(pz + half_d - kEps); bz++) {
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
    int head_y = fast_floor_to_int(new_y + height - kEps);
    
    for (int bx = fast_floor_to_int(px - half_w + kEps); bx <= fast_floor_to_int(px + half_w - kEps); bx++) {
        for (int bz = fast_floor_to_int(pz - half_d + kEps); bz <= fast_floor_to_int(pz + half_d - kEps); bz++) {
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

static void resolve_voxel_x(const server::voxel::Terrain& terrain, float& px, float py, float pz, float& vx, float dx, std::uint64_t serverTick) {
    if (dx == 0.0f) return;

    const float half_w = kPlayerWidth * 0.5f;
    const float height = kPlayerHeight;
    const float half_d = kPlayerWidth * 0.5f;

    // Check from one block below (for tall collision like fences with 1.5 height)
    int min_y = fast_floor_to_int(py + kEps) - 1;
    if (min_y < 0) min_y = 0;
    int max_y = fast_floor_to_int(py + height - kEps);
    int min_z = fast_floor_to_int(pz - half_d + kEps);
    int max_z = fast_floor_to_int(pz + half_d - kEps);

    if (dx > 0.0f) {
        int check_x = fast_floor_to_int((px + half_w) - kEps);
        for (int by = min_y; by <= max_y; by++) {
            for (int bz = min_z; bz <= max_z; bz++) {
                auto block_type = terrain.get_block(check_x, by, bz);
                // Use full 3D check for all blocks
                if (check_block_collision_3d(block_type, check_x, by, bz, px, py, pz, half_w, height, half_d)) {
                    auto coll = shared::voxel::get_collision_info(block_type);
                    // For narrow blocks, clamp to block's actual X extent
                    float block_edge = static_cast<float>(check_x) + coll.minX;
                    px = block_edge - half_w - kSkin;
                    vx = 0.0f;
                    logf(serverTick, "coll", "X clamp+ block=(%d,%d,%d) new_x=%.3f", check_x, by, bz, px);
                    return;
                }
            }
        }
    } else {
        int check_x = fast_floor_to_int((px - half_w) + kEps);
        for (int by = min_y; by <= max_y; by++) {
            for (int bz = min_z; bz <= max_z; bz++) {
                auto block_type = terrain.get_block(check_x, by, bz);
                // Use full 3D check for all blocks
                if (check_block_collision_3d(block_type, check_x, by, bz, px, py, pz, half_w, height, half_d)) {
                    auto coll = shared::voxel::get_collision_info(block_type);
                    // For narrow blocks, clamp to block's actual X extent
                    float block_edge = static_cast<float>(check_x) + coll.maxX;
                    px = block_edge + half_w + kSkin;
                    vx = 0.0f;
                    logf(serverTick, "coll", "X clamp- block=(%d,%d,%d) new_x=%.3f", check_x, by, bz, px);
                    return;
                }
            }
        }
    }
}

static void resolve_voxel_z(const server::voxel::Terrain& terrain, float px, float py, float& pz, float& vz, float dz, std::uint64_t serverTick) {
    if (dz == 0.0f) return;

    const float half_w = kPlayerWidth * 0.5f;
    const float height = kPlayerHeight;
    const float half_d = kPlayerWidth * 0.5f;

    // Check from one block below (for tall collision like fences with 1.5 height)
    int min_y = fast_floor_to_int(py + kEps) - 1;
    if (min_y < 0) min_y = 0;
    int max_y = fast_floor_to_int(py + height - kEps);
    int min_x = fast_floor_to_int(px - half_w + kEps);
    int max_x = fast_floor_to_int(px + half_w - kEps);

    if (dz > 0.0f) {
        int check_z = fast_floor_to_int((pz + half_d) - kEps);
        for (int by = min_y; by <= max_y; by++) {
            for (int bx = min_x; bx <= max_x; bx++) {
                auto block_type = terrain.get_block(bx, by, check_z);
                // Use full 3D check for all blocks
                if (check_block_collision_3d(block_type, bx, by, check_z, px, py, pz, half_w, height, half_d)) {
                    auto coll = shared::voxel::get_collision_info(block_type);
                    // For narrow blocks, clamp to block's actual Z extent
                    float block_edge = static_cast<float>(check_z) + coll.minZ;
                    pz = block_edge - half_d - kSkin;
                    vz = 0.0f;
                    logf(serverTick, "coll", "Z clamp+ block=(%d,%d,%d) new_z=%.3f", bx, by, check_z, pz);
                    return;
                }
            }
        }
    } else {
        int check_z = fast_floor_to_int((pz - half_d) + kEps);
        for (int by = min_y; by <= max_y; by++) {
            for (int bx = min_x; bx <= max_x; bx++) {
                auto block_type = terrain.get_block(bx, by, check_z);
                // Use full 3D check for all blocks
                if (check_block_collision_3d(block_type, bx, by, check_z, px, py, pz, half_w, height, half_d)) {
                    auto coll = shared::voxel::get_collision_info(block_type);
                    // For narrow blocks, clamp to block's actual Z extent
                    float block_edge = static_cast<float>(check_z) + coll.maxZ;
                    pz = block_edge + half_d + kSkin;
                    vz = 0.0f;
                    logf(serverTick, "coll", "Z clamp- block=(%d,%d,%d) new_z=%.3f", bx, by, check_z, pz);
                    return;
                }
            }
        }
    }
}

static void resolve_voxel_y(const server::voxel::Terrain& terrain, float px, float& py, float pz, float& vy, float dy, bool& onGround, std::uint64_t serverTick) {
    const float half_w = kPlayerWidth * 0.5f;
    const float height = kPlayerHeight;
    const float half_d = kPlayerWidth * 0.5f;

    // Ground check
    if (dy <= 0.0f) {
        // Check both the block at player's feet and one below
        for (int check_y = fast_floor_to_int(py - kEps); check_y >= fast_floor_to_int(py - 1.0f); check_y--) {
            for (int bx = fast_floor_to_int(px - half_w + kEps); bx <= fast_floor_to_int(px + half_w - kEps); bx++) {
                for (int bz = fast_floor_to_int(pz - half_d + kEps); bz <= fast_floor_to_int(pz + half_d - kEps); bz++) {
                    auto block_type = terrain.get_block(bx, check_y, bz);
                    auto coll = shared::voxel::get_collision_info(block_type);
                    if (!coll.hasCollision) continue;
                    
                    // For non-full XZ blocks (fences), check if player actually overlaps in XZ
                    if (!shared::voxel::is_full_collision_block(block_type)) {
                        float block_min_x = static_cast<float>(bx) + coll.minX;
                        float block_max_x = static_cast<float>(bx) + coll.maxX;
                        float block_min_z = static_cast<float>(bz) + coll.minZ;
                        float block_max_z = static_cast<float>(bz) + coll.maxZ;
                        
                        if (!(px - half_w < block_max_x && px + half_w > block_min_x &&
                              pz - half_d < block_max_z && pz + half_d > block_min_z)) {
                            continue; // No XZ overlap
                        }
                    }
                    
                    float ground_height = get_block_ground_height(block_type, check_y);
                    
                    // Check if player would land on this block
                    if (py <= ground_height + kEps && py > ground_height - 0.5f) {
                        py = ground_height;
                        if (vy < 0.0f) vy = 0.0f;
                        onGround = true;
                        logf(serverTick, "coll", "landed block=(%d,%d,%d) type=%d new_y=%.3f", bx, check_y, bz, static_cast<int>(block_type), py);
                        return;
                    }
                }
            }
        }
    }

    // Ceiling check
    if (dy > 0.0f) {
        int check_y = fast_floor_to_int((py + height) - kEps);
        for (int bx = fast_floor_to_int(px - half_w + kEps); bx <= fast_floor_to_int(px + half_w - kEps); bx++) {
            for (int bz = fast_floor_to_int(pz - half_d + kEps); bz <= fast_floor_to_int(pz + half_d - kEps); bz++) {
                auto block_type = terrain.get_block(bx, check_y, bz);
                auto coll = shared::voxel::get_collision_info(block_type);
                if (!coll.hasCollision) continue;
                
                // For non-full XZ blocks (fences), check if player actually overlaps in XZ
                if (!shared::voxel::is_full_collision_block(block_type)) {
                    float block_min_x = static_cast<float>(bx) + coll.minX;
                    float block_max_x = static_cast<float>(bx) + coll.maxX;
                    float block_min_z = static_cast<float>(bz) + coll.minZ;
                    float block_max_z = static_cast<float>(bz) + coll.maxZ;
                    
                    if (!(px - half_w < block_max_x && px + half_w > block_min_x &&
                          pz - half_d < block_max_z && pz + half_d > block_min_z)) {
                        continue; // No XZ overlap
                    }
                }
                
                // For ceiling check, we care about the bottom of the block's collision
                float block_bottom = static_cast<float>(check_y) + coll.minY;
                
                if (py + height > block_bottom) {
                    py = block_bottom - height;
                    if (vy > 0.0f) vy = 0.0f;
                    logf(serverTick, "coll", "ceiling block=(%d,%d,%d) type=%d new_y=%.3f", bx, check_y, bz, static_cast<int>(block_type), py);
                    return;
                }
            }
        }
    }
}

} // namespace

Server::Server(std::shared_ptr<shared::transport::IEndpoint> endpoint)
    : Server(std::move(endpoint), Options{}) {}

Server::Server(std::shared_ptr<shared::transport::IEndpoint> endpoint, Options opts)
    : endpoint_(std::move(endpoint)), opts_(opts) {
    // Apply logging options before any init logs.
    g_sv_log.enabled = opts_.logging.enabled;
    g_sv_log.init = opts_.logging.init;
    g_sv_log.rx = opts_.logging.rx;
    g_sv_log.tx = opts_.logging.tx;
    g_sv_log.move = opts_.logging.move;
    g_sv_log.coll = opts_.logging.coll;

    // Pick a seed once per server instance; client will render the same seed.
    worldSeed_ = static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    terrain_ = std::make_unique<server::voxel::Terrain>(worldSeed_);

    // Map editor runs against an empty/void base terrain (no procedural generation).
    // Authored blocks are applied as overrides and exported as the template.
    if (opts_.editorCameraMode) {
        terrain_->set_void_base(true);
    }

    // MT-1: if a map template exists on disk, prefer it over procedural base terrain.
    // The map editor disables this to avoid accidentally loading an unrelated map.
    if (opts.loadLatestMapTemplateFromDisk) {
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
                        logf(serverTick_, "init", "loaded map scripts (main: %zu bytes, modules: %zu)",
                             map.scriptData.mainScript.size(), map.scriptData.modules.size());
                    } else {
                        logf(serverTick_, "init", "failed to load map scripts: %s", result.error.c_str());
                    }
                }
            }
            
            terrain_->set_map_template(std::move(map));
            logf(serverTick_, "init", "loaded map template: %s (mapId=%s version=%u)",
                 path.generic_string().c_str(), mapId_.c_str(), mapVersion_);
        }
    }

    logf(serverTick_, "init", "tickRate=%u worldSeed=%u", tickRate_, worldSeed_);
}

Server::~Server() {
    stop();
}

void Server::start() {
    if (running_.exchange(true)) {
        return;
    }

    thread_ = std::thread([this]() { run_loop_(); });
}

void Server::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (thread_.joinable()) {
        thread_.join();
    }
}

void Server::run_loop_() {
    using clock = std::chrono::steady_clock;

    const auto tickDuration = std::chrono::duration<double>(1.0 / static_cast<double>(tickRate_));
    auto nextTick = clock::now();

    while (running_.load()) {
        nextTick += std::chrono::duration_cast<clock::duration>(tickDuration);
        tick_once_();
        std::this_thread::sleep_until(nextTick);
    }
}

void Server::tick_once_() {
    serverTick_++;

    // Drain incoming messages
    shared::proto::Message msg;
    while (endpoint_->try_recv(msg)) {
        handle_message_(msg);
    }

    // Authoritative movement integration: client sends only intent.
    if (joined_) {
        const float dt = 1.0f / static_cast<float>(tickRate_);
        const float speed = lastInput_.sprint ? 8.0f : 5.0f;

        // Move relative to yaw, similar to the old client-side implementation.
        const float yawRad = lastInput_.yaw * kDegToRad;

        const float forwardX = std::sin(yawRad);
        const float forwardZ = std::cos(yawRad);
        const float rightX = std::cos(yawRad);
        const float rightZ = -std::sin(yawRad);

        if (opts_.editorCameraMode) {
            // Editor camera: free-fly controls similar to typical map editors.
            // - WASD: horizontal movement relative to yaw
            // - Shift (sprint): faster movement
            // - camUp/camDown: vertical movement (no gravity)
            const float camSpeed = lastInput_.sprint ? 18.0f : 9.0f;

            const float moveX = lastInput_.moveX * camSpeed;
            const float moveZ = lastInput_.moveY * camSpeed;

            vx_ = rightX * moveX + forwardX * moveZ;
            vz_ = rightZ * moveX + forwardZ * moveZ;

            float vy = 0.0f;
            if (lastInput_.camUp || lastInput_.jump) vy += camSpeed;
            if (lastInput_.camDown) vy -= camSpeed;
            vy_ = vy;

            px_ += vx_ * dt;
            py_ += vy_ * dt;
            pz_ += vz_ * dt;
            onGround_ = false;
            lastJumpHeld_ = lastInput_.jump;
        } else {
            const float moveX = lastInput_.moveX * speed;
            const float moveZ = lastInput_.moveY * speed;

            vx_ = rightX * moveX + forwardX * moveZ;
            vz_ = rightZ * moveX + forwardZ * moveZ;

            // Jump edge detection to avoid re-triggering while held.
            const bool jumpHeld = lastInput_.jump;
            const bool jumpPressed = jumpHeld && !lastJumpHeld_;
            lastJumpHeld_ = jumpHeld;

            if (onGround_ && jumpPressed) {
                vy_ = kJumpVelocity;
                onGround_ = false;
            }

            // Gravity
            if (!onGround_) {
                vy_ -= kGravity * dt;
            } else if (vy_ < 0.0f) {
                vy_ = 0.0f;
            }

            // Integrate per-axis and resolve against voxel terrain blocks.
            // Step-up: if player is on ground and gets blocked by low obstacle, elevate and retry.
            const float half_w = kPlayerWidth * 0.5f;
            const float half_d = kPlayerWidth * 0.5f;
            
            const float dx = vx_ * dt;
            if (dx != 0.0f) {
                float old_px = px_;
                px_ += dx;
                resolve_voxel_x(*terrain_, px_, py_, pz_, vx_, dx, serverTick_);
                
                // If we got blocked and we're on ground, try step-up
                if (onGround_ && px_ == old_px && vx_ == 0.0f) {
                    // Temporarily move to where we wanted to go
                    px_ = old_px + dx;
                    float step_height = get_obstacle_step_height(*terrain_, px_, py_, pz_, half_w, half_d);
                    if (step_height > 0.0f && step_height <= kMaxStepUpHeight) {
                        // Check headroom
                        float new_y = py_ + step_height + kSkin;
                        int head_y = fast_floor_to_int(new_y + kPlayerHeight - kEps);
                        bool has_headroom = true;
                        for (int bx = fast_floor_to_int(px_ - half_w + kEps); bx <= fast_floor_to_int(px_ + half_w - kEps) && has_headroom; bx++) {
                            for (int bz = fast_floor_to_int(pz_ - half_d + kEps); bz <= fast_floor_to_int(pz_ + half_d - kEps) && has_headroom; bz++) {
                                auto block_type = terrain_->get_block(bx, head_y, bz);
                                if (check_block_collision_3d(block_type, bx, head_y, bz, px_, new_y, pz_, half_w, kPlayerHeight, half_d)) {
                                    has_headroom = false;
                                }
                            }
                        }
                        if (has_headroom) {
                            py_ = new_y;
                            vx_ = vx_ * dt != 0.0f ? vx_ : (dx > 0 ? 1.0f : -1.0f); // restore velocity direction
                            logf(serverTick_, "coll", "step-up X height=%.3f new_y=%.3f", step_height, py_);
                        } else {
                            px_ = old_px; // Revert
                            vx_ = 0.0f;
                        }
                    } else {
                        px_ = old_px; // Revert
                        vx_ = 0.0f;
                    }
                }
            }

            const float dz = vz_ * dt;
            if (dz != 0.0f) {
                float old_pz = pz_;
                pz_ += dz;
                resolve_voxel_z(*terrain_, px_, py_, pz_, vz_, dz, serverTick_);
                
                // If we got blocked and we're on ground, try step-up
                if (onGround_ && pz_ == old_pz && vz_ == 0.0f) {
                    // Temporarily move to where we wanted to go
                    pz_ = old_pz + dz;
                    float step_height = get_obstacle_step_height(*terrain_, px_, py_, pz_, half_w, half_d);
                    if (step_height > 0.0f && step_height <= kMaxStepUpHeight) {
                        // Check headroom
                        float new_y = py_ + step_height + kSkin;
                        int head_y = fast_floor_to_int(new_y + kPlayerHeight - kEps);
                        bool has_headroom = true;
                        for (int bx = fast_floor_to_int(px_ - half_w + kEps); bx <= fast_floor_to_int(px_ + half_w - kEps) && has_headroom; bx++) {
                            for (int bz = fast_floor_to_int(pz_ - half_d + kEps); bz <= fast_floor_to_int(pz_ + half_d - kEps) && has_headroom; bz++) {
                                auto block_type = terrain_->get_block(bx, head_y, bz);
                                if (check_block_collision_3d(block_type, bx, head_y, bz, px_, new_y, pz_, half_w, kPlayerHeight, half_d)) {
                                    has_headroom = false;
                                }
                            }
                        }
                        if (has_headroom) {
                            py_ = new_y;
                            vz_ = vz_ * dt != 0.0f ? vz_ : (dz > 0 ? 1.0f : -1.0f); // restore velocity direction
                            logf(serverTick_, "coll", "step-up Z height=%.3f new_y=%.3f", step_height, py_);
                        } else {
                            pz_ = old_pz; // Revert
                            vz_ = 0.0f;
                        }
                    } else {
                        pz_ = old_pz; // Revert
                        vz_ = 0.0f;
                    }
                }
            }

            const float dy = vy_ * dt;
            py_ += dy;
            onGround_ = false;
            resolve_voxel_y(*terrain_, px_, py_, pz_, vy_, dy, onGround_, serverTick_);
        }

        if (should_log_movement(serverTick_)) {
            logf(serverTick_, "move", "pos=(%.2f,%.2f,%.2f) vel=(%.2f,%.2f,%.2f) onGround=%d", px_, py_, pz_, vx_, vy_, vz_, onGround_ ? 1 : 0);
        }

        if (scriptEngine_ && scriptEngine_->has_scripts()) {
            scriptEngine_->update(dt);
            process_script_commands_();
        }

        // Periodic snapshot (every tick for now; can be throttled later)
        shared::proto::StateSnapshot snap;
        snap.serverTick = serverTick_;
        snap.playerId = playerId_;
        snap.px = px_;
        snap.py = py_;
        snap.pz = pz_;
        snap.vx = vx_;
        snap.vy = vy_;
        snap.vz = vz_;
        endpoint_->send(std::move(snap));
    }
}

void Server::handle_message_(shared::proto::Message& msg) {
    const auto is_in_block_reach = [this](int bx, int by, int bz) -> bool {
        constexpr float kMaxReach = shared::kBlockReachDistance;
        const float cx = static_cast<float>(bx) + 0.5f;
        const float cy = static_cast<float>(by) + 0.5f;
        const float cz = static_cast<float>(bz) + 0.5f;
        const float dx = cx - px_;
        const float dy = cy - (py_ + shared::kPlayerEyeHeight);
        const float dz = cz - pz_;
        return (dx * dx + dy * dy + dz * dz) <= (kMaxReach * kMaxReach);
    };

    const auto would_intersect_player = [this](int bx, int by, int bz) -> bool {
        // Player AABB (feet at py_)
        const float half_w = kPlayerWidth * 0.5f;
        const float half_d = kPlayerWidth * 0.5f;
        const float pMinX = px_ - half_w;
        const float pMaxX = px_ + half_w;
        const float pMinY = py_;
        const float pMaxY = py_ + kPlayerHeight;
        const float pMinZ = pz_ - half_d;
        const float pMaxZ = pz_ + half_d;

        // Block AABB
        const float bMinX = static_cast<float>(bx);
        const float bMaxX = static_cast<float>(bx + 1);
        const float bMinY = static_cast<float>(by);
        const float bMaxY = static_cast<float>(by + 1);
        const float bMinZ = static_cast<float>(bz);
        const float bMaxZ = static_cast<float>(bz + 1);

        const bool overlapX = (pMinX < (bMaxX - kEps)) && (pMaxX > (bMinX + kEps));
        const bool overlapY = (pMinY < (bMaxY - kEps)) && (pMaxY > (bMinY + kEps));
        const bool overlapZ = (pMinZ < (bMaxZ - kEps)) && (pMaxZ > (bMinZ + kEps));
        return overlapX && overlapY && overlapZ;
    };

    if (std::holds_alternative<shared::proto::ClientHello>(msg)) {
        const auto& hello = std::get<shared::proto::ClientHello>(msg);

        logf(serverTick_, "rx", "ClientHello version=%u name=%s", hello.version, hello.clientName.c_str());

        helloSeen_ = true;

        shared::proto::ServerHello resp;
        resp.acceptedVersion = hello.version; // minimal: assume compatible
        resp.tickRate = tickRate_;
        resp.worldSeed = worldSeed_;
        resp.hasMapTemplate = hasMapTemplate_;
        if (hasMapTemplate_) {
            resp.mapId = mapId_;
            resp.mapVersion = mapVersion_;
        }
        logf(serverTick_, "tx", "ServerHello tickRate=%u worldSeed=%u", resp.tickRate, resp.worldSeed);
        endpoint_->send(std::move(resp));
        return;
    }

    if (std::holds_alternative<shared::proto::JoinMatch>(msg)) {
        logf(serverTick_, "rx", "JoinMatch");
        if (!helloSeen_) {
            shared::proto::ActionRejected rej;
            rej.seq = 0;
            rej.reason = shared::proto::RejectReason::NotAllowed;
            logf(serverTick_, "tx", "ActionRejected seq=0 reason=%u", static_cast<unsigned>(rej.reason));
            endpoint_->send(std::move(rej));
            return;
        }

        joined_ = true;

        if (scriptEngine_ && scriptEngine_->has_scripts()) {
            scriptEngine_->on_player_join(playerId_);
        }

        shared::proto::JoinAck ack;
        ack.playerId = playerId_;
        logf(serverTick_, "tx", "JoinAck playerId=%u", ack.playerId);
        endpoint_->send(std::move(ack));
        return;
    }

    if (std::holds_alternative<shared::proto::InputFrame>(msg)) {
        lastInput_ = std::get<shared::proto::InputFrame>(msg);

        if (lastInputLogTick_ == 0 || (serverTick_ - lastInputLogTick_) >= tickRate_) {
            logf(serverTick_,
                 "rx",
                 "InputFrame seq=%u move=(%.2f,%.2f) yaw=%.1f pitch=%.1f jump=%d sprint=%d up=%d down=%d",
                 lastInput_.seq,
                 lastInput_.moveX,
                 lastInput_.moveY,
                 lastInput_.yaw,
                 lastInput_.pitch,
                 lastInput_.jump ? 1 : 0,
                 lastInput_.sprint ? 1 : 0,
                 lastInput_.camUp ? 1 : 0,
                 lastInput_.camDown ? 1 : 0);
            lastInputLogTick_ = serverTick_;
        }
        return;
    }

    if (std::holds_alternative<shared::proto::TryBreakBlock>(msg)) {
        const auto& req = std::get<shared::proto::TryBreakBlock>(msg);
        logf(serverTick_, "rx", "TryBreakBlock seq=%u pos=(%d,%d,%d)", req.seq, req.x, req.y, req.z);

        if (!joined_) {
            shared::proto::ActionRejected rej;
            rej.seq = req.seq;
            rej.reason = shared::proto::RejectReason::NotAllowed;
            logf(serverTick_, "tx", "ActionRejected seq=%u reason=%u", rej.seq, static_cast<unsigned>(rej.reason));
            endpoint_->send(std::move(rej));
            return;
        }

        if (req.y < 0 || req.y >= shared::voxel::CHUNK_HEIGHT) {
            shared::proto::ActionRejected rej;
            rej.seq = req.seq;
            rej.reason = shared::proto::RejectReason::Invalid;
            logf(serverTick_, "tx", "ActionRejected seq=%u reason=%u (bad y)", rej.seq, static_cast<unsigned>(rej.reason));
            endpoint_->send(std::move(rej));
            return;
        }

        if (!is_in_block_reach(req.x, req.y, req.z)) {
            shared::proto::ActionRejected rej;
            rej.seq = req.seq;
            rej.reason = shared::proto::RejectReason::OutOfRange;
            logf(serverTick_, "tx", "ActionRejected seq=%u reason=%u", rej.seq, static_cast<unsigned>(rej.reason));
            endpoint_->send(std::move(rej));
            return;
        }

        const auto cur = terrain_->get_block(req.x, req.y, req.z);
        if (cur == shared::voxel::BlockType::Air) {
            shared::proto::ActionRejected rej;
            rej.seq = req.seq;
            rej.reason = shared::proto::RejectReason::Invalid;
            logf(serverTick_, "tx", "ActionRejected seq=%u reason=%u", rej.seq, static_cast<unsigned>(rej.reason));
            endpoint_->send(std::move(rej));
            return;
        }

        if (!terrain_->can_player_break(req.x, req.y, req.z, cur)) {
            shared::proto::ActionRejected rej;
            rej.seq = req.seq;
            rej.reason = shared::proto::RejectReason::ProtectedBlock;
            logf(serverTick_, "tx", "ActionRejected seq=%u reason=%u", rej.seq, static_cast<unsigned>(rej.reason));
            endpoint_->send(std::move(rej));
            return;
        }

        terrain_->break_player_block(req.x, req.y, req.z);
        terrain_->set_block_state(req.x, req.y, req.z, shared::voxel::BlockRuntimeState::defaults());

        if (scriptEngine_ && scriptEngine_->has_scripts()) {
            scriptEngine_->on_block_break(playerId_, req.x, req.y, req.z, static_cast<int>(cur));
        }

        shared::proto::BlockBroken ev;
        ev.x = req.x;
        ev.y = req.y;
        ev.z = req.z;
        logf(serverTick_, "tx", "BlockBroken pos=(%d,%d,%d)", ev.x, ev.y, ev.z);
        endpoint_->send(std::move(ev));
        
        // Update neighbor connections (fences that were connected to this block)
        auto neighborUpdates = terrain_->update_neighbor_states(req.x, req.y, req.z);
        for (const auto& update : neighborUpdates) {
            shared::proto::BlockPlaced neighborPlaced;
            neighborPlaced.x = update.x;
            neighborPlaced.y = update.y;
            neighborPlaced.z = update.z;
            neighborPlaced.blockType = update.type;
            neighborPlaced.stateByte = update.state.to_byte();
            logf(serverTick_, "tx", "BlockPlaced (neighbor) pos=(%d,%d,%d) type=%u state=%u", 
                 neighborPlaced.x, neighborPlaced.y, neighborPlaced.z, 
                 static_cast<unsigned>(neighborPlaced.blockType), neighborPlaced.stateByte);
            endpoint_->send(std::move(neighborPlaced));
        }
        return;
    }

    if (std::holds_alternative<shared::proto::TryPlaceBlock>(msg)) {
        const auto& req = std::get<shared::proto::TryPlaceBlock>(msg);
        logf(serverTick_, "rx", "TryPlaceBlock seq=%u pos=(%d,%d,%d) type=%u hitY=%.2f face=%u",
             req.seq,
             req.x,
             req.y,
             req.z,
             static_cast<unsigned>(req.blockType),
             req.hitY,
             static_cast<unsigned>(req.face));

        if (!joined_) {
            shared::proto::ActionRejected rej;
            rej.seq = req.seq;
            rej.reason = shared::proto::RejectReason::NotAllowed;
            logf(serverTick_, "tx", "ActionRejected seq=%u reason=%u", rej.seq, static_cast<unsigned>(rej.reason));
            endpoint_->send(std::move(rej));
            return;
        }

        if (req.y < 0 || req.y >= shared::voxel::CHUNK_HEIGHT) {
            shared::proto::ActionRejected rej;
            rej.seq = req.seq;
            rej.reason = shared::proto::RejectReason::Invalid;
            logf(serverTick_, "tx", "ActionRejected seq=%u reason=%u (bad y)", rej.seq, static_cast<unsigned>(rej.reason));
            endpoint_->send(std::move(rej));
            return;
        }

        if (!is_in_block_reach(req.x, req.y, req.z)) {
            shared::proto::ActionRejected rej;
            rej.seq = req.seq;
            rej.reason = shared::proto::RejectReason::OutOfRange;
            logf(serverTick_, "tx", "ActionRejected seq=%u reason=%u", rej.seq, static_cast<unsigned>(rej.reason));
            endpoint_->send(std::move(rej));
            return;
        }

        if (would_intersect_player(req.x, req.y, req.z)) {
            shared::proto::ActionRejected rej;
            rej.seq = req.seq;
            rej.reason = shared::proto::RejectReason::NotAllowed;
            logf(serverTick_, "tx", "ActionRejected seq=%u reason=%u (intersects player)", rej.seq, static_cast<unsigned>(rej.reason));
            endpoint_->send(std::move(rej));
            return;
        }

        if (req.blockType == shared::voxel::BlockType::Air) {
            shared::proto::ActionRejected rej;
            rej.seq = req.seq;
            rej.reason = shared::proto::RejectReason::Invalid;
            logf(serverTick_, "tx", "ActionRejected seq=%u reason=%u", rej.seq, static_cast<unsigned>(rej.reason));
            endpoint_->send(std::move(rej));
            return;
        }

        if (req.blockType == shared::voxel::BlockType::Bedrock) {
            shared::proto::ActionRejected rej;
            rej.seq = req.seq;
            rej.reason = shared::proto::RejectReason::NotAllowed;
            logf(serverTick_, "tx", "ActionRejected seq=%u reason=%u", rej.seq, static_cast<unsigned>(rej.reason));
            endpoint_->send(std::move(rej));
            return;
        }

        const auto cur = terrain_->get_block(req.x, req.y, req.z);
        const auto curState = terrain_->get_block_state(req.x, req.y, req.z);
        
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
                    
                    if (scriptEngine_ && scriptEngine_->has_scripts()) {
                        scriptEngine_->on_block_place(playerId_, req.x, req.y, req.z, static_cast<int>(fullBlock));
                    }
                    
                    shared::proto::BlockPlaced ev;
                    ev.x = req.x;
                    ev.y = req.y;
                    ev.z = req.z;
                    ev.blockType = fullBlock;
                    ev.stateByte = 0;
                    logf(serverTick_, "tx", "BlockPlaced (slab merge) pos=(%d,%d,%d) type=%u", ev.x, ev.y, ev.z, static_cast<unsigned>(ev.blockType));
                    endpoint_->send(std::move(ev));
                    return;
                }
            }
        }
        
        // Normal placement: position must be air
        if (cur != shared::voxel::BlockType::Air) {
            shared::proto::ActionRejected rej;
            rej.seq = req.seq;
            rej.reason = shared::proto::RejectReason::Invalid;
            logf(serverTick_, "tx", "ActionRejected seq=%u reason=%u", rej.seq, static_cast<unsigned>(rej.reason));
            endpoint_->send(std::move(rej));
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

        if (scriptEngine_ && scriptEngine_->has_scripts()) {
            scriptEngine_->on_block_place(playerId_, req.x, req.y, req.z, static_cast<int>(finalBlockType));
        }

        shared::proto::BlockPlaced ev;
        ev.x = req.x;
        ev.y = req.y;
        ev.z = req.z;
        ev.blockType = finalBlockType;
        ev.stateByte = state.to_byte();
        logf(serverTick_, "tx", "BlockPlaced pos=(%d,%d,%d) type=%u state=%u", ev.x, ev.y, ev.z, static_cast<unsigned>(ev.blockType), ev.stateByte);
        endpoint_->send(std::move(ev));
        
        // Update neighbor connections (fences connecting to this block)
        auto neighborUpdates = terrain_->update_neighbor_states(req.x, req.y, req.z);
        for (const auto& update : neighborUpdates) {
            shared::proto::BlockPlaced neighborPlaced;
            neighborPlaced.x = update.x;
            neighborPlaced.y = update.y;
            neighborPlaced.z = update.z;
            neighborPlaced.blockType = update.type;
            neighborPlaced.stateByte = update.state.to_byte();
            logf(serverTick_, "tx", "BlockPlaced (neighbor) pos=(%d,%d,%d) type=%u state=%u", 
                 neighborPlaced.x, neighborPlaced.y, neighborPlaced.z, 
                 static_cast<unsigned>(neighborPlaced.blockType), neighborPlaced.stateByte);
            endpoint_->send(std::move(neighborPlaced));
        }
        return;
    }

    if (std::holds_alternative<shared::proto::TrySetBlock>(msg)) {
        const auto& req = std::get<shared::proto::TrySetBlock>(msg);
        logf(serverTick_, "rx", "TrySetBlock seq=%u pos=(%d,%d,%d) type=%u",
             req.seq,
             req.x,
             req.y,
             req.z,
             static_cast<unsigned>(req.blockType));

        if (!joined_) {
            shared::proto::ActionRejected rej;
            rej.seq = req.seq;
            rej.reason = shared::proto::RejectReason::NotAllowed;
            logf(serverTick_, "tx", "ActionRejected seq=%u reason=%u", rej.seq, static_cast<unsigned>(rej.reason));
            endpoint_->send(std::move(rej));
            return;
        }

        if (req.y < 0 || req.y >= shared::voxel::CHUNK_HEIGHT) {
            shared::proto::ActionRejected rej;
            rej.seq = req.seq;
            rej.reason = shared::proto::RejectReason::Invalid;
            logf(serverTick_, "tx", "ActionRejected seq=%u reason=%u (bad y)", rej.seq, static_cast<unsigned>(rej.reason));
            endpoint_->send(std::move(rej));
            return;
        }

        // Apply authoritative edit.
        const auto prev = terrain_->get_block(req.x, req.y, req.z);
        terrain_->set_block(req.x, req.y, req.z, req.blockType);
        const auto cur = terrain_->get_block(req.x, req.y, req.z);

        if (cur == prev) {
            shared::proto::ActionRejected rej;
            rej.seq = req.seq;
            rej.reason = shared::proto::RejectReason::Invalid;
            logf(serverTick_, "tx", "ActionRejected seq=%u reason=%u (no-op)", rej.seq, static_cast<unsigned>(rej.reason));
            endpoint_->send(std::move(rej));
            return;
        }

        if (cur == shared::voxel::BlockType::Air) {
            shared::proto::BlockBroken ev;
            ev.x = req.x;
            ev.y = req.y;
            ev.z = req.z;
            logf(serverTick_, "tx", "BlockBroken (editor) pos=(%d,%d,%d)", ev.x, ev.y, ev.z);
            endpoint_->send(std::move(ev));
        } else {
            shared::proto::BlockPlaced ev;
            ev.x = req.x;
            ev.y = req.y;
            ev.z = req.z;
            ev.blockType = cur;
            logf(serverTick_, "tx", "BlockPlaced (editor) pos=(%d,%d,%d) type=%u", ev.x, ev.y, ev.z, static_cast<unsigned>(ev.blockType));
            endpoint_->send(std::move(ev));
        }
        return;
    }

    if (std::holds_alternative<shared::proto::TryExportMap>(msg)) {
        const auto& req = std::get<shared::proto::TryExportMap>(msg);
        logf(serverTick_, "rx", "TryExportMap seq=%u mapId=%s version=%u chunks=[(%d,%d)-(%d,%d)]",
             req.seq,
             req.mapId.c_str(),
             req.version,
             req.chunkMinX, req.chunkMinZ, req.chunkMaxX, req.chunkMaxZ);

        shared::proto::ExportResult result;
        result.seq = req.seq;

        if (!joined_) {
            result.ok = false;
            result.reason = shared::proto::RejectReason::NotAllowed;
            logf(serverTick_, "tx", "ExportResult seq=%u ok=0 reason=%u", result.seq, static_cast<unsigned>(result.reason));
            endpoint_->send(std::move(result));
            return;
        }

        if (!is_valid_map_id(req.mapId) || req.version == 0) {
            result.ok = false;
            result.reason = shared::proto::RejectReason::Invalid;
            logf(serverTick_, "tx", "ExportResult seq=%u ok=0 reason=%u", result.seq, static_cast<unsigned>(result.reason));
            endpoint_->send(std::move(result));
            return;
        }

        if (req.chunkMinX > req.chunkMaxX || req.chunkMinZ > req.chunkMaxZ) {
            result.ok = false;
            result.reason = shared::proto::RejectReason::Invalid;
            logf(serverTick_, "tx", "ExportResult seq=%u ok=0 reason=%u (bad bounds)", result.seq, static_cast<unsigned>(result.reason));
            endpoint_->send(std::move(result));
            return;
        }

        // Maps are always loose files in maps/ next to executable.
        const std::filesystem::path mapsDir = shared::maps::runtime_maps_dir();
        std::error_code ec;
        std::filesystem::create_directories(mapsDir, ec);
        if (ec) {
            result.ok = false;
            result.reason = shared::proto::RejectReason::Unknown;
            logf(serverTick_, "tx", "ExportResult seq=%u ok=0 reason=%u (mkdir failed)", result.seq, static_cast<unsigned>(result.reason));
            endpoint_->send(std::move(result));
            return;
        }

        const auto fileName = req.mapId + "_v" + std::to_string(req.version) + ".rfmap";
        const auto outPath = mapsDir / fileName;

        shared::maps::ExportRequest exportReq;
        exportReq.mapId = req.mapId;
        exportReq.version = req.version;
        exportReq.bounds.chunkMinX = req.chunkMinX;
        exportReq.bounds.chunkMinZ = req.chunkMinZ;
        exportReq.bounds.chunkMaxX = req.chunkMaxX;
        exportReq.bounds.chunkMaxZ = req.chunkMaxZ;

        // MT-1: preserve template protection allow-list when exporting from an existing template.
        if (const auto* tmpl = terrain_->map_template(); tmpl) {
            exportReq.breakableTemplateBlocks = tmpl->breakableTemplateBlocks;
        }

        // MV-1: embed visual settings (render-only).
        exportReq.visualSettings = shared::maps::default_visual_settings();
        {
            const std::uint8_t k = req.skyboxKind;
            // MV-1 spec defined 0=None, 1=Day, 2=Night. The editor picker may provide additional
            // numeric IDs that map to Panorama_Sky_XX textures; keep it bounded.
            constexpr std::uint8_t kMaxSkyboxId = 25u;
            const std::uint8_t clamped = (k > kMaxSkyboxId) ? kMaxSkyboxId : k;
            exportReq.visualSettings.skyboxKind = static_cast<shared::maps::MapTemplate::SkyboxKind>(clamped);

            float t = req.timeOfDayHours;
            if (t < 0.0f) t = 0.0f;
            if (t > 24.0f) t = 24.0f;
            exportReq.visualSettings.timeOfDayHours = t;

            exportReq.visualSettings.useMoon = req.useMoon;

            float sunI = req.sunIntensity;
            if (sunI < 0.0f) sunI = 0.0f;
            if (sunI > 10.0f) sunI = 10.0f;
            exportReq.visualSettings.sunIntensity = sunI;

            float ambI = req.ambientIntensity;
            if (ambI < 0.0f) ambI = 0.0f;
            if (ambI > 5.0f) ambI = 5.0f;
            exportReq.visualSettings.ambientIntensity = ambI;

            float temp = req.temperature;
            if (temp < 0.0f) temp = 0.0f;
            if (temp > 1.0f) temp = 1.0f;
            exportReq.visualSettings.temperature = temp;

            float hum = req.humidity;
            if (hum < 0.0f) hum = 0.0f;
            if (hum > 1.0f) hum = 1.0f;
            exportReq.visualSettings.humidity = hum;
        }

        std::string err;
        const bool ok = shared::maps::write_rfmap(
            outPath,
            exportReq,
            [this](int x, int y, int z) { return terrain_->get_block(x, y, z); },
            &err);

        if (!ok) {
            result.ok = false;
            result.reason = shared::proto::RejectReason::Unknown;
            logf(serverTick_, "tx", "ExportResult seq=%u ok=0 reason=%u (write failed: %s)", result.seq, static_cast<unsigned>(result.reason), err.c_str());
            endpoint_->send(std::move(result));
            return;
        }

        result.ok = true;
        result.reason = shared::proto::RejectReason::Unknown;
        result.path = outPath.generic_string();
        logf(serverTick_, "tx", "ExportResult seq=%u ok=1 path=%s", result.seq, result.path.c_str());
        endpoint_->send(std::move(result));
        return;
    }
}

void Server::init_script_engine_() {
    scriptEngine_ = std::make_unique<server::scripting::ScriptEngine>();
    
    if (!scriptEngine_->init()) {
        logf(serverTick_, "init", "failed to initialize script engine");
        scriptEngine_.reset();
        return;
    }
    
    scriptEngine_->set_log_callback([this](const std::string& msg) {
        logf(serverTick_, "script", "%s", msg.c_str());
    });
    
    logf(serverTick_, "init", "script engine initialized");
}

void Server::process_script_commands_() {
    if (!scriptEngine_) return;
    
    auto commands = scriptEngine_->take_commands();
    for (const auto& cmd : commands) {
        switch (cmd.type) {
            case scripting::ScriptCommand::Type::Broadcast:
                // TODO: Implement broadcast message to all clients
                logf(serverTick_, "script", "broadcast: %s", cmd.stringParam.c_str());
                break;
                
            case scripting::ScriptCommand::Type::SetBlock: {
                const int x = cmd.intParams[0];
                const int y = cmd.intParams[1];
                const int z = cmd.intParams[2];
                const auto bt = static_cast<shared::voxel::BlockType>(cmd.intParams[3]);
                
                terrain_->set_block(x, y, z, bt);
                
                if (bt == shared::voxel::BlockType::Air) {
                    shared::proto::BlockBroken ev;
                    ev.x = x;
                    ev.y = y;
                    ev.z = z;
                    endpoint_->send(std::move(ev));
                } else {
                    shared::proto::BlockPlaced ev;
                    ev.x = x;
                    ev.y = y;
                    ev.z = z;
                    ev.blockType = bt;
                    endpoint_->send(std::move(ev));
                }
                break;
            }
            
            case scripting::ScriptCommand::Type::EndRound:
                // TODO: Implement round end logic
                logf(serverTick_, "script", "end_round: team=%d", cmd.intParams[0]);
                break;
                
            case scripting::ScriptCommand::Type::TeleportPlayer:
                // TODO: Implement player teleport
                break;
                
            case scripting::ScriptCommand::Type::SetPlayerHealth:
                // TODO: Implement player health
                break;
                
            case scripting::ScriptCommand::Type::SpawnEntity:
                // TODO: Implement entity spawning
                break;
                
            default:
                break;
        }
    }
}

} // namespace server::core
