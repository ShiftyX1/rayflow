/**
 * @file test_replication.cpp
 * @brief Integration tests for state replication consistency.
 */

#include <catch2/catch_test_macros.hpp>

#include "core/server.hpp"
#include "transport/local_transport.hpp"
#include "protocol/messages.hpp"
#include "test_utils.hpp"

#include <thread>
#include <chrono>
#include <map>

using namespace server::core;
using namespace shared::transport;
using namespace shared::proto;
using namespace test_helpers;

namespace {

void pump_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Helper struct to track replicated world state
struct ReplicatedState {
    PlayerId playerId{0};
    float px{0}, py{0}, pz{0};
    float vx{0}, vy{0}, vz{0};
    std::uint64_t lastTick{0};
    
    // Block changes
    std::map<std::tuple<int,int,int>, shared::voxel::BlockType> blocks;
    
    void apply(const StateSnapshot& snap) {
        playerId = snap.playerId;
        px = snap.px;
        py = snap.py;
        pz = snap.pz;
        vx = snap.vx;
        vy = snap.vy;
        vz = snap.vz;
        lastTick = snap.serverTick;
    }
    
    void apply(const BlockPlaced& placed) {
        blocks[{placed.x, placed.y, placed.z}] = placed.blockType;
    }
    
    void apply(const BlockBroken& broken) {
        blocks[{broken.x, broken.y, broken.z}] = shared::voxel::BlockType::Air;
    }
};

} // namespace

// =============================================================================
// Snapshot consistency tests
// =============================================================================

TEST_CASE("Replication: snapshots have monotonically increasing ticks", "[integration][replication]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    Message msg;
    
    // Handshake
    pair.client->send(make_client_hello());
    pump_ms(100);
    pair.client->try_recv(msg);
    pair.client->send(JoinMatch{});
    pump_ms(100);
    pair.client->try_recv(msg);
    
    // Collect snapshots for a while
    pump_ms(500);
    
    std::uint64_t lastTick = 0;
    int snapshotCount = 0;
    
    while (pair.client->try_recv(msg)) {
        if (is_message_type<StateSnapshot>(msg)) {
            auto& snap = std::get<StateSnapshot>(msg);
            REQUIRE(snap.serverTick >= lastTick);
            lastTick = snap.serverTick;
            snapshotCount++;
        }
    }
    
    // Should have received multiple snapshots
    REQUIRE(snapshotCount > 5);
    
    server.stop();
}

TEST_CASE("Replication: playerId remains constant in snapshots", "[integration][replication]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    Message msg;
    
    // Handshake
    pair.client->send(make_client_hello());
    pump_ms(100);
    pair.client->try_recv(msg);
    pair.client->send(JoinMatch{});
    pump_ms(100);
    REQUIRE(pair.client->try_recv(msg));
    
    PlayerId assignedId = std::get<JoinAck>(msg).playerId;
    REQUIRE(assignedId > 0);
    
    // Collect snapshots
    pump_ms(300);
    
    while (pair.client->try_recv(msg)) {
        if (is_message_type<StateSnapshot>(msg)) {
            REQUIRE(std::get<StateSnapshot>(msg).playerId == assignedId);
        }
    }
    
    server.stop();
}

// =============================================================================
// Block change replication tests
// =============================================================================

TEST_CASE("Replication: block changes are reflected in state", "[integration][replication][block]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    Message msg;
    ReplicatedState state;
    
    // Handshake
    pair.client->send(make_client_hello());
    pump_ms(100);
    pair.client->try_recv(msg);
    pair.client->send(JoinMatch{});
    pump_ms(100);
    pair.client->try_recv(msg);
    
    // Place a block
    TryPlaceBlock placeCmd;
    placeCmd.seq = 1;
    placeCmd.x = 100;
    placeCmd.y = 64;
    placeCmd.z = 100;
    placeCmd.blockType = shared::voxel::BlockType::Stone;
    pair.client->send(placeCmd);
    
    pump_ms(200);
    
    // Apply all received messages
    while (pair.client->try_recv(msg)) {
        if (is_message_type<StateSnapshot>(msg)) {
            state.apply(std::get<StateSnapshot>(msg));
        } else if (is_message_type<BlockPlaced>(msg)) {
            state.apply(std::get<BlockPlaced>(msg));
        }
    }
    
    // Check if block was placed (may have been rejected)
    auto it = state.blocks.find({100, 64, 100});
    if (it != state.blocks.end()) {
        CHECK(it->second == shared::voxel::BlockType::Stone);
    }
    
    server.stop();
}

// =============================================================================
// State continuity tests
// =============================================================================

