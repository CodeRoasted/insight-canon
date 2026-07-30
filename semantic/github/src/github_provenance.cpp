module;

module insight.semantic.github;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;

// github_provenance.cpp — the GitHub-Actions dialect CODE TIER (ADR 0024 §2.3). What remains is the
// echoed-source provenance hook and the command-echo SGR grammar it walks. Self-contained: only api
// + spi + std — never a sealed detail shard.
//
// ⚠ THE FORMAT STRATEGY IS GONE (T4 — ADR 0044 §8, ADR 0063 clause 6 item 1). `GitHubActionsStrategy`
// DETECTED a per-line RFC 3339 stamp and peeled it. That stamp is a property of GitHub's *delivery*,
// not of the GHA *dialect* (ADR 0044 §3): the format of a GHA job log is `RawText` and always was,
// and the dialect is the workflow-command VOCABULARY over it (ADR 0064 clause 1). The peel is now
// DECLARED — `IngestDeclaration{.stack = {"api-rfc3339-line-prefix"}}`, unwound by
// `TransportStack::peel` before canon sees the line — so nothing here detects anything.
//
// The deleted decision function is not lost: it is FROZEN VERBATIM into
// `core/tests/transport/test_transport_peel_equivalence_gate.cpp` (ADR 0062; re-homed to core per
// corpus_backed_gates.md § 5 — the SUT is core's peel and the oracle is inline), where it still
// scores the declared peel over 4 082 logs / 22 490 937 lines. That gate is the provenance record;
// `git log` of this file is not.
//
// CONSEQUENCE, stated because it is a real cost and not a tidiness: `kManifest.strategy` is now
// nullptr, so `ComposedPackage::has_strategy` is FALSE for github while `has_echoed_source` stays
// true. This package's code tier is one provenance hook — a byte predicate, not a grammar — which is
// what makes the dialect DATA-ONLY (ADR 0065 clause 5: a generator can only generate from data).

namespace insight::semantic::github
{
namespace
{

    [[nodiscard]] constexpr bool is_digit(char chr) noexcept
    {
        return static_cast<unsigned>(chr) - '0' < 10U;
    }
    [[nodiscard]] constexpr bool is_space(char chr) noexcept
    {
        return chr == ' ' || chr == '\t';
    }

    // ── Echoed-source detection (was src/scan/… kCommandEchoSgrParams / is_echoed_source_line) ──
    constexpr unsigned char kEsc{0x1bU};
    inline constexpr std::array<std::string_view, 2> kCommandEchoSgrParams{
        std::string_view{"36;1"}, std::string_view{"1;36"}};
    inline constexpr std::array<std::string_view, 3> kSgrResetParams{
        std::string_view{"0"}, std::string_view{}, std::string_view{"39"}};

    // Parse a CSI `\x1b[<params>m` at `pos`; on success advance `pos` past the final `m` and return
    // the parameter substring. nullopt when `pos` is not such a sequence. Pure byte walk (F5).
    // NOLINTNEXTLINE(bugprone-exception-escape)
    [[nodiscard]] std::optional<std::string_view> parse_sgr_params(std::string_view line,
                                                                   std::size_t& pos) noexcept
    {
        if (pos + 1U >= line.size() || static_cast<unsigned char>(line[pos]) != kEsc ||
            line[pos + 1U] != '[')
            return std::nullopt;
        const std::size_t params_begin{pos + 2U};
        std::size_t cur{params_begin};
        while (cur < line.size() && (is_digit(line[cur]) || line[cur] == ';'))
            ++cur;
        if (cur >= line.size() || line[cur] != 'm')
            return std::nullopt;
        const std::string_view params{line.substr(params_begin, cur - params_begin)};
        pos = cur + 1U;
        return params;
    }

} // namespace

// True iff `line` is an echoed-source line (D-PROV-1): the ENTIRE visible content is a SINGLE
// SGR-wrapped span — a command-echo SGR (`36;1`/`1;36`), a content run, and a closing reset
// (`0`/empty/`39`) — with no un-wrapped visible bytes outside the span. Operates on the RAW line
// (ANSI intact). Byte-exact state machine (F5).
//
// ⚠ THE LEADING-STAMP SKIP IS RIPPED (T4 — ADR 0044 §8 item 5). This predicate used to begin by
// calling `is_github_actions_prefix` and, on a hit, skipping 28 bytes plus one separator space, so
// it could recognize an echo on a still-stamped line. That was the fifth of the strategy's bundled
// behaviors and it was a DETECTION: a per-line content test deciding where the visible content
// starts. Under a declared stack the caller peels BEFORE canon sees the line, so by the time this
// runs there is no stamp left to skip; keeping the skip would have preserved a second, undeclared,
// content-inferred transport strip inside a provenance hook — exactly the shape T4 exists to delete.
//
// What that costs, stated rather than hidden: a caller that hands canon RAW GHA API bytes WITHOUT
// declaring the transform loses echoed-source provenance on those lines (the wrapper is no longer at
// the head of the line this predicate sees). That is fail-closed on DEPTH and it is the same
// contract as every other declared coordinate — declaring is the path to depth.
bool is_echoed_source(std::string_view line) noexcept
{
    std::size_t pos{0};
    const std::optional<std::string_view> open{parse_sgr_params(line, pos)};
    if (!open || std::ranges::find(kCommandEchoSgrParams, *open) == kCommandEchoSgrParams.end())
        return false;
    while (pos < line.size() && static_cast<unsigned char>(line[pos]) != kEsc)
        ++pos;
    if (pos >= line.size())
        return false;
    const std::optional<std::string_view> close{parse_sgr_params(line, pos)};
    if (!close || std::ranges::find(kSgrResetParams, *close) == kSgrResetParams.end())
        return false;
    while (pos < line.size() && (is_space(line[pos]) || line[pos] == '\r' || line[pos] == '\n'))
        ++pos;
    return pos == line.size();
}

} // namespace insight::semantic::github
