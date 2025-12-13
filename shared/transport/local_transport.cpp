#include "local_transport.hpp"

namespace shared::transport {

void LocalTransport::ClientEndpoint::send(shared::proto::Message msg) {
    std::lock_guard lock(*mutex_);
    toServer_->push(std::move(msg));
}

bool LocalTransport::ClientEndpoint::try_recv(shared::proto::Message& outMsg) {
    std::lock_guard lock(*mutex_);
    if (toClient_->empty()) {
        return false;
    }

    outMsg = std::move(toClient_->front());
    toClient_->pop();
    return true;
}

void LocalTransport::ServerEndpoint::send(shared::proto::Message msg) {
    std::lock_guard lock(*mutex_);
    toClient_->push(std::move(msg));
}

bool LocalTransport::ServerEndpoint::try_recv(shared::proto::Message& outMsg) {
    std::lock_guard lock(*mutex_);
    if (toServer_->empty()) {
        return false;
    }

    outMsg = std::move(toServer_->front());
    toServer_->pop();
    return true;
}

LocalTransport::Pair LocalTransport::create_pair() {
    auto mutex = std::make_shared<std::mutex>();
    auto toServer = std::make_shared<std::queue<shared::proto::Message>>();
    auto toClient = std::make_shared<std::queue<shared::proto::Message>>();

    Pair pair;
    pair.client = std::make_shared<ClientEndpoint>(mutex, toServer, toClient);
    pair.server = std::make_shared<ServerEndpoint>(mutex, toServer, toClient);
    return pair;
}

} // namespace shared::transport
