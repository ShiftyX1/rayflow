#pragma once

#include "config.hpp"
#include "engine/core/export.hpp"

namespace engine::console { class ConsoleLogSink; }

namespace core {

class RAYFLOW_CLIENT_API Logger {
public:
    static Logger& instance();

    void init(const LoggingConfig& cfg);
    void shutdown();

    /// Attach (or detach) the in-game developer console log sink.
    /// The sink receives every TraceLog message in addition to stderr/file.
    void set_console_sink(engine::console::ConsoleLogSink* sink);

private:
    Logger() = default;

    void* file_{nullptr};
    bool callback_installed_{false};
    engine::console::ConsoleLogSink* console_sink_{nullptr};

    static void trace_callback(int logLevel, const char* text, va_list args);
};

} // namespace core
