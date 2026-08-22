module;
#include "utils/log_macros.hpp" // textual macro layer (ADR-3.D4)
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>
// spdlog 1.13 declares BOTH colour sinks here; there is no `stderr_color_sinks.h` sibling to
// narrow this to (measured: the header does not exist in the pinned tree). So this include is
// not evidence that anything writes to stdout — this unit constructs `stderr_color_sink_mt` only.
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
    //   (A) init_logging() never ran   — nothing is registered, and the accessors resolve to a
    //       canon-owned quiet logger (see `quiet_logger` below). The FACADE must stay silent
    //       about itself here: a naive fail-fast turns every test that logs before init into a
    //       crash. Silence about itself is not silence about the module's records — those still
    //       go out, on canon's own stderr sink.
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

    // The name state-(A) records carry. It is not in `kAllLoggers` and is never registered, so it
    // cannot collide with a module name — and it tells an operator reading a stray warning the one
    // fact that explains it: this process never called init_logging().
    constexpr std::string_view kUninitialisedLogger{"insight.uninitialised"};

    // WHERE STATE (A)'s RECORDS GO. Before this existed the accessors handed back
    // `spdlog::default_logger()`, whose sink is STDOUT at info — so every entry point that linked
    // canon and called nothing put canon's diagnostics into its own standard output. Four of the
    // six entry points measured for DN-53 were that shape, insight-metalog's determinism fixture
    // among them: it returned two sha256 for two runs of one binary on one input, the differing
    // bytes a wall clock inside a log line. The artifact was a function of the operator's log
    // level rather than of the input.
    //
    // Three properties, each load-bearing and each with a way to get it wrong:
    //  * STDERR, never stdout. canon's callers are CLI tools and sidecars whose stdout is a
    //    machine artifact. This is logcraft's `install_library_default` posture — never stdout,
    //    never touch the host's default logger — applied to the sibling facade that lacked it.
    //  * IT OWNS A SINK, and its level ADMITS a warn. A sinkless logger, or one filtered to `err`,
    //    would keep stdout clean by making canon mute in every un-initialised process. That buys
    //    the stream property by deleting the diagnostics, and it is not the fix.
    //  * SINK ONLY, NOT A REGISTRATION. It registers no name, does not set `initialised()`, and
    //    does not consume `init_flag()`. logcraft shares one flag between `install_library_default`
    //    and `configure_loggers` ("first call wins") and documents the cost: a static initializer
    //    that logs before `main` silently discards the level `main` later chooses. canon does not
    //    inherit that hazard for a state whose entire purpose is to be quiet until someone decides
    //    — so init_logging() still wins whenever it runs, no matter what logged first.
    std::shared_ptr<spdlog::logger> quiet_logger()
    {
        static const std::shared_ptr<spdlog::logger> logger{
            []
            {
                auto made{std::make_shared<spdlog::logger>(
                    std::string{kUninitialisedLogger},
                    std::static_pointer_cast<spdlog::sinks::sink>(
                        std::make_shared<spdlog::sinks::stderr_color_sink_mt>()))};
                made->set_level(spdlog::level::warn);
                made->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
                return made;
            }()};
        return logger;
    }

    // The one accessor the per-module getters share. They differed by nothing but a name,
    // and seven copies of a fallback is how one of them quietly stopped matching the others.
    std::shared_ptr<spdlog::logger> logger_for(std::string_view name)
    {
        if (auto logger{spdlog::get(std::string{name})})
            return logger;
        if (initialised())
        {
            report_unregistered_once(name); // state (B) — loud, because logging works here
            return spdlog::default_logger();
        }
        return quiet_logger(); // state (A) — canon's own stream, never the host's
    }

    // Shared colour sink — all module loggers write to the SAME sink so output is interleaved
    // coherently. STDERR, unconditionally: see the declaration in canon.api.cppm for why this is
    // a constant and not a caller's choice. The sink itself is thread-safe (mt).
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
                       shared_sink() = std::static_pointer_cast<spdlog::sinks::sink>(
                           std::make_shared<spdlog::sinks::stderr_color_sink_mt>());

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
// An unresolved name never yields null (SPDLOG_LOGGER_CALL would dereference it), and which
// logger it yields instead is the state split: before init_logging, canon's own quiet stderr
// logger, silently; after it, spdlog's default logger plus a one-per-name WARN, because at that
// point the name is a defect. See quiet_logger/logger_for/initialised above.

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
