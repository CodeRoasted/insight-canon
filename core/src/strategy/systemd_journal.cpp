module;
#include "strategy/simdjson_scratch.hpp" // textual: TU-local simdjson entities (ADR-3.D4 family)
#include "utils/log_macros.hpp"          // textual macro layer (ADR-3.D4)
#include <simdjson.h>

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan; // fast_gates predicates + sv_* scan primitives

// SystemdJournalStrategy — parses systemd journal JSON export format:
//   {"__REALTIME_TIMESTAMP":"1705312200000000","PRIORITY":"6",
//    "_COMM":"nginx","MESSAGE":"Worker started"}
//
// Hot path uses simdjson on-demand via the shared scratch helpers; nlohmann is
// no longer linked into the production library.

namespace insight::tokenization
{

namespace
{

    // systemd journal PRIORITY follows syslog severity: 0=emerg .. 7=debug
    // NOLINTBEGIN(readability-magic-numbers)
    LogLevel priority_to_level(int priority) noexcept
    {
        switch (priority)
        {
        case 0:
        case 1:
        case 2:
            return LogLevel::Fatal; // emerg/alert/crit
        case 3:
            return LogLevel::Error;
        case 4:
            return LogLevel::Warn;
        case 5:
        case 6:
            return LogLevel::Info; // notice/info
        case 7:
            return LogLevel::Debug;
        default:
            return LogLevel::Unknown;
        }
    }
    // NOLINTEND(readability-magic-numbers)

    bool has_journal_indicators(std::string_view line) noexcept
    {
        return line.contains("__REALTIME_TIMESTAMP") || line.contains("_SYSTEMD_UNIT") ||
               line.contains("SYSLOG_IDENTIFIER");
    }

    // PRIORITY is encoded as a string in journalctl -o json. Parse with from_chars
    // (no allocation) over the simdjson string view.
    int parse_priority(std::string_view priority_view) noexcept
    {
        static constexpr int kDefaultInfoPriority{6};

        int prio{kDefaultInfoPriority};
        std::from_chars(priority_view.data(), priority_view.data() + priority_view.size(), prio);
        return prio;
    }

} // namespace

std::expected<ParsedLine, std::string> SystemdJournalStrategy::parse(std::string_view line,
                                                                     ArenaAllocator& arena) const
{
    static constexpr std::int64_t kMicrosecondsPerSecond{1'000'000};
    static constexpr std::array<std::string_view, 1> kRealtimeKeys{"__REALTIME_TIMESTAMP"};
    static constexpr std::array<std::string_view, 1> kPriorityKeys{"PRIORITY"};
    static constexpr std::array<std::string_view, 1> kCommKeys{"_COMM"};
    static constexpr std::array<std::string_view, 1> kSyslogIdentifierKeys{"SYSLOG_IDENTIFIER"};
    static constexpr std::array<std::string_view, 1> kSystemdUnitKeys{"_SYSTEMD_UNIT"};
    static constexpr std::array<std::string_view, 1> kMessageKeys{"MESSAGE"};

    // Cheap substring gate: production-shape journal lines always contain one
    // of these markers. Avoids a full simdjson parse on generic JSON.
    if (!has_journal_indicators(line))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(),
                          "strategy=SystemdJournal missing_indicator_keys");
        return std::unexpected(
            std::string("SystemdJournalStrategy: no journal indicator keys found"));
    }

    auto& scratch{json_scratch()};
    const auto padded{load_padded(scratch, line)};

    simdjson::ondemand::document doc;
    if (auto err = scratch.parser.iterate(padded).get(doc); err != simdjson::SUCCESS)
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=SystemdJournal invalid_json={}",
                          simdjson::error_message(err));
        return std::unexpected(std::string("SystemdJournalStrategy: invalid JSON"));
    }

    simdjson::ondemand::object root;
    if (doc.get_object().get(root) != simdjson::SUCCESS)
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=SystemdJournal not_an_object");
        return std::unexpected(std::string("SystemdJournalStrategy: not an object"));
    }

    ParsedLine parsed;
    parsed.raw_line = line;

    std::string_view scratch_view;

    // Timestamp: __REALTIME_TIMESTAMP is microseconds since epoch (string-encoded).
    if (try_get_string(root, kRealtimeKeys, scratch_view))
    {
        std::int64_t microsecs{};
        const auto res{std::from_chars(scratch_view.data(),
                                       scratch_view.data() + scratch_view.size(), microsecs)};
        if (res.ec == std::errc{})
        {
            const auto epoch_secs{static_cast<std::time_t>(microsecs / kMicrosecondsPerSecond)};
            parsed.timestamp =
                EventTime::parsed(std::chrono::system_clock::from_time_t(epoch_secs));
        }
    }

    if (try_get_string(root, kPriorityKeys, scratch_view))
        parsed.level = EventLevel::declared(priority_to_level(parse_priority(scratch_view)));

    // Component: prefer _COMM, fall back to SYSLOG_IDENTIFIER, then _SYSTEMD_UNIT.
    if (try_get_string(root, kCommKeys, scratch_view) ||
        try_get_string(root, kSyslogIdentifierKeys, scratch_view) ||
        try_get_string(root, kSystemdUnitKeys, scratch_view))
        parsed.component = arena.store_string(scratch_view);

    if (try_get_string(root, kMessageKeys, scratch_view))
    {
        parsed.content = arena.store_string(scratch_view);
    }
    else
    {
        // Fallback: arena-store the raw line (no dump() heap alloc).
        parsed.content = arena.store_string(line);
    }

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=SystemdJournal parsed component={} level={} has_timestamp={}",
                      parsed.component, to_string(parsed.level.value()),
                      parsed.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed};
}

LogFormat SystemdJournalStrategy::format() const noexcept
{
    return LogFormat::SystemdJournal;
}

double SystemdJournalStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr std::size_t kMinimumCandidateLength{20};
    static constexpr double kJournalConfidence{1.06};
    static constexpr double kNoConfidence{0.0};

    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;

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

    return has_journal_indicators(line) ? kJournalConfidence : kNoConfidence;
}

} // namespace insight::tokenization
