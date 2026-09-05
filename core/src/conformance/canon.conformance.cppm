/***************************************************************************************************
D-LSRC-5 — the conformance kit SHIPS INSTALLED — a gate the vendor keeps is a claim, not a gate
Absorbs SRC-SP-2, whose form ADR-26.D5 retires: this block IS that code's statement, and it
sits at the code's only declaration-position site. The kit is installed and public, so an
external semantic-package author runs the IDENTICAL gate CodeRoast's own packages are held
to — same source, same checks, same verdicts.
A gate held privately cannot make "every package passes the kit" falsifiable by anyone outside
this tree, and an honesty mechanism only its author can run is the thing it claims to replace.
Two obligations follow from shipping it and neither is negotiable. Verbose-on-failure is
doubled here: a failing check must diagnose itself to a reader who has never seen canon's
internals, so every `detail` carries actual-vs-expected and the offending row's key. And the
kit stays PURE — the facade and spi only, no gtest, and no global `operator new` override,
which must never ship inside the canon library; the heap/float-freedom guard is homed where a
`new` override is legitimate, in the canon core test binary, because allocation-freedom is a
canon-ALGORITHM property proven once over the walkers, not re-proven per data-only package.
***************************************************************************************************/
// refs: ADR-17, F-SRC-insight-canon:test_semantic_walkers.cpp
// invariant: the probe corpus is DERIVED from the manifest, so the kit self-adapts to any package's
// vocabulary with zero per-package configuration.
module;

export module insight.canon.conformance;
import insight.canon.internal;
import insight.canon;
import insight.canon.spi;

export namespace insight::semantic::conformance
{

// invariant: `detail` carries the verbose-on-failure diagnostic — actual-vs-expected and the
// offending row's key — and is empty on a pass.
struct CheckResult
{
    std::string_view name;
    bool passed;
    std::string detail;
};

struct Report
{
    std::vector<CheckResult> checks;

    [[nodiscard]] bool all_passed() const noexcept
    {
        return std::ranges::all_of(checks,
                                   [](const CheckResult& check) noexcept { return check.passed; });
    }

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
        return std::to_string(passed) + '/' + std::to_string(checks.size()) +
               " conformance checks passed" + failed;
    }
};

// post: one check per conformance property over one manifest; deterministic, single-threaded and
// seedless, so the report is a pure function of the manifest data.
[[nodiscard]] Report run(const SemanticPackageManifest& manifest);

// refs: ADR-23, STU-8, SRC-SID-2
// post: for every recognition marker, its PAIRED generation row is materialized and canon must
// recognize the declared kind, child order and payload back.
// invariant: both projections are read off the MANIFEST, so the rows this closes over are the rows
// `semantic_identity` hashes — one array, never two.
// note: a miss is a declaration-expressivity defect, never a knob.
[[nodiscard]] Report round_trip_report(const SemanticPackageManifest& manifest,
                                       const ComposedSemantics& composed);

// refs: DN-17.D21
// post: one check per manifest member, locating WHERE two rulesets differ — which member, which
// index, which field, and both values.
// invariant: rows pair BY INDEX, matching the identity serializer, so two manifests holding the
// same rows in a different ORDER are reported as differing.
// invariant: the two CODE-TIER members are compared by PRESENCE only — two packages hold two
// different symbols, so a pointer compare could never pass.
// note: two empty manifests are equivalent — non-vacuity is the caller's subject.
[[nodiscard]] Report manifest_equivalence_report(const SemanticPackageManifest& lhs,
                                                 const SemanticPackageManifest& rhs);

// refs: BIB:jenkins_dialect
// post: the row's writer dual materialized with a probe payload; "" for an UNPAIRED row, which
// `check_grammar_wellformed` already reds.
// invariant: EXPORTED so a consumer can assert the probe fires on its own dialect stream, which
// makes a regression a loud red OUTSIDE the kit.
[[nodiscard]] std::string marker_probe_for(const IntentMarkerRow& row,
                                           std::span<const IntentEmitRow> emits);

} // namespace insight::semantic::conformance

// note: inline in the interface so an external author reads the whole gate in one file.
namespace insight::semantic::conformance
{

// invariant: one benign single-token ASCII word carrying no structural token and no byte a payload
// extractor trims, so a probe that fails to fire is a real failure.
// note: module linkage, not the unnamed namespace — the exported probe renders it.
constexpr std::string_view kProbePayload{"probe"};

// invariant: the probe is the row's own writer dual, so it self-adapts to every present and future
// extractor by construction — the two projections are duals.
// note: `prefix + " probe"` is valid only for a RemainderAfterPrefix row.
std::string marker_probe_for(const IntentMarkerRow& row, std::span<const IntentEmitRow> emits)
{
    const IntentEmitRow* writer{paired_writer_row(row, emits)};
    return writer == nullptr ? std::string{} : render_row(*writer, kProbePayload);
}

namespace
{

    // refs: SRC-II-6
    // invariant: every row key is ASCII and every matcher is a byte comparison, so no
    // locale-sensitive path exists; the property is ASSERTED here rather than argued.
    [[nodiscard]] bool is_ascii(std::string_view str) noexcept
    {
        return std::ranges::all_of(str, [](char chr) noexcept
                                   { return static_cast<unsigned char>(chr) < 0x80U; });
    }

    // refs: ADR-22
    // invariant: the two differ in KIND — one is the caller declining to declare, which a
    // kAnyDialect row must still survive, the other a real foreign package name.
    // note: the foreign name must be one the manifest is not, or `for_stream` fatals.
    constexpr std::string_view kUndeclaredDialect{};
    constexpr std::string_view kForeignDialect{"conformance-foreign-dialect"};

    // invariant: a role row's key is line-anchored and carries no payload grammar, so `key + "
    // probe"` matches iff the row fires.
    [[nodiscard]] std::string probe_for(std::string_view prefix)
    {
        return std::string{prefix} + ' ' + std::string{kProbePayload};
    }

    // refs: ADR-21.D4, LSRC-5
    // invariant: the kit's ONE door to the walkers' NormalizedContent, and stage 1 is a FIXED POINT
    // on its escape-free probes, so no count can move.
    // note: never the LogParser mint here — that would grow its friend list to two.
    [[nodiscard]] insight::tokenization::NormalizedContent normalized_probe(std::string_view probe,
                                                                            std::string& scratch)
    {
        return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
    }

