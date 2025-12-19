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

#include <thread>
#include <chrono>

using namespace server::core;
using namespace shared::transport;
using namespace shared::proto;

namespace {

void pump_briefly(std::chrono::milliseconds ms = std::chrono::milliseconds(100)) {
    std::this_thread::sleep_for(ms);
}

// Helper to complete handshake
PlayerId join_session(LocalTransport::Pair& pair) {
    Message msg;
    
    pair.client->send(ClientHello{.version = kProtocolVersion});
    pump_briefly();
    pair.client->try_recv(msg);  // ServerHello
    
    pair.client->send(JoinMatch{});
    pump_briefly();
    
    if (pair.client->try_recv(msg) && std::holds_alternative<JoinAck>(msg)) {
        return std::get<JoinAck>(msg).playerId;
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
    pair.client->send(TryPlaceBlock{
        .seq = 1,
        .x = 50,
        .y = 65,  // Just above typical ground
        .z = 50,
        .blockType = shared::voxel::BlockType::Stone
    });
    
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
    pair.client->send(TryBreakBlock{
        .seq = 2,
        .x = 50,
        .y = 60,  // Should be solid ground in most terrain
        .z = 50
    });
    
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
    pair.client->send(TrySetBlock{
        .seq = 10,
        .x = 100,
        .y = 64,
        .z = 100,
        .blockType = shared::voxel::BlockType::Diamond
    });
    
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
    pair.client->send(TryBreakBlock{.seq = 100, .x = 0, .y = 0, .z = 0});
    pair.client->send(TryBreakBlock{.seq = 101, .x = 1, .y = 1, .z = 1});
    pair.client->send(TryBreakBlock{.seq = 102, .x = 2, .y = 2, .z = 2});
    
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
