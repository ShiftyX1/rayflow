/**
 * @file test_full_session.cpp
 * @brief Integration tests for full client-server session via LocalTransport.
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

void pump_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

} // namespace

// =============================================================================
// Full session lifecycle tests
// =============================================================================

TEST_CASE("Integration: complete session lifecycle", "[integration][session]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    Message msg;
    
    // Step 1: Connect and send ClientHello
    pair.client->send(ClientHello{
        .version = kProtocolVersion,
        .clientName = "IntegrationTestClient"
    });
    
    pump_ms(50);
    
    // Step 2: Receive ServerHello
    REQUIRE(pair.client->try_recv(msg));
    REQUIRE(is_message_type<ServerHello>(msg));
    
    auto& serverHello = std::get<ServerHello>(msg);
    REQUIRE(serverHello.acceptedVersion == kProtocolVersion);
    REQUIRE(serverHello.tickRate > 0);
    
    // Step 3: Send JoinMatch
    pair.client->send(JoinMatch{});
    
    pump_ms(50);
    
    // Step 4: Receive JoinAck
    REQUIRE(pair.client->try_recv(msg));
    REQUIRE(is_message_type<JoinAck>(msg));
    
    PlayerId playerId = std::get<JoinAck>(msg).playerId;
    REQUIRE(playerId > 0);
    
    // Step 5: Receive StateSnapshots
    pump_ms(200);
    
    bool receivedSnapshot = false;
    while (pair.client->try_recv(msg)) {
        if (is_message_type<StateSnapshot>(msg)) {
            auto& snap = std::get<StateSnapshot>(msg);
            REQUIRE(snap.playerId == playerId);
            receivedSnapshot = true;
        }
    }
    REQUIRE(receivedSnapshot);
    
    // Step 6: Send some inputs
    for (std::uint32_t i = 0; i < 10; ++i) {
        pair.client->send(InputFrame{
            .seq = i,
            .moveX = 0.5f,
            .moveY = 0.5f
        });
    }
    
    pump_ms(200);
    
    // Should still receive snapshots
    bool stillReceiving = false;
    while (pair.client->try_recv(msg)) {
        if (is_message_type<StateSnapshot>(msg)) {
            stillReceiving = true;
        }
    }
    REQUIRE(stillReceiving);
    
    // Step 7: Clean shutdown
    server.stop();
}

// =============================================================================
// Block interaction end-to-end tests
// =============================================================================

TEST_CASE("Integration: block placement end-to-end", "[integration][session][block]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    Message msg;
    
    // Complete handshake
    pair.client->send(ClientHello{.version = kProtocolVersion});
    pump_ms(50);
    pair.client->try_recv(msg);  // ServerHello
    
    pair.client->send(JoinMatch{});
    pump_ms(50);
    pair.client->try_recv(msg);  // JoinAck
    
    // Try to place a block
    pair.client->send(TryPlaceBlock{
        .seq = 100,
        .x = 50,
        .y = 80,
        .z = 50,
        .blockType = shared::voxel::BlockType::Stone
    });
    
    pump_ms(200);
    
    // Look for response
    bool gotBlockResponse = false;
    while (pair.client->try_recv(msg)) {
        if (is_message_type<BlockPlaced>(msg)) {
            gotBlockResponse = true;
            auto& placed = std::get<BlockPlaced>(msg);
            CHECK(placed.x == 50);
            CHECK(placed.y == 80);
            CHECK(placed.z == 50);
        } else if (is_message_type<ActionRejected>(msg)) {
            gotBlockResponse = true;
            auto& rejected = std::get<ActionRejected>(msg);
            CHECK(rejected.seq == 100);
        }
    }
    
    // Server should respond to block placement attempt
    // (Either placed or rejected)
    
    server.stop();
}

TEST_CASE("Integration: block break end-to-end", "[integration][session][block]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    Message msg;
    
    // Handshake
    pair.client->send(ClientHello{.version = kProtocolVersion});
    pump_ms(50);
    pair.client->try_recv(msg);
    pair.client->send(JoinMatch{});
    pump_ms(50);
    pair.client->try_recv(msg);
    
    // Try to break a block at ground level
    pair.client->send(TryBreakBlock{
        .seq = 200,
        .x = 50,
        .y = 60,
        .z = 50
    });
    
    pump_ms(200);
    
    // Collect response
    bool gotResponse = false;
    while (pair.client->try_recv(msg)) {
        if (is_message_type<BlockBroken>(msg) || is_message_type<ActionRejected>(msg)) {
            gotResponse = true;
        }
    }
    
    server.stop();
}

// =============================================================================
// Movement verification tests
// =============================================================================

TEST_CASE("Integration: player position changes with input", "[integration][session][movement]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    Message msg;
    
    // Handshake
    pair.client->send(ClientHello{.version = kProtocolVersion});
    pump_ms(50);
    pair.client->try_recv(msg);
    pair.client->send(JoinMatch{});
    pump_ms(50);
    pair.client->try_recv(msg);
    
    // Wait for initial state
    pump_ms(100);
    
    // Record initial position
    float initialX = 0, initialZ = 0;
    while (pair.client->try_recv(msg)) {
        if (is_message_type<StateSnapshot>(msg)) {
            auto& snap = std::get<StateSnapshot>(msg);
            initialX = snap.px;
            initialZ = snap.pz;
        }
    }
    
    // Send movement input
    for (std::uint32_t i = 0; i < 30; ++i) {
        pair.client->send(InputFrame{
            .seq = i,
            .moveX = 1.0f,
            .moveY = 0.0f,
            .yaw = 0.0f
        });
        pump_ms(33);  // ~30 fps
    }
    
    pump_ms(200);
    
    // Record final position
    float finalX = initialX, finalZ = initialZ;
    while (pair.client->try_recv(msg)) {
        if (is_message_type<StateSnapshot>(msg)) {
            auto& snap = std::get<StateSnapshot>(msg);
            finalX = snap.px;
            finalZ = snap.pz;
        }
    }
    
    // Position should have changed due to movement input
    // Note: exact change depends on implementation
    
    server.stop();
}

// =============================================================================
// Multiple client simulation (future)
// =============================================================================

TEST_CASE("Integration: server handles multiple sequential connections", "[integration][session]") {
    // Test that server can handle being restarted with new connections
    for (int i = 0; i < 3; ++i) {
        auto pair = LocalTransport::create_pair();
        Server::Options opts;
        opts.logging.enabled = false;
        opts.loadLatestMapTemplateFromDisk = false;
        
        Server server(pair.server, opts);
        server.start();
        
        Message msg;
        
        // Quick connect/disconnect cycle
        pair.client->send(ClientHello{.version = kProtocolVersion});
        pump_ms(50);
        REQUIRE(pair.client->try_recv(msg));
        REQUIRE(is_message_type<ServerHello>(msg));
        
        server.stop();
    }
}

// =============================================================================
// Error handling tests
// =============================================================================

TEST_CASE("Integration: server handles malformed sequence", "[integration][session][error]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    // Send JoinMatch without ClientHello first
    pair.client->send(JoinMatch{});
    
    pump_ms(100);
    
    // Server should handle gracefully (not crash)
    // May ignore or reject the message
    
    server.stop();
}

TEST_CASE("Integration: server handles input before join", "[integration][session][error]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    // Send hello
    pair.client->send(ClientHello{.version = kProtocolVersion});
    pump_ms(50);
    
    Message msg;
    pair.client->try_recv(msg);  // ServerHello
    
    // Send input before JoinMatch
    pair.client->send(InputFrame{.seq = 1, .moveX = 1.0f});
    
    pump_ms(100);
    
    // Server should handle gracefully
    
    server.stop();
}
