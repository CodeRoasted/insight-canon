module;
#include "utils/log_macros.hpp"

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// post: a Spark or an HDFS record — a short-year slash date, or a compact six-digit date and
// clock followed by a counter.
// invariant: a hand-written scanner with no regex and, on the SUCCESS path, no string copies — a
// decline builds an error message.
// invariant: the log macros stay TEXTUAL in the global module fragment, so no first-party
// declaration leaks through it.
// refs: ADR-3.D4
namespace insight::tokenization
{

std::expected<ParsedLine, std::string> SparkHDFSStrategy::parse(std::string_view line,
                                                                ArenaAllocator& /*arena*/) const
{
    static constexpr std::size_t kSparkTimestampLen{17U};
    static constexpr std::size_t kHdfsTimestampEndOffset{13U};

    if (is_spark_prefix(line))
    {
        if (line.size() < kSparkTimestampLen)
        {
            INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=SparkHDFS parse miss (short)");
            return std::unexpected(
                std::string("SparkHDFSStrategy: line too short for Spark format"));
        }
        const std::string_view ts_str{line.substr(0, kSparkTimestampLen)};
        std::string_view rest{line.substr(kSparkTimestampLen)};
        sv_skip_ws(rest);

        const std::string_view level_sv{sv_take_token(rest)};
        // invariant: the colon TERMINATES the component; absent it this line names none.
        // invariant: the unbounded form emptied content and moved the message onto the cube's WHERE
        // axis — the same defect shape repaired at the syslog strategy and left live here.
        // refs: ADR-16.D9, DN-43.D3
        const std::string_view component{sv_take_until_or_none(rest, ':')};
        sv_skip_ws(rest);

        ParsedLine parsed_line;
        parsed_line.raw_line = line;
        parsed_line.timestamp = EventTime::parsed(utils::parse_short_year_slash(ts_str));
        parsed_line.level = EventLevel::declared(utils::parse_log_level(level_sv));
        parsed_line.component = component;
        parsed_line.content = rest;
        INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                          "strategy=SparkHDFS parsed component={} level={} has_timestamp={}",
                          parsed_line.component, to_string(parsed_line.level.value()),
                          parsed_line.timestamp.has_value());
        return std::expected<ParsedLine, std::string>{parsed_line};
    }

    if (is_hdfs_prefix(line))
    {
        if (line.size() < kHdfsMinLen)
        {
            INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=SparkHDFS parse miss (short)");
            return std::unexpected(
                std::string("SparkHDFSStrategy: line too short for HDFS format"));
        }
        const std::string_view date{line.substr(0, 6U)};
        const std::string_view time_str{line.substr(7, 6U)};
        std::string_view rest{line.substr(kHdfsTimestampEndOffset)};
        sv_skip_ws(rest);

        (void)sv_take_token(rest);
        const std::string_view level_sv{sv_take_token(rest)};
        const std::string_view component{sv_take_until_or_none(rest, ':')};
        sv_skip_ws(rest);

        ParsedLine parsed_line;
        parsed_line.raw_line = line;
        parsed_line.timestamp = EventTime::parsed(utils::parse_compact_date_time(date, time_str));
        parsed_line.level = EventLevel::declared(utils::parse_log_level(level_sv));
        parsed_line.component = component;
        parsed_line.content = rest;
        INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                          "strategy=SparkHDFS parsed component={} level={} has_timestamp={}",
                          parsed_line.component, to_string(parsed_line.level.value()),
                          parsed_line.timestamp.has_value());
        return std::expected<ParsedLine, std::string>{parsed_line};
    }

    INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=SparkHDFS parse miss");
    return std::unexpected(
        std::string("SparkHDFSStrategy: line does not match Spark or HDFS format"));
}

LogFormat SparkHDFSStrategy::format() const noexcept
{
    return LogFormat::SparkHDFS;
}

double SparkHDFSStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr std::size_t kMinimumCandidateLength{17};
    static constexpr double kSparkHdfsConfidence{0.85};
    static constexpr double kNoConfidence{0.0};

    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;
    if (is_spark_prefix(line))
        return kSparkHdfsConfidence;
    if (is_hdfs_prefix(line))
        return kSparkHdfsConfidence;
    return kNoConfidence;
}

} // namespace insight::tokenization
