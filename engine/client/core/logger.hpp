#pragma once

#include "config.hpp"
#include "engine/core/export.hpp"

namespace core {

class RAYFLOW_CLIENT_API Logger {
public:
    static Logger& instance();

    void init(const LoggingConfig& cfg);
    void shutdown();

private:
    Logger() = default;

    void* file_{nullptr};
    bool callback_installed_{false};

    static void trace_callback(int logLevel, const char* text, va_list args);
};

} // namespace core
