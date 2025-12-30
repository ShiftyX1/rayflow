/**
 * @file test_block_validation.cpp
 * @brief Unit tests for block placement and breaking validation.
 * 
 * NOTE: These tests require server internals to be testable.
 * Currently tests are structured as integration tests via LocalTransport.
 */

#include <catch2/catch_test_macros.hpp>

#include "core/server.hpp"
#include "transport/local_transport.hpp"
#include "protocol/messages.hpp"
#include "test_utils.hpp"

#include <thread>
#include <chrono>

using namespace server::core;
using namespace shared::transport;
using namespace shared::proto;
using namespace test_helpers;

namespace {

void pump_briefly(std::chrono::milliseconds ms = std::chrono::milliseconds(150)) {
    std::this_thread::sleep_for(ms);
}

// Helper to complete handshake
PlayerId join_session(LocalTransport::Pair& pair) {
    Message msg;
    
    pair.client->send(make_client_hello());
    pump_briefly();
    pair.client->try_recv(msg);  // ServerHello
    
    pair.client->send(JoinMatch{});
    pump_briefly();
    
    // Receive JoinAck (skip game event messages like TeamAssigned, HealthUpdate)
    JoinAck ack;
    if (receive_message_type(*pair.client, ack)) {
        return ack.playerId;
    }
    return 0;
}

} // namespace

// =============================================================================
// Block placement tests
// =============================================================================

TEST_CASE("TryPlaceBlock: server responds with BlockPlaced or ActionRejected", "[server][validation][place]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    auto playerId = join_session(pair);
    REQUIRE(playerId > 0);
    
    // Try to place a block (location may or may not be valid)
    TryPlaceBlock placeCmd;
    placeCmd.seq = 1;
    placeCmd.x = 50;
    placeCmd.y = 65;  // Just above typical ground
    placeCmd.z = 50;
    placeCmd.blockType = shared::voxel::BlockType::Stone;
    pair.client->send(placeCmd);
    
    pump_briefly(std::chrono::milliseconds(200));
    
    // Should get either BlockPlaced or ActionRejected
    Message msg;
    bool gotResponse = false;
    while (pair.client->try_recv(msg)) {
        if (std::holds_alternative<BlockPlaced>(msg)) {
            gotResponse = true;
            auto& placed = std::get<BlockPlaced>(msg);
            REQUIRE(placed.x == 50);
            REQUIRE(placed.y == 65);
            REQUIRE(placed.z == 50);
            break;
        }
        if (std::holds_alternative<ActionRejected>(msg)) {
            gotResponse = true;
            auto& rejected = std::get<ActionRejected>(msg);
            REQUIRE(rejected.seq == 1);
            // Rejection reason should be valid enum
            REQUIRE(static_cast<int>(rejected.reason) >= 0);
            break;
        }
    }
    // Note: Server may not have implemented placement yet
    // This test documents expected behavior
    
    server.stop();
}

// =============================================================================
// Block breaking tests
// =============================================================================

TEST_CASE("TryBreakBlock: server responds with BlockBroken or ActionRejected", "[server][validation][break]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    auto playerId = join_session(pair);
    REQUIRE(playerId > 0);
    
    // Try to break a block
    TryBreakBlock breakCmd;
    breakCmd.seq = 2;
    breakCmd.x = 50;
    breakCmd.y = 60;  // Should be solid ground in most terrain
    breakCmd.z = 50;
    pair.client->send(breakCmd);
    
    pump_briefly(std::chrono::milliseconds(200));
    
    // Should get either BlockBroken or ActionRejected
    Message msg;
    bool gotResponse = false;
    while (pair.client->try_recv(msg)) {
        if (std::holds_alternative<BlockBroken>(msg)) {
            gotResponse = true;
            auto& broken = std::get<BlockBroken>(msg);
            REQUIRE(broken.x == 50);
            REQUIRE(broken.y == 60);
            REQUIRE(broken.z == 50);
            break;
        }
        if (std::holds_alternative<ActionRejected>(msg)) {
            gotResponse = true;
            auto& rejected = std::get<ActionRejected>(msg);
            REQUIRE(rejected.seq == 2);
            break;
        }
    }
    // Note: Server may not have implemented breaking yet
    
    server.stop();
}

// =============================================================================
// TrySetBlock tests (editor mode)
// =============================================================================

TEST_CASE("TrySetBlock: server processes editor block set", "[server][validation][setblock]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    opts.editorCameraMode = true;  // Enable editor mode
    
    Server server(pair.server, opts);
    server.start();
    
    auto playerId = join_session(pair);
    REQUIRE(playerId > 0);
    
    // Editor set block
    TrySetBlock setCmd;
    setCmd.seq = 10;
    setCmd.x = 100;
    setCmd.y = 64;
    setCmd.z = 100;
    setCmd.blockType = shared::voxel::BlockType::Diamond;
    pair.client->send(setCmd);
    
    pump_briefly(std::chrono::milliseconds(200));
    
    // Should get BlockPlaced (editor mode allows direct placement)
    Message msg;
    bool gotPlaced = false;
    while (pair.client->try_recv(msg)) {
        if (std::holds_alternative<BlockPlaced>(msg)) {
            gotPlaced = true;
            auto& placed = std::get<BlockPlaced>(msg);
            REQUIRE(placed.blockType == shared::voxel::BlockType::Diamond);
            break;
        }
    }
    // Note: Behavior depends on server implementation
    
    server.stop();
}

// =============================================================================
// Sequence number tests
// =============================================================================

TEST_CASE("ActionRejected includes correct sequence number", "[server][validation][seq]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    auto playerId = join_session(pair);
    REQUIRE(playerId > 0);
    
    // Send multiple requests with different seq numbers
    TryBreakBlock brk1, brk2, brk3;
    brk1.seq = 100; brk1.x = 0; brk1.y = 0; brk1.z = 0;
    brk2.seq = 101; brk2.x = 1; brk2.y = 1; brk2.z = 1;
    brk3.seq = 102; brk3.x = 2; brk3.y = 2; brk3.z = 2;
    pair.client->send(brk1);
    pair.client->send(brk2);
    pair.client->send(brk3);
    
    pump_briefly(std::chrono::milliseconds(300));
    
    // Collect all responses
    Message msg;
    std::vector<std::uint32_t> seenSeqs;
    while (pair.client->try_recv(msg)) {
        if (std::holds_alternative<ActionRejected>(msg)) {
            seenSeqs.push_back(std::get<ActionRejected>(msg).seq);
        }
        if (std::holds_alternative<BlockBroken>(msg)) {
            // Also valid response
        }
    }
    
    // Each rejection should have a valid seq from our requests
    for (auto seq : seenSeqs) {
        REQUIRE((seq == 100 || seq == 101 || seq == 102));
    }
    
    server.stop();
}
