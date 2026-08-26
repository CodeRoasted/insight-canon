// insight.canon.conformance — the permanent, package-agnostic CONFORMANCE KIT (ADR-17,
// SRC-SP-2). The canon-shipped harness EVERY semantic package (ours or an external author's) must
// pass. A package's test target instantiates it in one line:
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
// It is the OPEN-SOURCE HONESTY MECHANISM (SRC-SP-2): the identical kit ships installed so an
// external package author runs the same gate CodeRoast's own packages are held to.
// Verbose-on-failure matters doubly here — a failing check must diagnose itself for someone who has
// never read canon's internals.
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
// Reads BOTH projections off the MANIFEST (ADR-23 — the G4 identity wiring this signature was
// waiting on: `emits` is now a manifest member, so the rows the kit round-trips are the same rows
// `semantic_identity` hashes). It formerly took the two spans separately, from the dialect TYPE,
// because the manifest had no emit member; that split meant the kit could have closed over one
// array while the digest covered another — precisely the two-writers-one-identity divergence
// SRC-SID-2 forbids. Kept a SEPARATE entry point from run(): it needs the recognizer composition,
// and a package may want the closure report on its own.
[[nodiscard]] Report round_trip_report(const SemanticPackageManifest& manifest,
                                       const ComposedSemantics& composed);

// The MANIFEST EQUIVALENCE comparator (DN-17.D21 §5) — "do these two semantic packages agree,
// FIELD FOR FIELD?". A THIRD question, so a third entry point rather than a mode on an existing
// one: run() asks whether ONE package satisfies the contract, round_trip_report() asks whether one
// package's two projections close on each other, and this asks whether TWO packages are the same
// ruleset. The kit is canon's package-introspection surface, which is why an equivalence report
// lives in a module named for conformance; the tension is named rather than smoothed, and if the
// kit grows a fourth question the module earns a rename, not a split.
//
// WHY IT EXISTS WHEN A DIGEST ALREADY ANSWERS THE QUESTION. `compose({m}).identity()` decides
// equality better than any comparator can — it covers every data-tier field the serializer writes
// and is wrong only on a 2^-128 collision — but it is a 16-byte hash, so it can only ever say
// THAT two rulesets differ. This says WHERE: which member, which index, which field, and both
// values. That locator is the entire deliverable, which is why the report is a per-member Report
// and never a bool.
//
// SCOPE, AND IT IS NARROWER THAN "the packages behave identically". The two CODE-TIER members are
// compared by PRESENCE ONLY — their checks are named `strategy_presence_only` and
// `echoed_source_presence_only` so the report itself says so, in the report, to a reader who never
// opened this file. Two different packages hold two different function symbols, so comparing
// pointer values is a guaranteed can't-PASS, and `compose.cpp`'s serializer makes the same call for
// the same reason ("code tier, nominal"). Whether the two code tiers COMPUTE the same answers is a
// separate obligation with a separate leg (DN-17.D21 §2, leg 2b: run both hooks over a fixture set
// and require verdict-for-verdict agreement) and nothing here may be read as covering it.
//
// It is an EQUIVALENCE report, not a NON-VACUITY report: two empty manifests are genuinely
// equivalent and this returns 14 green checks for them. A caller who needs the comparison to mean
// something must feed it a package with rows — the subject is the caller's to choose, and
// pretending otherwise would make an empty package permanently non-conformant.
//
// Rows pair BY INDEX, matching the identity serializer, which walks each span in declared order:
// two manifests whose rows are the same SET in a different ORDER hash differently and are reported
// differently here too, because declared order is ruleset content.
//
// Pure and deterministic: reads the two manifests and allocates only the report strings. Always
// returns exactly one check per manifest member.
[[nodiscard]] Report manifest_equivalence_report(const SemanticPackageManifest& lhs,
                                                 const SemanticPackageManifest& rhs);

