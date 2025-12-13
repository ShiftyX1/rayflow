#pragma once

#include "endpoint.hpp"

#include <memory>
#include <mutex>
#include <queue>

namespace shared::transport {

class LocalTransport {
public:
    class ClientEndpoint final : public IEndpoint {
    public:
        ClientEndpoint(std::shared_ptr<std::mutex> mutex,
                      std::shared_ptr<std::queue<shared::proto::Message>> toServer,
                      std::shared_ptr<std::queue<shared::proto::Message>> toClient)
            : mutex_(std::move(mutex)), toServer_(std::move(toServer)), toClient_(std::move(toClient)) {}

        void send(shared::proto::Message msg) override;
        bool try_recv(shared::proto::Message& outMsg) override;

    private:
        std::shared_ptr<std::mutex> mutex_;
        std::shared_ptr<std::queue<shared::proto::Message>> toServer_;
        std::shared_ptr<std::queue<shared::proto::Message>> toClient_;
    };

    class ServerEndpoint final : public IEndpoint {
    public:
        ServerEndpoint(std::shared_ptr<std::mutex> mutex,
                      std::shared_ptr<std::queue<shared::proto::Message>> toServer,
                      std::shared_ptr<std::queue<shared::proto::Message>> toClient)
            : mutex_(std::move(mutex)), toServer_(std::move(toServer)), toClient_(std::move(toClient)) {}

        void send(shared::proto::Message msg) override;
        bool try_recv(shared::proto::Message& outMsg) override;

    private:
        std::shared_ptr<std::mutex> mutex_;
        std::shared_ptr<std::queue<shared::proto::Message>> toServer_;
        std::shared_ptr<std::queue<shared::proto::Message>> toClient_;
    };

    struct Pair {
        std::shared_ptr<ClientEndpoint> client;
        std::shared_ptr<ServerEndpoint> server;
    };

    static Pair create_pair();
};

} // namespace shared::transport
