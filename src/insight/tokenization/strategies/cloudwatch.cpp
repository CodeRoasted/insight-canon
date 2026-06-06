module;
#include <simdjson.h>
#include "insight/tokenization/strategies/detail/simdjson_scratch.hpp" // textual: TU-local simdjson entities (§11.8 family)
#include "insight/utils/log_macros.hpp" // textual macro layer (§11.9)

module insight.canon.detail;
import insight.canon.internal;
import insight.canon.api;

// src/1_tokenization/strategies/cloudwatch.cpp
//
// CloudWatchStrategy — parses AWS CloudWatch JSON log events:
//   {"timestamp":1705312200000,"message":"User login","logGroup":"/aws/lambda/myFunc"}
//
// Hot path uses simdjson on-demand via the shared scratch helpers; nlohmann is
// no longer linked into the production library. Detection is gated by an O(1)
// substring check in confidence(), so parse() is invoked at most once per line.




namespace insight::tokenization
{

namespace
{

    constexpr std::int64_t kMillisecondsPerSecond{1000};
    constexpr std::size_t kMinimumCandidateLength{15};
    constexpr double kNoConfidence{0.0};
    constexpr double kCloudWatchConfidence{1.05};

    constexpr std::array<std::string_view, 1> kComponentLogGroup{"logGroup"};
    constexpr std::array<std::string_view, 1> kComponentLogStream{"logStream"};
    constexpr std::array<std::string_view, 1> kLevelKeys{"level"};
    constexpr std::array<std::string_view, 1> kMessageKeys{"message"};
    constexpr std::array<std::string_view, 1> kTimestampKeys{"timestamp"};

    // Substring scan; cheaper than parsing for the detection step.
    bool has_cloudwatch_indicators(std::string_view line) noexcept
    {
        return line.contains("logGroup") || line.contains("logStream") ||
               line.contains("logEvents");
    }

} // namespace

std::expected<ParsedLine, std::string> CloudWatchStrategy::parse(std::string_view line,
                                                                 ArenaAllocator& arena) const
{
    // Cheap substring gate: production-shape CloudWatch lines always contain
    // one of these markers. Avoids a full simdjson parse on generic JSON.
    if (!has_cloudwatch_indicators(line))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=CloudWatch missing_indicator_keys");
        return std::unexpected(
            std::string("CloudWatchStrategy: no CloudWatch indicator keys found"));
    }

    // ── Fast path ─────────────────────────────────────────────────────────────
    // Bypass simdjson for escape-free CloudWatch objects. Falls back on any
    // anomaly (nested structures, escape sequences, malformed JSON).
    {
        const auto fast{detail::try_fast_json(line)};
        if (fast.has_result)
        {
            ParsedLine parsed;
            parsed.raw_line = line;
            if (fast.timestamp_ms != 0)
            {
                const auto epoch_secs{
                    static_cast<std::time_t>(fast.timestamp_ms / kMillisecondsPerSecond)};
                parsed.timestamp = std::chrono::system_clock::from_time_t(epoch_secs);
            }
            else if (!fast.timestamp_str.empty())
            {
                parsed.timestamp = utils::parse_iso8601(fast.timestamp_str);
            }
            if (!fast.level_str.empty())
                parsed.level = utils::parse_log_level(fast.level_str);
            if (!fast.component_str.empty())
                parsed.component = arena.store_string(fast.component_str);
            parsed.content = fast.message_str.empty() ? arena.store_string(line)
                                                      : arena.store_string(fast.message_str);
            INSIGHT_LOG_DEBUG(
                logging::strategy_logger(),
                "strategy=CloudWatch fast_path component={} level={} has_timestamp={}",
                parsed.component, to_string(parsed.level), parsed.timestamp.has_value());
            return std::expected<ParsedLine, std::string>{parsed};
        }
    }
    // ── Slow path: full simdjson ──────────────────────────────────────────────

    auto& scratch{detail::json_scratch()};
    const auto padded{detail::load_padded(scratch, line)};

    simdjson::ondemand::document doc;
    if (auto err = scratch.parser.iterate(padded).get(doc); err != simdjson::SUCCESS)
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=CloudWatch invalid_json={}",
                          simdjson::error_message(err));
        return std::unexpected(std::string("CloudWatchStrategy: invalid JSON"));
    }

    simdjson::ondemand::object root;
    if (doc.get_object().get(root) != simdjson::SUCCESS)
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=CloudWatch not_an_object");
        return std::unexpected(std::string("CloudWatchStrategy: not an object"));
    }

    ParsedLine parsed;
    parsed.raw_line = line;

    // Timestamp: millisecond epoch
    std::int64_t millis{};
    if (detail::try_get_int64(root, kTimestampKeys, millis))
    {
        const auto epoch_secs{static_cast<std::time_t>(millis / kMillisecondsPerSecond)};
        parsed.timestamp = std::chrono::system_clock::from_time_t(epoch_secs);
    }

    std::string_view scratch_view;

    if (detail::try_get_string(root, kLevelKeys, scratch_view))
        parsed.level = utils::parse_log_level(scratch_view);

    // Component: prefer logGroup, fall back to logStream
    if (detail::try_get_string(root, kComponentLogGroup, scratch_view) ||
        detail::try_get_string(root, kComponentLogStream, scratch_view))
        parsed.component = arena.store_string(scratch_view);

    if (detail::try_get_string(root, kMessageKeys, scratch_view))
    {
        parsed.content = arena.store_string(scratch_view);
    }
    else
    {
        // Fallback: arena-store the original line; avoids the dump() heap alloc.
        parsed.content = arena.store_string(line);
    }

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=CloudWatch parsed component={} level={} has_timestamp={}",
                      parsed.component, to_string(parsed.level), parsed.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed};
}

LogFormat CloudWatchStrategy::format() const noexcept
{
    return LogFormat::CloudWatch;
}

double CloudWatchStrategy::confidence(std::string_view line) const noexcept
{
    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;

    // First non-space char must be '{'.
    bool starts_brace{};
    for (const char chr : line)
    {
        if (chr == ' ' || chr == '\t')
            continue;
        starts_brace = (chr == '{');
        break;
    }
    if (!starts_brace)
        return kNoConfidence;

    return has_cloudwatch_indicators(line) ? kCloudWatchConfidence : kNoConfidence;
}

} // namespace insight::tokenization
