#include "enet_connection.hpp"

namespace shared::transport {

ENetConnection::ENetConnection(ENetPeer* peer)
    : peer_(peer) {
    // Peer starts in connecting state until on_connect() is called
}

ENetConnection::~ENetConnection() {
    // If still connected, force disconnect
    if (peer_ && state_.load() != State::Disconnected) {
        disconnect_now();
    }
}

// =============================================================================
// IEndpoint interface
// =============================================================================

void ENetConnection::send(shared::proto::Message msg) {
    if (!peer_ || state_.load() != State::Connected) {
        return;
    }
    
    // Serialize message
    std::vector<std::uint8_t> data = serialize_message(msg);
    
    // Get channel and flags for this message type
    ENetChannel channel = get_channel_for_message(msg);
    enet_uint32 flags = get_packet_flags_for_message(msg);
    
    // Create ENet packet
    ENetPacket* packet = enet_packet_create(
        data.data(),
        data.size(),
        flags
    );
    
    if (!packet) {
        return;
    }
    
    // Send on appropriate channel
    if (enet_peer_send(peer_, static_cast<enet_uint8>(channel), packet) < 0) {
        // Send failed, packet is destroyed by ENet
        return;
    }
    
    // Track bytes sent
    bytesSent_.fetch_add(data.size(), std::memory_order_relaxed);
}

bool ENetConnection::try_recv(shared::proto::Message& outMsg) {
    std::lock_guard<std::mutex> lock(recvMutex_);
    
    if (recvQueue_.empty()) {
        return false;
    }
    
    outMsg = std::move(recvQueue_.front());
    recvQueue_.pop();
    return true;
}

// =============================================================================
// Connection management
// =============================================================================

void ENetConnection::disconnect() {
    if (!peer_) return;
    
    State expected = State::Connected;
    if (state_.compare_exchange_strong(expected, State::Disconnecting)) {
        // Graceful disconnect - ENet will try to deliver pending packets
        enet_peer_disconnect(peer_, 0);
    }
}

void ENetConnection::disconnect_now() {
    if (!peer_) return;
    
    state_.store(State::Disconnected, std::memory_order_release);
    
    // Immediate disconnect - no graceful shutdown
    enet_peer_disconnect_now(peer_, 0);
    peer_ = nullptr;
}

// =============================================================================
// Statistics
// =============================================================================

std::uint32_t ENetConnection::ping_ms() const {
    if (!peer_) return 0;
    return peer_->roundTripTime;
}

std::uint32_t ENetConnection::packet_loss_percent() const {
    if (!peer_) return 0;
    // ENet stores packet loss as percentage * ENET_PEER_PACKET_LOSS_SCALE
    return peer_->packetLoss / (ENET_PEER_PACKET_LOSS_SCALE / 100);
}

// =============================================================================
// Internal callbacks (called by ENetServer/ENetClient)
// =============================================================================

void ENetConnection::on_connect() {
    state_.store(State::Connected, std::memory_order_release);
}

void ENetConnection::on_disconnect() {
    state_.store(State::Disconnected, std::memory_order_release);
    peer_ = nullptr;
}

void ENetConnection::on_receive(const std::uint8_t* data, std::size_t size) {
    // Track bytes received
    bytesRecv_.fetch_add(size, std::memory_order_relaxed);
    
    // Deserialize message
    shared::proto::Message msg;
    if (!deserialize_message(data, size, msg)) {
        // Malformed packet - ignore
        return;
    }
    
    // Queue for try_recv()
    std::lock_guard<std::mutex> lock(recvMutex_);
    recvQueue_.push(std::move(msg));
}

} // namespace shared::transport
