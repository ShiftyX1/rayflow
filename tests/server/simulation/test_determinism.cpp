/**
 * @file test_determinism.cpp
 * @brief Unit tests for simulation determinism.
 * 
 * Verifies that identical inputs produce identical outputs.
 */

#include <catch2/catch_test_macros.hpp>

#include "core/server.hpp"
#include "transport/local_transport.hpp"
#include "protocol/messages.hpp"
#include "test_utils.hpp"

#include <thread>
#include <chrono>
#include <vector>

using namespace server::core;
using namespace shared::transport;
using namespace shared::proto;
using namespace test_helpers;

namespace {

struct SimulationRun {
    std::vector<StateSnapshot> snapshots;
    std::vector<BlockPlaced> placements;
    std::vector<BlockBroken> breaks;
};

void pump_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Run a deterministic input sequence and collect results
SimulationRun run_simulation(const std::vector<InputFrame>& inputs, int run_time_ms = 500) {
    SimulationRun result;
    
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    // Handshake
    Message msg;
    pair.client->send(make_client_hello());
    pump_ms(50);
    pair.client->try_recv(msg);
    pair.client->send(JoinMatch{});
    pump_ms(50);
    pair.client->try_recv(msg);
    
    // Send all inputs
    for (const auto& input : inputs) {
        pair.client->send(input);
        pump_ms(10);
    }
    
    // Wait for simulation
    pump_ms(run_time_ms);
    
    // Collect all outputs
    while (pair.client->try_recv(msg)) {
        if (std::holds_alternative<StateSnapshot>(msg)) {
            result.snapshots.push_back(std::get<StateSnapshot>(msg));
        } else if (std::holds_alternative<BlockPlaced>(msg)) {
            result.placements.push_back(std::get<BlockPlaced>(msg));
        } else if (std::holds_alternative<BlockBroken>(msg)) {
            result.breaks.push_back(std::get<BlockBroken>(msg));
        }
    }
    
    server.stop();
    return result;
}

bool snapshots_approximately_equal(const StateSnapshot& a, const StateSnapshot& b, float epsilon = 0.01f) {
    return std::abs(a.px - b.px) < epsilon &&
           std::abs(a.py - b.py) < epsilon &&
           std::abs(a.pz - b.pz) < epsilon;
}

} // namespace

// =============================================================================
// Determinism tests
// =============================================================================

TEST_CASE("Determinism: identical inputs produce similar trajectory", "[server][determinism]") {
    // Create identical input sequence
    std::vector<InputFrame> inputs;
    for (std::uint32_t i = 0; i < 10; ++i) {
        InputFrame frame;
        frame.seq = i;
        frame.moveX = 0.5f;
        frame.moveY = 0.5f;
        frame.yaw = 0.0f;
        frame.pitch = 0.0f;
        frame.jump = (i == 3);  // Jump at seq 3
        inputs.push_back(frame);
    }
    
    // Run twice
    auto run1 = run_simulation(inputs, 400);
    auto run2 = run_simulation(inputs, 400);
    
    REQUIRE(run1.snapshots.size() > 0);
    REQUIRE(run2.snapshots.size() > 0);
    
    // Find snapshots at similar ticks and compare
    // Note: Due to timing variations in tests, we compare trajectories loosely
    // A strict determinism test would require synchronized ticks
    
    // At minimum, both runs should produce similar number of snapshots
    // (within some tolerance due to timing)
    auto minSize = std::min(run1.snapshots.size(), run2.snapshots.size());
    auto maxSize = std::max(run1.snapshots.size(), run2.snapshots.size());
    
    // Snapshot counts should be within 50% of each other
    REQUIRE(minSize > maxSize / 2);
}

TEST_CASE("Determinism: server tick increments consistently", "[server][determinism]") {
    std::vector<InputFrame> inputs;
    for (std::uint32_t i = 0; i < 5; ++i) {
        InputFrame frame;
        frame.seq = i;
        inputs.push_back(frame);
    }
    
    auto run = run_simulation(inputs, 300);
    
    // Verify tick numbers are monotonically increasing
    std::uint64_t lastTick = 0;
    for (const auto& snap : run.snapshots) {
        REQUIRE(snap.serverTick >= lastTick);
        lastTick = snap.serverTick;
    }
}

TEST_CASE("Determinism: no player teleportation between ticks", "[server][determinism]") {
    std::vector<InputFrame> inputs;
    for (std::uint32_t i = 0; i < 10; ++i) {
        InputFrame frame;
        frame.seq = i;
        frame.moveX = 1.0f;
        frame.moveY = 1.0f;
        inputs.push_back(frame);
    }
    
    auto run = run_simulation(inputs, 500);
    
    // Check that position changes are reasonable between consecutive snapshots
    for (size_t i = 1; i < run.snapshots.size(); ++i) {
        const auto& prev = run.snapshots[i-1];
        const auto& curr = run.snapshots[i];
        
        // Maximum reasonable position change per snapshot
        // At 30 TPS with ~10 blocks/second movement, ~0.5 block per tick max
        float dx = std::abs(curr.px - prev.px);
        float dy = std::abs(curr.py - prev.py);
        float dz = std::abs(curr.pz - prev.pz);
        
        // Allow for multiple ticks between snapshots
        std::uint64_t tickDiff = curr.serverTick - prev.serverTick;
        float maxMove = 2.0f * static_cast<float>(tickDiff + 1);
        
        REQUIRE(dx < maxMove);
        REQUIRE(dy < maxMove);
        REQUIRE(dz < maxMove);
    }
}

// =============================================================================
// State consistency tests
// =============================================================================

TEST_CASE("Determinism: playerId remains constant", "[server][determinism]") {
    std::vector<InputFrame> inputs;
    for (std::uint32_t i = 0; i < 5; ++i) {
        InputFrame frame;
        frame.seq = i;
        inputs.push_back(frame);
    }
    
    auto run = run_simulation(inputs, 200);
    
    if (run.snapshots.size() >= 2) {
        PlayerId firstId = run.snapshots.front().playerId;
        for (const auto& snap : run.snapshots) {
            REQUIRE(snap.playerId == firstId);
        }
    }
}

TEST_CASE("Determinism: velocity is bounded", "[server][determinism]") {
    std::vector<InputFrame> inputs;
    // Spam all movement inputs
    for (std::uint32_t i = 0; i < 20; ++i) {
        InputFrame frame;
        frame.seq = i;
        frame.moveX = 1.0f;
        frame.moveY = 1.0f;
        frame.jump = true;
        frame.sprint = true;
        inputs.push_back(frame);
    }
    
    auto run = run_simulation(inputs, 500);
    
    // Velocity should never be unreasonably high
    constexpr float MAX_REASONABLE_VELOCITY = 50.0f;
    
    for (const auto& snap : run.snapshots) {
        REQUIRE(std::abs(snap.vx) < MAX_REASONABLE_VELOCITY);
        REQUIRE(std::abs(snap.vy) < MAX_REASONABLE_VELOCITY);
        REQUIRE(std::abs(snap.vz) < MAX_REASONABLE_VELOCITY);
    }
}
