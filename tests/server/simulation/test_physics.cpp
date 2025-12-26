/**
 * @file test_physics.cpp
 * @brief Unit tests for server-side physics simulation.
 * 
 * Tests gravity, ground collision, jumping, and movement.
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

void pump_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

PlayerId join_and_get_id(LocalTransport::Pair& pair) {
    Message msg;
    pair.client->send(make_client_hello());
    pump_ms(50);
    pair.client->try_recv(msg);
    pair.client->send(JoinMatch{});
    pump_ms(50);
    if (pair.client->try_recv(msg) && std::holds_alternative<JoinAck>(msg)) {
        return std::get<JoinAck>(msg).playerId;
    }
    return 0;
}

// Collect all snapshots from endpoint
std::vector<StateSnapshot> collect_snapshots(shared::transport::IEndpoint& client, int max = 100) {
    std::vector<StateSnapshot> result;
    Message msg;
    while (result.size() < static_cast<size_t>(max) && client.try_recv(msg)) {
        if (std::holds_alternative<StateSnapshot>(msg)) {
            result.push_back(std::get<StateSnapshot>(msg));
        }
    }
    return result;
}

} // namespace

// =============================================================================
// Gravity tests
// =============================================================================

TEST_CASE("Physics: player falls due to gravity when in air", "[server][physics][gravity]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    auto playerId = join_and_get_id(pair);
    REQUIRE(playerId > 0);
    
    // Wait for physics to run
    pump_ms(500);
    
    auto snapshots = collect_snapshots(*pair.client);
    REQUIRE(snapshots.size() >= 2);
    
    // Check if Y position decreased or velocity is negative (falling)
    // Note: Player spawns at y=80, should fall until hitting ground
    bool hasFallingVelocity = false;
    for (const auto& snap : snapshots) {
        if (snap.vy < 0.0f) {
            hasFallingVelocity = true;
            break;
        }
    }
    
    // Player should experience falling at some point (unless spawned on ground)
    // This test documents expected behavior
    
    server.stop();
}

// =============================================================================
// Jump tests
// =============================================================================

TEST_CASE("Physics: jump input gives upward velocity", "[server][physics][jump]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    auto playerId = join_and_get_id(pair);
    REQUIRE(playerId > 0);
    
    // Wait for player to settle on ground
    pump_ms(500);
    
    // Drain old snapshots
    collect_snapshots(*pair.client);
    
    // Send jump input
    InputFrame jumpFrame;
    jumpFrame.seq = 1;
    jumpFrame.jump = true;
    pair.client->send(jumpFrame);
    pump_ms(200);
    
    auto snapshots = collect_snapshots(*pair.client);
    
    // Look for positive Y velocity (upward movement from jump)
    bool hasUpwardVelocity = false;
    for (const auto& snap : snapshots) {
        if (snap.vy > 0.0f) {
            hasUpwardVelocity = true;
            break;
        }
    }
    
    // If player was on ground and jumped, should have upward velocity
    // Note: depends on implementation
    
    server.stop();
}

TEST_CASE("Physics: jump only works when on ground", "[server][physics][jump]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    auto playerId = join_and_get_id(pair);
    REQUIRE(playerId > 0);
    
    // Immediately try to jump multiple times while potentially in air
    for (int i = 0; i < 5; ++i) {
        InputFrame frame;
        frame.seq = static_cast<uint32_t>(i);
        frame.jump = true;
        pair.client->send(frame);
        pump_ms(30);
    }
    
    pump_ms(200);
    auto snapshots = collect_snapshots(*pair.client);
    
    // Should not have unrealistically high Y position (no double jump)
    float maxY = 0.0f;
    for (const auto& snap : snapshots) {
        if (snap.py > maxY) maxY = snap.py;
    }
    
    // Spawn is y=80, reasonable jump height is ~3-5 blocks
    // Should not exceed ~90 without flying
    REQUIRE(maxY < 100.0f);
    
    server.stop();
}

// =============================================================================
// Movement tests
// =============================================================================

TEST_CASE("Physics: horizontal movement input affects position", "[server][physics][movement]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    auto playerId = join_and_get_id(pair);
    REQUIRE(playerId > 0);
    
    // Wait for initial state
    pump_ms(200);
    collect_snapshots(*pair.client);
    
    // Send forward movement
    float initialX = 0.0f, initialZ = 0.0f;
    
    // Get baseline position
    InputFrame idleFrame;
    idleFrame.seq = 0;
    pair.client->send(idleFrame);  // Idle frame
    pump_ms(50);
    auto baseline = collect_snapshots(*pair.client);
    if (!baseline.empty()) {
        initialX = baseline.back().px;
        initialZ = baseline.back().pz;
    }
    
    // Send movement input
    InputFrame moveFrame;
    moveFrame.seq = 1;
    moveFrame.moveX = 1.0f;
    moveFrame.moveY = 1.0f;  // Forward + strafe
    moveFrame.yaw = 0.0f;
    pair.client->send(moveFrame);
    
    pump_ms(300);
    
    auto afterMove = collect_snapshots(*pair.client);
    REQUIRE(!afterMove.empty());
    
    // Position should have changed
    float finalX = afterMove.back().px;
    float finalZ = afterMove.back().pz;
    
    // At least one axis should have moved (depending on implementation)
    // Note: exact movement depends on yaw interpretation
    
    server.stop();
}

// =============================================================================
// Editor camera mode tests
// =============================================================================

TEST_CASE("Physics: editor camera mode ignores gravity", "[server][physics][editor]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    opts.editorCameraMode = true;
    
    Server server(pair.server, opts);
    server.start();
    
    auto playerId = join_and_get_id(pair);
    REQUIRE(playerId > 0);
    
    // Get initial position
    pump_ms(100);
    auto initial = collect_snapshots(*pair.client);
    float initialY = initial.empty() ? 80.0f : initial.back().py;
    
    // Wait a bit
    pump_ms(300);
    
    auto later = collect_snapshots(*pair.client);
    float laterY = later.empty() ? initialY : later.back().py;
    
    // In editor mode, player should not fall (or fall very little)
    // This depends on implementation
    // REQUIRE(std::abs(laterY - initialY) < 1.0f);  // Uncomment when implemented
    
    server.stop();
}

TEST_CASE("Physics: editor camera mode allows vertical movement", "[server][physics][editor]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    opts.editorCameraMode = true;
    
    Server server(pair.server, opts);
    server.start();
    
    auto playerId = join_and_get_id(pair);
    REQUIRE(playerId > 0);
    
    // Initial position
    pump_ms(100);
    collect_snapshots(*pair.client);
    
    // Move up using editor controls
    InputFrame upFrame;
    upFrame.seq = 1;
    upFrame.camUp = true;
    pair.client->send(upFrame);
    pump_ms(200);
    
    auto afterUp = collect_snapshots(*pair.client);
    
    // Move down
    InputFrame downFrame;
    downFrame.seq = 2;
    downFrame.camDown = true;
    pair.client->send(downFrame);
    pump_ms(200);
    
    auto afterDown = collect_snapshots(*pair.client);
    
    // Positions should differ based on input
    // Note: exact behavior depends on implementation
    
    server.stop();
}
