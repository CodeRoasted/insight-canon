module;
#include "utils/log_macros.hpp" // textual macro layer (§11.9)

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan; // fast_gates predicates + sv_* scan primitives

// SparkHDFSStrategy — parses Spark and HDFS log formats.
//
// Spark:  "17/06/09 20:10:40 INFO executor.CoarseGrainedExecutorBackend: msg"
// HDFS:   "081109 203615 148 INFO dfs.DataNode$PacketResponder: msg"
//
// Hand-written scanner: zero RE2, zero string copies.

namespace insight::tokenization
{

std::expected<ParsedLine, std::string> SparkHDFSStrategy::parse(std::string_view line,
                                                                ArenaAllocator& /*arena*/) const
{
    static constexpr std::size_t kSparkTimestampLen{17U};      // "YY/MM/DD HH:MM:SS"
    static constexpr std::size_t kHdfsTimestampEndOffset{13U}; // YYMMDD + space + HHMMSS

    // ── Spark: "YY/MM/DD HH:MM:SS LEVEL component: msg" ────────────────────
    if (is_spark_prefix(line))
    {
        if (line.size() < kSparkTimestampLen)
        {
            INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=SparkHDFS parse miss (short)");
            return std::unexpected(
                std::string("SparkHDFSStrategy: line too short for Spark format"));
        }
        // "YY/MM/DD HH:MM:SS" — 17 contiguous chars; directly sliceable.
        const std::string_view ts_str{line.substr(0, kSparkTimestampLen)};
        std::string_view rest{line.substr(kSparkTimestampLen)};
        sv_skip_ws(rest);

        const std::string_view level_sv{sv_take_token(rest)};
        const std::string_view component{sv_take_until(rest, ':')};
        sv_skip_ws(rest);

        ParsedLine parsed_line;
        parsed_line.raw_line = line;
        parsed_line.timestamp = utils::parse_short_year_slash(ts_str);
        parsed_line.level = utils::parse_log_level(level_sv);
        parsed_line.component = component;
        parsed_line.content = rest;
        INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                          "strategy=SparkHDFS parsed component={} level={} has_timestamp={}",
                          parsed_line.component, to_string(parsed_line.level),
                          parsed_line.timestamp.has_value());
        return std::expected<ParsedLine, std::string>{parsed_line};
    }

    // ── HDFS: "YYMMDD HHMMSS N LEVEL component: msg" ──────────────────────
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

        (void)sv_take_token(rest); // skip record count / thread id
        const std::string_view level_sv{sv_take_token(rest)};
        const std::string_view component{sv_take_until(rest, ':')};
        sv_skip_ws(rest);

        ParsedLine parsed_line;
        parsed_line.raw_line = line;
        parsed_line.timestamp = utils::parse_compact_date_time(date, time_str);
        parsed_line.level = utils::parse_log_level(level_sv);
        parsed_line.component = component;
        parsed_line.content = rest;
        INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                          "strategy=SparkHDFS parsed component={} level={} has_timestamp={}",
                          parsed_line.component, to_string(parsed_line.level),
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
