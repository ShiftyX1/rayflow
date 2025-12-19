/**
 * @file test_client_session.cpp
 * @brief Unit tests for ClientSession message handling.
 * 
 * NOTE: These tests use MockEndpoint to avoid actual server dependency.
 */

#include <catch2/catch_test_macros.hpp>

#include "mock_endpoint.hpp"
#include "test_utils.hpp"

#include "protocol/messages.hpp"

using namespace shared::proto;
using namespace test_helpers;

// =============================================================================
// Message sending tests (using MockEndpoint)
// =============================================================================

TEST_CASE("MockEndpoint records sent messages", "[client][session]") {
    MockEndpoint endpoint;
    
    endpoint.send(ClientHello{.clientName = "Test"});
    endpoint.send(JoinMatch{});
    
    REQUIRE(endpoint.sent_count() == 2);
    REQUIRE(is_message_type<ClientHello>(endpoint.sent()[0]));
    REQUIRE(is_message_type<JoinMatch>(endpoint.sent()[1]));
}

TEST_CASE("MockEndpoint returns injected messages", "[client][session]") {
    MockEndpoint endpoint;
    
    endpoint.inject_message(ServerHello{.tickRate = 60});
    endpoint.inject_message(JoinAck{.playerId = 42});
    
    Message msg;
    
    REQUIRE(endpoint.try_recv(msg));
    REQUIRE(is_message_type<ServerHello>(msg));
    REQUIRE(std::get<ServerHello>(msg).tickRate == 60);
    
    REQUIRE(endpoint.try_recv(msg));
    REQUIRE(is_message_type<JoinAck>(msg));
    REQUIRE(std::get<JoinAck>(msg).playerId == 42);
    
    REQUIRE_FALSE(endpoint.try_recv(msg));
}

// =============================================================================
// Session state tests
// =============================================================================

TEST_CASE("Session receives ServerHello and extracts tick rate", "[client][session]") {
    MockEndpoint endpoint;
    
    endpoint.inject_message(ServerHello{
        .acceptedVersion = kProtocolVersion,
        .tickRate = 20,
        .worldSeed = 99999
    });
    
    Message msg;
    REQUIRE(endpoint.try_recv(msg));
    
    auto& hello = std::get<ServerHello>(msg);
    REQUIRE(hello.tickRate == 20);
    REQUIRE(hello.worldSeed == 99999);
}

TEST_CASE("Session receives JoinAck and extracts player ID", "[client][session]") {
    MockEndpoint endpoint;
    
    endpoint.inject_message(JoinAck{.playerId = 123});
    
    Message msg;
    REQUIRE(endpoint.try_recv(msg));
    
    auto playerId = std::get<JoinAck>(msg).playerId;
    REQUIRE(playerId == 123);
}

// =============================================================================
// StateSnapshot handling tests
// =============================================================================

TEST_CASE("Session receives StateSnapshot with position", "[client][session]") {
    MockEndpoint endpoint;
    
    endpoint.inject_message(StateSnapshot{
        .serverTick = 100,
        .playerId = 1,
        .px = 10.5f,
        .py = 64.0f,
        .pz = -5.5f,
        .vx = 1.0f,
        .vy = -0.5f,
        .vz = 0.0f
    });
    
    Message msg;
    REQUIRE(endpoint.try_recv(msg));
    
    auto& snap = std::get<StateSnapshot>(msg);
    REQUIRE(snap.serverTick == 100);
    REQUIRE(snap.px == 10.5f);
    REQUIRE(snap.py == 64.0f);
    REQUIRE(snap.pz == -5.5f);
    REQUIRE(snap.vx == 1.0f);
    REQUIRE(snap.vy == -0.5f);
    REQUIRE(snap.vz == 0.0f);
}

// =============================================================================
// Block event handling tests
// =============================================================================

TEST_CASE("Session receives BlockPlaced event", "[client][session]") {
    MockEndpoint endpoint;
    
    endpoint.inject_message(BlockPlaced{
        .x = 10,
        .y = 64,
        .z = 20,
        .blockType = shared::voxel::BlockType::Stone
    });
    
    Message msg;
    REQUIRE(endpoint.try_recv(msg));
    
    auto& placed = std::get<BlockPlaced>(msg);
    REQUIRE(placed.x == 10);
    REQUIRE(placed.y == 64);
    REQUIRE(placed.z == 20);
    REQUIRE(placed.blockType == shared::voxel::BlockType::Stone);
}

