#include "server.hpp"

#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <thread>

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
        endpoint_->send(std::move(snap));
    }
}

void Server::handle_message_(shared::proto::Message& msg) {
    if (std::holds_alternative<shared::proto::ClientHello>(msg)) {
        const auto& hello = std::get<shared::proto::ClientHello>(msg);

        logf(serverTick_, "rx", "ClientHello version=%u name=%s", hello.version, hello.clientName.c_str());

        helloSeen_ = true;

        shared::proto::ServerHello resp;
        resp.acceptedVersion = hello.version; // minimal: assume compatible
        resp.tickRate = tickRate_;
        resp.worldSeed = worldSeed_;
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
}

} // namespace server::core
