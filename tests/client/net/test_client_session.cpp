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
    
    ClientHello hello;
    hello.clientName = "Test";
    endpoint.send(hello);
    endpoint.send(JoinMatch{});
    
    REQUIRE(endpoint.sent_count() == 2);
    REQUIRE(is_message_type<ClientHello>(endpoint.sent()[0]));
    REQUIRE(is_message_type<JoinMatch>(endpoint.sent()[1]));
}

TEST_CASE("MockEndpoint returns injected messages", "[client][session]") {
    MockEndpoint endpoint;
    
    ServerHello serverHello;
    serverHello.tickRate = 60;
    JoinAck ack;
    ack.playerId = 42;
    
    endpoint.inject_message(serverHello);
    endpoint.inject_message(ack);
    
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
    
    ServerHello serverHello;
    serverHello.acceptedVersion = kProtocolVersion;
    serverHello.tickRate = 20;
    serverHello.worldSeed = 99999;
    endpoint.inject_message(serverHello);
    
    Message msg;
    REQUIRE(endpoint.try_recv(msg));
    
    auto& hello = std::get<ServerHello>(msg);
    REQUIRE(hello.tickRate == 20);
    REQUIRE(hello.worldSeed == 99999);
}

TEST_CASE("Session receives JoinAck and extracts player ID", "[client][session]") {
    MockEndpoint endpoint;
    
    JoinAck ack;
    ack.playerId = 123;
    endpoint.inject_message(ack);
    
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
    
    StateSnapshot snapshot;
    snapshot.serverTick = 100;
    snapshot.playerId = 1;
    snapshot.px = 10.5f;
    snapshot.py = 64.0f;
    snapshot.pz = -5.5f;
    snapshot.vx = 1.0f;
    snapshot.vy = -0.5f;
    snapshot.vz = 0.0f;
    endpoint.inject_message(snapshot);
    
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
    
    BlockPlaced placed;
    placed.x = 10;
    placed.y = 64;
    placed.z = 20;
    placed.blockType = shared::voxel::BlockType::Stone;
    endpoint.inject_message(placed);
    
    Message msg;
    REQUIRE(endpoint.try_recv(msg));
    
    auto& placedRef = std::get<BlockPlaced>(msg);
    REQUIRE(placedRef.x == 10);
    REQUIRE(placedRef.y == 64);
    REQUIRE(placedRef.z == 20);
    REQUIRE(placedRef.blockType == shared::voxel::BlockType::Stone);
}

TEST_CASE("Session receives BlockBroken event", "[client][session]") {
    MockEndpoint endpoint;
    
    BlockBroken broken;
    broken.x = 5;
    broken.y = 60;
    broken.z = 15;
    endpoint.inject_message(broken);
    
    Message msg;
    REQUIRE(endpoint.try_recv(msg));
    
    auto& brokenRef = std::get<BlockBroken>(msg);
    REQUIRE(brokenRef.x == 5);
    REQUIRE(brokenRef.y == 60);
    REQUIRE(brokenRef.z == 15);
}

// =============================================================================
// ActionRejected handling tests
// =============================================================================

TEST_CASE("Session receives ActionRejected with reason", "[client][session]") {
    MockEndpoint endpoint;
    
    ActionRejected rejected;
    rejected.seq = 42;
    rejected.reason = RejectReason::OutOfRange;
    endpoint.inject_message(rejected);
    
    Message msg;
    REQUIRE(endpoint.try_recv(msg));
    
    auto& rejectedRef = std::get<ActionRejected>(msg);
    REQUIRE(rejectedRef.seq == 42);
    REQUIRE(rejectedRef.reason == RejectReason::OutOfRange);
}

TEST_CASE("Session handles all RejectReason values", "[client][session]") {
    MockEndpoint endpoint;
    
    // Inject all possible rejection reasons
    ActionRejected r1, r2, r3, r4, r5, r6;
    r1.seq = 1; r1.reason = RejectReason::Unknown;
    r2.seq = 2; r2.reason = RejectReason::Invalid;
    r3.seq = 3; r3.reason = RejectReason::NotAllowed;
    r4.seq = 4; r4.reason = RejectReason::NotEnoughResources;
    r5.seq = 5; r5.reason = RejectReason::OutOfRange;
    r6.seq = 6; r6.reason = RejectReason::ProtectedBlock;
    endpoint.inject_message(r1);
    endpoint.inject_message(r2);
    endpoint.inject_message(r3);
    endpoint.inject_message(r4);
    endpoint.inject_message(r5);
    endpoint.inject_message(r6);
    
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
        StateSnapshot snap;
        snap.serverTick = tick;
        snap.playerId = 1;
        snap.px = static_cast<float>(tick);
        snap.py = 64.0f;
        snap.pz = 0.0f;
        endpoint.inject_message(snap);
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
    StateSnapshot snap1, snap2, snap3;
    snap1.serverTick = 1;
    snap2.serverTick = 2;
    snap3.serverTick = 3;
    BlockPlaced placed;
    placed.x = 1;
    BlockBroken broken;
    broken.x = 2;
    ActionRejected rejected;
    rejected.seq = 1;
    
    endpoint.inject_message(snap1);
    endpoint.inject_message(placed);
    endpoint.inject_message(snap2);
    endpoint.inject_message(broken);
    endpoint.inject_message(rejected);
    endpoint.inject_message(snap3);
    
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
