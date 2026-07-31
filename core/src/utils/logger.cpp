module;
#include "utils/log_macros.hpp" // textual macro layer (ADR-3.D4)
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

module insight.canon.api;
import insight.canon.internal;

// core/src/insight/utils/logger.cpp
//
// Centralized spdlog logger initialisation and per-module logger accessors.

namespace insight::logging
{

namespace
{

    auto& init_flag()
    {
        static std::once_flag flag;
        return flag;
    }

    // Shared colour sink — all module loggers write to the same stdout sink so
    // output is interleaved coherently.  The sink itself is thread-safe (mt).
    auto& shared_sink()
    {
        static std::shared_ptr<spdlog::sinks::sink> sink;
        return sink;
    }

    // Create and register a named logger backed by the shared sink.
    std::shared_ptr<spdlog::logger> make_logger(std::string_view name,
                                                spdlog::level::level_enum level)
    {
        auto logger{std::make_shared<spdlog::logger>(std::string{name}, shared_sink())};
        logger->set_level(level);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
        spdlog::register_logger(logger);
        return logger;
    }

} // namespace

void init_logging(spdlog::level::level_enum default_level)
{
    std::call_once(init_flag(),
                   [default_level]()
                   {
                       shared_sink() = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

                       // Module loggers — order does not matter; names must match kXxxLogger
                       // constants in logger.hpp.
                       const std::vector<std::string_view> logger_names{
                           kArenaLogger,  kMaskLogger,     kDetectorLogger,
                           kParserLogger, kStrategyLogger, kTokenizerLogger,
                       };

                       for (const auto name : logger_names)
                       {
                           make_logger(name, default_level);
                       }

                       // Global level floor — individual loggers may be raised above this.
                       spdlog::set_level(default_level);
                   });
}

// ── Per-module accessors ─────────────────────────────────────────────────────
// spdlog::get() acquires a mutex; acceptable because:
//   - TRACE/DEBUG macros are compiled out in Release (getter never runs on hot paths)
//   - INFO/WARN call sites are infrequent (constructors, thresholds, resets)
// Falls back to spdlog::default_logger() when init_logging() hasn't been called
// (e.g. in unit tests) — avoids null-pointer dereference in SPDLOG_LOGGER_CALL.

std::shared_ptr<spdlog::logger> arena_logger()
{
    auto logger{spdlog::get(std::string{kArenaLogger})};
    return logger ? logger : spdlog::default_logger();
}

std::shared_ptr<spdlog::logger> mask_logger()
{
    auto logger{spdlog::get(std::string{kMaskLogger})};
    return logger ? logger : spdlog::default_logger();
}

std::shared_ptr<spdlog::logger> pipeline_logger()
{
    auto logger{spdlog::get(std::string{kPipelineLogger})};
    return logger ? logger : spdlog::default_logger();
}

std::shared_ptr<spdlog::logger> detector_logger()
{
    auto logger{spdlog::get(std::string{kDetectorLogger})};
    return logger ? logger : spdlog::default_logger();
}

std::shared_ptr<spdlog::logger> parser_logger()
{
    auto logger{spdlog::get(std::string{kParserLogger})};
    return logger ? logger : spdlog::default_logger();
}

std::shared_ptr<spdlog::logger> strategy_logger()
{
    auto logger{spdlog::get(std::string{kStrategyLogger})};
    return logger ? logger : spdlog::default_logger();
}

std::shared_ptr<spdlog::logger> tokenizer_logger()
{
    auto logger{spdlog::get(std::string{kTokenizerLogger})};
    return logger ? logger : spdlog::default_logger();
}

} // namespace insight::logging
