// core/api/insight/utils/logger.hpp
//
// Centralized spdlog logger facility for SmartLog.
//
// Design:
//   - One named logger per module for fine-grained runtime level control.
//   - Call init_logging() once at startup.
//   - All hot-path call sites use INSIGHT_LOG_TRACE / INSIGHT_LOG_DEBUG
//     macros. These short-circuit before formatting when the compile-time
//     SPDLOG_ACTIVE_LEVEL is above the call's level, achieving true zero-cost
//     in Release builds.
//
// Runtime control (after init):
//   insight::logging::drain_logger()->set_level(spdlog::level::debug);
//   spdlog::set_level(spdlog::level::warn);   // change all loggers

#pragma once

// SPDLOG_ACTIVE_LEVEL is set via CMake compile definitions per build type:
//   Debug:   SPDLOG_LEVEL_TRACE  (all macros active)
//   Release: SPDLOG_LEVEL_INFO   (TRACE/DEBUG macros compiled out)
// Guard against missing definition — default to TRACE (safest: nothing elided).
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <fmt/core.h>
#include <fmt/format.h>
#include <memory>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <string_view>
#include <utility>

namespace insight::logging
{

inline constexpr bool kTraceLogsEnabled{SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE};
inline constexpr bool kDebugLogsEnabled{SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG};
inline constexpr bool kInfoLogsEnabled{SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_INFO};
inline constexpr bool kWarnLogsEnabled{SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_WARN};

namespace detail
{

    template <typename... Args>
    inline void log_message(const std::shared_ptr<spdlog::logger>& logger,
                            const spdlog::source_loc& source_location,
                            spdlog::level::level_enum level, fmt::format_string<Args...> format,
                            Args&&... args)
    {
        if (!logger || !logger->should_log(level))
        {
            return;
        }

        logger->log(source_location, level, fmt::format(format, std::forward<Args>(args)...));
    }

} // namespace detail

// ── Module logger names ──────────────────────────────────────────────────────
inline constexpr std::string_view kArenaLogger{"insight.arena"};
inline constexpr std::string_view kDrainLogger{"insight.drain"};
inline constexpr std::string_view kDetectorLogger{"insight.detector"};
inline constexpr std::string_view kParserLogger{"insight.parser"};
inline constexpr std::string_view kStrategyLogger{"insight.strategy"};
inline constexpr std::string_view kTokenizerLogger{"insight.tokenizer"};

// ── Initialisation ───────────────────────────────────────────────────────────
// Creates all named loggers with a shared stdout colour sink.
// Call once before any logging.  Thread-safe (first call wins; subsequent
// calls are no-ops).
// default_level applies to every logger; override per-module afterwards via
//   arena_logger()->set_level(spdlog::level::debug);
void init_logging(spdlog::level::level_enum default_level = spdlog::level::info);

// ── Per-module logger accessors ──────────────────────────────────────────────
// Each returns the named logger when registered, otherwise the spdlog default
// logger. This keeps unit tests safe even if init_logging() is not called.
std::shared_ptr<spdlog::logger> arena_logger();
std::shared_ptr<spdlog::logger> drain_logger();
std::shared_ptr<spdlog::logger> detector_logger();
std::shared_ptr<spdlog::logger> parser_logger();
std::shared_ptr<spdlog::logger> strategy_logger();
std::shared_ptr<spdlog::logger> tokenizer_logger();

} // namespace insight::logging

// The macro layer is intentional: disabled log levels must compile out before
// argument evaluation on hot paths.
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
#define INSIGHT_LOG_TRACE(logger, ...)                                                             \
    ::insight::logging::detail::log_message(                                                       \
        (logger), spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::trace,   \
        __VA_ARGS__)
#else
#define INSIGHT_LOG_TRACE(...) ((void)0)
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
#define INSIGHT_LOG_DEBUG(logger, ...)                                                             \
    ::insight::logging::detail::log_message(                                                       \
        (logger), spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::debug,   \
        __VA_ARGS__)
#else
#define INSIGHT_LOG_DEBUG(...) ((void)0)
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_INFO
#define INSIGHT_LOG_INFO(logger, ...)                                                              \
    ::insight::logging::detail::log_message(                                                       \
        (logger), spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::info,    \
        __VA_ARGS__)
#else
#define INSIGHT_LOG_INFO(...) ((void)0)
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_WARN
#define INSIGHT_LOG_WARN(logger, ...)                                                              \
    ::insight::logging::detail::log_message(                                                       \
        (logger), spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::warn,    \
        __VA_ARGS__)
#else
#define INSIGHT_LOG_WARN(...) ((void)0)
#endif
// NOLINTEND(cppcoreguidelines-macro-usage)