    CheckResult check_determinism(const SemanticPackageManifest& manifest)
    {
        const std::array<SemanticPackageManifest, 1> one{manifest};
        const ComposedSemantics first{compose(one)};
        const ComposedSemantics second{compose(one)};
        if (first.identity() != second.identity())
            return {
                .name = "determinism.identity",
                .passed = false,
                .detail =
                    "compose({manifest}) produced two DIFFERENT semantic_identity hashes across "
                    "independent runs: " +
                    first.identity_hex() + " vs " + second.identity_hex() +
                    " — composition is not a pure function of the manifest (SP-6 violated)."};

        for (const IntentMarkerRow& row : manifest.markers)
        {
            const std::string probe{marker_probe_for(row, manifest.emits)};
            std::string scratch;
            const ComposedSemantics first_view{first.for_stream(manifest.name, row.channel_gate)};
            const ComposedSemantics second_view{second.for_stream(manifest.name, row.channel_gate)};
            const auto lhs{
                insight::tokenization::recognize(normalized_probe(probe, scratch), first_view)};
            const auto rhs{
                insight::tokenization::recognize(normalized_probe(probe, scratch), second_view)};
            if (lhs.kind != rhs.kind || lhs.name != rhs.name ||
                lhs.discriminant != rhs.discriminant)
                return {.name = "determinism.recognize",
                        .passed = false,
                        .detail = "recognize(\"" + probe +
                                  "\") diverged across identical compositions for "
                                  "marker key \"" +
                                  std::string{row.prefix} + "\"."};
        }
        return {.name = "determinism", .passed = true, .detail = {}};
    }

    // invariant: the leak view composes the manifest WITH a synthetic row-less package and resolves
    // to THAT name, so gated rows are legitimately absent.
    [[nodiscard]] ComposedSemantics dialect_leak_view(const SemanticPackageManifest& manifest)
    {
        const SemanticPackageManifest foreign{.name = kForeignDialect, .version = "0.0.0"};
        const std::array<SemanticPackageManifest, 2> pair{manifest, foreign};
        return compose(pair).for_stream(kForeignDialect, kAnyChannel);
    }

    // note: one conformance property end-to-end; splitting the sequence scatters one verdict.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    CheckResult check_dialect_gate_honesty(const SemanticPackageManifest& manifest,
                                           const ComposedSemantics& composed)
    {
        // refs: ADR-22
        // assert: both views are built ONCE — the gate is a stream-scoped resolution, so the
        // probe must be one too.
        const ComposedSemantics own{composed.for_stream(manifest.name, kAnyChannel)};
        const ComposedSemantics foreign{dialect_leak_view(manifest)};

        std::string scratch;
        for (const StructuralRoleRow& row : manifest.roles)
        {
            const std::string probe{probe_for(row.prefix)};
            if (row.dialect_gate == kAnyDialect)
            {
                for (const auto& [view, label] :
                     {std::pair{std::cref(composed), std::string_view{"the UNDECLARED view"}},
                      std::pair{std::cref(foreign), kForeignDialect}})
                    if (insight::tokenization::classify(normalized_probe(probe, scratch),
                                                        view.get()) != row.role)
                        return {.name = "dialect_gate.role_any",
                                .passed = false,
                                .detail = "kAnyDialect role key \"" + std::string{row.prefix} +
                                          "\" failed to fire under " + std::string{label} +
                                          " — an ungated row must fire whatever the caller "
                                          "declared."};
            }
            else
            {
                if (insight::tokenization::classify(normalized_probe(probe, scratch), own) !=
                    row.role)
                    return {.name = "dialect_gate.role_own",
                            .passed = false,
                            .detail = "role key \"" + std::string{row.prefix} + "\" (gated to \"" +
                                      std::string{row.dialect_gate} +
                                      "\") did NOT fire on a stream declaring \"" +
                                      std::string{manifest.name} +
                                      "\" — the row is unreachable under any declaration."};
                if (insight::tokenization::classify(normalized_probe(probe, scratch), foreign) !=
                    insight::StructuralRole::None)
                    return {.name = "dialect_gate.role_leak",
                            .passed = false,
                            .detail = "role key \"" + std::string{row.prefix} + "\" (gated to \"" +
                                      std::string{row.dialect_gate} +
                                      "\") FIRED on a stream declaring \"" +
                                      std::string{kForeignDialect} + "\" — SRC-II-6 gate leak."};
            }
        }
        // refs: SRC-II-6
        // assert: the OWN leg is scored at the row's own Medium, never the kAnyChannel view, where
        // a channel-gated marker is legitimately absent.
        // note: skipping the own leg is what let the leak leg go vacuous.
        for (const IntentMarkerRow& row : manifest.markers)
        {
            if (row.dialect_gate == kAnyDialect)
                continue;
            const std::string probe{marker_probe_for(row, manifest.emits)};
            const ComposedSemantics medium{composed.for_stream(manifest.name, row.channel_gate)};
            if (insight::tokenization::recognize(normalized_probe(probe, scratch), medium).kind !=
                insight::tokenization::IntentMarkerKind::None)
            {
                if (insight::tokenization::recognize(normalized_probe(probe, scratch), foreign)
                        .kind != insight::tokenization::IntentMarkerKind::None)
                    return {.name = "dialect_gate.marker_leak",
                            .passed = false,
                            .detail = "marker key \"" + std::string{row.prefix} +
                                      "\" (gated to \"" + std::string{row.dialect_gate} +
                                      "\") FIRED on a stream declaring \"" +
                                      std::string{kForeignDialect} + "\" — SRC-II-6 gate leak."};
                continue;
            }
            return {.name = "dialect_gate.marker_own",
                    .passed = false,
                    .detail = "marker key \"" + std::string{row.prefix} +
                              "\" did NOT fire on its "
                              "OWN medium (dialect \"" +
                              std::string{manifest.name} + "\", channel \"" +
                              std::string{row.channel_gate} + "\") for the probe \"" + probe +
                              "\", which its own paired emit row rendered. The row is unreachable "
                              "under every declaration, so the leak leg below can never catch "
                              "anything — either the emit row is not the extractor's inverse, or "
                              "the row cannot match its own generated bytes."};
        }
        for (const OutcomeTokenRow& row : manifest.outcome_tokens)
        {
            if (row.dialect_gate == kAnyDialect)
                continue;
            if (!insight::map_outcome_token(row.token, own).has_value())
                return {.name = "dialect_gate.outcome_own",
                        .passed = false,
                        .detail = "outcome token \"" + std::string{row.token} + "\" (gated to \"" +
                                  std::string{row.dialect_gate} +
                                  "\") did NOT resolve on a stream declaring \"" +
                                  std::string{manifest.name} +
                                  "\" — the row is unreachable under any declaration."};
            if (insight::map_outcome_token(row.token, foreign).has_value())
                return {.name = "dialect_gate.outcome_leak",
                        .passed = false,
                        .detail = "outcome token \"" + std::string{row.token} + "\" (gated to \"" +
                                  std::string{row.dialect_gate} +
                                  "\") RESOLVED on a stream declaring \"" +
                                  std::string{kForeignDialect} + "\" — SRC-II-6 gate leak."};
        }
        // assert: an UNDECLARED stream is fail-closed on DEPTH — no concretely-gated row of any
        // kind may fire, which is the leg that catches a filter that never ran.
        for (const OutcomeTokenRow& row : manifest.outcome_tokens)
            if (row.dialect_gate != kAnyDialect &&
                insight::map_outcome_token(row.token, composed).has_value())
                return {.name = "dialect_gate.undeclared_leak",
                        .passed = false,
                        .detail = "outcome token \"" + std::string{row.token} + "\" (gated to \"" +
                                  std::string{row.dialect_gate} +
                                  "\") RESOLVED on an UNDECLARED stream — an absent declaration "
                                  "must withhold every concretely-gated row (fail-closed on "
                                  "depth), never fall open."};
        for (const IntentMarkerRow& row : manifest.markers)
            if (row.dialect_gate != kAnyDialect &&
                insight::tokenization::recognize(
                    normalized_probe(marker_probe_for(row, manifest.emits), scratch), composed)
                        .kind != insight::tokenization::IntentMarkerKind::None)
                return {.name = "dialect_gate.undeclared_leak",
                        .passed = false,
                        .detail = "marker key \"" + std::string{row.prefix} + "\" (gated to \"" +
                                  std::string{row.dialect_gate} +
                                  "\") FIRED on an UNDECLARED stream — an absent declaration must "
                                  "withhold every concretely-gated row (fail-closed on depth), "
                                  "never fall open."};
        return {.name = "dialect_gate_honesty", .passed = true, .detail = {}};
    }

