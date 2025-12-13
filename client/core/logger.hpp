#pragma once

#include "config.hpp"

namespace core {

class Logger {
public:
    static Logger& instance();

    // Applies logging settings (level, optional file sink).
    void init(const LoggingConfig& cfg);
    void shutdown();

private:
    Logger() = default;

    void* file_{nullptr};
    bool callback_installed_{false};

    static void trace_callback(int logLevel, const char* text, va_list args);
};

} // namespace core
