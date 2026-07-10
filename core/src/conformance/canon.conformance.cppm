// insight.canon.conformance — the permanent, package-agnostic CONFORMANCE KIT (ADR 0024 §2.3, SP-2).
// The canon-shipped harness EVERY semantic package (ours or an external author's) must pass. A package's
// test target instantiates it in one line:
//
//     import insight.canon.conformance;
//     import insight.semantic.github;
//     TEST(Conformance, GithubPackagePassesTheKit) {
//         const auto report{insight::semantic::conformance::run(insight::semantic::github::kManifest)};
//         for (const auto& check : report.checks)
//             EXPECT_TRUE(check.passed) << "[" << check.name << "] " << check.detail;
//     }
//
// It is the OPEN-SOURCE HONESTY MECHANISM (SP-2): the identical kit ships installed so an external
// package author runs the same gate CodeRoast's own packages are held to. Verbose-on-failure matters
// doubly here — a failing check must diagnose itself for someone who has never read canon's internals.
//
// PURE by design: imports only the facade (compose/ComposedSemantics + the recognition walkers) + spi
// (the row grammar + manifest). NO gtest dependency (a package's ~10-line test file adapts the report
// to its own framework), and — deliberately — NO global `operator new` override (that must never ship in
// the canon library). The DYNAMIC heap/float-freedom guard is homed where a `new` override is legitimate
// (the canon core test binary, tests/compose/test_semantic_walkers.cpp): allocation-freedom is a
// canon-ALGORITHM property, proven once over the walkers, not re-proven per data-only package.
//
// The probe corpus is DERIVED FROM THE MANIFEST (each row's own key becomes a probe line), so the kit is
// self-adapting — it works for any package's vocabulary with zero per-package configuration.
module;

export module insight.canon.conformance;
import insight.canon.internal; // std
import insight.canon;          // compose / ComposedSemantics / classify / recognize / recognize_location + enums
import insight.canon.spi;      // SemanticPackageManifest + the grammar rows + kAnyFormat + find_conflict

export namespace insight::semantic::conformance
{

// One check's outcome. `detail` carries the verbose-on-failure diagnostic (actual-vs-expected, the
// offending row's key) and is empty on pass.
struct CheckResult
{
    std::string_view name;
    bool passed;
    std::string detail;
};

// The kit's report over one manifest. Iterate `checks` in the package's own assertion framework.
struct Report
{
    std::vector<CheckResult> checks;

    [[nodiscard]] bool all_passed() const noexcept
    {
        return std::ranges::all_of(checks, [](const CheckResult& check) noexcept { return check.passed; });
    }

    // "K/N checks passed" + the names of any failures — a one-line summary for the top-level assertion.
    [[nodiscard]] std::string summary() const
    {
        std::size_t passed{0};
        std::string failed;
        for (const CheckResult& check : checks)
        {
            if (check.passed)
            {
                ++passed;
                continue;
            }
            failed += failed.empty() ? " (failed: " : ", ";
            failed += check.name;
        }
        if (!failed.empty())
            failed += ')';
        return std::to_string(passed) + '/' + std::to_string(checks.size()) + " conformance checks passed" +
               failed;
    }
};

// Run the full conformance kit over one package manifest. Deterministic, single-threaded, seedless — the
// recognizers are pure byte functions, so every check is a pure function of the manifest data.
[[nodiscard]] Report run(const SemanticPackageManifest& manifest);

} // namespace insight::semantic::conformance