    CheckResult check_ascii_safety(const SemanticPackageManifest& manifest)
    {
        const auto span_ok{[](std::span<const std::string_view> strings) noexcept
                           { return std::ranges::all_of(strings, is_ascii); }};
        for (const StructuralRoleRow& row : manifest.roles)
            if (!is_ascii(row.prefix))
                return {.name = "ascii.role",
                        .passed = false,
                        .detail = "role key contains a non-ASCII byte: \"" +
                                  std::string{row.prefix} + "\"."};
        for (const IntentMarkerRow& row : manifest.markers)
            if (!is_ascii(row.prefix) || !span_ok(row.payload_excludes))
                return {.name = "ascii.marker",
                        .passed = false,
                        .detail =
                            "marker key or payload-exclusion entry contains a non-ASCII byte: \"" +
                            std::string{row.prefix} + "\"."};
        // refs: ADR-23
        // assert: the generation projection is manifest data and identity-bearing, so it is held to
        // the same locale-safety property as the rows it is dual to.
        for (const IntentEmitRow& row : manifest.emits)
            if (!is_ascii(row.prefix))
                return {.name = "ascii.emit",
                        .passed = false,
                        .detail = "emit key contains a non-ASCII byte: \"" +
                                  std::string{row.prefix} + "\"."};
        for (const OutcomeTokenRow& row : manifest.outcome_tokens)
            if (!is_ascii(row.token))
                return {.name = "ascii.outcome_token",
                        .passed = false,
                        .detail = "outcome token contains a non-ASCII byte: \"" +
                                  std::string{row.token} + "\"."};
        for (const OutcomeMarkerRow& row : manifest.outcome_markers)
            if (!is_ascii(row.prefix))
                return {.name = "ascii.outcome_marker",
                        .passed = false,
                        .detail = "outcome-marker key contains a non-ASCII byte: \"" +
                                  std::string{row.prefix} + "\"."};
        for (const LevelLiftRow& row : manifest.level_lifts)
            if (!is_ascii(row.prefix))
                return {.name = "ascii.level_lift",
                        .passed = false,
                        .detail = "level-lift key contains a non-ASCII byte: \"" +
                                  std::string{row.prefix} + "\"."};
        for (const LocationRow& row : manifest.locations)
            if (!span_ok(row.infixes) || !span_ok(row.extensions) || !span_ok(row.prefixes) ||
                !span_ok(row.suffixes))
                return {.name = "ascii.location",
                        .passed = false,
                        .detail =
                            "a location row carries a non-ASCII vocabulary token (locale-unsafe)."};
        return {.name = "ascii_safety", .passed = true, .detail = {}};
    }

