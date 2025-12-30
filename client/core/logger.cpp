#include "logger.hpp"

#include <cstdarg>
#include <cstdio>

#include <raylib.h>

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
        if (file_) {
            SetTraceLogCallback(&Logger::trace_callback);
            callback_installed_ = true;
        }
    }
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

    FILE* sink = static_cast<FILE*>(g_logger->file_);
    if (sink) {
        va_list args_copy;
        va_copy(args_copy, args);

        std::fprintf(sink, "[%.3f][%s] ", t, level_str);
        std::vfprintf(sink, text, args);
        std::fputc('\n', sink);
        std::fflush(sink);

        std::fprintf(stderr, "[%.3f][%s] ", t, level_str);
        std::vfprintf(stderr, text, args_copy);
        std::fputc('\n', stderr);

        va_end(args_copy);
    } else {
        std::fprintf(stderr, "[%.3f][%s] ", t, level_str);
        std::vfprintf(stderr, text, args);
        std::fputc('\n', stderr);
    }
}

} // namespace core
