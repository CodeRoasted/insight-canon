// insight.canon.compose — static composition of semantic packages into a ComposedSemantics
// (ADR 0024 §3/§4). The consumer binary names its composition ONCE —
// `compose(std::array{github::kManifest, test_frameworks::kManifest})` — and threads the result
// into every Tokenizer. Composition is:
//   - STATIC: fixed by which manifests the call names (no dynamic loading).
//   - EXPLICIT: no static-init self-registration.
//   - CANONICALLY ORDERED: row order is a pure function of the package SET — core rows first (none
//     today), then packages sorted by name, declared order within — never link/registration order,
//     because table order is template identity.
//   - FAIL-CLOSED: an exact-duplicate match key across rows is a build error (constexpr
//     `find_conflict`, usable in static_assert) / a startup fatal invariant (the runtime compose).
//   - IDENTITY-BEARING: the composed rule set gets a content hash (semantic_identity, §4) over its
//     canonical serialization — the II-7 comparability key.
//
// Public + installed (product binaries call compose). The facade `export import`s this so
// `import insight.canon;` yields Tokenizer + compose + ComposedSemantics. It plain-imports spi
// (does NOT re-export it) — a consumer naming SemanticPackageManifest imports the package module
// (which carries spi); the provider contract stays off the consumer's default surface.
module;

export module insight.canon.compose;
import insight.canon.internal; // std + global C fixed-width types
import insight.canon.api;      // StructuralRole/IntentMarkerKind/LogFormat/…
import insight.canon.spi; // the grammar rows + SemanticPackageManifest (plain import — not re-exported)
import insight.canon.transport; // the transform catalogue — its version + rows are identity (0044 §6)

export namespace insight::semantic
{

// The truncated-SHA-256 width of semantic_identity (§4.1 — the TemplateId 16-byte precedent).
inline constexpr std::size_t kSemanticIdentityBytes{16};

// A composed package's identity, carried for the wire block's legibility (§4.2 — an operator can
// read what vocabulary a report understood). The hash (§4.1) is the key; this list is the label.
struct ComposedPackage
{
    std::string_view name;
    std::string_view version;
    bool has_strategy;      // the package shipped a dialect format strategy (code tier)
    bool has_echoed_source; // the package shipped an echoed-source provenance hook (code tier)
};

// A non-fatal shadowing note (§3 — prefix shadowing that is NOT an exact duplicate resolves
// longest-match-wins and is SURFACED here, never silent). Empty for the current two packages (no
// nested prefixes), but the mechanism is real.
struct ShadowNote
{
    std::string_view kind;           // "role" | "marker" | "level_lift"
    std::string_view shorter_prefix; // the shadowed (shorter) prefix
    std::string_view longer_prefix;  // the shadowing (longer) prefix — wins on a line matching both
};

// The composition report (§3): the shadowing notes surfaced during composition. A pure record; the
// fail-closed conflicts are NOT here (they abort composition before a report exists).
struct CompositionReport
{
    std::vector<ShadowNote> shadows;
};

// An exact-duplicate conflict (§3, fail-closed): same rule class + same key (prefix) + intersecting
// format gate across two rows. `has_conflict == false` is the empty sentinel. constexpr so a
// composition TU can `static_assert(!find_conflict(pkgs).has_conflict, …)` — the build-time half of
// G-SP-5; the runtime compose fatals on the same condition (defense in depth).
struct ConflictInfo
{
    bool has_conflict{false};
    std::string_view kind; // "role" | "marker" | "level_lift" | "value_class"
    std::string_view key;  // the duplicated prefix / value-class key
};

// Scan the manifest set for an exact-duplicate match key. Pure over the manifest DATA (constexpr:
// string_view compare + span iteration), so usable in a static_assert AND by the runtime compose.
[[nodiscard]] constexpr ConflictInfo
find_conflict(std::span<const SemanticPackageManifest> packages) noexcept;

// The composed rule set: the canonically-ordered, conflict-free tables the core mechanisms walk,
// the code-tier seams (strategy factories + provenance hooks), the package list, and the content
// hash. Owns its row storage (small POD copied from the manifest spans in canonical order; the
// pointed-at bytes stay alive in package static storage — SP-7). Built once per binary; passed by
// const-ref to every Tokenizer. Move-only.
class ComposedSemantics
{
  public:
    ComposedSemantics(const ComposedSemantics&) = delete;
    ComposedSemantics& operator=(const ComposedSemantics&) = delete;
    ComposedSemantics(ComposedSemantics&&) noexcept = default;
    ComposedSemantics& operator=(ComposedSemantics&&) noexcept = default;
    ~ComposedSemantics() = default;

