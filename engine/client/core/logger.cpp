#include "logger.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "engine/core/logging.hpp"
#include "engine/core/console/console_log_sink.hpp"

namespace core {

static Logger* g_logger = nullptr;

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::init(const LoggingConfig& cfg) {
    g_logger = this;

    shutdown();

    if (!cfg.enabled) {
        SetTraceLogLevel(LOG_NONE);
        return;
    }

    SetTraceLogLevel(cfg.level);

    if (!cfg.file.empty()) {
        file_ = std::fopen(cfg.file.c_str(), "a");
    }

    // Always install the callback so the console sink is reachable
    SetTraceLogCallback(&Logger::trace_callback);
    callback_installed_ = true;
}

void Logger::set_console_sink(engine::console::ConsoleLogSink* sink) {
    console_sink_ = sink;
}

void Logger::shutdown() {
    if (callback_installed_) {
        SetTraceLogCallback(nullptr);
    }

    if (file_) {
        std::fclose(static_cast<FILE*>(file_));
        file_ = nullptr;
    }

    callback_installed_ = false;
}

void Logger::trace_callback(int logLevel, const char* text, va_list args) {

    const char* level_str = "INFO";
    switch (logLevel) {
        case LOG_ALL: level_str = "ALL"; break;
        case LOG_TRACE: level_str = "TRACE"; break;
        case LOG_DEBUG: level_str = "DEBUG"; break;
        case LOG_INFO: level_str = "INFO"; break;
        case LOG_WARNING: level_str = "WARN"; break;
        case LOG_ERROR: level_str = "ERROR"; break;
        case LOG_FATAL: level_str = "FATAL"; break;
        case LOG_NONE: level_str = "NONE"; break;
        default: level_str = "INFO"; break;
    }

    const double t = GetTime();

    if (!g_logger) {
        std::fprintf(stderr, "[%.3f][%s] ", t, level_str);
        std::vfprintf(stderr, text, args);
        std::fputc('\n', stderr);
        return;
    }

    // Format message once into a buffer for the console sink
    char buf[2048];
    va_list args_copy;
    va_copy(args_copy, args);
    std::vsnprintf(buf, sizeof(buf), text, args_copy);
    va_end(args_copy);

    // Push to the in-game console sink (if attached)
    if (g_logger->console_sink_) {
        g_logger->console_sink_->push(logLevel, buf);
    }

    FILE* sink = static_cast<FILE*>(g_logger->file_);
    if (sink) {
        std::fprintf(sink, "[%.3f][%s] %s\n", t, level_str, buf);
        std::fflush(sink);
    }

    std::fprintf(stderr, "[%.3f][%s] %s\n", t, level_str, buf);
}

} // namespace core