TEST_CASE("Session receives BlockBroken event", "[client][session]") {
    MockEndpoint endpoint;
    
    endpoint.inject_message(BlockBroken{
        .x = 5,
        .y = 60,
        .z = 15
    });
    
    Message msg;
    REQUIRE(endpoint.try_recv(msg));
    
    auto& broken = std::get<BlockBroken>(msg);
    REQUIRE(broken.x == 5);
    REQUIRE(broken.y == 60);
    REQUIRE(broken.z == 15);
}

// =============================================================================
// ActionRejected handling tests
// =============================================================================

TEST_CASE("Session receives ActionRejected with reason", "[client][session]") {
    MockEndpoint endpoint;
    
    endpoint.inject_message(ActionRejected{
        .seq = 42,
        .reason = RejectReason::OutOfRange
    });
    
    Message msg;
    REQUIRE(endpoint.try_recv(msg));
    
    auto& rejected = std::get<ActionRejected>(msg);
    REQUIRE(rejected.seq == 42);
    REQUIRE(rejected.reason == RejectReason::OutOfRange);
}

TEST_CASE("Session handles all RejectReason values", "[client][session]") {
    MockEndpoint endpoint;
    
    // Inject all possible rejection reasons
    endpoint.inject_message(ActionRejected{.seq = 1, .reason = RejectReason::Unknown});
    endpoint.inject_message(ActionRejected{.seq = 2, .reason = RejectReason::Invalid});
    endpoint.inject_message(ActionRejected{.seq = 3, .reason = RejectReason::NotAllowed});
    endpoint.inject_message(ActionRejected{.seq = 4, .reason = RejectReason::NotEnoughResources});
    endpoint.inject_message(ActionRejected{.seq = 5, .reason = RejectReason::OutOfRange});
    endpoint.inject_message(ActionRejected{.seq = 6, .reason = RejectReason::ProtectedBlock});
    
    Message msg;
    for (int i = 0; i < 6; ++i) {
        REQUIRE(endpoint.try_recv(msg));
        REQUIRE(is_message_type<ActionRejected>(msg));
    }
}

// =============================================================================
// Message sequence tests
// =============================================================================

TEST_CASE("Session can handle rapid message sequence", "[client][session]") {
    MockEndpoint endpoint;
    
    // Simulate rapid server updates
    for (std::uint64_t tick = 0; tick < 100; ++tick) {
        endpoint.inject_message(StateSnapshot{
            .serverTick = tick,
            .playerId = 1,
            .px = static_cast<float>(tick),
            .py = 64.0f,
            .pz = 0.0f
        });
    }
    
    REQUIRE(endpoint.pending_count() == 100);
    
    Message msg;
    std::uint64_t lastTick = 0;
    int count = 0;
    while (endpoint.try_recv(msg)) {
        auto& snap = std::get<StateSnapshot>(msg);
        REQUIRE(snap.serverTick >= lastTick);
        lastTick = snap.serverTick;
        count++;
    }
    
    REQUIRE(count == 100);
}

TEST_CASE("Session handles interleaved message types", "[client][session]") {
    MockEndpoint endpoint;
    
    // Mix of message types
    endpoint.inject_message(StateSnapshot{.serverTick = 1});
    endpoint.inject_message(BlockPlaced{.x = 1});
    endpoint.inject_message(StateSnapshot{.serverTick = 2});
    endpoint.inject_message(BlockBroken{.x = 2});
    endpoint.inject_message(ActionRejected{.seq = 1});
    endpoint.inject_message(StateSnapshot{.serverTick = 3});
    
    Message msg;
    
    REQUIRE(endpoint.try_recv(msg));
    REQUIRE(is_message_type<StateSnapshot>(msg));
    
    REQUIRE(endpoint.try_recv(msg));
    REQUIRE(is_message_type<BlockPlaced>(msg));
    
    REQUIRE(endpoint.try_recv(msg));
    REQUIRE(is_message_type<StateSnapshot>(msg));
    
    REQUIRE(endpoint.try_recv(msg));
    REQUIRE(is_message_type<BlockBroken>(msg));
    
    REQUIRE(endpoint.try_recv(msg));
    REQUIRE(is_message_type<ActionRejected>(msg));
    
    REQUIRE(endpoint.try_recv(msg));
    REQUIRE(is_message_type<StateSnapshot>(msg));
}
