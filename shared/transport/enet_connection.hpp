#pragma once

#include "endpoint.hpp"
#include "enet_common.hpp"

#include <enet/enet.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <queue>


namespace shared::transport {


class ENetConnection : public IEndpoint {
public:
    enum class State {
        Connecting,
        Connected,
        Disconnecting,
        Disconnected
    };

    explicit ENetConnection(ENetPeer* peer);
    ~ENetConnection();

    ENetConnection(const ENetConnection&) = delete;
    ENetConnection& operator=(const ENetConnection&) = delete;

    void send(shared::proto::Message msg) override;
    bool try_recv(shared::proto::Message& outMsg) override;

    State state() const { return state_.load(std::memory_order_acquire); }
    bool is_connected() const { return state() == State::Connected; }
    void disconnect();
    void disconnect_now();

    std::uint32_t ping_ms() const;
    std::uint32_t packet_loss_percent() const;
    std::uint64_t bytes_sent() const { return bytesSent_.load(); }
    std::uint64_t bytes_received() const { return bytesRecv_.load(); }

    ENetPeer* peer() const { return peer_; }
    void on_connect();
    void on_disconnect();
    void on_receive(const std::uint8_t* data, std::size_t size);

private:
    ENetPeer* peer_{nullptr};
    std::atomic<State> state_{State::Connecting};

    std::queue<shared::proto::Message> recvQueue_;
    mutable std::mutex recvMutex_;

    std::atomic<std::uint64_t> bytesSent_{0};
    std::atomic<std::uint64_t> bytesRecv_{0};
};

} // namespace shared::transport