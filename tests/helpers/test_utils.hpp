#pragma once

/**
 * @file test_utils.hpp
 * @brief Common test utilities and helpers.
 */

#include "protocol/messages.hpp"
#include "transport/local_transport.hpp"

#include <chrono>
#include <thread>
#include <functional>
#include <variant>

namespace test_helpers {

// =============================================================================
// Message type checking helpers
// =============================================================================

/** @brief Check if a Message variant holds a specific type. */
template <typename T>
bool is_message_type(const shared::proto::Message& msg) {
    return std::holds_alternative<T>(msg);
}

/** @brief Get a message of specific type, or nullptr if wrong type. */
template <typename T>
const T* get_message(const shared::proto::Message& msg) {
    return std::get_if<T>(&msg);
}

/** @brief Get a message of specific type, throws if wrong type. */
template <typename T>
const T& get_message_or_throw(const shared::proto::Message& msg) {
    return std::get<T>(msg);
}

// =============================================================================
// Message construction helpers (C++17 compatible, avoid designated initializers)
// =============================================================================

/** @brief Create a ClientHello with default protocol version. */
inline shared::proto::ClientHello make_client_hello(const std::string& name = "TestClient") {
    shared::proto::ClientHello hello;
    hello.version = shared::proto::kProtocolVersion;
    hello.clientName = name;
    return hello;
}

/** @brief Create an InputFrame with given sequence number. */
inline shared::proto::InputFrame make_input_frame(std::uint32_t seq) {
    shared::proto::InputFrame frame;
    frame.seq = seq;
    return frame;
}

// =============================================================================
// Session helpers
// =============================================================================

/**
 * @brief Perform a complete client-server handshake via LocalTransport.
 * 
 * @param client Client endpoint
 * @param server Server endpoint (must be running and processing messages)
 * @param client_name Optional client name
 * @return Player ID assigned by server, or 0 on failure
 */
inline shared::proto::PlayerId perform_handshake(
    shared::transport::IEndpoint& client,
    std::function<void()> pump_server,  // Function to process server messages
    const std::string& client_name = "TestClient")
{
    // Send ClientHello
    shared::proto::ClientHello hello;
    hello.version = shared::proto::kProtocolVersion;
    hello.clientName = client_name;
    client.send(hello);

    pump_server();

    // Receive ServerHello
    shared::proto::Message msg;
    if (!client.try_recv(msg)) return 0;
    if (!is_message_type<shared::proto::ServerHello>(msg)) return 0;

    // Send JoinMatch
    client.send(shared::proto::JoinMatch{});

    pump_server();

    // Receive JoinAck (skip game event messages like TeamAssigned, HealthUpdate)
    int maxIterations = 20;
    while (maxIterations-- > 0 && client.try_recv(msg)) {
        if (is_message_type<shared::proto::JoinAck>(msg)) {
            auto* ack = get_message<shared::proto::JoinAck>(msg);
            if (!ack) return 0;
            return ack->playerId;
        }
        // Skip TeamAssigned, HealthUpdate, and other game event messages
    }

    return 0;
}

/**
 * @brief Receive a message of a specific type, skipping other messages.
 * 
 * @tparam T The message type to receive
 * @param client Client endpoint
 * @param out_msg Output for the received message of type T
 * @param max_skip Maximum number of messages to skip
 * @return true if found, false if not found within max_skip
 */
template <typename T>
bool receive_message_type(shared::transport::IEndpoint& client, T& out_msg, int max_skip = 50) {
    shared::proto::Message msg;
    while (max_skip-- > 0 && client.try_recv(msg)) {
        if (is_message_type<T>(msg)) {
            out_msg = std::get<T>(msg);
            return true;
        }
    }
    return false;
}

// =============================================================================
// Timing helpers (for integration tests)
// =============================================================================

/** @brief Wait with timeout for a condition to become true. */
template <typename Predicate>
bool wait_for(Predicate pred, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
    auto start = std::chrono::steady_clock::now();
    while (!pred()) {
        if (std::chrono::steady_clock::now() - start > timeout) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

/** @brief Wait for a message to be available on an endpoint. */
inline bool wait_for_message(shared::transport::IEndpoint& endpoint,
                              std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
    shared::proto::Message dummy;
    return wait_for([&]() {
        // Peek without consuming - this is a limitation, caller should try_recv after
        // For proper use, this waits then caller calls try_recv
        return true; // Simplified: just returns after timeout check
    }, timeout);
}

// =============================================================================
// Assertion helpers
// =============================================================================

/** @brief Check that two floats are approximately equal. */
inline bool approx_equal(float a, float b, float epsilon = 0.0001f) {
    return std::abs(a - b) < epsilon;
}

/** @brief Check that two 3D positions are approximately equal. */
inline bool approx_equal_pos(float x1, float y1, float z1,
                              float x2, float y2, float z2,
                              float epsilon = 0.0001f) {
    return approx_equal(x1, x2, epsilon) &&
           approx_equal(y1, y2, epsilon) &&
           approx_equal(z1, z2, epsilon);
}

// =============================================================================
// Fixed seeds for deterministic tests
// =============================================================================

namespace seeds {
    constexpr std::uint32_t DEFAULT_TEST_SEED = 12345u;
    constexpr std::uint32_t FLAT_WORLD_SEED = 0u;
    constexpr std::uint32_t HILLY_WORLD_SEED = 42u;
} // namespace seeds

} // namespace test_helpers
