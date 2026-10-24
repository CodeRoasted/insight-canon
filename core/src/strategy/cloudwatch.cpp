module;
#include "strategy/simdjson_scratch.hpp"
#include "utils/log_macros.hpp"
#include <simdjson.h>

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// post: an AWS CloudWatch JSON log event — a millisecond epoch, a message and a log group.
// invariant: the hot path uses simdjson on-demand through the shared scratch helpers.
// invariant: detection is gated by an O(1) substring check, so the parse is invoked at most once
// per line.
// invariant: the log macros and the simdjson entities stay TEXTUAL in the global module fragment
// and are TU-local, so no first-party declaration leaks through it.
// refs: ADR-3.D4
namespace insight::tokenization
{

namespace
{

    // invariant: the fast reader's assignment: a string member, its key resolved by the compound
    // shape, fills the first still-empty role whose name list holds that key, in document order.
    // invariant: a numeric `timestamp` or `ts` member is a millisecond epoch.
    // invariant: the simdjson reader below reads only the strategy's declared keys, so a line whose
    // roles sit under the wider names reads differently on the two paths.
    struct CloudWatchFastSink
    {
        std::string_view timestamp_str;
        std::int64_t timestamp_ms{0};
        std::string_view level_str;
        std::string_view component_str;
        std::string_view message_str;

        [[nodiscard]] bool member(std::string_view raw_key, JsonScalar scalar) noexcept
        {
            if (scalar.kind == JsonScalarKind::String)
                assign_string(compound_key_name(raw_key), scalar.text);
            else if (scalar.kind == JsonScalarKind::Number)
                assign_number(raw_key, scalar.text);
            return true;
        }

        void descended_member(std::string_view /*key*/, JsonScalar /*scalar*/) noexcept {}

        [[nodiscard]] bool has_timestamp() const noexcept
        {
            return !timestamp_str.empty() || timestamp_ms != 0;
        }

        void assign_string(std::string_view key, std::string_view value) noexcept
        {
            if (!has_timestamp() && (key == "ts" || key == "timestamp" || key == "@timestamp" ||
                                     key == "time" || key == "datetime"))
                timestamp_str = value;
            else if (level_str.empty() && (key == "level" || key == "severity" ||
                                           key == "loglevel" || key == "log_level"))
                level_str = value;
            else if (component_str.empty() &&
                     (key == "component" || key == "source" || key == "logger" ||
                      key == "service" || key == "module" || key == "logGroup" ||
                      key == "logStream"))
                component_str = value;
            else if (message_str.empty() && (key == "message" || key == "msg" || key == "log" ||
                                             key == "text" || key == "body"))
                message_str = value;
        }

        // invariant: the integer prefix of the literal is read and a literal past int64 leaves the
        // field unset, so the time is absent rather than wrapped.
        void assign_number(std::string_view key, std::string_view text) noexcept
        {
            if (has_timestamp() || (key != "timestamp" && key != "ts"))
                return;
            std::int64_t millis{0};
            if (std::from_chars(text.data(), text.data() + text.size(), millis).ec != std::errc{})
                return;
            timestamp_ms = millis;
        }
    };
    static_assert(JsonMemberSink<CloudWatchFastSink>);

    // post: true iff the line carries one of the production-shape markers; a substring scan, which
    // is cheaper than parsing for the detection step.
    bool has_cloudwatch_indicators(std::string_view line) noexcept
    {
        return line.contains("logGroup") || line.contains("logStream") ||
               line.contains("logEvents");
    }

} // namespace

std::expected<ParsedLine, std::string> CloudWatchStrategy::parse(std::string_view line,
                                                                 ArenaAllocator& arena) const
{
    static constexpr std::int64_t kMillisecondsPerSecond{1000};
    static constexpr std::array<std::string_view, 1> kTimestampKeys{"timestamp"};
    static constexpr std::array<std::string_view, 1> kLevelKeys{"level"};
    static constexpr std::array<std::string_view, 1> kComponentLogGroup{"logGroup"};
    static constexpr std::array<std::string_view, 1> kComponentLogStream{"logStream"};
    static constexpr std::array<std::string_view, 1> kMessageKeys{"message"};

    // invariant: the cheap substring gate is what avoids a full simdjson parse on generic JSON.
    if (!has_cloudwatch_indicators(line))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=CloudWatch missing_indicator_keys");
        return std::unexpected(
            std::string("CloudWatchStrategy: no CloudWatch indicator keys found"));
    }