// The kit's own marker probe, EXPORTED for regression tripwires (bibles/jenkins_dialect.md §3,
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
    // SRC-II-6 / ASCII-only determinism hazard — det_math musts). We ASSERT the ASCII property
    // rather than argue it.
    [[nodiscard]] bool is_ascii(std::string_view str) noexcept
    {
        return std::ranges::all_of(str, [](char chr) noexcept
                                   { return static_cast<unsigned char>(chr) < 0x80U; });
    }

    // Declared dialects distinct from a row's own gate, for the "does NOT fire cross-dialect" leg
    // (ADR-22 — the gate is a composed package NAME). Two are enough and they are
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

    // The kit's ONE door to the walkers' NormalizedContent. The probes this kit synthesizes
    // (render_row / probe_for / marker_probe_for) are ESCAPE-FREE BY CONSTRUCTION, so stage 1 is
    // a FIXED POINT on them: `normalize()` copies nothing (the returned content views the probe
    // itself, so `scratch` is untouched and a caller-scoped scratch may be shared across probes),
    // no count can move, and the kit exercises the same public ingest a production consumer does.
    // ⚠ NEVER the LogParser mint here — that would grow its friend list to two and delete the
    // mechanism (ADR-21.D4 — the friend list IS the audit surface; named in advance).
    [[nodiscard]] insight::tokenization::NormalizedContent normalized_probe(std::string_view probe,
                                                                            std::string& scratch)
    {
        return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
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
        // The two views the legs are scored under, both built ONCE (ADR-22 — the gate is
        // a stream-scoped resolution, so the probe has to be one too; probing a row against a
        // per-call coordinate is the shape T4 removed).
        const ComposedSemantics own{composed.for_stream(manifest.name, kAnyChannel)};
        const ComposedSemantics foreign{dialect_leak_view(manifest)};

        // Structural roles.
        std::string scratch;
        for (const StructuralRoleRow& row : manifest.roles)
        {
            const std::string probe{probe_for(row.prefix)};
            if (row.dialect_gate == kAnyDialect)
            {
                // Must fire under EVERY declaration — check the undeclared view and a foreign one.
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
                // Must be present under its OWN dialect and inert under a foreign one.
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
        // Intent markers (always concretely gated by construction — SRC-II-6): present under their
        // own MEDIUM, inert under a foreign declaration.
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
        // Outcome tokens (grammar-2, always concretely gated — a dialect's verdict string never
        // resolves under another dialect, ADR-17).
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
        // ADR-23: the generation projection is manifest data and identity-bearing, so it is
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
        // ADR-23 / SRC-SID-2: with `emits` on the manifest, "a reader without a writer" is a
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

    // ── Check 6: the outcome round-trip law (T5 §3.1) — scan_run_outcome(render_outcome(…))
    // recovers the row's own verdict ──
    //
    // The writer dual of the console-tail scan, held to the same one-owner closure as the intent
    // rows' G2 round-trip: for every outcome-marker row, the line `render_outcome` materializes is
    // recognized back by the shipped scan under the package's OWN declaration, and the verdict it
    // resolves is the one the rendered row/token carries. Self-adapting over both grammar-5 shapes:
    // a RemainderToken row round-trips once per declared outcome token (the remainder maps through
    // the SAME token set both ways), a PrefixIsVerdict row round-trips its prefix alone (its
    // remainder is free-form and unread). Trivially green for a package that ships tokens but no
    // marker (GHA — it has no run-verdict console line to render); the law is about the marker's
    // two projections, not about token existence.
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

    // Verbose-on-failure enum names (no canon to_string exists for these two — G2's diagnostic
    // needs them). Both are switches with no `default:` label so the `-Werror=switch` / `/we4062`
    // option set in core/CMakeLists.txt covers them; `order_name` is a switch over a two-valued
    // enum for exactly that reason — as the equivalent ternary it was outside the option's reach
    // and a third ChildOrder would have printed "Ordered" for the new value, silently.
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

        // ADR-22 / ADR-22: the Medium is `dialect × IntentChannel`, so the
        // round-trip closes PER MEDIUM — a row is recognized under the stream view its writer
        // materializes into. Reading BOTH coordinates off the ROW keeps the kit self-adapting (zero
        // per-package config): a kAnyChannel row round-trips under the undeclared channel view, a
        // channel-gated row under its own channel, and every row under its own dialect. Recognizing
        // every row against ONE composition would be asking whether the stripped banner is a banner
        // in the annotated channel — which is the phantom, not the closure.
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

// ════════════════════════════════════════════════════════════════════════════════════════════════════
// The MANIFEST EQUIVALENCE comparator (DN-17.D21 §5) — implementation.
// ════════════════════════════════════════════════════════════════════════════════════════════════════

namespace
{

    // How many differing rows one member's diagnostic enumerates before it summarizes, and how many
    // unpaired rows a length mismatch names. Bounded on purpose: a report is read by a human, and
    // an 800-row diff is a wall, not a locator. The COUNTS are always exact — only the enumeration
    // is capped, so the cap can never make a difference look smaller than it is.
    constexpr std::size_t kMaxReportedRowDiffs{8};
    constexpr std::size_t kMaxReportedUnpairedRows{4};

    // ── Value renderers ─────────────────────────────────────────────────────────────────────────
    // Verbose-on-failure IS the deliverable here: leg 3 (semantic_identity equal) already decides
    // the boolean, so this report earns its keep only by locating. Every enum therefore prints as
    // its NAME — "extract: lhs=RemainderAfterPrefix rhs=None" locates a defect, "extract: lhs=1
    // rhs=0" sends the reader back to the header to decode a byte. Produced strings are ASCII only:
    // the report crosses into an external author's test framework and the surrounding kit asserts
    // ASCII of every row key, so the diagnostic must not be the one place a multi-byte glyph
    // enters.
    //
    // The five *_name switches below carry no `default:` label, deliberately, and what enforces
    // that is a build option rather than this comment: `core/CMakeLists.txt` sets `-Werror=switch`
    // (GCC/Clang) and `/we4062` (MSVC) PRIVATE on `insight_canon` and `insight_canon_tests`, so an
    // unhandled enumerator is a compile error HERE instead of a "?" in a diagnostic nobody reads
    // twice. Measured 2026-08-26 on `extract_name`. `dual()` in canon.spi.cppm stands on the same
    // option for the same reason. The trailing return exists only because the function is non-void.

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

    // ── Field equality ──────────────────────────────────────────────────────────────────────────
    // `std::span` has NO operator== (deliberately, upstream: it is a view, and the committee
    // refused to pick between identity and element-wise), which is exactly why this comparator has
    // to exist at all — the manifest is nine spans and no compiler-generated equality can be
    // defaulted onto it. The constrained template therefore does not apply to a span at all and the
    // span overload takes it, element-wise, which is the semantics the identity serializer encodes.
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

    // Accumulates the FIELDS of one row pair that differ, as "field: lhs=<v> rhs=<v>",
    // comma-joined. Empty text is the equality verdict: a row pair is equal exactly when no field
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

    // ── Row comparison ──────────────────────────────────────────────────────────────────────────
    // ⚠ THE STRUCTURED BINDING IS THE COVERAGE INSTRUMENT, NOT A STYLE CHOICE — do not "simplify"
    // these to member access. A comparator whose apparent subject is "the row" and whose real
    // subject is "the fields someone remembered" is a silent, green, wrong answer, and the failure
    // is invisible precisely because a partial comparator still reports. A structured binding must
    // name EXACTLY as many members as the type has, so adding a field to any row struct — or to the
    // manifest itself, bound the same way in manifest_equivalence_report below — is a COMPILE ERROR
    // at that line, in this file, on the next build. The instrument costs one line per row kind and
    // it is the only thing standing between this function and the defect class it lives inside.

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
        // `outcome` is INERT on a RemainderToken row (its verdict rides its remainder), and it is
        // compared anyway — for the same reason the identity serializer writes it for every row:
        // the manifest's content is a fixed field layout, not a per-shape union, so a row that
        // changed shape must report the field that moved rather than have the comparison shift
        // underneath.
        diff.field("outcome", lhs_outcome, rhs_outcome);
        return diff.text();
    }

    // The short human handle a row is named by when a LENGTH mismatch leaves it unpaired. A
    // LocationRow has no key of its own — it is an ALGORITHM plus its vocabulary — so it names its
    // algorithm, which is the only field a reader can act on without the full row.
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

    // Name the rows one side declares past the paired prefix — the "what was added / removed"
    // half of a length mismatch, which the paired-row diff structurally cannot show.
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

    // One manifest row-span member, compared row by row and field by field. Rows pair BY INDEX
    // (the identity serializer's own semantics — declared order is ruleset content), so a length
    // mismatch is reported as counts plus the unpaired keys, and the paired prefix is still
    // compared: an appended row and a mutated row are different findings and a reader needs both.
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

    // A declared VOCABULARY member (channels, dialect_revisions): a short closed set, so both sides
    // print in full rather than by index — the whole value IS the locator at this size.
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

    // The code tier, PRESENCE ONLY — see the entry point's contract above. The two packages hold
    // two different symbols, so a pointer compare is a can't-PASS, and the behavioural comparison
    // is a separate leg. The check NAME carries the word `presence_only` so the scope travels with
    // the report into a framework that shows nothing but names.
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
    // ⚠ THE STRUCTURED BINDING IS THE COVERAGE INSTRUMENT (see the row_differences block above).
    // SemanticPackageManifest has fourteen members and this names fourteen; adding a fifteenth is a
    // COMPILE ERROR here rather than a member this report silently never looks at. That failure —
    // a comparator whose apparent subject is "the manifest" and whose real subject is "the members
    // someone remembered" — is the one this function is most likely to have, because it reports
    // either way and the missing member's check simply never appears in a list nobody counts.
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