    // note: one conformance property end-to-end; the guarded sequence IS the check.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    CheckResult check_grammar_wellformed(const SemanticPackageManifest& manifest)
    {
        for (const StructuralRoleRow& row : manifest.roles)
            if (row.prefix.empty())
                return {.name = "grammar.empty_role",
                        .passed = false,
                        .detail = "a structural-role row has an empty prefix."};
        for (const IntentMarkerRow& row : manifest.markers)
            if (row.prefix.empty())
                return {.name = "grammar.empty_marker",
                        .passed = false,
                        .detail = "an intent-marker row has an empty prefix."};
        for (const IntentEmitRow& row : manifest.emits)
            if (row.prefix.empty())
                return {.name = "grammar.empty_emit",
                        .passed = false,
                        .detail = "an intent-emit row has an empty prefix."};
        // refs: ADR-23, SRC-SID-2
        // assert: a reader without a writer is a MANIFEST property, so the runtime kit states it
        // — the concept cannot see an `emits` wired to another array.
        for (const IntentMarkerRow& row : manifest.markers)
            if (paired_writer_row(row, manifest.emits) == nullptr)
                return {.name = "grammar.unpaired_marker",
                        .passed = false,
                        .detail = "marker key \"" + std::string{row.prefix} +
                                  "\" has no paired emit row in the MANIFEST (same prefix, kind, "
                                  "dialect_gate and channel_gate). A reader without a writer — the "
                                  "manifest's `emits` may be wired to a different array than the "
                                  "dialect type's `emit_markers`."};
        for (const LevelLiftRow& row : manifest.level_lifts)
            if (row.prefix.empty())
                return {.name = "grammar.empty_level_lift",
                        .passed = false,
                        .detail = "a level-lift row has an empty prefix."};
        for (const OutcomeTokenRow& row : manifest.outcome_tokens)
            if (row.token.empty())
                return {.name = "grammar.empty_outcome_token",
                        .passed = false,
                        .detail = "an outcome-token row has an empty token."};
        for (const OutcomeMarkerRow& row : manifest.outcome_markers)
            if (row.prefix.empty())
                return {.name = "grammar.empty_outcome_marker",
                        .passed = false,
                        .detail = "an outcome-marker row has an empty prefix."};
        for (const IntentMarkerRow& row : manifest.markers)
            for (const std::string_view exclude : row.payload_excludes)
                if (exclude.empty())
                    return {.name = "grammar.empty_payload_exclude",
                            .passed = false,
                            .detail = "marker key \"" + std::string{row.prefix} +
                                      "\" carries an empty payload-exclusion entry (would exclude "
                                      "every payload)."};

        const std::array<SemanticPackageManifest, 1> one{manifest};
        if (const ConflictInfo conflict{find_conflict(one)}; conflict.has_conflict)
            return {.name = "grammar.self_conflict",
                    .passed = false,
                    .detail = "the package carries an exact-duplicate " +
                              std::string{conflict.kind} + " key \"" + std::string{conflict.key} +
                              "\" within its own rows (would fatal at compose)."};

        for (const LocationRow& row : manifest.locations)
        {
            const auto fail{[&](std::string_view why)
                            {
                                return CheckResult{.name = "grammar.location_params",
                                                   .passed = false,
                                                   .detail = std::string{why}};
                            }};
            switch (row.kind)
            {
            case LocationMatchKind::TestSpecExtension:
                if (row.infixes.empty() || row.extensions.empty())
                    return fail("TestSpecExtension row needs non-empty infixes AND extensions.");
                break;
            case LocationMatchKind::PrefixAndExtension:
                if (row.extensions.empty() || (row.prefixes.empty() && row.suffixes.empty()))
                    return fail("PrefixAndExtension row needs extensions AND at least one of "
                                "prefixes/suffixes.");
                break;
            case LocationMatchKind::SuffixSet:
                if (row.suffixes.empty())
                    return fail("SuffixSet row needs a non-empty suffixes set.");
                break;
            }
        }
        return {.name = "grammar_wellformed", .passed = true, .detail = {}};
    }

    CheckResult check_code_tier(const SemanticPackageManifest& manifest)
    {
        if (manifest.strategy != nullptr)
        {
            const std::unique_ptr<insight::tokenization::IFormatStrategy> strategy{
                manifest.strategy()};
            if (strategy == nullptr)
                return {.name = "code_tier.strategy_null",
                        .passed = false,
                        .detail = "the strategy factory returned nullptr."};
            if (strategy->format() == insight::LogFormat::Unknown)
                return {.name = "code_tier.strategy_format",
                        .passed = false,
                        .detail = "the dialect strategy reports LogFormat::Unknown — a dialect "
                                  "must own a concrete format."};
            constexpr std::string_view kProbe{"2026-01-01T00:00:00.0000000Z probe line"};
            const double first{strategy->confidence(kProbe)};
            const double second{strategy->confidence(kProbe)};
            // assert: the negation of an in-range test — a NaN passes neither `< 0.0` nor `>
            // 1.0`, so the DeMorgan form would misreport it as non-deterministic.
            const bool confidence_in_range{first >= 0.0 && first <= 1.0};
            if (!confidence_in_range || first != second)
                return {.name = "code_tier.confidence",
                        .passed = false,
                        .detail = "confidence() is out of [0,1] or non-deterministic (" +
                                  std::to_string(first) + " then " + std::to_string(second) + ")."};
        }
        if (manifest.echoed_source != nullptr)
        {
            constexpr std::string_view kProbe{"plain unwrapped line"};
            if (manifest.echoed_source(kProbe) != manifest.echoed_source(kProbe))
                return {.name = "code_tier.echoed_source",
                        .passed = false,
                        .detail = "the echoed-source hook is non-deterministic on a fixed line."};
        }
        return {.name = "code_tier", .passed = true, .detail = {}};
    }

    // refs: ADR-27.D4
    // post: for every outcome-marker row, the line `render_outcome` materializes is recognized back
    // by the shipped scan under the package's OWN declaration.
    // invariant: self-adapting over both marker shapes — a RemainderToken row round-trips once
    // per declared token, a PrefixIsVerdict row round-trips its prefix alone.
    // note: trivially green for a package with tokens but no outcome marker.
    CheckResult check_outcome_round_trip(const SemanticPackageManifest& manifest,
                                         const ComposedSemantics& composed)
    {
        const ComposedSemantics own{composed.for_stream(manifest.name, kAnyChannel)};
        for (const OutcomeMarkerRow& row : manifest.outcome_markers)
        {
            const auto scan_one{[&own](const std::string& line)
                                {
                                    const std::array<std::string, 1> lines{line};
                                    return insight::scan_run_outcome(lines, own);
                                }};
            if (row.shape == OutcomeMarkerShape::PrefixIsVerdict)
            {
                const std::string line{render_outcome(row, {})};
                const insight::RunOutcomeScan scan{scan_one(line)};
                if (!scan.marker_present || !scan.verdict.has_value() ||
                    *scan.verdict != row.outcome)
                    return {.name = "outcome.round_trip",
                            .passed = false,
                            .detail = "PrefixIsVerdict row \"" + std::string{row.prefix} +
                                      "\": scan_run_outcome(render_outcome(row)) did not recover "
                                      "the row's own verdict (marker_present=" +
                                      (scan.marker_present ? "true" : "false") +
                                      ") — the two projections of the run-verdict line disagree."};
                continue;
            }
            for (const OutcomeTokenRow& token : manifest.outcome_tokens)
            {
                const std::string line{render_outcome(row, token.token)};
                const insight::RunOutcomeScan scan{scan_one(line)};
                const std::optional<insight::RunOutcome> mapped{
                    scan.marker_present && !scan.token.empty()
                        ? insight::map_outcome_token(scan.token, own)
                        : std::nullopt};
                if (!mapped.has_value() || *mapped != token.outcome)
                    return {.name = "outcome.round_trip",
                            .passed = false,
                            .detail = "RemainderToken row \"" + std::string{row.prefix} +
                                      "\" + token \"" + std::string{token.token} +
                                      "\": scan_run_outcome(render_outcome(row, token)) over \"" +
                                      line +
                                      "\" did not map back to the token's own verdict "
                                      "(marker_present=" +
                                      (scan.marker_present ? "true" : "false") +
                                      ", scanned token \"" + scan.token + "\")."};
            }
        }
        return {.name = "outcome_round_trip", .passed = true, .detail = {}};
    }

} // namespace

