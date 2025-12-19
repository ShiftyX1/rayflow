/**
 * @file test_server_lifecycle.cpp
 * @brief Unit tests for server lifecycle (start/stop).
 */

#include <catch2/catch_test_macros.hpp>

#include "core/server.hpp"
#include "transport/local_transport.hpp"

#include <thread>
#include <chrono>

using namespace server::core;
using namespace shared::transport;

// =============================================================================
// Server construction tests
// =============================================================================

TEST_CASE("Server can be constructed with endpoint", "[server][lifecycle]") {
    auto pair = LocalTransport::create_pair();
    Server server(pair.server);
    // No crash, server exists
}

TEST_CASE("Server can be constructed with options", "[server][lifecycle]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    Server server(pair.server, opts);
    // No crash, server exists with options
}

// =============================================================================
// Server start/stop tests
// =============================================================================

TEST_CASE("Server starts and stops cleanly", "[server][lifecycle]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    
    server.start();
    
    // Give server thread time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    server.stop();
    
    // Server stopped without crash
}

TEST_CASE("Server can be stopped immediately after start", "[server][lifecycle]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    
    server.start();
    server.stop();
    
    // No deadlock or crash
}

TEST_CASE("Server stop is idempotent", "[server][lifecycle]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    
    Server server(pair.server, opts);
    
    server.start();
    server.stop();
    server.stop();  // Second stop should be safe
    
    // No crash
}

// =============================================================================
// Server options tests
// =============================================================================

TEST_CASE("Server respects logging options", "[server][lifecycle][options]") {
    auto pair = LocalTransport::create_pair();
    
    SECTION("All logging enabled") {
        Server::Options opts;
        opts.logging.enabled = true;
        opts.logging.init = true;
        opts.logging.rx = true;
        opts.logging.tx = true;
        opts.logging.move = true;
        opts.logging.coll = true;
        opts.loadLatestMapTemplateFromDisk = false;
        
        Server server(pair.server, opts);
        server.start();
        server.stop();
    }
    
    SECTION("All logging disabled") {
        Server::Options opts;
        opts.logging.enabled = false;
        opts.loadLatestMapTemplateFromDisk = false;
        
        Server server(pair.server, opts);
        server.start();
        server.stop();
    }
}

TEST_CASE("Server respects editorCameraMode option", "[server][lifecycle][options]") {
    auto pair = LocalTransport::create_pair();
    Server::Options opts;
    opts.logging.enabled = false;
    opts.loadLatestMapTemplateFromDisk = false;
    opts.editorCameraMode = true;
    
    Server server(pair.server, opts);
    server.start();
    server.stop();
}
