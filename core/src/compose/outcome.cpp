module;
// refs: ADR-3.D4
#include "utils/log_macros.hpp"

module insight.canon;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;
import insight.canon.compose;
import insight.canon.detail.parse;

namespace insight
{

namespace
{
    // invariant: the arena is reset per segment, so live extent is bounded by the longest segment.
    constexpr std::size_t kScanArenaCapacity{std::size_t{64U} * 1024U};

    // post: true only for a single native verdict word — ASCII letters, digits and underscore.
    // refs: STU-6
    [[nodiscard]] constexpr bool is_verdict_word(std::string_view token) noexcept
    {
        if (token.empty())
            return false;
        return std::ranges::all_of(token,
                                   [](const char chr) noexcept
                                   {
                                       return (chr >= 'a' && chr <= 'z') ||
                                              (chr >= 'A' && chr <= 'Z') ||
                                              (chr >= '0' && chr <= '9') || chr == '_';
                                   });
    }

    // invariant: `token` is OWNED — the parse arena is reset between segments, so a view into it
    // would dangle.
    struct OutcomeMarkerMatch
    {
        const insight::semantic::OutcomeMarkerRow* row{nullptr};
        std::string token;
    };

    // post: within one line the longest VALID prefix wins; a row whose shape requirement fails
    // falls through so a shorter row can still claim it.
    // refs: ADR-17.D4
    void improve_match(OutcomeMarkerMatch& best, std::string_view content,
                       const insight::semantic::ComposedSemantics& composed)
    {
        for (const insight::semantic::OutcomeMarkerRow& row : composed.outcome_markers())
        {
            if (!content.starts_with(row.prefix) ||
                (best.row != nullptr && row.prefix.size() <= best.row->prefix.size()))
                continue;
            std::string_view token{content};
            token.remove_prefix(row.prefix.size());
            if (row.shape == insight::semantic::OutcomeMarkerShape::RemainderToken &&
                !is_verdict_word(token))
                continue;
            best.row = &row;
            best.token.assign(token);
        }
    }

