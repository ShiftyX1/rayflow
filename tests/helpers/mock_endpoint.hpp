#pragma once

/**
 * @file mock_endpoint.hpp
 * @brief Mock transport endpoint for testing.
 * 
 * Provides a controllable endpoint implementation for unit testing
 * components that depend on IEndpoint without using LocalTransport.
 */

#include "transport/endpoint.hpp"
#include "protocol/messages.hpp"

#include <queue>
#include <vector>

namespace test_helpers {

/**
 * @brief Mock endpoint that records sent messages and allows injecting received messages.
 * 
 * Usage:
 *   MockEndpoint endpoint;
 *   endpoint.inject_message(SomeMessage{});  // Simulate incoming
 *   component.process(&endpoint);
 *   REQUIRE(endpoint.sent().size() == 1);    // Check outgoing
 */
class MockEndpoint final : public shared::transport::IEndpoint {
public:
    void send(shared::proto::Message msg) override {
        sent_messages_.push_back(std::move(msg));
    }

    bool try_recv(shared::proto::Message& outMsg) override {
        if (incoming_queue_.empty()) {
            return false;
        }
        outMsg = std::move(incoming_queue_.front());
        incoming_queue_.pop();
        return true;
    }

    // =========================================================================
    // Test helpers
    // =========================================================================

    /** @brief Inject a message to be received by the component under test. */
    void inject_message(shared::proto::Message msg) {
        incoming_queue_.push(std::move(msg));
    }

    /** @brief Get all messages sent by the component under test. */
    const std::vector<shared::proto::Message>& sent() const {
        return sent_messages_;
    }

    /** @brief Get mutable access to sent messages (for extracting). */
    std::vector<shared::proto::Message>& sent() {
        return sent_messages_;
    }

    /** @brief Clear all sent messages and incoming queue. */
    void clear() {
        sent_messages_.clear();
        while (!incoming_queue_.empty()) {
            incoming_queue_.pop();
        }
    }

    /** @brief Number of messages waiting to be received. */
    std::size_t pending_count() const {
        return incoming_queue_.size();
    }

    /** @brief Number of messages that were sent. */
    std::size_t sent_count() const {
        return sent_messages_.size();
    }

private:
    std::vector<shared::proto::Message> sent_messages_;
    std::queue<shared::proto::Message> incoming_queue_;
};

} // namespace test_helpers