Report run(const SemanticPackageManifest& manifest)
{
    const std::array<SemanticPackageManifest, 1> one{manifest};
    const ComposedSemantics composed{compose(one)};

    Report report;
    report.checks.push_back(check_determinism(manifest));
    report.checks.push_back(check_dialect_gate_honesty(manifest, composed));
    report.checks.push_back(check_ascii_safety(manifest));
    report.checks.push_back(check_grammar_wellformed(manifest));
    report.checks.push_back(check_code_tier(manifest));
    report.checks.push_back(check_outcome_round_trip(manifest, composed));
    return report;
}

namespace
{

    /***********************************************************************************************
    D-LSRC-7 — a switch over a canon enum carries NO `default:` label
    `core/CMakeLists.txt` sets `-Werror=switch` (GCC/Clang) and `/we4062` (MSVC) PRIVATE on
    `insight_canon` and `insight_canon_tests`, so an unhandled enumerator is a COMPILE ERROR at
    the switch instead of a "?" in a diagnostic nobody reads twice. A `default:` label disarms
    that arm completely: it makes every future enumerator handled, silently and wrongly, and the
    cost is paid by the reader of a failure message rather than by the author of the enum.
    Measured 2026-08-26 on `extract_name`. The rule reaches further than it looks, in both
    directions. It is why `order_name` is a switch over a TWO-valued enum rather than the
    equivalent ternary: the ternary sat outside the option's reach, and a third `ChildOrder`
    would have printed "Ordered" for the new value. And it is why the unreachable trailing
    return under each switch exists at all — the function is non-void, and the option, not the
    return, is what makes the switch total. Obeyed by every *_name function in this file and by
    `dual()` in `core/api/canon.spi.cppm`. ONE LIMIT, stated so nobody reads the rule wider: the
    option is PRIVATE, so it binds THIS build. A consumer compiling the installed module
    interface gets no such enforcement, and the totality it relies on is ours to keep.
    ***********************************************************************************************/
    // note: no canon `to_string` exists for these two enums; the diagnostic needs both names.
    [[nodiscard]] std::string_view kind_name(insight::tokenization::IntentMarkerKind kind) noexcept
    {
        using insight::tokenization::IntentMarkerKind;
        switch (kind)
        {
        case IntentMarkerKind::None:
            return "None";
        case IntentMarkerKind::Job:
            return "Job";
        case IntentMarkerKind::Step:
            return "Step";
        }
        return "?";
    }

    [[nodiscard]] std::string_view order_name(insight::tokenization::ChildOrder order) noexcept
    {
        using insight::tokenization::ChildOrder;
        switch (order)
        {
        case ChildOrder::Unordered:
            return "Unordered";
        case ChildOrder::Ordered:
            return "Ordered";
        }
        return "?";
    }

} // namespace

Report round_trip_report(const SemanticPackageManifest& manifest, const ComposedSemantics& composed)
{
    const std::span<const IntentMarkerRow> markers{manifest.markers};
    const std::span<const IntentEmitRow> emits{manifest.emits};

    Report report;
    for (const IntentMarkerRow& reader : markers)
    {
        const IntentEmitRow* writer{paired_writer_row(reader, emits)};
        if (writer == nullptr)
        {
            // assert: the concept should have refused this at compile time; the kit asserts it too
            // so an author who bypassed the concept still gets a legible failure.
            report.checks.push_back(
                {.name = "round_trip.unpaired",
                 .passed = false,
                 .detail =
                     "recognition marker \"" + std::string{reader.prefix} +
                     "\" has NO paired generation row — a reader without a writer (SID / G2). "
                     "DialectIntent<Dialect> should have refused to compile."});
            continue;
        }

        // refs: ADR-22
        // invariant: the Medium is dialect × channel, so the round trip closes PER MEDIUM — both
        // coordinates are read off the ROW, which keeps the kit self-adapting.
        // note: ONE composition for every row would ask the phantom, not the closure.
        const std::string line{render_row(*writer, kProbePayload)};
        std::string scratch;
        const ComposedSemantics medium_view{
            composed.for_stream(writer->dialect_gate, writer->channel_gate)};
        const insight::tokenization::IntentMarker got{
            insight::tokenization::recognize(normalized_probe(line, scratch), medium_view)};

        if (got.kind == reader.kind && got.child_order == reader.child_order &&
            got.name == kProbePayload)
        {
            report.checks.push_back({.name = "round_trip", .passed = true, .detail = {}});
            continue;
        }

        report.checks.push_back(
            {.name = "round_trip.closure",
             .passed = false,
             .detail = "recognize(render_row(\"" + std::string{reader.prefix} + "\", \"" +
                       std::string{kProbePayload} + "\")) over line \"" + line +
                       "\" did NOT recover the declared intent. expected {kind=" +
                       std::string{kind_name(reader.kind)} +
                       ", child_order=" + std::string{order_name(reader.child_order)} +
                       ", payload=\"" + std::string{kProbePayload} +
                       "\"} got {kind=" + std::string{kind_name(got.kind)} +
                       ", child_order=" + std::string{order_name(got.child_order)} +
                       ", payload=\"" + std::string{got.name} + "\"}."});
    }
    return report;
}

namespace
{

    // invariant: the COUNTS are always exact and only the enumeration is capped, so the cap can
    // never make a difference look smaller than it is.
    constexpr std::size_t kMaxReportedRowDiffs{8};
    constexpr std::size_t kMaxReportedUnpairedRows{4};

    // refs: LSRC-7
    // invariant: every enum prints as its NAME, because the identity hash already decides the
    // boolean and this report earns its keep only by LOCATING.
    // invariant: produced strings are ASCII only — the report crosses into an external author's
    // framework, and the surrounding kit asserts ASCII of every row key.
    [[nodiscard]] std::string_view extract_name(PayloadExtract extract) noexcept
    {
        switch (extract)
        {
        case PayloadExtract::None:
            return "None";
        case PayloadExtract::RemainderAfterPrefix:
            return "RemainderAfterPrefix";
        case PayloadExtract::RemainderToClosingParen:
            return "RemainderToClosingParen";
        case PayloadExtract::NumericFieldThenRemainder:
            return "NumericFieldThenRemainder";
        }
        return "?";
    }

    [[nodiscard]] std::string_view emit_name(PayloadEmit emit) noexcept
    {
        switch (emit)
        {
        case PayloadEmit::None:
            return "None";
        case PayloadEmit::PayloadAfterPrefix:
            return "PayloadAfterPrefix";
        case PayloadEmit::PayloadThenClosingParen:
            return "PayloadThenClosingParen";
        case PayloadEmit::PlaceholderNumericFieldThenPayload:
            return "PlaceholderNumericFieldThenPayload";
        }
        return "?";
    }