    void stream_composed_packages(std::span<const insight::semantic::ComposedPackage> packages)
    {
        if (packages.empty())
        {
            std::cerr << "<none>";
            return;
        }
        for (std::size_t i{0}; i < packages.size(); ++i)
            std::cerr << (i == 0 ? "" : ", ") << '"' << packages[i].name << '"';
    }

} // namespace

std::optional<RunOutcome>
map_outcome_token(std::string_view token,
                  const insight::semantic::ComposedSemantics& composed) noexcept
{
    for (const insight::semantic::OutcomeTokenRow& row : composed.outcome_tokens())
        if (row.token == token)
            return row.outcome;
    return std::nullopt;
}

std::optional<RunOutcome> map_outcome_token_in(std::string_view token, std::string_view vocabulary,
                                               const insight::semantic::ComposedSemantics& composed)
{
    const auto packages{composed.packages()};
    // note: a missing vocabulary is a CONFIG error; an unmapped token is a VALUE error.
    // refs: DN-32.D6
    if (vocabulary.empty())
    {
        std::cerr << "FATAL: insight::map_outcome_token_in — a verdict token (\"" << token
                  << "\") was declared with NO outcome vocabulary to interpret it. The composed "
                     "packages are: ";
        stream_composed_packages(packages);
        std::cerr
            << ".\nA caller-declared verdict is a PAIR — (vocabulary, token) — never a bare "
               "string (DN-32.D6): `failure` / `failed` / `FAILURE` mean the same thing on three "
               "platforms and `UNSTABLE` means nothing on two of them, so a token does not "
               "interpret itself. A side-input verdict is interpreted by the vocabulary of whoever "
               "SUPPLIED it, not by the dialect of whoever WROTE the bytes — name it (e.g. "
               "--outcome-vocabulary) rather than declaring a dialect the bytes do not have.\n"
               "This is a WIRING error, not a resolution failure: the half-pair resolved nothing "
               "and silently disarmed every rule that reads the verdict.\n";
        std::terminate();
    }
    if (std::ranges::none_of(packages, [vocabulary](const insight::semantic::ComposedPackage& pkg)
                             { return pkg.name == vocabulary; }))
    {
        std::cerr << "FATAL: insight::map_outcome_token_in — unknown outcome vocabulary \""
                  << vocabulary << "\". The composed packages are: ";
        stream_composed_packages(packages);
        std::cerr
            << ".\nThis is the vocabulary that INTERPRETS a caller-supplied verdict (DN-32.D6) — "
               "who SUPPLIED the verdict, which is not the same question as the stream's dialect "
               "(who WROTE the bytes). An unknown name is a MISTAKE: it would resolve nothing and "
               "silently disarm every rule that reads the verdict. Declare one of the names above, "
               "or none.\n";
        std::terminate();
    }
    // note: `for_stream` re-derives from the UNFILTERED tables; this view's own rows match nothing.
    // refs: ADR-22.D6
    const insight::semantic::ComposedSemantics view{composed.for_stream(vocabulary, {})};
    return map_outcome_token(token, view);
}

// post: the LAST line carrying a marker match wins — a run has one terminal verdict.
// invariant: byte-exact ASCII compare and an integer line index; no float, no wall-clock.
// refs: ADR-17.D4, BIB:determinism_model
RunOutcomeScan scan_run_outcome(std::span<const std::string> lines,
                                const insight::semantic::ComposedSemantics& composed)
{
    RunOutcomeScan scan;
    if (composed.outcome_tokens().empty() && composed.outcome_markers().empty())
        return scan;

    tokenization::ArenaAllocator arena{kScanArenaCapacity};
    tokenization::LogParser parser{arena, composed};
    for (const std::string& line : lines)
    {
        if (line.empty())
            continue;
        // note: a bare CR also starts a line: a pre-18.9 GitLab epilogue is framed with one.
        OutcomeMarkerMatch best;
        std::string_view remaining{line};
        while (true)
        {
            const auto carriage_return{remaining.find('\r')};
            if (const std::string_view segment{remaining.substr(0, carriage_return)};
                !segment.empty())
            {
                if (const auto parsed{parser.parse_line(segment)}; parsed.has_value())
                    improve_match(best, parsed->content, composed);
                arena.reset();
            }
            if (carriage_return == std::string_view::npos)
                break;
            remaining.remove_prefix(carriage_return + 1);
        }
        if (best.row != nullptr)
        {
            scan.marker_present = true;
            if (best.row->shape == insight::semantic::OutcomeMarkerShape::PrefixIsVerdict)
            {
                scan.verdict = best.row->outcome;
                scan.token.clear();
            }
            else
            {
                scan.verdict.reset();
                scan.token = std::move(best.token);
            }
        }
    }
    return scan;
}

// post: the strict total ladder — authoritative side-input, then the console tail, then Unknown;
// never a reconciliation.
// refs: ADR-17.D5, SRC-D-OUT-RUN-1
RunOutcomeResolution resolve_run_outcome(SideInputVerdict side_input, const RunOutcomeScan& scan,
                                         const insight::semantic::ComposedSemantics& stream_view,
                                         const insight::semantic::ComposedSemantics& vocabularies)
{
    const std::string_view side_input_token{side_input.token};
    // note: a verdict in the bytes is read by its WRITER, a side-input by its SUPPLIER.
    // refs: DN-32.D6
    const insight::semantic::ComposedSemantics& composed{stream_view};
    RunOutcomeResolution resolution;

    std::optional<RunOutcome> console_mapped;
    if (scan.marker_present)
    {
        // note: only a RemainderToken verdict comes off the LINE and can miss the vocabulary.
        // refs: ADR-17.D5
        console_mapped = scan.verdict ? scan.verdict : map_outcome_token(scan.token, composed);
        if (console_mapped)
            resolution.console = *console_mapped;
        else
            resolution.note = "run-outcome: console verdict '" + scan.token +
                              "' is not in the composed outcome vocabulary (fail-closed: Unknown)";
    }

    // note: an ABSENT declaration skips this rung; an INCOMPLETE one terminates before it.
    // refs: DN-32.D7
    if (!side_input_token.empty())
    {
        const std::optional<RunOutcome> mapped{
            map_outcome_token_in(side_input_token, side_input.vocabulary, vocabularies)};
        if (mapped)
        {
            resolution.outcome = *mapped;
            resolution.authoritative = true;
            if (console_mapped && *console_mapped != *mapped)
            {
                // note: a console tail can be a local or caught outcome: legible, never a tiebreak.
                // refs: ADR-17.D5, F-SRC-insight-canon:test_jenkins_outcome.cpp
                resolution.divergent = true;
                INSIGHT_LOG_TRACE(logging::parser_logger(),
                                  "run_outcome: authoritative={} console={} -> {} (divergent "
                                  "console tail not consulted, SRC-D-OUT-RUN-1)",
                                  to_string(*mapped), to_string(*console_mapped),
                                  to_string(resolution.outcome));
            }
            return resolution;
        }
        resolution.note = "run-outcome: side-input '" + std::string{side_input_token} +
                          "' is not in the '" + std::string{side_input.vocabulary} +
                          "' outcome vocabulary (fail-closed: falling back)";
    }

    if (console_mapped)
    {
        resolution.outcome = *console_mapped;
        return resolution;
    }

    return resolution;
}

} // namespace insight
