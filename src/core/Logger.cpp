#include "core/Logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>

static std::shared_ptr<spdlog::logger> s_logger;

void Logger::init(Level level) {
    s_logger = spdlog::stdout_color_mt("viewcam");
#ifdef NDEBUG
    // Release: clean output — no source file/line noise
    s_logger->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
#else
    s_logger->set_pattern("[%H:%M:%S.%e] [%^%l%$] [%s:%#] %v");
#endif

    switch (level) {
        case Level::Trace:    s_logger->set_level(spdlog::level::trace); break;
        case Level::Debug:    s_logger->set_level(spdlog::level::debug); break;
        case Level::Info:     s_logger->set_level(spdlog::level::info);  break;
        case Level::Warn:     s_logger->set_level(spdlog::level::warn);  break;
        case Level::Error:    s_logger->set_level(spdlog::level::err);   break;
        case Level::Critical: s_logger->set_level(spdlog::level::critical); break;
    }
    // Flush per message: log volume is low at these levels, and a buffered
    // tail is exactly the part needed when diagnosing a hang or crash.
    s_logger->flush_on(s_logger->level());
}

void Logger::shutdown() {
    s_logger.reset();
    spdlog::shutdown();
}

std::shared_ptr<spdlog::logger>& Logger::get() {
    return s_logger;
}