    [[nodiscard]] std::string_view location_kind_name(LocationMatchKind kind) noexcept
    {
        switch (kind)
        {
        case LocationMatchKind::TestSpecExtension:
            return "TestSpecExtension";
        case LocationMatchKind::PrefixAndExtension:
            return "PrefixAndExtension";
        case LocationMatchKind::SuffixSet:
            return "SuffixSet";
        }
        return "?";
    }

    [[nodiscard]] std::string_view outcome_shape_name(OutcomeMarkerShape shape) noexcept
    {
        switch (shape)
        {
        case OutcomeMarkerShape::RemainderToken:
            return "RemainderToken";
        case OutcomeMarkerShape::PrefixIsVerdict:
            return "PrefixIsVerdict";
        }
        return "?";
    }

    [[nodiscard]] std::string_view value_class_name(ValueClass cls) noexcept
    {
        switch (cls)
        {
        case ValueClass::None:
            return "None";
        }
        return "?";
    }

    [[nodiscard]] std::string quoted(std::string_view value)
    {
        return '"' + std::string{value} + '"';
    }

    [[nodiscard]] std::string render_value(std::string_view value)
    {
        return quoted(value);
    }

    [[nodiscard]] std::string render_value(std::span<const std::string_view> values)
    {
        std::string out{"["};
        for (std::size_t idx{0}; idx < values.size(); ++idx)
        {
            if (idx != 0)
                out += ", ";
            out += quoted(values[idx]);
        }
        out += ']';
        return out;
    }

    [[nodiscard]] std::string render_value(std::int64_t value)
    {
        return std::to_string(value);
    }

    [[nodiscard]] std::string render_value(insight::StructuralRole role)
    {
        return std::string{insight::to_string(role)};
    }

    [[nodiscard]] std::string render_value(insight::LogLevel level)
    {
        return std::string{insight::to_string(level)};
    }

    [[nodiscard]] std::string render_value(insight::RunOutcome outcome)
    {
        return std::string{insight::to_string(outcome)};
    }

    [[nodiscard]] std::string render_value(insight::tokenization::IntentMarkerKind kind)
    {
        return std::string{kind_name(kind)};
    }

    [[nodiscard]] std::string render_value(insight::tokenization::ChildOrder order)
    {
        return std::string{order_name(order)};
    }

    [[nodiscard]] std::string render_value(PayloadExtract extract)
    {
        return std::string{extract_name(extract)};
    }

    [[nodiscard]] std::string render_value(PayloadEmit emit)
    {
        return std::string{emit_name(emit)};
    }

    [[nodiscard]] std::string render_value(LocationMatchKind kind)
    {
        return std::string{location_kind_name(kind)};
    }

    [[nodiscard]] std::string render_value(OutcomeMarkerShape shape)
    {
        return std::string{outcome_shape_name(shape)};
    }

    [[nodiscard]] std::string render_value(ValueClass cls)
    {
        return std::string{value_class_name(cls)};
    }

    // note: `std::span` has no operator==, so a nine-span manifest has no defaulted equality.
    template <typename Value>
        requires requires(const Value& lhs, const Value& rhs) {
            { lhs == rhs } -> std::convertible_to<bool>;
        }
    [[nodiscard]] bool equal_values(const Value& lhs, const Value& rhs) noexcept
    {
        return lhs == rhs;
    }

    [[nodiscard]] bool equal_values(std::span<const std::string_view> lhs,
                                    std::span<const std::string_view> rhs) noexcept
    {
        return std::ranges::equal(lhs, rhs);
    }

    // invariant: empty text IS the equality verdict — a row pair is equal exactly when no field
    // was recorded.
    class FieldDiff
    {
      public:
        template <typename Value>
        void field(std::string_view label, const Value& lhs, const Value& rhs)
        {
            if (equal_values(lhs, rhs))
                return;
            if (!text_.empty())
                text_ += ", ";
            text_ += label;
            text_ += ": lhs=";
            text_ += render_value(lhs);
            text_ += " rhs=";
            text_ += render_value(rhs);
        }

        [[nodiscard]] const std::string& text() const noexcept
        {
            return text_;
        }

      private:
        std::string text_;
    };

    /***********************************************************************************************
    D-LSRC-6 — the structured binding IS the coverage instrument, never a style choice
    Every row comparator below, and `manifest_equivalence_report` itself, destructures its subject
    with a structured binding and never with member access. Do not "simplify" that away. A
    comparator whose apparent subject is "the row" and whose real subject is "the fields someone
    remembered" is a silent, green, WRONG answer, and the failure is invisible precisely because
    a partial comparator still reports: the missing member's check simply never appears in a list
    nobody counts. A structured binding must name EXACTLY as many members as the type has, so
    adding a field to any row struct — or to `SemanticPackageManifest`, bound the same way —
    is a COMPILE ERROR at that line, in this file, on the next build. It costs one line per row
    kind and it is the only thing standing between this comparator and the defect class it lives
    inside.
    ***********************************************************************************************/
    [[nodiscard]] std::string row_differences(const StructuralRoleRow& lhs,
                                              const StructuralRoleRow& rhs)
    {
        const auto& [lhs_prefix, lhs_role, lhs_dialect] = lhs;
        const auto& [rhs_prefix, rhs_role, rhs_dialect] = rhs;
        FieldDiff diff;
        diff.field("prefix", lhs_prefix, rhs_prefix);
        diff.field("role", lhs_role, rhs_role);
        diff.field("dialect_gate", lhs_dialect, rhs_dialect);
        return diff.text();
    }

    [[nodiscard]] std::string row_differences(const IntentMarkerRow& lhs,
                                              const IntentMarkerRow& rhs)
    {
        const auto& [lhs_prefix, lhs_kind, lhs_order, lhs_dialect, lhs_extract, lhs_excludes,
                     lhs_channel] = lhs;
        const auto& [rhs_prefix, rhs_kind, rhs_order, rhs_dialect, rhs_extract, rhs_excludes,
                     rhs_channel] = rhs;
        FieldDiff diff;
        diff.field("prefix", lhs_prefix, rhs_prefix);
        diff.field("kind", lhs_kind, rhs_kind);
        diff.field("child_order", lhs_order, rhs_order);
        diff.field("dialect_gate", lhs_dialect, rhs_dialect);
        diff.field("extract", lhs_extract, rhs_extract);
        diff.field("payload_excludes", lhs_excludes, rhs_excludes);
        diff.field("channel_gate", lhs_channel, rhs_channel);
        return diff.text();
    }

