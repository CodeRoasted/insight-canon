module;
#include "utils/log_macros.hpp"

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// post: a BlueGene/L or Thunderbird supercomputer record.
// invariant: the FIRST COLUMN IS THE CORPUS'S ANSWER KEY — the curators prepended an alert-class
// column to records the RAS system wrote without one.
// invariant: the strategy VALIDATES it, because it is part of the grammar, and carries it in NO
// projection field.
// invariant: so a labelled line projects IDENTICALLY to its unlabelled twin, and one event class
// cannot split dozens of ways by curation label.
// invariant: an instrument measured against an oracle must not ingest that oracle as a feature; the
// oracle stays outside canon, where the cube-measurement loader reads it.
// invariant: a hand-written scanner: no regex and, on the SUCCESS path, no string copies — a
// decline builds an error message.
// refs: DN-43.D14
// invariant: the log macros stay TEXTUAL in the global module fragment, so no first-party
// declaration leaks through it.
// refs: ADR-3.D4
namespace insight::tokenization
{

namespace
{
    // post: true iff the view opens with the RAS grammar's fixed two-value facility vocabulary.
    // invariant: this is the token that keys the two header shapes apart.
    [[nodiscard]] bool starts_with_facility(std::string_view view) noexcept
    {
        std::string_view probe{view};
        const std::string_view token{sv_take_token(probe)};
        return token == "RAS" || token == "NULL";
    }

    // post: true iff the token is an uppercase identifier, unbounded in length.
    // invariant: this is the cube's WHERE axis, so a token that is not an identifier means the
    // parse is off by a field and the record must DECLINE rather than publish a message fragment.
    // refs: ADR-19.D4
    [[nodiscard]] bool is_subsystem_token(std::string_view token) noexcept
    {
        if (token.empty() || !is_upper(token.front()))
            return false;
        for (const char chr : token)
            if (!is_bgl_identifier_byte(chr))
                return false;
        return true;
    }

    // pre: the remainder starts at the secondary timestamp, which the caller has already proved
    // digit-leading.
    [[nodiscard]] std::optional<BglRecord>
    scan_ras_record(std::string_view epoch, std::string_view node, std::string_view rest) noexcept
    {
        std::string_view tail{rest};
        (void)sv_take_token(tail);

        std::string_view without_node2{tail};
        (void)sv_take_token(without_node2);

        // invariant: the CANONICAL shape is probed FIRST and the order is LOAD-BEARING — the
        // repeated node is itself the literal NULL on 89 296 of the pinned corpus's lines.
        // invariant: probing the early position first would read that node as the facility, take
        // the next two tokens for subsystem and level, and decline 89 296 well-formed records.
        std::string_view fields{};
        if (starts_with_facility(without_node2))
            fields = without_node2;
        else if (starts_with_facility(tail))
            fields = tail;
        else
            return std::nullopt;

        (void)sv_take_token(fields);
        const std::string_view subsystem{sv_take_token(fields)};
        const std::string_view level_word{sv_take_token(fields)};
        if (!is_subsystem_token(subsystem))
            return std::nullopt;
        const LogLevel level{utils::parse_log_level(level_word)};
        if (level == LogLevel::Unknown)
            return std::nullopt;

        // invariant: an EMPTY message body is legitimate — 34 470 corpus lines end at the header
        // — and it is an empty projection rather than the swallow the totality instrument hunts.
        // refs: ADR-16.D9
        return BglRecord{.epoch = epoch,
                         .node = node,
                         .component = subsystem,
                         .content = fields,
                         .declared_level = level};
    }

