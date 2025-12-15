#include "server.hpp"

#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <system_error>
#include <thread>

#include "../../shared/maps/rfmap_io.hpp"

namespace server::core {

namespace {

constexpr float kDegToRad = 0.017453292519943295f;

// Keep these aligned with the old client-side defaults for now.
constexpr float kPlayerWidth = 0.6f;
constexpr float kPlayerHeight = 1.8f;
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

    const std::filesystem::path mapsDir = std::filesystem::path("maps");
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

static void logf(std::uint64_t serverTick, const char* tag, const char* fmt, ...) {
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

static void resolve_voxel_x(const server::voxel::Terrain& terrain, float& px, float py, float pz, float& vx, float dx, std::uint64_t serverTick) {
    if (dx == 0.0f) return;

    const float half_w = kPlayerWidth * 0.5f;
    const float height = kPlayerHeight;
    const float half_d = kPlayerWidth * 0.5f;

    int min_y = fast_floor_to_int(py + kEps);
    int max_y = fast_floor_to_int(py + height - kEps);
    int min_z = fast_floor_to_int(pz - half_d + kEps);
    int max_z = fast_floor_to_int(pz + half_d - kEps);

    if (dx > 0.0f) {
        int check_x = fast_floor_to_int((px + half_w) - kEps);
        for (int by = min_y; by <= max_y; by++) {
            for (int bz = min_z; bz <= max_z; bz++) {
                if (shared::voxel::util::is_solid(terrain.get_block(check_x, by, bz))) {
                    px = static_cast<float>(check_x) - half_w - kSkin;
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
                if (shared::voxel::util::is_solid(terrain.get_block(check_x, by, bz))) {
                    px = static_cast<float>(check_x + 1) + half_w + kSkin;
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

    int min_y = fast_floor_to_int(py + kEps);
    int max_y = fast_floor_to_int(py + height - kEps);
    int min_x = fast_floor_to_int(px - half_w + kEps);
    int max_x = fast_floor_to_int(px + half_w - kEps);

    if (dz > 0.0f) {
        int check_z = fast_floor_to_int((pz + half_d) - kEps);
        for (int by = min_y; by <= max_y; by++) {
            for (int bx = min_x; bx <= max_x; bx++) {
                if (shared::voxel::util::is_solid(terrain.get_block(bx, by, check_z))) {
                    pz = static_cast<float>(check_z) - half_d - kSkin;
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
                if (shared::voxel::util::is_solid(terrain.get_block(bx, by, check_z))) {
                    pz = static_cast<float>(check_z + 1) + half_d + kSkin;
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
        int check_y = fast_floor_to_int(py - kEps);
        for (int bx = fast_floor_to_int(px - half_w + kEps); bx <= fast_floor_to_int(px + half_w - kEps); bx++) {
            for (int bz = fast_floor_to_int(pz - half_d + kEps); bz <= fast_floor_to_int(pz + half_d - kEps); bz++) {
                if (shared::voxel::util::is_solid(terrain.get_block(bx, check_y, bz))) {
                    py = static_cast<float>(check_y + 1);
                    if (vy < 0.0f) vy = 0.0f;
                    onGround = true;
                    logf(serverTick, "coll", "landed block=(%d,%d,%d) new_y=%.3f", bx, check_y, bz, py);
                    return;
                }
            }
        }
    }

    // Ceiling check
    if (dy > 0.0f) {
        int check_y = fast_floor_to_int((py + height) - kEps);
        for (int bx = fast_floor_to_int(px - half_w + kEps); bx <= fast_floor_to_int(px + half_w - kEps); bx++) {
            for (int bz = fast_floor_to_int(pz - half_d + kEps); bz <= fast_floor_to_int(pz + half_d - kEps); bz++) {
                if (shared::voxel::util::is_solid(terrain.get_block(bx, check_y, bz))) {
                    py = static_cast<float>(check_y) - height;
                    if (vy > 0.0f) vy = 0.0f;
                    logf(serverTick, "coll", "ceiling block=(%d,%d,%d) new_y=%.3f", bx, check_y, bz, py);
                    return;
                }
            }
        }
    }
}

} // namespace

Server::Server(std::shared_ptr<shared::transport::IEndpoint> endpoint)
    : endpoint_(std::move(endpoint)) {
    // Pick a seed once per server instance; client will render the same seed.
    worldSeed_ = static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    terrain_ = std::make_unique<server::voxel::Terrain>(worldSeed_);

    // MT-1: if a map template exists on disk, prefer it over procedural base terrain.
    {
        shared::maps::MapTemplate map;
        std::filesystem::path path;
        if (load_latest_rfmap(&map, &path)) {
            hasMapTemplate_ = true;
            mapId_ = map.mapId;
            mapVersion_ = map.version;
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
        const float dx = vx_ * dt;
        if (dx != 0.0f) {
            px_ += dx;
            resolve_voxel_x(*terrain_, px_, py_, pz_, vx_, dx, serverTick_);
        }

        const float dz = vz_ * dt;
        if (dz != 0.0f) {
            pz_ += dz;
            resolve_voxel_z(*terrain_, px_, py_, pz_, vz_, dz, serverTick_);
        }

        const float dy = vy_ * dt;
        py_ += dy;
        onGround_ = false;
        resolve_voxel_y(*terrain_, px_, py_, pz_, vy_, dy, onGround_, serverTick_);

        if (should_log_movement(serverTick_)) {
            logf(serverTick_, "move", "pos=(%.2f,%.2f,%.2f) vel=(%.2f,%.2f,%.2f) onGround=%d", px_, py_, pz_, vx_, vy_, vz_, onGround_ ? 1 : 0);
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
        // Keep in sync with client voxel::BlockInteraction::MAX_REACH_DISTANCE (with small fudge).
        constexpr float kMaxReach = 6.0f;
        const float cx = static_cast<float>(bx) + 0.5f;
        const float cy = static_cast<float>(by) + 0.5f;
        const float cz = static_cast<float>(bz) + 0.5f;
        const float dx = cx - px_;
        const float dy = cy - (py_ + 1.62f);
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

        shared::proto::JoinAck ack;
        ack.playerId = playerId_;
        logf(serverTick_, "tx", "JoinAck playerId=%u", ack.playerId);
        endpoint_->send(std::move(ack));
        return;
    }

    if (std::holds_alternative<shared::proto::InputFrame>(msg)) {
        lastInput_ = std::get<shared::proto::InputFrame>(msg);
        logf(serverTick_, "rx", "InputFrame seq=%u move=(%.2f,%.2f) yaw=%.1f pitch=%.1f jump=%d sprint=%d",
             lastInput_.seq,
             lastInput_.moveX,
             lastInput_.moveY,
             lastInput_.yaw,
             lastInput_.pitch,
             lastInput_.jump ? 1 : 0,
             lastInput_.sprint ? 1 : 0);
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

        shared::proto::BlockBroken ev;
        ev.x = req.x;
        ev.y = req.y;
        ev.z = req.z;
        logf(serverTick_, "tx", "BlockBroken pos=(%d,%d,%d)", ev.x, ev.y, ev.z);
        endpoint_->send(std::move(ev));
        return;
    }

    if (std::holds_alternative<shared::proto::TryPlaceBlock>(msg)) {
        const auto& req = std::get<shared::proto::TryPlaceBlock>(msg);
        logf(serverTick_, "rx", "TryPlaceBlock seq=%u pos=(%d,%d,%d) type=%u",
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
        if (cur != shared::voxel::BlockType::Air) {
            shared::proto::ActionRejected rej;
            rej.seq = req.seq;
            rej.reason = shared::proto::RejectReason::Invalid;
            logf(serverTick_, "tx", "ActionRejected seq=%u reason=%u", rej.seq, static_cast<unsigned>(rej.reason));
            endpoint_->send(std::move(rej));
            return;
        }

        terrain_->place_player_block(req.x, req.y, req.z, req.blockType);

        shared::proto::BlockPlaced ev;
        ev.x = req.x;
        ev.y = req.y;
        ev.z = req.z;
        ev.blockType = req.blockType;
        logf(serverTick_, "tx", "BlockPlaced pos=(%d,%d,%d) type=%u", ev.x, ev.y, ev.z, static_cast<unsigned>(ev.blockType));
        endpoint_->send(std::move(ev));
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

        const std::filesystem::path mapsDir = std::filesystem::path("maps");
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

} // namespace server::core