    [[nodiscard]] std::string row_differences(const IntentEmitRow& lhs, const IntentEmitRow& rhs)
    {
        const auto& [lhs_prefix, lhs_kind, lhs_order, lhs_dialect, lhs_emit, lhs_channel] = lhs;
        const auto& [rhs_prefix, rhs_kind, rhs_order, rhs_dialect, rhs_emit, rhs_channel] = rhs;
        FieldDiff diff;
        diff.field("prefix", lhs_prefix, rhs_prefix);
        diff.field("kind", lhs_kind, rhs_kind);
        diff.field("child_order", lhs_order, rhs_order);
        diff.field("dialect_gate", lhs_dialect, rhs_dialect);
        diff.field("emit", lhs_emit, rhs_emit);
        diff.field("channel_gate", lhs_channel, rhs_channel);
        return diff.text();
    }

    [[nodiscard]] std::string row_differences(const LevelLiftRow& lhs, const LevelLiftRow& rhs)
    {
        const auto& [lhs_prefix, lhs_level, lhs_dialect] = lhs;
        const auto& [rhs_prefix, rhs_level, rhs_dialect] = rhs;
        FieldDiff diff;
        diff.field("prefix", lhs_prefix, rhs_prefix);
        diff.field("level", lhs_level, rhs_level);
        diff.field("dialect_gate", lhs_dialect, rhs_dialect);
        return diff.text();
    }

    [[nodiscard]] std::string row_differences(const LocationRow& lhs, const LocationRow& rhs)
    {
        const auto& [lhs_kind, lhs_infixes, lhs_extensions, lhs_prefixes, lhs_suffixes] = lhs;
        const auto& [rhs_kind, rhs_infixes, rhs_extensions, rhs_prefixes, rhs_suffixes] = rhs;
        FieldDiff diff;
        diff.field("kind", lhs_kind, rhs_kind);
        diff.field("infixes", lhs_infixes, rhs_infixes);
        diff.field("extensions", lhs_extensions, rhs_extensions);
        diff.field("prefixes", lhs_prefixes, rhs_prefixes);
        diff.field("suffixes", lhs_suffixes, rhs_suffixes);
        return diff.text();
    }

    [[nodiscard]] std::string row_differences(const ValueClassRow& lhs, const ValueClassRow& rhs)
    {
        const auto& [lhs_key, lhs_cls, lhs_schedule, lhs_scale] = lhs;
        const auto& [rhs_key, rhs_cls, rhs_schedule, rhs_scale] = rhs;
        FieldDiff diff;
        diff.field("key", lhs_key, rhs_key);
        diff.field("cls", lhs_cls, rhs_cls);
        diff.field("schedule_id", lhs_schedule, rhs_schedule);
        diff.field("scale", lhs_scale, rhs_scale);
        return diff.text();
    }

    [[nodiscard]] std::string row_differences(const OutcomeTokenRow& lhs,
                                              const OutcomeTokenRow& rhs)
    {
        const auto& [lhs_token, lhs_outcome, lhs_dialect] = lhs;
        const auto& [rhs_token, rhs_outcome, rhs_dialect] = rhs;
        FieldDiff diff;
        diff.field("token", lhs_token, rhs_token);
        diff.field("outcome", lhs_outcome, rhs_outcome);
        diff.field("dialect_gate", lhs_dialect, rhs_dialect);
        return diff.text();
    }

    [[nodiscard]] std::string row_differences(const OutcomeMarkerRow& lhs,
                                              const OutcomeMarkerRow& rhs)
    {
        const auto& [lhs_prefix, lhs_dialect, lhs_shape, lhs_outcome] = lhs;
        const auto& [rhs_prefix, rhs_dialect, rhs_shape, rhs_outcome] = rhs;
        FieldDiff diff;
        diff.field("prefix", lhs_prefix, rhs_prefix);
        diff.field("dialect_gate", lhs_dialect, rhs_dialect);
        diff.field("shape", lhs_shape, rhs_shape);
        // assert: `outcome` is INERT on a RemainderToken row and compared anyway — the manifest
        // is a fixed field layout, not a per-shape union.
        diff.field("outcome", lhs_outcome, rhs_outcome);
        return diff.text();
    }

    // invariant: a LocationRow has no key of its own — it is an algorithm plus its vocabulary —
    // so it names its algorithm, the only field a reader can act on alone.
    [[nodiscard]] std::string_view row_key(const StructuralRoleRow& row) noexcept
    {
        return row.prefix;
    }
    [[nodiscard]] std::string_view row_key(const IntentMarkerRow& row) noexcept
    {
        return row.prefix;
    }
    [[nodiscard]] std::string_view row_key(const IntentEmitRow& row) noexcept
    {
        return row.prefix;
    }
    [[nodiscard]] std::string_view row_key(const LevelLiftRow& row) noexcept
    {
        return row.prefix;
    }
    [[nodiscard]] std::string_view row_key(const LocationRow& row) noexcept
    {
        return location_kind_name(row.kind);
    }
    [[nodiscard]] std::string_view row_key(const ValueClassRow& row) noexcept
    {
        return row.key;
    }
    [[nodiscard]] std::string_view row_key(const OutcomeTokenRow& row) noexcept
    {
        return row.token;
    }
    [[nodiscard]] std::string_view row_key(const OutcomeMarkerRow& row) noexcept
    {
        return row.prefix;
    }

    template <typename Row>
    [[nodiscard]] std::string unpaired_note(std::string_view side, std::span<const Row> longer,
                                            std::size_t paired)
    {
        const std::size_t surplus{longer.size() - paired};
        const std::size_t shown{std::min(surplus, kMaxReportedUnpairedRows)};
        std::string out{" Unpaired "};
        out += side;
        out += " rows:";
        for (std::size_t idx{paired}; idx < paired + shown; ++idx)
        {
            out += idx == paired ? " [" : ", [";
            out += std::to_string(idx);
            out += "] ";
            out += quoted(row_key(longer[idx]));
        }
        if (surplus > shown)
        {
            out += ", and ";
            out += std::to_string(surplus - shown);
            out += " more";
        }
        out += '.';
        return out;
    }

