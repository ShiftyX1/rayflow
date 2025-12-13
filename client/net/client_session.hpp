#pragma once

#include "../../shared/transport/endpoint.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace client::net {

class ClientSession {
public:
    explicit ClientSession(std::shared_ptr<shared::transport::IEndpoint> endpoint);

    void start_handshake();
    void poll();

    void send_input(float moveX, float moveY, float yaw, float pitch, bool jump, bool sprint);

    const std::optional<shared::proto::ServerHello>& server_hello() const { return serverHello_; }
    const std::optional<shared::proto::JoinAck>& join_ack() const { return joinAck_; }
    const std::optional<shared::proto::StateSnapshot>& latest_snapshot() const { return latestSnapshot_; }

private:
    std::shared_ptr<shared::transport::IEndpoint> endpoint_;

    std::uint32_t inputSeq_{0};

    std::optional<shared::proto::ServerHello> serverHello_;
    std::optional<shared::proto::JoinAck> joinAck_;
    std::optional<shared::proto::StateSnapshot> latestSnapshot_;
};

} // namespace client::net
