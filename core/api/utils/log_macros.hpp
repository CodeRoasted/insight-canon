// log_macros.hpp — the INSIGHT_LOG_* macro layer (ADR-3.D4).
//
// Macros cannot cross a module boundary (`import` brings declarations, not `#define`s), and the
// compile-time level elision these macros provide is load-bearing (true zero-cost in Release). So
// the macros stay a TEXTUAL header, #included in the GMF of every logging TU. This header is
// SEALED to pure preprocessor + a SINGLE third-party include — NO first-party declarations leak
// (ADR-3.D4: GMF = third-party-textual). The function the macros expand to (detail::log_message)
// lives in the insight.canon.api module; every logging TU (canon's own + downstream consumers like
// eidos) also does `import insight.canon;`. (The compile-time DEBUG gate for `if constexpr` elision
// is a build-only local in canon's tokenizer/parser impl units — never the api surface, so no level
// macro leaks through the module.) This header stays PUBLIC/installed because downstream packages
// use the INSIGHT_LOG_* macros.
#pragma once

// SPDLOG_ACTIVE_LEVEL is the compile-time elision threshold. A TU that wants a specific level sets
// it as its OWN PRIVATE -D (canon does, per build type); canon does NOT export its level PUBLIC — a
// global spdlog macro forced onto every consumer's command line collides with theirs (it collided
// with logcraft's) and exposing a config macro through an api is wrong. A TU that sets nothing (a
// consumer just using the INSIGHT_LOG_* macros) gets an NDEBUG-aware default: INFO in release
// (trace/debug elided — the sensible zero-cost default), TRACE in debug. The `#define` is lazy, so
// SPDLOG_LEVEL_* (from the include below) resolves only when SPDLOG_ACTIVE_LEVEL is later compared.
#ifndef SPDLOG_ACTIVE_LEVEL
#ifdef NDEBUG
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#else
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#endif

#include <spdlog/common.h> // SPDLOG_FUNCTION, spdlog::source_loc, spdlog::level, SPDLOG_LEVEL_*

// The macro layer is intentional: disabled log levels must compile out before argument evaluation
// on hot paths. detail::log_message resolves via `import insight.canon;`.
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
