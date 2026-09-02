module;
#include "utils/log_macros.hpp" // textual macro layer (ADR-3.D4)

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan; // fast_gates predicates + sv_* scan primitives

// BGLStrategy — parses BlueGene/L (BGL) and Thunderbird supercomputer logs.
//
// BGL:         "- 1117838570 2005.06.03 R02-M1-N0 addr1 addr2 RAS KERNEL LEVEL msg"
// BGL alert:   "KERNDTLB 1117838570 2005.06.03 R02-M1-N0 addr1 addr2 RAS KERNEL FATAL msg"
// Thunderbird: "- 1131566461 2005.11.09 dn228 Nov 9 12:01:01 dn228/dn228 crond[2915]: msg"
//
// Hand-written scanner: zero RE2, zero string copies.
//
// THE FIRST COLUMN IS THE CORPUS'S ANSWER KEY (DN-43.D14). LogHub's curators prepended an
// alert-class column to records BGL's RAS system wrote without one: `-` on a normal record, a
// bounded uppercase class name on an anomalous one. The strategy VALIDATES it — it is part of the
// grammar — and carries it in NO projection field, so a labelled line projects identically to its
// `-` twin and one event class cannot split 42 ways by curation. An instrument measured against an
// oracle must not ingest that oracle as a feature; the oracle stays outside canon, where the
// cube-measurement loader reads it.

namespace insight::tokenization
{

namespace
{
    // <FACILITY> — the RAS grammar's fixed two-value vocabulary, and the token that keys the two
    // BGL header shapes apart.
    [[nodiscard]] bool starts_with_facility(std::string_view view) noexcept
    {
        std::string_view probe{view};
        const std::string_view token{sv_take_token(probe)};
        return token == "RAS" || token == "NULL";
    }

    // <SUBSYS> — `[A-Z][A-Z0-9_]*`, unbounded in length (KERNEL, APP, DISCOVERY, MMCS, HARDWARE,
    // MONITOR, LINKCARD, CMCS, BGLMASTER, SERV_NET over the pinned corpus). This is the cube's
    // WHERE axis, so a token that is not an identifier means the parse is off by a field and the
    // record must decline rather than publish a message fragment as a dimension.
    [[nodiscard]] bool is_subsystem_token(std::string_view token) noexcept
    {
        if (token.empty() || !is_upper(token.front()))
            return false;
        for (const char chr : token)
            if (!is_bgl_identifier_byte(chr))
                return false;
        return true;
    }

    // "<ts2> [<node2>] <FACILITY> <SUBSYS> <LEVEL> <msg>" — the RAS record body.
    // `rest` starts at <ts2>, which the caller has already proved digit-leading.
    [[nodiscard]] std::optional<BglRecord>
    scan_ras_record(std::string_view epoch, std::string_view node, std::string_view rest) noexcept
    {
        std::string_view tail{rest};
        (void)sv_take_token(tail); // <ts2> — a second, redundant rendering of <epoch>

        std::string_view without_node2{tail};
        (void)sv_take_token(without_node2); // <node2> — the repeated node, present on most records

        // THE CANONICAL SHAPE IS PROBED FIRST AND THE ORDER IS LOAD-BEARING. <node2> is itself the
        // literal `NULL` on 89 296 of the pinned corpus's 4 747 963 lines, so probing the early
        // position first reads that node as the facility, takes `RAS` for the subsystem and
        // `MMCS` for the level, and declines 89 296 well-formed records.
        std::string_view fields{};
        if (starts_with_facility(without_node2))
            fields = without_node2;
        else if (starts_with_facility(tail))
            fields = tail; // the second header shape: <node> is `-` and no node is repeated
        else
            return std::nullopt;

        (void)sv_take_token(fields); // <FACILITY> — validated grammar, not a dimension
        const std::string_view subsystem{sv_take_token(fields)};
        const std::string_view level_word{sv_take_token(fields)};
        if (!is_subsystem_token(subsystem))
            return std::nullopt;
        const LogLevel level{utils::parse_log_level(level_word)};
        if (level == LogLevel::Unknown)
            return std::nullopt;

        // `fields` is now the message body, empty when the record's body genuinely ended at the
        // header — 34 470 corpus lines, and a legitimate empty projection (DN-43.D6 member (a)),
        // not the swallow the projection-totality instrument hunts.
        return BglRecord{.epoch = epoch,
                         .node = node,
                         .component = subsystem,
                         .content = fields,
                         .declared_level = level};
    }

