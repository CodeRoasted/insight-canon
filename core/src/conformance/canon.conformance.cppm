// insight.canon.conformance — the permanent, package-agnostic CONFORMANCE KIT (ADR 0024 §2.3,
// SP-2). The canon-shipped harness EVERY semantic package (ours or an external author's) must pass.
// A package's test target instantiates it in one line:
//
//     import insight.canon.conformance;
//     import insight.semantic.github;
//     TEST(Conformance, GithubPackagePassesTheKit) {
//         const auto
//         report{insight::semantic::conformance::run(insight::semantic::github::kManifest)}; for
//         (const auto& check : report.checks)
//             EXPECT_TRUE(check.passed) << "[" << check.name << "] " << check.detail;
//     }
//
// It is the OPEN-SOURCE HONESTY MECHANISM (SP-2): the identical kit ships installed so an external
// package author runs the same gate CodeRoast's own packages are held to. Verbose-on-failure
// matters doubly here — a failing check must diagnose itself for someone who has never read canon's
// internals.
//
// PURE by design: imports only the facade (compose/ComposedSemantics + the recognition walkers) +
// spi (the row grammar + manifest). NO gtest dependency (a package's ~10-line test file adapts the
// report to its own framework), and — deliberately — NO global `operator new` override (that must
// never ship in the canon library). The DYNAMIC heap/float-freedom guard is homed where a `new`
// override is legitimate (the canon core test binary, tests/compose/test_semantic_walkers.cpp):
// allocation-freedom is a canon-ALGORITHM property, proven once over the walkers, not re-proven per
// data-only package.
//
// The probe corpus is DERIVED FROM THE MANIFEST, so the kit is self-adapting — it works for any
// package's vocabulary with zero per-package configuration. An intent-marker probe is its own row's
// PAIRED EMIT ROW, materialized (`marker_probe_for`); a structural-role probe, which has no writer
// dual, is still the key plus a payload token.
//
// RED-CAPABILITY, OBSERVED 2026-07-29 rather than asserted. Mutation: build the marker probe as
// `prefix + " probe"` — what this kit did before grammar-5 — and run the Jenkins package suite.
// Reported: `[dialect_gate.marker_own] marker key "[Pipeline] { (" did NOT fire on its OWN medium …
// for the probe "[Pipeline] { ( probe"`, 4/5 checks passed. That is the defect the current shape
// repairs: the naive probe is valid only for a RemainderAfterPrefix row, so Jenkins's STAGE row
// could not fire ANYWHERE, and `dialect_gate.marker_leak` was reporting green about a row it could
// never have caught. Reverted; the suite returns to 21/21.
module;

export module insight.canon.conformance;
import insight.canon.internal; // std
import insight.canon; // compose / ComposedSemantics / classify / recognize / recognize_location + enums
import insight.canon.spi; // SemanticPackageManifest + the grammar rows + kAnyDialect + find_conflict

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
        return std::ranges::all_of(checks,
                                   [](const CheckResult& check) noexcept { return check.passed; });
    }

    // "K/N checks passed" + the names of any failures — a one-line summary for the top-level
    // assertion.
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

// Run the full conformance kit over one package manifest. Deterministic, single-threaded, seedless
// — the recognizers are pure byte functions, so every check is a pure function of the manifest
// data.
[[nodiscard]] Report run(const SemanticPackageManifest& manifest);

// The studies/008 G2 round-trip closure kit — the RUNTIME (value) half of the C2 bidirectionality
// obligation the DialectIntent concept enforces at COMPILE time (shared_intent_declaration §3.2/§6,
// G2). For every recognition marker, materialize its PAIRED generation row (render_row) with a
// probe payload and assert canon recognizes the declared (kind, child_order, payload) back —
// recognize(render_row(W))==R. Pure, deterministic, seedless, self-adapting over ANY dialect's row
// spans (zero per-package config, the same honesty-kit spirit as run()). Target: 100% — a miss is a
// declaration-expressivity bug, never a knob (studies/008 §5 G2).
//
// Reads BOTH projections off the MANIFEST (ADR 0044 §7 — the G4 identity wiring this signature was
// waiting on: `emits` is now a manifest member, so the rows the kit round-trips are the same rows
// `semantic_identity` hashes). It formerly took the two spans separately, from the dialect TYPE,
// because the manifest had no emit member; that split meant the kit could have closed over one
// array while the digest covered another — precisely the two-writers-one-identity divergence SID-2
// forbids. Kept a SEPARATE entry point from run(): it needs the recognizer composition, and a
// package may want the closure report on its own.
[[nodiscard]] Report round_trip_report(const SemanticPackageManifest& manifest,
                                       const ComposedSemantics& composed);