    // ── The composed tables (canonical order) ──
    [[nodiscard]] std::span<const StructuralRoleRow> roles() const noexcept
    {
        return roles_;
    }
    [[nodiscard]] std::span<const IntentMarkerRow> markers() const noexcept
    {
        return markers_;
    }
    [[nodiscard]] std::span<const LevelLiftRow> level_lifts() const noexcept
    {
        return level_lifts_;
    }
    [[nodiscard]] std::span<const LocationRow> locations() const noexcept
    {
        return locations_;
    }
    // The christened ValueClassRegistry (ADR 0024 §5): the composed view over the package
    // ValueClassRow seat. In 1.7.5 no package ships a value class (we do not build dormant
    // vocabulary), so this is empty — the UNIVERSAL value concepts (kOrdinalFieldCatalog /
    // kOtelFieldCatalog / the KEEP lexicons) stay core (the ratified rule), consumed directly. The
    // registry is the point where a future package's client-ordinal / domain value classes compose
    // in — the grammar seat exists, its unification with the core catalogs waits for a real
    // consumer.
    [[nodiscard]] std::span<const ValueClassRow> value_classes() const noexcept
    {
        return value_classes_;
    }
    // The run-outcome vocabulary (grammar-2, ADR 0025): the composed dialect verdict maps + the
    // console-tail markers. Consumed by map_outcome_token / scan_run_outcome / resolve_run_outcome.
    [[nodiscard]] std::span<const OutcomeTokenRow> outcome_tokens() const noexcept
    {
        return outcome_tokens_;
    }
    [[nodiscard]] std::span<const OutcomeMarkerRow> outcome_markers() const noexcept
    {
        return outcome_markers_;
    }
    // ADR 0029 D5 — the composed INTENT CHANNEL vocabulary: every channel any package declares.
    // This is the closed set a caller's `--channel` is validated against, and the list an unknown
    // channel's error names.
    [[nodiscard]] std::span<const std::string_view> channels() const noexcept
    {
        return channels_;
    }

    // ── The channel-filtered view (ADR 0029 D1/D2/D5) ──
    // Build the vocabulary ONE stream declares, at stream open. `declared_channel` is the caller's
    // provenance fact (D2 — never auto-detected: a content heuristic decides the channel from a
    // PREFIX of the stream, so a later line can contradict an earlier decision ⇒ content
    // non-determinism under streaming). The returned composition drops every marker row gated to a
    // different channel, so:
    //
    //   * the HOT PATH never sees a channel — recognize() walks a plain row span, zero per-line
    //   cost, and
    //     its signature is unchanged;
    //   * one IntentChannel per TREE (D5) is STRUCTURAL, not merely asserted — a sibling channel's
    //   rows
    //     are not in the table, so a multi-channel tree is unrepresentable rather than rejected;
    //   * `kAnyChannel` rows survive every filter, so single-materialization dialects are
    //   untouched.
    //
    // `kAnyChannel` (empty) as the ARGUMENT means Unspecified — the caller did not declare: every
    // concretely-gated row drops ⇒ no dialect structure ⇒ the raw-text fallback. That is
    // fail-closed on DEPTH, not on the run, and it is deliberate: never default an undeclared
    // stream to a concrete channel, because "both channels' rows live at once" IS the phantom
    // defect this exists to kill.
    //
    // FATALS on an UNKNOWN channel (a non-empty name no package declares — e.g.
    // `--channel=annotatd`), listing the declared vocabulary. An unknown channel is a MISTAKE; an
    // absent channel is a CHOICE; they must not share a code path — silently degrading a typo to
    // the fallback is the exact silent-fallback bug class this workstream has already paid for
    // twice.
    //
    // Cold path by construction: called once per stream, copies ~10 POD rows. The pointed-at bytes
    // stay in package-static storage (SP-7), so the copy is trivial and the identity is preserved
    // verbatim — semantic_identity is the RULESET's identity, not a stream's view of it.
    [[nodiscard]] ComposedSemantics for_channel(std::string_view declared_channel) const;