    // invariant: rows pair BY INDEX, so a length mismatch reports counts plus the unpaired keys AND
    // still compares the paired prefix.
    template <typename Row>
    [[nodiscard]] CheckResult compare_rows(std::string_view check_name, std::string_view member,
                                           std::span<const Row> lhs, std::span<const Row> rhs)
    {
        const std::size_t paired{std::min(lhs.size(), rhs.size())};
        std::size_t differing{0};
        std::string rows_detail;
        for (std::size_t idx{0}; idx < paired; ++idx)
        {
            const std::string fields{row_differences(lhs[idx], rhs[idx])};
            if (fields.empty())
                continue;
            ++differing;
            if (differing > kMaxReportedRowDiffs)
                continue;
            rows_detail += ' ';
            rows_detail += member;
            rows_detail += '[';
            rows_detail += std::to_string(idx);
            rows_detail += "]: ";
            rows_detail += fields;
        }

        if (lhs.size() == rhs.size() && differing == 0)
            return {.name = check_name, .passed = true, .detail = {}};

        std::string detail{member};
        detail += ": ";
        if (lhs.size() != rhs.size())
        {
            detail += "LHS declares ";
            detail += std::to_string(lhs.size());
            detail += " rows, RHS declares ";
            detail += std::to_string(rhs.size());
            detail += " (rows pair BY INDEX; the first ";
            detail += std::to_string(paired);
            detail += " are compared, the surplus is unpaired).";
            detail += lhs.size() > rhs.size() ? unpaired_note("LHS", lhs, paired)
                                              : unpaired_note("RHS", rhs, paired);
            detail += ' ';
        }
        detail += std::to_string(differing);
        detail += " of the ";
        detail += std::to_string(paired);
        detail += " paired rows differ field-for-field.";
        if (differing > kMaxReportedRowDiffs)
        {
            detail += " First ";
            detail += std::to_string(kMaxReportedRowDiffs);
            detail += " shown.";
        }
        detail += rows_detail;
        return {.name = check_name, .passed = false, .detail = std::move(detail)};
    }

    [[nodiscard]] CheckResult compare_scalar(std::string_view check_name, std::string_view member,
                                             std::string_view lhs, std::string_view rhs)
    {
        if (lhs == rhs)
            return {.name = check_name, .passed = true, .detail = {}};
        return {.name = check_name,
                .passed = false,
                .detail = std::string{member} + ": lhs=" + quoted(lhs) + " rhs=" + quoted(rhs)};
    }

    // invariant: a short closed vocabulary prints in FULL on both sides — at this size the whole
    // value is the locator.
    [[nodiscard]] CheckResult compare_vocabulary(std::string_view check_name,
                                                 std::string_view member,
                                                 std::span<const std::string_view> lhs,
                                                 std::span<const std::string_view> rhs)
    {
        if (std::ranges::equal(lhs, rhs))
            return {.name = check_name, .passed = true, .detail = {}};
        return {.name = check_name,
                .passed = false,
                .detail = std::string{member} + ": lhs=" + render_value(lhs) +
                          " rhs=" + render_value(rhs)};
    }

    // refs: DN-17.D21
    // invariant: the check NAME carries `presence_only`, so the scope travels with the report into
    // a framework that shows nothing but names.
    // note: whether two present hooks AGREE is a separate obligation, not covered.
    [[nodiscard]] CheckResult compare_presence(std::string_view check_name, std::string_view member,
                                               bool lhs, bool rhs)
    {
        if (lhs == rhs)
            return {.name = check_name, .passed = true, .detail = {}};
        return {.name = check_name,
                .passed = false,
                .detail = std::string{member} + ": one side declares the hook and the other does " +
                          "not (lhs=" + (lhs ? "present" : "absent") +
                          ", rhs=" + (rhs ? "present" : "absent") +
                          "). This check compares PRESENCE only; whether two present hooks agree " +
                          "on their verdicts is a separate obligation this kit does not cover."};
    }

} // namespace

Report manifest_equivalence_report(const SemanticPackageManifest& lhs,
                                   const SemanticPackageManifest& rhs)
{
    // refs: LSRC-6
    // assert: fourteen members bound, so a fifteenth is a COMPILE ERROR here and forces the edit
    // — it does not force the matching check to be pushed.
    const auto& [lhs_name, lhs_version, lhs_roles, lhs_markers, lhs_emits, lhs_level_lifts,
                 lhs_locations, lhs_value_classes, lhs_outcome_tokens, lhs_outcome_markers,
                 lhs_channels, lhs_dialect_revisions, lhs_strategy, lhs_echoed_source] = lhs;
    const auto& [rhs_name, rhs_version, rhs_roles, rhs_markers, rhs_emits, rhs_level_lifts,
                 rhs_locations, rhs_value_classes, rhs_outcome_tokens, rhs_outcome_markers,
                 rhs_channels, rhs_dialect_revisions, rhs_strategy, rhs_echoed_source] = rhs;

    Report report;
    report.checks.push_back(compare_scalar("equivalence.name", "name", lhs_name, rhs_name));
    report.checks.push_back(
        compare_scalar("equivalence.version", "version", lhs_version, rhs_version));
    report.checks.push_back(compare_rows("equivalence.roles", "roles", lhs_roles, rhs_roles));
    report.checks.push_back(
        compare_rows("equivalence.markers", "markers", lhs_markers, rhs_markers));
    report.checks.push_back(compare_rows("equivalence.emits", "emits", lhs_emits, rhs_emits));
    report.checks.push_back(
        compare_rows("equivalence.level_lifts", "level_lifts", lhs_level_lifts, rhs_level_lifts));
    report.checks.push_back(
        compare_rows("equivalence.locations", "locations", lhs_locations, rhs_locations));
    report.checks.push_back(compare_rows("equivalence.value_classes", "value_classes",
                                         lhs_value_classes, rhs_value_classes));
    report.checks.push_back(compare_rows("equivalence.outcome_tokens", "outcome_tokens",
                                         lhs_outcome_tokens, rhs_outcome_tokens));
    report.checks.push_back(compare_rows("equivalence.outcome_markers", "outcome_markers",
                                         lhs_outcome_markers, rhs_outcome_markers));
    report.checks.push_back(
        compare_vocabulary("equivalence.channels", "channels", lhs_channels, rhs_channels));
    report.checks.push_back(compare_vocabulary("equivalence.dialect_revisions", "dialect_revisions",
                                               lhs_dialect_revisions, rhs_dialect_revisions));
    report.checks.push_back(compare_presence("equivalence.strategy_presence_only", "strategy",
                                             lhs_strategy != nullptr, rhs_strategy != nullptr));
    report.checks.push_back(compare_presence("equivalence.echoed_source_presence_only",
                                             "echoed_source", lhs_echoed_source != nullptr,
                                             rhs_echoed_source != nullptr));
    return report;
}

} // namespace insight::semantic::conformance
