// invariant: this header is SEALED — pure preprocessor plus ONE third-party include, and no
// first-party declaration.
// pre: a translation unit using these macros also does `import insight.canon;`.
// note: a macro cannot cross a module boundary, so this layer stays a textual header.
// refs: ADR-3.D4
#pragma once

// note: canon sets its level as a PRIVATE -D; a public one collides with a consumer's own.
#ifndef SPDLOG_ACTIVE_LEVEL
#ifdef NDEBUG
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#else
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#endif

// invariant: the lazy #define above resolves SPDLOG_LEVEL_* only at its comparison, after this
// include.
#include <spdlog/common.h>

// note: a disabled level must compile out before its arguments are evaluated on a hot path.
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
