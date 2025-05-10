#pragma once

#include <spdlog/spdlog.h>

class Logger {
public:
    enum class Level {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Critical
    };

    static void init(Level level = Level::Debug);
    static void shutdown();

    static std::shared_ptr<spdlog::logger>& get();
};

// Strip path to filename only
#define VC_FILE (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define VC_TRACE(...)    SPDLOG_LOGGER_CALL(Logger::get(), spdlog::level::trace,    __VA_ARGS__)
#define VC_DEBUG(...)    SPDLOG_LOGGER_CALL(Logger::get(), spdlog::level::debug,    __VA_ARGS__)
#define VC_INFO(...)     SPDLOG_LOGGER_CALL(Logger::get(), spdlog::level::info,     __VA_ARGS__)
#define VC_WARN(...)     SPDLOG_LOGGER_CALL(Logger::get(), spdlog::level::warn,     __VA_ARGS__)
#define VC_ERROR(...)    SPDLOG_LOGGER_CALL(Logger::get(), spdlog::level::err,      __VA_ARGS__)
#define VC_CRITICAL(...) SPDLOG_LOGGER_CALL(Logger::get(), spdlog::level::critical, __VA_ARGS__)
