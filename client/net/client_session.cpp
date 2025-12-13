#include "client_session.hpp"

#include <raylib.h>

namespace client::net {

ClientSession::ClientSession(std::shared_ptr<shared::transport::IEndpoint> endpoint)
    : endpoint_(std::move(endpoint)) {}

void ClientSession::start_handshake() {
    shared::proto::ClientHello hello;
    hello.version = shared::proto::kProtocolVersion;
    hello.clientName = "local-client";
    endpoint_->send(std::move(hello));

    endpoint_->send(shared::proto::JoinMatch{});
}

void ClientSession::send_input(float moveX, float moveY, float yaw, float pitch, bool jump, bool sprint) {
    shared::proto::InputFrame frame;
    frame.seq = ++inputSeq_;
    frame.moveX = moveX;
    frame.moveY = moveY;
    frame.yaw = yaw;
    frame.pitch = pitch;
    frame.jump = jump;
    frame.sprint = sprint;

    endpoint_->send(std::move(frame));
}

void ClientSession::poll() {
    shared::proto::Message msg;
    while (endpoint_->try_recv(msg)) {
        if (std::holds_alternative<shared::proto::ServerHello>(msg)) {
            serverHello_ = std::get<shared::proto::ServerHello>(msg);
            TraceLog(LOG_INFO, "[net] ServerHello: tickRate=%u", serverHello_->tickRate);
        } else if (std::holds_alternative<shared::proto::JoinAck>(msg)) {
            joinAck_ = std::get<shared::proto::JoinAck>(msg);
            TraceLog(LOG_INFO, "[net] JoinAck: playerId=%u", joinAck_->playerId);
        } else if (std::holds_alternative<shared::proto::StateSnapshot>(msg)) {
            latestSnapshot_ = std::get<shared::proto::StateSnapshot>(msg);
        } else if (std::holds_alternative<shared::proto::ActionRejected>(msg)) {
            const auto& rej = std::get<shared::proto::ActionRejected>(msg);
            TraceLog(LOG_WARNING, "[net] ActionRejected: seq=%u reason=%u", rej.seq, static_cast<unsigned>(rej.reason));
        }
    }
}

} // namespace client::net