// The kit's own marker probe, EXPORTED for regression tripwires (jenkins_retrofit_gates.md §4,
// leg L-C). The repaired construction is `render_row(paired_writer_row(row), "probe")` — the
// writer dual materialized, self-adapting over every extractor. The OLD form (`prefix + " probe"`)
// yielded, for the Jenkins STAGE row, `[Pipeline] { ( probe` — a probe that fires NOWHERE, which
// made the `dialect_gate.marker_leak` leg vacuous. A consumer asserting "this probe FIRES on its
// own dialect stream" observes THIS function, so a regression to the old form is a loud red in
// the consumer, never a silent vacuity inside the kit. Returns "" for an UNPAIRED row (already
// red under grammar.unpaired_marker).
[[nodiscard]] std::string marker_probe_for(const IntentMarkerRow& row,
                                           std::span<const IntentEmitRow> emits);

} // namespace insight::semantic::conformance

// ════════════════════════════════════════════════════════════════════════════════════════════════════
// Implementation (inline in the module interface — the kit is small, pure, and self-contained so an
// external author can read the whole gate in one file).
// ════════════════════════════════════════════════════════════════════════════════════════════════════
namespace insight::semantic::conformance
{

// The payload every probe carries. One benign single-token ASCII word: not a Jenkins
// kStepExcludes structural token, no parens/brackets/whitespace/CR that a payload extractor
// would trim — so a probe that fails to fire is a real gate failure, never an artifact of the
// probe. Deterministic (a fixed literal, no RNG). Module linkage (not in the unnamed namespace)
// because the exported marker_probe_for below renders with it.
constexpr std::string_view kProbePayload{"probe"};

// A probe line for an INTENT MARKER row: the row's own writer dual, materialized. `prefix +
// " probe"` is only a valid probe for a RemainderAfterPrefix row, and building one that way was
// a live defect — for the Jenkins STAGE row it yields `[Pipeline] { ( probe`, which fails
// RemainderToClosingParen's required line-final ')', so the row cannot fire and the
// `dialect_gate.marker_leak` leg then asserted "it did not fire on a foreign stream" about a
// probe that fires NOWHERE. A gate that cannot fail. It would have been vacuous for every
// GitLab row too (NumericFieldThenRemainder needs a numeric field the naive probe has no way to
// produce), which is what surfaced it.
//
// The repair is the canonical inverse, which `round_trip_report` already uses: render the paired
// writer row. That makes the probe self-adapting over every present and future extractor by
// construction, because the two projections are each other's duals. An UNPAIRED row has no
// probe — `check_grammar_wellformed` fails that separately (grammar.unpaired_marker), so the
// empty string here reaches only a manifest already red, and it fires no row.
// EXPORTED (declared in the interface block above) so the Jenkins retrofit's L-C leg can assert
// the kit's own probe fires — vacuity-by-regression guarded outside the kit.
std::string marker_probe_for(const IntentMarkerRow& row, std::span<const IntentEmitRow> emits)
{
    const IntentEmitRow* writer{paired_writer_row(row, emits)};
    return writer == nullptr ? std::string{} : render_row(*writer, kProbePayload);
}

namespace
{

    // A byte is ASCII when its high bit is clear (< 0x80). Locale-safety is structural: every row
    // key is ASCII and every matcher is a byte comparison, so no locale-sensitive path exists (the
    // II-6 / ASCII-only determinism hazard — det_math musts). We ASSERT the ASCII property rather
    // than argue it.
    [[nodiscard]] bool is_ascii(std::string_view str) noexcept
    {
        return std::ranges::all_of(str, [](char chr) noexcept
                                   { return static_cast<unsigned char>(chr) < 0x80U; });
    }