    // Would declaring an IntentChannel unlock recognition this view is withholding? (ADR 0029 D5's
    // diagnostic.) True iff some marker row for `format` is channel-gated to a channel that
    // `declared_channel` does not admit — i.e. this dialect HAS materializations and the caller has
    // not said which one it acquired, so depth is being withheld and saying so would unlock it.
    //
    // A narrow QUERY, deliberately not an `all_markers()` accessor: exposing the unfiltered table
    // would re-open the fail-open door this class exists to close (a caller could walk it and
    // recognize against every channel at once — the phantom defect). Returns false for a
    // single-materialization dialect (Jenkins), so a Jenkins user is never told to declare a
    // channel that does not apply to them — a diagnostic that fires where it cannot help is exactly
    // the fatigue the product is against.
    [[nodiscard]] bool withholds_markers_for(insight::LogFormat format,
                                             std::string_view declared_channel) const noexcept;

    // ── The code-tier seams ──
    [[nodiscard]] std::span<const StrategyFactory> strategy_factories() const noexcept
    {
        return strategies_;
    }
    [[nodiscard]] std::span<const ProvenanceHook> provenance_hooks() const noexcept
    {
        return provenance_hooks_;
    }

    // ── Identity + legibility (§4) ──
    [[nodiscard]] const std::array<std::uint8_t, kSemanticIdentityBytes>& identity() const noexcept
    {
        return identity_;
    }
    [[nodiscard]] std::string identity_hex() const; // rendered hex, only at seams
    [[nodiscard]] std::span<const ComposedPackage> packages() const noexcept
    {
        return packages_;
    }
    [[nodiscard]] const CompositionReport& report() const noexcept
    {
        return report_;
    }

  private:
    ComposedSemantics() = default;
    friend ComposedSemantics compose(std::span<const SemanticPackageManifest>);

