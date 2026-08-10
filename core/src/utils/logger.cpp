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

    // Set at the END of the init_logging lambda. It exists to SEPARATE TWO STATES the old
    // fallback conflated, which is the whole reason a missing registration could hide for
    // weeks:
    //
    //   (A) init_logging() never ran   — nothing is registered, the default logger is the
    //       only thing that works, and falling back is CORRECT. This is the unit-test path
    //       and it must stay silent: a naive fail-fast here turns every test that logs
    //       before init into a crash.
    //   (B) init_logging() ran, but this NAME is not registered — a programming error. The
    //       old code degraded to the default logger identically to (A), so nothing
    //       distinguished "not initialised" from "not registered" at runtime.
    //
    // The trap in reporting (B) is that you cannot complain about a broken logging facade
    // THROUGH the logging facade. That trap does not bind here, and the state split is what
    // dissolves it: in (B) logging demonstrably WORKS — init_logging succeeded, the shared
    // sink exists, the default logger is live — so the facade can report on itself. No
    // `std::cerr` is needed, and ADR-5.D1's stderr ban is therefore not in tension with this.
    std::atomic<bool>& initialised()
    {
        static std::atomic<bool> flag{false};
        return flag;
    }

    // ONCE per offending name, not once per call: the accessors are called from every log
    // site of that module, so warning per call would answer a silent degradation with a
    // flood — the same trade CLAUDE.md's hot-path doctrine refuses elsewhere. Cold by
    // construction: it is reachable only after init_logging() succeeded AND only for a name
    // init_logging did not create, i.e. only when the defect is present.
    void report_unregistered_once(std::string_view name)
    {
        // The memo OWNS its keys. Storing the `string_view` would be correct only while every
        // caller happens to pass one of the `inline constexpr` name constants — static storage
        // — and the parameter type invites a temporary that would leave a dangling view to be
        // compared against on every later miss. The copy costs nothing on a path that is cold
        // by construction, and it removes a lifetime coupling to the caller nobody would think
        // to preserve.
        static std::mutex mutex;
        static std::vector<std::string> reported;
        {
            const std::scoped_lock lock{mutex};
            if (std::ranges::find(reported, name) != reported.end())
                return;
            reported.emplace_back(name);
        }
        INSIGHT_LOG_WARN(spdlog::default_logger(),
                         "logger '{}' is NOT REGISTERED although init_logging() has run — "
                         "falling back to the default logger, so every record from this "
                         "module ships WITHOUT its [{}] tag and is invisible to any sink "
                         "attached by name. Add it to kAllLoggers in canon.api.cppm.",
                         name, name);
    }

    // The one accessor the per-module getters share. They differed by nothing but a name,
    // and seven copies of a fallback is how one of them quietly stopped matching the others.
    std::shared_ptr<spdlog::logger> logger_for(std::string_view name)
    {
        if (auto logger{spdlog::get(std::string{name})})
            return logger;
        if (initialised())
            report_unregistered_once(name); // state (B) — loud, because logging works here
        return spdlog::default_logger();    // state (A) — silent, and correct
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

void init_logging(spdlog::level::level_enum default_level, bool diagnostics_to_stderr)
{
    std::call_once(init_flag(),
                   [default_level, diagnostics_to_stderr]()
                   {
                       // The sink is chosen ONCE, by the first caller, like every other decision
                       // under this call_once. A tool that owns a machine-readable stdout says so
                       // here rather than hoping the level stays low — see the declaration.
                       shared_sink() =
                           diagnostics_to_stderr
                               ? std::static_pointer_cast<spdlog::sinks::sink>(
                                     std::make_shared<spdlog::sinks::stderr_color_sink_mt>())
                               : std::static_pointer_cast<spdlog::sinks::sink>(
                                     std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

                       // Module loggers — order does not matter. The SET is `kAllLoggers`
                       // in canon.api.cppm, beside the name constants; this unit no longer
                       // keeps a copy. The copy it used to keep had silently lost
                       // `kPipelineLogger`, so `pipeline_logger()` resolved to spdlog's
                       // default logger and every pipeline WARN shipped untagged.
                       for (const auto name : kAllLoggers)
                       {
                           make_logger(name, default_level);
                       }

                       // Global level floor — individual loggers may be raised above this.
                       spdlog::set_level(default_level);

                       // LAST: from here on, an unresolved name means "not registered"
                       // rather than "not initialised", and the accessors say so out loud.
                       initialised().store(true, std::memory_order_release);
                   });
}

// ── Per-module accessors ─────────────────────────────────────────────────────
// spdlog::get() acquires a mutex; acceptable because:
//   - TRACE/DEBUG macros are compiled out in Release (getter never runs on hot paths)
//   - INFO/WARN call sites are infrequent (constructors, thresholds, resets)
// Falls back to spdlog::default_logger() when init_logging() hasn't been called
// (e.g. in unit tests) — avoids null-pointer dereference in SPDLOG_LOGGER_CALL. That
// fallback is SILENT only in that state; once init_logging has run, an unresolved name is
// a defect and logger_for() reports it. See logger_for/initialised above.

std::shared_ptr<spdlog::logger> arena_logger()
{
    return logger_for(kArenaLogger);
}

std::shared_ptr<spdlog::logger> mask_logger()
{
    return logger_for(kMaskLogger);
}

std::shared_ptr<spdlog::logger> pipeline_logger()
{
    return logger_for(kPipelineLogger);
}

std::shared_ptr<spdlog::logger> detector_logger()
{
    return logger_for(kDetectorLogger);
}

std::shared_ptr<spdlog::logger> parser_logger()
{
    return logger_for(kParserLogger);
}

std::shared_ptr<spdlog::logger> strategy_logger()
{
    return logger_for(kStrategyLogger);
}

std::shared_ptr<spdlog::logger> tokenizer_logger()
{
    return logger_for(kTokenizerLogger);
}

} // namespace insight::logging
