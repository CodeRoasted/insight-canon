// log_macros.hpp — TEXTUAL macro layer for the canon logging facility (1.5.1 unwrap, §11.9).
//
// The INSIGHT_LOG_* macros + the detail::log_message template + the compile-time gate constants live
// HERE, as a textual header, NOT in a module: macros cannot be exported from a module, the gate
// constants need spdlog's SPDLOG_LEVEL_* macros, and the template pulls fmt/spdlog (third-party, not in
// `import std`). Module impl units that log `#include` this in their global module fragment; the runtime
// logger accessors (init_logging / *_logger()) live in the insight.canon.api module instead.
#pragma once

// SPDLOG_ACTIVE_LEVEL is a CMake -D per build type (Debug: TRACE, Release: INFO). Guard a missing
// definition → default TRACE (nothing elided).
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <fmt/core.h>
#include <fmt/format.h>
#include <memory>
#include <spdlog/common.h>
#include <spdlog/logger.h>
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
#define INSIGHT_LOG_WARN(logger, ...)                                                             \
    ::insight::logging::detail::log_message(                                                       \
        (logger), spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::warn,    \
        __VA_ARGS__)
#else
#define INSIGHT_LOG_WARN(...) ((void)0)
#endif
// NOLINTEND(cppcoreguidelines-macro-usage)
