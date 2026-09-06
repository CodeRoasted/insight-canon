module;
// refs: ADR-3.D4
#include "utils/log_macros.hpp"
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>
// note: this header declares both colour sinks; spdlog ships no stderr_color_sinks.h.
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

module insight.canon.api;
import insight.canon.internal;

namespace insight::logging
{

namespace
{

    auto& init_flag()
    {
        static std::once_flag flag;
        return flag;
    }

    // invariant: false until init_logging()'s once-lambda has registered every logger; it is set
    // last, inside that lambda.
    std::atomic<bool>& initialised()
    {
        static std::atomic<bool> flag{false};
        return flag;
    }

    // pre: init_logging() has run and `name` is not one of the loggers it registered.
    // post: at most one record per distinct name, for the lifetime of the process.
    // refs: ADR-5.D1
    void report_unregistered_once(std::string_view name)
    {
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

    // invariant: absent from kAllLoggers and never registered, so it cannot collide with a module
    // logger's name.
    constexpr std::string_view kUninitialisedLogger{"insight.uninitialised"};

    // post: registers no name, leaves initialised() false and does not consume init_flag(), so a
    // later init_logging() still wins whatever logged first.
    // refs: DN-53
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

    // post: never null on the registered or quiet-logger branch; the default-logger branch is null
    // once the host drops it, and log_message() then discards the record.
    std::shared_ptr<spdlog::logger> logger_for(std::string_view name)
    {
        if (auto logger{spdlog::get(std::string{name})})
            return logger;
        if (initialised())
        {
            report_unregistered_once(name);
            return spdlog::default_logger();
        }
        return quiet_logger();
    }

    auto& shared_sink()
    {
        static std::shared_ptr<spdlog::sinks::sink> sink;
        return sink;
    }

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

                       for (const auto name : kAllLoggers)
                       {
                           make_logger(name, default_level);
                       }

                       spdlog::set_level(default_level);

                       // assert: every logger is registered; past this point an unresolved name is
                       // a defect, not a cold start.
                       initialised().store(true, std::memory_order_release);
                   });
}

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