    // Declared dialects distinct from a row's own gate, for the "does NOT fire cross-dialect" leg
    // (ADR 0065 clause 1 — the gate is a composed package NAME). Two are enough and they are
    // deliberately different in kind: `kUndeclaredDialect` is the caller declining to declare (the
    // fail-closed leg, which a `kAnyDialect` row must still survive), and `kForeignDialect` is a
    // real, different, composed package name.
    //
    // ⚠ The foreign name must be one the SUT manifest is not. `run()` composes ONE manifest at a
    // time, so a name no package carries would fatal `for_stream` on the unknown-dialect path
    // before any probe ran; the checks below therefore build the foreign view from a second,
    // synthetic manifest carrying only that name. See `dialect_leak_view`.
    constexpr std::string_view kUndeclaredDialect{};
    constexpr std::string_view kForeignDialect{"conformance-foreign-dialect"};

    // A probe line for a row that has no writer dual — the structural-role rows. The key is
    // line-anchored and a role row carries no payload grammar, so `key + " probe"` matches iff the
    // row fires.
    [[nodiscard]] std::string probe_for(std::string_view prefix)
    {
        return std::string{prefix} + ' ' + std::string{kProbePayload};
    }

    // ── Check 1: determinism — identical identity + identical recognizer output across independent
    // runs ──
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

