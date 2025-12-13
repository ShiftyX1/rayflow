#include "server.hpp"

#include <chrono>
#include <cmath>
#include <thread>

namespace server::core {

Server::Server(std::shared_ptr<shared::transport::IEndpoint> endpoint)
    : endpoint_(std::move(endpoint)) {}

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

    // Very temporary movement integration (authoritative placeholder)
    if (joined_) {
        const float dt = 1.0f / static_cast<float>(tickRate_);
        const float speed = lastInput_.sprint ? 8.0f : 5.0f;

        // Move relative to yaw, similar to the old client-side implementation.
        constexpr float kDegToRad = 0.017453292519943295f;
        const float yawRad = lastInput_.yaw * kDegToRad;

        const float forwardX = std::sin(yawRad);
        const float forwardZ = std::cos(yawRad);
        const float rightX = std::cos(yawRad);
        const float rightZ = -std::sin(yawRad);

        const float moveX = lastInput_.moveX * speed;
        const float moveZ = lastInput_.moveY * speed;

        const float vx = rightX * moveX + forwardX * moveZ;
        const float vz = rightZ * moveX + forwardZ * moveZ;

        px_ += vx * dt;
        pz_ += vz * dt;

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

        helloSeen_ = true;

        shared::proto::ServerHello resp;
        resp.acceptedVersion = hello.version; // minimal: assume compatible
        resp.tickRate = tickRate_;
        endpoint_->send(std::move(resp));
        return;
    }

    if (std::holds_alternative<shared::proto::JoinMatch>(msg)) {
        if (!helloSeen_) {
            shared::proto::ActionRejected rej;
            rej.seq = 0;
            rej.reason = shared::proto::RejectReason::NotAllowed;
            endpoint_->send(std::move(rej));
            return;
        }

        joined_ = true;

        shared::proto::JoinAck ack;
        ack.playerId = playerId_;
        endpoint_->send(std::move(ack));
        return;
    }

    if (std::holds_alternative<shared::proto::InputFrame>(msg)) {
        lastInput_ = std::get<shared::proto::InputFrame>(msg);
        return;
    }
}

} // namespace server::core