    // "<Mon> <DD> <HH:MM:SS> <host> [<tag>[pid]:] <msg>" — the Thunderbird syslog tail.
    [[nodiscard]] std::optional<BglRecord> scan_thunderbird_record(std::string_view epoch,
                                                                   std::string_view node,
                                                                   std::string_view rest) noexcept
    {
        // The same BSD header predicate SyslogStrategy claims on — one definition of that shape.
        if (!is_bsd_syslog_prefix(rest))
            return std::nullopt;

        std::string_view tail{rest};
        (void)sv_take_token(tail);                        // <Mon>
        (void)sv_take_token(tail);                        // <DD>
        (void)sv_take_token(tail);                        // <HH:MM:SS>
        const std::string_view host{sv_take_token(tail)}; // redundant with <node>
        if (host.empty())
            return std::nullopt;

        // No tag delimited means nothing is removed: component empty, content the whole remainder
        // (`exiting on signal 15`, 1 309 corpus lines whose body used to land on the cube's WHERE
        // axis with an empty content behind it). The record is already identified by its BGL
        // header, so an absent daemon name is an absence, never a reason to decline.
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
    (void)sv_take_token(rest);                         // <label> — validated above, carried nowhere
    const std::string_view epoch{sv_take_token(rest)}; // <epoch>
    (void)sv_take_token(rest);                         // <date> — redundant with <epoch>
    const std::string_view node{sv_take_token(rest)};  // <node>
    if (node.empty() || rest.empty())
        return std::nullopt;

    // The field after <node> discriminates the two sub-formats: BGL's secondary timestamp is
    // digit-leading ("2005-06-08-08.53.27…"), Thunderbird's syslog month is alphabetic.
    return is_digit(rest.front()) ? scan_ras_record(epoch, node, rest)
                                  : scan_thunderbird_record(epoch, node, rest);
}

std::expected<ParsedLine, std::string> BGLStrategy::parse(std::string_view line,
                                                          ArenaAllocator& /*arena*/) const
{
    const std::optional<BglRecord> record{scan_bgl_record(line)};
    if (!record)
    {
        // Reachable only under set_format(): auto-detection routes on the same predicate, so a
        // line that reaches parse() by scoring has already passed it (ADR-23.D2).
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=BGL parse miss");
        return std::unexpected(
            std::string("BGLStrategy: line does not match BGL or Thunderbird format"));
    }

    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp = EventTime::parsed(utils::parse_epoch_timestamp(record->epoch));
    // BGL DECLARES its severity in a fixed column and Thunderbird has none, so the two branches
    // land in different species: read on one, inferred from the message body on the other
    // (DN-43.D5). The RAS column is the reason the alert-labelled lines were worth claiming —
    // 348 460 of them, every one carrying a declared fatal-class word nothing was reading.
    parsed_line.level = record->declared_level.has_value()
                            ? EventLevel::declared(*record->declared_level)
                            : utils::infer_leading_log_level(record->content);
    parsed_line.component = record->component; // F3b D-F3b-1: the cube's low-card functional source
    parsed_line.host = record->node;           // F3b D-F3b-1: the node identity (hors-cube)
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
    // The gate IS the grammar: one scan, and a non-zero score means "this parse will succeed"
    // (DN-43.D2). What fails it declines to the raw-text fallback rather than publishing a
    // mis-aligned parse — 10 lines of the pinned BGL corpus, each one a record whose <node2> field
    // holds a spliced message fragment instead of a node.
    if (scan_bgl_record(line).has_value())
        return kBglConfidence;
    return kNoConfidence;
}

} // namespace insight::tokenization