// ════════════════════════════════════════════════════════════════════════════════════════════════════
// Implementation (inline in the module interface — the kit is small, pure, and self-contained so an
// external author can read the whole gate in one file).
// ════════════════════════════════════════════════════════════════════════════════════════════════════
namespace insight::semantic::conformance
{
namespace
{

// A byte is ASCII when its high bit is clear (< 0x80). Locale-safety is structural: every row key is
// ASCII and every matcher is a byte comparison, so no locale-sensitive path exists (the II-6 / ASCII-only
// determinism hazard — det_math musts). We ASSERT the ASCII property rather than argue it.
[[nodiscard]] bool is_ascii(std::string_view str) noexcept
{
    return std::ranges::all_of(str, [](char chr) noexcept { return static_cast<unsigned char>(chr) < 0x80U; });
}

// Two concrete formats distinct from `gate`, for the "does NOT fire cross-format" leg. GitHubActions and
// JSON cover a dialect gate and a representation gate; whichever equals `gate` is skipped by the caller.
constexpr std::array<insight::LogFormat, 4> kProbeFormats{insight::LogFormat::GitHubActions,
                                                          insight::LogFormat::JSON,
                                                          insight::LogFormat::Syslog,
                                                          insight::LogFormat::RawText};

// A probe line for a prefix row: the key verbatim + a trailing payload token so payload-extract has
// content to return. The key is line-anchored, so `key + " probe"` matches iff the row fires.
[[nodiscard]] std::string probe_for(std::string_view prefix) { return std::string{prefix} + " probe"; }

// ── Check 1: determinism — identical identity + identical recognizer output across independent runs ──
CheckResult check_determinism(const SemanticPackageManifest& manifest)
{
    const std::array<SemanticPackageManifest, 1> one{manifest};
    const ComposedSemantics first{compose(one)};
    const ComposedSemantics second{compose(one)};
    if (first.identity() != second.identity())
        return {.name = "determinism.identity",
                .passed = false,
                .detail = "compose({manifest}) produced two DIFFERENT semantic_identity hashes across "
                          "independent runs: " +
                          first.identity_hex() + " vs " + second.identity_hex() +
                          " — composition is not a pure function of the manifest (SP-6 violated)."};

    // Recognizer output must be bit-identical run-to-run for every derived probe.
    for (const IntentMarkerRow& row : manifest.markers)
    {
        const std::string probe{probe_for(row.prefix)};
        const auto lhs{insight::tokenization::recognize(probe, row.format_gate, first)};
        const auto rhs{insight::tokenization::recognize(probe, row.format_gate, second)};
        if (lhs.kind != rhs.kind || lhs.name != rhs.name || lhs.discriminant != rhs.discriminant)
            return {.name = "determinism.recognize",
                    .passed = false,
                    .detail = "recognize(\"" + probe + "\") diverged across identical compositions for "
                              "marker key \"" +
                              std::string{row.prefix} + "\"."};
    }
    return {.name = "determinism", .passed = true, .detail = {}};
}

// ── Check 2: format-gate honesty — a gated row is inert outside its format; kAnyFormat fires anywhere ──
CheckResult check_format_gate_honesty(const SemanticPackageManifest& manifest, const ComposedSemantics& composed)
{
    // Structural roles.
    for (const StructuralRoleRow& row : manifest.roles)
    {
        const std::string probe{probe_for(row.prefix)};
        if (row.format_gate == kAnyFormat)
        {
            // Must fire regardless of routed format — check two distinct formats.
            for (const insight::LogFormat fmt : {insight::LogFormat::JSON, insight::LogFormat::Syslog})
                if (insight::tokenization::classify(probe, fmt, composed) != row.role)
                    return {.name = "format_gate.role_any",
                            .passed = false,
                            .detail = "kAnyFormat role key \"" + std::string{row.prefix} +
                                      "\" failed to fire under " + std::string{insight::to_string(fmt)} +
                                      " — an ungated row must fire on every format."};
        }
        else
        {
            // Must be inert under a DIFFERENT concrete format.
            for (const insight::LogFormat fmt : kProbeFormats)
            {
                if (fmt == row.format_gate)
                    continue;
                if (insight::tokenization::classify(probe, fmt, composed) != insight::StructuralRole::None)
                    return {.name = "format_gate.role_leak",
                            .passed = false,
                            .detail = "role key \"" + std::string{row.prefix} + "\" (gated to " +
                                      std::string{insight::to_string(row.format_gate)} +
                                      ") FIRED cross-format under " +
                                      std::string{insight::to_string(fmt)} + " — II-6 gate leak."};
            }
        }
    }
    // Intent markers (always concretely gated by construction — II-6). Inert under any other format.
    for (const IntentMarkerRow& row : manifest.markers)
    {
        const std::string probe{probe_for(row.prefix)};
        for (const insight::LogFormat fmt : kProbeFormats)
        {
            if (fmt == row.format_gate || row.format_gate == kAnyFormat)
                continue;
            if (insight::tokenization::recognize(probe, fmt, composed).kind != insight::tokenization::IntentMarkerKind::None)
                return {.name = "format_gate.marker_leak",
                        .passed = false,
                        .detail = "marker key \"" + std::string{row.prefix} + "\" (gated to " +
                                  std::string{insight::to_string(row.format_gate)} + ") FIRED cross-format under " +
                                  std::string{insight::to_string(fmt)} + " — II-6 gate leak."};
        }
    }
    // Outcome tokens (grammar-2, always concretely gated — a dialect's verdict string never resolves
    // under another format, ADR 0025 §3.1).
    for (const OutcomeTokenRow& row : manifest.outcome_tokens)
        for (const insight::LogFormat fmt : kProbeFormats)
        {
            if (fmt == row.format_gate || row.format_gate == kAnyFormat)
                continue;
            if (insight::map_outcome_token(row.token, fmt, composed).has_value())
                return {.name = "format_gate.outcome_leak",
                        .passed = false,
                        .detail = "outcome token \"" + std::string{row.token} + "\" (gated to " +
                                  std::string{insight::to_string(row.format_gate)} +
                                  ") RESOLVED cross-format under " +
                                  std::string{insight::to_string(fmt)} + " — II-6 gate leak."};
        }
    return {.name = "format_gate_honesty", .passed = true, .detail = {}};
}

// ── Check 3: ASCII / locale safety — every row key is ASCII (byte-only matching ⇒ locale-independent) ──
CheckResult check_ascii_safety(const SemanticPackageManifest& manifest)
{
    const auto span_ok{[](std::span<const std::string_view> strings) noexcept
                       { return std::ranges::all_of(strings, is_ascii); }};
    for (const StructuralRoleRow& row : manifest.roles)
        if (!is_ascii(row.prefix))
            return {.name = "ascii.role", .passed = false,
                    .detail = "role key contains a non-ASCII byte: \"" + std::string{row.prefix} + "\"."};
    for (const IntentMarkerRow& row : manifest.markers)
        if (!is_ascii(row.prefix) || !span_ok(row.payload_excludes))
            return {.name = "ascii.marker", .passed = false,
                    .detail = "marker key or payload-exclusion entry contains a non-ASCII byte: \"" +
                              std::string{row.prefix} + "\"."};
    for (const OutcomeTokenRow& row : manifest.outcome_tokens)
        if (!is_ascii(row.token))
            return {.name = "ascii.outcome_token", .passed = false,
                    .detail = "outcome token contains a non-ASCII byte: \"" + std::string{row.token} + "\"."};
    for (const OutcomeMarkerRow& row : manifest.outcome_markers)
        if (!is_ascii(row.prefix))
            return {.name = "ascii.outcome_marker", .passed = false,
                    .detail = "outcome-marker key contains a non-ASCII byte: \"" + std::string{row.prefix} + "\"."};
    for (const LevelLiftRow& row : manifest.level_lifts)
        if (!is_ascii(row.prefix))
            return {.name = "ascii.level_lift", .passed = false,
                    .detail = "level-lift key contains a non-ASCII byte: \"" + std::string{row.prefix} + "\"."};
    for (const LocationRow& row : manifest.locations)
        if (!span_ok(row.infixes) || !span_ok(row.extensions) || !span_ok(row.prefixes) || !span_ok(row.suffixes))
            return {.name = "ascii.location", .passed = false,
                    .detail = "a location row carries a non-ASCII vocabulary token (locale-unsafe)."};
    return {.name = "ascii_safety", .passed = true, .detail = {}};
}

// ── Check 4: grammar well-formedness — non-empty keys, no self-conflict, LocationRow param/kind match ──
CheckResult check_grammar_wellformed(const SemanticPackageManifest& manifest)
{
    for (const StructuralRoleRow& row : manifest.roles)
        if (row.prefix.empty())
            return {.name = "grammar.empty_role", .passed = false, .detail = "a structural-role row has an empty prefix."};
    for (const IntentMarkerRow& row : manifest.markers)
        if (row.prefix.empty())
            return {.name = "grammar.empty_marker", .passed = false, .detail = "an intent-marker row has an empty prefix."};
    for (const LevelLiftRow& row : manifest.level_lifts)
        if (row.prefix.empty())
            return {.name = "grammar.empty_level_lift", .passed = false, .detail = "a level-lift row has an empty prefix."};
    for (const OutcomeTokenRow& row : manifest.outcome_tokens)
        if (row.token.empty())
            return {.name = "grammar.empty_outcome_token", .passed = false, .detail = "an outcome-token row has an empty token."};
    for (const OutcomeMarkerRow& row : manifest.outcome_markers)
        if (row.prefix.empty())
            return {.name = "grammar.empty_outcome_marker", .passed = false, .detail = "an outcome-marker row has an empty prefix."};
    for (const IntentMarkerRow& row : manifest.markers)
        for (const std::string_view exclude : row.payload_excludes)
            if (exclude.empty())
                return {.name = "grammar.empty_payload_exclude", .passed = false,
                        .detail = "marker key \"" + std::string{row.prefix} +
                                  "\" carries an empty payload-exclusion entry (would exclude every payload)."};

    // Self-conflict: a package must not carry an exact-duplicate key within itself (the runtime compose
    // would fatal on it — the kit catches it as a well-formedness failure, not a crash).
    const std::array<SemanticPackageManifest, 1> one{manifest};
    if (const ConflictInfo conflict{find_conflict(one)}; conflict.has_conflict)
        return {.name = "grammar.self_conflict", .passed = false,
                .detail = "the package carries an exact-duplicate " + std::string{conflict.kind} +
                          " key \"" + std::string{conflict.key} + "\" within its own rows (would fatal at compose)."};

    // LocationRow: the selected LocationMatchKind must be parameterized by the params that algorithm reads.
    for (const LocationRow& row : manifest.locations)
    {
        const auto fail{[&](std::string_view why)
                        { return CheckResult{.name = "grammar.location_params", .passed = false,
                                             .detail = std::string{why}}; }};
        switch (row.kind)
        {
        case LocationMatchKind::TestSpecExtension:
            if (row.infixes.empty() || row.extensions.empty())
                return fail("TestSpecExtension row needs non-empty infixes AND extensions.");
            break;
        case LocationMatchKind::PrefixAndExtension:
            if (row.extensions.empty() || (row.prefixes.empty() && row.suffixes.empty()))
                return fail("PrefixAndExtension row needs extensions AND at least one of prefixes/suffixes.");
            break;
        case LocationMatchKind::SuffixSet:
            if (row.suffixes.empty())
                return fail("SuffixSet row needs a non-empty suffixes set.");
            break;
        }
    }
    return {.name = "grammar_wellformed", .passed = true, .detail = {}};
}

// ── Check 5: code-tier well-behaved — the strategy/hook (if any) are deterministic + in-range ──
CheckResult check_code_tier(const SemanticPackageManifest& manifest)
{
    if (manifest.strategy != nullptr)
    {
        const std::unique_ptr<insight::tokenization::IFormatStrategy> strategy{manifest.strategy()};
        if (strategy == nullptr)
            return {.name = "code_tier.strategy_null", .passed = false,
                    .detail = "the strategy factory returned nullptr."};
        if (strategy->format() == insight::LogFormat::Unknown)
            return {.name = "code_tier.strategy_format", .passed = false,
                    .detail = "the dialect strategy reports LogFormat::Unknown — a dialect must own a concrete format."};
        // confidence() must be O(1), in [0,1], and deterministic (same line → same score).
        constexpr std::string_view kProbe{"2026-01-01T00:00:00.0000000Z probe line"};
        const double first{strategy->confidence(kProbe)};
        const double second{strategy->confidence(kProbe)};
        if (!(first >= 0.0 && first <= 1.0) || first != second)
            return {.name = "code_tier.confidence", .passed = false,
                    .detail = "confidence() is out of [0,1] or non-deterministic (" + std::to_string(first) +
                              " then " + std::to_string(second) + ")."};
    }
    if (manifest.echoed_source != nullptr)
    {
        constexpr std::string_view kProbe{"plain unwrapped line"};
        if (manifest.echoed_source(kProbe) != manifest.echoed_source(kProbe))
            return {.name = "code_tier.echoed_source", .passed = false,
                    .detail = "the echoed-source hook is non-deterministic on a fixed line."};
    }
    return {.name = "code_tier", .passed = true, .detail = {}};
}

} // namespace

Report run(const SemanticPackageManifest& manifest)
{
    const std::array<SemanticPackageManifest, 1> one{manifest};
    const ComposedSemantics composed{compose(one)};

    Report report;
    report.checks.push_back(check_determinism(manifest));
    report.checks.push_back(check_format_gate_honesty(manifest, composed));
    report.checks.push_back(check_ascii_safety(manifest));
    report.checks.push_back(check_grammar_wellformed(manifest));
    report.checks.push_back(check_code_tier(manifest));
    return report;
}

} // namespace insight::semantic::conformance