    std::vector<StructuralRoleRow> roles_;
    // The marker rows THIS composition recognizes — already channel-filtered (ADR 0029 D5). A
    // freshly composed vocabulary is the UNSPECIFIED view: no caller declared a channel, so every
    // concretely-gated row is absent and only kAnyChannel rows fire (fail-closed). for_channel()
    // re-derives this from all_markers_.
    std::vector<IntentMarkerRow> markers_;
    // Every marker row the packages declared, channel-gated or not — the SOURCE for_channel()
    // filters, never walked by recognition. Kept private and separate on purpose: if the full set
    // were the public `markers()`, the default composition would fire BOTH GHA Step rows at once,
    // which IS the phantom defect. Fail-closed has to be the DEFAULT, not an opt-in a caller can
    // forget (ADR 0029 D5's promoted MUST — a safety default that must be requested is not a
    // default).
    std::vector<IntentMarkerRow> all_markers_;
    std::vector<LevelLiftRow> level_lifts_;
    std::vector<LocationRow> locations_;
    std::vector<ValueClassRow> value_classes_;
    std::vector<OutcomeTokenRow> outcome_tokens_;
    std::vector<OutcomeMarkerRow> outcome_markers_;
    std::vector<std::string_view> channels_; // ADR 0029 — the composed declared channel vocabulary
    std::vector<StrategyFactory> strategies_;
    std::vector<ProvenanceHook> provenance_hooks_;
    std::vector<ComposedPackage> packages_;
    CompositionReport report_;
    std::array<std::uint8_t, kSemanticIdentityBytes> identity_{};
};

// ── The per-stream resolution of an IngestDeclaration (ADR 0044 §6) ──────────────────────────────
// What ONE stream analyzes with: the channel-filtered vocabulary and the resolved transport stack.
// Move-only, because ComposedSemantics is.
struct ResolvedStream
{
    ComposedSemantics semantics;                  // the declared channel's view of the ruleset
    insight::transport::TransportStack transport; // resolved once, before the first line (§4)
};

// Resolve a declaration against a composition — the ONE call a caller makes at stream open, and the
// only place the three declared coordinates are checked together.
//
// CANON VERIFIES, NEVER INFERS (ADR 0030's split, not reopened). Each coordinate fails closed on an
// UNKNOWN value, naming the known vocabulary, and degrades on an ABSENT one:
//   * `dialect`   — must name a composed package; unknown ⇒ hard error listing the composed names.
//                   Absent ⇒ no dialect assertion (today's behavior). Verified, not yet GATING:
//                   it becomes the successor to per-row `format_gate` at T4. Verification alone
//                   already earns its place — it turns `--dialect=guthub` into a named error
//                   instead of a silently structure-less analysis.
//   * `channel`   — delegated to for_channel(), whose fail-closed posture is ADR 0029 D5's and is
//                   unchanged here.
//   * `stack`     — delegated to resolve_transport_stack(); unknown transform ⇒ hard error listing
//                   the catalogue.
//
// A DEFAULT-CONSTRUCTED DECLARATION IS EXACTLY TODAY'S BEHAVIOR — empty stack (the peel is the
// identity function), no dialect assertion, the Unspecified channel view. That is the G1 case, and
// it is what makes declaring purely SUBTRACTIVE: a caller who says nothing loses nothing.
//
// Note what this function does NOT return: anything the tokenizer takes. The stack is handed back
// to the CALLER, who peels and passes `PeeledLine::content` on. There is deliberately no path from
// here into the identity path (§4).
[[nodiscard]] ResolvedStream
resolve_stream(const ComposedSemantics& composed,
               const insight::transport::IngestDeclaration& declaration);

// Compose the manifest set. Sorts packages by name (canonical order — independent of the caller's
// argument order), concatenates rows in declared order, FATALS on an exact-duplicate key (the
// startup fail-closed invariant — a clear message then std::terminate), records longest-match
// shadow notes, and computes semantic_identity over the canonical serialization (§4). The
// degenerate case (empty span) is a defined, runnable state: no rows, no strategies, the hash of
// just (grammar-version + kCanonicalizationVersion).
[[nodiscard]] ComposedSemantics compose(std::span<const SemanticPackageManifest> packages);

// ── find_conflict — constexpr definition (inline so it is usable in static_assert at any TU) ──
namespace detail
{
    // Two format gates INTERSECT when a single line could satisfy both: equal, or either is
    // kAnyFormat. The COMPOSITION-time predicate: it answers "could these two rows ever both
    // claim one line?" and drives the fail-closed duplicate check.
    [[nodiscard]] constexpr bool gates_intersect(insight::LogFormat lhs,
                                                 insight::LogFormat rhs) noexcept
    {
        return lhs == rhs || lhs == kAnyFormat || rhs == kAnyFormat;
    }