        // Recognizer output must be bit-identical run-to-run for every derived probe.
        for (const IntentMarkerRow& row : manifest.markers)
        {
            const std::string probe{marker_probe_for(row, manifest.emits)};
            const ComposedSemantics first_view{first.for_stream(manifest.name, row.channel_gate)};
            const ComposedSemantics second_view{second.for_stream(manifest.name, row.channel_gate)};
            const auto lhs{insight::tokenization::recognize(probe, first_view)};
            const auto rhs{insight::tokenization::recognize(probe, second_view)};
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

    // ── Check 2: dialect-gate honesty — a gated row is inert outside its dialect; kAnyDialect
    // fires under every declaration ──
    //
    // The FOREIGN view. `run()` composes ONE manifest, so `for_stream("some-other-name", …)` would
    // fatal on the unknown-dialect path before any probe ran — canon verifies names, and that is
    // the behavior, not an obstacle to route around. So the leak leg composes the manifest under
    // test WITH a synthetic, row-less second package whose only content is a name, and resolves to
    // THAT name. Every concretely-gated row of the manifest is then legitimately absent, and every
    // kAnyDialect row is legitimately present — which is exactly the two-sided property this check
    // asserts.
    [[nodiscard]] ComposedSemantics dialect_leak_view(const SemanticPackageManifest& manifest)
    {
        const SemanticPackageManifest foreign{.name = kForeignDialect, .version = "0.0.0"};
        const std::array<SemanticPackageManifest, 2> pair{manifest, foreign};
        return compose(pair).for_stream(kForeignDialect, kAnyChannel);
    }

    // one conformance property verified end-to-end (dialect-gate honesty); the sequence of guarded
    // assertions is the check — splitting it scatters a single verdict across helpers.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    CheckResult check_dialect_gate_honesty(const SemanticPackageManifest& manifest,
                                           const ComposedSemantics& composed)
    {
        // The two views the legs are scored under, both built ONCE (ADR 0065 clause 2 — the gate is
        // a stream-scoped resolution, so the probe has to be one too; probing a row against a
        // per-call coordinate is the shape T4 removed).
        const ComposedSemantics own{composed.for_stream(manifest.name, kAnyChannel)};
        const ComposedSemantics foreign{dialect_leak_view(manifest)};

        // Structural roles.
        for (const StructuralRoleRow& row : manifest.roles)
        {
            const std::string probe{probe_for(row.prefix)};
            if (row.dialect_gate == kAnyDialect)
            {
                // Must fire under EVERY declaration — check the undeclared view and a foreign one.
                for (const auto& [view, label] :
                     {std::pair{std::cref(composed), std::string_view{"the UNDECLARED view"}},
                      std::pair{std::cref(foreign), kForeignDialect}})
                    if (insight::tokenization::classify(probe, view.get()) != row.role)
                        return {.name = "dialect_gate.role_any",
                                .passed = false,
                                .detail = "kAnyDialect role key \"" + std::string{row.prefix} +
                                          "\" failed to fire under " + std::string{label} +
                                          " — an ungated row must fire whatever the caller "
                                          "declared."};
            }
            else
            {
                // Must be present under its OWN dialect and inert under a foreign one.
                if (insight::tokenization::classify(probe, own) != row.role)
                    return {.name = "dialect_gate.role_own",
                            .passed = false,
                            .detail = "role key \"" + std::string{row.prefix} + "\" (gated to \"" +
                                      std::string{row.dialect_gate} +
                                      "\") did NOT fire on a stream declaring \"" +
                                      std::string{manifest.name} +
                                      "\" — the row is unreachable under any declaration."};
                if (insight::tokenization::classify(probe, foreign) !=
                    insight::StructuralRole::None)
                    return {.name = "dialect_gate.role_leak",
                            .passed = false,
                            .detail = "role key \"" + std::string{row.prefix} + "\" (gated to \"" +
                                      std::string{row.dialect_gate} +
                                      "\") FIRED on a stream declaring \"" +
                                      std::string{kForeignDialect} + "\" — II-6 gate leak."};
            }
        }
        // Intent markers (always concretely gated by construction — II-6): present under their own
        // MEDIUM, inert under a foreign declaration.
        //
        // The OWN leg is scored at the row's own Medium (`dialect × channel`), not against the
        // kAnyChannel view — a channel-gated marker is legitimately absent from that one, which is
        // why the leg used to be skipped altogether. Skipping it is what let the leak leg go
        // vacuous: a probe that fires NOWHERE also fails to leak, so `marker_leak` reported green
        // about a row it could never have caught. It is asserted here rather than left to
        // `round_trip_report` because that is a SEPARATE entry point a package may never call, and
        // a leg whose non-vacuity depends on another test being run is not guarded.
        for (const IntentMarkerRow& row : manifest.markers)
        {
            if (row.dialect_gate == kAnyDialect)
                continue;
            const std::string probe{marker_probe_for(row, manifest.emits)};
            const ComposedSemantics medium{
                composed.for_stream(manifest.name, row.channel_gate)};
            if (insight::tokenization::recognize(probe, medium).kind !=
                insight::tokenization::IntentMarkerKind::None)
            {
                if (insight::tokenization::recognize(probe, foreign).kind !=
                    insight::tokenization::IntentMarkerKind::None)
                    return {.name = "dialect_gate.marker_leak",
                            .passed = false,
                            .detail = "marker key \"" + std::string{row.prefix} + "\" (gated to \"" +
                                      std::string{row.dialect_gate} +
                                      "\") FIRED on a stream declaring \"" +
                                      std::string{kForeignDialect} + "\" — II-6 gate leak."};
                continue;
            }
            return {.name = "dialect_gate.marker_own",
                    .passed = false,
                    .detail = "marker key \"" + std::string{row.prefix} + "\" did NOT fire on its "
                              "OWN medium (dialect \"" +
                              std::string{manifest.name} + "\", channel \"" +
                              std::string{row.channel_gate} + "\") for the probe \"" + probe +
                              "\", which its own paired emit row rendered. The row is unreachable "
                              "under every declaration, so the leak leg below can never catch "
                              "anything — either the emit row is not the extractor's inverse, or "
                              "the row cannot match its own generated bytes."};
        }
        // Outcome tokens (grammar-2, always concretely gated — a dialect's verdict string never
        // resolves under another dialect, ADR 0025 §3.1).
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
                                  std::string{kForeignDialect} + "\" — II-6 gate leak."};
        }
        // The UNDECLARED stream is fail-closed on DEPTH: no concretely-gated row of any kind may
        // fire. This is the leg that would have caught "the filter was never applied", which the
        // foreign-view legs cannot — a filter that silently kept everything would still drop the
        // manifest's rows under a foreign NAME only if the filter runs at all.
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
                insight::tokenization::recognize(marker_probe_for(row, manifest.emits), composed)
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

