/**
 * @file test_session_flow.cpp
 * @brief Unit tests for server session flow (handshake, join).
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

// Helper to run server for a short time
void pump_server_briefly(std::chrono::milliseconds duration = std::chrono::milliseconds(100)) {
    std::this_thread::sleep_for(duration);
}

} // namespace

// =============================================================================
// Handshake tests
// =============================================================================

TEST_CASE("Server responds to ClientHello with ServerHello", "[server][session]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    // Client sends hello
    ClientHello clientHello;
    clientHello.version = kProtocolVersion;
    clientHello.clientName = "TestClient";
    pair.client->send(clientHello);
    
    pump_server_briefly();
    
    // Client should receive ServerHello
    Message msg;
    REQUIRE(pair.client->try_recv(msg));
    REQUIRE(std::holds_alternative<ServerHello>(msg));
    
    auto& serverHello = std::get<ServerHello>(msg);
    REQUIRE(serverHello.acceptedVersion == kProtocolVersion);
    REQUIRE(serverHello.tickRate == 30);
    
    server.stop();
}

TEST_CASE("Server responds to JoinMatch with JoinAck", "[server][session]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    // Complete handshake
    pair.client->send(make_client_hello());
    pump_server_briefly();
    
    Message msg;
    REQUIRE(pair.client->try_recv(msg));
    REQUIRE(std::holds_alternative<ServerHello>(msg));
    
    // Send join
    pair.client->send(JoinMatch{});
    pump_server_briefly();
    
    // Should receive JoinAck (may need to skip game event messages like TeamAssigned, HealthUpdate)
    JoinAck ack;
    REQUIRE(receive_message_type(*pair.client, ack));
    REQUIRE(ack.playerId > 0);
    
    server.stop();
}

// =============================================================================
// Full session flow tests
// =============================================================================

TEST_CASE("Full session: Hello -> ServerHello -> Join -> JoinAck -> Snapshots", "[server][session]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    Message msg;
    
    // Step 1: ClientHello
    ClientHello helloMsg;
    helloMsg.version = kProtocolVersion;
    helloMsg.clientName = "IntegrationTest";
    pair.client->send(helloMsg);
    pump_server_briefly();
    
    // Step 2: ServerHello
    REQUIRE(pair.client->try_recv(msg));
    REQUIRE(std::holds_alternative<ServerHello>(msg));
    
    // Step 3: JoinMatch
    pair.client->send(JoinMatch{});
    pump_server_briefly();
    
    // Step 4: JoinAck (skip game event messages like TeamAssigned, HealthUpdate)
    JoinAck ack;
    REQUIRE(receive_message_type(*pair.client, ack));
    auto playerId = ack.playerId;
    REQUIRE(playerId > 0);
    
    // Step 5: Wait for StateSnapshot
    // Use longer timeout for slower CI runners
    pump_server_briefly(std::chrono::milliseconds(500));
    
    // Drain any extra messages, look for StateSnapshot
    // Note: server may send ChunkData messages after JoinAck, skip those
    bool foundSnapshot = false;
    int maxIterations = 200;  // Increased for ChunkData messages
    while (maxIterations-- > 0 && pair.client->try_recv(msg)) {
        if (std::holds_alternative<StateSnapshot>(msg)) {
            foundSnapshot = true;
            auto& snap = std::get<StateSnapshot>(msg);
            REQUIRE(snap.playerId == playerId);
            REQUIRE(snap.serverTick > 0);
            break;
        }
        // Skip ChunkData and other messages
    }
    REQUIRE(foundSnapshot);
    
    server.stop();
}

// =============================================================================
// InputFrame handling tests
// =============================================================================

TEST_CASE("Server processes InputFrame after join", "[server][session][input]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    server.start();
    
    Message msg;
    
    // Complete handshake
    pair.client->send(make_client_hello());
    pump_server_briefly();
    pair.client->try_recv(msg);  // ServerHello
    
    pair.client->send(JoinMatch{});
    pump_server_briefly();
    pair.client->try_recv(msg);  // JoinAck
    
    // Send input
    InputFrame input;
    input.seq = 1;
    input.moveX = 1.0f;
    input.moveY = 0.0f;
    input.jump = false;
    pair.client->send(input);
    
    pump_server_briefly(std::chrono::milliseconds(200));
    
    // Should still receive snapshots (server didn't crash on input)
    bool gotSnapshot = false;
    while (pair.client->try_recv(msg)) {
        if (std::holds_alternative<StateSnapshot>(msg)) {
            gotSnapshot = true;
        }
    }
    REQUIRE(gotSnapshot);
    
    server.stop();
}