    // A row's format gate MATCHES a line's routed format when the gate is kAnyFormat (fire on any
    // format — the pre-split ungated behavior) or equals the concrete format. The RECOGNITION-time
    // predicate every composed-row walker shares (classify / recognize / lift_level), homed here so
    // the two gate questions sit side by side and cannot drift apart in separate copies.
    //
    // Deliberately NOT gates_intersect: a line whose routed format is Unknown must NOT trigger a
    // concretely-gated dialect row (the pre-split `format != GitHubActions → {}` guard).
    // gates_intersect would fire it, because Unknown is a concrete enumerator and the intersect
    // question is the wrong one at recognition time.
    [[nodiscard]] constexpr bool gate_matches(insight::LogFormat row_gate,
                                              insight::LogFormat line_format) noexcept
    {
        return row_gate == kAnyFormat || row_gate == line_format;
    }
} // namespace detail

namespace detail
{
    // Every unordered (package,row) pair exactly ONCE: pkg_b>pkg_a, or pkg_b==pkg_a with
    // idx_j>idx_i. Never a row against itself, and address-independent — detects an intra-package
    // duplicate AND two packages that happen to share the same static row array (position, not
    // address, is the key). Keyed on the shared .prefix + .format_gate members (roles / markers /
    // level-lifts). Returns the duplicated prefix, or nullopt.
    template <typename Row>
    [[nodiscard]] constexpr std::optional<std::string_view>
    first_prefix_dup(std::span<const SemanticPackageManifest> packages,
                     std::span<const Row> SemanticPackageManifest::* member) noexcept
    {
        for (std::size_t pkg_a{0}; pkg_a < packages.size(); ++pkg_a)
        {
            const std::span<const Row> rows_a{packages[pkg_a].*member};
            for (std::size_t idx_i{0}; idx_i < rows_a.size(); ++idx_i)
                for (std::size_t pkg_b{pkg_a}; pkg_b < packages.size(); ++pkg_b)
                {
                    const std::span<const Row> rows_b{packages[pkg_b].*member};
                    for (std::size_t idx_j{(pkg_b == pkg_a) ? idx_i + 1 : 0}; idx_j < rows_b.size();
                         ++idx_j)
                        if (rows_a[idx_i].prefix == rows_b[idx_j].prefix &&
                            gates_intersect(rows_a[idx_i].format_gate, rows_b[idx_j].format_gate))
                            return rows_a[idx_i].prefix;
                }
        }
        return std::nullopt;
    }
} // namespace detail

namespace detail
{
    // The outcome-token variant of first_prefix_dup: keyed on .token + intersecting gate (ADR 0025
    // §3.3 — two packages mapping the SAME token under intersecting gates is a conflict, whatever
    // the mapped outcome; a duplicate row has no deterministic resolution either way).
    [[nodiscard]] constexpr std::optional<std::string_view>
    first_outcome_token_dup(std::span<const SemanticPackageManifest> packages) noexcept
    {
        for (std::size_t pkg_a{0}; pkg_a < packages.size(); ++pkg_a)
        {
            const std::span<const OutcomeTokenRow> rows_a{packages[pkg_a].outcome_tokens};
            for (std::size_t idx_i{0}; idx_i < rows_a.size(); ++idx_i)
                for (std::size_t pkg_b{pkg_a}; pkg_b < packages.size(); ++pkg_b)
                {
                    const std::span<const OutcomeTokenRow> rows_b{packages[pkg_b].outcome_tokens};
                    for (std::size_t idx_j{(pkg_b == pkg_a) ? idx_i + 1 : 0}; idx_j < rows_b.size();
                         ++idx_j)
                        if (rows_a[idx_i].token == rows_b[idx_j].token &&
                            gates_intersect(rows_a[idx_i].format_gate, rows_b[idx_j].format_gate))
                            return rows_a[idx_i].token;
                }
        }
        return std::nullopt;
    }
} // namespace detail

constexpr ConflictInfo find_conflict(std::span<const SemanticPackageManifest> packages) noexcept
{
    // An exact duplicate is same key + (for prefix rows) intersecting format gate. Each unordered
    // (package,row) pair is checked once; O(rows²) over a handful of rows at compile time.
    if (const auto key{
            detail::first_prefix_dup<StructuralRoleRow>(packages, &SemanticPackageManifest::roles)})
        return {.has_conflict = true, .kind = "role", .key = *key};
    if (const auto key{
            detail::first_prefix_dup<IntentMarkerRow>(packages, &SemanticPackageManifest::markers)})
        return {.has_conflict = true, .kind = "marker", .key = *key};
    if (const auto key{detail::first_prefix_dup<LevelLiftRow>(
            packages, &SemanticPackageManifest::level_lifts)})
        return {.has_conflict = true, .kind = "level_lift", .key = *key};
    if (const auto key{detail::first_outcome_token_dup(packages)})
        return {.has_conflict = true, .kind = "outcome_token", .key = *key};
    if (const auto key{detail::first_prefix_dup<OutcomeMarkerRow>(
            packages, &SemanticPackageManifest::outcome_markers)})
        return {.has_conflict = true, .kind = "outcome_marker", .key = *key};
    // Value classes are keyed by `.key` (no format gate).
    for (std::size_t pkg_a{0}; pkg_a < packages.size(); ++pkg_a)
        for (std::size_t idx_i{0}; idx_i < packages[pkg_a].value_classes.size(); ++idx_i)
            for (std::size_t pkg_b{pkg_a}; pkg_b < packages.size(); ++pkg_b)
                for (std::size_t idx_j{(pkg_b == pkg_a) ? idx_i + 1 : 0};
                     idx_j < packages[pkg_b].value_classes.size(); ++idx_j)
                    if (packages[pkg_a].value_classes[idx_i].key ==
                        packages[pkg_b].value_classes[idx_j].key)
                        return {.has_conflict = true,
                                .kind = "value_class",
                                .key = packages[pkg_a].value_classes[idx_i].key};
    return {};
}

} // namespace insight::semantic

