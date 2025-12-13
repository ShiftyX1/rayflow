#pragma once

#include "../protocol/messages.hpp"

namespace shared::transport {

class IEndpoint {
public:
    virtual ~IEndpoint() = default;

    virtual void send(shared::proto::Message msg) = 0;
    virtual bool try_recv(shared::proto::Message& outMsg) = 0;
};

} // namespace shared::transport