    // post: the Thunderbird syslog tail — a BSD clock, a host, an optional bracketed tag, then
    // the message.
    [[nodiscard]] std::optional<BglRecord> scan_thunderbird_record(std::string_view epoch,
                                                                   std::string_view node,
                                                                   std::string_view rest) noexcept
    {
        // invariant: the SAME BSD header predicate the syslog strategy claims on, so that shape has
        // one definition.
        if (!is_bsd_syslog_prefix(rest))
            return std::nullopt;

        std::string_view tail{rest};
        (void)sv_take_token(tail);
        (void)sv_take_token(tail);
        (void)sv_take_token(tail);
        const std::string_view host{sv_take_token(tail)};
        if (host.empty())
            return std::nullopt;

        // invariant: no tag delimited means nothing is removed — the component is empty and the
        // content is the whole remainder.
        // invariant: the record is already identified by its header, so an absent daemon name is an
        // ABSENCE and never a reason to decline.
        // invariant: 1 309 corpus bodies used to land on the cube's WHERE axis with an empty
        // content behind them.
        const std::string_view daemon{take_bounded_syslog_tag(tail)};
        return BglRecord{.epoch = epoch,
                         .node = node,
                         .component = daemon,
                         .content = tail,
                         .declared_level = std::nullopt};
    }
} // namespace

std::optional<BglRecord> scan_bgl_record(std::string_view line) noexcept
{
    if (!is_bgl_labelled_prefix(line))
        return std::nullopt;

    std::string_view rest{line};
    (void)sv_take_token(rest);
    const std::string_view epoch{sv_take_token(rest)};
    (void)sv_take_token(rest);
    const std::string_view node{sv_take_token(rest)};
    if (node.empty() || rest.empty())
        return std::nullopt;

    // invariant: the field after the node discriminates the two sub-formats — the secondary
    // timestamp is digit-leading and the Thunderbird month is alphabetic.
    return is_digit(rest.front()) ? scan_ras_record(epoch, node, rest)
                                  : scan_thunderbird_record(epoch, node, rest);
}

std::expected<ParsedLine, std::string> BGLStrategy::parse(std::string_view line,
                                                          ArenaAllocator& /*arena*/) const
{
    const std::optional<BglRecord> record{scan_bgl_record(line)};
    if (!record)
    {
        // invariant: reachable only under an explicit format declaration — auto-detection routes
        // on the same predicate, so a line that reaches parse by scoring has already passed it.
        // refs: ADR-23.D2
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=BGL parse miss");
        return std::unexpected(
            std::string("BGLStrategy: line does not match BGL or Thunderbird format"));
    }

    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp = EventTime::parsed(utils::parse_epoch_timestamp(record->epoch));
    // invariant: this grammar DECLARES its severity in a fixed column and the Thunderbird one has
    // none, so the two branches land in different species — read on one, inferred on the other.
    // invariant: that declared column is the reason the alert-labelled lines were worth claiming
    // — 348 460 of them, every one carrying a fatal-class word nothing was reading.
    // refs: DN-43.D5
    parsed_line.level = record->declared_level.has_value()
                            ? EventLevel::declared(*record->declared_level)
                            : utils::infer_leading_log_level(record->content);
    // invariant: the component is the low-card functional source and the node is the host identity,
    // which is deliberately hors-cube.
    // refs: ADR-19.D4
    parsed_line.component = record->component;
    parsed_line.host = record->node;
    parsed_line.content = record->content;
    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=BGL parsed component={} host={} level={} declared={} "
                      "has_timestamp={}",
                      parsed_line.component, parsed_line.host, to_string(parsed_line.level.value()),
                      record->declared_level.has_value(), parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat BGLStrategy::format() const noexcept
{
    return LogFormat::BGL;
}

double BGLStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr std::string_view::size_type kMinimumCandidateLength{20};
    static constexpr double kBglConfidence{0.90};
    static constexpr double kNoConfidence{0.0};

    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;
    // invariant: the gate IS the grammar — one scan, and a non-zero score means this parse will
    // succeed.
    // invariant: what fails it declines to the raw-text fallback rather than publishing a
    // mis-aligned parse; 10 pinned lines hold a spliced message fragment where a node belongs.
    // refs: DN-43.D2
    if (scan_bgl_record(line).has_value())
        return kBglConfidence;
    return kNoConfidence;
}

} // namespace insight::tokenization
