module;
#include "strategy/simdjson_scratch.hpp"
#include "utils/log_macros.hpp"
#include <simdjson.h>

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// post: an AWS CloudWatch JSON log event — a millisecond epoch, a message and a log group.
// invariant: an escape-free flat line takes the byte reader and any other the simdjson on-demand
// walk, both through the shared scratch helpers and into one assignment rule.
// invariant: detection is gated by an O(1) substring check, so the parse is invoked at most once
// per line.
// invariant: the log macros and the simdjson entities stay TEXTUAL in the global module fragment
// and are TU-local, so no first-party declaration leaks through it.
// refs: ADR-3.D4
namespace insight::tokenization
{

namespace
{

    // invariant: THE ONE ASSIGNMENT RULE of a CloudWatch line, fed by the byte reader and the
    // simdjson reader alike, so an unrelated member's bytes never move a role.
    // invariant: only the strategy's declared top-level keys are read — `timestamp`, `level`,
    // `logGroup`, `logStream`, `message` — and no compound spelling or descended member is.
    // invariant: a role takes the FIRST member of its key and kind in document order — a number
    // for `timestamp`, a string otherwise — and an empty string is a value.
    // invariant: the component is `logGroup` when one was read, else `logStream`, whatever order.
    // invariant: a `timestamp` is a millisecond count only when its whole token is an int64
    // integer, so a fraction, an exponent or a literal past int64 leaves the time absent.
    class CloudWatchRecordSink
    {
      public:
        [[nodiscard]] bool member(std::string_view key, JsonScalar scalar) noexcept
        {
            if (scalar.kind == JsonScalarKind::Number)
            {
                if (key == "timestamp" && !timestamp_seen_)
                {
                    timestamp_seen_ = true;
                    timestamp_ms_ = parse_millis(scalar.text);
                }
            }
            else if (scalar.kind == JsonScalarKind::String)
            {
                if (key == "level")
                    take_first(level_, scalar.text);
                else if (key == "logGroup")
                    take_first(log_group_, scalar.text);
                else if (key == "logStream")
                    take_first(log_stream_, scalar.text);
                else if (key == "message")
                    take_first(message_, scalar.text);
            }
            return true;
        }

        void descended_member(std::string_view /*key*/, JsonScalar /*scalar*/) noexcept {}

        // post: the chosen roles written to `parsed`, the content being the `message` member or
        // else the whole line.
        // invariant: every value is parsed or arena-stored here, so the views it holds may dangle
        // once this returns.
        void apply(ParsedLine& parsed, std::string_view line, ArenaAllocator& arena) const
        {
            static constexpr std::int64_t kMillisecondsPerSecond{1000};
            parsed.raw_line = line;
            if (timestamp_ms_.has_value())
            {
                // invariant: the producer's exact millisecond count is divided to whole seconds
                // toward zero, its remainder dropped: this strategy's event time has second grain.
                // invariant: a second outside Timestamp's range leaves the time absent.
                // refs: DN-17.D44, DN-43.D21
                parsed.timestamp = EventTime::parsed(
                    utils::epoch_seconds_to_timestamp(*timestamp_ms_ / kMillisecondsPerSecond));
            }
            if (level_.has_value())
                parsed.level = EventLevel::declared(utils::parse_log_level(*level_));
            if (const auto& component{log_group_.has_value() ? log_group_ : log_stream_})
                parsed.component = arena.store_string(*component);
            parsed.content = arena.store_string(message_.value_or(line));
        }

      private:
        static void take_first(std::optional<std::string_view>& role,
                               std::string_view value) noexcept
        {
            if (!role.has_value())
                role = value;
        }

        [[nodiscard]] static std::optional<std::int64_t>
        parse_millis(std::string_view text) noexcept
        {
            std::int64_t millis{0};
            const char* const end{text.data() + text.size()};
            const auto [stop, error]{std::from_chars(text.data(), end, millis)};
            if (error != std::errc{} || stop != end)
                return std::nullopt;
            return millis;
        }

        bool timestamp_seen_{false};
        std::optional<std::int64_t> timestamp_ms_;
        std::optional<std::string_view> level_;
        std::optional<std::string_view> log_group_;
        std::optional<std::string_view> log_stream_;
        std::optional<std::string_view> message_;
    };
    static_assert(JsonMemberSink<CloudWatchRecordSink>);

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
    // invariant: the cheap substring gate is what avoids a full simdjson parse on generic JSON.
    if (!has_cloudwatch_indicators(line))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=CloudWatch missing_indicator_keys");
        return std::unexpected(
            std::string("CloudWatchStrategy: no CloudWatch indicator keys found"));
    }

    // invariant: the byte reader bypasses simdjson for escape-free flat objects and falls back on
    // any anomaly — nested structures, escape sequences or malformed JSON.
    // invariant: both readers feed the one sink, so the reader decides only how bytes are read.
    if (CloudWatchRecordSink fast; scan_flat_json(line, fast))
    {
        ParsedLine parsed;
        fast.apply(parsed, line, arena);
        INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                          "strategy=CloudWatch fast_path component={} level={} has_timestamp={}",
                          parsed.component, to_string(parsed.level.value()),
                          parsed.timestamp.has_value());
        return std::expected<ParsedLine, std::string>{parsed};
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

    CloudWatchRecordSink sink;
    walk_json_object(root, sink);
    ParsedLine parsed;
    sink.apply(parsed, line, arena);
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