    // ── Check 3: ASCII / locale safety — every row key is ASCII (byte-only matching ⇒
    // locale-independent) ──
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
        // ADR 0044 §7: the generation projection is manifest data and identity-bearing, so it is
        // held to the same locale-safety property as the recognition rows it is the dual of.
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

    // ── Check 4: grammar well-formedness — non-empty keys, no self-conflict, LocationRow
    // param/kind match ──
    // one conformance property verified end-to-end (grammar well-formedness); the guarded-assertion
    // sequence is the check, not decomposable sprawl.
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
        // ADR 0044 §7 / SID-2: with `emits` on the manifest, "a reader without a writer" is a
        // MANIFEST property and the runtime kit can state it as one. The DialectIntent concept
        // already refuses it at compile time for a package that models the concept — this catches
        // the package that declares markers on its manifest but wires `.emits` to a different (or
        // absent) array, which the concept cannot see and which would make the identity hash cover
        // a generation projection nothing round-trips.
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

        // Self-conflict: a package must not carry an exact-duplicate key within itself (the runtime
        // compose would fatal on it — the kit catches it as a well-formedness failure, not a
        // crash).
        const std::array<SemanticPackageManifest, 1> one{manifest};
        if (const ConflictInfo conflict{find_conflict(one)}; conflict.has_conflict)
            return {.name = "grammar.self_conflict",
                    .passed = false,
                    .detail = "the package carries an exact-duplicate " +
                              std::string{conflict.kind} + " key \"" + std::string{conflict.key} +
                              "\" within its own rows (would fatal at compose)."};

        // LocationRow: the selected LocationMatchKind must be parameterized by the params that
        // algorithm reads.
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

    // ── Check 5: code-tier well-behaved — the strategy/hook (if any) are deterministic + in-range
    // ──
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
            // confidence() must be O(1), in [0,1], and deterministic (same line → same score).
            constexpr std::string_view kProbe{"2026-01-01T00:00:00.0000000Z probe line"};
            const double first{strategy->confidence(kProbe)};
            const double second{strategy->confidence(kProbe)};
            // Written as the negation of an in-range test, NOT `first < 0.0 || first > 1.0`: a NaN
            // confidence fails `>= 0.0 && <= 1.0` (⇒ out of range, caught), but passes neither
            // `< 0.0` nor `> 1.0`, so the DeMorgan form would let NaN slip through.
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
    return report;
}

namespace
{

    // Verbose-on-failure enum names (no canon to_string exists for these two — G2's diagnostic
    // needs them).
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
        return order == ChildOrder::Unordered ? "Unordered" : "Ordered";
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
            // The DialectIntent concept should have rejected this at compile time; the runtime kit
            // asserts it too so an external author who bypassed the concept still gets a legible
            // failure.
            report.checks.push_back(
                {.name = "round_trip.unpaired",
                 .passed = false,
                 .detail =
                     "recognition marker \"" + std::string{reader.prefix} +
                     "\" has NO paired generation row — a reader without a writer (SID / G2). "
                     "DialectIntent<Dialect> should have refused to compile."});
            continue;
        }

        // ADR 0029 D1 / ADR 0065 clause 1: the Medium is `dialect × IntentChannel`, so the
        // round-trip closes PER MEDIUM — a row is recognized under the stream view its writer
        // materializes into. Reading BOTH coordinates off the ROW keeps the kit self-adapting (zero
        // per-package config): a kAnyChannel row round-trips under the undeclared channel view, a
        // channel-gated row under its own channel, and every row under its own dialect. Recognizing
        // every row against ONE composition would be asking whether the stripped banner is a banner
        // in the annotated channel — which is the phantom, not the closure.
        const std::string line{render_row(*writer, kProbePayload)};
        const ComposedSemantics medium_view{
            composed.for_stream(writer->dialect_gate, writer->channel_gate)};
        const insight::tokenization::IntentMarker got{
            insight::tokenization::recognize(line, medium_view)};

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

} // namespace insight::semantic::conformance