    {
        // invariant: the fast path bypasses simdjson for escape-free objects and falls back on any
        // anomaly — nested structures, escape sequences or malformed JSON.
        if (CloudWatchFastSink fast; scan_flat_json(line, fast))
        {
            ParsedLine parsed;
            parsed.raw_line = line;
            if (fast.timestamp_ms != 0)
            {
                // invariant: the producer's exact millisecond count is divided to whole seconds
                // toward zero, its remainder dropped: this path's event time has second grain.
                // invariant: a second outside Timestamp's range leaves the time absent.
                // refs: DN-17.D44, DN-43.D21
                parsed.timestamp = EventTime::parsed(
                    utils::epoch_seconds_to_timestamp(fast.timestamp_ms / kMillisecondsPerSecond));
            }
            else if (!fast.timestamp_str.empty())
            {
                parsed.timestamp = EventTime::parsed(utils::parse_iso8601(fast.timestamp_str));
            }
            if (!fast.level_str.empty())
                parsed.level = EventLevel::declared(utils::parse_log_level(fast.level_str));
            if (!fast.component_str.empty())
                parsed.component = arena.store_string(fast.component_str);
            parsed.content = fast.message_str.empty() ? arena.store_string(line)
                                                      : arena.store_string(fast.message_str);
            INSIGHT_LOG_DEBUG(
                logging::strategy_logger(),
                "strategy=CloudWatch fast_path component={} level={} has_timestamp={}",
                parsed.component, to_string(parsed.level.value()), parsed.timestamp.has_value());
            return std::expected<ParsedLine, std::string>{parsed};
        }
    }

    auto& scratch{json_scratch()};
    const auto padded{load_padded(scratch, line)};

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

    std::int64_t millis{};
    if (try_get_int64(root, kTimestampKeys, millis))
    {
        // invariant: the producer's exact millisecond count is divided to whole seconds
        // toward zero, its remainder dropped: this path's event time has second grain.
        // invariant: a second outside Timestamp's range leaves the time absent.
        // refs: DN-17.D44, DN-43.D21
        parsed.timestamp =
            EventTime::parsed(utils::epoch_seconds_to_timestamp(millis / kMillisecondsPerSecond));
    }

    std::string_view scratch_view;

    if (try_get_string(root, kLevelKeys, scratch_view))
        parsed.level = EventLevel::declared(utils::parse_log_level(scratch_view));

    if (try_get_string(root, kComponentLogGroup, scratch_view) ||
        try_get_string(root, kComponentLogStream, scratch_view))
        parsed.component = arena.store_string(scratch_view);

    if (try_get_string(root, kMessageKeys, scratch_view))
    {
        parsed.content = arena.store_string(scratch_view);
    }
    else
    {
        // invariant: the fallback arena-stores the ORIGINAL line, which avoids the heap allocation
        // a re-serialisation would force.
        parsed.content = arena.store_string(line);
    }

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=CloudWatch parsed component={} level={} has_timestamp={}",
                      parsed.component, to_string(parsed.level.value()),
                      parsed.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed};
}

LogFormat CloudWatchStrategy::format() const noexcept
{
    return LogFormat::CloudWatch;
}

double CloudWatchStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr std::size_t kMinimumCandidateLength{15};
    static constexpr double kCloudWatchConfidence{1.05};
    static constexpr double kNoConfidence{0.0};

    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;

    // post: true iff the first non-space byte opens an object.
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