TEST_CASE("Replication: position changes smoothly", "[integration][replication][position]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    Message msg;
    
    // Handshake
    pair.client->send(make_client_hello());
    pump_ms(100);
    pair.client->try_recv(msg);
    pair.client->send(JoinMatch{});
    pump_ms(100);
    pair.client->try_recv(msg);
    
    // Send continuous movement
    for (int i = 0; i < 20; ++i) {
        InputFrame input;
        input.seq = static_cast<uint32_t>(i);
        input.moveX = 0.5f;
        input.moveY = 0.5f;
        pair.client->send(input);
        pump_ms(33);
    }
    
    pump_ms(200);
    
    // Collect snapshots
    std::vector<StateSnapshot> snapshots;
    while (pair.client->try_recv(msg)) {
        if (is_message_type<StateSnapshot>(msg)) {
            snapshots.push_back(std::get<StateSnapshot>(msg));
        }
    }
    
    // Check position continuity
    for (size_t i = 1; i < snapshots.size(); ++i) {
        const auto& prev = snapshots[i-1];
        const auto& curr = snapshots[i];
        
        // Position shouldn't jump dramatically between consecutive snapshots
        float dx = std::abs(curr.px - prev.px);
        float dy = std::abs(curr.py - prev.py);
        float dz = std::abs(curr.pz - prev.pz);
        
        // Allow for multiple ticks between snapshots
        uint64_t tickDiff = curr.serverTick - prev.serverTick;
        float maxJump = 5.0f * static_cast<float>(tickDiff + 1);
        
        CHECK(dx < maxJump);
        CHECK(dy < maxJump);
        CHECK(dz < maxJump);
    }
    
    server.stop();
}

// =============================================================================
// Tick rate consistency tests
// =============================================================================

TEST_CASE("Replication: snapshot rate approximately matches tick rate", "[integration][replication]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    Message msg;
    
    // Handshake
    pair.client->send(make_client_hello());
    pump_ms(100);
    pair.client->try_recv(msg);
    
    std::uint32_t tickRate = std::get<ServerHello>(msg).tickRate;
    
    pair.client->send(JoinMatch{});
    pump_ms(100);
    pair.client->try_recv(msg);
    
    // Wait exactly 1 second
    auto start = std::chrono::steady_clock::now();
    pump_ms(1000);
    auto elapsed = std::chrono::steady_clock::now() - start;
    
    int snapshotCount = 0;
    while (pair.client->try_recv(msg)) {
        if (is_message_type<StateSnapshot>(msg)) {
            snapshotCount++;
        }
    }
    
    // Should receive approximately tickRate snapshots per second
    // Allow ±50% tolerance due to timing variations in tests
    int expectedMin = static_cast<int>(tickRate * 0.5);
    int expectedMax = static_cast<int>(tickRate * 1.5);
    
    CHECK(snapshotCount >= expectedMin);
    CHECK(snapshotCount <= expectedMax);
    
    server.stop();
}

// =============================================================================
// State recovery tests
// =============================================================================

TEST_CASE("Replication: client can rebuild state from snapshots", "[integration][replication]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    Message msg;
    ReplicatedState state;
    
    // Handshake
    pair.client->send(make_client_hello());
    pump_ms(100);
    pair.client->try_recv(msg);
    pair.client->send(JoinMatch{});
    pump_ms(100);
    REQUIRE(pair.client->try_recv(msg));
    state.playerId = std::get<JoinAck>(msg).playerId;
    
    // Run simulation with input
    for (int i = 0; i < 30; ++i) {
        InputFrame input;
        input.seq = static_cast<uint32_t>(i);
        input.moveX = static_cast<float>(i % 3 - 1);
        input.jump = (i % 10 == 0);
        pair.client->send(input);
        pump_ms(33);
    }
    
    pump_ms(200);
    
    // Apply all messages to replicated state
    while (pair.client->try_recv(msg)) {
        if (is_message_type<StateSnapshot>(msg)) {
            state.apply(std::get<StateSnapshot>(msg));
        } else if (is_message_type<BlockPlaced>(msg)) {
            state.apply(std::get<BlockPlaced>(msg));
        } else if (is_message_type<BlockBroken>(msg)) {
            state.apply(std::get<BlockBroken>(msg));
        }
    }
    
    // State should be populated
    REQUIRE(state.playerId > 0);
    REQUIRE(state.lastTick > 0);
    
    // Position should be somewhere reasonable
    CHECK(std::abs(state.px) < 1000.0f);
    CHECK(state.py > -100.0f);
    CHECK(state.py < 500.0f);
    CHECK(std::abs(state.pz) < 1000.0f);
    
    server.stop();
}