// ── The level-lift walker (ADR 0063 clause 2) ────────────────────────────────────────────────────
// The last row kind whose matching algorithm lived in a semantic PACKAGE: `LevelLiftRow` was walked
// by the GitHub-Actions strategy over its own `kLevelLifts` array, inside `parse()`. Canon owns the
// ALGORITHM for every other row kind (classify / recognize / recognize_location / map_outcome_token
// over the composed tables), and this one now joins them — so `ComposedSemantics::level_lifts()`,
// which was serialized into `semantic_identity` while no production code read it, has a reader.
//
// WHY IT IS DECLARED HERE and not in the facade beside `classify`/`recognize`: its production
// consumer is `LogParser` (`insight.canon.detail.parse`), a SEALED shard that sits BELOW the
// facade. The level is decided at the parse stage — that is where the pre-existing echoed-source
// demotion (D-PROV-1) overrides it, and the lift must be applied BEFORE that demotion to reproduce
// the pre-relocation order exactly. A walker declared in `insight.canon` is unreachable from there
// without inverting the facade↔detail dependency arrow, so it is declared in the module that owns
// the composed tables. The facade `export import`s this module, so `import insight.canon;` still
// yields it alongside the other walkers.
export namespace insight::tokenization
{

// Lift a line's LogLevel from the composed level-lift rows: the FIRST row whose gate matches
// `format` and whose prefix the content carries wins. Unknown when no gated row matches — which is
// the caller's signal to keep whatever level the strategy inferred, never a level in its own right.
//
// FIRST-match, not longest-match (the rule `classify`/`recognize` use), because first-match in
// declared order is what the pre-relocation package walk did and this relocation is
// output-neutral by construction. Composition already surfaces the only case where the two rules
// could differ: a prefix nesting among level-lift rows is reported as a `ShadowNote` (kind
// "level_lift") and an exact duplicate fails composition closed.
[[nodiscard]] LogLevel lift_level(std::string_view content, LogFormat format,
                                  const insight::semantic::ComposedSemantics& composed) noexcept;

} // namespace insight::tokenization
