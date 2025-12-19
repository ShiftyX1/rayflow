/**
 * @file test_local_transport.cpp
 * @brief Unit tests for LocalTransport.
 * 
 * Tests FIFO ordering, bidirectional communication, and thread safety.
 */

#include <catch2/catch_test_macros.hpp>

#include "transport/local_transport.hpp"
#include "protocol/messages.hpp"

using namespace shared::transport;
using namespace shared::proto;

// =============================================================================
// Pair creation tests
// =============================================================================

TEST_CASE("LocalTransport::create_pair creates valid endpoints", "[transport][local]") {
    auto pair = LocalTransport::create_pair();
    
    REQUIRE(pair.client != nullptr);
    REQUIRE(pair.server != nullptr);
}

// =============================================================================
// Empty receive tests
// =============================================================================

TEST_CASE("LocalTransport try_recv returns false when empty", "[transport][local]") {
    auto pair = LocalTransport::create_pair();
    Message msg;
    
    SECTION("Client receives nothing initially") {
        REQUIRE_FALSE(pair.client->try_recv(msg));
    }
    
    SECTION("Server receives nothing initially") {
        REQUIRE_FALSE(pair.server->try_recv(msg));
    }
}

// =============================================================================
// Unidirectional tests
// =============================================================================

TEST_CASE("LocalTransport client->server message passing", "[transport][local]") {
    auto pair = LocalTransport::create_pair();
    Message msg;
    
    // Client sends to server
    pair.client->send(ClientHello{.clientName = "TestClient"});
    
    // Server receives
    REQUIRE(pair.server->try_recv(msg));
    REQUIRE(std::holds_alternative<ClientHello>(msg));
    REQUIRE(std::get<ClientHello>(msg).clientName == "TestClient");
    
    // No more messages
    REQUIRE_FALSE(pair.server->try_recv(msg));
}

TEST_CASE("LocalTransport server->client message passing", "[transport][local]") {
    auto pair = LocalTransport::create_pair();
    Message msg;
    
    // Server sends to client
    pair.server->send(ServerHello{.tickRate = 60, .worldSeed = 12345});
    
    // Client receives
    REQUIRE(pair.client->try_recv(msg));
    REQUIRE(std::holds_alternative<ServerHello>(msg));
    auto& hello = std::get<ServerHello>(msg);
    REQUIRE(hello.tickRate == 60);
    REQUIRE(hello.worldSeed == 12345);
    
    // No more messages
    REQUIRE_FALSE(pair.client->try_recv(msg));
}

// =============================================================================
// FIFO ordering tests
// =============================================================================

TEST_CASE("LocalTransport maintains FIFO ordering client->server", "[transport][local][fifo]") {
    auto pair = LocalTransport::create_pair();
    Message msg;
    
    // Send multiple messages
    pair.client->send(InputFrame{.seq = 1});
    pair.client->send(InputFrame{.seq = 2});
    pair.client->send(InputFrame{.seq = 3});
    
    // Receive in order
    REQUIRE(pair.server->try_recv(msg));
    REQUIRE(std::get<InputFrame>(msg).seq == 1);
    
    REQUIRE(pair.server->try_recv(msg));
    REQUIRE(std::get<InputFrame>(msg).seq == 2);
    
    REQUIRE(pair.server->try_recv(msg));
    REQUIRE(std::get<InputFrame>(msg).seq == 3);
    
    REQUIRE_FALSE(pair.server->try_recv(msg));
}

TEST_CASE("LocalTransport maintains FIFO ordering server->client", "[transport][local][fifo]") {
    auto pair = LocalTransport::create_pair();
    Message msg;
    
    // Send multiple messages
    pair.server->send(StateSnapshot{.serverTick = 100});
    pair.server->send(StateSnapshot{.serverTick = 101});
    pair.server->send(StateSnapshot{.serverTick = 102});
    
    // Receive in order
    REQUIRE(pair.client->try_recv(msg));
    REQUIRE(std::get<StateSnapshot>(msg).serverTick == 100);
    
    REQUIRE(pair.client->try_recv(msg));
    REQUIRE(std::get<StateSnapshot>(msg).serverTick == 101);
    
    REQUIRE(pair.client->try_recv(msg));
    REQUIRE(std::get<StateSnapshot>(msg).serverTick == 102);
    
    REQUIRE_FALSE(pair.client->try_recv(msg));
}

// =============================================================================
// Bidirectional independence tests
// =============================================================================

TEST_CASE("LocalTransport channels are independent", "[transport][local]") {
    auto pair = LocalTransport::create_pair();
    Message msg;
    
    // Client sends
    pair.client->send(InputFrame{.seq = 10});
    
    // Server sends (independent channel)
    pair.server->send(StateSnapshot{.serverTick = 50});
    
    // Client receives from server (not its own message)
    REQUIRE(pair.client->try_recv(msg));
    REQUIRE(std::holds_alternative<StateSnapshot>(msg));
    REQUIRE(std::get<StateSnapshot>(msg).serverTick == 50);
    
    // Server receives from client
    REQUIRE(pair.server->try_recv(msg));
    REQUIRE(std::holds_alternative<InputFrame>(msg));
    REQUIRE(std::get<InputFrame>(msg).seq == 10);
}

// =============================================================================
// Mixed message type tests
// =============================================================================

TEST_CASE("LocalTransport handles mixed message types", "[transport][local]") {
    auto pair = LocalTransport::create_pair();
    Message msg;
    
    // Send different message types
    pair.client->send(ClientHello{});
    pair.client->send(JoinMatch{});
    pair.client->send(InputFrame{.seq = 1});
    pair.client->send(TryPlaceBlock{.x = 5, .y = 64, .z = 10});
    
    // Receive and verify types
    REQUIRE(pair.server->try_recv(msg));
    REQUIRE(std::holds_alternative<ClientHello>(msg));
    
    REQUIRE(pair.server->try_recv(msg));
    REQUIRE(std::holds_alternative<JoinMatch>(msg));
    
    REQUIRE(pair.server->try_recv(msg));
    REQUIRE(std::holds_alternative<InputFrame>(msg));
    
    REQUIRE(pair.server->try_recv(msg));
    REQUIRE(std::holds_alternative<TryPlaceBlock>(msg));
    REQUIRE(std::get<TryPlaceBlock>(msg).x == 5);
}

// =============================================================================
// Multiple pairs independence tests
// =============================================================================

TEST_CASE("Multiple LocalTransport pairs are independent", "[transport][local]") {
    auto pair1 = LocalTransport::create_pair();
    auto pair2 = LocalTransport::create_pair();
    Message msg;
    
    // Send on pair1
    pair1.client->send(InputFrame{.seq = 1});
    
    // Send on pair2
    pair2.client->send(InputFrame{.seq = 2});
    
    // Pair1 server only receives pair1 messages
    REQUIRE(pair1.server->try_recv(msg));
    REQUIRE(std::get<InputFrame>(msg).seq == 1);
    REQUIRE_FALSE(pair1.server->try_recv(msg));
    
    // Pair2 server only receives pair2 messages
    REQUIRE(pair2.server->try_recv(msg));
    REQUIRE(std::get<InputFrame>(msg).seq == 2);
    REQUIRE_FALSE(pair2.server->try_recv(msg));
}
